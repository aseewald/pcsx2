
#include "PrecompiledHeader.h"

#include "Memory.h"
#include "IopCommon.h"

extern void _psxMemReset();

namespace IopMemory
{
// standard constructor initializes the static IOP translation table.
void TranslationTable::Initialize()
{
	// Clear all memory to the default handlers:
	AssignHandler( 0, HandlerId_Unmapped, 0x20000000 );
	
	//AssignHandler( 0, HandlerId_LegacyAPI, 0x20000000 );
	//return;
	
	// Mirror the 2MB of IOP memory four times consecutively, filling up the lower
	// 8mb with maps:
	for( int i=0; i<4; ++i )
		AssignLookup( Ps2MemSize::IopRam*i, iopMem->Ram, Ps2MemSize::IopRam );

	AssignLookup ( 0x1fc00000, PS2MEM_ROM,  Ps2MemSize::Rom );
	AssignLookup ( 0x1e000000, PS2MEM_ROM1, Ps2MemSize::Rom1 );

	// SIF registers at 0x1d00 -- should this be mapped across 4kb or 64kb?
	// (currently mapping across only 4kb)
	AssignHandler( 0x1d000000, HandlerId_SIFregs );

	// Hardware Registers, mapped to psxH, with three handlers for special register areas
	// at 0x1f801, 0x1f803, and 0x1f808 pages.
	AssignLookup ( 0x1f800000, iopMem->Hardware, PageSize*0x10 );

	AssignHandler( 0x1f801000, HandlerId_Hardware_Page1 );
	AssignHandler( 0x1f803000, HandlerId_Hardware_Page3 );
	AssignHandler( 0x1f808000, HandlerId_Hardware_Page8 );

	// Special CDVD Registers (single page, handler only)
	//AssignHandler( 0x1f400000, HandlerId_Hardware4, PageSize*0x10 );
	AssignHandler( 0x1f402000, HandlerId_Hardware4 );

	AssignHandler( 0x10000000, HandlerId_Dev9 );
	AssignHandler( 0x1f900000, HandlerId_SPU2 );
}
	
void TranslationTable::AssignLookup( uint startaddr, u8* dest, uint bytesize )
{
	const uint startpage = startaddr / PageSize;
	const uint endpage = (startaddr + bytesize + (PageSize-1)) / PageSize;	// rounded up.

	for( uint i=startpage; i<endpage; ++i, dest+=PageSize )
	{
		jASSUME( i < PageCount );		// because I'm fallible.
		Contents[i] = (sptr)dest;
	}
}

void TranslationTable::AssignHandler( uint startaddr, HandlerIdentifier handidx, uint bytesize )
{
	const uint startpage = startaddr / PageSize;
	const uint endpage = (startaddr + bytesize + (PageSize-1)) / PageSize;	// rounded up.
	for( uint i=startpage; i<endpage; ++i )
	{
		jASSUME( i < PageCount );		// really really fallible
		Contents[i] = handidx;
	}
}

PCSX2_ALIGNED( 64, TranslationTable tbl_Translation );		// 512k table via 4k pages


/////////////////////////////////////////////////////////////////////////////////////////////
// Unmapped address handlers.
// It's known for fact that the IOP fails silently on unmapped writes.  What's not known is
// what it's supposed to do for unmapped reads.  Supposed or not, what works is to simply
// return a big fat zero (yay!)
//
template< typename T >
static T __fastcall UnmappedRead( u32 iopaddr ) { return 0; }		// it's big

template< typename T >
static void __fastcall UnmappedWrite( u32 iopaddr, T writeval )
{
	// fails silently
}

//////////////////////////////////////////////////////////////////////////////////////////
// Patch-throughs for plugin API calls.  We can't reference the plugins directly for
// two reasons:
//   a) they're dynamic pointers, and our table is const (for now).
//   b) we need the fastcall API for our memory operations.
//
// I may change this around some in the future if we redesign an API utilizing __fastcall.
//
static u8 __fastcall _dev9_Read8( u32 addr )	{ return DEV9read8( addr ); }
static u16 __fastcall _dev9_Read16( u32 addr )	{ return DEV9read16( addr ); }
static u32 __fastcall _dev9_Read32( u32 addr )	{ return DEV9read32( addr ); }

static void __fastcall _dev9_Write8( u32 addr, mem8_t val )		{ DEV9write8( addr, val ); }
static void __fastcall _dev9_Write16( u32 addr, mem16_t val )	{ DEV9write16( addr, val ); }
static void __fastcall _dev9_Write32( u32 addr, mem32_t val )	{ DEV9write32( addr, val ); }

static mem16_t __fastcall _spu2_read16( u32 addr )				{ return SPU2read( addr ); }
static void __fastcall _spu2_write16( u32 addr, mem16_t val )	{ SPU2write( addr, val ); }

FnType_Read8* const uRead8  = UnmappedRead<mem8_t>;
FnType_Read16* const uRead16 = UnmappedRead<mem16_t>;
FnType_Read32* const uRead32 = UnmappedRead<mem32_t>;

FnType_Write8* const uWrite8  = UnmappedWrite<mem8_t>;
FnType_Write16* const uWrite16 = UnmappedWrite<mem16_t>;
FnType_Write32* const uWrite32 = UnmappedWrite<mem32_t>;

/////////////////////////////////////////////////////////////////////////////////////////////
// Indirect handlers table (constant!)
// The handlers need to be initialized in the following manner to allow for efficient indexing
// from the recompilers.  Most handlers macro out nicely, but the iopHw and spu2 handlers
// have unmapped bit widths so they can't macro.
// 
#define DEF_INDIRECT_SECTION( mode, bits ) \
	u##mode##bits, \
	Sif##mode##bits, \
	iopHw##mode##bits##_Page1, \
	iopHw##mode##bits##_Page3, \
	iopHw##mode##bits##_Page8, \
	_dev9_##mode##bits
	//_iopMem##mode##bits

PCSX2_ALIGNED( 64, void* const tbl_IndirectHandlers[2][3][ HandlerId_Maximum ] ) =
{
	{
		{ DEF_INDIRECT_SECTION( Read, 8 ),	iopHw4Read8,	uRead8			},
		{ DEF_INDIRECT_SECTION( Read, 16 ),	uRead16,		_spu2_read16	},
		{ DEF_INDIRECT_SECTION( Read, 32 ),	uRead32,		uRead32			}
	},

	{
		{ DEF_INDIRECT_SECTION( Write, 8 ),	iopHw4Write8,	uWrite8			},
		{ DEF_INDIRECT_SECTION( Write, 16 ),uWrite16,		_spu2_write16	},
		{ DEF_INDIRECT_SECTION( Write, 32 ),uWrite32,		uWrite32		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////
//

// ------------------------------------------------------------------------
// Fast 'shortcut' memory access for when the source address is known to be
// a *direct* mapping.  (excludes hardware registers and such).
//
template< typename T > __releaseinline
T __fastcall DirectReadType( u32 iopaddr )
{
	const uptr masked = iopaddr & AddressMask;
	const sptr tab = tbl_Translation.Contents[masked/PageSize];

	jASSUME( tab > HandlerId_Maximum );
	return *(T*)(tab+(masked & PageMask));
}

// ------------------------------------------------------------------------
template< typename T > __releaseinline
T __fastcall ReadType( u32 iopaddr )
{
	const uptr masked = iopaddr & AddressMask;
	const sptr tab = tbl_Translation.Contents[masked/PageSize];
	
	if( tab > HandlerId_Maximum )
	{
		sptr inpage_addr = masked & PageMask;
		return *(T*)(tab+inpage_addr);
	}
	else
	{
		switch( sizeof( T ) )
		{
			case 1: return ((FnType_Read8*)tbl_IndirectHandlers[0][0][tab])( iopaddr );
			case 2: return ((FnType_Read16*)tbl_IndirectHandlers[0][1][tab])( iopaddr );
			case 4: return ((FnType_Read32*)tbl_IndirectHandlers[0][2][tab])( iopaddr );
			jNO_DEFAULT;
		}
	}
}

// ------------------------------------------------------------------------
template< typename T > __releaseinline
void __fastcall WriteType( u32 iopaddr, T writeval )
{
	const uptr masked = iopaddr & AddressMask;
	const sptr tab = tbl_Translation.Contents[masked/PageSize];

	if( tab > HandlerId_Maximum )
	{
		if( !(iopRegs.CP0.n.Status & 0x10000) )
		{
			sptr inpage_addr = masked & PageMask;
			*((T*)(tab+inpage_addr)) = writeval;
		}
	}
	else
	{
		switch( sizeof( T ) )
		{
			case 1: ((FnType_Write8*)tbl_IndirectHandlers[1][0][tab])( iopaddr, writeval );  break;
			case 2: ((FnType_Write16*)tbl_IndirectHandlers[1][1][tab])( iopaddr, writeval ); break;
			case 4: ((FnType_Write32*)tbl_IndirectHandlers[1][2][tab])( iopaddr, writeval ); break;
			jNO_DEFAULT;
		}
	}
}

}


using namespace IopMemory;

__forceinline u8   iopMemRead8 (u32 mem) { return ReadType<mem8_t>( mem ); }
__forceinline u16  iopMemRead16(u32 mem) { return ReadType<mem16_t>( mem ); }
__forceinline u32  iopMemRead32(u32 mem) { return ReadType<mem32_t>( mem ); }

__forceinline u8   iopMemDirectRead8 (u32 mem) { return DirectReadType<mem8_t>( mem ); }
__forceinline u16  iopMemDirectRead16(u32 mem) { return DirectReadType<mem16_t>( mem ); }
__forceinline u32  iopMemDirectRead32(u32 mem) { return DirectReadType<mem32_t>( mem ); }

__forceinline void iopMemWrite8 (u32 mem, mem8_t value) { WriteType<mem8_t>( mem, value ); }
__forceinline void iopMemWrite16(u32 mem, mem16_t value) { WriteType<mem16_t>( mem, value ); }
__forceinline void iopMemWrite32(u32 mem, mem32_t value) { WriteType<mem32_t>( mem, value ); }

IopMemoryAlloc* iopMem = NULL;

void psxMemAlloc()
{
	if( iopMem == NULL )
		iopMem = (IopMemoryAlloc*)vtlb_malloc( sizeof( IopMemoryAlloc ), 4096 );
}

void psxMemShutdown()
{
	vtlb_free( iopMem, sizeof( IopMemoryAlloc ) );
}

void psxMemReset()
{
	jASSUME( iopMem != NULL );
	DbgCon::Status( "Resetting IOP Memory..." );

	memzero_ptr<sizeof(IopMemoryAlloc)>( iopMem );
	tbl_Translation.Initialize();
	recInitialize();
}
