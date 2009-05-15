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

#include "System.h"

#include "R3000A.h"
#include "IopMem.h"
#include "IopHw.h"
#include "IopCounters.h"
#include "R3000Exceptions.h"

#include "R3000airInstruction.inl"
#include "R3000airOpcodeTables.inl"
#include "R3000airOpcodeImpl.inl"

#include "DebugTools/Debug.h"

namespace R3000Exception
{
	BaseExcept::~BaseExcept() throw() {} 
}

namespace R3000Air {

//////////////////////////////////////////////////////////////////////////////////////////
// Branch delay note:  it's actually legal for the R3000 to have branch instructions in
// the delay slot.  In such a case, the instruction pointed to by the original branch
// becomes the delay slot of the delay slot branch, and the final PC ends up being the
// target of the delay slot branch instruction (this one!).
//
// This is solved by using a method of instruction fetching that closely mimics the actual
// MIPS cpu.  Each instruction is fetched and processed, and then the target PC of the
// previous instruction is applied to iopRegs.pc.  iopRegs.VectorPC holds the target PC of
// the previously executed instruction.

static void intAlloc() { }
static void intReset() { }

//////////////////////////////////////////////////////////////////////////////////////////
// Tests the iop Interrupt status to see if something needs to raise an
// exception.
//
// Implementation notes: Interrupt-style exceptions are raised inline, through  re-assignment
// of the iopRegs.pc (which differs from other exceptions based on C++ SEH handlers).
// Interrupts happen a lot, unlike other types of exceptions, so handling them inline is a
// must (SEH is slow and intended for 'exceptional' use only).
static __forceinline bool intExceptionTest()
{
	if( psxHu32(0x1078) == 0 ) return false;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return false;
	if( (iopRegs.CP0.n.Status & 0xFE01) < 0x401 ) return false;

	iopException(0, iopRegs.IsDelaySlot );
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Yay hack!  This is here until I replace it with a non-shitty event system.
//
static void intEventTest()
{
	if( iopTestCycle( iopRegs.NextsCounter, iopRegs.NextCounter ) )
	{
		psxRcntUpdate();
	}

	// start the next branch at the next counter event by default
	// the interrupt code below will assign nearer branches if needed.
	iopRegs.NextBranchCycle = iopRegs.NextsCounter+iopRegs.NextCounter;

	if (iopRegs.interrupt)
	{
		iopEventTestIsActive = true;
		_iopTestInterrupts();
		iopEventTestIsActive = false;
	}
}

#ifdef PCSX2_DEVBUILD
static string m_disasm;
static string m_comment;
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Steps over the next instruction.
//
static __releaseinline void intStep()
{
	Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );

	if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
	{
		// Optimization note: NOPs are almost never issued in pairs, so changing the if()
		// above into a while() actually decreases overall performance.

		PSXCPU_LOG( "NOP" );

		iopRegs.pc			 = iopRegs.VectorPC;
		iopRegs.VectorPC	+= 4;
		iopRegs.IsDelaySlot	 = false;

		iopRegs.cycle++;

		if( iopRegs.DivUnitCycles > 0 )
			iopRegs.DivUnitCycles--;
		
		opcode = iopMemDirectRead32( iopRegs.pc );
	}

	s32 woot = iopRegs.NextBranchCycle - iopRegs.cycle;
	if( woot <= 0 )
		intEventTest();

	#ifdef PCSX2_DEVBUILD
	if( varLog & 0x00100000 )
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

	Instruction dudley( opcode );
	Instruction::Process( dudley );

	// prep the iopRegs for the next instruction fetch -->
	// 
	// Typically VectorPC points to the delay slot instruction on branches, and the GetNextPC()
	// references the *target* of the branch.  Thus the delay slot is processed on the next
	// pass (unless an exception occurs), and *then* we vector to the branch target.  In other
	// words, VectorPC acts as an instruction prefetch, only we prefetch just the PC (doing a
	// full-on instruction prefetch is less efficient).
	//
	// note: In the case of raised exceptions, VectorPC and GetNextPC() be overridden during
	//  instruction processing above.
	
	iopRegs.pc			= iopRegs.VectorPC;
	iopRegs.VectorPC	= dudley.GetNextPC();
	iopRegs.IsDelaySlot	= dudley.IsBranchType();

	// Test for interrupts *after* updating the PC, otherwise the EPC on exception
	// vector will be wrong!
	if( intExceptionTest() )
	{
		iopRegs.pc			 = iopRegs.VectorPC;
		iopRegs.VectorPC	+= 4;
		iopRegs.IsDelaySlot	 = false;
	}

	iopRegs.cycle++;

	// ------------------------------------------------------------------------
	// The DIV pipe runs in parallel to the rest of the CPU, but if one of the dependent instructions
	// is used, it means we need to stall the IOP to wait for the result.

	if( iopRegs.DivUnitCycles > 0 )
	{
		iopRegs.DivUnitCycles--;
		if( dudley.GetDivStall() != 0 )
		{
			iopRegs.cycle += iopRegs.DivUnitCycles;
			iopRegs.DivUnitCycles = dudley.GetDivStall();	// stall for the following instruction.
		}
	}
	else iopRegs.DivUnitCycles = dudley.GetDivStall();	// optimized case, when DivUnitCycles is known zero.
}

static void intExecute()
{
	while( true )
	{
		intStep();
	}
}

// For efficiency sake, the Iop Interpreter has been designed to take an eeCycles
// parameter, which instructs it on the number of cycles needed to run to get it
// caught up with the EE's instruction status.  The actual number of cycles will
// vary from the value requested (usually higher, but sometimes lower, depending
// on if events occurred).
static s32 intExecuteBlock( s32 eeCycles )
{	
	iopRegs.IsExecuting = true;
	iopRegs.eeCycleStart = iopRegs.cycle;
	iopRegs.eeCycleDelta = eeCycles/8;

	do
	{
		intStep();
	} while( iopTestCycle( iopRegs.eeCycleStart, iopRegs.eeCycleDelta ) == 0 );
	
	iopRegs.IsExecuting = false;
	return eeCycles - ((iopRegs.cycle - iopRegs.eeCycleStart) * 8);
}

static void intClear(u32 Addr, u32 Size) { }

static void intShutdown() { }

}

using namespace R3000Air;

R3000Acpu iopInt =
{
	intAlloc,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};
