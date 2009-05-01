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

namespace IopMemory {

//////////////////////////////////////////////////////////////////////////////////////////
//
void __fastcall iopHwWrite8_Page1( u32 addr, u8 val )
{
	// all addresses are assumed to be prefixed with 0x1f801xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f801 );

	u32 masked_addr = addr & 0x0fff;

	switch( masked_addr )
	{		
		case 0x040: sioWrite8( val ); break;

		// for use of serial port ignore for now
		//case 0x50: serial_write8( val ); break;

		case 0x100:
		case 0x104:
		case 0x108:
		case 0x110:
		case 0x114:
		case 0x118:
		case 0x120:
		case 0x124:
		case 0x128:
		case 0x480:
		case 0x484:
		case 0x488:
		case 0x490:
		case 0x494:
		case 0x498:
		case 0x4a0:
		case 0x4a4:
		case 0x4a8:
			DevCon::Notice( "*Hardware Write8 to Counter Register [ignored] [addr=0x%02x]", params addr, psxHu8(addr) );
			psxHu8( addr ) = val;
		break;

		case 0x46e: DEV9write8( addr, val ); break;

		case 0x800: cdrWrite0( val ); break;
		case 0x801: cdrWrite1( val ); break;
		case 0x802: cdrWrite2( val ); break;
		case 0x803: cdrWrite3( val ); break;

		default:
			if( (masked_addr >= 0x600) && (masked_addr < 0x700) )
			{
				USBwrite8( addr, val );
				PSXHW_LOG( "Hardware Write8 to USB, aadr 0x%08x = 0x%02x\n", addr, val );
			}
			else
			{
				psxHu8(addr) = val;
				PSXHW_LOG( "*Unknown Hardware Write8 to addr 0x%08x = 0x%02x\n", addr, val );
			}
		return;
	}

	PSXHW_LOG( "*Hardware Write8 to addr 0x%08x = 0x%02x\n", addr, val );
}

void __fastcall iopHwWrite8_Page3( u32 addr, u8 val )
{
}

void __fastcall iopHwWrite8_Page8( u32 addr, u8 val )
{
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void __fastcall iopHwWrite16_Page1( u32 addr, u16 val )
{
}

void __fastcall iopHwWrite16_Page3( u32 addr, u16 val )
{
}

void __fastcall iopHwWrite16_Page8( u32 addr, u16 val )
{
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void __fastcall iopHwWrite32_Page1( u32 addr, u32 val )
{
}

void __fastcall iopHwWrite32_Page3( u32 addr, u32 val )
{
}

void __fastcall iopHwWrite32_Page8( u32 addr, u32 val )
{
}

}
