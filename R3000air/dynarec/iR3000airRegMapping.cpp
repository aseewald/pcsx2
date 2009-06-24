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
#include "R3000airIntermediate.h"
#include "../R3000airInstruction.inl"

using namespace x86Emitter;

namespace R3000A {

typedef x86IntRep xIR;

namespace Analytics
{
	int RegMapped_TypeA = 0;
	int RegMapped_TypeB = 0;
}

// GRP's indexer is mapped to esi (using const 6 instead of esi to avoid the pitfalls of out-
// of-order C++ initializer chaos).
static const xAddressReg GPR_xIndexReg( 6 );

// ------------------------------------------------------------------------
// returns an indirect x86 memory operand referencing the requested register.
//
ModSibStrict<u32> xIR::GetMemIndexer( MipsGPRs_t gpridx )
{
	// There are 34 GPRs counting HI/LO, so the GPR indexer (ESI) points to
	// the HI/LO pair and uses negative addressing for the regular 32 GPRs.

	return ptr32[GPR_xIndexReg - ((32-(uint)gpridx)*4)];
}

// ------------------------------------------------------------------------
ModSibStrict<u32> xIR::GetMemIndexer( int gpridx )
{
	jASSUME( gpridx < 34 );
	return ptr32[GPR_xIndexReg - ((32-gpridx)*4)];
}

// ------------------------------------------------------------------------
static RegField_t ToRegField( const ExitMapType& src )
{
	return (RegField_t)src;
}

// ------------------------------------------------------------------------
static const char* RegField_ToString( RegField_t field )
{
	switch( field )
	{
		case RF_Rd: return "Rd";
		case RF_Rt: return "Rt";
		case RF_Rs: return "Rs";
		case RF_Hi: return "Hi";
		case RF_Lo: return "Lo";
		case RF_Link: return "Link";
		
		jNO_DEFAULT
	}
}

// ------------------------------------------------------------------------
bool R3000A::InstructionConstOptimizer::IsConstField( RegField_t field ) const
{
	switch( field )
	{
		case RF_Rd: return false;
		case RF_Rt: return IsConstRt();
		case RF_Rs: return IsConstRs();

		case RF_Hi: return IsConstGpr[GPR_hi];
		case RF_Lo: return IsConstGpr[GPR_lo];

		case RF_Link: return IsConstGpr[GPR_ra];

		jNO_DEFAULT
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
//
xIR::x86IntRep( const xAddressReg& idxreg ) :
	m_constinfoex( (IntermediateRepresentation&)*(IntermediateRepresentation*)0 )		// One of the more evil hacks you'll ever see?
{
	SetFlushingState();
}


xIR::x86IntRep( const IntermediateRepresentation& src ) :
	Inst( src.inst )
,	m_constinfoex( src )
,	ixImm( Inst.SignExtendsImm() ? Inst._Opcode_.Imm() : Inst._Opcode_.ImmU() )
,	IsSwappedSources( false )
,	m_EbpReadMap( GPR_Invalid )
,	m_EbpLoadMap( GPR_Invalid )
{
	memzero_obj( RequiredSrcMapping );

	xForceFlush.eax = false;
	xForceFlush.edx = false;
	xForceFlush.ecx = false;
	xForceFlush.ebx = false;
	
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
}

// Initializes all sources to memory / flush status.
void xIR::SetFlushingState()
{
	for( int gpr=0; gpr<34; ++gpr )
		Dest[gpr] = GetMemIndexer( gpr );
}

// ------------------------------------------------------------------------
// Removes the direct register mapping for the specified register (if one exists).  Or does
// nothing if the register is unmapped.
//
void xIR::UnmapReg( xDirectOrIndirect32 maparray[34], const xRegister32& reg )
{
	// Note: having a register mapped to multiple GPRs is not an error.  Instructions
	// which operate on cases where Rs == Rt and such will tend to end up with multiple
	// GPRs being mapped to a single x86reg.
	
	DynarecAssert( !reg.IsEmpty(), "Tried to unmap an empty register." );

	int unmap_count=0;		// analytic for number of gprs this reg was mapped to.
	for( int i=0; i<34; ++i )
	{
		if( maparray[i] == reg )
		{
			if( i == g_BlockState.gpr_map_edi )
				maparray[i] = edi;
			else
				maparray[i] = GetMemIndexer( i );
			unmap_count++;
		}
	}
	
	if( unmap_count > 2 )
		Console::Notice( "IOPrec Unmap Analytic: xRegister unmapped from %d GPRs.", params unmap_count );
}

// ------------------------------------------------------------------------
// Returns true if the register is currently mapped in the given list.
//
bool xIR::IsMappedReg( const xDirectOrIndirect32 maparray[34], const xRegister32& reg ) const
{
	for( int i=0; i<34; ++i )
	{
		if( maparray[i] == reg )
			return true;
	}
	return false;
}

void xIR::DynRegs_AssignTempReg( int tempslot, const xRegister32& reg )
{
	m_TempReg[tempslot] = reg;
	m_TempReg8[tempslot] = xRegister8( reg.Id );
}

// ------------------------------------------------------------------------
// A prioritized  array of valid mappable registers between/across instructions -
// typically  eax, ebx, ecx, edx, and edi.  EDI is allowed as a mappable register
// since the recompiler makes no guarantee that mappable registers have 8-bit forms.
// (temp registers are limited to the first four for that reason).
//
// EDI is conditionally available based on the state of the gpr_map_edi var.
// When the map is -1 (unmapped), this struct returns eax->edi.  If the gpr
// is mapped then it returns eax->edx (edi is reserved for recompiler special
// use).
//
const xRegister32& iopRec_x86BlockState::GetMappableReg( uint idx ) const
{
	if( HasMappedEdi() )
	{
		switch( idx )
		{
		case 0: return edx;
		case 1: return eax;
		case 2: return ebx;
		case 3: return ecx;

			jNO_DEFAULT		// assert/exception here means index was out of range..
		}
	}
	else
	{
		// EDI is available so let's make it the highest priority mapping,
		// since it gets preserved across memory operations.

		switch( idx )
		{
		case 0: return edi;
		case 1: return edx;
		case 2: return eax;
		case 3: return ebx;
		case 4: return ecx;

			jNO_DEFAULT		// assert/exception here means index was out of range..
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void iopRec_x86BlockState::PerformDynamicRegisterMapping( xIR& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.DynMap );
	
	// To map registers smartly, and without doing mind-breaking logic, we break the
	// process into several passes.  The first couple pass unmaps things we know don't
	// need to be mapped.  The next pass checks for existing maps that are "logical"
	// or required, and marks them for retention.  The third pass finds all available
	// temp registers, pools them, and dispatches them to the needed tempReg mappings.

	// ------------------------------------------------------------------------
	// Reserve/Map temp registers
	
	int tmpinc = 0;
	for( int tc=0; tc<4; ++tc )
	{
		if( !dyno.AllocTemp[tc] ) continue;

		uint freereg, freglen = 4;		// temps *must* be mapped from the top 4 registers.
		for( freereg=0; freereg<freglen; ++freereg )
		{
			if( !cir.IsMappedReg( cir.Src, GetMappableReg(freereg) ) )
				break;
		}

		if( freereg >= freglen )
			freereg = tmpinc++;		// TODO: pick register based on some analytic?

		// there should always be a free register, unless the regmap descriptor is malformed,
		// such as an instruction which incorrectly tries to allocate too many regs.
		cir.DynarecAssert( freereg < freglen, "Ran out of allocable x86 registers (possible malformed regmap descriptor)" );

		const xRegister32& woot( GetMappableReg(freereg) );	// translate from mappableRegs list to xRegInUse list.
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
		const int frlen = GetMappableRegsLength();
		for( freereg=0; freereg<frlen; ++freereg )
		{
			if( !dyno.xRegInUse[GetMappableReg(freereg)] && !cir.IsMappedReg( cir.Src, GetMappableReg(freereg) ) )
				break;
		}

		if( freereg >= frlen )
		{
			// Second Pass:
			for( freereg=0; freereg<frlen; ++freereg )
			{
				if( !dyno.xRegInUse[GetMappableReg(freereg)] ) break;
			}

			// assert: there should always be a free register, unless the regalloc descriptor is
			// malformed, such as an instruction which incorrectly tries to allocate too many regs.
			cir.DynarecAssert( freereg < frlen, "Ran out of allocable x86 registers (possible malformed regmap descriptor)" );
		}

		const xRegister32& woot( GetMappableReg(freereg) );
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

		if( cir.Inst.MipsInst.IsConstField( curfield ) ) continue;	// don't map consts
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
		const int frlen = GetMappableRegsLength();
		for( freereg=0; freereg<frlen; ++freereg )
		{
			const xRegister32& mreg( GetMappableReg(freereg) );
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

void iopRec_x86BlockState::ForceDestFlushes( xIR& cir )
{
	// ebp is never a valid dest (it's read-only)
	cir.UnmapReg( cir.Dest, ebp );

	// Unmap anything flagged as needing a flush.
	for( int i=0; i<cir.xForceFlush.Length(); ++i )
	{
		if( cir.xForceFlush[i] )
			cir.UnmapReg( cir.Dest, cir.xForceFlush.GetRegAt(i) );
	}

	// Output map for edi is always valid:
	if( HasMappedEdi() )
		cir.Dest[gpr_map_edi] = edi;
}

void iopRec_x86BlockState::PerformDynamicRegisterMapping_Exit( xIR& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.DynMap );

	// Assign the instructions "suggested" mappings into the Dest array.  These
	// will be copied into the next instruction's Src later, and used as hints or
	// guidelines for the next instruction's mappings.

	for( int cf=0; cf<RF_Count; ++cf )
	{
		const InstructionConstOptimizer& mips( cir.Inst.MipsInst );
		RegField_t curfield = (RegField_t)cf;
		int gpr = mips.GprFromField( curfield );

		xRegister32 exitReg;

		switch( dyno.ExitMap[curfield] )
		{
			case DynEM_Rt: exitReg = cir.Src[mips.GprFromField( RF_Rs )].GetReg(); break;
			case DynEM_Rs: exitReg = cir.Src[mips.GprFromField( RF_Rt )].GetReg(); break;
			case DynEM_Hi: exitReg = cir.Src[mips.GprFromField( RF_Hi )].GetReg(); break;
			case DynEM_Lo: exitReg = cir.Src[mips.GprFromField( RF_Lo )].GetReg(); break;

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
void iopRec_x86BlockState::PerformStrictRegisterMapping( xIR& cir )
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
			cir.RequiredSrcMapping[f_gpr] = true;
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

void iopRec_x86BlockState::PerformStrictRegisterMapping_Exit( xIR& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.StatMap );

	// ExitMaps are hints used to assign the Dest[] mappings as efficiently as possible.

	for( int ridx=0; ridx<stro.ExitMap.Length(); ++ridx )
	{
		ExitMapType exitmap = stro.ExitMap[ridx];
		if( exitmap == StrictEM_Untouched ) continue;		// retains src[] mapping

		const xRegister32& xReg( stro.ExitMap.GetRegAt( ridx ) );

		// flush all src mappings and replace with the strict specifications of
		// the user-specified mappings (if not invalid).
		cir.UnmapReg( cir.Dest, xReg );
		if( exitmap == StrictEM_Invalid ) continue;		// unmapped / clobbered

		// Got this far?  Valid register mapping specified:
		// exit-mapped register must be a valid Written Field (assertion failure indicates
		// ints/recs mismatch, ie a malformed regmap descriptor)
		int f_gpr = cir.Inst.WritesField( ToRegField( exitmap ) );
		cir.DynarecAssert( f_gpr != -1, "Regmap descriptor specifies an invalid write-field." );

		cir.UnmapReg( cir.Dest, xReg );
		cir.Dest[f_gpr] = xReg;
	}
}

static string m_disasm;

static const char* GetGprStatus( const xDirectOrIndirect32* arr, const IntermediateRepresentation& cinfo, int gpridx )
{
	if( cinfo.MipsInfo.IsConst( gpridx ) ) return "const";
	if( arr[gpridx].IsDirect() ) return xGetRegName( arr[gpridx].GetReg() );
	return "unmap";
}

static void DumpSomeGprs( char* sbuf, const xDirectOrIndirect32* arr, const IntermediateRepresentation& cinfo )
{
	for( int reg=0; reg<32; reg+=8 )
	{
		sprintf( sbuf, "\t%s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]",
			Diag_GetGprName( (MipsGPRs_t)(reg+0) ), GetGprStatus( arr, cinfo, reg+0 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+1) ), GetGprStatus( arr, cinfo, reg+1 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+2) ), GetGprStatus( arr, cinfo, reg+2 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+3) ), GetGprStatus( arr, cinfo, reg+3 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+4) ), GetGprStatus( arr, cinfo, reg+4 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+5) ), GetGprStatus( arr, cinfo, reg+5 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+6) ), GetGprStatus( arr, cinfo, reg+6 ),
			Diag_GetGprName( (MipsGPRs_t)(reg+7) ), GetGprStatus( arr, cinfo, reg+7 )
		);

		Console::WriteLn( sbuf );
	}
}

static const int Pass3_Unmap = 0;
static const int Pass3_DoNothing = 1;

//////////////////////////////////////////////////////////////////////////////////////////
// Used to propagate some local static info through the Pass3 recursion.
//
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

	// Checks the specified instruction index for matching register allocation
	// info to the originating instruction info (contained within this class).
	// If the instruction reuses the register, NextInstruction returns Pass3_DoNothing.
	// If it unmaps the register, NextInstruction returns Pass3_Unmap.
	//
	int NextInstruction( int ni )
	{
		// End of list without a second use? then unmap...
		if( ni == numinsts ) return Pass3_Unmap;

		/*xIR& nir( m_intermediates[ni] );

		if( nir.Src[gpr].IsIndirect() )
		{
			nir.UnmapReg( nir.Src, xanal );
			return Pass3_Unmap;
		}

		if( nir.RequiredSrcMapping[gpr] )
			return Pass3_DoNothing;

		if( NextInstruction( ni+1 ) == Pass3_Unmap )
		{
			nir.UnmapReg( nir.Src, xanal );
			return Pass3_Unmap;
		}*/

		return Pass3_DoNothing;
	}
};

// ------------------------------------------------------------------------
// Find the most frequently used GPR for memory indexing, and map it to EDI at the block-
// wide level.  The mapping is done when there are memory operations for EDI to map
// across, and EDI is picked to map to the most frequently used register.  This generally
// results in the most optimal solution since EDI is automatically preserved across
// register calls.
//
// If the block contains no memory operations, EDI will not be mapped.
// May be extended at a later date to include EBP.. ?
//
void iopRec_x86BlockState::MapBlockWideRegisters( const iopRec_IntermediateState& irBlock )
{
	const int numinsts = irBlock.GetLength();
	int gpr_counter[34] = {0};

	for( int i=0; i<numinsts; ++i )
	{
		const InstructionConstOptimizer& imess( (irBlock.inst[i].MipsInst) );

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

	SetMapEdi( high );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void iopRec_x86BlockState::RegisterMapper( const iopRec_IntermediateState& irBlock )
{
	const int numinsts = irBlock.GetLength();

	MapBlockWideRegisters( irBlock );

	// Using placement-new syntax to initialize the Intermediate Representation, because
	// it spares us the agony of the failure that is the C++ copy constructor.

	for( int i=0; i<numinsts; ++i )
		new (&m_xir[i]) x86IntRep( irBlock.GetInst(i) );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Intermediate Register Mapper (part of the x86-specific passes) -- Maps GPRs to x86
// registers prior to the x86 codegen.
//
void iopRec_x86BlockState::RegisterMapper()
{
	const int numinsts = m_xir.GetLength();

	for( int i=0; i<numinsts; ++i )
	{
		xIR& fnew( m_xir[i] );

		if( i == 0 )
		{
			// First instruction: Start with a clean slate.  Everything's loaded from memory, except EDI:
			for( int gpr=0; gpr<34; ++gpr )
				fnew.Src[gpr] = IsEdiMappedTo( gpr ) ? (xDirectOrIndirect32)edi : (xDirectOrIndirect32)xIR::GetMemIndexer( gpr );
		}
		else
		{
			// All other instructions: source mappings start with the previous instruction's dest mappings.
			memcpy_fast( fnew.Src, m_intermediates[i-1].Dest, sizeof( fnew.Src ) );
			fnew.m_EbpReadMap = m_xir[i-1].m_EbpLoadMap;
		}
		
		if( irBlock.GetInst(i).DelayedDependencyRd )
		{
			fnew.DynarecAssert( !fnew.m_EbpLoadMap, "DependencySlot is already reserved to a GPR." );
			fnew.DynarecAssert( fnew.Inst.WritesField( RF_Rd ) != GPR_Invalid, "DependencySlot flag enabled on an instruction with no Rd writeback." );
			fnew.m_EbpLoadMap = fnew.Inst._Rd_;
		}
		else if( fnew.m_EbpReadMap != GPR_Invalid )
		{
			fnew.Src[fnew.m_EbpReadMap] = ebp;
		}

		if( fnew.RegOpts.IsStrictMode )
			PerformStrictRegisterMapping( fnew );
		else		
			PerformDynamicRegisterMapping( fnew );

		// Assign destination (result) registers.  Start with the source registers, and remove
		// register mappings based on the instruction's information (regs clobbered, etc)

		memcpy_fast( fnew.Dest, fnew.Src, sizeof( fnew.Src ) );
		ForceDestFlushes( fnew );		// unmap clobbers

		if( i == numinsts-1 )
			; //fnew.SetFlushingState();

		else if( fnew.RegOpts.IsStrictMode )
			PerformStrictRegisterMapping_Exit( fnew );
		else
			PerformDynamicRegisterMapping_Exit( fnew );
	}

	// Unmapping Pass!
	// Goal here is to unmap any registers which are not being used in a "useful" fashion.
	// If a register is is unmapped before it gets read again later on, there's no point
	// in retaining a mapping for it.

	for( int i=0; i<numinsts; ++i )
	{
		xIR& ir( m_xir[i] );

		// For each GPR at each instruction, look for any Direct Mappings that are not
		// strict/required.  From there count forward until either the instruction is
		// unmapped or meets a strict/required mapping.  If unmapped before it's re-read,
		// unmap the whole thing (stop on strict mapping or re-read).
		
		for( int gpr=1; gpr<34; ++gpr )
		{
			if( ir.RequiredSrcMapping[gpr] || ir.Src[gpr].IsIndirect() ) continue;
			
			// Note: buggy as hell, just crashes still.
			
			/*PASS3_RECURSE searcher( numinsts, gpr, ir.Src[gpr].GetReg() );

			if( searcher.NextInstruction( i ) == Pass3_Unmap )
			{
				Console::Status( "IOPrec Unmapping Register %s @ inst %d", params xGetRegName( searcher.xanal ), i );
				ir.UnmapReg( ir.Src, searcher.xanal );
			}*/
		}
	}

	//return;

	// ------------------------------------------------------------------------
	// Perform Full-on Block Dump
	// ------------------------------------------------------------------------

	char sbuf[512];

	for( int i=0; i<numinsts; ++i )
	{
		const xIR& rdump( m_xir[i] );

		rdump.Inst.GetDisasm( m_disasm );
		Console::WriteLn( "\n[0x%08X] %s\n", params rdump.Inst._Pc_, m_disasm.c_str() );

		Console::WriteLn( "    Input Mappings:" );
		DumpSomeGprs( sbuf, rdump.Src, rdump.m_constinfoex );
		
		Console::WriteLn( "    Output Mappings:" );
		DumpSomeGprs( sbuf, rdump.Dest, rdump.m_constinfoex );
	}	

}

}

