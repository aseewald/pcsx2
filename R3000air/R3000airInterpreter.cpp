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
#include "IopCommon.h"

#include "R3000airInstruction.inl"
#include "R3000airOpcodeImpl.inl"
#include "R3000airOpcodeTables.inl"

#include "DebugTools/Debug.h"

extern u32 bExecBIOS;
extern int s_vsync_count;


R3000Exception::BaseExcept::~BaseExcept() throw() {}

R3000Exception::BaseExcept::BaseExcept( const R3000A::Instruction& inst, const std::string& msg ) :
	Exception::Ps2Generic( "(IOP) " + msg ),
	cpuState( iopRegs ),
	Inst( inst ),
	m_IsDelaySlot( iopRegs.IsDelaySlot )
{
}

R3000Exception::BaseExcept::BaseExcept( const std::string& msg ) :
	Exception::Ps2Generic( "(IOP) " + msg ),
	cpuState( iopRegs ),
	Inst( iopMemDirectRead32( iopRegs.pc ) ),
	m_IsDelaySlot( iopRegs.IsDelaySlot )
{
}


namespace R3000A {

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
		// Optimization note:
		// Special NOP handler.  This speeds up the interpretation process significantly.
		// NOPs are almost never issued in pairs, so changing the if() above into a while()
		// actually decreases overall performance.

		if( iopRegs.GetCycle() > 0x0046b200 ) //0x003b0a57 )
		//if( s_vsync_count >= 96 )
		//if( !bExecBIOS )
			PSXCPU_LOG( "NOP", iopRegs.IsDelaySlot ? "\n" : "" );

		iopRegs.pc			 = iopRegs.VectorPC;
		iopRegs.VectorPC	+= 4;
		iopRegs.IsDelaySlot	 = false;
		iopRegs.AddCycles( 1 );

		opcode = iopMemDirectRead32( iopRegs.pc );
	}

#ifdef PCSX2_DEVBUILD
	InstructionOptimizer dudley( opcode );
#else
	Instruction dudley( opcode );
#endif

	Instruction::Process( dudley );

#ifdef PCSX2_DEVBUILD
	if( (varLog & 0x00100000) && (iopRegs.GetCycle() > 0x0046b200 ) ) //0x003b0a57) )
	//if( !bExecBIOS )
	{
		dudley.GetDisasm( m_disasm );
		dudley.GetValuesComment( m_comment );

		if( m_comment.empty() )
			PSXCPU_LOG( "%s%s", m_disasm.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
		else
			PSXCPU_LOG( "%-34s ; %s%s", m_disasm.c_str(), m_comment.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
	}
#endif

	// Prep iopRegs for the next instruction in the queue.  The pipeline works by beginning
	// processing of the current instruction above, and fetching the next instruction (below).
	// The next instruction to fetch is determined by the VectorPC assigned by the previous
	// instruction which is typically pc+4, but could be any branch target address.

	// note: In the case of raised exceptions, VectorPC and GetNextPC() be overridden during
	//  instruction processing above.

	iopRegs.pc			= iopRegs.VectorPC;
	iopRegs.VectorPC	= dudley.GetVectorPC();
	iopRegs.IsDelaySlot	= dudley.HasDelaySlot();

	iopRegs.AddCycles( 1 );
	iopRegs.DivUnitStall( dudley.GetDivStall() );
}

static void intExecute()
{
	while( true )
	{
		intStep();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// For efficiency sake, the Iop Interpreter has been designed to take an eeCycles
// parameter, which instructs it on the number of cycles needed to run to get it
// caught up with the EE's instruction status.  The actual number of cycles will
// vary from the value requested (usually higher, but sometimes lower, depending
// on if events occurred).
//
static s32 intExecuteBlock( s32 eeCycles )
{
	iopRegs.IsExecuting = true;
	u32 eeCycleStart = iopRegs.GetCycle();

	iopRegs.RescheduleEvent( IopEvt_BreakForEE, eeCycles/8 );

	do
	{
		if( iopRegs.evtCycleCountdown <= 0 )
			iopRegs.ExecutePendingEvents();

		intStep();

		jASSUME( iopRegs.evtCycleCountdown <= iopRegs.evtCycleDuration );

	} while( iopRegs.IsExecuting );

	//PSXDMA_LOG( "IOP SYNC BREAK SPOT = %d", eeCycles - ((iopRegs.GetCycle() - eeCycleStart) * 8) );
	return eeCycles - ((iopRegs.GetCycle() - eeCycleStart) * 8);
}

static void intClear(u32 Addr, u32 Size) { }

static void intShutdown() { }

}

R3000Acpu iopInt =
{
	R3000A::intAlloc,
	R3000A::intReset,
	R3000A::intExecute,
	R3000A::intExecuteBlock,
	R3000A::intClear,
	R3000A::intShutdown
};
