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
static IntermediateRepresentation m_intermediates[MaxCyclesPerBlock+1];
static const xAddressReg GPR_xIndexReg( esi );

// Important!  the following two vars must have matching contents for the first four registers.
// Otherwise dynamic regalloc will go insane and Kefka all over the ruined landscapes of recompilation.

static const xRegister32 m_mappableRegs[RegCount_Mappable]	= { xRegister32(0), xRegister32(1), xRegister32(2), xRegister32(3), xRegister32(7) };
static const xRegister32 m_tempRegs[RegCount_Temps]			= { xRegister32(0), xRegister32(1), xRegister32(2), xRegister32(3) };

// ------------------------------------------------------------------------
// returns an indirect x86 memory operand referencing the requested register.
//
ModSibStrict<u32> GPR_GetMemIndexer( MipsGPRs_t gpridx )
{
	// There are 34 GPRs counting HI/LO, so the GPR indexer (ESI) points to
	// the HI/LO pair and uses negative addressing for the regular 32 GPRs.

	return ptr32[GPR_xIndexReg - ((32-(uint)gpridx)*4)];
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
			maparray[i] = GPR_GetMemIndexer( (MipsGPRs_t)i );
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
		if( maparray[i] == reg ) return true;

	return false;
}


// ------------------------------------------------------------------------
// Finds an unmappable x86 register and returns it.
// This function never returns an empty register.  It'll throw an exception instead, under the
// premise that such a situation only occurs if the RegOpts struct is in an invalid state (ie,
// it attempts to map/alloc too many regs).
//
xRegister32 IntermediateRepresentation::DynRegs_FindUnmappable( const xDirectOrIndirect32 maparray[34] ) const
{
	for( int i=0; i<34; ++i )
	{
		//if( m_do_not_touch[i] ) continue;
		

		
	}

	// TODO: Add instruction name diagnostic info.
	throw Exception::LogicError( "IOP Recompiler Logic Error: Could not find an unmappable register." );
	jASSUME( false );

	return xRegister32();
}

// ------------------------------------------------------------------------
// This is the intended first step of register mapping.  Registers which have been specified to
// forced-indirect are cleared of any direct mappings (allowing those registers to be used for
// other purposes).
//
void IntermediateRepresentation::DynRegs_UnmapForcedIndirects( const RegMapInfo_Dynamic& dyno )
{
	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = Inst.ReadsField( curfield );

		if( f_gpr == -1 ) continue;
		if( !dyno[curfield].ForceIndirect ) continue;

		Src[f_gpr] = GPR_GetMemIndexer( (MipsGPRs_t)f_gpr );
	}
}

void IntermediateRepresentation::DynRegs_AssignTempReg( int tempslot, const xRegister32& reg )
{
	m_TempReg[tempslot] = reg;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void recIR_PerformDynamicRegisterMapping( int instidx, IntermediateRepresentation& cir )
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

	cir.DynRegs_UnmapForcedIndirects( dyno );

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );
		if( f_gpr == -1 ) continue;
		if( !dyno[curfield].ForceDirect ) continue;

		// ForceDirect and ForceIndirect are mutually exclusive:
		jASSUME( !dyno[curfield].ForceIndirect );

		if( cir.Src[f_gpr].IsDirect() )
		{
			// Already mapped -- retain it.
			dyno.xRegInUse[cir.Src[f_gpr].GetReg()] = true;
			continue;
		}
		
		// find an available register in xRegInUse;
		// Another 2-pass system.  First pass looks for a register that is completely unmapped/unused.
		// Second pass will just pick one that's not needed by this instruction and unmap it.

		const xRegister32* freereg		= m_mappableRegs;
		const xRegister32* const endreg	= &freereg[RegCount_Mappable];
		for( ; freereg!=endreg; ++freereg )
			if( !dyno.xRegInUse[*freereg] && !cir.IsMappedReg( cir.Src, *freereg ) ) break;

		if( freereg == endreg )
		{
			// Second Pass:
			for( freereg=m_mappableRegs; freereg!=endreg; ++freereg )
				if( !dyno.xRegInUse[*freereg] ) break;

			// assert: there should always be a free register, unless the regalloc descriptor is
			// malformed, such as an instruction which incorrectly tries to allocate too many regs.
			jASSUME( freereg != endreg );
		}

		cir.UnmapReg( cir.Src, *freereg );
		cir.Src[f_gpr] = *freereg;
		dyno.xRegInUse[*freereg] = true;
	}
	
	// Reserve/Map temp registers

	for( int tc=0; tc<4; ++tc )
	{
		if( !dyno.AllocTemp[tc] ) continue;

		const xRegister32* freereg		= m_tempRegs;
		const xRegister32* const endreg	= &freereg[RegCount_Temps];
		for( ; freereg!=endreg; ++freereg )
			if( !dyno.xRegInUse[*freereg] ) break;

		// there should always be a free register, unless the regalloc descriptor is malformed,
		// such as an instruction which incorrectly tries to allocate too many regs.
		jASSUME( freereg != endreg );

		cir.DynRegs_AssignTempReg( tc, *freereg );
		dyno.xRegInUse[*freereg] = true;
	}
	
	// Map non-ForceDirect SourceFields to any remaining unmapped registers:
	
	for( int reg=0; reg<RegCount_Mappable; reg++ )
	{
		if( dyno.xRegInUse[m_mappableRegs[reg]] ) continue;

		for( int cf=0; cf<RF_Count; ++cf )
		{
			RegField_t curfield = (RegField_t)cf;
			int f_gpr = cir.Inst.ReadsField( curfield );
			if( f_gpr == -1 ) continue;
			
			if( cir.Inst.IsConstField( curfield ) ) continue;	// don't map consts
			if( dyno[curfield].ForceIndirect ) continue;		// don't map indirects to registers.
			if( cir.Src[f_gpr].IsDirect() ) continue;			// already mapped, so leave it alone...

			const xRegister32* freereg		= m_mappableRegs;
			const xRegister32* const endreg	= &freereg[RegCount_Mappable];
			for( ; freereg!=endreg; ++freereg )
				if( !cir.IsMappedReg( cir.Src, *freereg ) ) break;

			// no free regs?  Make one:
			// [TODO] : This code should pick an optimal register to unmap based on some first
			// pass register usage analysis, which unconveniently doesn't exist yet.
			if( freereg == endreg )
			{
				cir.UnmapReg( cir.Src, m_mappableRegs[reg] );
				freereg = &m_mappableRegs[reg];
			}

			cir.Src[f_gpr] = *freereg;
		}
	}
}

void recIR_PerformDynamicRegisterMapping_Exit( int instidx, IntermediateRepresentation& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.DynMap );

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int gpr = cir.Inst.GprFromField( curfield );

		xRegister32 exitReg;

		switch( dyno[curfield].ExitMap )
		{
			case DynEM_Unmapped: break;

			case DynEM_Temp0: exitReg = cir.TempReg(0); break;
			case DynEM_Temp1: exitReg = cir.TempReg(1); break;
			case DynEM_Temp2: exitReg = cir.TempReg(2); break;
			case DynEM_Temp3: exitReg = cir.TempReg(3); break;

			case DynEM_Rt: exitReg = cir.Src[cir.Inst.GprFromField( RF_Rs )].GetReg(); break;
			case DynEM_Rs: exitReg = cir.Src[cir.Inst.GprFromField( RF_Rt )].GetReg(); break;
			case DynEM_Hi: exitReg = cir.Src[cir.Inst.GprFromField( RF_Hi )].GetReg(); break;
			case DynEM_Lo: exitReg = cir.Src[cir.Inst.GprFromField( RF_Lo )].GetReg(); break;

			jNO_DEFAULT;
		}
		
		if( !exitReg.IsEmpty() )
		{
			cir.UnmapReg( cir.Dest, exitReg );
			cir.Dest[gpr] = exitReg;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void recIR_PerformStrictRegisterMapping( int instidx, IntermediateRepresentation& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.StatMap );

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );
		if( f_gpr == -1 ) continue;

		// EntryMaps are treated as  unquestionable "laws" in the world of strict mapping,
		// since none of the instructions that require strict mapping would benefit from
		// fuzzy mapping alternatives.

		if( !stro.EntryMap[curfield].IsEmpty() )
		{
			cir.UnmapReg( cir.Src, stro.EntryMap[curfield] );
			cir.Src[f_gpr] = stro.EntryMap[curfield];
			cir.m_do_not_touch[f_gpr] = true;
		}
	}
}

RegField_t ToRegField( const ExitMapType& src )
{
	return (RegField_t)src;
}

void recIR_PerformStrictRegisterMapping_Exit( int instidx, IntermediateRepresentation& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.StatMap );

	// ExitMaps are hints used to assign the Dest[] mappings as efficiently as possible.

	for( int ridx=0; ridx<RegCount_Temps; ++ridx )
	{
		ExitMapType exitmap = stro.ExitMap[m_tempRegs[ridx]];

		if( exitmap == eMap_Untouched ) continue;		// retains src[] mapping

		// flush all src mappings and replace with user-specified mappings.
		cir.UnmapReg( cir.Dest, m_tempRegs[ridx] );
		if( exitmap == eMap_Invalid ) continue;		// unmapped / clobbered

		// Got this far?  Valid register mapping specified:
		// exit-mapped register must be a valid Written Field (assertion failure indicates ints/recs mismatch)
		int f_gpr = cir.Inst.WritesField( ToRegField( exitmap ) );
		jASSUME( f_gpr != -1 );

		cir.UnmapReg( cir.Dest, m_tempRegs[ridx] );
		cir.Dest[f_gpr] = m_tempRegs[ridx];
	}
}

static string m_disasm;

//////////////////////////////////////////////////////////////////////////////////////////
// Intermediate Pass 2 -- Maps GPRs to x86 registers prior to the x86 codegen.
//
void recIR_Pass2( const SafeArray<InstructionConstOpt>& iList )
{
	const int numinsts = iList.GetLength();

	// Pass 2 officially consists of a multiple-pass IL stage, which first initializes
	// a flat unoptimized register mapping, and then goes back through and re-maps
	// registers in a second pass in a more optimized fashion.
	
	// First pass - Map "required" registers accordingly, and fill in everything
	// else with basic unoptimized register maps.


	// Using placement-new syntax to initialize the Intermediate Representation, because
	// it spares us the agony of the failure that is the C++ copy constructor.
	for( int i=0; i<numinsts; ++i )
		new (&m_intermediates[i]) IntermediateRepresentation( iList[i], m_intermediates[i+1].Src );

	for( int i=0; i<numinsts; ++i )
	{
		IntermediateRepresentation& fnew( m_intermediates[i] );

		// this instruction's source mappings start with the previous instruction's dest mappings.
		// We'll modify relevant entries (this instruction's rs/rt/rd and requests for temp regs).
		if( i == 0 )
		{
			// Start with a clean slate.  Everything's loaded from memory:
			for( int gpr=0; gpr<34; ++gpr )
				fnew.Src[gpr] = GPR_GetMemIndexer( (MipsGPRs_t)gpr );
		}

		if( fnew.RegOpts.IsStrictMode )
			recIR_PerformStrictRegisterMapping( i, m_intermediates[i] );
		else
			recIR_PerformDynamicRegisterMapping( i, m_intermediates[i] );

		// Assign destination (result) registers.
		// Start with the source registers, and remove register mappings based on the
		// instruction's information (regs clobbered, etc)

		memcpy_fast( fnew.Dest, fnew.Src, sizeof( fnew.Src ) );

		if( fnew.RegOpts.IsStrictMode )
			recIR_PerformStrictRegisterMapping_Exit( i, m_intermediates[i] );
		else
			recIR_PerformDynamicRegisterMapping_Exit( i, m_intermediates[i] );	
	}

	// ------------------------------------------------------------------------
	// Perform Full-on Block Dump
	// ------------------------------------------------------------------------
	
	return;
	char sbuf[512];

	for( int i=0; i<numinsts; ++i )
	{
		IntermediateRepresentation& rdump( m_intermediates[i] );

		rdump.Inst.GetDisasm( m_disasm );

		//Console::WriteLn( "\n%s\n", params m_disasm.c_str() );

		for( int reg=0; reg<32; reg+=8 )
		{
			sprintf( sbuf, "    %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]  %s[%-5s]",
				Diag_GetGprName( (MipsGPRs_t)(reg+0) ), rdump.Src[reg+0].IsDirect() ? xGetRegName( rdump.Src[reg+0].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+1) ), rdump.Src[reg+1].IsDirect() ? xGetRegName( rdump.Src[reg+1].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+2) ), rdump.Src[reg+2].IsDirect() ? xGetRegName( rdump.Src[reg+2].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+3) ), rdump.Src[reg+3].IsDirect() ? xGetRegName( rdump.Src[reg+3].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+4) ), rdump.Src[reg+4].IsDirect() ? xGetRegName( rdump.Src[reg+4].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+5) ), rdump.Src[reg+5].IsDirect() ? xGetRegName( rdump.Src[reg+5].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+6) ), rdump.Src[reg+6].IsDirect() ? xGetRegName( rdump.Src[reg+6].GetReg() ) : "unmap",
				Diag_GetGprName( (MipsGPRs_t)(reg+7) ), rdump.Src[reg+7].IsDirect() ? xGetRegName( rdump.Src[reg+7].GetReg() ) : "unmap"
			);
				
			//Console::WriteLn( sbuf );
		}
	}	
	
	// ------------------------------------------------------------------------
	//         W-T-Crap?  Time to generate x86 code?!  *scrreeech*
	// ------------------------------------------------------------------------
}


}