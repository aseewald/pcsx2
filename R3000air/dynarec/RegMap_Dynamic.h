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

// ------------------------------------------------------------------------
// Implementation note: I've aligned the values of the enumeration to be in synch with
// other Rs / Rt / Hi / Lo enumerations you'll spot around these parts.
//
//  * Rd not included since it's never mapped on entry/source.	
//
enum DynExitMap_t
{
	DynEM_Rt = 1,
	DynEM_Rs,
	DynEM_Hi,
	DynEM_Lo,

	DynEM_Invalid,
	DynEM_Untouched,

	DynEM_Temp0 = 8,
	DynEM_Temp1,
	DynEM_Temp2,
	DynEM_Temp3,
};

// ------------------------------------------------------------------------
//
class RegMapInfo_Dynamic
{
public:
	// Set to true to force the specified field into a register.  Note: by default Rs
	// is auto-forced into a register!  Set it to false here to disable that behavior.
	RegFieldArray<bool> ForceDirect;
	
	// Array contents cover: Forced Direct or Indirect booleans for each GPR field, and
	// output register mappings for the dest fields.
	RegFieldArray<DynExitMap_t> ExitMap;

	// Set any of these true to allocate a temporary register to that slot.  Allocated
	// registers are always from the pool of eax, edx, ecx, ebx -- so it's assured your
	// allocated register will have low and high forms (al, bl, cl, dl, etc).
	//
	// Note: Setting all four of these true will only work if you are *not* using other
	// forms of register mapping, since you can't map eax (for example) and expect to
	// get four temp regs as well.  The recompiler will assert/exception.
	int AllocTemp[4];

	// Intended for internal use by the RecMapping loginc only.
	// Each entry corresponds to the register in the m_tempRegs array.
	xRegisterArray32<bool> xRegInUse;

public:

	RegMapInfo_Dynamic()
	{
		memzero_obj( ForceDirect );
		ForceDirect.Rs = true;
		
		for( int i=0; i<ExitMap.Length(); ++i )
			ExitMap[(DynExitMap_t)i] = DynEM_Untouched;
		
		memzero_obj( AllocTemp );
		
		xRegInUse[eax] = false;
		xRegInUse[ecx] = false;
		xRegInUse[edx] = false;
		xRegInUse[ebx] = false;

		// Reserved / Unusable
		xRegInUse[esi] = true;
		xRegInUse[edi] = true;
		xRegInUse[ebp] = true;
		xRegInUse[esp] = true;
	}
};

}
