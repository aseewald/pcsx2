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

namespace IopMemory
{

//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T>
static __forceinline const char* _log_GetIopHwName_Page1( u32 addr )
{
	switch( addr )
	{
		// ------------------------------------------------------------------------
		// SSBUS -- Two Ess'es?
	
		case 0x000: return "SSBUS spd_addr";
		case 0x004: return "SSBUS pio_addr";
		case 0x008:	return "SSBUS spd_delay";
		case 0x00C:	return "SSBUS dev1_delay";
		case 0x010:	return "SSBUS rom_delay";
		case 0x014:	return "SSBUS spu_delay";
		case 0x018:	return "SSBUS dev5_delay";
		case 0x01C:	return "SSBUS pio_delay";
		case 0x020:	return "SSBUS com_delay";
		case 0x400:	return "SSBUS dev1_addr";
		case 0x404:	return "SSBUS spu_addr";
		case 0x408:	return "SSBUS dev5_addr";
		case 0x40C:	return "SSBUS spu1_addr";
		case 0x410:	return "SSBUS dev9_addr3";
		case 0x414:	return "SSBUS spu1_delay";
		case 0x418:	return "SSBUS dev9_delay2";
		case 0x41C:	return "SSBUS dev9_delay3";
		case 0x420:	return "SSBUS dev9_delay1";

		// ------------------------------------------------------------------------
		case 0x060:	return "RAM_SIZE";
		case 0x070:	return "IREG";
		case 0x074:	return "IMASK";

		// BCR_LABEL -- Selects label for BCR depending on operand size (BCR has hi
		// and low values of count and size, respectively)
		#define BCR_LABEL( dma ) (sizeof(T)==4) ? dma" BCR" : dma" BCR_size";
		
		case 0x0a0:	return "DMA2 MADR";
		case 0x0a4:	return BCR_LABEL( "DMA2" );
		case 0x0a6:	return "DMA2 BCR_count";
		case 0x0a8:	return "DMA2 CHCR";
		case 0x0ac:	return "DMA2 TADR";

		case 0x0b0:	return "DMA3 MADR";
		case 0x0b4:	return BCR_LABEL( "DMA3" );
		case 0x0b6:	return "DMA3 BCR_count";
		case 0x0b8:	return "DMA3 CHCR";
		case 0x0bc:	return "DMA3 TADR";

		case 0x0c0:	return "[SPU]DMA4 MADR";
		case 0x0c4:	return BCR_LABEL( "DMA4" );
		case 0x0c6:	return "[SPU]DMA4 BCR_count";
		case 0x0c8:	return "[SPU]DMA4 CHCR";
		case 0x0cc:	return "[SPU]DMA4 TADR";

		case 0x0f0: return "DMA PCR";
		case 0x0f4:	return "DMA ICR";

		case 0x450: return "ICFG";

		case 0x500:	return "[SPU2]DMA7 MADR";
		case 0x504:	return BCR_LABEL( "DMA7" );
		case 0x506: return "[SPU2]DMA7 BCR_count";
		case 0x508:	return "[SPU2]DMA7 CHCR";
		case 0x50C:	return "[SPU2]DMA7 TADR";

		case 0x520:	return "DMA9 MADR";
		case 0x524:	return BCR_LABEL( "DMA9" );
		case 0x526: return "DMA9 BCR_count";
		case 0x528:	return "DMA9 CHCR";
		case 0x52C:	return "DMA9 TADR";

		case 0x530: return "DMA10 MADR";
		case 0x534:	return BCR_LABEL( "DMA10" );
		case 0x536: return "DMA10 BCR_count";
		case 0x538:	return "DMA10 CHCR";
		case 0x53c:	return "DMA10 TADR";

		case 0x570: return "DMA PCR2";
		case 0x574: return "DMA ICR2";

		default: return "Unknown";
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u8 __fastcall iopHwRead8_Page1( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f801xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f801 );

	u32 masked_addr = addr & 0x0fff;

	u8 ret;		// using a return var can be helpful in debugging.
	switch( masked_addr )
	{		
		case 0x040: ret = sioRead8(); break;

		// for use of serial port ignore for now
		//case 0x50: ret = serial_read8(); break;

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
			DevCon::Notice( "*Hardware Read8 from Counter Register [ignored] [addr=0x%02x]", params addr, psxHu8(addr) );
			ret = psxHu8( addr );
		break;

		case 0x46e: ret = DEV9read8( addr ); break;

		case 0x800: ret = cdrRead0(); break;
		case 0x801: ret = cdrRead1(); break;
		case 0x802: ret = cdrRead2(); break;
		case 0x803: ret = cdrRead3(); break;

		default:
			if( (masked_addr >= 0x600) && (masked_addr < 0x700) )
			{
				ret = USBread8( addr );
				PSXHW_LOG( "Hardware Read8 from USB: addr 0x%08x = 0x%02x", addr, ret );
			}
			else
			{
				ret = psxHu8(addr);
				PSXHW_LOG( "*Unknown Hardware Read8 from addr 0x%08x = 0x%02x", addr, ret );
			}
		return ret;
	}

	PSXHW_LOG( "*Hardware Read 8 from addr 0x%08x = 0x%02x", addr, ret );
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u8 __fastcall iopHwRead8_Page3( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f803 );

	if( addr == 0x1f083100 )	// PS/EE/IOP conf related
	{
		PSXHW_LOG( "Hardware Read8 from addr 0x%08x [IOP Ram Status] - returning 0x10", addr );
		return 0x10; // Dram 2M
	}

	PSXHW_LOG( "*Unknown Hardware Read8 from addr 0x%08x = 0x%02x", addr, psxHu8(addr) );
	return psxHu8( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u8 __fastcall iopHwRead8_Page8( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f808 );

	u8 ret;

	if( addr == 0x1f808264 )
	{
		ret = sio2_fifoOut();//sio2 serial data feed/fifo_out
		PSXHW_LOG( "Hardware Read8 from SIO FIFO 0x%08x = 0x%02x", addr, ret );
	}
	else
	{
		ret = psxHu8( addr );
		PSXHW_LOG( "*Unknown Hardware Read8 from addr 0x%08x = 0x%02x", addr, ret );
	}
	return ret;	
}

//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T >
static __forceinline T _HwRead_16or32_Page1( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f801xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f801 );

	// all addresses should be aligned to the data operand size:
	jASSUME(
		( sizeof(T) == 2 && (addr & 1) == 0 ) ||
		( sizeof(T) == 4 && (addr & 3) == 0 )
	);


	u32 masked_addr = addr & 0x0fff;
	T ret;
	const char* regname = "Unknown";

	// ------------------------------------------------------------------------
	// Counters 16-bit varieties!
	//
	if( masked_addr >= 0x100 && masked_addr < 0x130 )
	{
		int cntidx = ( masked_addr >> 4 ) & 0xf;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				regname = "CNT16_READ";
				ret = (T)psxRcntRcount16( cntidx );
			break;

			case 0x4:
				regname = "CNT16_MODE";
				ret = psxCounters[cntidx].mode;
				
				// hmm!  The old code only did this bitwise math for 16 bit reads.
				// Logic indicates it should do the math consistently.  Question is,
				// should it do the logic for both 16 and 32, or not do logic at all?

				psxCounters[cntidx].mode &= ~0x1800;
				psxCounters[cntidx].mode |= 0x400;
			break;

			case 0x8:
				regname = "CNT16_TARGET";
				ret = psxCounters[cntidx].target;
			break;
		}
	}
	// ------------------------------------------------------------------------
	// Counters 32-bit varieties!
	//
	else if( masked_addr >= 0x480 && masked_addr < 0x4b0 )
	{
		int cntidx = ( masked_addr >> 4 ) & 0xf;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				regname = "CNT32_READ";
				ret = (T)psxRcntRcount32( cntidx );
			break;

			case 0x4:
				regname = "CNT32_MODE";
				ret = psxCounters[cntidx].mode;

				// hmm!  The old code only did this bitwise math for 16 bit reads.
				// Logic indicates it should do the math consistently.  Question is,
				// should it do the logic for both 16 and 32, or not do logic at all?

				psxCounters[cntidx].mode &= ~0x1800;
				psxCounters[cntidx].mode |= 0x400;
			break;

			case 0x8:
				regname = "CNT32_TARGET";
				ret = psxCounters[cntidx].target;
			break;
		}
	}
	// ------------------------------------------------------------------------
	// USB, with both 16 and 32 bit interfaces
	//
	else if( (masked_addr >= 0x600) && (masked_addr < 0x700) )
	{
		regname = "USB";
		ret = (sizeof(T) == 2) ? USBread16( addr ) : USBread32( addr );
	}
	// ------------------------------------------------------------------------
	// SPU2, accessible in 16 bit mode only!
	//
	else if( (masked_addr >= 0xc00) && (masked_addr < 0xe00) )
	{
		regname = "SPU2";
		if( sizeof(T) == 2 )
			ret = SPU2read( addr );
		else
		{
			DevCon::Notice( "*PCSX2* SPU2 Hardware Read32 (addr=0x%08X)?  What manner of trickery is this?!", params addr );
			ret = 0;
		}
	}
	else
	{
		switch( masked_addr )
		{
			// ------------------------------------------------------------------------
			case 0x040:
				regname = "SIO";
				ret  = sioRead8();
				ret |= sioRead8() << 8;
				if( sizeof(T) == 4 )
				{
					ret |= sioRead8() << 16;
					ret |= sioRead8() << 24;
				}
			break;

			case 0x044:
				regname = "SIO_STAT";
				ret = sio.StatReg;
			break;

			case 0x048:
				regname = "SIO_MODE";
				ret = sio.ModeReg;
				if( sizeof(T) == 4 )
				{
					regname = "SIO_MODE+CTRL";
					ret |= sio.CtrlReg << 16;
				}
			break;
		
			case 0x04a:
				regname = "SIO_CTRL";
				ret = sio.CtrlReg;
			break;

			case 0x04e:
				regname = "SIO_BAUD";
				ret = sio.BaudReg;
			break;

			// ------------------------------------------------------------------------
			//Serial port stuff not support now ;P
			// case 0x050: hard = serial_read32(); break;
			//	case 0x054: hard = serial_status_read(); break;
			//	case 0x05a: hard = serial_control_read(); break;
			//	case 0x05e: hard = serial_baud_read(); break;

			case 0x078:
				regname = "ICTRL";
				ret = psxHu32(0x1078);
				psxHu32(0x1078) = 0;
			break;

			case 0x07a:
				regname = "ICTRL_hiword";
				ret = psxHu16(0x107a);
				psxHu32(0x1078) = 0;	// most likely should clear all 32 bits here.
			break;

			// ------------------------------------------------------------------------
			// Soon-to-be outdated SPU2 DMA hack (spu2 manages its own DMA MADR).
			//
			case 0x0C0:
				regname = "[SPU]DMA4 MADR";
				HW_DMA4_MADR = SPU2ReadMemAddr(0);
			break;

			case 0x500:
				regname = "[SPU2]DMA7 MADR";
				HW_DMA7_MADR = SPU2ReadMemAddr(1);
			break;

			// ------------------------------------------------------------------------
			// Legacy GPU  emulation (not needed).
			// The IOP emulates the GPU itself through the EE's hardware.

			/*case 0x1f801810:
				PSXHW_LOG("GPU DATA 32bit write %lx", value);
				GPU_writeData(value); return;
			case 0x1f801814:
				PSXHW_LOG("GPU STATUS 32bit write %lx", value);
				GPU_writeStatus(value); return;

			case 0x1f801820:
				mdecWrite0(value); break;
			case 0x1f801824:
				mdecWrite1(value); break;*/

			// ------------------------------------------------------------------------

			case 0x46e:
				regname = "DEV9_R_REV";
				ret = DEV9read16( addr );
			break;

			default:
				regname = _log_GetIopHwName_Page1<T>( addr );
				ret = psxHu32( addr );
			break;
		}
	}
	
	PSXHW_LOG( "Hardware Read%s from %s addr 0x%08x = 0x%04x",
		sizeof(T) == 2 ? "16" : "32", regname, addr, ret
	);
	return ret;
}

// Some Page 2 mess?  I love random question marks for comments!
//case 0x1f802030: hard =   //int_2000????
//case 0x1f802040: hard =//dip switches...??

//////////////////////////////////////////////////////////////////////////////////////////
//
u16 __fastcall iopHwRead16_Page1( u32 addr )
{
	return _HwRead_16or32_Page1<u16>( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u16 __fastcall iopHwRead16_Page3( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f803 );

	u16 ret = psxHu16( addr );
	PSXHW_LOG( "*Unknown Hardware Read16 from addr 0x%08x = 0x%04x", addr, ret );
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u16 __fastcall iopHwRead16_Page8( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f808 );

	u16 ret = psxHu16( addr );
	PSXHW_LOG( "*Unknown Hardware Read16 from addr 0x%08x = 0x%04x", addr, ret );
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u32 __fastcall iopHwRead32_Page1( u32 addr )
{
	return _HwRead_16or32_Page1<u16>( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u32 __fastcall iopHwRead32_Page3( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f803 );
	return psxHu32( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
u32 __fastcall iopHwRead32_Page8( u32 addr )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	jASSUME( (addr & 0xfffff000) == 0x1f808 );

	u32 masked_addr = addr & 0x0fff;

	if( masked_addr >= 0x200 )
	{
		if( masked_addr < 0x240 )
		{
			const int parm = (masked_addr-0x200) / 4;
			const u32 ret = sio2_getSend3( parm );
			PSXHW_LOG( "Hardware Read32 from SIO2 param[%d] = 0x%08x", parm, ret );
			return ret;
		}
		else if( masked_addr < 0x260 )
		{
			// SIO2 Send commands alternate registers.  First reg maps to Send1, second
			// to Send2, third to Send1, etc.  And the following clever code does this:
			
			const int parm = (masked_addr-0x240) / 8;
			const u32 ret = (masked_addr & 4) ? sio2_getSend2( parm ) : sio2_getSend1( parm );
			PSXHW_LOG( "Hardware Read32 from SIO2 send%d[%d] = 0x%08x",
				(masked_addr & 4) ? 2 : 1, parm, ret
			);
			return ret;
		}

		const char* regname = "Unknown";
		u32 ret;
		
		switch( masked_addr )
		{
			case 0x268:
				regname = "SIO2_CTRL";
				ret = sio2_getCtrl();
			break;

			case 0x26C:
				regname = "SIO2_RECV1";
				ret = sio2_getRecv1();
			break;

			case 0x270:
				regname = "SIO2_RECV2";
				ret = sio2_getRecv2();
			break;

			case 0x274:
				regname = "SIO2_RECV3";
				ret = sio2_getRecv3();
			break;

			case 0x278:
				regname = "SIO2_8278";
				ret = sio2_get8278();
			break;

			case 0x27C:
				regname = "SIO2_827C";
				ret = sio2_get827C();
			break;

			case 0x280:
				regname = "SIO2_INTR";
				ret = sio2_getIntr();
			break;
			
			default:
				ret = psxHu32( addr );
				PSXHW_LOG( "*Unknown Hardware Read32 from addr 0x%08x = 0x%08x", addr, ret );
			return ret;
		}

		PSXHW_LOG( "Hardware Read32 from [%s] addr 0x%08x = 0x%02x", regname, addr, ret );
		return ret;
	}

	u32 ret = psxHu32( addr );
	PSXHW_LOG( "*Unknown Hardware Read32 from addr 0x%08x = 0x%08x", addr, ret );
	return ret;
}

}