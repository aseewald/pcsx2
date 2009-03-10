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

#include "R3000air.h"

namespace R3000Air
{

//////////////////////////////////////////////////////////////////////////////////////////
// Opcode Dispatcher.
// Opcodes are dispatched through a switch statement to the instructions the represent.
// I chose to use a switch over tables, because the class-based nature of the interpreter
// would have required member-style dispatchers, which are slow and bulky and have ugly
// syntax.  Additionally, a switch() ends up generating much faster code in this situation.
//

// Notes on the macro use:  __COUNTER__ increments every time it's used, which allows me
// to compound the table indexes into the macros below.  I then record the value of
// __COUNTER__ prior to each use of switch statements.

#undef su
#undef ex
#undef null

#define ex(func) case (__COUNTER__-baseval): func(); return;
#define su(func) case (__COUNTER__-baseval): _interpret_##func(); return;
#define null() case (__COUNTER__-baseval): break;


void Instruction::_interpret_SPECIAL()
{
	static const int baseval = __COUNTER__ + 1;

	switch( _Funct_ )
	{
		ex(SLL)    ex(NULL)   ex(SRL)   ex(SRA)     ex(SLLV)    null()    ex(SRLV)  ex(SRAV)
		ex(JR)     ex(JALR)   null()    null()      ex(SYSCALL) ex(BREAK) null()    null()
		ex(MFHI)   ex(MTHI)   ex(MFLO)  ex(MTLO)    null()      null()    null()    null()
		ex(MULT)   ex(MULTU)  ex(DIV)   ex(DIVU)    null()      null()    null()    null()
		ex(ADD)    ex(ADDU)   ex(SUB)   ex(SUBU)    ex(AND)     ex(OR)    ex(XOR)   ex(NOR)
		null()     null()     ex(SLT)   ex(SLTU)    null()      null()    null()    null()
		ex(TGE)    ex(TGEU)   ex(TLT)   ex(TLTU)    ex(TEQ)     null()    ex(TNE)   null()
		null()     null()     null()    null()      null()      null()    null()    null()
	}
	Unknown();
}

void Instruction::_interpret_REGIMM()
{
	static const int baseval = __COUNTER__ + 1;

	switch( _Rt_ )
	{
		ex(BLTZ)    ex(BGEZ)    ex(BLTZL)    ex(BGEZL)    null()     null()     null()     null()
		ex(TGEI)    ex(TGEIU)   ex(TLTI)     ex(TLTIU)    ex(TEQI)   null()     ex(TNEI)   null()
		ex(BLTZAL)  ex(BGEZAL)  ex(BLTZALL)  ex(BGEZALL)  null()     null()     null()     null()
	}
	Unknown();
}

void Instruction::_interpret_COP0()
{
	static const int baseval = __COUNTER__ + 1;

	switch( _Rs_ )
	{
		case 0x00: MFC0(); return;
		case 0x02: CFC0(); return;
		case 0x04: MTC0(); return;
		case 0x06: CTC0(); return;
		case 0x10: RFE(); return;
	}
	Unknown();
}

void Instruction::_interpret_COP2()
{
	// _Funct_ is the opcode type.  Opcode 0 references the CP0BASIC table.
	// But!  No COP2 instructions are valid in our emulator, so nothing

	Unknown();
}

void Instruction::Interpret()
{
	static const int baseval = __COUNTER__ + 1;

	switch( _Opcode_ )
	{
		su(SPECIAL)  su(REGIMM)  ex(J)     ex(JAL)      ex(BEQ)     ex(BNE)     ex(BLEZ)   ex(BGTZ)
		ex(ADDI)     ex(ADDIU)   ex(SLTI)  ex(SLTIU)    ex(ANDI)    ex(ORI)     ex(XORI)   ex(LUI)
		su(COP0)     null()      su(COP2)  null()       ex(BEQL)    ex(BNEL)    ex(BLEZL)  ex(BGTZL)
		null()       null()      null()    null()       null()      null()      null()     null()
		ex(LB)       ex(LH)      ex(LWL)   ex(LW)       ex(LBU)     ex(LHU)     ex(LWR)    null()
		ex(SB)       ex(SH)      ex(SWL)   ex(SW)       null()      null()      ex(SWR)    null() 
		null()       null()      null()    null()       null()      null()      null()     null()
		null()       null()      null()    null()       null()      null()      null()     null()
	}
	Unknown();
}

}
