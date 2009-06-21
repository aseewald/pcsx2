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
#include "R5900.h"

namespace R3000A {

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

__releaseinline void Registers::IdleEventHandler()
{
	// Note: the idle event handler should only be invoked at times when the full list of
	// active/pending events is empty:
	DevAssert( m_ActiveEvents == NULL, "IopEvtSched Logic Error: Idle event handler called with a non-empty Active list." );
	m_ActiveEvents = &m_Events[IopEvt_Idle];
}

static void _evthandler_Idle()
{
	iopRegs.IdleEventHandler();
}

static void _evthandler_BreakForEE()
{
	iopRegs.IsExecuting = false;
}

static void _evthandler_SPU2async()
{
	PSXDMA_LOG( "SPU2 Async!" );
	if(SPU2async)
		SPU2async( iopRegs.GetEventInfo( IopEvt_SPU2 ).OrigDelta );
	iopRegs.RescheduleEvent( IopEvt_SPU2, 768*8 );
}

static void _evthandler_Exception()
{
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

// ------------------------------------------------------------------------
//
void Registers::ResetEvents()
{
	memzero_obj( m_Events );
	m_ActiveEvents = NULL;

	m_Events[IopEvt_Exception].Execute	= _evthandler_Exception;
	m_Events[IopEvt_SIF0].Execute		= sif0Interrupt;
	m_Events[IopEvt_SIF1].Execute		= sif1Interrupt;
	m_Events[IopEvt_SIO].Execute		= sioInterrupt;
	m_Events[IopEvt_SIO2_Dma11].Execute	= psxDMA11Interrupt;
	m_Events[IopEvt_SIO2_Dma12].Execute	= psxDMA12Interrupt;
	m_Events[IopEvt_Cdvd].Execute		= cdvdActionInterrupt;
	m_Events[IopEvt_CdvdRead].Execute	= cdvdReadInterrupt;
	m_Events[IopEvt_Cdrom].Execute		= cdrInterrupt;
	m_Events[IopEvt_CdromRead].Execute	= cdrReadInterrupt;

	// fixme: currently invoked by SPU2 plugins (evil!)
	m_Events[IopEvt_SPU2_Dma4].Execute	= NULL; // Core 0 DMA
	m_Events[IopEvt_SPU2_Dma7].Execute	= NULL; // Core 1 DMA
	m_Events[IopEvt_SPU2].Execute		= _evthandler_SPU2async;      // Intermittent polling event

	m_Events[IopEvt_DEV9].Execute		= dev9Interrupt;
	m_Events[IopEvt_USB].Execute		= usbInterrupt;
	m_Events[IopEvt_BreakForEE].Execute	= _evthandler_BreakForEE;

	m_Events[IopEvt_Idle].Execute		= _evthandler_Idle;
	m_Events[IopEvt_Idle].OrigDelta		= 0x4000;
	m_Events[IopEvt_Idle].RelativeDelta	= 0x4000;
	
	m_ActiveEvents = &m_Events[IopEvt_Idle];
}

// ------------------------------------------------------------------------
// Performs an immediate execution of all pending events.  Because the recompilers in par-
// ticular can be quite latent in their updating and dispatching of events, it is necessary
// for this routine to perform a thorough and orderly execution of latent events, to ensure
// proper event order.
//
__releaseinline void Registers::ExecutePendingEvents()
{
	DevAssert( evtCycleCountdown <= 0, "IopEvtSched Logic Error: ExecutePendingEvents called, but EventCycleCountdown didn't reach 0 yet." );

	while( true )
	{
		IopCounters::AdvanceCycles( evtCycleDuration );

		s32 oldcdown = evtCycleCountdown;

		evtCycleCountdown	 = 0;		// this sets the iop's "clock" to the exact cycle this event had been scheduled for.
		m_cycle				+= evtCycleDuration;
		evtCycleDuration	 = 0;

		CpuEventType* exeEvt = m_ActiveEvents;
		m_ActiveEvents = exeEvt->next;
		exeEvt->next = NULL;
		exeEvt->Execute();

		evtCycleDuration	 = m_ActiveEvents->RelativeDelta;
		evtCycleCountdown	 = oldcdown + m_ActiveEvents->RelativeDelta;
		
		if( evtCycleCountdown > 0 ) break;
	}

	// Periodic culling of DivStallCycles, prevents it from overflowing in the unlikely event that
	// running code doesn't invoke a DIV or MUL in 2 billion cycles. ;)
	if( DivUnitCycles < 0 )
		DivUnitCycles = 0;
}

// ------------------------------------------------------------------------
// Note: CancelEvent cannot cancel the Idle event (which is always pending some cycles past
// the final event on the Active stack).
//
void Registers::CancelEvent( CpuEventType& thisevt )
{
	if( thisevt.next == NULL ) return;		// not even scheduled, yo.
	thisevt.next->RelativeDelta += thisevt.RelativeDelta;

	// Your Basic List Removal:

	if( m_ActiveEvents == &thisevt )
	{
		m_ActiveEvents = thisevt.next;

		// Node is being removed from the head of the list, so reschedule the
		// IOP's master event scheduler:
		int iopPending = GetPendingCycles();
		evtCycleDuration	= m_ActiveEvents->RelativeDelta;
		evtCycleCountdown	= m_ActiveEvents->RelativeDelta-iopPending;
	}
	else
	{
		CpuEventType* curEvt = m_ActiveEvents;
		while( true )
		{
			if( curEvt->next == &thisevt )
			{
				curEvt->next = thisevt.next;
				break;
			}
			curEvt = curEvt->next;
			DevAssert( curEvt != NULL, "IopEvtSched Logic Error: CancelEvent request cannot find the requested event on the Active list" );
		}
	}

	thisevt.next = NULL;
	m_Events[IopEvt_Idle].RelativeDelta = 0x4000;
}

void Registers::CancelEvent( IopEventType evt )
{
	jASSUME( evt < IopEvt_CountNonIdle );
	CancelEvent( m_Events[evt] );
}

// ------------------------------------------------------------------------
//
__releaseinline void Registers::RescheduleEvent( CpuEventType& thisevt, s32 delta )
{
	DevAssert( thisevt.next == NULL, "iopEvtSched Logic Error: Invalid object state; RescheduleEvent called on an event that is already scheduled." );
	thisevt.OrigDelta = delta;

	// Find the sorted insertion point into the list of active events:

	CpuEventType* curEvt = m_ActiveEvents;
	CpuEventType* prevEvt = NULL;
	s32 runningDelta = -GetPendingCycles();

	while( true )
	{
		// Note: curEvt->next represents the Idle node, which should always be scheduled
		// last .. so the following conditional checks for it and schedules in front of it.
		if( (curEvt == &m_Events[IopEvt_Idle]) || (runningDelta+curEvt->RelativeDelta > delta) )
		{
			thisevt.next = curEvt;
			thisevt.RelativeDelta  = delta - runningDelta;
			curEvt->RelativeDelta -= thisevt.RelativeDelta;

			if( prevEvt == NULL )
			{
				m_ActiveEvents = &thisevt;

				// Node is being inserted at the head of the list, so reschedule the IOP's
				// master counters as needed (only done if the IopEventDispatch isn't currently
				// running, since it does its own internal management of the iopRegs vars).

				evtCycleDuration	= thisevt.RelativeDelta;
				evtCycleCountdown	= thisevt.OrigDelta;

				// EE/IOP sync: If the EE is the current cpu timeslice (actively running code)
				// then we might need to signal it for a branch test.
				// Important: timeouts over 0x10000000 will cause overflows on the EE's current
				// event system, and anything over 0x10000 really doesn't matter anyway

				if( !IsExecuting && (evtCycleCountdown < 0x10000) )
					cpuSetNextBranchDelta( evtCycleCountdown * 8 );
			}
			else
				prevEvt->next = &thisevt;
			break;
		}

		runningDelta += curEvt->RelativeDelta;
		prevEvt = curEvt;
		curEvt  = curEvt->next;
	}

	m_Events[IopEvt_Idle].RelativeDelta = 0x4000;
}

// ------------------------------------------------------------------------
// schedules the requested event for xx cycles into the future, starting from
// the current cycle.
//
__releaseinline void Registers::ScheduleEvent( IopEventType evt, s32 delta )
{
	jASSUME( evt < IopEvt_CountNonIdle );
	CpuEventType& thisevt( m_Events[evt] );
	CancelEvent( thisevt );
	RescheduleEvent( thisevt, delta );
}

void Registers::ScheduleEvent( IopEventType evt, s32 delta, void (*execute)() )
{
	jASSUME( evt < IopEvt_CountNonIdle );
	CpuEventType& thisevt( m_Events[evt] );
	thisevt.Execute = execute;
	ScheduleEvent( evt, delta );
}

// ------------------------------------------------------------------------
//
void Registers::RaiseException()
{
	if( !eeEventTestIsActive )
	{
		// An iop exception has occurred while the EE is running code.
		// Inform the EE to branch so the IOP can handle it promptly:

		iopBranchAction = true;
	}
	
	if( !GetEventInfo( IopEvt_Exception ).next != NULL )
	{
		ScheduleEvent( IopEvt_Exception, 1 );
	}
}

}

__forceinline void PSX_INT( IopEventType evt, int deltaCycles )
{
	PSXDMA_LOG( "Event: %s @ %d cycles", R3000A::tbl_EventNames[evt], deltaCycles );
	R3000A::iopRegs.ScheduleEvent( evt, deltaCycles );
}

// Entry point for the IOP recompiler.
void iopExecutePendingEvents()
{
	R3000A::iopRegs.ExecutePendingEvents();
}

