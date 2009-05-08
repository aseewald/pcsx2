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

#include "HashMap.h"

using namespace x86Emitter;

namespace R3000A
{
	// There is a possible entry point into the recompiler for every PC in IOP
	// physical ram -- 2 megs / 4:
	static const uint NumEntryPCs = Ps2MemSize::IopRam / 4;

	// One x86 pointer is possible for every PC in our 2 megs of ram:
	PCSX2_ALIGNED( 64, static uptr m_tbl_BlockPtrs[NumEntryPCs] );
	
	struct BlockInfoHash
	{
		uint x86len;	// length of the recompiled block
		u32 pc;			// address of the block in ps2 memory

		// pointer to the jump instruction that links this block to the next in the execution chain
		s32* x86JumpInst;

		// If true, the jump has been optimized away, and blocks are contiguous in memory, thus
		// allowing for fall-through execution.
		bool IsContigious;

		// Intermediate language allocation.  If size is non-zero, then we're on our second pass
		// and the IL should be recompiled into x86 code for direct execution.
		SafeList<IntermediateInstruction> IL;

		// This member contains a copy of the code originally recompiled, for use with
		// MMX/XMM optimized validation of blocks (and subsequent clearing if the block
		// in memoy does not match the validation copy recorded when recompilation was
		// performed).
		SafeArray<u32> ValidationCopy;
	};
	
	//////////////////////////////////////////////////////////////////////////////////////////
	// iopRecState - contains data representing the current known state of the emu during the
	// process of block recompilation.  Information in this struct
	//
	struct iopRecState
	{
		int BlockCycleAccum;
		int DivCycleAccum;
		
		int GetScaledBlockCycles() const
		{
			return BlockCycleAccum * 1;
		}
		
		int GetScaledDivCycles() const
		{
			return DivCycleAccum * 1;
		}
	};
	
	iopRecState m_RecState;

	// ------------------------------------------------------------------------
	// cycleAcc - accumulated cycles since last stall update.
	static void __fastcall DivStallUpdater( uint cycleAcc, int newstall )
	{
		if( cycleAcc < iopRegs.DivUnitCycles )
			iopRegs.cycle += iopRegs.DivUnitCycles - cycleAcc;

		iopRegs.DivUnitCycles = newstall;
	}
	
	// ------------------------------------------------------------------------
	//
	void DynGen_DivStallUpdate( int stallcycles, const xRegister32& tempreg=eax )
	{
		// DivUnit Stalling occurs any time the current instruction has a non-zero
		// DivStall value.  Otherwise we just increment internal cycle counters
		// (which essentially behave as const-optimizations, and are written to
		// memory only when needed).

		if( stallcycles != 0 )
		{
			// Inline version:
			xMOV( tempreg, &iopRegs.DivUnitCycles );
			xSUB( tempreg, m_RecState.GetScaledDivCycles() );
			xForwardJS8 skipStall;
			xADD( &iopRegs.cycle, tempreg );
			skipStall.SetTarget();

			m_RecState.DivCycleAccum = 0;
		}
		else if( m_RecState.DivCycleAccum < 0x7f )
			m_RecState.DivCycleAccum++;		// cap it at 0x7f (anything over 35 is ignored anyway)
	}

	static void EventHandler()
	{
	}

	namespace DynFunc
	{
		u8* CallEventHandler = NULL;
		u8* DispatcherReg = NULL;
	}
	
	static void DynGen_CallEventHandler()
	{
		xCALL( EventHandler );
		xTEST( eax, eax );
		xJNZ( DynFunc::DispatcherReg );

		// This ret() is reached if EventHandler returns non-zero, which signals that the
		// IOP code execution needs to break, and return control to the EE.
		xRET();
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	// newpc - address to link to.  If -1 (0xffffffff), the address is assumed unknown, and
	// we pass through DispatcherReg instead.
	//
	void DynGen_BranchToPC( u32 newpc )
	{
		xMOV( eax, &iopRegs.cycle );
		xADD( eax, m_RecState.GetScaledBlockCycles() );
		xMOV( &iopRegs.cycle, eax );
		xSUB( eax, &iopRegs.NextBranchCycle );

		if( newpc == 0xffffffff )
			xJNS( DynFunc::CallEventHandler );
		else
		{
			// Add this block to the block manager list.
			
		}
	}

};


//////////////////////////////////////////////////////////////////////////////////////////
// Branch Recompilers
//
// At it's core, branching is simply the process of (conditionally) setting a new PC.  The
// recompiler has a few added responsibilities however, due to the fact that we can't
// efficiently handle updates of some things on a per-instruction basis.
//
// Branching Duties:
//  * Flush cached and const registers out to the GPRs struct
//  * Test for events
//  * Execute block dispatcher
//
// Delay slots:
//  The branch delay slot is handled by the IL, so we don't have to worry with it here.

void recJ( const IntermediateInstruction& inst )
{
	
}

void recJR( const IntermediateInstruction& inst )
{
}