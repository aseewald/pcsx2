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

namespace R3000A
{

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
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	static void Emit( const IntermediateInstruction& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( ORI );

// ------------------------------------------------------------------------
namespace recXORI_ConstNone
{
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	static void Emit( const IntermediateInstruction& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( XORI );

// ------------------------------------------------------------------------
namespace recAND_ConstNone
{
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	static void Emit( const IntermediateInstruction& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( AND );

// ------------------------------------------------------------------------
namespace recOR_ConstNone
{
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	static void Emit( const IntermediateInstruction& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( OR );

// ------------------------------------------------------------------------
namespace recXOR_ConstNone
{
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	static void Emit( const IntermediateInstruction& info )
	{
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( XOR );

// ------------------------------------------------------------------------
namespace recSLTI_ConstNone
{
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
	}

	template< bool IsSigned >
	static void Emit( const IntermediateInstruction& info )
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
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
		opts.xModifiesReg.None();
		opts.xModifiesReg( eax );

		opts.ForceDirectRs = true;
	}

	template< bool IsSigned >
	static void Emit( const IntermediateInstruction& info )
	{
		if( info.Dest[RF_Rd].IsDirect() )
		{
			xCMP( info.Src[RF_Rs], info.Src[RF_Rt] );
			if( IsSigned )
				xSETL( al );
			else
				xSETB( al );

			xMOVZX( info.Dest[RF_Rd].GetReg(), al );
		}
		else
		{
			xXOR( eax, eax );
			xCMP( info.Src[RF_Rs], info.Src[RF_Rt] );
			if( IsSigned )
				xSETL( al );
			else
				xSETB( al );

			xMOV( info.Dest[RF_Rd], eax );
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
	static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts )
	{
		opts.xModifiesReg.None();
		opts.CommutativeSources = true;
		opts.ForceDirectRs = true;
	}

	static void Emit( const IntermediateInstruction& info )
	{
		xOR( info.Src[RF_Rs], info.Src[RF_Rt] );
		xNOT( info.Src[RF_Rs] );
		xMOV( info.Dest[RF_Rd], info.Src[RF_Rs] );
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( NOR );

}

