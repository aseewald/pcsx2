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

void InstInterp::BGEZ()    { SetBranchInst(); if( RsValue().SL >= 0 ) DoBranch();  } // Branch if Rs >= 0
void InstInterp::BGTZ()    { SetBranchInst(); if( RsValue().SL > 0 ) DoBranch();   } // Branch if Rs >  0
void InstInterp::BLEZ()    { SetBranchInst(); if( RsValue().SL <= 0 ) DoBranch();  } // Branch if Rs <= 0
void InstInterp::BLTZ()    { SetBranchInst(); if( RsValue().SL < 0 ) DoBranch();   }  // Branch if Rs <  0
void InstInterp::BGEZAL()  { SetLink(); BGEZ(); } // Branch if Rs >= 0 and link
void InstInterp::BLTZAL()  { SetLink(); BLTZ(); }  // Branch if Rs <  0 and link


/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
void InstInterp::BEQ()	 { SetBranchInst(); if( RsValue().SL == RtValue().SL ) DoBranch();  } // Branch if Rs == Rt
void InstInterp::BNE()	 { SetBranchInst(); if( RsValue().SL != RtValue().SL ) DoBranch();  } // Branch if Rs != Rt


/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void InstInterp::J()    { SetBranchInst(); DoBranch( JumpTarget() );  }
void InstInterp::JAL()  { SetLink(); J();  }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void InstInterp::JR()   { SetBranchInst(); DoBranch( RsValue().UL ); }
void InstInterp::JALR() { SetLinkRd(); JR(); }


/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

// Rt = Rs + Im 	(Exception on Integer Overflow)
void InstInterp::ADDI()
{
	s64 result = (s64)RsValue().SL + Imm();
	_OverflowCheck( *this, result );

	if(!_Rt_) return;
	RtValue().SL = (s32)result;
	SetConstRt( IsConstRs() );
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
	if( !_Rt_ ) return;
	RtValue().SL = RsValue().SL + Imm();
	SetConstRt( IsConstRs() );
}

void InstInterp::ANDI()	// Rt = Rs And Im
{
	if (!_Rt_) return; 
	RtValue().UL = RsValue().UL & ImmU();
	SetConstRt( IsConstRs() );
}

void InstInterp::ORI()		// Rt = Rs Or  Im
{
	if (!_Rt_) return;
	RtValue().UL = RsValue().UL | ImmU();
	SetConstRt( IsConstRs() );
}

void InstInterp::XORI()	// Rt = Rs Xor Im
{
	if (!_Rt_) return;
	RtValue().UL = RsValue().UL ^ ImmU();
	SetConstRt( IsConstRs() );
}

void InstInterp::SLTI()	// Rt = Rs < Im		(Signed)
{
	if (!_Rt_) return;
	RtValue().SL = (RsValue().SL < Imm() ) ? 1 : 0;
	SetConstRt( IsConstRs() );
}

void InstInterp::SLTIU()	// Rt = Rs < Im		(Unsigned)
{
	if (!_Rt_) return;
	RtValue().UL = (RsValue().UL < (u32)Imm()) ? 1 : 0; 
	SetConstRt( IsConstRs() );
}

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
// Rd = Rs + Rt		(Exception on Integer Overflow)
void InstInterp::ADD()
{
	s64 result = (s64)RsValue().SL + RtValue().SL;
	
	_OverflowCheck( *this, result );
	if (!_Rd_) return;
	
	RdValue().SL = (s32)result;
	SetConstRd( IsConstRs() && IsConstRt() );
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
void InstInterp::SUB()
{
	s64 result = (s64)RsValue().SL - RtValue().SL;

	_OverflowCheck( *this, result );
	if (!_Rd_) return;

	RdValue().SL = (s32)result;
	SetConstRd( IsConstRs() && IsConstRt() );
}

void InstInterp::ADDU()	// Rd = Rs + Rt
{
	if(!_Rd_) return;
	RdValue().SL = RsValue().SL + RtValue().SL;
	SetConstRd_OnRsRt();
}

void InstInterp::SUBU()	// Rd = Rs - Rt
{
	if(!_Rd_) return;
	RdValue().SL = RsValue().SL - RtValue().SL;
	SetConstRd_OnRsRt();
}

void InstInterp::AND()		// Rd = Rs And Rt
{
	if(!_Rd_) return;
	RdValue().UL = RsValue().UL & RtValue().UL;
	SetConstRd_OnRsRt();
}

void InstInterp::OR()		// Rd = Rs Or  Rt
{
	if(!_Rd_) return;
	RdValue().UL = RsValue().UL | RtValue().UL;
	SetConstRd_OnRsRt();
}
void InstInterp::XOR()		// Rd = Rs Xor Rt
{
	if(!_Rd_) return;
	RdValue().UL = RsValue().UL ^ RtValue().UL;
	SetConstRd_OnRsRt();
}
void InstInterp::NOR()		// Rd = Rs Nor Rt
{
	if(!_Rd_) return;
	RdValue().UL =~(RsValue().UL | RtValue().UL);
	SetConstRd_OnRsRt();
}
void InstInterp::SLT()		// Rd = Rs < Rt		(Signed)
{
	if(!_Rd_) return;
	RdValue().SL = (RsValue().SL < RtValue().SL) ? 1 : 0;
	SetConstRd_OnRsRt();
}
void InstInterp::SLTU()	// Rd = Rs < Rt		(Unsigned)
{
	if(!_Rd_) return;
	RdValue().UL = (RsValue().UL < RtValue().UL) ? 1 : 0;
	SetConstRd_OnRsRt();
}


/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void InstInterp::DIV()
{
	// If Rt is zero, then the result is undefined.
	// Which means we can safely ignore the instruction entirely. :D

	if( RtValue().UL == 0 ) return;

	LoValue().UL = RsValue().SL / RtValue().SL;
	HiValue().UL = RsValue().SL % RtValue().SL;

	SetConstHi( IsConstRs() && IsConstRt() );
	SetConstLo( IsConstRs() && IsConstRt() );
	
	return;
}

void InstInterp::DIVU()
{
	if( RtValue().UL == 0 ) return;

	LoValue().UL = RsValue().UL / RtValue().UL;
	HiValue().UL = RsValue().UL % RtValue().UL;

	SetConstHi( IsConstRs() && IsConstRt() );
	SetConstLo( IsConstRs() && IsConstRt() );
}

void InstInterp::MultHelper( u64 result )
{
	LoValue().UL = (u32)result;
	HiValue().UL = (u32)(result >> 32);

	SetConstHi( IsConstRs() && IsConstRt() );
	SetConstLo( IsConstRs() && IsConstRt() );
}

void InstInterp::MULT()
{
	MultHelper( (s64)RsValue().SL * RtValue().SL );
}

void InstInterp::MULTU()
{
	MultHelper( (u64)RsValue().UL * RtValue().UL );
}

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void InstInterp::SLL()		// Rd = Rt << sa
{
	if( !_Rd_ ) return;
	RdValue().UL = RtValue().UL << Sa();
	SetConstRd( IsConstRt() );
}

void InstInterp::SRA()		// Rd = Rt >> sa (arithmetic) [signed]
{
	if( !_Rd_ ) return;
	RdValue().SL = RtValue().SL >> Sa();
	SetConstRd( IsConstRt() );
}

void InstInterp::SRL()		// Rd = Rt >> sa (logical) [unsigned]
{
	if( !_Rd_ ) return;
	RdValue().UL = RtValue().UL >> Sa();
	SetConstRd( IsConstRt() );
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
	if( !_Rd_ ) return;
	RdValue().UL = RtValue().UL << (RsValue().UL & 0x1f);
	SetConstRd( IsConstRs() && IsConstRt() );
} 

void InstInterp::SRAV()	// Rd = Rt >> rs (arithmetic)
{
	if( !_Rd_ ) return;
	RdValue().SL = RtValue().SL >> (RsValue().UL & 0x1f);
	SetConstRd( IsConstRs() && IsConstRt() );
}

void InstInterp::SRLV()	// Rd = Rt >> rs (logical)
{
	if( !_Rd_ ) return;
	RdValue().UL = RtValue().UL >> (RsValue().UL & 0x1f);
	SetConstRd( IsConstRs() && IsConstRt() );
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void InstInterp::LUI()	// Rt = Im << 16  (lower 16 bits zeroed)
{
	if( !_Rt_ ) return;
	RtValue().SL = Imm() << 16;
	SetConstRd( true );
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void InstInterp::MFHI()	// Rd = Hi
{
	if( !_Rd_ ) return;
	RdValue() = HiValue();
	SetConstRd( IsConstHi() );
}

void InstInterp::MFLO()	 // Rd = Lo
{
	if (!_Rd_) return;
	RdValue() = LoValue();
	SetConstRd( IsConstLo() );
}

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void InstInterp::MTHI()	// Hi = Rs
{
	HiValue() = RsValue();
	SetConstHi( IsConstRs() );
}
void InstInterp::MTLO()	// Lo = Rs
{
	LoValue() = RsValue();
	SetConstLo( IsConstRs() );
}

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
// Break exception - psx rom doesn't handle this
void InstInterp::BREAK()
{
	//iopRegs.pc -= 4;
	iopException( IopExcCode::Breakpoint, iopRegs.IsDelaySlot );
	m_NextPC = iopRegs.VectorPC+4;

	//assert(0);

	// TODO : Implement!
	//throw R3000Exception::Break();
}

void InstInterp::SYSCALL()
{
	iopException( IopExcCode::Syscall, iopRegs.IsDelaySlot );
	m_NextPC = iopRegs.VectorPC+4;
}

void InstInterp::RFE()
{
//	SysPrintf("RFE\n");
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
	if( !_Rt_ ) return;

	RtValue().SL = result;
	SetConstRt( false );
}

void InstInterp::LBU()
{
	const u32 addr = AddrImm();
	u8 result = iopMemRead8( addr );
	if( !_Rt_ ) return;

	RtValue().UL = result;
	SetConstRt( false );
}

// Load half-word (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void InstInterp::LH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );
	
	s16 result = iopMemRead16( addr );
	if( !_Rt_ ) return;

	RtValue().SL = result;
	SetConstRt( false );
}

// Load Halfword Unsigned (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void InstInterp::LHU()
{
	const u32 addr = AddrImm();

	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );

	u16 result = iopMemRead16( addr );
	if( !_Rt_ ) return;

	RtValue().UL = result;
	SetConstRt( false );
}

// Load Word (32 bits)
// AddressError exception if the address is not 32-bit aligned.
void InstInterp::LW()
{
	const u32 addr = AddrImm();

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, false );

	s32 result = iopMemRead32( addr );
	if( !_Rt_ ) return;

	RtValue().SL = result;
	SetConstRt( false );
}

// Load Word Left (portion loaded determined by address lower 2 bits)
// No exception is thrown if the address is unaligned.
void InstInterp::LWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32( addr & 0xfffffffc );

	if (!_Rt_) return;
	RtValue().UL = ( RtValue().UL & (0x00ffffff >> shift) ) | ( mem << (24 - shift) );
	SetConstRt( false );

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

	if (!_Rt_) return;
	RtValue().UL = ( RtValue().UL & (0xffffff00 << (24-shift)) ) | (mem >> shift);
	SetConstRt( false );

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
	iopMemWrite8( AddrImm(), (u8)RtValue().UL );
}

void InstInterp::SH()
{
	const u32 addr = AddrImm();
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite16( addr, RtValue().US[0] );
}

void InstInterp::SW()
{
	const u32 addr = AddrImm();


	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite32( addr, RtValue().UL );
}

// Store Word Left
// No Address Error Exception occurs.
void InstInterp::SWL()
{
	const u32 addr = AddrImm();
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32(addr & 0xfffffffc);

	iopMemWrite32( (addr & 0xfffffffc),
		(( RtValue().UL >> (24 - shift) )) | (mem & (0xffffff00 << shift))
	);
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

	iopMemWrite32( (addr & 0xfffffffc),
		( (RtValue().UL << shift) | (mem & (0x00ffffff >> (24 - shift))) )
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

void InstInterp::MFC0()
{
	if( !_Rt_ ) return;
	RtValue() = FsValue();
}

void InstInterp::CFC0()
{
	if( !_Rt_ ) return;
	RtValue() = FsValue();
}

void InstInterp::MTC0()
{
	FsValue() = RtValue();
}

void InstInterp::CTC0()
{
	FsValue() = RtValue();
}

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
void InstInterp::Unknown()
{
	Console::Error("R3000A: Unimplemented op, code=0x%x\n", params U32 );
}

}
