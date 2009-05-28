/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2009  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
 
#include "PrecompiledHeader.h"
#include "IopCommon.h"

#include "iR3000air.h"
#include "../R3000airInstruction.inl"

using namespace x86Emitter;

namespace R3000A {

namespace Analytics
{
	int RegMapped_TypeA = 0;
	int RegMapped_TypeB = 0;
}

// buffer used to process intermediate instructions.
static IntermediateRepresentation m_intermediates[MaxCyclesPerBlock];

// GRP's indexer is mapped to esi (using const 6 instead of esi to avoid the pitfalls of out-
// of-order C++ initializer chaos).
static const xAddressReg GPR_xIndexReg( 6 );

static const xMappableRegs32 m_mappableRegs;

static int gpr_map_edi;

// ------------------------------------------------------------------------
// returns an indirect x86 memory operand referencing the requested register.
//
static ModSibStrict<u32> GPR_GetMemIndexer( MipsGPRs_t gpridx )
{
	// There are 34 GPRs counting HI/LO, so the GPR indexer (ESI) points to
	// the HI/LO pair and uses negative addressing for the regular 32 GPRs.

	return ptr32[GPR_xIndexReg - ((32-(uint)gpridx)*4)];
}

static ModSibStrict<u32> GPR_GetMemIndexer( int gpridx )
{
	return ptr32[GPR_xIndexReg - ((32-gpridx)*4)];
}

static RegField_t ToRegField( const ExitMapType& src )
{
	return (RegField_t)src;
}


static const char* RegField_ToString( RegField_t field )
{
	switch( field )
	{
		case RF_Rd: return "Rd";
		case RF_Rt: return "Rt";
		case RF_Rs: return "Rs";
		case RF_Hi: return "Hi";
		case RF_Lo: return "Lo";
		
		jNO_DEFAULT
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
IntermediateRepresentation::IntermediateRepresentation( const xAddressReg& idxreg ) :
	m_constinfoex( (InstConstInfoEx&)*(InstConstInfoEx*)0 )		// One of the more evil hacks you'll ever see?
{
	SetFlushingState();
}


IntermediateRepresentation::IntermediateRepresentation( const InstConstInfoEx& src ) :
	Inst( src.inst )
,	m_constinfoex( src )
,	ixImm( Inst.SignExtendsImm() ? Inst._Opcode_.Imm() : Inst._Opcode_.ImmU() )
,	IsSwappedSources( false )
{
	memzero_obj( RequiredSrcMapping );

	xForceFlush.eax = false;
	xForceFlush.edx = false;
	xForceFlush.ecx = false;
	xForceFlush.ebx = false;
	
	Inst.GetRecInfo();

	if( Inst.IsConstRt() && Inst.IsConstRs() )
	{
		Inst.API.ConstRsRt( Emitface );
	}
	else if( Inst.IsConstRt() )
	{
		Inst.API.ConstRt( Emitface );
	}
	else if( Inst.IsConstRs() )
	{
		Inst.API.ConstRs( Emitface );
	}
	else
	{
		Inst.API.ConstNone( Emitface );
	}

	Emitface.RegMapInfo( *this );
}

// Initializes all sources to memory / flush status.
void IntermediateRepresentation::SetFlushingState()
{
	for( int gpr=0; gpr<34; ++gpr )
		Dest[gpr] = GPR_GetMemIndexer( gpr );
}

// ------------------------------------------------------------------------
// Removes the direct register mapping for the specified register (if one exists).  Or does
// nothing if the register is unmapped.
//
void IntermediateRepresentation::UnmapReg( xDirectOrIndirect32 maparray[34], const xRegister32& reg )
{
	// Note: having a register mapped to multiple GPRs is not an error.  Instructions
	// which operate on cases where Rs == Rt and such will tend to end up with multiple
	// GPRs being 

	int unmap_count=0;		// analytic for number of gprs this reg was mapped to.
	for( int i=0; i<34; ++i )
	{
		if( maparray[i] == reg )
		{
			if( i == gpr_map_edi )
				maparray[i] = edi;
			else
				maparray[i] = GPR_GetMemIndexer( i );
			unmap_count++;
		}
	}
	
	if( unmap_count > 2 )
		Console::Notice( "IOPrec Unmap Analytic: xRegister unmapped from %d GPRs.", params unmap_count );
}

// ------------------------------------------------------------------------
// Returns true if the register is currently mapped in the given list.
//
bool IntermediateRepresentation::IsMappedReg( const xDirectOrIndirect32 maparray[34], const xRegister32& reg ) const
{
	for( int i=0; i<34; ++i )
	{
		if( maparray[i] == reg )
			return true;
	}
	return false;
}

void IntermediateRepresentation::DynRegs_AssignTempReg( int tempslot, const xRegister32& reg )
{
	m_TempReg[tempslot] = reg;
	m_TempReg8[tempslot] = xRegister8( reg.Id );
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void recIR_PerformDynamicRegisterMapping( IntermediateRepresentation& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.DynMap );
	
	// Requirements:
	//  * At least one of the sources must be mapped to a direct register, since no
	//    x86 operations support two indirect sources.

	// To map registers smartly, and without doing mind-breaking logic, we break the
	// process into several passes.  The first couple pass unmaps things we know don't
	// need to be mapped.  The next pass checks for existing maps that are "logical"
	// or required, and marks them for retention.  The third pass finds all available
	// temp registers, pools them, and dispatches them to the needed tempReg mappings.

	// ------------------------------------------------------------------------
	// Reserve/Map temp registers
	
	if( strcmp( cir.Inst.GetName(), "SLTU" ) == 0 )
		Console::WriteLn( "Sing" );

	int tmpinc = 0;
	for( int tc=0; tc<4; ++tc )
	{
		if( !dyno.AllocTemp[tc] ) continue;

		uint freereg, freglen = 4;		// temps *must* be mapped from the top 4 registers.
		for( freereg=0; freereg<freglen; ++freereg )
		{
			if( !cir.IsMappedReg( cir.Src, m_mappableRegs[freereg] ) )
				break;
		}

		if( freereg >= freglen )
			freereg = tmpinc++;		// TODO: pick register based on some analytic?

		// there should always be a free register, unless the regalloc descriptor is malformed,
		// such as an instruction which incorrectly tries to allocate too many regs.
		jASSUME( freereg < freglen );

		const xRegister32& woot( m_mappableRegs[freereg] );	// translate from mappable list to xRegInUse list.
		cir.UnmapReg( cir.Src, woot );
		cir.DynRegs_AssignTempReg( tc, woot );
		dyno.xRegInUse[woot] = true;
	}

	// ------------------------------------------------------------------------
	// Map Forced Directs, which are modified registers that need to be flushed.
	//
	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );
		if( f_gpr == -1 ) continue;
		if( !dyno.ForceDirect[curfield] ) continue;

		if( cir.Src[f_gpr].IsDirect() )
		{
			// Already mapped -- retain it.
			cir.RequiredSrcMapping[f_gpr] = true;
			dyno.xRegInUse[cir.Src[f_gpr].GetReg()] = true;
			cir.xForceFlush[cir.Src[f_gpr].GetReg()] = (dyno.ExitMap[curfield] == DynEM_Invalid);
			continue;
		}

		if( cir.RegOpts.CommutativeSources )
		{
			// TODO: try a source swap first, since the other field might already be
			// allocated to a register.
		}

		// find an available register in xRegInUse;
		// Another 2-pass system.  First pass looks for a register that is completely unmapped/unused.
		// Second pass will just pick one that's not needed by this instruction and unmap it.

		int freereg;
		const int frlen = m_mappableRegs.Length();
		for( freereg=0; freereg<frlen; ++freereg )
		{
			if( !dyno.xRegInUse[m_mappableRegs[freereg]] && !cir.IsMappedReg( cir.Src, m_mappableRegs[freereg] ) )
				break;
		}

		if( freereg >= frlen )
		{
			// Second Pass:
			for( freereg=0; freereg<frlen; ++freereg )
			{
				if( !dyno.xRegInUse[m_mappableRegs[freereg]] ) break;
			}

			// assert: there should always be a free register, unless the regalloc descriptor is
			// malformed, such as an instruction which incorrectly tries to allocate too many regs.
			jASSUME( freereg <= frlen );
		}

		const xRegister32& woot( (m_mappableRegs[freereg]) );
		cir.UnmapReg( cir.Src, woot );
		cir.Src[f_gpr] = woot;
		cir.RequiredSrcMapping[f_gpr] = true;
		dyno.xRegInUse[woot] = true;
	}
	
	// ------------------------------------------------------------------------
	// Map non-ForceDirect SourceFields to any remaining unmapped registers:
	//
		
	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		if( dyno.ForceDirect[curfield] ) continue;			// already mapped these above...

		int f_gpr = cir.Inst.ReadsField( curfield );
		if( f_gpr == -1 ) continue;

		if( cir.Inst.IsConstField( curfield ) ) continue;	// don't map consts
		if( cir.Src[f_gpr].IsDirect() )
		{
			// already mapped from prev inst, so leave it alone...
			// .. and provide an optimization hint that it should remain mapped.
			// (done because this is a clear indication that the reg is being "read" from
			//  more than once, which qualifies it as a valid reg-over-mem use).

			cir.RequiredSrcMapping[f_gpr] = true;
			continue;
		}

		int freereg;
		const int frlen = m_mappableRegs.Length();
		for( freereg=0; freereg<frlen; ++freereg )
		{
			const xRegister32& mreg( m_mappableRegs[freereg] );
			if( !dyno.xRegInUse[mreg] && !cir.IsMappedReg( cir.Src, mreg ) )
			{
				cir.Src[f_gpr] = mreg;
				break;
			}
		}
		
		// If no regs are free leave it unmapped.  The instruction can use indirect
		// access instead.
	}


	for( int ridx=0; ridx<dyno.ExitMap.Length(); ++ridx )
	{
		if( dyno.ExitMap[ridx] == StrictEM_Untouched ) continue;
		
	}
}

void recIR_ForceDestFlushes( IntermediateRepresentation& cir )
{
	// Unmap anything flagged as needing a flush.

	for( int i=0; i<cir.xForceFlush.Length(); ++i )
	{
		if( cir.xForceFlush[i] )
			cir.UnmapReg( cir.Dest, cir.xForceFlush.GetRegAt(i) );
	}

	// Output map for edi is always valid:
	if( gpr_map_edi > 0 )
		cir.Dest[gpr_map_edi] = edi;
}

void recIR_PerformDynamicRegisterMapping_Exit( IntermediateRepresentation& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.DynMap );

	// Assign the instructions "suggested" mappings into the Dest array.  These
	// will be copied into the next instruction's Src later, and used as hints or
	// guidelines for the next instruction's mappings.

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int gpr = cir.Inst.GprFromField( curfield );

		xRegister32 exitReg;

		switch( dyno.ExitMap[curfield] )
		{
			case DynEM_Rt: exitReg = cir.Src[cir.Inst.GprFromField( RF_Rs )].GetReg(); break;
			case DynEM_Rs: exitReg = cir.Src[cir.Inst.GprFromField( RF_Rt )].GetReg(); break;
			case DynEM_Hi: exitReg = cir.Src[cir.Inst.GprFromField( RF_Hi )].GetReg(); break;
			case DynEM_Lo: exitReg = cir.Src[cir.Inst.GprFromField( RF_Lo )].GetReg(); break;

			case DynEM_Temp0: exitReg = cir.TempReg(0); break;
			case DynEM_Temp1: exitReg = cir.TempReg(1); break;
			case DynEM_Temp2: exitReg = cir.TempReg(2); break;
			case DynEM_Temp3: exitReg = cir.TempReg(3); break;

			// everything else is ignored (they're unmapped in the Src stage)
		}

		if( !exitReg.IsEmpty() )
			cir.Dest[gpr] = exitReg;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void recIR_PerformStrictRegisterMapping( IntermediateRepresentation& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.StatMap );

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );
		if( f_gpr == -1 ) continue;

		// unmap and remap, in strict fashion:

		if( !stro.EntryMap[curfield].IsEmpty() )
		{
			cir.UnmapReg( cir.Src, stro.EntryMap[curfield] );
			cir.Src[f_gpr] = stro.EntryMap[curfield];
			cir.RequiredSrcMapping[f_gpr] = true;	// unused, but might use it later...
		}
	}
	
	// Make sure to force-flush any register that's not preserved (specified via the
	// StrictEM_Untouched setting on the exitmap).
	
	for( int ridx=0; ridx<stro.ExitMap.Length(); ++ridx )
	{
		if( stro.ExitMap[ridx] == StrictEM_Untouched ) continue;
		cir.xForceFlush[ridx] = true;
	}
}

void recIR_PerformStrictRegisterMapping_Exit( IntermediateRepresentation& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.StatMap );

	// ExitMaps are hints used to assign the Dest[] mappings as efficiently as possible.

	for( int ridx=0; ridx<stro.ExitMap.Length(); ++ridx )
	{
		ExitMapType exitmap = stro.ExitMap[ridx];
		if( exitmap == StrictEM_Untouched ) continue;		// retains src[] mapping

		const xRegister32& tempReg( stro.ExitMap.GetRegAt( ridx ) );

		// flush all src mappings and replace with the strict specifications of
		// the user-specified mappings (if not invalid).
		cir.UnmapReg( cir.Dest, tempReg );
		if( exitmap == StrictEM_Invalid ) continue;		// unmapped / clobbered

		// Got this far?  Valid register mapping specified:
		// exit-mapped register must be a valid Written Field (assertion failure indicates ints/recs mismatch)
		int f_gpr = cir.Inst.WritesField( ToRegField( exitmap ) );
		jASSUME( f_gpr != -1 );

		cir.UnmapReg( cir.Dest, tempReg );
		cir.Dest[f_gpr] = tempReg;
	}
}

static string m_disasm;

struct GPR_UsePair
{
	int gpr;
	int used;
};

static void DumpSomeGprs( char* sbuf, const xDirectOrIndirect32* arr )
{
	for( int reg=0; reg<32; reg+=8 )
	{
		sprintf( sbuf, "\t%s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]",
			Diag_GetGprName( (MipsGPRs_t)(reg+0) ), arr[reg+0].IsDirect() ? xGetRegName( arr[reg+0].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+1) ), arr[reg+1].IsDirect() ? xGetRegName( arr[reg+1].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+2) ), arr[reg+2].IsDirect() ? xGetRegName( arr[reg+2].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+3) ), arr[reg+3].IsDirect() ? xGetRegName( arr[reg+3].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+4) ), arr[reg+4].IsDirect() ? xGetRegName( arr[reg+4].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+5) ), arr[reg+5].IsDirect() ? xGetRegName( arr[reg+5].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+6) ), arr[reg+6].IsDirect() ? xGetRegName( arr[reg+6].GetReg() ) : "unmap",
			Diag_GetGprName( (MipsGPRs_t)(reg+7) ), arr[reg+7].IsDirect() ? xGetRegName( arr[reg+7].GetReg() ) : "unmap"
		);

		Console::WriteLn( sbuf );
	}
}

static const int Pass3_Unmap = 0;
static const int Pass3_DoNothing = 1;

// ------------------------------------------------------------------------
// Used to propagate some local static info through the Pass3 recursion.
struct PASS3_RECURSE
{
	int numinsts;
	int gpr;
	const xRegister32& xanal;
	
	PASS3_RECURSE( int instcnt, int gpr, const xRegister32& reg ) :
		numinsts( instcnt )
	,	gpr( gpr )
	,	xanal( reg )
	{
	}

	int NextInstruction( int ni )
	{
		if( ni == numinsts ) return Pass3_Unmap;

		IntermediateRepresentation& nir( m_intermediates[ni] );

		if( nir.Src[gpr].IsIndirect() )
		{
			// Unmap this bad boy.
			nir.UnmapReg( nir.Src, xanal );
			return Pass3_Unmap;
		}

		if( nir.RequiredSrcMapping[gpr] || nir.xForceFlush[xanal] )
			return Pass3_DoNothing;

		if( NextInstruction( ni+1 ) == Pass3_Unmap )
		{
			nir.UnmapReg( nir.Src, xanal );
			return Pass3_Unmap;
		}

		return Pass3_DoNothing;
	}
};


//////////////////////////////////////////////////////////////////////////////////////////
// Intermediate Pass 2 -- Maps GPRs to x86 registers prior to the x86 codegen.
//
void recIR_Pass2( const SafeArray<InstConstInfoEx>& iList )
{
	const int numinsts = iList.GetLength();

	// Pass 2 officially consists of a multiple-pass IL stage, which first initializes
	// a flat unoptimized register mapping, and then goes back through and re-maps
	// registers in a second pass in a more optimized fashion.
	
	// ------------------------------------------------------------------------
	// Find the most frequently used GPR for memory indexing, and map it to EDI!
	// Not counted is anything which is const, since that's of minimal importance.
	// Only memory operations matter, since EDI is "special" in being preserved
	// across memory operations.
	//
	int gpr_counter[34] = {0};

	for( int i=0; i<numinsts; ++i )
	{
		const InstructionRecMess& imess( (iList[i].inst) );

		// skip non-memory operations
		if( !imess.ReadsMemory() && !imess.WritesMemory() ) continue;

		for( int cf=0; cf<RF_Count; ++cf )
		{
			RegField_t curfield = (RegField_t)cf;
			int f_gpr = imess.ReadsField( curfield );
			if( f_gpr == -1 || imess.IsConstField( curfield ) ) continue;

			gpr_counter[f_gpr]++;
		}
	}

	GPR_UsePair high = {-1, -1};
	for( int f_gpr=1; f_gpr<34; ++f_gpr )
	{
		if( high.used < gpr_counter[f_gpr] )
		{
			high.gpr = f_gpr;
			high.used = gpr_counter[f_gpr];
		}
	}

	gpr_map_edi = high.gpr;
	
	if( high.used < 2 )
	{
		Console::Notice( "IOPrec: No good match for EDI found. ;_;" );
		gpr_map_edi = -1;
	}

	// ------------------------------------------------------------------------
	// Next pass - Map "required" registers accordingly, and fill in everything
	// else with basic unoptimized register maps.

	// Using placement-new syntax to initialize the Intermediate Representation, because
	// it spares us the agony of the failure that is the C++ copy constructor.
	//
	for( int i=0; i<numinsts; ++i )
		new (&m_intermediates[i]) IntermediateRepresentation( iList[i] );

	for( int i=0; i<numinsts; ++i )
	{
		IntermediateRepresentation& fnew( m_intermediates[i] );

		if( i == 0 )
		{
			// Start with a clean slate.  Everything's loaded from memory, except EDI:
			for( int gpr=0; gpr<34; ++gpr )
				fnew.Src[gpr] = ((gpr == gpr_map_edi) ? (xDirectOrIndirect32)edi : (xDirectOrIndirect32)GPR_GetMemIndexer( gpr ) );
		}
		else
		{
			// this instruction's source mappings start with the previous instruction's dest mappings.
			// We'll modify relevant entries (this instruction's rs/rt/rd and requests for temp regs).

			memcpy_fast( fnew.Src, m_intermediates[i-1].Dest, sizeof( fnew.Src ) );
		}

		if( fnew.RegOpts.IsStrictMode )
			recIR_PerformStrictRegisterMapping( fnew );
		else		
			recIR_PerformDynamicRegisterMapping( fnew );

		// Assign destination (result) registers.
		// Start with the source registers, and remove register mappings based on the
		// instruction's information (regs clobbered, etc)

		memcpy_fast( fnew.Dest, fnew.Src, sizeof( fnew.Src ) );
		recIR_ForceDestFlushes( fnew );		// unmap clobbers

		if( i == numinsts-1 )
			fnew.SetFlushingState();

		else if( fnew.RegOpts.IsStrictMode )
			recIR_PerformStrictRegisterMapping_Exit( fnew );
		else
			recIR_PerformDynamicRegisterMapping_Exit( fnew );
	}
	
	// Unmapping Pass!
	// Goal here is to unmap any registers which are not being used in a "useful" fashion.
	// If a register is is unmapped before it gets read again later on, there's no point
	// in retaining a mapping for it.

	for( int i=0; i<numinsts; ++i )
	{
		IntermediateRepresentation& ir( m_intermediates[i] );

		// For each GPR at each instruction, look for any Direct Mappings that are not
		// strict/required.  From there count forward until either the instruction is
		// unmapped or meets a strict/required mapping.  If unmapped before it's re-read,
		// unmap the whole thing (stop on strict mapping or re-read).
		
		for( int gpr=1; gpr<34; ++gpr )
		{
			if( ir.RequiredSrcMapping[gpr] || ir.Src[gpr].IsIndirect() ) continue;
			
			/*PASS3_RECURSE searcher( numinsts, gpr, ir.Src[gpr].GetReg() );
			
			if( searcher.NextInstruction( i ) == Pass3_Unmap )
			{
				Console::Status( "IOPrec Unmapping Register %s @ inst %d", params xGetRegName( searcher.xanal ), i );
				ir.UnmapReg( ir.Src, searcher.xanal );
			}*/
		}
	}

	// ------------------------------------------------------------------------
	// Perform Full-on Block Dump
	// ------------------------------------------------------------------------

	char sbuf[512];

	for( int i=0; i<numinsts; ++i )
	{
		const IntermediateRepresentation& rdump( m_intermediates[i] );

		rdump.Inst.GetDisasm( m_disasm );
		Console::WriteLn( "\n%s\n", params m_disasm.c_str() );

		Console::WriteLn( "    Input Mappings:" );
		DumpSomeGprs( sbuf, rdump.Src );
		
		Console::WriteLn( "    Output Mappings:" );
		DumpSomeGprs( sbuf, rdump.Dest );
	}	

}

// ------------------------------------------------------------------------
//         W-T-Crap?  Time to generate x86 code?!  *scrreeech*
// ------------------------------------------------------------------------

extern u8* m_xBlock_CurPtr;

void recIR_Pass3( uint numinsts )
{
	xSetPtr( m_xBlock_CurPtr );
	
	// Generate Event Test: Suspiciously Absent ?!
	// It's a [TODO]


	// Prepare Esi/Edi:
	
	xMOV( esi, &iopRegs[GPR_hi] );
	if( gpr_map_edi > 0 )
		xMOV( edi, GPR_GetMemIndexer( gpr_map_edi ) );

	// Dirty tracking for x86 registers -- if the register is written, it becomes
	// dirty and needs to be flushed.  If it's not dirty then no flushing is needed
	// (dirty implies read-only).

	bool xIsDirty[iREGCNT_GPR] = { false };

	for( uint i=0; i<numinsts; ++i )
	{
		const IntermediateRepresentation& ir( m_intermediates[i] );
		
		if( IsDevBuild )
		{
			// Debugging helper: Marks the current instruction's PC.
			// Double up on the XOR preserves EAX status. :D
			xXOR( eax, ir.Inst._Pc_ );
			xXOR( eax, ir.Inst._Pc_ );
		}

		// ------------------------------------------------------------------------
		// Map Entry Registers:

		if( i == 0 )
		{
			// Initial mapping from memory / const / edi
			// (zero reg is included -- needed for some store ops, but should otherwise be unmapped)

			for( int gpr=0; gpr<34; gpr++ )
			{
				if( ir.Src[gpr].IsIndirect() ) continue;

				if( gpr == gpr_map_edi )
					xMOV( ir.Src[gpr], edi );

				else if( ir.m_constinfoex.IsConst( gpr ) )
					xMOV( ir.Src[gpr], ir.m_constinfoex.ConstVal[gpr] );

				else
					xMOV( ir.Src[gpr].GetReg(), GPR_GetMemIndexer(gpr) );
			}
		}
		else
		{
			// Standard mapping from prev instruction's output register status to the input
			// status needed by the current instruction.

			IntermediateRepresentation& previr( m_intermediates[i-1] );
			xDirectOrIndirect32 prevmap[34];
			
			memcpy_fast( prevmap, previr.Dest, sizeof( prevmap ) );

			// (note: zero reg is included -- needed for some store ops, but should otherwise be unmapped)

			for( int dgpr=0; dgpr<34; ++dgpr )
			{
				const xDirectOrIndirect32* target = ir.Src;
				if( prevmap[dgpr] == target[dgpr] ) continue;

				if( target[dgpr].IsDirect() )
				{
					if( dgpr == gpr_map_edi )
					{
						// Special handler for reloading from EDI
						xMOV( target[dgpr].GetReg(), edi );
						prevmap[dgpr] = target[dgpr];
						continue;
					}

					// Xchg/Order checks on direct targets: It's possible for registers in a src map to
					// be dependent on each other, in which case movs must be ordered to avoid over-writing
					// a register before it's moved to it's destination.  For example, if eax->ecx and
					// ecx->edx, then it's imperative that ecx be moved *first*, otherwise it'll end up
					// holding the value of eax instead.

					for( int sgpr=0; sgpr<34; ++sgpr )
					{
						// Scan a copy of the previr's output registers to see if the current target
						// register had any previous assignments that need to be dealt with first.

						if( (dgpr == sgpr) || (target[dgpr] != prevmap[sgpr]) ) continue;

						if( prevmap[dgpr] == target[sgpr] )
						{
							Console::Notice( "IOPrec: Exchange Condition Detected." );
							prevmap[sgpr] = target[sgpr];
							prevmap[dgpr] = target[dgpr];	// this'll turn the xMOV below into a no-op. :)
						}
						else
						{
							if( ir.m_constinfoex.IsConst( sgpr ) )
								xMOV( target[sgpr], ir.m_constinfoex.ConstVal[sgpr] );
							else
								xMOV( target[sgpr], prevmap[sgpr] );
						}
						prevmap[sgpr] = target[sgpr];		// mark it as done.
					}

					if( target[dgpr] != prevmap[dgpr] )		// second check needed in case of out-of-order loading above.
					{
						if( ir.m_constinfoex.IsConst( dgpr ) )
							xMOV( target[dgpr], ir.m_constinfoex.ConstVal[dgpr] );
						else
							xMOV( target[dgpr], prevmap[dgpr] );

						prevmap[dgpr] = target[dgpr];		// mark it as done.
					}
				}
				else if( xIsDirty[prevmap[dgpr].GetReg().Id] )
				{
					// Flushing cases -- Only flush if needed.

					jASSUME( prevmap[dgpr].IsDirect() );
					xMOV( target[dgpr].GetMem(), prevmap[dgpr].GetReg() );
					prevmap[dgpr] = target[dgpr];
				}
			}
			
			// ------------------------------------------------------------------------
			// Pre-instruction Flush Stage
			//
			// Check the Src against the Dest, and flush any registers which go from Direct to Indirect mappings.

			for( int gpr=1; gpr<34; ++gpr )
			{
				if( gpr == gpr_map_edi ) continue;

				const xRegister32& xreg( ir.Src[gpr].GetReg() );

				if( !xreg.IsEmpty() && xIsDirty[xreg.Id] && ir.Dest[gpr].IsIndirect() )
				{
					xMOV( ir.Dest[gpr].GetMem(), xreg );
					xIsDirty[xreg.Id] = false;
				}
			}
		}

		ir.Emitface.Emit( ir );

		// Update Dirty Status on Exit, for any written regs.
		
		for( int cf=0; cf<RF_Count; ++cf )
		{
			RegField_t curfield = (RegField_t)cf;
			int gpr = ir.Inst.WritesField( curfield );
			if( gpr == -1 ) continue;
			
			if( ir.Dest[gpr].IsDirect() )
				xIsDirty[ir.Dest[gpr].GetReg().Id] = true;
		}
	}

	// ------------------------------------------------------------------------
	// End-of-Block Flush Stage
	//
	IntermediateRepresentation& last( m_intermediates[numinsts-1] );

	for( int gpr=1; gpr<34; gpr++ )
	{
		const xRegister32& xreg( last.Dest[gpr].GetReg() );
		
		if( !xreg.IsEmpty() && xIsDirty[xreg.Id] )
			xMOV( GPR_GetMemIndexer( gpr ), last.Dest[gpr].GetReg() );

		else if( last.m_constinfoex.IsConst(gpr) )
			xMOV( GPR_GetMemIndexer( gpr ), last.m_constinfoex.ConstVal[gpr] );
	}
	
	m_xBlock_CurPtr = xGetPtr();
	Console::Notice( "Stage done!" );
}


}

