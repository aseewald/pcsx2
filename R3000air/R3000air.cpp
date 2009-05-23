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
#include "R5900.h"

// Used to signal to the EE when important actions that need IOP-attention have
// happened (hsyncs, vsyncs, IOP exceptions, etc).  IOP runs code whenever this
// is true, even if it's already running ahead a bit.
bool iopBranchAction = false;

bool iopEventTestIsActive = false;

PCSX2_ALIGNED16(R3000A::Registers iopRegs);

R3000Acpu *psxCpu;

using namespace R3000A;

void iopReset()
{
	memzero_obj(iopRegs);

	iopRegs.pc = 0xbfc00000; // Start in bootstrap
	iopRegs.CP0.n.Status = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	iopRegs.CP0.n.PRid   = 0x0000001f; // PRevID = Revision ID, same as the IOP R3000A

	iopRegs.NextBranchCycle = iopRegs.cycle + 4;
	iopRegs.VectorPC = iopRegs.pc + 4;
	iopRegs.IsDelaySlot = false;

	psxHwReset();
	psxBiosInit();
	//psxExecuteBios();
}

void iopShutdown() {
	psxBiosShutdown();
	psxSIOShutdown();
	//psxCpu->Shutdown();
}

// Returns the new PC to vector to.
void iopException(u32 code, u32 bd)
{
	PSXCPU_LOG("psxException 0x%x: 0x%x, 0x%x %s\n",
		code, psxHu32(0x1070), psxHu32(0x1074), bd ? "(branch delay)" : ""
	);

	// Set the Cause
	iopRegs.CP0.n.Cause &= ~0x7f;
	iopRegs.CP0.n.Cause |= code<<2;

	// Set the EPC & PC
	if( bd )
	{
		iopRegs.CP0.n.Cause |= 0x80000000;
		iopRegs.CP0.n.EPC    = iopRegs.pc - 4;
	}
	else
		iopRegs.CP0.n.EPC = iopRegs.pc;

	iopRegs.SetExceptionPC( (iopRegs.CP0.n.Status & 0x400000) ? 0xbfc00180 : 0x80000080 );

	// Set the Status
	iopRegs.CP0.n.Status = (iopRegs.CP0.n.Status &~0x3f) |
						  ((iopRegs.CP0.n.Status & 0xf) << 2);

	/*if ((((PSXMu32(iopRegs.CP0.n.EPC) >> 24) & 0xfe) == 0x4a)) {
		// "hokuto no ken" / "Crash Bandicot 2" ... fix
		PSXMu32(iopRegs.CP0.n.EPC)&= ~0x02000000;
	}*/

	if (Config.PsxOut && !CHECK_EEREC)
	{
		u32 call = iopRegs[GPR_t1].UL & 0xff;
		switch (iopRegs.pc & 0x1fffff)
		{
			case 0xa0:

				if (call != 0x28 && call != 0xe)
					PSXBIOS_LOG("Bios call a0: %s (%x) %x,%x,%x,%x\n", biosA0n[call], call, iopRegs[GPR_a0].UL, iopRegs[GPR_a1].UL, iopRegs[GPR_a2].UL, iopRegs[GPR_a3].UL);

				if (biosA0[call])
			   		biosA0[call]();
				break;

			case 0xb0:
				if (call != 0x17 && call != 0xb)
					PSXBIOS_LOG("Bios call b0: %s (%x) %x,%x,%x,%x\n", biosB0n[call], call, iopRegs[GPR_a0].UL, iopRegs[GPR_a1].UL, iopRegs[GPR_a2].UL, iopRegs[GPR_a3].UL);

				if (biosB0[call])
			   		biosB0[call]();
				break;

			case 0xc0:
				PSXBIOS_LOG("Bios call c0: %s (%x) %x,%x,%x,%x\n", biosC0n[call], call, iopRegs[GPR_a0].UL, iopRegs[GPR_a1].UL, iopRegs[GPR_a2].UL, iopRegs[GPR_a3].UL);
			
				if (biosC0[call])
			   		biosC0[call]();
				break;
		}
	}

	/*if (iopRegs.CP0.n.Cause == 0x400 && (!(psxHu32(0x1450) & 0x8))) {
		hwIntcIrq(1);
	}*/
}

__forceinline void iopSetNextBranch( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't blow up
	// if startCycle is greater than our next branch cycle.

	if( (int)(iopRegs.NextBranchCycle - startCycle) > delta )
		iopRegs.NextBranchCycle = startCycle + delta;
}

__forceinline void iopSetNextBranchDelta( s32 delta )
{
	iopSetNextBranch( iopRegs.cycle, delta );
}

__forceinline int iopTestCycle( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't explode
	// if the startCycle is ahead of our current cpu cycle.

	return (int)(iopRegs.cycle - startCycle) >= delta;
}

__forceinline void PSX_INT( IopEventId n, s32 ecycle )
{
	// Generally speaking games shouldn't throw ints that haven't been cleared yet.
	// It's usually indicative os something amiss in our emulation, so uncomment this
	// code to help trap those sort of things.

	// Exception: IRQ16 - SIO - it drops ints like crazy when handling PAD stuff.
	//if( /*n!=16 &&*/ iopRegs.interrupt & (1<<n) )
	//	SysPrintf( "***** IOP > Twice-thrown int on IRQ %d\n", n );

	iopRegs.interrupt |= 1 << n;

	iopRegs.sCycle[n] = iopRegs.cycle;
	iopRegs.eCycle[n] = ecycle;

	iopSetNextBranchDelta( ecycle );

	if( !iopRegs.IsExecuting )
	{
		s32 iopDelta = (iopRegs.NextBranchCycle-iopRegs.cycle)*8;
		cpuSetNextBranchDelta( iopDelta );
	}
}

static __forceinline void IopTestEvent( IopEventId n, void (*callback)() )
{
	if( !(iopRegs.interrupt & (1 << n)) ) return;

	if( iopTestCycle( iopRegs.sCycle[n], iopRegs.eCycle[n] ) )
	{
		iopRegs.interrupt &= ~(1 << n);
		callback();
	}
	else
		iopSetNextBranch( iopRegs.sCycle[n], iopRegs.eCycle[n] );
}

__forceinline void _iopTestInterrupts()
{
	IopTestEvent(IopEvt_SIF0,		sif0Interrupt);	// SIF0
	IopTestEvent(IopEvt_SIF1,		sif1Interrupt);	// SIF1
	IopTestEvent(IopEvt_SIO,		sioInterrupt);
	IopTestEvent(IopEvt_CdvdRead,	cdvdReadInterrupt);

	// Profile-guided Optimization (sorta)
	// The following ints are rarely called.  Encasing them in a conditional
	// as follows helps speed up most games.

	if( iopRegs.interrupt & ( (1ul<<5) | (3ul<<11) | (3ul<<20) | (3ul<<17) ) )
	{
		IopTestEvent(IopEvt_Cdvd,		cdvdActionInterrupt);
		IopTestEvent(IopEvt_Dma11,		psxDMA11Interrupt);	// SIO2
		IopTestEvent(IopEvt_Dma12,		psxDMA12Interrupt);	// SIO2
		IopTestEvent(IopEvt_Cdrom,		cdrInterrupt);
		IopTestEvent(IopEvt_CdromRead,	cdrReadInterrupt);
		IopTestEvent(IopEvt_DEV9,		dev9Interrupt);
		IopTestEvent(IopEvt_USB,		usbInterrupt);
	}
}

void iopEventTest()
{
	if( iopTestCycle( iopRegs.NextsCounter, iopRegs.NextCounter ) )
	{
		psxRcntUpdate();
		iopBranchAction = true;
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

	if( psxHu32(0x1078) == 0 ) return;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return;

	if ((iopRegs.CP0.n.Status & 0xFE01) >= 0x401)
	{
		PSXCPU_LOG("Interrupt: %x  %x\n", psxHu32(0x1070), psxHu32(0x1074));
		iopException(0, 0);
		iopBranchAction = true;
	}
}

void iopTestIntc()
{
	if( psxHu32(0x1078) == 0 ) return;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return;

	if( !eeEventTestIsActive )
	{
		// An iop exception has occurred while the EE is running code.
		// Inform the EE to branch so the IOP can handle it promptly:

		cpuSetNextBranchDelta( 16 );
		iopBranchAction = true;
		//Console::Error( "** IOP Needs an EE EventText, kthx **  %d", params psxCycleEE );

		// Note: No need to set the iop's branch delta here, since the EE
		// will run an IOP branch test regardless.
	}
	else if( !iopEventTestIsActive )
		iopSetNextBranchDelta( 2 );
}

void psxExecuteBios() {
/*	while (iopRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
	PSX_LOG("*BIOS END*\n");
*/
}

