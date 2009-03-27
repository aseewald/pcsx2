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

#include "R3000A.h"
#include "R3000Exceptions.h"
#include "IopMem.h"

namespace R3000Air
{

typedef Instruction Inst;

static __forceinline void _OverflowCheck( const Instruction& inst, u64 result )
{
	// This 32bit method can rely on the MIPS documented method of checking for
	// overflow, which simply compares bit 32 (rightmost bit of the upper word),
	// against bit 31 (leftmost of the lower word).

	//assert( 0 );

	const u32* const resptr = (u32*)&result;

	if( !!(resptr[1] & 1) != !!(resptr[0] & 0x80000000) )
		throw R3000Exception::Overflow( inst );
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

__instinline void Inst::BGEZ()	// Branch if Rs >= 0
{
	SetBranchInst();
	if( GetRs().SL >= 0 ) DoBranch();
}

__instinline void Inst::BGTZ()	// Branch if Rs >  0
{
	SetBranchInst();
	if( GetRs().SL > 0 ) DoBranch();
}

__instinline void Inst::BLEZ()	// Branch if Rs <= 0
{
	SetBranchInst();
	if( GetRs().SL <= 0 ) DoBranch();
}

__instinline void Inst::BLTZ()	// Branch if Rs <  0
{
	SetBranchInst();
	if( GetRs().SL < 0 ) DoBranch();
}

__instinline void Inst::BGEZAL()	// Branch if Rs >= 0 and link
{
	SetLink();
	BGEZ();
}

__instinline void Inst::BLTZAL()	// Branch if Rs <  0 and link
{
	SetLink();
	BLTZ();
}


/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
__instinline void Inst::BEQ()		// Branch if Rs == Rt
{
	SetBranchInst();
	if( GetRs().SL == GetRt().SL ) DoBranch();
}

__instinline void Inst::BNE()		// Branch if Rs != Rt
{
	SetBranchInst();
	if( GetRs().SL != GetRt().SL ) DoBranch();
}


/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
__instinline void Inst::J()
{
	SetBranchInst();
	DoBranch( JumpTarget() );
}

__instinline void Inst::JAL()
{
	SetLink(); J();
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
__instinline void Inst::JR()
{
	SetBranchInst();
	DoBranch( GetRs().UL );
}

__instinline void Inst::JALR()
{
	SetLinkRd(); JR();
}


/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

// Rt = Rs + Im 	(Exception on Integer Overflow)
__instinline void Inst::ADDI()
{
	s64 result = (s64)GetRs().SL + Imm();
	_OverflowCheck( *this, result );
	SetRt_SL( (s32)result );
}

// Rt = Rs + Im (no exception)
// This is exactly like ADDI but does not perform an overflow exception check.
__instinline void Inst::ADDIU()
{
	/*if( !_Rt_ )
	{
		zeroEx();
		return;
	}*/
	SetRt_SL( GetRs().SL + Imm() );
}

__instinline void Inst::ANDI()	// Rt = Rs And Im
{
	SetRt_UL( GetRs().UL & ImmU() );
}

__instinline void Inst::ORI()		// Rt = Rs Or  Im
{
	SetRt_UL( GetRs().UL | ImmU() );
}

__instinline void Inst::XORI()	// Rt = Rs Xor Im
{
	SetRt_UL( GetRs().UL ^ ImmU() );
}

__instinline void Inst::SLTI()	// Rt = Rs < Im		(Signed)
{
	// Note: C standard guarantees conditionals resolve to 0 or 1 when cast to int.
	SetRt_SL( GetRs().SL < Imm() );
}

__instinline void Inst::SLTIU()	// Rt = Rs < Im		(Unsigned)
{
	// Note: Imm is the 16 bit value SIGN EXTENDED into 32 bits, which is why we
	// cannot use ImmU() here!!
	SetRt_UL( GetRs().UL < (u32)Imm() ); 
}

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
// Rd = Rs + Rt		(Exception on Integer Overflow)
__instinline void Inst::ADD()
{
	s64 result = (s64)GetRs().SL + GetRt().SL;
	
	_OverflowCheck( *this, result );
	SetRd_SL( (s32)result );
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
__instinline void Inst::SUB()
{
	s64 result = (s64)GetRs().SL - GetRt().SL;

	_OverflowCheck( *this, result );
	SetRd_SL( (s32)result );
}

__instinline void Inst::ADDU()	// Rd = Rs + Rt
{
	SetRd_SL( GetRs().SL + GetRt().SL );
}

__instinline void Inst::SUBU()	// Rd = Rs - Rt
{
	SetRd_SL( GetRs().SL - GetRt().SL );
}

__instinline void Inst::AND()		// Rd = Rs And Rt
{
	SetRd_UL( GetRs().UL & GetRt().UL );
}

__instinline void Inst::OR()		// Rd = Rs Or  Rt
{
	SetRd_UL( GetRs().UL | GetRt().UL );
}

__instinline void Inst::XOR()		// Rd = Rs Xor Rt
{
	SetRd_UL( GetRs().UL ^ GetRt().UL );
}

__instinline void Inst::NOR()		// Rd = Rs Nor Rt
{
	SetRd_UL( ~(GetRs().UL | GetRt().UL) );
}

__instinline void Inst::SLT()		// Rd = Rs < Rt		(Signed)
{
	SetRd_UL( GetRs().SL < GetRt().SL );
}

__instinline void Inst::SLTU()	// Rd = Rs < Rt		(Unsigned)
{
	SetRd_UL( GetRs().UL < GetRt().UL );
}


/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/

__instinline void Inst::MultHelper( u64 result )
{
	SetHiLo( (u32)(result >> 32), (u32)result );
	m_DivStall = 11;
}

__instinline void Inst::DIV()
{
	m_DivStall = 35;

	// Div on MIPS Magic:
	// * If Rt is zero, then the result is undefined by MIPS standard.  EE/IOP results are pretty
	//   consistent, however:  Hi == Rs, Lo == (Rs > 0) ? -1 : 1

	// * MIPS has special defined behavior on signed DIVs, to cope with it's lack of overflow
	//   exception handling.  If Rs == -0x80000000 (which is the same as unsigned 0x80000000 on
	//   our beloved twos-compliment math), and Rt == -1 (0xffffffff as unsigned), the result
	//   is 0x80000000 and remainder of zero.
	
	// [TODO] : This could be handled with an exception or signal handler instead of conditionals.

	const s32 Rt = GetRt().SL;
	if( Rt == -1 )
	{
		const u32 Rs = GetRs().UL;
		if( Rs == 0x80000000 )
		{
			SetHiLo( 0, 0x80000000 );
			return;
		}
	}
	else if( Rt == 0 )
	{
		const s32 Rs = GetRs().SL;
		SetHiLo( Rs, (Rs > 0) ? -1 : 1 );
		return;
	}

	const s32 Rs = GetRs().SL;
	SetHiLo( (Rs % Rt), (Rs / Rt) );
}

__instinline void Inst::DIVU()
{
	m_DivStall = 35;

	const u32 Rt = GetRt().UL;
	const u32 Rs = GetRs().UL;

	if( Rt == 0 )
		SetHiLo( Rs, (u32)(-1) );		// unsigned div by zero always returns Rs, 0xffffffff
	else
		SetHiLo( (Rs % Rt), (Rs / Rt) );
}

__instinline void Inst::MULT()
{
	MultHelper( (s64)GetRs().SL * GetRt().SL );
}

__instinline void Inst::MULTU()
{
	MultHelper( (u64)GetRs().UL * GetRt().UL );
}

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
__instinline void Inst::SLL()		// Rd = Rt << sa
{
	SetRd_UL( GetRt().UL << Sa() );
}

__instinline void Inst::SRA()		// Rd = Rt >> sa (arithmetic) [signed]
{
	SetRd_SL( GetRt().SL >> Sa() );
}

__instinline void Inst::SRL()		// Rd = Rt >> sa (logical) [unsigned]
{
	SetRd_UL( GetRt().UL >> Sa() );
}

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/

// Implementation Wanderings:
//   According to modern MIPS cpus, the upper bits of the Rs register are
//   ignored during the shift (only bottom 5 bits matter).  Old interpreters
//   did not take this into account.  Nut sure if by design or if by bug.

__instinline void Inst::SLLV()	// Rd = Rt << rs
{
	SetRd_UL( GetRt().UL << (GetRs().UL & 0x1f) );
} 

__instinline void Inst::SRAV()	// Rd = Rt >> rs (arithmetic)
{
	SetRd_SL( GetRt().SL >> (GetRs().UL & 0x1f) );
}

__instinline void Inst::SRLV()	// Rd = Rt >> rs (logical)
{
	SetRd_UL( GetRt().UL >> (GetRs().UL & 0x1f) );
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
__instinline void Inst::LUI()	// Rt = Im << 16  (lower 16 bits zeroed)
{
	SetRt_SL( Imm() << 16 );
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
__instinline void Inst::MFHI()	// Rd = Hi
{
	SetRd_UL( GetHi().UL );
	m_DivStall = 1;
}

__instinline void Inst::MFLO()	 // Rd = Lo
{
	SetRd_UL( GetLo().UL );
	m_DivStall = 1;
}

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
__instinline void Inst::MTHI()	// Hi = Rs
{
	SetHi_UL( GetRs().UL );
}
__instinline void Inst::MTLO()	// Lo = Rs
{
	SetLo_UL( GetRs().UL );
}

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
// Break exception - psx rom doesn't handle this
__instinline void Inst::BREAK()
{
	RaiseException( IopExcCode::Breakpoint );
}

__instinline void Inst::SYSCALL()
{
	RaiseException( IopExcCode::Syscall );
}

void Inst::_RFE()
{
	HasSideEffects();
	iopRegs.CP0.n.Status = (iopRegs.CP0.n.Status & 0xfffffff0) | ((iopRegs.CP0.n.Status & 0x3c) >> 2);
}

__instinline void Inst::RFE()
{
	_RFE();		// defer to the virtual implementation
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

__instinline void Inst::LB()
{
	const u32 addr = AddrImm();
	SetRt_SL( (s8)MemoryRead8( addr ) );
}

__instinline void Inst::LBU()
{
	const u32 addr = AddrImm();
	SetRt_UL( MemoryRead8( addr ) );
}

// Load half-word (16 bits)
// AddressError exception if the address is not 16-bit aligned.
__instinline void Inst::LH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );
	
	SetRt_SL( (s16)MemoryRead16( addr ) );
}

// Load Halfword Unsigned (16 bits)
// AddressError exception if the address is not 16-bit aligned.
__instinline void Inst::LHU()
{
	const u32 addr = AddrImm();

	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );

	SetRt_UL( MemoryRead16( addr ) );
}

// Load Word (32 bits)
// AddressError exception if the address is not 32-bit aligned.
__instinline void Inst::LW()
{
	const u32 addr = AddrImm();

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, false );

	SetRt_SL( MemoryRead32( addr ) );
}

// Load Word Left (portion loaded determined by address lower 2 bits)
// No exception is thrown if the address is unaligned.
__instinline void Inst::LWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = MemoryRead32( addr & 0xfffffffc );

	SetRt_UL( (GetRt().UL & (0x00ffffff >> shift)) | (mem << (24 - shift)) );

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)

	*/
}

// Load Word Left (portion loaded determined by address lower 2 bits)
// No exception is thrown if the address is unaligned.
__instinline void Inst::LWR()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = MemoryRead32( addr & 0xfffffffc );

	SetRt_UL( (GetRt().UL & (0xffffff00 << (24-shift))) | (mem >> shift) );

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)

	*/
}

__instinline void Inst::SB()
{
	MemoryWrite8( AddrImm(), GetRt().UB[0] );
}

__instinline void Inst::SH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, true );

	MemoryWrite16( addr, GetRt().US[0] );
}

__instinline void Inst::SW()
{
	const u32 addr = AddrImm();

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, true );

	MemoryWrite32( addr, GetRt().UL );
}

// Store Word Left
// No Address Error Exception occurs.
__instinline void Inst::SWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = MemoryRead32(addr & 0xfffffffc);

	MemoryWrite32( (addr & 0xfffffffc),
		(( GetRt().UL >> (24 - shift) )) | (mem & (0xffffff00 << shift))
	);

	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)

	*/
}

__instinline void Inst::SWR()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = MemoryRead32(addr & 0xfffffffc);

	MemoryWrite32( (addr & 0xfffffffc),
		( (GetRt().UL << shift) | (mem & (0x00ffffff >> (24 - shift))) )
	);

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)

	*/
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/

__instinline void Inst::MFC0()
{
	SetRt_UL( GetFs().UL );
}

__instinline void Inst::CFC0()
{
	SetRt_UL( GetFs().UL );
}

__instinline void Inst::MTC0()
{
	SetFs_UL( GetRt().UL );
}

__instinline void Inst::CTC0()
{
	SetFs_UL( GetRt().UL );
}

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
__instinline void Inst::Unknown()
{
	Console::Error("R3000A: Unimplemented op, code=0x%x\n", params _Opcode_ );
}

}
