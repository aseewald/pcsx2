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
#include "IopCommon.h"
#include "iR3000air.h"

namespace R3000A
{

typedef InstructionRecMess InstAPI;

IMPL_RecPlacebo( MULT );
IMPL_RecPlacebo( MULTU );
IMPL_RecPlacebo( DIVU );


// ------------------------------------------------------------------------
namespace recADDI_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Dynamic& rd( info.RegOpts.UseDynMode() );

		rd.ExitMap.Rt = DynEM_Rs;
		if( info.GetImm() == 0 )
			rd.ExitMap.Rs = DynEM_Untouched;
	}
	
	static void Emit( const IntermediateRepresentation& info )
	{
		if( info.GetImm() != 0 )
			xADD( RegRs, info.GetImm() );
		info.MoveToRt( RegRs );
	}
	
	IMPL_GetInterface()
}

IMPL_RecInstAPI( ADDI );

// Map to ADDI until such time we implement exception handling/checking for ADDI.
void InstAPI::ADDIU()
{
	API.ConstNone	= recADDI_ConstNone::GetInterface;
	API.ConstRt		= recADDI_ConstNone::GetInterface;
	API.ConstRs		= recADDI_ConstNone::GetInterface;
}

// ------------------------------------------------------------------------
namespace recADD_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Dynamic& rd( info.RegOpts.UseDynMode() );

		// Rs and Rt can be swapped freely:
		info.RegOpts.CommutativeSources	= true;
		
		//if( _Rs_ == _Rd_ )
	}

	static void Emit( const IntermediateRepresentation& info )
	{
		/*if( _Rs_ == _Rd_ )
		{
			xADD( DestRegRd, RegRt );
		}*/
		xADD( RegRs, RegRt );
		info.MoveToRd( RegRs );
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( ADD );

// Map to ADD until such time we implement exception handling/checking for ADD.
void InstAPI::ADDU()
{
	API.ConstNone	= recADD_ConstNone::GetInterface;
	API.ConstRt		= recADD_ConstNone::GetInterface;
	API.ConstRs		= recADD_ConstNone::GetInterface;
}

// ------------------------------------------------------------------------
namespace recSUB_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Dynamic& rd( info.RegOpts.UseDynMode() );
	}

	static void Emit( const IntermediateRepresentation& info )
	{
		xSUB( RegRt, RegRs );
		info.MoveToRd( RegRs );
	}

	IMPL_GetInterface()
}

IMPL_RecInstAPI( SUB );

// Map to ADD until such time we implement exception handling/checking for ADD.
void InstAPI::SUBU()
{
	API.ConstNone	= recSUB_ConstNone::GetInterface;
	API.ConstRt		= recSUB_ConstNone::GetInterface;
	API.ConstRs		= recSUB_ConstNone::GetInterface;
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// Div on MIPS:
// * If Rt is zero, then the result is undefined by MIPS standard.  EE/IOP results are pretty
//   consistent, however:  Hi == Rs, Lo == (Rs >= 0) ? -1 : 1
//
// * MIPS has special defined behavior on signed DIVs, to cope with it's lack of overflow
//   exception handling.  If Rs == -0x80000000 (which is the same as unsigned 0x80000000 on
//   our beloved twos-compliment math), and Rt == -1 (0xffffffff as unsigned), the result
//   is 0x80000000 and remainder of zero.


//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recDIV_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );
		
		rs.EntryMap.Rs = ecx;
		rs.EntryMap.Rt = eax;

		rs.ExitMap.eax = StrictEM_Lo;
		rs.ExitMap.ecx = StrictEM_Untouched;
		rs.ExitMap.ebx = StrictEM_Untouched;
	}

	static void Emit( const IntermediateRepresentation& info )
	{
		// Make sure recompiler did it's job:
		jASSUME( info.Src[RF_Rs].GetReg() == ecx );
		jASSUME( info.Src[RF_Rt].GetReg() == eax );

		xCMP( eax, 0 );
		xForwardJE8 skipOverflowRt;

		xMOV( eax, 1 );
		xMOV( edx, -1 );
		xCMP( ecx, 0 );
		xCMOVGE( eax, edx );
		xForwardJump8 writeBack;	// write eax:edx and be done!

		skipOverflowRt.SetTarget();
		xCMP( eax, -1 );
		xForwardJNE8 skipOverflowRtNeg;
		xMOV( eax, 0x80000000 );
		xCMP( ecx, eax );
		xForwardJNE8 skipOverflowRtNeg2;
		info.MoveToHiLo( eax );	// eax == 0x80000000
		xForwardJump8 skipAll;

		// All the evil MIPS behavior checks fell through..?
		// Then Perform full-on div!
		skipOverflowRtNeg.SetTarget();
		skipOverflowRtNeg2.SetTarget();
		xDIV( ecx );

		// writeback for Rt-overflow case.
		writeBack.SetTarget();
		info.MoveToHiLo( edx, eax );

		skipAll.SetTarget();
	}
	
	IMPL_GetInterface()
};

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recDIV_ConstRt
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		if( info.GetConstRt() == 0 )
		{
			rs.EntryMap.Rs = edx;
			rs.ExitMap.edx = StrictEM_Untouched;
			rs.ExitMap.ebx = StrictEM_Untouched;
			rs.ExitMapHiLo( edx, eax );
		}
		else if( info.GetConstRt() == -1 )
		{
			rs.ExitMap.ecx = StrictEM_Untouched;
			rs.ExitMap.ebx = StrictEM_Untouched;
			rs.ExitMapHiLo( edx, eax );
		}
		else
		{
			recDIV_ConstNone::RegMapInfo( info );
		}
	}

	static void Emit( const IntermediateRepresentation& info )
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IL time.
		jASSUME( !info.IsConstRs() );

		if( info.GetConstRt() == 0 )
		{
			// Rs is loaded into edx.
			
			xMOV( eax, 1 );
			xMOV( ecx, -1 );
			xCMP( edx, 0 );
			xCMOVGE( eax, ecx );
			// edx == Hi, eax == Lo
			//info.MoveToHiLo( edx, eax );
		}
		else if( info.GetConstRt() == -1 )
		{
			xMOV( eax, 0x80000000 );
			xCMP( info.Src[RF_Rs], eax );

			xForwardJNE8 doFullDiv;
			info.MoveToHiLo( eax );
			xForwardJump8 skipAll;

			doFullDiv.SetTarget();
			xMOV( eax, info.GetConstRs() );
			xDIV( info.Src[RF_Rs] );
			//info.MoveToHiLo( edx, eax );

			skipAll.SetTarget();
		}
		else
		{
			recDIV_ConstNone::Emit( info );
		}
	}
	
	IMPL_GetInterface()
};


//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recDIV_ConstRs
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		rs.EntryMap.Rt = eax;
		rs.EntryMap.Rs = ecx;		// DIV lacks Imm forms, so force-load const Rs into ecx

		// When Const Rs == 0x80000000, the mappings of Hi/Lo are indeterminate.
		if( info.GetConstRs() != 0x80000000 )
		{
			rs.ExitMapHiLo( edx, eax );
		}

		rs.ExitMap.ecx = StrictEM_Untouched;
		rs.ExitMap.ebx = StrictEM_Untouched;
	}
	
	static void Emit( const IntermediateRepresentation& info )
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IL time.
		jASSUME( !info.IsConstRt() );

		// Make sure recompiler did it's job:
		jASSUME( info.Src[RF_Rt].GetReg() == eax );
		jASSUME( info.Src[RF_Rs].GetReg() == ecx );

		xCMP( eax, 0 );
		xForwardJZ8 skipOverflowRt;

		xMOV( eax, (info.GetConstRs() >= 0) ? -1 : 1 );
		info.MoveToHiLo( ecx, eax );
		xForwardJump8 skipAll;

		skipOverflowRt.SetTarget();
		if( info.GetConstRs() == 0x80000000 )
		{
			// If Rs is 0x80000000 then we need to check Rt for -1.

			xCMP( eax, -1 );
			xForwardJNE8 skipOverflowRtNeg;

			info.MoveToHiLo( ecx );	// ecx is Rs, which is const 0x80000000
			xForwardJump8 skipDiv;

			// eh, need to duplicate div code because of jump label scoping rules.
			skipOverflowRtNeg.SetTarget();
			xDIV( ecx );
			info.MoveToHiLo( edx, eax );

			skipDiv.SetTarget();
		}
		else
		{
			xDIV( ecx );
			info.MoveToHiLo( edx, eax );
		}
		skipAll.SetTarget();
	}
	
	IMPL_GetInterface()
};

typedef InstructionRecMess InstAPI;

void InstAPI::DIV()
{
	API.ConstNone	= recDIV_ConstNone::GetInterface;
	API.ConstRt		= recDIV_ConstRt::GetInterface;
	API.ConstRs		= recDIV_ConstRs::GetInterface;
}

}
