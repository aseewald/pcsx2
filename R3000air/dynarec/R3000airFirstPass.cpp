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
#include "../R3000airOpcodeImpl.inl"
#include "../R3000airOpcodeDispatcher.inl"

#include "R3000airIntermediate.inl"

static string m_disasm;
static string m_comment;

namespace R3000A
{

// ------------------------------------------------------------------------
// Adds this instruction to the IntRep list -- but *only* if it's non-const.  If the inputs
// and result are const then we can just skip the little bugger altogether.
//
__releaseinline bool _recIR_TestConst( InstructionConstOptimizer& inst, GprConstStatus& gpr_IsConst )
{
	// Test and update const status -- return now if the instruction is full const
	// (optimized away to nothing)
	bool isConstWrite = inst.UpdateExternalConstStatus( gpr_IsConst );
	if( isConstWrite && !inst.WritesMemory() && !inst.HasSideEffects() && inst.IsConstPc() ) return true;

	return false;
}

// ------------------------------------------------------------------------
//
__releaseinline void InstructionConstOptimizer::Process()
{
	OpcodeDispatcher( *this );
	jASSUME( !ReadsRd() );		// Rd should always be a target register.
	jASSUME( !WritesRs() );		// Rs should always be a source register.

	// Const Exceptions and Const PCs can never be true at the same time.
	jASSUME( !m_IsConstException || !m_IsConstPc );
}


// ------------------------------------------------------------------------
// Executes the current block of code using the InstructionConstOpt decoder, which collects
// optimization information and propagates constants while executing instructions.  Results
// of the code execution are recorded into m_blockspace, and transferred into a heap alloc
// buffer for later reference.
//
void iopRec_FirstPassConstAnalysis::InterpretBlock()
{
	bool termBlock = false;
	bool skipTerm = false;		// used to skip termination of const branches

	instlen = 0;

	GprConstStatus gpr_IsConst;
	int gpr_DepIndex[34] = { 0 };

	do 
	{
		termBlock = iopRegs.IsDelaySlot;		// terminate blocks after delay slots

		Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );

		if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
		{
			//if( iopRegs.cycle > 0x470000 )
				PSXCPU_LOG( "( NOP )%s", iopRegs.IsDelaySlot ? "\n" : "" );

			iopRegs.pc			 = iopRegs.VectorPC;
			iopRegs.VectorPC	+= 4;
			iopRegs.IsDelaySlot	 = false;
		}
		else
		{
			InstructionConstOptimizer& inst( icex[instlen] );
			inst.Assign( opcode, gpr_IsConst );
			inst.Process();

			// ------------------------------------------------------------------------
			// Check for 'Forced' End-of-Block Conditions:
			//  * Writes to the Status register (can enable / disable memory operations)
			//  * SYSCALL and BREAK need immediate branches (the interpreted handling of the exception
			//     will have updated our PC accordingly)
			//  * RFE re-enables exceptions, so it needs immediate Event Test to handle pending exceptions.
			//    (note, by rule RFE should always be in the delay slot of a non-const branch instruction
			//     so the block should be breaking *anyway* -- but no harm in being certain).

			if( inst.CausesConstException() )	// SYSCALL / BREAK (plus any other const-time exceptions)
				termBlock = true;
			if( strcmp(inst.GetName(), "RFE")==0 )
				termBlock = true;

			// Note: the MIPS _Rd_ field doubles as the Fs field on MTC instructions.
			//if( (inst._Rd_ == 12) && ( (strcmp( inst.GetName(), "MTC0") == 0) || (strcmp( inst.GetName(), "CTC0") == 0) ) )
			//	termBlock = true;

			// ------------------------------------------------------------------------
			// Const branching heuristics.
			// Requirements: We only want to follow const branches if the "reward" outweighs the
			// cost.  Typically this should be a function of const optimization benefits against
			// the actual number of IR instructions being generated.

			if( inst.IsConstBranch() )
			{
				if( inst.GetVectorPC() == iopRegs.pc )
				{
					// Const jump instruction to itself.  If the delay slot is a NOP then this is
					// a common case in the IOP which means it's waiting for an exception to occur.
					// We optimize it by generating code that fast forwards to the next event and ends the block.

					// [not implemented yet]
				}
				else
				{
					// I dunno.  I'm burnt out thinking about optimizing MIPS right now.
					// Let's just break all the other branches regardless and optimize later. --air
				}
			}

			bool isConstOptimised = _recIR_TestConst( inst, gpr_IsConst );
			if( !isConstOptimised )
				instlen++;
			
			if( (varLog & 0x00100000) ) //&& (iopRegs.cycle > 0x470000) )
			{
				inst.GetDisasm( m_disasm );
				inst.GetValuesComment( m_comment );

				// format: Prefixed with (const) tag and postfixed with the comment from the asm
				// generator, if one exists.

				if( m_comment.empty() )
					PSXCPU_LOG( "(%s) %s%s", isConstOptimised ? "const" : "     ", m_disasm.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
				else
					PSXCPU_LOG( "(%s) %-34s ; %s%s", isConstOptimised ? "const" : "     ", m_disasm.c_str(), m_comment.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
			}

			// ------------------------------------------------------------------------
			// Prep iopRegs for the next instruction in the queue.  The pipeline works by beginning
			// processing of the current instruction above, and fetching the next instruction (below).
			// The next instruction to fetch is determined by the VectorPC assigned by the previous
			// instruction which is typically pc+4, but could be any branch target address.

			// note: In the case of raised exceptions, VectorPC and GetNextPC() be overridden during
			//  instruction processing above.

			iopRegs.pc			= iopRegs.VectorPC;
			iopRegs.VectorPC	= inst.GetVectorPC();
			iopRegs.IsDelaySlot	= inst.HasDelaySlot();

			// Note: DivStallUpdater applies the stall directly to the iopRegs, instead of using the
			// RecState's cycle accumulator.  This is because the same thing is done by the recs, and
			// the global stall vars must be updates to allow proper handling of stalls between blocks.

			DivStallUpdater( g_BlockState.DivCycleAccum, inst.GetDivStall() );
		}

		g_BlockState.IncCycleAccum();

		if( instlen >= MaxInstructionsPerBlock ) break;

	} while( !termBlock && (g_BlockState.BlockCycleAccum < MaxCyclesPerBlock) );

	iopRegs.AddCycles( g_BlockState.BlockCycleAccum );
}

}
