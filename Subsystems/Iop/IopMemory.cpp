
#include "PrecompiledHeader.h"

#include "Memory.h"
#include "IopMem.h"

namespace IopMemory
{

static const uint PageBitShift = 12;		// 4k pages
static const uint PageSize = 1<<PageBitShift;
static const uint PageCount = 0x20000000 / PageSize;
static const uint AddressMask = 0x1fffffff;

enum HandlerIdentifier
{
	HandlerId_Unmapped = 0,
	HandlerId_SIFregs,
	HandlerId_Hardware_Page1,
	HandlerId_Hardware_Page3,
	HandlerId_Hardware_Page8,
	HandlerId_Hardware4,
	HandlerId_Maximum = 8,
};


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
// TranslationTable - contains translated addresses corresponding to each IOP memory page.
// If the value is 0-63, it means that it's a LUT index for the indirect handler.
// All other values correspond to real addresses in PC memory.
//
class TranslationTable
{
public:
	uptr Contents[PageCount];

public:	
	// standard constructor initializes the static IOP translation table.
	TranslationTable()
	{
		// Mirror the 2MB of IOP memory four times consecutively, filling up the lower
		// 8mb with maps:
		for( int i=0; i<4; ++i )
			AssignLookup( Ps2MemSize::IopRam*i, psxM, Ps2MemSize::IopRam );
			
		AssignLookup( 0x1fc00000, PS2MEM_ROM, Ps2MemSize::Rom );
		AssignLookup( 0x1e000000, PS2MEM_ROM1, Ps2MemSize::Rom1 );

		// SIF registers at 0x1d00 -- should this be mapped across 4kb or 64kb?
		// (currently mapping across only 4kb)
		AssignHandler( 0x1d000000, HandlerId_SIFregs );

		// Hardware Registers, mapped to psxH, with two handlers for special register areas
		// at 0x1f801 and 0x1f808 pages.
		AssignLookup( 0x1f800000, psxH, 0x10 );

		AssignHandler( 0x1f801000, HandlerId_Hardware_Page1 );
		AssignHandler( 0x1f803000, HandlerId_Hardware_Page3 );
		AssignHandler( 0x1f808000, HandlerId_Hardware_Page8 );

		// Special CDVD Registers (single page, handler only)
		AssignHandler( 0x1f402000, HandlerId_Hardware4 );
	}
	
protected:
	void AssignLookup( uint startaddr, u8* dest, uint bytesize=1 )
	{
		const uint startpage = startaddr / PageSize;
		const uint endpage = (startaddr + bytesize + (PageSize-1)) / PageSize;	// rounded up.

		for( uint i=startpage; i<endpage; ++i, dest+=PageSize )
		{
			jASSUME( i < PageCount );		// because I'm fallible.
			Contents[i] = (uptr)dest;
		}
	}

	void AssignHandler( uint startaddr, HandlerIdentifier handidx, uint bytesize=1 )
	{
		const uint startpage = startaddr / PageSize;
		const uint endpage = (startaddr + bytesize + (PageSize-1)) / PageSize;	// rounded up.
		for( uint i=startpage; i<endpage; ++i )
		{
			jASSUME( i < PageCount );		// really really fallible
			Contents[i] = handidx;
		}
	}

};

static const TranslationTable tbl_Translation;		// 512k table via 4k pages

/////////////////////////////////////////////////////////////////////////////////////////////
// Unmapped address handlers.
// It's known for fact that the IOP fails silently on unmapped writes.  What's not known is
// what it's supposed to do for unmapped reads.  Supposed or not, what works is to simply
// return a big fat zero (yay!)
//
u8 __fastcall UnmappedRead8( u32 iopaddr ) { return 0; }		// it's big
u16 __fastcall UnmappedRead16( u32 iopaddr ) { return 0; }	// it's fat
u32 __fastcall UnmappedRead32( u32 iopaddr ) { return 0; }	// it's zero!

template< typename T >
void __fastcall UnmappedWrite( u32 iopaddr, T writeval )
{
	// fails silently
}


/////////////////////////////////////////////////////////////////////////////////////////////
//
static const IndirectHandler tbl_IndirectHandlers[ HandlerId_Maximum ] =
{
	{
		UnmappedRead8,  UnmappedRead16,  UnmappedRead32,
		UnmappedWrite<u8>, UnmappedWrite<u16>, UnmappedWrite<u32>
	},

	{
		SifRead8,  SifRead16,  SifRead32,
		SifWrite8, SifWrite16, SifWrite32
	},

	{
		iopHwRead8_Page1,  iopHwRead16_Page1,  iopHwRead32_Page1,
		iopHwWrite8_Page1, iopHwWrite16_Page1, iopHwWrite32_Page1
	},

	{
		iopHwRead8_Page3,  iopHwRead16_Page3,  iopHwRead32_Page3,
		iopHwWrite8_Page3, iopHwWrite16_Page3, iopHwWrite32_Page3
	},

	{
		iopHwRead8_Page8,  iopHwRead16_Page8,  iopHwRead32_Page8,
		iopHwWrite8_Page8, iopHwWrite16_Page8, iopHwWrite32_Page8
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
