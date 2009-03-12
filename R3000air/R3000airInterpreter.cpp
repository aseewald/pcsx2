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

#include "R3000airOpcodeTables.inl"

namespace R3000Exception
{
	BaseExcept::~BaseExcept() {}
}

namespace R3000Air
{

static void intAlloc() { }
static void intReset() { }

// Tests the iop Interrupt status to see if something needs to raise an
// exception.

// Implementation notes: Interrupt-style exceptions are raised inline, through  re-assignment
// of the iopRegs.pc (which differs from other exceptions based on C++ SEH handlers).
// Interrupts happen a lot, unlike other types of exceptions, so handling them inline is a
// must (SEH is slow and intended for 'exceptional' use only).
static bool intInterruptTest()
{
	if( psxHu32(0x1078) == 0 ) return false;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return false;
	if( (iopRegs.CP0.n.Status & 0xFE01) < 0x401 ) return false;

	iopException(0, 0);
	return true;
}

// Yay hack!  This is here until I replace it with a non-shitty event system.
static void intEventTest()
{
	if( iopTestCycle( psxNextsCounter, psxNextCounter ) )
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

static void Process( Instruction& inst )
{
	//if( (iopRegs.pc >= 0x1200 && iopRegs.pc <= 0x1400) || (iopRegs.pc >= 0x0b40 && iopRegs.pc <= 0xe00))
	{
		if( inst.U32 == 0 )
			Console::WriteLn( "0x%8.8x: NOP", params inst._Pc_ );
		else
		{
			InstructionDiagnostic::Process( inst );
			Console::WriteLn( "0x%8.8x: %-34s ; %s", params
				inst._Pc_, inst.GetDisasm().c_str(), inst.GetValuesComment().c_str() );
		}

		if( inst.IsDelaySlot )
			Console::WriteLn("");
	}

	InstructionInterpreter::Process( inst );
	
	iopRegs.cycle++;
	psxCycleEE -= 8;
}

// Steps over the next instruction.
static void intStep()
{
	intEventTest();

	// Fetch current instruction, and interpret!

	Instruction inst( iopRegs.pc );
	Process( inst );

	if( inst.IsBranchType() )
	{
		// Execute the delay slot before branching (but after the branch
		// target has already been calculated above!)

		// Now's a good time to test for interrupts, which might vector us
		// to a new location.
		if( intInterruptTest() ) return;

		Instruction delaySlot( iopRegs.pc+4, true );
		Process( delaySlot );
		
		if( delaySlot.IsBranchType() )
			Console::Error( "Branch in the delay slot!!" );
	
	}

	iopRegs.pc = inst.GetNextPC();
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
	// psxBreak and psxCycleEE are used to determine the actual number of cycles run.
	psxBreak = 0;
	psxCycleEE = eeCycles;

	while (psxCycleEE > 0)
	{
		intStep();
	}
	return psxBreak + psxCycleEE;
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
