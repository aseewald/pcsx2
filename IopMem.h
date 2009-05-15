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

#pragma once

#include "MemoryTypes.h"

extern u8 *psxM;
extern u8 *psxP;
extern u8 *psxH;
extern u8 *psxS;
extern uptr *psxMemWLUT;
extern const uptr *psxMemRLUT;


// Obtains a read-safe pointer into the IOP's physical memory, with TLB address translation.
// Returns NULL if the address maps to an invalid/unmapped physical address.
//
// Hacky!  This should really never be used, since anything reading through the
// TLB should be using iopMemRead/Write instead for each individual access.  That ensures
// correct handling of page boundary crossings.
template<typename T>
static __forceinline const T* iopVirtMemR( u32 mem )
{
	return (const T*)&psxM[mem & 0x1fffff];
}

// Obtains a pointer to the IOP's physical mapping (bypasses the TLB).
// This function is intended for use by DMA address resolution only.
static __forceinline u8* iopPhysMem( u32 addr )
{
	return &psxM[addr & 0x1fffff];
}

#define psxSs8(mem)		psxS[(mem) & 0xffff]
#define psxSs16(mem)	(*(s16*)&psxS[(mem) & 0x00ff])
#define psxSs32(mem)	(*(s32*)&psxS[(mem) & 0x00ff])
#define psxSu8(mem)		(*(u8*) &psxS[(mem) & 0x00ff])
#define psxSu16(mem)	(*(u16*)&psxS[(mem) & 0x00ff])
#define psxSu32(mem)	(*(u32*)&psxS[(mem) & 0x00ff])

#define psxPs8(mem)		psxP[(mem) & 0xffff]
#define psxPs16(mem)	(*(s16*)&psxP[(mem) & 0xffff])
#define psxPs32(mem)	(*(s32*)&psxP[(mem) & 0xffff])
#define psxPu8(mem)		(*(u8*) &psxP[(mem) & 0xffff])
#define psxPu16(mem)	(*(u16*)&psxP[(mem) & 0xffff])
#define psxPu32(mem)	(*(u32*)&psxP[(mem) & 0xffff])

#define psxHs8(mem)		psxH[(mem) & 0xffff]
#define psxHs16(mem)	(*(s16*)&psxH[(mem) & 0xffff])
#define psxHs32(mem)	(*(s32*)&psxH[(mem) & 0xffff])
#define psxHu8(mem)		(*(u8*) &psxH[(mem) & 0xffff])
#define psxHu16(mem)	(*(u16*)&psxH[(mem) & 0xffff])
#define psxHu32(mem)	(*(u32*)&psxH[(mem) & 0xffff])

extern void psxMemAlloc();
extern void psxMemReset();
extern void psxMemShutdown();

extern u8   iopMemRead8 (u32 mem);
extern u16  iopMemRead16(u32 mem);
extern u32  iopMemRead32(u32 mem);
extern u8   iopMemDirectRead8 (u32 mem);
extern u16  iopMemDirectRead16(u32 mem);
extern u32  iopMemDirectRead32(u32 mem);
extern void iopMemWrite8 (u32 mem, mem8_t value);
extern void iopMemWrite16(u32 mem, mem16_t value);
extern void iopMemWrite32(u32 mem, mem32_t value);


namespace IopMemory
{
	static const uint PageBitShift = 12;		// 4k pages
	static const uint PageSize = 1<<PageBitShift;
	static const uint PageMask = PageSize-1;
	static const uint PageCount = 0x20000000 / PageSize;
	static const uint AddressMask = 0x1fffffff;

	enum HandlerIdentifier
	{
		HandlerId_Unmapped = 0,
		HandlerId_SIFregs,
		HandlerId_Hardware_Page1,
		HandlerId_Hardware_Page3,
		HandlerId_Hardware_Page8,
		HandlerId_Dev9,
		HandlerId_LegacyAPI,

		HandlerId_Hardware4,
		HandlerId_SPU2,

		HandlerId_Maximum,
	};
	
	typedef u8 __fastcall FnType_Read8( u32 iopaddr );
	typedef u16 __fastcall FnType_Read16( u32 iopaddr );
	typedef u32 __fastcall FnType_Read32( u32 iopaddr );

	typedef void __fastcall FnType_Write8( u32 iopaddr, u8 data );
	typedef void __fastcall FnType_Write16( u32 iopaddr, u16 data );
	typedef void __fastcall FnType_Write32( u32 iopaddr, u32 data );

	/////////////////////////////////////////////////////////////////////////////////////////////
	// TranslationTable - contains translated addresses corresponding to each IOP memory page.
	// If the value is 0-63, it means that it's a LUT index for the indirect handler.
	// All other values correspond to real addresses in PC memory.
	//
	class TranslationTable
	{
	public:
		sptr Contents[PageCount];

	public:	
		// Can't initialize here -- the iop's memory buffers haven't been allocated yet.
		TranslationTable() {}

		// standard constructor initializes the static IOP translation table.
		void Initialize();

	protected:
		void AssignLookup( uint startaddr, u8* dest, uint bytesize=1 );
		void AssignHandler( uint startaddr, HandlerIdentifier handidx, uint bytesize=1 );
	};

	PCSX2_ALIGNED_EXTERN( 64, TranslationTable tbl_Translation );		// 512k table via 4k pages
	PCSX2_ALIGNED_EXTERN( 64, void* const tbl_IndirectHandlers[2][3][ HandlerId_Maximum ] );
	
	extern u8 __fastcall Read8( u32 iopaddr );
	extern u16 __fastcall Read16( u32 iopaddr );
	extern u32 __fastcall Read32( u32 iopaddr );
	extern void __fastcall Write8( u32 iopaddr, u8 writeval );
	extern void __fastcall Write16( u32 iopaddr, u16 writeval );
	extern void __fastcall Write32( u32 iopaddr, u32 writeval );

	extern mem8_t __fastcall SifRead8( u32 iopaddr );
	extern mem16_t __fastcall SifRead16( u32 iopaddr );
	extern mem32_t __fastcall SifRead32( u32 iopaddr );

	extern void __fastcall SifWrite8( u32 iopaddr, mem8_t data );
	extern void __fastcall SifWrite16( u32 iopaddr, mem16_t data );
	extern void __fastcall SifWrite32( u32 iopaddr, mem32_t data );

	extern u8 __fastcall iopHw4Read8( u32 iopaddr );
	extern void __fastcall iopHw4Write8( u32 iopaddr, u8 data );
	extern mem8_t __fastcall iopHwRead8_generic( u32 addr );
	extern mem16_t __fastcall iopHwRead16_generic( u32 addr );
	extern mem32_t __fastcall iopHwRead32_generic( u32 addr );
	extern void __fastcall iopHwWrite8_generic( u32 addr, mem8_t val );
	extern void __fastcall iopHwWrite16_generic( u32 addr, mem16_t val );
	extern void __fastcall iopHwWrite32_generic( u32 addr, mem32_t val );


	extern mem8_t __fastcall iopHwRead8_Page1( u32 iopaddr );
	extern mem8_t __fastcall iopHwRead8_Page3( u32 iopaddr );
	extern mem8_t __fastcall iopHwRead8_Page8( u32 iopaddr );
	extern mem16_t __fastcall iopHwRead16_Page1( u32 iopaddr );
	extern mem16_t __fastcall iopHwRead16_Page3( u32 iopaddr );
	extern mem16_t __fastcall iopHwRead16_Page8( u32 iopaddr );
	extern mem32_t __fastcall iopHwRead32_Page1( u32 iopaddr );
	extern mem32_t __fastcall iopHwRead32_Page3( u32 iopaddr );
	extern mem32_t __fastcall iopHwRead32_Page8( u32 iopaddr );

	extern void __fastcall iopHwWrite8_Page1( u32 iopaddr, mem8_t data );
	extern void __fastcall iopHwWrite8_Page3( u32 iopaddr, mem8_t data );
	extern void __fastcall iopHwWrite8_Page8( u32 iopaddr, mem8_t data );
	extern void __fastcall iopHwWrite16_Page1( u32 iopaddr, mem16_t data );
	extern void __fastcall iopHwWrite16_Page3( u32 iopaddr, mem16_t data );
	extern void __fastcall iopHwWrite16_Page8( u32 iopaddr, mem16_t data );
	extern void __fastcall iopHwWrite32_Page1( u32 iopaddr, mem32_t data );
	extern void __fastcall iopHwWrite32_Page3( u32 iopaddr, mem32_t data );
	extern void __fastcall iopHwWrite32_Page8( u32 iopaddr, mem32_t data );

	extern void recInitialize();
}
