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
// Builds the intermediate const and regalloc resolutions.
//
void recIL_Expand( IntermediateInstruction& iInst )
{
	const InstructionConstOpt& dudley = iInst.Inst;
	
	if( dudley.ReadsRd() )
	{
	}
}

recBlockItemTemp m_blockspace;

#ifdef PCSX2_DEVBUILD
static string m_disasm;
static string m_comment;
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Generates IL for an entire block of code.
//
void recIL_Block()
{
	bool delaySlot = false;

	m_blockspace.ramlen = 0;
	m_blockspace.instlen = 0;

	bool gpr_IsConst[34] = { true, false };	// GPR0 is always const!

	do 
	{
		Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );
		m_blockspace.ramcopy[m_blockspace.ramlen++] = opcode.U32;

		if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
		{
			PSXCPU_LOG( "NOP%s", iopRegs.IsDelaySlot ? "\n" : "" );

			iopRegs.pc			 = iopRegs.VectorPC;
			iopRegs.VectorPC	+= 4;
			iopRegs.IsDelaySlot	 = false;
		}
		else
		{
			#ifdef PCSX2_DEVBUILD
			if( (varLog & 0x00100000) && iopRegs.cycle > 0x40000 )
			{
				InstructionDiagnostic diag( opcode );
				diag.Process();
				diag.GetDisasm( m_disasm );
				diag.GetValuesComment( m_comment );

				if( m_comment.empty() )
					PSXCPU_LOG( "%s%s", m_disasm.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
				else
					PSXCPU_LOG( "%-34s ; %s%s", m_disasm.c_str(), m_comment.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
			}
			#endif

			InstructionConstOpt& inst( m_blockspace.inst[m_blockspace.instlen] );
			inst.Assign( opcode, gpr_IsConst );
			inst.Process();

			// Update const status for registers.  The const status of all written registers is
			// based on the const status of the read registers.  If the operation reads from
			// memory or from an Fs register, then const status is always false.
			
			bool constStatus;
			
			if( inst.ReadsMemory() || inst.ReadsFs() )
				constStatus = false;
			else
			{
				constStatus = 
					(inst.ReadsRd() ? gpr_IsConst[inst._Rd_] : true) &&
					(inst.ReadsRt() ? gpr_IsConst[inst._Rt_] : true) &&
					(inst.ReadsRs() ? gpr_IsConst[inst._Rs_] : true) &&
					(inst.ReadsHi() ? gpr_IsConst[32] : true) &&
					(inst.ReadsLo() ? gpr_IsConst[33] : true);
			}

			if( inst.WritesRd() ) gpr_IsConst[inst._Rd_] = constStatus;
			if( inst.WritesRt() ) gpr_IsConst[inst._Rt_] = constStatus;
			if( inst.WritesRs() ) gpr_IsConst[inst._Rs_] = constStatus;

			jASSUME( gpr_IsConst[0] == true );		// GPR 0 should *always* be const

			if( inst.WritesLink() ) gpr_IsConst[31] = constStatus;
			if( inst.WritesHi() ) gpr_IsConst[32] = constStatus;
			if( inst.WritesLo() ) gpr_IsConst[33] = constStatus;

			if( !constStatus || inst.WritesMemory() || inst.IsBranchType() || inst.HasSideEffects() )
				m_blockspace.instlen++;

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

			// RFE, SYSCALL and BREAK cause exceptions, which should terminate block recompilation:
			if( inst.HasSideEffects() )
				delaySlot = true;
		}

		m_RecState.IncCycleAccum();
		if( delaySlot ) break;
		delaySlot = iopRegs.IsDelaySlot;

	} while( m_RecState.BlockCycleAccum < MaxCyclesPerBlock );

	jASSUME( m_blockspace.instlen != 0 );

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

static IntermediateInstruction m_intermediates[MaxCyclesPerBlock];

//////////////////////////////////////////////////////////////////////////////////////////
// Intermediate Pass 2 -- Assigns regalloc prior to the x86 codegen.
//
void recIL_Pass2( const SafeArray<InstructionConstOpt>& iList )
{

	// First instruction starts out as a blank slate:
	const InstructionConstOpt& first( iList[0] );
	const Opcode& effop( first._Opcode_ );

	IntermediateInstruction& fnew( m_intermediates[0] );
	
	fnew.SrcRs = GPR_GetMemIndexer( effop.Rs() );
	fnew.SrcRt = GPR_GetMemIndexer( effop.Rt() );
	fnew.SrcRd = GPR_GetMemIndexer( effop.Rd() );

	fnew.SrcHi = GPR_GetMemIndexer( 32 );		// Hi
	fnew.SrcLo = GPR_GetMemIndexer( 33 );		// Lo

	int numinsts = iList.GetLength();
	for( int i=1; i<numinsts; ++i )
	{
		
	}

}

}
