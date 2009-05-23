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
static const xAddressReg GPR_xIndexReg( esi );

// Important!  the following two vars must have matching contents for the first four registers.
// Otherwise dynamic regalloc will go insane and Kefka all over the ruined landscapes of recompilation.

static const xRegister32 m_mappableRegs[MappableRegCount]	= { eax, edx, ecx, ebx, edi };
static const xRegister32 m_tempRegs[TempRegCount]			= { eax, edx, ecx, ebx };

// ------------------------------------------------------------------------
// returns an indirect x86 memory operand referencing the requested register.
//
ModSibStrict<u32> GPR_GetMemIndexer( GPRnames gpridx )
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
			maparray[i] = GPR_GetMemIndexer( (GPRnames)i );
			unmap_count++;
		}
	}
	
	if( unmap_count > 2 )
		Console::Notice( "IOPrec Unmap Analytic: xRegister unmapped from %d GPRs.", params unmap_count );
}


// ------------------------------------------------------------------------
// Removes mappings for registers that get clobbered by the instruction.
//
void IntermediateRepresentation::StrictRegs_UnmapClobbers( const RegMapInfo_Strict& stro )
{
	for( int ridx=0; ridx<MappableRegCount; ++ridx )
		if( stro.ClobbersReg[m_mappableRegs[ridx]] )
			UnmapReg( Dest, m_mappableRegs[ridx] );
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

		Src[f_gpr] = GPR_GetMemIndexer( (GPRnames)f_gpr );
	}
}

void recIR_PerformDynamicRegisterMapping( int instidx, IntermediateRepresentation& cir )
{
	RegMapInfo_Dynamic& dyno( cir.RegOpts.UseDynMode() );
	cir.DynRegs_UnmapForcedIndirects( dyno );
	
	// Requirements:
	//  * At least one of the sources must be mapped to a direct register, since no
	//    x86 operations support two indirect sources.

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );

		int regpoolidx = MappableRegCount;		// used for force direct mappings.

		if( f_gpr != -1 )
		{
			// Force-direct mappings.  If the reg is currently indirectly mapped then we'll
			// need to change that here:

			if( dyno[curfield].ForceDirect && cir.Src[f_gpr].IsIndirect() )
			{
				jASSUME( !dyno[curfield].ForceIndirect );

				cir.UnmapReg( cir.Src, m_mappableRegs[regpoolidx] );
				cir.Src[f_gpr] = m_mappableRegs[regpoolidx];
				regpoolidx--;

				

				jASSUME( regpoolidx > 0 );		// ran out of mappable registers? >_<
			}
		}
	}

}

void recIR_PerformStrictRegisterMapping( int instidx, IntermediateRepresentation& cir )
{
	const RegMapInfo_Strict& stro( cir.RegOpts.UseStrictMode() );

	for( int cf=0; cf<RF_Count; ++cf )
	{
		RegField_t curfield = (RegField_t)cf;
		int f_gpr = cir.Inst.ReadsField( curfield );

		if( f_gpr != -1 )
		{
			if( !stro[curfield].EntryMap.IsEmpty() )
			{
				// Top Priority: Requested Direct Mappings override other forms of register
				// mapping, so handle it here first.

				cir.UnmapReg( cir.Src, stro[curfield].EntryMap );
				cir.Src[f_gpr] = stro[curfield].EntryMap;
				cir.m_do_not_touch[f_gpr] = true;
			}
		}
	}
}

// ------------------------------------------------------------------------
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
		new (&m_intermediates[i]) IntermediateRepresentation( iList[i] );

	for( int i=0; i<numinsts; ++i )
	{
		IntermediateRepresentation& fnew( m_intermediates[i] );

		// this instruction's source mappings start with the previous instruction's dest mappings.
		// We'll modify relevant entries (this instruction's rs/rt/rd and requests for temp regs).
		if( i != 0 )
			memcpy_fast( fnew.Src, m_intermediates[i-1].Dest, sizeof( fnew.Src ) );
		else
		{
			// Start with a clean slate.  Everything's loaded from memory:
			for( int gpr=0; gpr<34; ++gpr )
				fnew.Src[gpr] = GPR_GetMemIndexer( (GPRnames)gpr );
		}

		if( fnew.RegOpts.IsStrictMode )
			recIR_PerformDynamicRegisterMapping( i, m_intermediates[i] );
		else
			recIR_PerformStrictRegisterMapping( i, m_intermediates[i] );

		// For each field we need to update the Reads (src) and Writes (dest) mappings.
		// If the field is neither read or written then no mappings are changed.


		//fnew.MapTemporaryRegs( fnew.m_do_not_touch );

		// Assign destination (result) registers.
		// Start with the source registers, and remove register mappings based on the
		// instruction's information (regs clobbered, etc)
		
		/*memcpy_fast( fnew.Dest, fnew.Src, sizeof( fnew.Src ) );

		fnew.UnmapClobbers();

		for( int cf=0; cf<RF_Count; ++cf )
		{
			RegField_t curfield = (RegField_t)cf;
			int f_gpr = iList[i].WritesField( curfield );

			if( f_gpr != -1 )
			{
				if( !fnew.RegOpts[curfield].ExitMap.IsEmpty() )
				{
					fnew.UnmapReg( fnew.Dest, fnew.RegOpts[curfield].ExitMap );
					fnew.Dest[f_gpr] = fnew.RegOpts[curfield].ExitMap;
				}
			}
		}*/		
	}

	// ------------------------------------------------------------------------
	//      Second Pass   - This one goes in reverse?  W-T-Crap!
	// ------------------------------------------------------------------------
	
	// For some magical reason it's easier to do the dynamic regalloc in reverse.  The src/dest
	// mappings are currently required and explicit mappings.
	
	for( int i=numinsts-1; i; --i )
	{
		IntermediateRepresentation& fnew( m_intermediates[i] );

		//fnew.MapTemporaryRegs();
	}
}


}