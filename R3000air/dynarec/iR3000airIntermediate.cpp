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
#include "../R3000airOpcodeImpl.inl"
#include "../R3000airOpcodeTables.inl"

using namespace x86Emitter;

namespace R3000A
{

static const xAddressReg GPR_xIndexReg( esi );

//////////////////////////////////////////////////////////////////////////////////////////
//
// Notes:
//  * Interrupt/Event tests and Exception tests are not performed here.  They're done at
//    the conclusion of the block execution, to match the same behavior that the recompiled
//    code would implement.
//
__releaseinline void recIL_Step( InstructionOptimizer& dudley )
{
	InstructionOptimizer::Process( dudley );

	// prep the iopRegs for the next instruction fetch -->
	// 
	// Typically VectorPC points to the delay slot instruction on branches, and the GetNextPC()
	// references the *target* of the branch.  Thus the delay slot is processed on the next
	// pass (unless an exception occurs), and *then* we vector to the branch target.  In other
	// words, VectorPC acts as an instruction prefetch, only we prefetch just the PC (doing a
	// full-on instruction prefetch is less efficient).
	//
	// note: In the case of raised exceptions, VectorPC and GetNextPC() can be overridden during
	//  instruction processing above.

	iopRegs.pc			= iopRegs.VectorPC;
	iopRegs.VectorPC	= dudley.GetNextPC();
	iopRegs.IsDelaySlot	= dudley.IsBranchType();

	m_RecState.IncCycleAccum();
	
	// Test for DivUnit Stalls.
	// Note: DivStallUpdater applies the stall directly to the iopRegs, instead of using the
	// RecState's cycle accumulator.  This is because the same thing is done by the recs, and
	// is designed to allow proper handling of stalls across branches.
	
	if( dudley.GetDivStall() != 0 )
		DivStallUpdater( m_RecState.DivCycleAccum, dudley.GetDivStall() );
}

__releaseinline void recIL_StepFast( InstructionOptimizer& dudley )
{
	Opcode opcode( iopMemRead32( iopRegs.pc ) );

	while( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
	{
		//PSXCPU_LOG( "NOP" );

		iopRegs.pc			 = iopRegs.VectorPC;
		iopRegs.VectorPC	+= 4;
		iopRegs.IsDelaySlot	 = false;

		m_RecState.IncCycleAccum();
		opcode = iopMemRead32( iopRegs.pc );
	}

	dudley.Assign( opcode );
	recIL_Step( dudley );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Builds the intermediate const and regalloc resolutions.
//
void recIL_Expand( IntermediateInstruction& iInst )
{
	const InstructionOptimizer& dudley = iInst.Inst;
	
	if( dudley.ReadsRd() )
	{
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Generates IL for an entire block of code.
//
__releaseinline void recIL_Block( SafeList<InstructionOptimizer>& iList )
{
	bool delaySlot = false;

	do 
	{
		Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );

		if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
		{
			//PSXCPU_LOG( "NOP" );

			iopRegs.pc			 = iopRegs.VectorPC;
			iopRegs.VectorPC	+= 4;
			iopRegs.IsDelaySlot	 = false;
		}
		else
		{
			InstructionOptimizer& inst( (iList.New()) );
			inst.Assign( opcode );
			InstructionOptimizer::Process( inst );

			// prep the iopRegs for the next instruction fetch -->
			// 
			// Typically VectorPC points to the delay slot instruction on branches, and the GetNextPC()
			// references the *target* of the branch.  Thus the delay slot is processed on the next
			// pass (unless an exception occurs), and *then* we vector to the branch target.  In other
			// words, VectorPC acts as an instruction prefetch, only we prefetch just the PC (doing a
			// full-on instruction prefetch is less efficient).
			//
			// note: In the case of raised exceptions, VectorPC and GetNextPC() can be overridden during
			//  instruction processing above.

			iopRegs.pc			= iopRegs.VectorPC;
			iopRegs.VectorPC	= inst.GetNextPC();
			iopRegs.IsDelaySlot	= inst.IsBranchType();

			// Test for DivUnit Stalls.
			// Note: DivStallUpdater applies the stall directly to the iopRegs, instead of using the
			// RecState's cycle accumulator.  This is because the same thing is done by the recs, and
			// is designed to allow proper handling of stalls across branches.

			if( inst.GetDivStall() != 0 )
				DivStallUpdater( m_RecState.DivCycleAccum, inst.GetDivStall() );
		}

		m_RecState.IncCycleAccum();
		if( delaySlot ) break;
		delaySlot = iopRegs.IsDelaySlot;

	} while( m_RecState.BlockCycleAccum < 64 );
	
	iopRegs.cycle += m_RecState.BlockCycleAccum;
}


//////////////////////////////////////////////////////////////////////////////////////////
// returns an indirect x86 memory operand referencing the requested register.
ModSibStrict<u32> GPR_GetMemIndexer( uint gpridx )
{
	// There are 34 GPRs counting HI/LO, so the GPR indexer (ESI) points to
	// the HI/LO pair and uses negative addressing for the regular 32 GPRs.

	return ptr32[GPR_xIndexReg - ((32-gpridx)*4)];
}

static SafeList<IntermediateInstruction> il_List( 128 );

//////////////////////////////////////////////////////////////////////////////////////////
// Intermediate Pass 2 -- Assigns regalloc and const prop prior to the x86 codegen.
//
void recIL_Pass2( SafeList<InstructionOptimizer>& iList )
{
	// First instruction starts out as a blank slate:
	const InstructionOptimizer& first( iList[0] );
	const Opcode& effop( first._Opcode_ );

	il_List.Clear();
	IntermediateInstruction& fnew( il_List.New() );
	
	fnew.SrcRs = GPR_GetMemIndexer( effop.Rs() );
	fnew.SrcRt = GPR_GetMemIndexer( effop.Rt() );
	fnew.SrcRd = GPR_GetMemIndexer( effop.Rd() );

	fnew.SrcHi = GPR_GetMemIndexer( 32 );		// Hi
	fnew.SrcLo = GPR_GetMemIndexer( 33 );		// Lo


	for( int i=1, len=iList.GetLength(); i<len; ++i )
	{
		
	}

}

}
