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
#include "R3000Exceptions.h"
#include "IopMem.h"

namespace R3000Air
{

typedef InstructionInterpreter InstInterp;

static __forceinline void _OverflowCheck( const Instruction& inst, u64 result )
{
	// This 32bit method can rely on the MIPS documented method of checking for
	// overflow, which simply compares bit 32 (rightmost bit of the upper word),
	// against bit 31 (leftmost of the lower word).

	//assert( 0 );

	const u32* const resptr = (u32*)&result;

	// If bit32 != bit31 then we have an overflow.
	//if( !!(result & ((u64)1<<32)) != !!(result & 0x80000000) )
	if( !!(resptr[1] & 1) != !!(resptr[0] & 0x80000000) )
		throw R3000Exception::Overflow( inst );
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void InstInterp::BGEZ()	// Branch if Rs >= 0
{
	SetBranchInst();
	if( GetRs().SL >= 0 ) DoBranch();
}

void InstInterp::BGTZ()	// Branch if Rs >  0
{
	SetBranchInst();
	if( GetRs().SL > 0 ) DoBranch();
}

void InstInterp::BLEZ()	// Branch if Rs <= 0
{
	SetBranchInst();
	if( GetRs().SL <= 0 ) DoBranch();
}

void InstInterp::BLTZ()	// Branch if Rs <  0
{
	SetBranchInst();
	if( GetRs().SL < 0 ) DoBranch();
}

void InstInterp::BGEZAL()	// Branch if Rs >= 0 and link
{
	SetLink();
	BGEZ();
}

void InstInterp::BLTZAL()	// Branch if Rs <  0 and link
{
	SetLink();
	BLTZ();
}


/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
void InstInterp::BEQ()		// Branch if Rs == Rt
{
	SetBranchInst();
	if( GetRs().SL == GetRt().SL ) DoBranch();
}

void InstInterp::BNE()		// Branch if Rs != Rt
{
	SetBranchInst();
	if( GetRs().SL != GetRt().SL ) DoBranch();
}


/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void InstInterp::J()
{
	SetBranchInst();
	DoBranch( JumpTarget() );
}

void InstInterp::JAL()
{
	SetLink(); J();
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void InstInterp::JR()
{
	SetBranchInst();
	DoBranch( GetRs().UL );
}

void InstInterp::JALR()
{
	SetLinkRd(); JR();
}


/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

// Rt = Rs + Im 	(Exception on Integer Overflow)
void InstInterp::ADDI()
{
	s64 result = (s64)GetRs().SL + Imm();
	_OverflowCheck( *this, result );
	SetRt_SL( (s32)result );
}

// Rt = Rs + Im (no exception)
// This is exactly like ADDI but does not perform an overflow exception check.
void InstInterp::ADDIU()
{
	/*if( !_Rt_ )
	{
		zeroEx();
		return;
	}*/
	SetRt_SL( GetRs().SL + Imm() );
}

void InstInterp::ANDI()	// Rt = Rs And Im
{
	SetRt_UL( GetRs().UL & ImmU() );
}

void InstInterp::ORI()		// Rt = Rs Or  Im
{
	SetRt_UL( GetRs().UL | ImmU() );
}

void InstInterp::XORI()	// Rt = Rs Xor Im
{
	SetRt_UL( GetRs().UL ^ ImmU() );
}

void InstInterp::SLTI()	// Rt = Rs < Im		(Signed)
{
	// Note: C standard guarantees conditionals resolve to 0 or 1 when cast to int.
	SetRt_SL( GetRs().SL < Imm() );
}

void InstInterp::SLTIU()	// Rt = Rs < Im		(Unsigned)
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
void InstInterp::ADD()
{
	s64 result = (s64)GetRs().SL + GetRt().SL;
	
	_OverflowCheck( *this, result );
	SetRd_SL( (s32)result );
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
void InstInterp::SUB()
{
	s64 result = (s64)GetRs().SL - GetRt().SL;

	_OverflowCheck( *this, result );
	SetRd_SL( (s32)result );
}

void InstInterp::ADDU()	// Rd = Rs + Rt
{
	SetRd_SL( GetRs().SL + GetRt().SL );
}

void InstInterp::SUBU()	// Rd = Rs - Rt
{
	SetRd_SL( GetRs().SL - GetRt().SL );
}

void InstInterp::AND()		// Rd = Rs And Rt
{
	SetRd_UL( GetRs().UL & GetRt().UL );
}

void InstInterp::OR()		// Rd = Rs Or  Rt
{
	SetRd_UL( GetRs().UL | GetRt().UL );
}

void InstInterp::XOR()		// Rd = Rs Xor Rt
{
	SetRd_UL( GetRs().UL ^ GetRt().UL );
}

void InstInterp::NOR()		// Rd = Rs Nor Rt
{
	SetRd_UL( ~(GetRs().UL | GetRt().UL) );
}

void InstInterp::SLT()		// Rd = Rs < Rt		(Signed)
{
	SetRd_UL( GetRs().SL < GetRt().SL );
}

void InstInterp::SLTU()	// Rd = Rs < Rt		(Unsigned)
{
	SetRd_UL( GetRs().UL < GetRt().UL );
}


/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void InstInterp::DIV()
{
	// If Rt is zero, then the result is undefined.
	// Which means we can safely ignore the instruction entirely. :D

	const s32 Rt = GetRt().SL;
	if( Rt == 0 ) return;

	const s32 Rs = GetRs().SL;
	SetHiLo( (Rs % Rt), (Rs / Rt) );
}

void InstInterp::DIVU()
{
	const u32 Rt = GetRt().UL;
	if( Rt == 0 ) return;

	const u32 Rs = GetRs().UL;
	SetHiLo( (Rs % Rt), (Rs / Rt) );
}

void InstInterp::MultHelper( u64 result )
{
	SetHiLo( (u32)(result >> 32), (u32)result );
}

void InstInterp::MULT()
{
	MultHelper( (s64)GetRs().SL * GetRt().SL );
}

void InstInterp::MULTU()
{
	MultHelper( (u64)GetRs().UL * GetRt().UL );
}

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void InstInterp::SLL()		// Rd = Rt << sa
{
	SetRd_UL( GetRt().UL << Sa() );
}

void InstInterp::SRA()		// Rd = Rt >> sa (arithmetic) [signed]
{
	SetRd_SL( GetRt().SL >> Sa() );
}

void InstInterp::SRL()		// Rd = Rt >> sa (logical) [unsigned]
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

void InstInterp::SLLV()	// Rd = Rt << rs
{
	SetRd_UL( GetRt().UL << (GetRs().UL & 0x1f) );
} 

void InstInterp::SRAV()	// Rd = Rt >> rs (arithmetic)
{
	SetRd_SL( GetRt().SL >> (GetRs().UL & 0x1f) );
}

void InstInterp::SRLV()	// Rd = Rt >> rs (logical)
{
	SetRd_UL( GetRt().UL >> (GetRs().UL & 0x1f) );
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void InstInterp::LUI()	// Rt = Im << 16  (lower 16 bits zeroed)
{
	SetRt_SL( Imm() << 16 );
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void InstInterp::MFHI()	// Rd = Hi
{
	SetRd_UL( GetHi().UL );
}

void InstInterp::MFLO()	 // Rd = Lo
{
	SetRd_UL( GetLo().UL );
}

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void InstInterp::MTHI()	// Hi = Rs
{
	SetHi_UL( GetRs().UL );
}
void InstInterp::MTLO()	// Lo = Rs
{
	SetLo_UL( GetRs().UL );
}

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
// Break exception - psx rom doesn't handle this
void InstInterp::BREAK()
{
	iopException( IopExcCode::Breakpoint, iopRegs.IsDelaySlot );
	m_NextPC = iopRegs.VectorPC+4;
}

void InstInterp::SYSCALL()
{
	iopException( IopExcCode::Syscall, iopRegs.IsDelaySlot );
	m_NextPC = iopRegs.VectorPC+4;
}

void InstInterp::RFE()
{
	HasSideEffects();
	iopRegs.CP0.n.Status =
		(iopRegs.CP0.n.Status & 0xfffffff0) | ((iopRegs.CP0.n.Status & 0x3c) >> 2);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

void InstInterp::LB()
{
	const u32 addr = AddrImm();
	s8 result = iopMemRead8( addr );
	SetRt_SL( result );
	ReadsMemory();
}

void InstInterp::LBU()
{
	const u32 addr = AddrImm();
	u8 result = iopMemRead8( addr );
	SetRt_SL( result );
	ReadsMemory();
}

// Load half-word (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void InstInterp::LH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );
	
	s16 result = iopMemRead16( addr );
	SetRt_SL( result );
	ReadsMemory();
}

// Load Halfword Unsigned (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void InstInterp::LHU()
{
	const u32 addr = AddrImm();

	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );

	u16 result = iopMemRead16( addr );
	SetRt_SL( result );
	ReadsMemory();
}

// Load Word (32 bits)
// AddressError exception if the address is not 32-bit aligned.
void InstInterp::LW()
{
	const u32 addr = AddrImm();

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, false );

	s32 result = iopMemRead32( addr );
	SetRt_SL( result );
	ReadsMemory();
}

// Load Word Left (portion loaded determined by address lower 2 bits)
// No exception is thrown if the address is unaligned.
void InstInterp::LWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32( addr & 0xfffffffc );

	SetRt_UL( (GetRt().UL & (0x00ffffff >> shift)) | (mem << (24 - shift)) );
	ReadsMemory();

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
void InstInterp::LWR()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32( addr & 0xfffffffc );

	SetRt_UL( (GetRt().UL & (0xffffff00 << (24-shift))) | (mem >> shift) );
	ReadsMemory();

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)

	*/
}

void InstInterp::SB()
{
	iopMemWrite8( AddrImm(), (u8)GetRt().UL );
	WritesMemory();
}

void InstInterp::SH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite16( addr, GetRt().US[0] );
	WritesMemory();
}

void InstInterp::SW()
{
	const u32 addr = AddrImm();

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite32( addr, GetRt().UL );
	WritesMemory();
}

// Store Word Left
// No Address Error Exception occurs.
void InstInterp::SWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32(addr & 0xfffffffc);

	ReadsMemory();

	iopMemWrite32( (addr & 0xfffffffc),
		(( GetRt().UL >> (24 - shift) )) | (mem & (0xffffff00 << shift))
	);
	WritesMemory();

	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)

	*/
}

void InstInterp::SWR()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32(addr & 0xfffffffc);

	ReadsMemory();

	iopMemWrite32( (addr & 0xfffffffc),
		( (GetRt().UL << shift) | (mem & (0x00ffffff >> (24 - shift))) )
	);
	WritesMemory();

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

void InstInterp::MFC0()
{
	SetRt_UL( GetFs().UL );
}

void InstInterp::CFC0()
{
	SetRt_UL( GetFs().UL );
}

void InstInterp::MTC0()
{
	SetFs_UL( GetRt().UL );
}

void InstInterp::CTC0()
{
	SetFs_UL( GetRt().UL );
}

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
void InstInterp::Unknown()
{
	Console::Error("R3000A: Unimplemented op, code=0x%x\n", params _Opcode_ );
}

}
