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

#include "R3000air.h"

namespace R3000A
{

//////////////////////////////////////////////////////////////////////////////////////////
// Opcode Dispatcher.
// Opcodes are dispatched through a switch statement to the instructions the represent.
// I chose to use a switch over tables, because the class-based nature of the interpreter
// would have required member-style dispatchers, which are slow and bulky and have ugly
// syntax.  Additionally, a switch() ends up generating much faster code in this case,
// thanks to the compiler's optimizations reducing it to a call-less quick sort (all
// inlines are much faster than nested void (*func)() dispatchers).
//
// R3000A Instruction Support Notes:
//  * MIPS-I Architecture.
//  * No Traps or Branch Likelys.
//  * Load instructions have a delay slot (ie, instruction immediately following a load
//    will still have the old value for the register being loaded).
//
// Performance / Optimization Notes:
//  * I tested various combinations of virtual and non-virtual functions, and uses of
//    templates and settled on this implementation of using static templated dispatchers,
//    non-virtual instructions, and virtual operators (Read/Set).  Using static templates
//    with non-virtual operators yielded slightly better speed, but also complicates the
//    process of extending the functionality of the Instruction class.  Speed increase
//    was only about 3%.

// -----------------------------------------------------------------------
// Notes on the macro use:  __COUNTER__ increments every time it's used, which allows me
// to compound the table indexes into the macros below.  I then record the value of
// __COUNTER__ prior to each use of switch statements.

#undef su
#undef ex
#undef null

#define ex(func) case (__COUNTER__-baseval): inst.SetName(#func); inst.func(); return;
#define su(func) case (__COUNTER__-baseval): _dispatch_##func(inst); return;
#define null() case (__COUNTER__-baseval): break;

namespace OpcodeDispatchPrivate
{
	template< typename T > __forceinline 
	void _dispatch_SPECIAL( T& inst )
	{
		static const int baseval = __COUNTER__ + 1;

		switch( inst.Funct() )
		{
			ex(SLL)    null()     ex(SRL)   ex(SRA)     ex(SLLV)    null()    ex(SRLV)  ex(SRAV)
			ex(JR)     ex(JALR)   null()    null()      ex(SYSCALL) ex(BREAK) null()    null()
			ex(MFHI)   ex(MTHI)   ex(MFLO)  ex(MTLO)    null()      null()    null()    null()
			ex(MULT)   ex(MULTU)  ex(DIV)   ex(DIVU)    null()      null()    null()    null()
			ex(ADD)    ex(ADDU)   ex(SUB)   ex(SUBU)    ex(AND)     ex(OR)    ex(XOR)   ex(NOR)
			null()     null()     ex(SLT)   ex(SLTU)    null()      null()    null()    null()
			
			// R3000A is MIPSI, and does not support TRAP instructions.
		}
		inst.Unknown();
	}

	template< typename T > __forceinline 
	void _dispatch_REGIMM( T& inst )
	{
		static const int baseval = __COUNTER__ + 1;

		switch( inst._Rt_ )
		{
			ex(BLTZ)    ex(BGEZ)    null()     null()     null()     null()     null()     null()
			null()      null()      null()     null()     null()     null()     null()     null()
			ex(BLTZAL)  ex(BGEZAL)  null()     null()     null()     null()     null()     null()
		}
		inst.Unknown();
	}

	template< typename T > __forceinline 
	void _dispatch_COP0( T& inst )
	{
		static const int baseval = __COUNTER__ + 1;

		switch( inst._Rs_ )
		{
			case 0x00: inst.SetName("MFC0"); inst.MFC0(); return;
			case 0x02: inst.SetName("CFC0"); inst.CFC0(); return;
			case 0x04: inst.SetName("MTC0"); inst.MTC0(); return;
			case 0x06: inst.SetName("CTC0"); inst.CTC0(); return;
			case 0x10: inst.SetName("RFE"); inst.RFE(); return;
		}
		inst.Unknown();
	}

	template< typename T > __forceinline 
	void _dispatch_COP2( T& inst )
	{
		// _Funct_ is the opcode type.  Opcode 0 references the CP0BASIC table.
		// But!  No COP2 instructions are valid in our emulator, since we don't
		// emulate the PSX's GPU .. so nothing to do here for now :D

		inst.Unknown();
	}
}

// ------------------------------------------------------------------------
template< typename T > __forceinline
void OpcodeDispatcher( T& inst )
{
	using namespace OpcodeDispatchPrivate;
	static const int baseval = __COUNTER__ + 1;

	switch( inst.Basecode() )
	{
		su(SPECIAL)  su(REGIMM)  ex(J)     ex(JAL)      ex(BEQ)     ex(BNE)     ex(BLEZ)   ex(BGTZ)
		ex(ADDI)     ex(ADDIU)   ex(SLTI)  ex(SLTIU)    ex(ANDI)    ex(ORI)     ex(XORI)   ex(LUI)
		su(COP0)     null()      su(COP2)  null()       null()      null()      null()     null()
		null()       null()      null()    null()       null()      null()      null()     null()
		ex(LB)       ex(LH)      ex(LWL)   ex(LW)       ex(LBU)     ex(LHU)     ex(LWR)    null()
		ex(SB)       ex(SH)      ex(SWL)   ex(SW)       null()      null()      ex(SWR)    null() 
		null()       null()      null()    null()       null()      null()      null()     null()
		null()       null()      null()    null()       null()      null()      null()     null()
	}
	inst.Unknown();
}

// cleanup the preprocessor namespace:

#undef su
#undef ex
#undef null

}
