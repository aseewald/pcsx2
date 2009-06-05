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

//////////////////////////////////////////////////////////////////////////////////////////
//
// Performance Goal: The event system needs to offer a system that is fast for checking
// for a timeout on a scheduled event, but can be otherwise slow for just about everything
// else.  To make event-timeout tests as fast as possible, the event system uses a toward-
// zero countdown system that allows the test code to be a simple SUB+JNZ combination.
//
class BaseEventSystem
{
public:
protected:
};

class IopEventSystem;

enum IopEventType
{
	// Idle state, no events scheduled.  Placed at -1 since it has no actual
	// entry in the Event System's event schedule table.

	IopEvt_Idle = -1,

	IopEvt_Counter0 = 0,
	IopEvt_Counter1,
	IopEvt_Counter2,
	IopEvt_Counter3,
	IopEvt_Counter4,
	IopEvt_Counter5,
	IopEvt_Exception,

	IopEvt_SIF0,
	IopEvt_SIF1,
	IopEvt_SIO,
	IopEvt_SIO2_Dma11,
	IopEvt_SIO2_Dma12,

	IopEvt_Cdvd,
	IopEvt_CdvdRead,
	IopEvt_Cdrom,
	IopEvt_CdromRead,
	IopEvt_SPU2_Dma4,	// Core 0 DMA
	IopEvt_SPU2_Dma7,	// Core 1 DMA
	IopEvt_SPU2,		// SPU command/event processor
	IopEvt_DEV9,
	IopEvt_USB,
	
	IopEvt_BreakForEE,

	IopEvt_Count		// total number of schedulable event types in the Iop
};

class IopEventSystem : public BaseEventSystem
{
public:
	// Performance note: IsEnabled might be more efficient as a separate array of
	// booleans (it would pack with byte alignment).  This is nicer and easier though
	// and I'm not sure it's enough of a diff to matter.
	struct ScheduleInfo
	{
		s32 OrigDelta;		// original delta time of the scheduled event
		s32 Countdown;		// current countdown from OrigDelta to zero (when event fires)
		bool IsEnabled;		// If false the event is disabled (does not count down or fire)
		u8 ModeType;		// user-defined event type (optionally used by some events to define types/modes)

		// Returns the number of cycles passed since the last time this event's update
		// handler fired.
		const s32 GetTimepass() const
		{
			return OrigDelta - Countdown;
		}
	};
	
	// [TODO] : Make an accessor array for this one?
	const ScheduleInfo& GetInfo( IopEventType evt ) const
	{
		return m_Events[evt];
	}

protected:
	struct Pair
	{
		int nearest_evt;
		s32 shortest_delta;
	};

protected:
	// Countdown value for each event known to the IOP scheduler.
	ScheduleInfo m_Events[IopEvt_Count];
	
	// Next scheduled event, indexes m_EventCountdowns table.
	int m_NextEvent;
	
	// Set TRUE while dispatching is in progress; which alters the behavior of scheduling
	// of events.
	bool m_IsDispatching;

public:
	void Reset();

	void UpdateSchedule( s32 timepass );
	void ScheduleEvent( IopEventType evt, s32 delta );
	void RaiseException();
	void Dispatch( IopEventType evt );
	void ExecutePendingEvents();
	
	void SetEventModeType( IopEventType evt, u8 modetype )
	{
		jASSUME( evt < IopEvt_Count );
		m_Events[evt].ModeType = modetype;
	}

	void CancelEvent( IopEventType evt );

protected:
	void GetNearestEvent( Pair& pair ) const;
	IopEventType PrepNextEvent() const;
	void FetchNextEvent( Pair& pair ) const;
};

extern IopEventSystem iopEvtSys;

extern void iopExecutePendingEvents();

static __forceinline void PSX_INT( IopEventType evt, int deltaCycles )
{
	iopEvtSys.ScheduleEvent( evt, deltaCycles );
}
