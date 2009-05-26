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

enum DynExitMap_t
{
	DynEM_Unmapped = -1,
	DynEM_Temp0 = 0,
	DynEM_Temp1,
	DynEM_Temp2,
	DynEM_Temp3,
	
	DynEM_Rs,
	DynEM_Rt,
	DynEM_Hi,
	DynEM_Lo,
	
	// Rd not included since it's never mapped on entry/source.	
};

static const int RegCount_Mappable = 5;
static const int RegCount_Temps = 4;

// ------------------------------------------------------------------------
// Describes register mapping information, on a per-gpr or per-field (Rs Rd Rt) basis.
//
struct DynRegMapInfo_Entry
{
	// Tells the recompiler that the reg must be force-loaded into an x86 register on entry.
	// (recompiler will flush another reg if needed, and load this field in preparation for
	// Emit entry).  The x86 register will be picked by the recompiler.
	// Note: ForceDirect and ForceIndirect are mutually exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceDirect;

	// Tells recompiler the given instruction register field must be forced to Memory
	// (flushed).  This is commonly used as an optimization guide for cases where all x86
	// registers are modified by the instruction prior to the instruction using Rs [LWL/SWL
	// type memory ops, namely].
	// Note: ForceDirect and ForceIndirect are mutually exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceIndirect;

	// Specifies known "valid" mappings on exit from the instruction emitter.  The recompiler
	// will use this to map registers more efficiently and avoid unnecessary register swapping.
	// Notes:
	//   * this is a suggestion only, and the recompiler reserves the right to map destinations
	//     however it sees fit.
	//
	//   * By default the recompiler will assume (prefer) the register used for Rs as matching
	//     Rt/Rd on exit, which is what most instructions do.
	//
	DynExitMap_t ExitMap;

	DynRegMapInfo_Entry() :
		ForceDirect( false ),
		ForceIndirect( false ),
		ExitMap( DynEM_Unmapped )
	{
	}
};

// ------------------------------------------------------------------------
//
class RegMapInfo_Dynamic
{
public:
	// Array contents cover: Forced Direct or Indirect booleans for each GPR field, and
	// output register mappings for the dest fields.
	RegFieldArray<DynRegMapInfo_Entry> GprFields;

	// Set any of these true to allocate a temporary register to that slot.  Allocated
	// registers are always from the pool of eax, edx, ecx, ebx -- so it's assured your
	// allocated register will have low and high forms (al, bl, cl, dl, etc).
	//
	// Note: Setting all four of these true will only work if you are *not* using other
	// forms of register mapping, since you can't map eax (for example) and expect to
	// get four temp regs as well.  The recompiler will assert/exception.
	bool AllocTemp[4];

	// Each entry corresponds to the register in the m_tempRegs array.
	xRegisterArray32<bool> xRegInUse;

public:
	__forceinline DynRegMapInfo_Entry& operator[]( RegField_t idx )
	{
		return GprFields[idx];
	}

	__forceinline const DynRegMapInfo_Entry& operator[]( RegField_t idx ) const
	{
		return GprFields[idx];
	}

	RegMapInfo_Dynamic()
	{
		memzero_obj( AllocTemp );
		
		for( int i=0; i<xRegInUse.Length(); ++i )
			xRegInUse[(xRegister32)i] = false;

	}
};

}
