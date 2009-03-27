
#include "PrecompiledHeader.h"

namespace IopMemory
{

#ifdef PAGESIZE_16bit
static const uint PageBitShift = 16;		// 64k pages
static const uint PageSize = 1<<PageBitShift;
static const uint PageCount = 0x20000000 / PageSize;
static const uint AddressMask = 0x1fffffff;

enum HandlerIdentifier
{
	HandlerId_Unmapped = 0,
	HandlerId_SIFregs,
	HandlerId_Hardware,
	HandlerId_Hardware4,
	HandlerId_Maximum = 8,
};

#define XLATE_UNMAPPED_15 \
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, \
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, \
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, \
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped
	
#define XLATE_UNMAPPED_16 \
	XLATE_UNMAPPED_15, HandlerId_Unmapped

#define XLATE_UNMAPPED_32 \
	XLATE_UNMAPPED_16, XLATE_UNMAPPED_16

#define XLATE_UNMAPPED_128 \
	XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32

#define	XLATE_UNMAPPED_255 \
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, \
	XLATE_UNMAPPED_16, XLATE_UNMAPPED_15	

	
#define XLATE_UNMAPPED_1024 \
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, \
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128

#define XLATE_ROM( baseaddr ) \
	PS2MEM_ROM[(baseaddr<<16)+0x0000], PS2MEM_ROM[(baseaddr<<16)+0x1000], PS2MEM_ROM[(baseaddr<<16)+0x2000], PS2MEM_ROM[(baseaddr<<16)+0x3000], \
	PS2MEM_ROM[(baseaddr<<16)+0x4000], PS2MEM_ROM[(baseaddr<<16)+0x5000], PS2MEM_ROM[(baseaddr<<16)+0x6000], PS2MEM_ROM[(baseaddr<<16)+0x7000], \
	PS2MEM_ROM[(baseaddr<<16)+0x8000], PS2MEM_ROM[(baseaddr<<16)+0x9000], PS2MEM_ROM[(baseaddr<<16)+0xa000], PS2MEM_ROM[(baseaddr<<16)+0xb000], \
	PS2MEM_ROM[(baseaddr<<16)+0xc000], PS2MEM_ROM[(baseaddr<<16)+0xd000], PS2MEM_ROM[(baseaddr<<16)+0xe000], PS2MEM_ROM[(baseaddr<<16)+0xf000]

/////////////////////////////////////////////////////////////////////////////////////////////
// translated address corresponding to each IOP memory page
// If the value is 0-63, it means that it's a LUT index for the indirect handler.
// All other values correspond to real addresses in PC memory.
static const uptr TranslationTable[PageCount] =		// 32k table
{
	// first meg of main memory
	&psxM[0x0000], &psxM[0x1000], &psxM[0x2000], &psxM[0x3000],
	&psxM[0x4000], &psxM[0x5000], &psxM[0x6000], &psxM[0x7000],
	&psxM[0x8000], &psxM[0x9000], &psxM[0xa000], &psxM[0xb000],
	&psxM[0xc000], &psxM[0xd000], &psxM[0xe000], &psxM[0xf000],

	// second meg of main memory
	&psxM[0x10000], &psxM[0x11000], &psxM[0x12000], &psxM[0x13000],
	&psxM[0x14000], &psxM[0x15000], &psxM[0x16000], &psxM[0x17000],
	&psxM[0x18000], &psxM[0x19000], &psxM[0x1a000], &psxM[0x1b000],
	&psxM[0x1c000], &psxM[0x1d000], &psxM[0x1e000], &psxM[0x1f000],

	// 0x20 -> 1d00 are "unmapped" [ route to the NULL handler ]

	XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32,	// 128
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, // 512 
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, XLATE_UNMAPPED_128, // 1024 / 0x400

	XLATE_UNMAPPED_1024,						// 2048 / 0x800
	XLATE_UNMAPPED_1024, XLATE_UNMAPPED_1024,	// 0x1000
	XLATE_UNMAPPED_1024, XLATE_UNMAPPED_1024,	// 0x1800
	XLATE_UNMAPPED_1024,						// 0x1c00
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_128,		// 0x1d00

	// 1d00 - SIF Regs
	HandlerId_SIFregs,
	XLATE_UNMAPPED_255,

	// 1e00 - ROM1
	PS2MEM_ROM1[0x0000], PS2MEM_ROM1[0x1000], PS2MEM_ROM1[0x2000], PS2MEM_ROM1[0x3000],
	XLATE_UNMAPPED_128, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32, XLATE_UNMAPPED_32,
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped,
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped,
	HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped, HandlerId_Unmapped,
	
	// 1f80 - Hardware Regs
	HandlerId_Hardware,
	XLATE_UNMAPPED_255,
	
	// 1fc0 - ROM
	XLATE_ROM( 0 ), XLATE_ROM( 1 ), XLATE_ROM( 2 ), XLATE_ROM( 3 )	
};
#else

static const uint PageBitShift = 12;		// 4k pages
static const uint PageSize = 1<<PageBitShift;
static const uint PageCount = 0x20000000 / PageSize;
static const uint AddressMask = 0x1fffffff;

enum HandlerIdentifier
{
	HandlerId_Unmapped = 0,
	HandlerId_SIFregs,
	HandlerId_Hardware_Page1,
	HandlerId_Hardware_Page8,
	HandlerId_Hardware4,
	HandlerId_Maximum = 8,
};

#endif

typedef u8 __fastcall FnType_Read8( u32 iopaddr );
typedef u16 __fastcall FnType_Read16( u32 iopaddr );
typedef u32 __fastcall FnType_Read32( u32 iopaddr );

typedef void __fastcall FnType_Write8( u32 iopaddr, u8 data );
typedef void __fastcall FnType_Write16( u32 iopaddr, u16 data );
typedef void __fastcall FnType_Write32( u32 iopaddr, u32 data );

struct IndirectHandler
{
	FnType_Read8* HandleRead8;
	FnType_Read16* HandleRead16;
	FnType_Read32* HandleRead32;
	
	FnType_Write8* HandleWrite8;
	FnType_Write16* HandleWrite16;
	FnType_Write32* HandleWrite32;
};

/////////////////////////////////////////////////////////////////////////////////////////////
// translated address corresponding to each IOP memory page
// If the value is 0-63, it means that it's a LUT index for the indirect handler.
// All other values correspond to real addresses in PC memory.
//
struct TranslationTable
{
	uptr Contents[PageCount];
	
	// standard constructor initializes the static IOP translation table.
	TranslationTable()
	{
		// IOP has 2MB of main memory:
		static const uint pages_mainmem	= 0x20000000 / PageSize;
		static const uint pages_rom		= Ps2MemSize::Rom / PageSize;
		static const uint pages_rom1	= Ps2MemSize::Rom1 / PageSize;
		
				
		// Mirror the 2MB of IOP memory four times consecutively, filling up the lower
		// 8mb with maps:
		for( int i=0; i<pages_mainmem*4; ++i )
			Contents[i] = &psxM[(i&(pages_mainmem-1))*PageSize];
			
		// Rom Mappings (pretty straight-forward)
		for( int i=0; i<pages_rom; ++i )
			Contents[0x1fc00] = &PS2MEM_ROM[i*PageSize];

		for( int i=0; i<pages_rom1; ++i )
			Contents[0x1e000+i] = &PS2MEM_ROM1[i*PageSize];

		// SIF registers at 0x1d00 -- should this be mapped across 4kb or 64kb?
		// (currently mapping across only 4kb)
		Contents[0x1d000] = HandlerId_SIFregs;

		// Hardware Registers, mapped to psxH, with two handlers for special register areas
		// at 0x1f801 and 0x1f808 pages.
		for( int i=0; i<0x10; ++i )
			Contents[0x1f800+i] = psxH[i*PageSize];

		Contents[0x1f801] = HandlerId_Hardware_Page1;
		Contents[0x1f808] = HandlerId_Hardware_Page8;

		// Special CDVD Registers (single page, handler only)
		Contents[0x1f402] = HandlerId_Hardware4;
	}
};

static const TranslationTable tbl_Translation;		// 512k table

/////////////////////////////////////////////////////////////////////////////////////////////
// Unmapped address handlers.
// It's known for fact that the IOP fails silently on unmapped writes.  What's not known is
// what it's supposed to do for unmapped reads.  Supposed or not, what works is to simply
// return a big fat zero (yay!)
//
template< typename T >
T __fastcall UnmappedRead( u32 iopaddr )
{
	return 0;
}

template< typename T >
T __fastcall UnmappedWrite( u32 iopaddr, T writeval )
{
	// fails silently
}

/////////////////////////////////////////////////////////////////////////////////////////////
//

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
			ret = psxSu16( mem );
		break;
	}
	SIF_LOG("SIFreg Read addr=0x%x value=0x%x\n", iopaddr, ret);
	return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////
//
static const IndirectHandler tbl_IndirectHandlers[ HandlerId_Maximum ] =
{
	{
		UnmappedRead<8>,  UnmappedRead<16>,  UnmappedRead<32>,
		UnmappedWrite<8>, UnmappedWrite<16>, UnmappedWrite<32>
	},

	{
		SifRead8,  SifRead16,  SifRead32,
		SifWrite8, SifWrite16, SifWrite32
	},

	{
		HwRead8_Page1,  HwRead16_Page1,  HwRead32_Page1,
		HwWrite8_Page1, HwWrite16_Page1, HwWrite32_Page1
	}

	{
		HwRead8_Page8,  HwRead16_Page8,  HwRead32_Page8,
		HwWrite8_Page8, HwWrite16_Page8, HwWrite32_Page8
	}

};

/////////////////////////////////////////////////////////////////////////////////////////////
//

u8 __fastcall Read8( u32 iopaddr )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	return ( tab >= HandlerId_Maximum ) ?
		*(u8*)tab : tbl_IndirectHandlers[tab].HandleRead8( iopaddr );
}

u8 __fastcall Read16( u32 iopaddr )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	return ( tab >= HandlerId_Maximum ) ?
		*(u16*)tab : tbl_IndirectHandlers[tab].HandleRead16( iopaddr );
}

u32 __fastcall Read32( u32 iopaddr )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	return ( tab >= HandlerId_Maximum ) ?
		*(u32*)tab : tbl_IndirectHandlers[tab].HandleRead32( iopaddr );
}

/////////////////////////////////////////////////////////////////////////////////////////////
//

void __fastcall Write8( u32 iopaddr, u8 writeval )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	if( tab >= HandlerId_Maximum )
		*(u8*)tab = writeval;
	else
		tbl_IndirectHandlers[tab].HandleWrite8( iopaddr, writeval );
}

void __fastcall Write16( u32 iopaddr, u16 writeval )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	if( tab >= HandlerId_Maximum )
		*(u16*)tab = writeval;
	else
		tbl_IndirectHandlers[tab].HandleWrite16( iopaddr, writeval );
}

void __fastcall Write32( u32 iopaddr, u32 writeval )
{
	iopaddr &= 0x1fffffff;
	uptr tab = tbl_Translation.Contents[iopaddr>>16];

	if( tab >= HandlerId_Maximum )
		*(u32*)tab = writeval;
	else
		tbl_IndirectHandlers[tab].HandleWrite32( iopaddr, writeval );
}

}