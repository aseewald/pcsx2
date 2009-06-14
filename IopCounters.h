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

namespace IopCounters
{
	enum EventType
	{
		IopCntEvt_Target,
		IopCntEvt_Overflow,

		// Special event types for when the overflow that's been scheduled
		// is too far into the future for the delta to fit within the context
		// of an s32.
		IopCntEvt_RescheduleTarget,
		IopCntEvt_RescheduleOverflow
	};

	extern void Reset();

	extern u16 ReadCount16( uint cntidx );
	extern u32 ReadCount32( uint cntidx );
	extern u16 ReadTarget16( uint cntidx );
	extern u32 ReadTarget32( uint cntidx );
	extern u32 ReadMode( uint cntidx );

	extern void WriteCount16( uint cntidx, u16 count );
	extern void WriteCount32( uint cntidx, u32 count );
	extern void WriteTarget16( uint cntidx, u16 target );
	extern void WriteTarget32( uint cntidx, u32 target );
	extern void WriteMode( uint cntidx, u32 mode );

	extern void OnEvent( uint cntidx, EventType evttype );
	extern void AdvanceCycles( s32 delta );

	extern void VBlankStart();
	extern void VBlankEnd();
	extern void CheckStartGate0();
	extern void CheckEndGate0();
}
