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


// ------------------------------------------------------------------------
// Fixed bit constants for the IOP's counter rates [needed for the IOP's pixel clock].
// The slowest counter is 1/256 for 32 bit counters, and 1/8 for 16 bit counters.  The
// 32 bit counters multiply into 64 bit results, which means we have 32-8 == 24 bits for
// fixed point there.  The 16 bit counters multiply into 32 bits, which gives us 16-3 bits
// for fixed point.  To keep life simple I prefer to use the same fixed point modifier for
// both, so let's go with 12 (nice round number!).  That should plenty enough for an
// accurate pixel clock.
//
static const int IopRate_FixedBits		= 12;
static const int IopRate_FixedRange		= (1<<IopRate_FixedRange);
static const int IopRate_FixedBitMask	= IopRate_FixedRange - 1;

// ------------------------------------------------------------------------
// The IOP's pixel rate (same as the PS1's) is a fixed counter source running at 13.5mhz.
// This generates a counter which is roughly 2.71 IOP clocks apart (less if the IOP is in
// legacy PS1 mode).  The best way to represent this value efficiently, without using
// the FPU, is to make use of some fixed point magic:
//
static const int IopPixelClock_Hertz = 13500000;
static const int IopRate_Pixel()
{
	// fixme: PSXCLK should be a variable, not a const.
	return (PSXCLK << IopRate_FixedBits) / IopPixelClock_Hertz;
}

// ------------------------------------------------------------------------
//
static const u32 IopRate_HBLANK()
{
	u32 fps, scans;
	if(Config.PsxType & 1)
	{
		fps = 50;
		scans = 625;
	}
	else
	{
		fps = 59.94;
		scans = 525;
	}
	
	u64 Scanline = (((u64)PSXCLK) << IopRate_FixedBits) / (fps*scans);
	return Scanline;
}


// ------------------------------------------------------------------------
// Bit fields defined by the IOP's hardware counter registers:

static const u32 IOPCNT_ENABLE_GATE		= (1<<0);	// enables gate-based counters
static const u32 IOPCNT_INT_TARGET		= (1<<4);	// 0x10  triggers an interrupt on targets
static const u32 IOPCNT_INT_OVERFLOW	= (1<<5);	// 0x20  triggers an interrupt on overflows
static const u32 IOPCNT_ALT_SOURCE		= (1<<8);	// 0x100 uses hblank on counters 1 and 3, and PSXCLOCK on counter 0

struct IopGateFlags_t
{
	bool hBlank0:1;		// counter 0 is counting HBLANKs (16bit)
	bool vBlank1:1;		// counter 1 is counting VBLANKs (16bit)
	bool vBlank3:1;		// Counter 3 is counting VBLANKs in 32 bit style.
};

static IopGateFlags_t iopGateFlags;

enum IopCntEventType
{
	IopCntEvt_Target,
	IopCntEvt_Overflow,
	
	// Special event types for when the overflow that's been scheduled
	// is too far into the future for the delta to fit within the context
	// of an s32.
	IopCntEvt_RescheduleTarget,
	IopCntEvt_RescheduleOverflow
};

//////////////////////////////////////////////////////////////////////////////////////////
// IopCounterType
//
// Template Parameters >>
//  CntType		- Counter's count/target type, either u16 or u32.
//  IntMathType	- Type used for count/target mathematics (either s32 or s64)
//
template< typename CntType, typename IntMathType >
class IopCounterType
{
public:
	// ------------------------------------------------------------------------
	// Variable Members section -->

	// self-proclaimed counter index, rages 0-5.  Indexes 0-2 are 16 bit counters and
	// 3-5 are 32 bit counters.
	const u8 Index;

	// interrupt raised by this counter when counter target/overflow occurs.
	const u32 Interrupt;

	// Flag indicating the active status of counting.
	bool IsCounting;

	// When true, delays target checking until post-overflow.
	bool IsFutureTarget;

	CntType Count;
	CntType Target;

	// rate of counting (cycles per counter increment), typically a cached value based
	// on counter index and mode flags.
	u32 m_rate;

	// counter mode flags.  Use ReadMode/WriteMode to access them from the hwRegisters.
	// (only access directly for logging/debugging purposes).
	u32 m_mode;

public:
	// ------------------------------------------------------------------------
	// Method Members Section -->
	
	IopCounterType() :
		Index( 0xcd )			// some kinda..
	,	Interrupt( 0xcdcd )		// ... invalid object state!
	{
	}
	
	IopCounterType( u8 cntidx ) : 
		Index( cntidx )
	,	Interrupt( Is16Bit() ? (4+cntidx) : (14 + (cntidx-3)) )
	,	IsCounting( true )
	,	IsFutureTarget( true )
	,	Count( 0 )
	,	Target( 0 )
	,	m_rate( 1 << IopRate_FixedBits )
	,	m_mode( 0x400 )
	{
	}
	
	bool Is16Bit() const { return (sizeof(CntType)==2); }
	int GetCntBits() const { return sizeof(CntType) * 8; }

	IopEventType GetEventId() const
	{
		return (IopEventType)(IopEvt_Counter0 + Index);
	}

	const IopEventSystem::ScheduleInfo& GetEvtInfo() const
	{
		return iopEvtSys.GetInfo( GetEventId() );
	}

	__forceinline IopEventType GetEventType() const
	{
		IopEventType retval( (IopEventType)GetEvtInfo().ModeType );

		if( Is16Bit() )
			DevAssume( retval <= IopCntEvt_Overflow, "IopCounterEvent Error: Invalid IopEventType specified on 16-bit counter." );
		else
			DevAssume( retval <= IopCntEvt_RescheduleOverflow, "IopCounterEvent Error: Invalid IopEventType specified on 32-bit counter." );

		return retval;
	}

	CntType ReadCount() const;
	u32  ReadMode();

	void WriteCount( CntType newcount );
	void WriteMode( u32 newmode );
	void WriteTarget( CntType Target );

	void CheckStartGate();
	void CheckEndGate();
	void Stop();
	void Reschedule();
	void ResetCount( CntType newcnt );

	void OnEvent();
	
	void ScheduleTarget() const;
	void ScheduleOverflow() const;
	void Freeze( SaveState& state );

protected:
	// ------------------------------------------------------------------------
	// Internal / Protected Methods:
	
	void _writeMode16( u32 newmode );
	void _writeMode32( u32 newmode );
	
	// ------------------------------------------------------------------------
	// scales the given delta by the counter's rate, with fixed point accuracy.
	// Return value is the integer result (Without fixed point).  Result is 64-bit for
	// 32 bit counters, so that the scheduler can handle 
	//
	IntMathType ScaleByRate( IntMathType delta ) const
	{
		return (delta * m_rate) >> IopRate_FixedBits;
	}

	// ------------------------------------------------------------------------
	// Important: If you call this function, you must also call Reschedule() or
	// iopEvtSys.CancelEvent!  Failing to do so results in unpredictable event
	// scheduling.
	//
	__releaseinline void _update_counted_timepass()
	{
		if( GetEvtInfo().IsEnabled )
		{
			if( IsDevBuild )
			{
				u64 test = Count + GetEvtInfo().GetTimepass();
				DevAssume( Count < (Is16Bit() ? 0x10000 : 0x100000000), "IopCounter Logic Error : Rescheduled timer exceeds overflow value." );
			}
			Count += GetEvtInfo().GetTimepass();
		}
	}

	// ------------------------------------------------------------------------
	//
	__releaseinline void _schedule_helper( IntMathType evtDelta, IopCntEventType etype ) const
	{
		if( !Is16Bit() && (evtDelta > 0x40000000) )
		{
			// 32 bit counters can overflow the Delta's 32 bit scope.  This copes for that
			// by using a special "RescheduleOverflow" event type available in 32 bit mode:

			evtDelta = 0x40000000;
			switch( etype )
			{
				case IopCntEvt_Overflow:	etype = IopCntEvt_RescheduleOverflow; break;
				case IopCntEvt_Target:		etype = IopCntEvt_RescheduleTarget;	  break;
				jNO_DEFAULT;
			}
		}
		iopEvtSys.ScheduleEvent( GetEventId(), (s32)evtDelta );
		iopEvtSys.SetEventModeType( GetEventId(), (int)etype );
	}
};

typedef IopCounterType<u16,u32> IopCounterType16;
typedef IopCounterType<u32,u64> IopCounterType32;

static IopCounterType16 iopCounters16[3];
static IopCounterType32 iopCounters32[3];

#define IopCounterMethod( return_type ) template< typename CntType, typename MathType > __forceinline return_type
#define ICT IopCounterType<CntType,MathType>

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::Stop()
{
	_update_counted_timepass();
	IsCounting	= false;
	iopEvtSys.CancelEvent( GetEventId() );
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::ScheduleOverflow() const
{
	if( Is16Bit() )
		_schedule_helper( ScaleByRate(0x10000 - Count), IopCntEvt_Overflow );
	else
		_schedule_helper( ScaleByRate(0x100000000ULL - Count), IopCntEvt_Overflow );
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::ScheduleTarget() const
{
	DevAssume( Target >= Count, "IopCounter Logic Error: ScheduleTarget called when target is already past." );

	// Schedule the target only if it's purposefully enabled.  Otherwise just bypass it
	// and schedule the overflow.  No point in handling a dud target.

	if( (m_mode & IOPCNT_INT_TARGET) || (m_mode & 0x08) )
		_schedule_helper( ScaleByRate( Target - Count ), IopCntEvt_Target );
	else
		ScheduleOverflow();
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::Reschedule()
{
	if( IsFutureTarget || (Count > Target) )
		ScheduleOverflow();
	else
		ScheduleTarget();
}

// ------------------------------------------------------------------------
// Resets the count to a specific value and re-schedules the timer.
//
IopCounterMethod(void) ICT::ResetCount( CntType newcnt )
{
	Count = newcnt;
	IsFutureTarget = false;
	iopEvtSys.CancelEvent( GetEventId() );

	Reschedule();
}

// ------------------------------------------------------------------------
IopCounterMethod(CntType) ICT::ReadCount() const
{
	uint retval = (uint)Count;

	if( IsCounting )
	{
		s32 delta = (((MathType)GetEvtInfo().GetTimepass()) << IopRate_FixedBits) / m_rate;
		retval += delta;
	}

	PSXCNT_LOG("IOP Counter[%d] readCount%d = 0x%08x", Index, GetCntBits(), retval );
	return (CntType)retval;
}

// ------------------------------------------------------------------------
// Reads the counter mode flags.
// 
IopCounterMethod(u32) ICT::ReadMode()
{
	PSXCNT_LOG( "IOP Counter[%d] readMode = 0x%04x", Index, m_mode );

	u32 retval = m_mode;

	// 0x400 resets on mode reads, 0x800 and 0x1000 clear on read.
	m_mode |= 0x0400;
	m_mode &= ~0x1800;

	return retval;
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::WriteCount( CntType newcount )
{
	PSXCNT_LOG("IOP Counter[%d] writeCount%d = %x", Index, GetCntBits(), newcount );
	ResetCount( newcount );
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::WriteTarget( CntType newtarg )
{
	PSXCNT_LOG("IOP Counter[%d] writeTarget%d = 0x%08x", Index, GetCntBits(), newtarg );
	Target = newtarg;

	_update_counted_timepass();

	// protect the target from an early arrival, if the target is behind the current count:
	IsFutureTarget = ( Target <= Count );
	Reschedule();
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::_writeMode16( u32 newmode )
{
	if( Index == 2 )
	{
		switch( newmode & 0x200 )
		{
			case 0x000: m_rate = 1 << IopRate_FixedBits; break;
			case 0x200: m_rate = 8 << IopRate_FixedBits; break;
			jNO_DEFAULT;
		}

		if((m_mode & 0x7) == 0x7 || (m_mode & 0x7) == 0x1)
			IsCounting = false;
	}
	else
	{
		jASSUME( Index == 0 || Index == 1 );
		
		// Counters 0 and 1 can select PIXEL or HSYNC as an alternate source:
		m_rate = 1 << IopRate_FixedBits;

		if( newmode & IOPCNT_ALT_SOURCE )
			m_rate = (Index==0) ? IopRate_Pixel() : IopRate_HBLANK();

		if( m_mode & IOPCNT_ENABLE_GATE )
		{
			PSXCNT_LOG( "IOP Counter[%d] Notice: Gate Check Enabled.", Index );
			if( Index == 0 )
				iopGateFlags.hBlank0 = true;
			else
				iopGateFlags.vBlank1 = true;
		}
		else
		{
			if( Index == 0 )
				iopGateFlags.hBlank0 = false;
			else
				iopGateFlags.vBlank1 = false;
		}
	}
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::_writeMode32( u32 newmode )
{
	if( Index == 3 )
	{
		// Counter 3 has the HBlank as an alternate source.
		m_rate = 1 << IopRate_FixedBits;
		if( newmode & IOPCNT_ALT_SOURCE)
			m_rate = IopRate_HBLANK();

		if( m_mode & IOPCNT_ENABLE_GATE )
		{
			PSXCNT_LOG( "IOP Counter[3] Notice: Gate Check Enabled" );
			iopGateFlags.vBlank3 = true;
		}
		else iopGateFlags.vBlank3 = false;
	}
	else
	{
		jASSUME( Index == 4 || Index == 5 );
		
		switch( newmode & 0x6000 )
		{
			case 0x0000: m_rate = 1;   break;
			case 0x2000: m_rate = 8;   break;
			case 0x4000: m_rate = 16;  break;
			case 0x6000: m_rate = 256; break;
			
			jNO_DEFAULT
		}
		
		m_rate <<= IopRate_FixedBits;

		// Need to set a rate and target
		if((m_mode & 0x7) == 0x7 || (m_mode & 0x7) == 0x1)
		{
			Console::WriteLn( "Gate set on IOP Counter %d, disabling", params Index );
			IsCounting = false;
		}
	}
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::WriteMode( u32 newmode )
{
	PSXCNT_LOG( "IOP Counter[%d] writeMode%d = 0x%04X", Index, GetCntBits(), newmode );

	// Bit 0x0400 gets enabled on mode writes and reads.
	// Counting is enabled on mode writes by default, and then selectively disabled if the
	// counter is using specific gate flag combinations.
	
	m_mode		= newmode | 0x400;
	IsCounting	= true;

	if( Is16Bit() )
		_writeMode16( newmode );
	else
		_writeMode32( newmode );

	ResetCount(0);
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::OnEvent()
{
	// disabled counters shouldn't have events scheduled
	jASSUME( IsCounting );

	// HBLANK counters update on the v/hsyncs, not the event system.
	//jASSUME( Rate != PSXHBLANK );
	
	// Event is "guaranteed" to happen at the scheduled IOP cycle, so no need
	// to do an complicated logic to account for remainder cycles or whatever.

	switch( GetEventType() ) 
	{
		// OnTarget - Set counter to target and schedule the overflow.
		// Note: Overflows always schedule regardless of the OverlowInterurpt flag, since
		// the counter depends on the Overflow event to wrap the counter and schedule
		// targets.
		case IopCntEvt_Target:
		{
			PSXCNT_LOG( "IOP Counter[%d] Target Reached @ 0x%08x", Index, Target );

			DevAssume( (m_mode & IOPCNT_INT_TARGET) || (m_mode & 0x08),
				"IopCntUpdate Logic Error: Target event scheduled, but no target purpose is enabled."
			);

			if( IsFutureTarget )
			{
				// [TODO] : remove this log/check once code is confirmed unbuggy. :)
				Console::Notice( "IopCntUpdate Logic Warning: Target event scheduled on Future Target (event ignored)" );
			}

			Count = Target;

			if( !IsFutureTarget )	// remove this check when code is confirmed unbuggy
			{
				if( m_mode & IOPCNT_INT_TARGET )
				{
					if( m_mode & 0x80 )
						m_mode &= ~0x0400; // Interrupt flag
					m_mode |= 0x0800; // Target flag

					iopRegs.RaiseExtInt( Interrupt );
				}
				
				if( m_mode & 0x08 )
				{
					// Reset on target
					Count = 0;
					if(!(m_mode & 0x40))
					{
						Console::Notice( "Counter %x repeat intr not set on zero ret, ignoring target", params Index );
						IsFutureTarget = true;
					}
				}
				else
					IsFutureTarget = true;
			}

			ScheduleOverflow();
		}
		break;

		// OnOverflow - Wrap counter to 0 and schedule the target if target enabled.
		// (otherwise schedule the next overflow)
		case IopCntEvt_Overflow:
		{
			PSXCNT_LOG( "IOP Counter[%d] Overflow Reached", Index, Target );

			Count = 0;
			IsFutureTarget = false;

			if( m_mode & IOPCNT_INT_OVERFLOW )
			{
				// Overflow interrupt
				iopRegs.RaiseExtInt( Interrupt );
				m_mode |= 0x1000; // Overflow flag
				if(m_mode & 0x80)
					m_mode &= ~0x0400; // Interrupt flag
			}
			ScheduleTarget();
		}
		break;
		
		case IopCntEvt_RescheduleTarget:
			ScheduleTarget();
		break;

		case IopCntEvt_RescheduleOverflow:
			ScheduleOverflow();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// The IOP Does Sci-Fi: How?  Gate Travel!

/*
Gate:
   TM_NO_GATE                   000
   TM_GATE_ON_Count             001
   TM_GATE_ON_ClearStart        011
   TM_GATE_ON_Clear_OFF_Start   101
   TM_GATE_ON_Start             111

   V-blank  ----+    +----------------------------+    +------
                |    |                            |    |
                |    |                            |    |
                +----+                            +----+
 TM_NO_GATE:

                0================================>============

 TM_GATE_ON_Count:

                <---->0==========================><---->0=====

 TM_GATE_ON_ClearStart:

                0====>0================================>0=====

 TM_GATE_ON_Clear_OFF_Start:

                0====><-------------------------->0====><-----

 TM_GATE_ON_Start:

                <---->0==========================>============
*/

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::CheckStartGate()
{
	if(!(m_mode & IOPCNT_ENABLE_GATE)) return;	// not enabled? nothing to do!

	switch((m_mode & 0x6) >> 1)
	{
		case 0x0: // GATE_ON_count - stop count on gate start:
			Stop();
		return;

		case 0x1: //GATE_ON_ClearStart - count normally with resets after every end gate
			// do nothing - All counting will be done on a need-to-count basis.
		return;

		case 0x2: //GATE_ON_Clear_OFF_Start - start counting on gate start, stop on gate end
			IsCounting	= true;
			ResetCount(0);
		break;

		case 0x3: //GATE_ON_Start - start and count normally on gate end (no restarts or stops or clears)
			// do nothing!
		return;
	}
}

// ------------------------------------------------------------------------
IopCounterMethod(void) ICT::CheckEndGate()
{
	if(!(m_mode & IOPCNT_ENABLE_GATE)) return; //Ignore Gate

	switch((m_mode & 0x6) >> 1)
	{
		case 0x0: //GATE_ON_count - reset and start counting
		case 0x1: //GATE_ON_ClearStart - count normally with resets after every end gate
			IsCounting = true;
			ResetCount(0);
		break;

		case 0x2: //GATE_ON_Clear_OFF_Start - start counting on gate start, stop on gate end
			Stop();
		return;	// do not set the counter

		case 0x3: //GATE_ON_Start - start and count normally (no restarts or stops or clears)
			if( !IsCounting )
			{
				IsCounting = true;
				ResetCount(0);
			}
		break;
	}
}

IopCounterMethod(void) ICT::Freeze( SaveState& state )
{
	// Index and Interrupt are consts initialized at program startup/reset, so no need
	// to save them into the state.

	state.Freeze( IsCounting );
	state.Freeze( IsFutureTarget );

	state.Freeze( Count );
	state.Freeze( Target );
	state.Freeze( m_rate );
	state.Freeze( m_mode );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Public API Helpers

__releaseinline void IopCounters::Reset()
{
	for( int i=0; i<3; ++i )
	{
		new (&iopCounters16[i]) IopCounterType16( i );
		iopCounters16[i].Reschedule();
		new (&iopCounters32[i]) IopCounterType32( i+3 );
		iopCounters32[i].Reschedule();
	}
}

// ------------------------------------------------------------------------
__releaseinline u16 IopCounters::ReadCount16( uint cntidx )
{
	return iopCounters16[cntidx].ReadCount();
}

__releaseinline u32 IopCounters::ReadCount32( uint cntidx )
{
	return iopCounters32[cntidx-3].ReadCount();
}

// ------------------------------------------------------------------------
__releaseinline u16 IopCounters::ReadTarget16( uint cntidx )
{
	return iopCounters16[cntidx].Target;
}

__releaseinline u32 IopCounters::ReadTarget32( uint cntidx )
{
	return iopCounters32[cntidx-3].Target;
}

// ------------------------------------------------------------------------
__releaseinline u32 IopCounters::ReadMode( uint cntidx )
{
	if( cntidx < 3 )
		return iopCounters16[cntidx].ReadMode();

	else if( cntidx < 6 )
		return iopCounters32[cntidx-3].ReadMode();

	else
		throw Exception::LogicError( "Invalid counter index passed to IopCounters_ReadMode." );
}

// ------------------------------------------------------------------------
__releaseinline void IopCounters::WriteCount16( uint cntidx, u16 count )
{
	iopCounters16[cntidx].WriteCount( count );
}

__releaseinline void IopCounters::WriteCount32( uint cntidx, u32 count )
{
	iopCounters32[cntidx-3].WriteCount( count );
}

// ------------------------------------------------------------------------
__releaseinline void IopCounters::WriteTarget16( uint cntidx, u16 target )
{
	iopCounters16[cntidx].WriteTarget( target );
}

__releaseinline void IopCounters::WriteTarget32( uint cntidx, u32 target )
{
	iopCounters32[cntidx-3].WriteTarget( target );
}

// ------------------------------------------------------------------------
__releaseinline void IopCounters::WriteMode( uint cntidx, u32 mode )
{
	if( cntidx < 3 )
		iopCounters16[cntidx].WriteMode( mode );

	else if( cntidx < 6 )
		iopCounters32[cntidx-3].WriteMode( mode );

	else
		throw Exception::LogicError( "Invalid counter index passed to IopCounters_WriteMode." );
}

// ------------------------------------------------------------------------

__releaseinline void IopCounters::VBlankStart()
{
	PSXDMA_LOG( " -------->>>> Iop Vsync Start!  O_o <<<<--------" );
	cdvdVsync();
	iopRegs.RaiseExtInt( IopInt_VBlank );
	//psxHu32(0x1070) |= 1;
	if( iopGateFlags.vBlank1 ) iopCounters16[1].CheckStartGate();
	if( iopGateFlags.vBlank3 ) iopCounters32[3].CheckStartGate();
}

__releaseinline void IopCounters::VBlankEnd()
{
	PSXDMA_LOG( " -------->>>>  Iop Vsync End!  o_O  <<<<--------" );
	iopRegs.RaiseExtInt( IopInt_VBlankEnd );
	//psxHu32(0x1070) |= 0x800;
	if( iopGateFlags.vBlank1 ) iopCounters16[1].CheckEndGate();
	if( iopGateFlags.vBlank3 ) iopCounters32[3].CheckEndGate();
}

__releaseinline void IopCounters::Update( uint cntidx )
{
	if( cntidx < 3 )
		iopCounters16[cntidx].OnEvent();

	else if( cntidx < 6 )
		iopCounters32[cntidx-3].OnEvent();

	else
		throw Exception::LogicError( "Invalid counter index passed to IopCounters::Update." );
}

__releaseinline void IopCounters::CheckStartGate0()
{
	if( iopGateFlags.hBlank0 )
		iopCounters16[0].CheckStartGate();
}

__releaseinline void IopCounters::CheckEndGate0()
{
	if( iopGateFlags.hBlank0 )
		iopCounters16[0].CheckEndGate();
}

static void _setGates()
{
	iopGateFlags.hBlank0 = (iopCounters16[0].m_mode & IOPCNT_ENABLE_GATE);
	iopGateFlags.vBlank1 = (iopCounters16[1].m_mode & IOPCNT_ENABLE_GATE);
	iopGateFlags.vBlank3 = (iopCounters32[0].m_mode & IOPCNT_ENABLE_GATE);
}

void SaveState::psxRcntFreeze()
{
	FreezeTag( "iopCounters" );

	for( int i=0; i<3; ++i )
	{
		iopCounters16[i].Freeze( *this );
		iopCounters32[i].Freeze( *this );
	}

	if( IsLoading() )
		_setGates();
}
