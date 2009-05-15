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

static void recDIV_ConstRt( const IntermediateInstruction& info )
{
	// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
	// since the result is known at IL time.
	jASSUME( !info.IsConstRs() );

	if( info.SrcRt.Imm == 0 )
	{
		info.MoveRsTo( eax );
		xMOV( edx, 1 );
		xMOV( ecx, -1 );
		xCMP( eax, 0 );
		xCMOVGE( edx, ecx );

		info.MoveToHiLo( eax, edx );	// yea eax:edx this time (no div to swap regs)
		return;
	}
	else if( info.SrcRt.Imm == -1 )
	{
		// yay someday I'll simplify this with an improved emitter (when having
		// the extra overloads doesn't triple link times).

		if( info.SrcRs.RegDirect.IsEmpty() )
			xCMP( info.SrcRs.MemIndirect, 0x80000000 );
		else
			xCMP( info.SrcRs.RegDirect, 0x80000000 );

		xForwardJNE8 doFullDiv;
		info.MoveToHiLo( 0, 0x80000000 );
		xForwardJump8 skipAll;

		doFullDiv.SetTarget();
		xDIV( eax );
		info.MoveToHiLo( edx, eax );
		skipAll.SetTarget();
	}
}

static void recDIV_ConstRs( const IntermediateInstruction& info )
{
	// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
	// since the result is known at IL time.
	jASSUME( !info.IsConstRt() );

	info.MoveRtTo( eax );
	xCMP( eax, 0 );
	xForwardJZ8 skipOverflowRt;

	info.MoveToHiLo( info.SrcRs.Imm, (info.SrcRs.Imm >= 0) ? -1 : 1 );
	xForwardJump8 skipAll;

	skipOverflowRt.SetTarget();
	if( info.SrcRs.Imm == 0x80000000 )
	{
		// If Rs is 0x80000000 then we need to check Rt for -1.

		xCMP( eax, -1 );
		xForwardJNE8 skipOverflowRtNeg;
		info.MoveToHiLo( 0, 0x80000000 );
		skipOverflowRtNeg.SetTarget();
		xForwardJump8 skipDiv;

		// eh, need to duplicate div code because of jump label scoping rules.
		info.MoveRsTo( ecx );
		xDIV( ecx );
		info.MoveToHiLo( edx, eax );
		skipDiv.SetTarget();
	}
	else
	{
		info.MoveRsTo( ecx );		// DIV doesn't have imm forms, so load it into ecx...
		xDIV( ecx );
		info.MoveToHiLo( edx, eax );
	}
	skipAll.SetTarget();
}

void R3000A::recDIV( const IntermediateInstruction& info )
{
	if( info.IsConstRt() )
	{
		recDIV_ConstRt( info );
	}
	else if( info.IsConstRs() )
	{
		recDIV_ConstRs( info );
	}
	else
	{
		info.MoveRtTo( eax );
		info.MoveRsTo( edx );

		xCMP( eax, 0 );
		xForwardJZ8 skipOverflowRt;

		xMOV( eax, 1 );
		xMOV( ecx, -1 );
		xCMP( edx, 0 );
		xCMOVGE( eax, ecx );
		xForwardJump8 writeBack;	// write eax:edx and be done!

		skipOverflowRt.SetTarget();
		xCMP( eax, -1 );
		xForwardJNE8 skipOverflowRtNeg;
		xCMP( edx, 0x80000000 );
		xForwardJNE8 skipOverflowRtNeg2;
		info.MoveToHiLo( 0, 0x80000000 );
		xForwardJump8 skipAll;
		
		skipOverflowRtNeg.SetTarget();
		skipOverflowRtNeg2.SetTarget();
		
		// All the evil MIPS behavior checks fell through..?
		// Then Perform full-on div!
		xDIV( edx );

		writeBack.SetTarget();
		info.MoveToHiLo( edx, eax );

		skipAll.SetTarget();
	}
}

void R3000A::recDIVU( const IntermediateInstruction& info )
{
}

}
