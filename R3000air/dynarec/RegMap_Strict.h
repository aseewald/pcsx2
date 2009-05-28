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

namespace R3000A {

using namespace x86Emitter;

enum ExitMapType
{
	StrictEM_Rd = 0,
	StrictEM_Rt,
	StrictEM_Rs,
	StrictEM_Hi,
	StrictEM_Lo,

	StrictEM_Invalid,		// specifies the register is clobbered or in an invalid state on exit
	StrictEM_Untouched,		// specifies register is untouched -- previous mapping is retained on exit
};

// ------------------------------------------------------------------------
//
class RegMapInfo_Strict
{
public:
	// Maps a GPR into to the specific x86 register on entry to the instruction's emitter.
	// If a GPR is mapped to Empty here, it will be flushed to memory, and the Instruction
	// recompiler will be provisioned with an indirect register mapping.
	RegFieldArray<xRegister32> EntryMap;

	// Specifies known "valid" mappings on exit from the instruction emitter.  The recompiler
	// will use this to map registers more efficiently and avoid unnecessary register swapping,
	// when possible.
	// Note: this is a suggestion only, and the recompiler reserves the right to map destinations
	// however it sees fit (most often a destination won't be mapped to a register at all, and
	// will instead flush directly to memory).
	xTempRegsArray32<ExitMapType> ExitMap;

	RegMapInfo_Strict()
	{
		// EntryMap will auto-init to Empties, but ExitMap needs some love:
		for( int i=0; i<ExitMap.Length(); ++i )
			ExitMap[(xRegister32)i] = StrictEM_Invalid;
	}

	void EntryMapHiLo( const xRegister32& srchi, const xRegister32& srclo )
	{
		EntryMap.Hi = srchi;
		EntryMap.Lo = srclo;
	}

	void ExitMapHiLo( const xRegister32& srchi, const xRegister32& srclo )
	{
		ExitMap[srchi] = StrictEM_Hi;
		ExitMap[srclo] = StrictEM_Lo;
	}
	
	void ClobbersNothing()
	{
		for( int i=0; i<ExitMap.Length(); ++i )
			ExitMap[(xRegister32)i] = StrictEM_Untouched;
	}
};

}
