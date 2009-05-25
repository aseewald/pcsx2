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

#include "Memory.h"
#include "IopCommon.h"

namespace IopMemory
{

//////////////////////////////////////////////////////////////////////////////////////////
// BIU/Cache config register [0xfffe0130]
// "Where's it goin' to?  And what does it think it's gonna do when it gets there?" - air
//
// ** RES    : 31 -> 18 : Reserved
// ** RES    : 17       : Reserved (should be 1 for normal operation)
// ** RES    : 16       : Reserved (should be 0 for normal operation)
// ** BGNT   : 15       : Enables Bus Grant
// ** NOPAD  : 14       : No padding of waitstates between transactions
// ** RDPRI  : 13       : Loads have priority over stores
// ** INTP   : 12       : Interrupts are active high
// ** IS1    : 11       : Enable I-Cache (certain values me cause bus error?)
// ** IS0    : 10       : Hardwired to zero
// ** IBLKSZ :  9 ->  8 : I-Cache refill size = 8 words
// ** DS     :  7       : Enable D-Cache / Scratchpad
// ** RES    :  6       : Hardwired to zero
// ** DBLKSZ :  5 ->  4 : D-Cache refill block size 8 words
// ** RAM    :  3       : No Scratchpad RAM (when disabled, accesses to scratchpad cause Bus Error)
// ** TAG    :  2       : Disable tag test
// ** INV    :  1       : Disable invalidate mode
// ** LOCK   :  0       : Disable cache lock
//
// Moral of the story: We can just ignore the dumb thing and all games still run, but
// where is the fun in that?!
//


static const uint BIU_BusGrant_Enable		= 1 << 15;	// still not sure what this does on the IOP
static const uint BIU_IntsActiveHigh		= 1 << 12;	// dunno if any point in supporting this..
static const uint BIU_ICache_Enable			= 1 << 11;
static const uint BIU_Scratchpad_Enable		= 1 << 7;	// when disabled reads/writes to scratch are ignored
static const uint BIU_Scratchpad_Mapped		= 1 << 3;	// when disabled accesses to scratch cause Bus Error


//////////////////////////////////////////////////////////////////////////////////////////
//
// standard constructor initializes the static IOP translation table.
void TranslationTable::Initialize()
{
	// Clear all memory to the default handlers:
	AssignHandler( 0, HandlerId_Unmapped, 0x20000000 );

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
	// at 0x1f801, 0x1f803, and 0x1f808 pages.  Note: Page 0 (0 -> 0x1000) is the IOP Scratchpad.
	AssignLookup ( 0x1f800000, iopMem->Hardware, PageSize*0x10 );

	AssignHandler( 0x1f801000, HandlerId_Hardware_Page1 );
	AssignHandler( 0x1f803000, HandlerId_Hardware_Page3 );
	AssignHandler( 0x1f808000, HandlerId_Hardware_Page8 );

	// Special CDVD Registers (single page, handler only)
	//AssignHandler( 0x1f400000, HandlerId_Hardware4, PageSize*0x10 );
	AssignHandler( 0x1f402000, HandlerId_Hardware4 );

	AssignHandler( 0x10000000, HandlerId_Dev9 );
	AssignHandler( 0x1f900000, HandlerId_SPU2 );
	
	AssignHandler( 0x1ffe0000, HandlerId_BIUCtrl );
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

void TranslationTable::RemapScratchpad( u32 newStatus )
{
	// Scratchpad Note: Chances are the Mapped status of the scratchpad takes precedence
	// over Enable.  That is, if the Scratchpad is unmapped, and attempts to access it regardless
	// of the Enable bit will cause a Bus Error.

	if( newStatus & BIU_Scratchpad_Mapped )
	{
		if( newStatus & BIU_Scratchpad_Enable )
			AssignLookup ( 0x1f800000, iopMem->Hardware, PageSize );
		else
			AssignHandler( 0x1f800000, HandlerId_Null );
	}
	else
	{
		AssignHandler( 0x1f800000, HandlerId_Unmapped );	
	}
}


PCSX2_ALIGNED( 64, TranslationTable tbl_Translation );		// 512k table via 4k pages


/////////////////////////////////////////////////////////////////////////////////////////////
// Unmapped address handlers -- Unmapped reads and writes invoke a BusError exception (tested).
// Null Address handlers -- Used to ignore memory write operations to certain pages (reads
//   will still generate Buss Error).
//
template< typename T >
static T __fastcall UnmappedRead( u32 iopaddr )
{
	throw R3000Exception::BusError( iopaddr, false );
}

template< typename T >
static void __fastcall UnmappedWrite( u32 iopaddr, T writeval )
{
	throw R3000Exception::BusError( iopaddr, true );
}

template< typename T >
static T __fastcall NullRead( u32 iopaddr )
{
	throw R3000Exception::BusError( iopaddr, false );
}

template< typename T >			// Do nothing!
static void __fastcall NullWrite( u32 iopaddr, T writeval ) { }

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

//////////////////////////////////////////////////////////////////////////////////////////
// BIU / Cache Control Handlers
//
// This page also includes two Mysterious Regs: 0xfffe0140, and 0xfffe0144.
// These are written during system startup once, and generally never touched again.
// They are assigned values that map to the IOP's scratchpad.  Who knows.  Almost
// assuredly not important for emulation purposes so we ignore it.  Documented in
// code for documentation purposes only.
//

template< typename T >
static T __fastcall _BIUctrl_Read( u32 addr )
{
	const bool is16bit = (sizeof(T) == 2);

	// registers below 0x20 don't BusError:
	if( addr < 0xfffe0020 ) return 0;

	if( addr >= 0xfffe0100 )
	{
		if( addr == 0xfffe0130 )
			return ((T*)&iopRegs.BIU_Cache_Ctrl)[0];
		if( is16bit && addr == 0xfffe0132 )
			return ((T*)&iopRegs.BIU_Cache_Ctrl)[1];

		if( addr == 0xfffe0140 )
			return ((T*)&iopRegs.MysteryRegAt_FFFE0140)[0];
		if( is16bit && addr == 0xfffe0142 )
			return ((T*)&iopRegs.MysteryRegAt_FFFE0140)[1];

		if( addr == 0xfffe0144 )
			return ((T*)&iopRegs.MysteryRegAt_FFFE0144)[0];
		if( is16bit && addr == 0xfffe0146 )
			return ((T*)&iopRegs.MysteryRegAt_FFFE0144)[1];

		// unhandled registers from 0x100 thru 0x160 don't bus error.
		if( addr < 0xfffe0160 ) return 0;
	}

	// Unhandled above?  Bus Error time.
	return UnmappedRead<T>( addr );
}

static void __fastcall _BIUctrl_Write32( u32 addr, mem32_t val )
{
	// Writes to registers below 0x20 don't BusError:
	if( addr < 0xfffe0020 ) return;

	if( addr >= 0xfffe0100 )
	{
		if( addr == 0xfffe0130 )
		{
			val &= ~((1<<6) | (1<<10));			// bits 6 and 10 are hardwired to zero.

			// Remap the scratchpad if the bits are changed
			uint scratchbits = BIU_Scratchpad_Mapped | BIU_Scratchpad_Enable;
			if( (val & scratchbits) != (iopRegs.BIU_Cache_Ctrl & scratchbits) )
			{
				tbl_Translation.RemapScratchpad( val );

				// [TODO]: Issue a recompiler reset.
				//  (should be implemented via a C++ exception)
			}

			iopRegs.BIU_Cache_Ctrl = val;

			//Console::Notice( "IopMemory: Mysterious write to Mysterious Reg @ 0x%08x: val=%0x08x (pc=0x%08x)",
			//	params addr, val, iopRegs.pc );
		}
		else if( addr == 0xfffe0140 )
		{
			iopRegs.MysteryRegAt_FFFE0140 = val;
		}
		else if( addr == 0xfffe0144 )
		{
			iopRegs.MysteryRegAt_FFFE0144 = val;
		}

		// unhandled registers from 0x100 thru 0x160 don't bus error.
		if( addr < 0xfffe0160 ) return;
	}

	// anything not handled above should bus error here:
	UnmappedWrite<mem32_t>( addr, val );
}

// ------------------------------------------------------------------------
// HACK: Template failure on MSVC 2008 requires I do this, because the compiler fails
// to properly handle the use of templated functions in array-style initializers of
// global variables. [it generates type mismatch errors in spite of there clearly not
// being one].  Obscure.

static FnType_Read8*	const uRead8		= UnmappedRead<mem8_t>;
static FnType_Read16*	const uRead16		= UnmappedRead<mem16_t>;
static FnType_Read32*	const uRead32		= UnmappedRead<mem32_t>;
static FnType_Write8*	const uWrite8		= UnmappedWrite<mem8_t>;
static FnType_Write16*	const uWrite16		= UnmappedWrite<mem16_t>;
static FnType_Write32*	const uWrite32		= UnmappedWrite<mem32_t>;

static FnType_Read8*	const nullRead8		= NullRead<mem8_t>;
static FnType_Read16*	const nullRead16	= NullRead<mem16_t>;
static FnType_Read32*	const nullRead32	= NullRead<mem32_t>;
static FnType_Write8*	const nullWrite8	= NullWrite<mem8_t>;
static FnType_Write16*	const nullWrite16	= NullWrite<mem16_t>;
static FnType_Write32*	const nullWrite32	= NullWrite<mem32_t>;

static FnType_Read8*	const biuRead8		= _BIUctrl_Read<mem8_t>;
static FnType_Read16*	const biuRead16		= _BIUctrl_Read<mem16_t>;
static FnType_Read32*	const biuRead32		= _BIUctrl_Read<mem32_t>;

/////////////////////////////////////////////////////////////////////////////////////////////
// Indirect handlers table
// The handlers need to be initialized in the following manner to allow for efficient indexing
// from the recompilers.  Most handlers macro out nicely, but the iopHw and spu2 handlers
// have unmapped bit widths so they can't macro.
// 
#define DEF_INDIRECT_SECTION( mode, bits ) \
	u##mode##bits, \
	null##mode##bits, \
	Sif##mode##bits, \
	iopHw##mode##bits##_Page1, \
	iopHw##mode##bits##_Page3, \
	iopHw##mode##bits##_Page8, \
	_dev9_##mode##bits
//_iopMem##mode##bits

PCSX2_ALIGNED( 64, const void* const tbl_IndirectHandlers[2][3][ HandlerId_Maximum ] ) =
{
	{
		{ DEF_INDIRECT_SECTION( Read, 8 ),	iopHw4Read8,	uRead8,			biuRead8	},
		{ DEF_INDIRECT_SECTION( Read, 16 ),	uRead16,		_spu2_read16,	biuRead16	},
		{ DEF_INDIRECT_SECTION( Read, 32 ),	uRead32,		uRead32,		biuRead32	}
	},

	{
		{ DEF_INDIRECT_SECTION( Write, 8 ),	iopHw4Write8,	uWrite8,		uWrite8		},
		{ DEF_INDIRECT_SECTION( Write, 16 ),uWrite16,		_spu2_write16,	uWrite16	},
		{ DEF_INDIRECT_SECTION( Write, 32 ),uWrite32,		uWrite32,		_BIUctrl_Write32 }
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////
//

__releaseinline u8* __fastcall iopGetPhysPtr( u32 iopaddr )
{
	const uptr masked = iopaddr & AddressMask;
	const sptr tab = tbl_Translation.Contents[masked/PageSize];

	jASSUME( tab > HandlerId_Maximum );
	return (u8*)tab + (masked & PageMask);
}

// ------------------------------------------------------------------------
// Fast 'shortcut' memory access for when the source address is known to be
// a *direct* mapping.  (excludes hardware registers and such).
//
template< typename T > __forceinline
T __fastcall DirectReadType( u32 iopaddr )
{
	return *(T*)iopGetPhysPtr( iopaddr );
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

//////////////////////////////////////////////////////////////////////////////////////////
// Begin External / Public API

using namespace IopMemory;

__forceinline bool IsIopRamPage( u32 iopaddr )
{
	// anything under the 8 MB line maps to IOP ram:
	return iopaddr < 0x800000;
}

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
	iopInitRecMem();
}
