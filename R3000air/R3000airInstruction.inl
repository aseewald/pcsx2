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

	__instinline void Instruction::SetBranchInst()
	{
		m_IsBranchType = true;
	}

	__instinline void Instruction::DoBranch( u32 jumptarg )
	{
		m_NextPC = jumptarg;
	}

	__instinline void Instruction::DoBranch()
	{
		DoBranch( BranchTarget() );
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

	__instinline void Instruction::RaiseException( uint code )
	{
		iopException( code, iopRegs.IsDelaySlot );
		m_NextPC = iopRegs.VectorPC+4;
	}
}
