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
#include "IopMem.h"


namespace R3000A {

	__instinline const bool Instruction::IsBranchType() const
	{
		return m_IsBranchType;
	}

	// Sets the link to the next instruction in the given GPR
	// (defaults to the link register if none specified)
	__instinline void Instruction::SetLink()
	{
		SetLink( _Pc_ + 8 );
	}

	__instinline void Instruction::SetLinkRd()
	{
		SetRd_UL( _Pc_ + 8 );
	}

	__instinline void Instruction::DoConditionalBranch( bool cond )
	{
		GetImm(); m_IsBranchType = true;
		if( cond )
			m_NextPC = BranchTarget();
	}

	__instinline void Instruction::RaiseException( uint code )
	{
		iopException( code, iopRegs.IsDelaySlot );
		m_NextPC = iopRegs.VectorPC+4;
		SetSideEffects();
	}

	__instinline bool Instruction::ConditionalException( uint code, bool cond )
	{
		if( cond )
		{
			iopException( code, iopRegs.IsDelaySlot );
			m_NextPC = iopRegs.VectorPC+4;
		}

		return cond;
	}
	
	__instinline u8  Instruction::MemoryRead8( u32 addr )  { return iopMemRead8( addr ); }
	__instinline u16 Instruction::MemoryRead16( u32 addr ) { return iopMemRead16( addr ); }
	__instinline u32 Instruction::MemoryRead32( u32 addr ) { return iopMemRead32( addr ); }

	__instinline void Instruction::MemoryWrite8( u32 addr, u8 val )   { iopMemWrite8( addr, val ); }
	__instinline void Instruction::MemoryWrite16( u32 addr, u16 val ) { iopMemWrite16( addr, val ); }
	__instinline void Instruction::MemoryWrite32( u32 addr, u32 val ) { iopMemWrite32( addr, val ); }

	__instinline u8  InstructionOptimizer::MemoryRead8( u32 addr )  { m_ReadsGPR.Memory = true; return iopMemRead8( addr ); }
	__instinline u16 InstructionOptimizer::MemoryRead16( u32 addr ) { m_ReadsGPR.Memory = true; return iopMemRead16( addr ); }
	__instinline u32 InstructionOptimizer::MemoryRead32( u32 addr ) { m_ReadsGPR.Memory = true; return iopMemRead32( addr ); }

	__instinline void InstructionOptimizer::MemoryWrite8( u32 addr, u8 val )   { m_WritesGPR.Memory = true; iopMemWrite8( addr, val ); }
	__instinline void InstructionOptimizer::MemoryWrite16( u32 addr, u16 val ) { m_WritesGPR.Memory = true; iopMemWrite16( addr, val ); }
	__instinline void InstructionOptimizer::MemoryWrite32( u32 addr, u32 val ) { m_WritesGPR.Memory = true; iopMemWrite32( addr, val ); }

	
	//////////////////////////////////////////////////////////////////////////////////////////
	// -- InstructionOptimizer -- Method Implementations

	__instinline bool InstructionOptimizer::ConditionalException( uint code, bool cond )
	{
		m_CanCauseExceptions = true;
		return Instruction::ConditionalException( code, cond );
	}

	// returns the index of the GPR for the given field, or -1 if the field is not read
	// by this instruction.
	__instinline MipsGPRs_t InstructionOptimizer::ReadsField( RegField_t field ) const
	{
		switch( field )
		{
			case RF_Rd: return !ReadsRd() ? GPR_Invalid : _Rd_;
			case RF_Rt: return !ReadsRt() ? GPR_Invalid : _Rt_;
			case RF_Rs: return !ReadsRs() ? GPR_Invalid : _Rs_;

			case RF_Hi: return !ReadsHi() ? GPR_Invalid : GPR_hi;
			case RF_Lo: return !ReadsLo() ? GPR_Invalid : GPR_lo;
			
			jNO_DEFAULT
		}
	}

	// returns the index of the GPR for the given field, or -1 if the field is not written
	// by this instruction.
	__instinline MipsGPRs_t InstructionOptimizer::WritesField( RegField_t field ) const
	{
		switch( field )
		{
			case RF_Rd: return !WritesRd() ? GPR_Invalid : _Rd_;
			case RF_Rt: return !WritesRt() ? GPR_Invalid : _Rt_;
			case RF_Rs: return !WritesRs() ? GPR_Invalid : _Rs_;

			case RF_Hi: return !WritesHi() ? GPR_Invalid : GPR_hi;
			case RF_Lo: return !WritesLo() ? GPR_Invalid : GPR_lo;
			
			jNO_DEFAULT
		}
	}

	// gpridx - index of a MIPS general purpose register (0 thru 33) [32/33 are hi/lo]
	// Returns the field the GPR is mapped to, or RF_Unused if the given register is
	// not written to by this opcode.
	__instinline RegField_t InstructionOptimizer::WritesReg( int gpridx ) const
	{
		if( gpridx == _Rd_ && WritesRd() ) return RF_Rd;
		if( gpridx == _Rt_ && WritesRt() ) return RF_Rt;
		if( gpridx == _Rs_ && WritesRs() ) return RF_Rs;
		
		if( gpridx == 32 && WritesHi() ) return RF_Hi;
		if( gpridx == 33 && WritesLo() ) return RF_Lo;
		
		return RF_Unused;
	}
	
	//////////////////////////////////////////////////////////////////////////////////////////
	// -- InstructionConstOpt -- Method Implementations

	__instinline void InstructionConstOpt::DoConditionalBranch( bool cond )
	{
		m_IsConstBranch = (!ReadsRt() || m_IsConst.Rt) && (!ReadsRs() || m_IsConst.Rs);
		Instruction::DoConditionalBranch( cond );
	}

	__instinline void InstructionConstOpt::RaiseException( uint code )
	{
		m_IsConstException = true;
		iopException( code, iopRegs.IsDelaySlot );
		m_NextPC = iopRegs.VectorPC+4;
		SetSideEffects();
	}

	__instinline bool InstructionConstOpt::ConditionalException( uint code, bool cond )
	{
		// Only handle the exception if it *is* const.  Otherwise pretend like it didn't
		// happen -- the recompiler will check for and handle it in recompiled code.
		m_IsConstException = cond && (!ReadsRt() || m_IsConst.Rt) && (!ReadsRs() || m_IsConst.Rs);
		return InstructionOptimizer::ConditionalException( code, m_IsConstException );
	}

	__instinline void InstructionConstOpt::Assign( const Opcode& opcode, bool constStatus[34] )
	{
		InstructionOptimizer::Assign( opcode );
		
		ConstVal_Rd = iopRegs[_Rd_].SL;
		ConstVal_Rt = iopRegs[_Rt_].SL;
		ConstVal_Rs = iopRegs[_Rs_].SL;
		ConstVal_Hi = iopRegs[GPR_hi].SL;
		ConstVal_Lo = iopRegs[GPR_lo].SL;

		m_IsConst.Value = 0;
		m_IsConst.Rd = constStatus[_Rd_];
		m_IsConst.Rt = constStatus[_Rt_];
		m_IsConst.Rs = constStatus[_Rs_];
		m_IsConst.Hi = constStatus[GPR_hi];
		m_IsConst.Lo = constStatus[GPR_lo];

		m_IsConstBranch		= false;
		m_IsConstException	= false;
	}
	
	// Updates the const status flags in the given array as according to the register
	// modifications performed by this instruction.
	__instinline bool InstructionConstOpt::UpdateConstStatus( bool gpr_IsConst[34] ) const
	{
		// if no regs are written then const status will be unchanged
		if( !m_WritesGPR.Value )
			return true;

		// Update const status for registers.  The const status of all written registers is
		// based on the const status of the read registers.  If the operation reads from
		// memory or from an Fs register, then const status is always false.

		bool constStatus;

		if( ReadsMemory() || ReadsFs() )
			constStatus = false;
		else
		{
			constStatus = 
				//(ReadsRd() ? gpr_IsConst[_Rd_] : true) &&		// Rd should never be read.
				(ReadsRt() ? gpr_IsConst[_Rt_] : true) &&
				(ReadsRs() ? gpr_IsConst[_Rs_] : true) &&
				(ReadsHi() ? gpr_IsConst[GPR_hi] : true) &&
				(ReadsLo() ? gpr_IsConst[GPR_lo] : true);
		}

		if( WritesRd() ) gpr_IsConst[_Rd_] = constStatus;
		if( WritesRt() ) gpr_IsConst[_Rt_] = constStatus;
		//if( WritesRs() ) gpr_IsConst[_Rs_] = constStatus;	// Rs should never be written

		jASSUME( gpr_IsConst[0] == true );		// GPR 0 should *always* be const

		if( WritesLink() ) gpr_IsConst[GPR_ra] = constStatus;
		if( WritesHi() ) gpr_IsConst[GPR_hi] = constStatus;
		if( WritesLo() ) gpr_IsConst[GPR_lo] = constStatus;

		return constStatus;
	}

	__instinline bool InstructionConstOpt::IsConstField( RegField_t field ) const
	{
		switch( field )
		{
			case RF_Rd: return false;
			case RF_Rt: return IsConstRt();
			case RF_Rs: return IsConstRs();

			case RF_Hi: return m_IsConst.Hi;
			case RF_Lo: return m_IsConst.Lo;

			jNO_DEFAULT
		}
	}
}
