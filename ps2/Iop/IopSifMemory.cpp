
#include "PrecompiledHeader.h"
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
			ret = psHu16(0x1000F200);
		break;
		
		case 0x10:
			ret = psHu16(0x1000F210);
		break;
		
		case 0x40:
			ret = psHu16(0x1000F240) | 0x0002;
		break;
		
		case 0x60:
			ret = 0;
		break;

		default:
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
			ret = psHu32(0x1000F200);
		break;

		case 0x10:
			ret = psHu32(0x1000F210);
		break;

		case 0x20:
			ret = psHu32(0x1000F220);
		break;

		case 0x30:	// EE Side
			ret = psHu32(0x1000F230);
		break;

		case 0x40:
			ret = psHu32(0x1000F240) | 0xF0000002;
		break;

		case 0x60:
			ret = 0;
		break;

		default:
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
			psHu16(0x1000F210) = data;
		return;
		
		case 0x40:
		{
			u32 temp = data & 0xF0;
			// write to ps2 mem
			if(data & 0x20 || data & 0x80)
			{
				psHu16(0x1000F240) &= ~0xF000;
				psHu16(0x1000F240) |= 0x2000;
			}

			if(psHu16(0x1000F240) & temp)
				psHu16(0x1000F240) &= ~temp;
			else
				psHu16(0x1000F240) |= temp;
		}
		return;

		case 0x60:
			psHu32(0x1000F260) = 0;
		return;
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
			psHu32(0x1000F210) = data;
		return;

		case 0x20:			// Bits cleared when written from IOP.
			// write to ps2 mem
			psHu32(0x1000F220) &= ~data;
		return;

		case 0x30:			// bits set when written from IOP
			// write to ps2 mem
			psHu32(0x1000F230) |= data;
		return;

		case 0x40:			// Control Register
		{
			u32 temp = iopaddr & 0xF0;

			// write to ps2 mem
			if(iopaddr & 0x20 || iopaddr & 0x80)
			{
				psHu32(0x1000F240) &= ~0xF000;
				psHu32(0x1000F240) |= 0x2000;
			}

			if(psHu32(0x1000F240) & temp)
				psHu32(0x1000F240) &= ~temp;
			else
				psHu32(0x1000F240) |= temp;
		}
		return;

		case 0x60:
			psHu32(0x1000F260) = 0;
		return;
	}
	psxSu32(iopaddr) = data; 

	// write to ps2 mem
	//if( (mem & 0xf0) != 0x60 )
	//	*(u32*)(PS2MEM_HW+0xf200+(mem&0xf0)) = value;
}

}