
#include "PrecompiledHeader.h"

#include "Common.h"		// needed to communicate with EE's memory space
#include "IopCommon.h"

namespace IopMemory
{

// ------------------------------------------------------------------------
u8 __fastcall SifRead8( u32 iopaddr )
{
	SIF_LOG("SIFreg Read8, addr=0x%08x [ignored, returns zero]", iopaddr);
	return 0;
}

// ------------------------------------------------------------------------
u16 __fastcall SifRead16( u32 iopaddr )
{
	u16 ret;

	switch( iopaddr & 0xf0 )
	{
		case 0x00:
			ret = psHu16(SBUS_F200);
		break;
		
		case 0x10:
			ret = psHu16(SBUS_F210);
		break;
		
		case 0x40:
			ret = psHu16(SBUS_F240) | 0x0002;
		break;
		
		case 0x60:
			ret = 0;
		break;

		default:
			// Note: Should probably just be a Bus Error here.
			ret = psxSu16( iopaddr );
		break;
	}
	SIF_LOG("SIFreg Read16, addr=0x%08x value=0x%04x", iopaddr, ret);
	return ret;
}

// ------------------------------------------------------------------------
u32 __fastcall SifRead32( u32 iopaddr )
{
	u32 ret;

	switch( iopaddr & 0xF0)
	{
		case 0x00:
			ret = psHu32(SBUS_F200);
		break;

		case 0x10:
			ret = psHu32(SBUS_F210);
		break;

		case 0x20:
			ret = psHu32(SBUS_F220);
		break;

		case 0x30:	// EE Side
			ret = psHu32(SBUS_F230);
		break;

		case 0x40:
			ret = psHu32(SBUS_F240) | 0xF0000002;
		break;

		case 0x60:
			ret = 0;
		break;

		default:
			// Note: Should probably just be a Bus Error here too.
			ret = psxSu32(iopaddr);
		break;
	}
	SIF_LOG("SIFreg Read32 addr=0x%08x value=0x%08x", iopaddr, ret);
	return ret;
}

// ------------------------------------------------------------------------
void __fastcall SifWrite8( u32 iopaddr, u8 data )
{
	psxSu8( iopaddr ) = data;
	SIF_LOG("SIFreg Write8 addr=0x%08x value=0x%02x", iopaddr, data);
}

// ------------------------------------------------------------------------
void __fastcall SifWrite16( u32 iopaddr, u16 data )
{
	SIF_LOG("SIFreg Write16 addr=0x%08x value=0x%04x", iopaddr, data);

	switch( iopaddr & 0xf0 )
	{
		case 0x10:
			// write to ps2 mem
			psHu16(SBUS_F210) = data;
		return;
		
		case 0x40:
		{
			u32 temp = data & 0xF0;
			// write to ps2 mem
			if(data & 0x20 || data & 0x80)
			{
				psHu16(SBUS_F240) &= ~0xF000;
				psHu16(SBUS_F240) |= 0x2000;
			}

			if(psHu16(SBUS_F240) & temp)
				psHu16(SBUS_F240) &= ~temp;
			else
				psHu16(SBUS_F240) |= temp;
		}
		return;

		case 0x60:
			psHu32(SBUS_F260) = 0;
		return;
		
		default:
			// Chances are writes past 0x60 are a BUS ERROR.  Needs testing.
		break;
	}
	psxSu16(iopaddr) = data;
}

// ------------------------------------------------------------------------
void __fastcall SifWrite32( u32 iopaddr, u32 data )
{
	SIF_LOG("SIFreg Write32 addr=0x%08x value=0x%08x", iopaddr, data);

	switch( iopaddr & 0xf0 )
	{
		case 0x00: return;	// EE write path (EE/IOP readable, IOP ignores writes)

		case 0x10:			// IOP write path (EE/IOP readable, EE ignores writes)
			psHu32(SBUS_F210) = data;
		return;

		case 0x20:			// Bits cleared when written from IOP.
			// write to ps2 mem
			psHu32(SBUS_F220) &= ~data;
		return;

		case 0x30:			// bits set when written from IOP
			// write to ps2 mem
			psHu32(SBUS_F230) |= data;
		return;

		case 0x40:			// Control Register
		{
			u32 temp = iopaddr & 0xF0;

			// write to ps2 mem
			if(iopaddr & 0x20 || iopaddr & 0x80)
			{
				psHu32(SBUS_F240) &= ~0xF000;
				psHu32(SBUS_F240) |= 0x2000;
			}

			if(psHu32(SBUS_F240) & temp)
				psHu32(SBUS_F240) &= ~temp;
			else
				psHu32(SBUS_F240) |= temp;
		}
		return;

		case 0x60:
			psHu32(SBUS_F260) = 0;
		return;

		default:
			// Chances are writes past 0x60 are a BUS ERROR.  Needs testing.
		break;
	}
	psxSu32(iopaddr) = data; 
}

}