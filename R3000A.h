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

#pragma once

#include <stdio.h>
#include "R3000air/R3000air.h"

extern bool iopBranchAction;
extern bool iopEventTestIsActive;

////////////////////////////////////////////////////////////////////
// R3000A  Public Interface / API

struct R3000Acpu
{
	void (*Allocate)();
	void (*Reset)();
	void (*Execute)();
	s32 (*ExecuteBlock)( s32 eeCycles );		// executes the given number of EE cycles.
	void (*Clear)(u32 Addr, u32 Size);
	void (*Shutdown)();
};

extern R3000Acpu *psxCpu;
//extern R3000Acpu psxInt;
extern R3000Acpu psxRec;
extern R3000Acpu iopInt;

extern void iopReset();
extern void iopShutdown();
extern void iopException(u32 code, u32 step);
extern void iopBranchTest();
extern void iopExecuteBios();
extern void iopMemReset();

extern void iopEventTest();


// Recognized IOP exceptions:

namespace IopExcCode
{
	static const uint Interrupt = 0;		/* Interrupt */
	static const uint TLB_Mod	= 1;		/* TLB modification (not implemented) */
	static const uint TLB_Load	= 2;		/* TLB exception (not implemented) */
	static const uint TLB_Store	= 3;		/* TLB exception (not implemented) */
	static const uint AddrErr_Load	= 4;	/* Address error (load/fetch) */
	static const uint AddrErr_Store	= 5;	/* Address error (store) */
	static const uint BusErr_Instruction = 6;	/* Bus error, instruction fetch */
	static const uint BusErr_Data = 7;		/* Bus error, data reference */
	static const uint Syscall	= 8;		/* Syscall exception */
	static const uint Breakpoint	= 9;	/* Breakpoint exception */
	static const uint ReservedInst	= 10;	/* Reserve instruction */
	static const uint CopUnavail	= 11;	/* Coprocessor unusable */
	static const uint Overflow	= 12;		/* Arithmetic overflow */
	static const uint Trap	= 13;			/* Trap */
	static const uint FloatingPoint	= 15;	/* Floating point */
	static const uint Cop2	= 18;			/* Coprocessor 2 (not implemented) */
	static const uint MDMX	= 22;			/* MDMX unusable (not implemented) */
	static const uint WATCH	= 23;			/* Reference to Watch (not implemented) */
	static const uint MachineCheck = 24;	/* Machine check (not implemented) */
	static const uint CacheErr = 30;		/* Cache error (not implemented) */
}

/*mfc0 $k0 $13		# Cause register
srl $a0 $k0 2		# Extract ExcCode Field
andi $a0 $a0 0x1f*/
