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

#include "iR3000air.h"

// ------------------------------------------------------------------------
__instinline void R3000A::InstructionConstOpt::DoConditionalBranch( bool cond )
{
	m_IsConstPc = (!ReadsRt() || m_IsConst.Rt) && (!ReadsRs() || m_IsConst.Rs);
	m_BranchTarget = BranchTarget();
	Instruction::DoConditionalBranch( cond );
}

// ------------------------------------------------------------------------
__instinline void R3000A::InstructionConstOpt::RaiseException( uint code )
{
	m_IsConstException = true;
	iopException( code, iopRegs.IsDelaySlot );
	SetNextPC( iopRegs.VectorPC + 4 );
	SetSideEffects();
}

// ------------------------------------------------------------------------
__instinline bool R3000A::InstructionConstOpt::ConditionalException( uint code, bool cond )
{
	// Only handle the exception if it *is* const.  Otherwise pretend like it didn't
	// happen -- the recompiler will check for and handle it in recompiled code.
	m_IsConstException = cond && (!ReadsRt() || m_IsConst.Rt) && (!ReadsRs() || m_IsConst.Rs);
	return InstructionOptimizer::ConditionalException( code, m_IsConstException );
}

// ------------------------------------------------------------------------
__instinline void R3000A::InstructionConstOpt::Assign( const Opcode& opcode, bool constStatus[34] )
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

	m_IsConstPc			= true;
	m_IsConstException	= false;
}

// ------------------------------------------------------------------------
// Updates the const status flags in the given array as according to the register
// modifications performed by this instruction.
//
__instinline bool R3000A::InstructionConstOpt::UpdateConstStatus( bool gpr_IsConst[34] ) const
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
