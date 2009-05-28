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

#include "System.h"
#include "IopCommon.h"
#include "iR3000air.h"

namespace R3000A {

using namespace x86Emitter;

typedef InstructionRecMess InstAPI;


IMPL_RecPlacebo( SLL );
IMPL_RecPlacebo( SRA );
IMPL_RecPlacebo( SRL );
IMPL_RecPlacebo( SLLV );
IMPL_RecPlacebo( SRAV );
IMPL_RecPlacebo( SRLV );

IMPL_RecPlacebo( ANDI );

// ------------------------------------------------------------------------
namespace recORI_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( ORI );

// ------------------------------------------------------------------------
namespace recXORI_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( XORI );

// ------------------------------------------------------------------------
namespace recAND_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( AND );

// ------------------------------------------------------------------------
namespace recOR_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( OR );

// ------------------------------------------------------------------------
namespace recXOR_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( XOR );

// ------------------------------------------------------------------------
namespace recSLTI_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
	}

	template< bool IsSigned >
	static void Emit( const IntermediateRepresentation& info )
	{
	}

	IMPL_GetInterfaceTee( bool )
}

void InstAPI::SLTI()
{
	API.ConstNone	= recSLTI_ConstNone::GetInterface<true>;
	API.ConstRt		= recSLTI_ConstNone::GetInterface<true>;
	API.ConstRs		= recSLTI_ConstNone::GetInterface<true>;
}

void InstAPI::SLTIU()
{
	API.ConstNone	= recSLTI_ConstNone::GetInterface<false>;
	API.ConstRt		= recSLTI_ConstNone::GetInterface<false>;
	API.ConstRs		= recSLTI_ConstNone::GetInterface<false>;
}

// ------------------------------------------------------------------------
namespace recSLT_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Dynamic& rd( info.RegOpts.UseDynMode() );
		
		info.RegOpts.CommutativeSources = true;
		rd.AllocTemp[0]	= true;
		rd.ExitMap.Rs	= DynEM_Untouched;
		rd.ExitMap.Rt	= DynEM_Untouched;
		rd.ExitMap.Rd	= DynEM_Temp0;
	}

	static JccComparisonType CompareRsRt( bool isSwapped, bool isSigned )
	{
		if( isSwapped )
			return isSigned ? Jcc_GreaterOrEqual : Jcc_AboveOrEqual;
		else
			return isSigned ? Jcc_Less : Jcc_Below;
	}

	template< bool IsSigned >
	static void Emit( const IntermediateRepresentation& info )
	{
		xCMP( RegRt, RegRs );
		xSET( CompareRsRt( info.IsSwappedSources, IsSigned ), tmp0reg8 );

		if( DestRegRd.IsDirect() )
			xMOVZX( DestRegRd.GetReg(), tmp0reg8 );
		else
		{
			xMOVZX( tmp0reg, tmp0reg8 );
			xMOV( DestRegRd, tmp0reg );
		}
	}

	IMPL_GetInterfaceTee( bool )
}

void InstAPI::SLT()
{
	API.ConstNone	= recSLT_ConstNone::GetInterface<true>;
	API.ConstRt		= recSLT_ConstNone::GetInterface<true>;
	API.ConstRs		= recSLT_ConstNone::GetInterface<true>;
}

void InstAPI::SLTU()
{
	API.ConstNone	= recSLT_ConstNone::GetInterface<false>;
	API.ConstRt		= recSLT_ConstNone::GetInterface<false>;
	API.ConstRs		= recSLT_ConstNone::GetInterface<false>;
}


// ------------------------------------------------------------------------
namespace recNOR_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Dynamic& rd( info.RegOpts.UseDynMode() );
		info.RegOpts.CommutativeSources = true;
		rd.ExitMap.Rt = DynEM_Untouched;
		rd.ExitMap.Rd = DynEM_Rs;
	}

	static void Emit( const IntermediateRepresentation& info )
	{
		xOR( RegRs, RegRt );
		xNOT( RegRs );
		xMOV( DestRegRd, RegRs );
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( NOR );

}

