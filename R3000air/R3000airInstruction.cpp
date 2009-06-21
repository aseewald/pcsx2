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
#include "R3000a.h"
#include "R3000airInstConstOpt.h"

// ------------------------------------------------------------------------
// DynarecAssert: Generates an exception if the given condition is not true.  Exception's
// description contains "IOPrec assumption failed on instruction 'INST': [usr msg]".
//
// inReleaseMode - Exception is generated in Devel and Debug builds only by default.
//   Pass 'true' as the inReleaseMode parameter to enable exception checking in Release
//   builds as well (ie, all builds).
//
void R3000A::InstructionConstOpt::DynarecAssert( bool condition, const char* msg, bool inReleaseMode ) const
{
	// [TODO]  Add a new exception type that allows specialized handling of dynarec-level
	// exceptions, so that the exception handler can save the state of the emulation to
	// a special savestate.  Such a savestate *should* be fully intact since dynarec errors
	// occur during codegen, and prior to executing bad code.  Thus, once such bugs are
	// fixed, the emergency savestate can be resumed successfully. :)
	//
	// [TODO]  Add a recompiler state/info dump to this, so that we can log PC, surrounding
	// code, and other fun stuff!

	if( (inReleaseMode || IsDevBuild) && !condition )
	{
		throw Exception::AssertionFailure( fmt_string(
			"IOPrec assertion failed on instruction '%s': %s", GetName(), msg
		) );
	}
}

bool R3000A::InstructionConstOpt::IsConstField( RegField_t field ) const
{
	switch( field )
	{
		case RF_Rd: return false;
		case RF_Rt: return IsConstRt();
		case RF_Rs: return IsConstRs();

		case RF_Hi: return m_IsConst.Hi;
		case RF_Lo: return m_IsConst.Lo;

		case RF_Link: return m_IsConst.Link;

		jNO_DEFAULT
	}
}
