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

namespace R3000A
{

IMPL_RecPlacebo( NOP );

IMPL_RecPlacebo( Exception );
IMPL_RecPlacebo( RFE );

IMPL_RecPlacebo( CTC0 );
IMPL_RecPlacebo( CFC0 );
IMPL_RecPlacebo( MFC0 );

IMPL_RecPlacebo( Unknown );


// ------------------------------------------------------------------------
class recMTC0 : public x86IntRep
{
public:
	recMTC0( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );
		dyn.ForceDirect.Rt	= true;
		dyn.ExitMap.Rt		= DynEM_Untouched;
	}

	void Emit()
	{
		xMOV( &iopRegs.CP0.r[Inst.MipsInst._Rd_], RegRt.GetReg() );
	}
};

}