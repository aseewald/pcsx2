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

IMPL_RecPlacebo( ShiftLeft );
IMPL_RecPlacebo( ShiftRight );
IMPL_RecPlacebo( ShiftLeftImm );
IMPL_RecPlacebo( ShiftRightImm );

IMPL_RecPlacebo( AndImm );
IMPL_RecPlacebo( XorImm );

// ------------------------------------------------------------------------
class recOrImm : public x86IntRep
{
public:
	recOrImm( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
	}

	void Emit() const
	{
	}
};

IMPL_RecPlacebo( Xor );
IMPL_RecPlacebo( And );
IMPL_RecPlacebo( Or );

// ------------------------------------------------------------------------
class recSetImm : public x86IntRep
{
public:
	recSetImm( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
	}

	void Emit() const
	{
	}
};

// ------------------------------------------------------------------------
class recSet : public x86IntRep
{
public:
	recSet( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );
		
		RegOpts.CommutativeSources = true;
		dyn.AllocTemp[0]	= true;
		dyn.ExitMap.Rs	= DynEM_Untouched;
		dyn.ExitMap.Rt	= DynEM_Untouched;
		dyn.ExitMap.Rd	= DynEM_Temp0;
	}

	void Emit() const
	{
		xCMP( RegRt, RegRs );
		xSET( BccToJcc( Inst.CompareType, IsSwappedSources ), tmp0reg8 );

		if( DestRegRd.IsDirect() )
			xMOVZX( DestRegRd.GetReg(), tmp0reg8 );
		else
		{
			xMOVZX( tmp0reg, tmp0reg8 );
			xMOV( DestRegRd, tmp0reg );
		}
	}
};

// ------------------------------------------------------------------------
class recNor : public x86IntRep
{
public:
	recNor( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );
		RegOpts.CommutativeSources = true;
		dyn.ExitMap.Rt = DynEM_Untouched;
		dyn.ExitMap.Rd = DynEM_Rs;
	}

	void Emit() const
	{
		xOR( RegRs, RegRt );
		xNOT( RegRs );
		xMOV( DestRegRd, RegRs );
	}
};

}

