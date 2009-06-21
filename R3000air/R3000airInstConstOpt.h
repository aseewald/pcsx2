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

#include "ix86/ix86.h"

namespace R3000A {

using namespace x86Emitter;

//////////////////////////////////////////////////////////////////////////////////////////
//
class InstructionConstOpt : public InstructionOptimizer
{
public:
	// Actual const values of registers on input:
	s32 ConstVal_Rd;
	s32 ConstVal_Rt;
	s32 ConstVal_Rs;
	s32 ConstVal_Hi;
	s32 ConstVal_Lo;

	// fully qualified (absolute) target of this branch (valid for non-register jumps and
	// branches only!)
	u32 m_BranchTarget;
	
protected:
	GprStatus m_IsConst;		// Const status of registers on input
	bool m_IsConstException;	// flagged TRUE for instructions that cause exceptions with certainty.
	bool m_IsConstPc;			// flagged TRUE for instructions that branch unconditionally (m_NextPC is a known const)

public:
	InstructionConstOpt() {}

	InstructionConstOpt( const Opcode& opcode ) :
		InstructionOptimizer( opcode )
	,	m_IsConstException( false )
	,	m_IsConstPc( true )
	{
		m_IsConst.Value = 0;
	}

	void Assign( const Opcode& opcode, bool constStatus[34] );

public:
	__releaseinline void Process()
	{
		Instruction::Process( *this );
		jASSUME( !ReadsRd() );		// Rd should always be a target register.
	}

	void DynarecAssert( bool condition, const char* msg, bool inReleaseMode=false ) const;

	bool UpdateConstStatus( bool gpr_IsConst[34] ) const;
	
	bool IsConstRs() const { return ReadsRs() && m_IsConst.Rs; }
	bool IsConstRt() const { return ReadsRt() && m_IsConst.Rt; }
	bool IsConstField( RegField_t field ) const;
	bool IsConstPc() const { return m_IsConstPc; }
	
	bool CausesConstException() const { return m_IsConstException; }
	
	u32  GetBranchTarget() const { return m_BranchTarget; }
	
protected:
	void DoConditionalBranch( bool cond );
	void RaiseException( uint code );
	bool ConditionalException( uint code, bool cond );
};

//////////////////////////////////////////////////////////////////////////////////////////
// Implementation note: I've separated the Gpr const info into two arrays to have an easier
// time of bit-packing the IsConst array.
//
class InstConstInfoEx
{
public:
	InstructionConstOpt	inst;

	JccComparisonType BranchCompareType;

	// Intermediate Representation dependency indexer, for each valid readable field.
	// The values of this array are indexes for the instructions that the field being
	// read is dependent on.  When re-ordering instructions, this instruction must be
	// ordered *after* any instruction listed here.
	// [not implemented yet]
	int			iRepDep[RF_Count];

	// Set true when the following instruction has a read dependency on the Rd gpr
	// of this instruction.  The regmapper will map the value of Rd prior to instruction
	// execution to a fixed register (and preserve it).  The next instruction in the 
	// list will read Rs or Rt from the Dependency slot instead.
	bool		DelayedDependencyRd;

	//bool		isConstPC:1;
	u8			m_IsConstBits[5];		// 5 bytes to contain 34 bits.

	// Stores all constant status for this instruction.
	// Note: I store all 34 GPRs on purpose, even tho the instruction itself will only
	// need to know it's own regfields at dyngen time.  The rest are used for generating
	// register state info for advanced exception handling and recovery.
	u32			ConstVal[34];
	u32			ConstPC;

	bool IsConst( int gpridx ) const
	{
		return !!( m_IsConstBits[gpridx/8] & (1<<(gpridx&7)) );
	}
};

}