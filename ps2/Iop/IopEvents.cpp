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

using namespace R3000A;

static const char *const tbl_EventNames[] =
{
	"Counter0", "Counter1", "Counter2",
	"Counter3", "Counter4", "Counter5",
	
	"Exception",

	"SIF0", "SIF1", "SIO", "SIO2_Dma11", "SIO2_Dma12",

	"CdvdCommand",  "CdvdRead",
	"CdromCommand", "CdromRead",
	
	"SPU2_Dma4", "Sup2_Dma7", "SPU2cmd",

	"DEV9", "USB",

	"BreakForEE"
};

// ------------------------------------------------------------------------
//
void IopEventSystem::Reset()
{
	memzero_obj( m_Events );
	m_NextEvent = IopEvt_Idle;
	m_IsDispatching = false;
}

// ------------------------------------------------------------------------
//
__forceinline void IopEventSystem::GetNearestEvent( Pair& pair ) const
{
	pair.nearest_evt = -1;
	pair.shortest_delta = 0x40000;

	for( int i=0; i<IopEvt_Count; ++i )
	{
		if( !m_Events[i].IsEnabled ) continue;

		if( m_Events[i].Countdown < pair.shortest_delta )
		{
			pair.shortest_delta	= m_Events[i].Countdown;
			pair.nearest_evt	= i;
		}
	}
}

// ------------------------------------------------------------------------
// updates the delta times for all events, so that they stay in synch with the sliding forward
// of the iopRegs._cycle variable.  This function should only be called during
// ExecutePendingEvents()
//
__forceinline void IopEventSystem::UpdateSchedule( s32 timepass, Pair& pair )
{
	// performance note: This function is currently the "slow part" of the event system.
	// The best way to optimize it is to sub-divide the event list into three "Buckets"
	// similar to the concept of a hash table:
	//  * Counters
	//  * SIF/SIO/CDVD
	//  * Everything else (including exceptions, which are typically infrequent on the IOP).
	//
	// Furthermore, the BreakForEE event should be a top-level event since it typically occurs
	// more often than any other event.  (and, in fact, simply optimizing BreakForEE out into
	// it's own separate event timer would probably be enough to cut the overhead of UpdateSchedule
	// to 50% it's current level).
	//
	// Alternative: The system could also be set up as an ordered list, where each event in the
	// event list has a "nextEvent" pointer that points to the next event in the schedule.  However
	// all this does is defer the performance cost of executing an event from the dispatcher to
	// the rescheduler (which must sort and insert into the list).  My gut says buckets will be
	// better since most events reschedule themselves anyway.

	pair.nearest_evt = -1;
	pair.shortest_delta = 0x40000;

	for( int i=0; i<IopEvt_Count; ++i )
	{
		if( !m_Events[i].IsEnabled ) continue;
		m_Events[i].Countdown -= timepass;

		if( m_Events[i].Countdown < pair.shortest_delta )
		{
			pair.shortest_delta	= m_Events[i].Countdown;
			pair.nearest_evt	= i;
		}
	}
}

__forceinline void IopEventSystem::FetchNextEvent( Pair& pair ) const
{
	GetNearestEvent( pair );
}

// ------------------------------------------------------------------------
//
__releaseinline void IopEventSystem::Dispatch( IopEventType evt )
{
	if( evt == IopEvt_Idle ) return;

	// Optimization Note:  The use of a switch statement here is *intentional* as it
	// typically yields superior performance to function pointer lookup tables, and is
	// generally easier to maintain and debug as an added bonus. :)
	//
	// Reason: All functions are marked as __forceinline, allowing the compiler to
	// use its choice of a jmp LUT (faster than a call LUT), or a binary partition
	// branching algo.  In this case the compiler will typically use a LUT since the
	// range of cases are contiguous from 0 to ~18 or so.  Binary partitions are used
	// for non-contiguous switches, such as the IopHwMemory handlers.
	
	m_Events[evt].IsEnabled = false;

	//IOPEVT_LOG( "IopEvent Dispatch @ %d(%s)", evt, m_tbl_EventNames[evt] );

	switch( evt )
	{
		case IopEvt_Counter0:
		case IopEvt_Counter1:
		case IopEvt_Counter2:
		case IopEvt_Counter3:
		case IopEvt_Counter4:
		case IopEvt_Counter5:
			IopCounters::Update( evt );
		break;
		
		case IopEvt_Exception:
		{
			//IOPEVT_LOG("Interrupt: %x  %x\n", psxHu32(0x1070), psxHu32(0x1074));

			// Note: Re-test conditions here under the assumption that something else might have
			// cleared the condition masks between the time the exception was raised and the time
			// it's being handled here.

			if( psxHu32(0x1078) == 0 ) return;
			if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return;

			if ((iopRegs.CP0.n.Status & 0xFE01) >= 0x401)
			{
				iopException( 0, iopRegs.IsDelaySlot );
				
				iopRegs.pc = iopRegs.VectorPC;
				iopRegs.VectorPC += 4;
				iopRegs.IsDelaySlot = false;
				//iopBranchAction = true;
			}
		}
		break;

		// ---------------------------------------------------------------
		case IopEvt_SIF0:
			sif0Interrupt();
		break;

		case IopEvt_SIF1:
			sif1Interrupt();
		break;

		case IopEvt_SIO:
			sioInterrupt();
		break;

		case IopEvt_SIO2_Dma11:
			psxDMA11Interrupt();
		break;

		case IopEvt_SIO2_Dma12:
			psxDMA12Interrupt();
		break;

		case IopEvt_Cdvd:
			cdvdActionInterrupt();
		break;

		case IopEvt_CdvdRead:
			cdvdReadInterrupt();
		break;

		case IopEvt_Cdrom:
			cdrInterrupt();
		break;

		case IopEvt_CdromRead:
			cdrReadInterrupt();
		break;

		case IopEvt_SPU2_Dma4:	// Core 0 DMA
			// fixme: currently invoked by SPU2 plugins (evil!)
		break;

		case IopEvt_SPU2_Dma7:	// Core 1 DMA
			// fixme: currently invoked by SPU2 plugins (evil!)
		break;

		case IopEvt_SPU2:	// Intermittent polling event
			if(SPU2async)
				SPU2async( GetInfo( IopEvt_SPU2 ).GetTimepass() );
			ScheduleEvent( IopEvt_SPU2, 768*8 );
		break;

		case IopEvt_DEV9:
			dev9Interrupt();
		break;

		case IopEvt_USB:
			usbInterrupt();
		break;
		
		case IopEvt_BreakForEE:
			iopRegs.IsExecuting = false;
		break;

		jNO_DEFAULT
	}
}

// ------------------------------------------------------------------------
// Performs an immediate execution of all pending events.  Because the recompilers in par-
// ticular can be quite latent in their updating and dispatching of events, it is necessary
// for this routine to perform a thorough and orderly execution of latent events, to ensure
// proper event order.
//
void IopEventSystem::ExecutePendingEvents()
{
	m_IsDispatching = true;

	DevAssume( iopRegs.evtCycleCountdown <= 0, "IopEvtSched Logic Error: ExecutePendingEvents called, but EventCycleCountdown didn't reach 0 yet." );

	do
	{
		//Console::WriteLn( "Dispatching event %d @ cycle=0x%x", params m_NextEvent, iopRegs._cycle );
		s32 oldcdown = iopRegs.evtCycleCountdown;
		iopRegs.evtCycleCountdown	 = 0;
		Dispatch( (IopEventType) m_NextEvent );

		iopRegs._cycle += iopRegs.evtCycleDuration;
		Pair pair;
		UpdateSchedule( iopRegs.evtCycleDuration, pair );
		m_NextEvent					 = pair.nearest_evt;
		iopRegs.evtCycleDuration	 = pair.shortest_delta;
		iopRegs.evtCycleCountdown	 = oldcdown + pair.shortest_delta;

		if( iopRegs.evtCycleCountdown > 0 ) break;
	} while( true );

	m_IsDispatching = false;
}

// Entry point for the IOP recompiler.
void iopExecutePendingEvents()
{
	iopEvtSys.ExecutePendingEvents();
}

#include "R5900.h"


void IopEventSystem::CancelEvent( IopEventType evt )
{
	jASSUME( evt < IopEvt_Count );
	m_Events[evt].IsEnabled = false;
	if( !m_IsDispatching && (evt == m_NextEvent) )
	{
		//Console::Notice( "IOP Canceling Event %d", params evt );

		// pending cycles in the event queue:
		int iopPending = iopRegs.evtCycleDuration - iopRegs.evtCycleCountdown;

		Pair pair;
		GetNearestEvent( pair );

		m_NextEvent					 = pair.nearest_evt;
		iopRegs.evtCycleDuration	 = pair.shortest_delta;
		iopRegs.evtCycleCountdown	 = pair.shortest_delta - iopPending;
	}
}

// ------------------------------------------------------------------------
// schedules the requested event for xx cycles into the future, starting from
// the current iopRegs.cycle.
//
void IopEventSystem::ScheduleEvent( IopEventType evt, s32 delta )
{
	jASSUME( evt < IopEvt_Count );
	//DevAssume( delta > 0, "Iop ScheduleEvent Logic Error: delta must be non-zero." );

	// iopPass is our base/reference point for scheduling stuff:
	int iopPending = iopRegs.evtCycleDuration - iopRegs.evtCycleCountdown;

	m_Events[evt].OrigDelta = delta;
	m_Events[evt].Countdown = delta + iopPending;
	m_Events[evt].IsEnabled = true;
	
	if( !m_IsDispatching && ((evt == m_NextEvent) || (delta < iopRegs.evtCycleCountdown)) )
	{
		// event happens sooner than the currently-scheduled one.  Replace it with ours.
		
		m_NextEvent					= evt;
		iopRegs.evtCycleCountdown	= delta;
		iopRegs.evtCycleDuration	= iopRegs.evtCycleCountdown + iopPending;

		// Tell the EE to do an event test if the EE's currently the code-runner:
		// Important: timeouts over 0x10000000 will cause overflows on the EE's current
		// event system, and anything over 0x1000 really doesn't matter anyway, since the EE
		// forces event tests periodically.

		if( !iopRegs.IsExecuting && (iopRegs.evtCycleCountdown < 0x10000) )
			cpuSetNextBranchDelta( iopRegs.evtCycleCountdown * 8 );
	}
}

// ------------------------------------------------------------------------
//
void IopEventSystem::RaiseException()
{
	if( !eeEventTestIsActive )
	{
		// An iop exception has occurred while the EE is running code.
		// Inform the EE to branch so the IOP can handle it promptly:

		iopBranchAction = true;
	}
	
	if( !GetInfo( IopEvt_Exception ).IsEnabled )
		ScheduleEvent( IopEvt_Exception, 0 );		// must be zero!
}

