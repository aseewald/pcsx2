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

#include "R3000air.h"
#include "IopMem.h"


namespace R3000Air {

	const bool Instruction::IsBranchType() const
	{
		return m_IsBranchType;

		/*switch(_Opcode_)
		{
			case 0: // special
				return _Funct_ == 8 || _Funct_ == 9;
			break;

			case 1: // regimm
				return _Rt_ == 0 || _Rt_ == 1 || _Rt_ == 16 || _Rt_ == 17;
			break;

			// J / JAL / Branches
			case 2:
			case 3:
			case 4: case 5: case 6: case 7: 
				return true;
		}
		return false;*/
	}

	Instruction::Instruction( u32 srcPc, GprConstStatus constStatus, bool isDelaySlot ) :
		U32( iopMemRead32( srcPc ) )
	,	_Funct_( Funct() )
	,	_Sa_( Sa() )
	,	_Rd_( Rd() )
	,	_Rt_( Rt() )
	,	_Rs_( Rs() )
	,	_Opcode_( Opcode() )
	,	_Pc_( srcPc )
	,	IsDelaySlot( isDelaySlot )

	,	IsConstInput( constStatus )
	,	IsConstOutput( constStatus )

	,	RdValue( (IntSign32&)iopRegs.GPR.r[_Rd_] )
	,	RsValue( (IntSign32&)iopRegs.GPR.r[_Rs_] )
	,	RtValue( (IntSign32&)iopRegs.GPR.r[_Rt_] )

	,	m_IsBranchType( false )
	,	m_Branching( false )
	,	m_NextPC( _Pc_ + 4 )
	
	,	m_Syntax( NULL )
	{
	}

	Instruction::Instruction( u32 srcPc, bool isDelaySlot ) :
		U32( iopMemRead32( srcPc ) )
	,	_Funct_( Funct() )
	,	_Sa_( Sa() )
	,	_Rd_( Rd() )
	,	_Rt_( Rt() )
	,	_Rs_( Rs() )
	,	_Opcode_( Opcode() )
	,	_Pc_( srcPc )
	,	IsDelaySlot( isDelaySlot )

	,	IsConstInput()
	,	IsConstOutput()

	,	RdValue( (IntSign32&)iopRegs.GPR.r[_Rd_] )
	,	RsValue( (IntSign32&)iopRegs.GPR.r[_Rs_] )
	,	RtValue( (IntSign32&)iopRegs.GPR.r[_Rt_] )
	
	,	m_IsBranchType( false )
	,	m_Branching( false )
	,	m_NextPC( _Pc_ + 4 )
	
	,	m_Syntax( NULL )
	{
	}

	// Applies the const flag to Rd based on the const status of Rs and Rt;
	void Instruction::SetConstRd_OnRsRt()
	{
		IsConstOutput.Rd = IsConstInput.Rs && IsConstInput.Rt;
	}

	// Sets the link to the next instruction in the given GPR
	// (defaults to the link register if none specified)
	void Instruction::SetLink()
	{
		iopRegs.GPR.n.ra.UL = _Pc_ + 8;
		IsConstOutput.Link = true;
	}

	void Instruction::SetLinkRd()
	{
		if( !_Rd_ ) return;
		RdValue.UL = _Pc_ + 8;
		IsConstOutput.Rd = true;
	}

	void Instruction::SetBranchInst()
	{
		m_IsBranchType = true;
		m_NextPC = _Pc_ + 8;		// skips the delay slot
	}

	void Instruction::DoBranch( u32 jumptarg )
	{
		m_Branching = true;
		m_NextPC = jumptarg;
	}

	void Instruction::DoBranch()
	{
		DoBranch( BranchTarget() );
	}
}
