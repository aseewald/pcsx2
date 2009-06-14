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

PCSX2_ALIGNED16(R3000A::Registers iopRegs);

R3000Acpu *psxCpu;

using namespace R3000A;

// ------------------------------------------------------------------------
//
void iopReset()
{
	memzero_obj(iopRegs);

	iopRegs.pc = 0xbfc00000; // Start in bootstrap
	iopRegs.CP0.n.Status = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	iopRegs.CP0.n.PRid   = 0x0000001f; // PRevID = Revision ID, same as the IOP R3000A

	iopRegs.VectorPC = iopRegs.pc + 4;
	iopRegs.IsDelaySlot = false;
	
	iopRegs.evtCycleCountdown	= 32;
	iopRegs.evtCycleDuration	= 32;

	iopRegs.ResetEvents();

	psxHwReset();
	psxBiosInit();
	//psxExecuteBios();
}

void iopShutdown() {
	psxBiosShutdown();
	psxSIOShutdown();
	//psxCpu->Shutdown();
}

// ------------------------------------------------------------------------
//
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

void R3000A::Registers::StopExecution()
{
	if( !IsExecuting ) return;
	iopRegs.ScheduleEvent( IopEvt_BreakForEE, 0 );
}

void R3000A::Registers::RaiseExtInt( uint irq )
{
	psxHu32(0x1070) |= (1 << irq);
	iopTestIntc();
}


void iopTestIntc()
{
	if( psxHu32(0x1078) == 0 ) return;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return;

	iopRegs.RaiseException();
}

void psxHwReset() {

	//	mdecInit(); //initialize mdec decoder
	cdrReset();
	cdvdReset();

	IopCounters::Reset();
	iopRegs.ScheduleEvent( IopEvt_SPU2, 768*8 );

	sioInit();
	//sio2Reset();
}

void psxDmaInterrupt(int n)
{
	if (HW_DMA_ICR & (1 << (16 + n)))
	{
		PSXDMA_LOG( "DMA Interrupt Raised on DMA %02d", n );

		HW_DMA_ICR|= (1 << (24 + n));
		iopRegs.CP0.n.Cause |= 1 << (9 + n);
		iopRegs.RaiseExtInt( IopInt_DMA );
	}
}

void psxDmaInterrupt2(int n)
{
	if (HW_DMA_ICR2 & (1 << (16 + n)))
	{
/*		if (HW_DMA_ICR2 & (1 << (24 + n))) {
			Console::WriteLn("*PCSX2*: HW_DMA_ICR2 n=%d already set", params n);
		}
		if (psxHu32(0x1070) & 8) {
			Console::WriteLn("*PCSX2*: psxHu32(0x1070) 8 already set (n=%d)", params n);
		}*/

		PSXDMA_LOG( "DMA Interrupt Raised on DMA %02d", n+16 );

		HW_DMA_ICR2 |= (1 << (24 + n));
		iopRegs.CP0.n.Cause |= 1 << (16 + n);
		iopRegs.RaiseExtInt( IopInt_DMA );
	}
}


void psxExecuteBios() {
/*	while (iopRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
	PSX_LOG("*BIOS END*\n");
*/
}

