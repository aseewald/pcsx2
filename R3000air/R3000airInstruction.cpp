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

	// Sets the link to the next instruction in the given GPR
	// (defaults to the link register if none specified)
	void Instruction::SetLink()
	{
		SetLink( _Pc_ + 8 );
	}

	void Instruction::SetLinkRd()
	{
		SetRd_UL( _Pc_ + 8 );
	}

	void Instruction::SetBranchInst()
	{
		m_IsBranchType = true;
	}

	void Instruction::DoBranch( u32 jumptarg )
	{
		m_NextPC = jumptarg;
	}

	void Instruction::DoBranch()
	{
		DoBranch( BranchTarget() );
	}
}
