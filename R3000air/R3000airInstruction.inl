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

// r3000airInstruction.inl:  Implements inlinable method members of Instruction and 
// InstructionDiagnostic classes.
//
// Notes on .inl file use in R3000air and Pcsx2 in general: MSVC requires that
// __forceinline members of classes must be implemented in the source files that use
// them (some throwback mess to the pre-LTCG days, that's still in effect due to
// either oversight, laziness, or raw asinine stupidity).   So in effect I have to
// treat forceinline methods like they were templated functions, even though they're not.

#pragma once 

#include "R3000air.h"
#include "IopMem.h"

namespace R3000A {

	// Sets the link to the next instruction in the link register (Ra)
	__instinline void Instruction::SetLink()
	{
		SetLink( GetPC() + 8 );
	}

	// Sets the link to the next instruction (after the delay slot) in the link
	// register
	__instinline void Instruction::SetLinkRd()
	{
		SetRd_UL( GetPC() + 8 );
	}

	__instinline void Instruction::DoConditionalBranch( bool cond )
	{
		GetImm(); m_HasDelaySlot = true;
		SetNextPC( cond ? BranchTarget() : m_VectorPC );
	}

	__instinline void Instruction::RaiseException( uint code )
	{
		iopException( code, m_HasDelaySlot );
		SetNextPC( iopRegs.VectorPC + 4 );
		SetSideEffects();
		SetCausesExceptions();
	}

	__instinline bool Instruction::ConditionalException( uint code, bool cond )
	{
		if( cond )
		{
			iopException( code, m_HasDelaySlot );
			SetNextPC( iopRegs.VectorPC + 4 );
		}
		SetCausesExceptions();
		return cond;
	}
	
	__instinline u8  Instruction::MemoryRead8( u32 addr )  { return iopMemRead8( addr ); }
	__instinline u16 Instruction::MemoryRead16( u32 addr ) { return iopMemRead16( addr ); }
	__instinline u32 Instruction::MemoryRead32( u32 addr ) { return iopMemRead32( addr ); }

	__instinline void Instruction::MemoryWrite8( u32 addr, u8 val )   { iopMemWrite8( addr, val ); }
	__instinline void Instruction::MemoryWrite16( u32 addr, u16 val ) { iopMemWrite16( addr, val ); }
	__instinline void Instruction::MemoryWrite32( u32 addr, u32 val ) { iopMemWrite32( addr, val ); }

	__instinline u8  InstructionDiagnostic::MemoryRead8( u32 addr )  { m_ReadsGPR.Memory = true; return iopMemRead8( addr ); }
	__instinline u16 InstructionDiagnostic::MemoryRead16( u32 addr ) { m_ReadsGPR.Memory = true; return iopMemRead16( addr ); }
	__instinline u32 InstructionDiagnostic::MemoryRead32( u32 addr ) { m_ReadsGPR.Memory = true; return iopMemRead32( addr ); }

	__instinline void InstructionDiagnostic::MemoryWrite8( u32 addr, u8 val )   { m_WritesGPR.Memory = true; iopMemWrite8( addr, val ); }
	__instinline void InstructionDiagnostic::MemoryWrite16( u32 addr, u16 val ) { m_WritesGPR.Memory = true; iopMemWrite16( addr, val ); }
	__instinline void InstructionDiagnostic::MemoryWrite32( u32 addr, u32 val ) { m_WritesGPR.Memory = true; iopMemWrite32( addr, val ); }

	
	//////////////////////////////////////////////////////////////////////////////////////////
	// -- InstructionDiagnostic -- Method Implementations

	__instinline bool InstructionDiagnostic::ConditionalException( uint code, bool cond )
	{
		m_CanCauseExceptions = true;
		return Instruction::ConditionalException( code, cond );
	}

	// returns the index of the GPR for the given field, or -1 if the field is not read
	// by this instruction.
	__instinline MipsGPRs_t InstructionDiagnostic::ReadsField( RegField_t field ) const
	{
		switch( field )
		{
			case RF_Rd: return !ReadsRd() ? GPR_Invalid : _Rd_;
			case RF_Rt: return !ReadsRt() ? GPR_Invalid : _Rt_;
			case RF_Rs: return !ReadsRs() ? GPR_Invalid : _Rs_;

			case RF_Hi: return !ReadsHi() ? GPR_Invalid : GPR_hi;
			case RF_Lo: return !ReadsLo() ? GPR_Invalid : GPR_lo;
			
			case RF_Link: return GPR_Invalid;
			
			jNO_DEFAULT
		}
	}

	// returns the index of the GPR for the given field, or -1 if the field is not written
	// by this instruction.
	__instinline MipsGPRs_t InstructionDiagnostic::WritesField( RegField_t field ) const
	{
		switch( field )
		{
			case RF_Rd: return !WritesRd() ? GPR_Invalid : _Rd_;
			case RF_Rt: return !WritesRt() ? GPR_Invalid : _Rt_;
			case RF_Rs: return !WritesRs() ? GPR_Invalid : _Rs_;

			case RF_Hi: return !WritesHi() ? GPR_Invalid : GPR_hi;
			case RF_Lo: return !WritesLo() ? GPR_Invalid : GPR_lo;
			
			case RF_Link: return !WritesLink() ? GPR_Invalid : GPR_ra;
			
			jNO_DEFAULT
		}
	}

	// gpridx - index of a MIPS general purpose register (0 thru 33) [32/33 are hi/lo]
	// Returns the field the GPR is mapped to, or RF_Unused if the given register is
	// not written to by this opcode.
	__instinline RegField_t InstructionDiagnostic::WritesReg( int gpridx ) const
	{
		if( gpridx == _Rd_ && WritesRd() ) return RF_Rd;
		if( gpridx == _Rt_ && WritesRt() ) return RF_Rt;
		if( gpridx == _Rs_ && WritesRs() ) return RF_Rs;
		
		if( gpridx == GPR_hi && WritesHi() ) return RF_Hi;
		if( gpridx == GPR_lo && WritesLo() ) return RF_Lo;
		
		if( gpridx == GPR_ra && WritesLink() ) return RF_Link;
		
		return RF_Unused;
	}
}
