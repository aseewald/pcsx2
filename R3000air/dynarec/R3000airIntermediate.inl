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
__instinline void R3000A::InstructionConstOptimizer::DoConditionalBranch( bool cond )
{
	m_IsConstPc = (!ReadsRt() || IsConstGpr[_Rt_]) && (!ReadsRs() || IsConstGpr[_Rs_]);
	Instruction::DoConditionalBranch( cond );
}

// ------------------------------------------------------------------------
__instinline void R3000A::InstructionConstOptimizer::RaiseException( uint code )
{
	m_IsConstException = true;
	iopException( code, iopRegs.IsDelaySlot );
	SetNextPC( iopRegs.VectorPC + 4 );
	SetSideEffects();
}

// ------------------------------------------------------------------------
__instinline bool R3000A::InstructionConstOptimizer::ConditionalException( uint code, bool cond )
{
	// Only handle the exception if it *is* const.  Otherwise pretend like it didn't
	// happen -- the recompiler will check for and handle it in recompiled code.
	m_IsConstException = cond && (!ReadsRt() || IsConstGpr[_Rt_]) && (!ReadsRs() || IsConstGpr[_Rs_]);
	m_IsConstPc = false;

	return InstructionDiagnostic::ConditionalException( code, m_IsConstException );
}

// ------------------------------------------------------------------------
__instinline void R3000A::InstructionConstOptimizer::Assign( const Opcode& opcode, const GprConstStatus& constStatus )
{
	InstructionDiagnostic::Assign( opcode );

	IsConstGpr = constStatus;

	m_IsConstPc			= true;
	m_IsConstException	= false;
}

// ------------------------------------------------------------------------
// Updates the const status flags in the given array as according to the register
// modifications performed by this instruction.
//
__instinline bool R3000A::InstructionConstOptimizer::UpdateExternalConstStatus( GprConstStatus& gpr_IsConst ) const
{
	// if no regs are written then const status will be unchanged
	if( !m_WritesGPR.Value )
		return true;

	// Update const status for registers.  The const status of all written registers is
	// based on the const status of the read registers.  If the operation reads from
	// memory or from an Fs register, then const status is always false, since status of
	// those sources is not tracked (and thusly considered non-const at all times)

	bool constStatus;

	if( ReadsMemory() || ReadsFs() )
		constStatus = false;
	else
	{
		constStatus = 
			(ReadsRt() ? gpr_IsConst[_Rt_] : true) &&
			(ReadsRs() ? gpr_IsConst[_Rs_] : true) &&
			(ReadsHi() ? gpr_IsConst[GPR_hi] : true) &&
			(ReadsLo() ? gpr_IsConst[GPR_lo] : true);
	}

	jASSUME( gpr_IsConst[GPR_r0] );		// GPR 0 should *always* be const

	if( WritesRd() )	gpr_IsConst.Set(_Rd_, constStatus);
	if( WritesRt() )	gpr_IsConst.Set(_Rt_, constStatus);

	if( WritesLink() )	gpr_IsConst.Set(GPR_ra, constStatus);
	if( WritesHi() )	gpr_IsConst.Set(GPR_hi, constStatus);
	if( WritesLo() )	gpr_IsConst.Set(GPR_lo, constStatus);

	return constStatus;
}
