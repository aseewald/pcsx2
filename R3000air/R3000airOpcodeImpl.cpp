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
static __forceinline void _OverflowCheck( const Instruction& inst, s64 result )
{
	// This 32bit method can rely on the MIPS documented method of checking for
	// overflow, which simply compares bit 32 (rightmost bit of the upper word),
	// against bit 31 (leftmost of the lower word).

	//assert( 0 );

	// If bit32 != bit31 then we have an overflow.
	if( !!(result & ((u64)1<<32)) != !!(result & (1UL<<31)) )
		throw R3000Exception::Overflow( inst );
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void Instruction::BGEZ()    { SetBranchInst(); if( RsValue.SL >= 0 ) DoBranch();  } // Branch if Rs >= 0
void Instruction::BGTZ()    { SetBranchInst(); if( RsValue.SL > 0 ) DoBranch();   } // Branch if Rs >  0
void Instruction::BLEZ()    { SetBranchInst(); if( RsValue.SL <= 0 ) DoBranch();  } // Branch if Rs <= 0
void Instruction::BLTZ()    { SetBranchInst(); if( RsValue.SL < 0 ) DoBranch();   }  // Branch if Rs <  0
void Instruction::BGEZAL()  { SetLink(); BGEZ(); } // Branch if Rs >= 0 and link
void Instruction::BLTZAL()  { SetLink(); BLTZ(); }  // Branch if Rs <  0 and link


void Instruction::BGEZL()   { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BGEZ(); }
void Instruction::BGTZL()   { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BGTZ(); }
void Instruction::BLEZL()   { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BLEZ(); }
void Instruction::BLTZL()   { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BLTZ(); }
void Instruction::BGEZALL() { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BGEZAL(); }
void Instruction::BLTZALL() { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BLTZAL(); }


/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
void Instruction::BEQ()	 { SetBranchInst(); if( RsValue.SL == RtValue.SL ) DoBranch();  } // Branch if Rs == Rt
void Instruction::BNE()	 { SetBranchInst(); if( RsValue.SL != RtValue.SL ) DoBranch();  } // Branch if Rs != Rt

void Instruction::BEQL() { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BEQ(); }
void Instruction::BNEL() { Console::Error( "R3000A Unimplemented Op: Branch Likely." ); BNE(); }


/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void Instruction::J()    { SetBranchInst(); DoBranch( JumpTarget() );  }
void Instruction::JAL()  { SetLink(); J();  }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void Instruction::JR()   { SetBranchInst(); DoBranch( RsValue.UL ); }
void Instruction::JALR() { SetLinkRd(); JR(); }


/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

// Rt = Rs + Im 	(Exception on Integer Overflow)
void Instruction::ADDI()
{
	s64 result = RsValue.SL + Imm();
	_OverflowCheck( *this, result );

	if(!_Rt_) return;
	RtValue.UL = (u32)result;
	IsConstOutput.Rt = IsConstInput.Rs;
}

// Rt = Rs + Im (no exception)
// This is exactly like ADDI but does not perform an overflow exception check.
void Instruction::ADDIU()
{
	/*if( !_Rt_ )
	{
		zeroEx();
		return;
	}*/
	RtValue.SL = RsValue.SL + Imm();
	IsConstOutput.Rt = IsConstInput.Rs;
}

void Instruction::ANDI()	// Rt = Rs And Im
{  if (!_Rt_) return;  RtValue.UL = RsValue.UL & ImmU(); IsConstOutput.Rt = IsConstInput.Rs;  }
void Instruction::ORI()		// Rt = Rs Or  Im
{  if (!_Rt_) return;  RtValue.UL = RsValue.UL | ImmU(); IsConstOutput.Rt = IsConstInput.Rs;  }
void Instruction::XORI()	// Rt = Rs Xor Im
{  if (!_Rt_) return;  RtValue.UL = RsValue.UL ^ ImmU(); IsConstOutput.Rt = IsConstInput.Rs;  }
void Instruction::SLTI()	// Rt = Rs < Im		(Signed)
{  if (!_Rt_) return;  RtValue.SL = (RsValue.SL < Imm() ) ? 1 : 0; IsConstOutput.Rt = IsConstInput.Rs;  }
void Instruction::SLTIU()	// Rt = Rs < Im		(Unsigned)
{  if (!_Rt_) return;  RtValue.UL = (RsValue.UL < ImmU()) ? 1 : 0; IsConstOutput.Rt = IsConstInput.Rs;  }

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
// Rd = Rs + Rt		(Exception on Integer Overflow)
void Instruction::ADD()
{
	if (!_Rd_) return;
	s64 result = RsValue.SL + RtValue.SL;
	
	_OverflowCheck( *this, result );
	
	RdValue.UL = (u32)result;
	IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
void Instruction::SUB()
{
	if (!_Rd_) return;
	u64 result = RsValue.UL - RtValue.UL;

	_OverflowCheck( *this, result );

	RdValue.UL = (u32)result;
	IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
}

void Instruction::ADDU()	// Rd = Rs + Rt
{  if(!_Rd_) return;  RdValue.SL = RsValue.SL + RtValue.SL; SetConstRd_OnRsRt(); }
void Instruction::SUBU()	// Rd = Rs - Rt
{  if(!_Rd_) return;  RdValue.SL = RsValue.SL - RtValue.SL; SetConstRd_OnRsRt(); }
void Instruction::AND()		// Rd = Rs And Rt
{  if(!_Rd_) return;  RdValue.UL = RsValue.UL & RtValue.UL; SetConstRd_OnRsRt(); }
void Instruction::OR()		// Rd = Rs Or  Rt
{  if(!_Rd_) return;  RdValue.UL = RsValue.UL | RtValue.UL; SetConstRd_OnRsRt(); }
void Instruction::XOR()		// Rd = Rs Xor Rt
{  if(!_Rd_) return;  RdValue.UL = RsValue.UL ^ RtValue.UL; SetConstRd_OnRsRt(); }
void Instruction::NOR()		// Rd = Rs Nor Rt
{  if(!_Rd_) return;  RdValue.UL =~(RsValue.UL | RtValue.UL); SetConstRd_OnRsRt(); }
void Instruction::SLT()		// Rd = Rs < Rt		(Signed)
{  if(!_Rd_) return;  RdValue.SL = (RsValue.SL < RtValue.SL) ? 1 : 0; SetConstRd_OnRsRt(); }
void Instruction::SLTU()	// Rd = Rs < Rt		(Unsigned)
{  if(!_Rd_) return;  RdValue.UL = (RsValue.UL < RtValue.UL) ? 1 : 0; SetConstRd_OnRsRt(); }


/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void Instruction::DIV()
{
	// If Rt is zero, then the result is undefined.
	// Which means we can safely ignore the instruction entirely. :D

	if( RtValue.UL == 0 ) return;

	LoValue().UL = RsValue.SL / RtValue.SL;
	HiValue().UL = RsValue.SL % RtValue.SL;

	IsConstOutput.Hi = IsConstInput.Rs && IsConstInput.Rt;
	IsConstOutput.Lo = IsConstInput.Rs && IsConstInput.Rt;
	
	return;
}

void Instruction::DIVU()
{
	if( RtValue.UL == 0 ) return;

	LoValue().UL = RsValue.UL / RtValue.UL;
	HiValue().UL = RsValue.UL % RtValue.UL;

	IsConstOutput.Hi = IsConstInput.Rs && IsConstInput.Rt;
	IsConstOutput.Lo = IsConstInput.Rs && IsConstInput.Rt;
}

void Instruction::MultHelper( u64 result )
{
	LoValue().UL = (u32)result;
	HiValue().UL = (u32)(result >> 32);

	IsConstOutput.Hi = IsConstInput.Rs && IsConstInput.Rt;
	IsConstOutput.Lo = IsConstInput.Rs && IsConstInput.Rt;
}

void Instruction::MULT()
{
	MultHelper( (s64)RsValue.SL * RtValue.SL );
}

void Instruction::MULTU()
{
	MultHelper( (u64)RsValue.UL * RtValue.UL );
}

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void Instruction::SLL()		// Rd = Rt << sa
{
	if( !_Rd_ ) return;
	RdValue.UL = RtValue.UL << _Sa_;
	IsConstOutput.Rd = IsConstInput.Rt;
}

void Instruction::SRA()		// Rd = Rt >> sa (arithmetic) [signed]
{
	if( !_Rd_ ) return;
	RdValue.SL = RtValue.SL >> _Sa_;
	IsConstOutput.Rd = IsConstInput.Rt;
}

void Instruction::SRL()		// Rd = Rt >> sa (logical) [unsigned]
{
	if( !_Rd_ ) return;
	RdValue.UL = RtValue.UL >> _Sa_;
	IsConstOutput.Rd = IsConstInput.Rt;
}

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/

// Implementation Wanderings:
//   According to modern MIPS cpus, the upper buts of the Rs register are
//   ignored during the shift (only bottom 5 bits matter).  Old interpreters
//   did not take this into account.  Nut sure if by design or if by bug.

void Instruction::SLLV()	// Rd = Rt << rs
{
	if( !_Rd_ ) return;
	RdValue.UL = RtValue.UL << (RsValue.UL & 0x1f);
	IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
} 

void Instruction::SRAV()	// Rd = Rt >> rs (arithmetic)
{
	if( !_Rd_ ) return;
	RdValue.SL = RtValue.SL >> (RsValue.UL & 0x1f);
	IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
}

void Instruction::SRLV()	// Rd = Rt >> rs (logical)
{
	if( !_Rd_ ) return;
	RdValue.UL = RtValue.UL >> (RsValue.UL & 0x1f);
	IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void Instruction::LUI()	// Rt = Im << 16  (lower 16 bits zeroed)
{
	if( !_Rt_ ) return;
	RtValue.SL = Imm() << 16;
	IsConstOutput.Rd = true;
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void Instruction::MFHI()	// Rd = Hi
{
	if( !_Rd_ ) return;
	RdValue = HiValue();
	IsConstOutput.Rd = IsConstInput.Hi;
}

void Instruction::MFLO()	 // Rd = Lo
{
	if (!_Rd_) return;
	RdValue = LoValue();
	IsConstOutput.Rd = IsConstInput.Lo;
}

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void Instruction::MTHI()	// Hi = Rs
{
	HiValue() = RsValue;
	IsConstOutput.Hi = IsConstInput.Rs;
}
void Instruction::MTLO()	// Lo = Rs
{
	LoValue() = RsValue;
	IsConstOutput.Lo = IsConstInput.Rs;
}

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
// Break exception - psx rom doens't handles this
void Instruction::BREAK()
{
	//iopRegs.pc -= 4;
	//psxException(0x24, IsDelaySlot);

	assert(0);

	// TODO : Implement!
	//throw R3000Exception::Break();
}

void Instruction::SYSCALL()
{
	//iopRegs.pc -= 4;
	iopException(IopExcCode::Syscall, IsDelaySlot);

	//throw R3000Exception::SystemCall( *this );
}

void Instruction::RFE()
{
//	SysPrintf("RFE\n");
	iopRegs.CP0.n.Status =
		(iopRegs.CP0.n.Status & 0xfffffff0) | ((iopRegs.CP0.n.Status & 0x3c) >> 2);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define AddrImm (RsValue.UL + Imm())

void Instruction::LB()
{
	const u32 addr = AddrImm;
	s8 result = iopMemRead8( addr );
	if( !_Rt_ ) return;

	RtValue.SL = result;
	IsConstOutput.Rt = false;
}

void Instruction::LBU()
{
	const u32 addr = AddrImm;
	u8 result = iopMemRead8( addr );
	if( !_Rt_ ) return;

	RtValue.UL = result;
	IsConstOutput.Rt = false;
}

// Load half-word (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void Instruction::LH()
{
	const u32 addr = AddrImm;
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );
	
	s16 result = iopMemRead16( addr );
	if( !_Rt_ ) return;

	RtValue.SL = result;
	IsConstOutput.Rt = false;
}

// Load Halfword Unsigned (16 bits)
// AddressError exception if the address is not 16-bit aligned.
void Instruction::LHU()
{
	const u32 addr = AddrImm;

	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, false );

	u16 result = iopMemRead16( addr );
	if( !_Rt_ ) return;

	RtValue.UL = result;
	IsConstOutput.Rt = false;
}

// Load Word (32 bits)
// AddressError exception if the address is not 32-bit aligned.
void Instruction::LW()
{
	const u32 addr = AddrImm;

	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, false );

	s32 result = iopMemRead32( addr );
	if( !_Rt_ ) return;

	RtValue.SL = result;
	IsConstOutput.Rt = false;
}

// Load Word Left (portion loaded determined by address lower 2 bits)
// No exception is thrown if the address is unaligned.
void Instruction::LWL()
{
	const u32 addr = AddrImm;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32( addr & 0xfffffffc );

	if (!_Rt_) return;
	RtValue.UL = ( RtValue.UL & (0x00ffffff >> shift) ) | ( mem << (24 - shift) );
	IsConstOutput.Rt = false;

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
void Instruction::LWR()
{
	const u32 addr = AddrImm;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32( addr & 0xfffffffc );

	if (!_Rt_) return;
	RtValue.UL = ( RtValue.UL & (0xffffff00 << (24-shift)) ) | (mem >> shift);
	IsConstOutput.Rt = false;

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)

	*/
}

void Instruction::SB()
{
	iopMemWrite8( AddrImm, (u8)RtValue.UL );
}

void Instruction::SH()
{
	const u32 addr = AddrImm;
	
	if( addr & 1 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite16( addr, RtValue.US[0] );
}

void Instruction::SW()
{
	const u32 addr = AddrImm;


	if( addr & 3 )
		throw R3000Exception::AddressError( *this, addr, true );

	iopMemWrite32( addr, RtValue.UL );
}

// Store Word Left
// No Address Error Exception occurs.
void Instruction::SWL()
{
	const u32 addr = AddrImm;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32(addr & 0xfffffffc);

	iopMemWrite32( (addr & 0xfffffffc),
		(( RtValue.UL >> (24 - shift) )) | (mem & (0xffffff00 << shift))
	);
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)

	*/
}

void Instruction::SWR()
{
	const u32 addr = AddrImm;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = iopMemRead32(addr & 0xfffffffc);

	iopMemWrite32( (addr & 0xfffffffc),
		( (RtValue.UL << shift) | (mem & (0x00ffffff >> (24 - shift))) )
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

void Instruction::MFC0()
{
	if( !_Rt_ ) return;
	RtValue = FsValue();
}

// This is an undocumented instruction? Is it implemented correctly? -air
void Instruction::CFC0()
{
	if( !_Rt_ ) return;
	RtValue = FsValue();
}

void Instruction::MTC0()
{
	FsValue() = RtValue;
}

// This is an undocumented instruction? Is it implemented correctly? -air
void Instruction::CTC0()
{
	FsValue() = RtValue;
}

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
void Instruction::Unknown()
{
	Console::Error("R3000A: Unimplemented op, code=0x%x\n", params U32 );
}

/*********************************************************
* Register trap                                          *
* Format:  OP rs, rt                                     *
*********************************************************/
void Instruction::TGE()  { if( RsValue.SL >= RtValue.SL) throw R3000Exception::Trap(*this, TrapCode()); }
void Instruction::TGEU() { if( RsValue.UL >= RtValue.UL) throw R3000Exception::Trap(*this, TrapCode()); }
void Instruction::TLT()  { if( RsValue.SL <  RtValue.SL) throw R3000Exception::Trap(*this, TrapCode()); }
void Instruction::TLTU() { if( RsValue.UL <  RtValue.UL) throw R3000Exception::Trap(*this, TrapCode()); }
void Instruction::TEQ()  { if( RsValue.SL == RtValue.SL) throw R3000Exception::Trap(*this, TrapCode()); }
void Instruction::TNE()  { if( RsValue.SL != RtValue.SL) throw R3000Exception::Trap(*this, TrapCode()); }

/*********************************************************
* Trap with immediate operand                            *
* Format:  OP rs, imm                                    *
*********************************************************/
void Instruction::TGEI()  { if( RsValue.SL >= RtValue.SL) throw R3000Exception::Trap(*this); }
void Instruction::TGEIU() { if( RsValue.UL >= RtValue.UL) throw R3000Exception::Trap(*this); }
void Instruction::TLTI()  { if( RsValue.SL <  RtValue.SL) throw R3000Exception::Trap(*this); }
void Instruction::TLTIU() { if( RsValue.UL <  RtValue.UL) throw R3000Exception::Trap(*this); }
void Instruction::TEQI()  { if( RsValue.SL == RtValue.SL) throw R3000Exception::Trap(*this); }
void Instruction::TNEI()  { if( RsValue.SL != RtValue.SL) throw R3000Exception::Trap(*this); }

}
