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

namespace R3000A {

//////////////////////////////////////////////////////////////////////////////////////////
// Builds the intermediate const and regalloc resolutions.
//
void recIR_Expand( IntermediateRepresentation& iInst )
{
	const InstructionConstOpt& dudley = iInst.Inst;
	
	if( dudley.ReadsRd() )
	{
	}
}

recBlockItemTemp m_blockspace;

static string m_disasm;
static string m_comment;

//////////////////////////////////////////////////////////////////////////////////////////
// Generates IR for an entire block of code.
//
void recIR_Block()
{
	bool termBlock = false;
	bool skipTerm = false;		// used to skip termination of const branches

	m_blockspace.ramlen = 0;
	m_blockspace.instlen = 0;

	bool gpr_IsConst[34] = { true, false };	// GPR0 is always const!

	do 
	{
		termBlock = iopRegs.IsDelaySlot;		// terminate blocks after delay slots
		
		Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );
		m_blockspace.ramcopy[m_blockspace.ramlen++] = opcode.U32;

		if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
		{
			if( iopRegs.cycle > 0x470000 )
				PSXCPU_LOG( "NOP%s", iopRegs.IsDelaySlot ? "\n" : "" );

			iopRegs.pc			 = iopRegs.VectorPC;
			iopRegs.VectorPC	+= 4;
			iopRegs.IsDelaySlot	 = false;
		}
		else
		{
			InstructionConstOpt& inst( m_blockspace.inst[m_blockspace.instlen] );
			inst.Assign( opcode, gpr_IsConst );
			inst.Process();

			if( (varLog & 0x00100000) && (iopRegs.cycle > 0x470000) )
			{
				inst.GetDisasm( m_disasm );
				inst.GetValuesComment( m_comment );

				if( m_comment.empty() )
					PSXCPU_LOG( "%s%s", m_disasm.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
				else
					PSXCPU_LOG( "%-34s ; %s%s", m_disasm.c_str(), m_comment.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
			}

			// Add this instruction to the IntRep list -- but *only* if it's non-const.
			// If the inputs and result are const then we can just skip the little bugger altogether.
			
			if( !inst.IsConstBranch() )
			{
				bool isConstWrite = inst.UpdateConstStatus( gpr_IsConst );
				if( !isConstWrite || inst.WritesMemory() || inst.HasSideEffects() )
					m_blockspace.instlen++;
			}

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

			// Check for 'Forced' End-of-Block Conditions:
			//  * Writes to the Status register (can enable / disable memory operations)
			//  * SYSCALL and BREAK need immediate branches (the interpreted handling of the exception
			//     will have updated our PC accordingly)
			//  * RFE re-enables exceptions, so it needs immediate Event Test
			//    (note, by rule RFE should always be in the delay slot of a non-const branch instruction
			//     so the block should be breaking *anyway* -- but no harm in being certain).
			
			if( inst.CausesConstException() )	// SYSCALL / BREAK (plus any other const-time exceptions)
				termBlock = true;
			if( strcmp(inst.GetName(), "RFE")==0 )
				termBlock = true;
			//else if( inst.IsConstBranch() )
			//	termBlock = false;

			// Note: the MIPS _Rd_ field doubles as the Fs field on MTC instructions.
			//if( (inst._Rd_ == 12) && ( (strcmp( inst.GetName(), "MTC0") == 0) || (strcmp( inst.GetName(), "CTC0") == 0) ) )
			//	termBlock = true;
		}

		m_RecState.IncCycleAccum();

	} while( !termBlock && (m_RecState.BlockCycleAccum < MaxCyclesPerBlock) );

	iopRegs.cycle += m_RecState.BlockCycleAccum;
}

// TODO: Add the Const type to the parameters here, but I'm waiting for the wxWidgets merge.
void InstructionRecAPI::_const_error()
{
	assert( false );
	throw Exception::LogicError( "R3000A Recompiler Logic Error: invalid const form for this instruction." );
}

void InstructionRecAPI::Error_ConstNone( InstructionEmitterAPI& api )	{ _const_error(); }
void InstructionRecAPI::Error_ConstRs( InstructionEmitterAPI& api )		{ _const_error(); }
void InstructionRecAPI::Error_ConstRt( InstructionEmitterAPI& api )		{ _const_error(); }
void InstructionRecAPI::Error_ConstRsRt( InstructionEmitterAPI& api )	{ _const_error(); }

void InstructionRecMess::GetRecInfo()
{
	API.Reset();
	Instruction::Process( *this );
}

}
