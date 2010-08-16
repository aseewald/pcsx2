/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace Ps2MemSize
{
	static const uint Base	= 0x02000000;		// 32 MB main memory!
	static const uint Rom	= 0x00400000;		// 4 MB main rom
	static const uint Rom1	= 0x00040000;		// DVD player
	static const uint Rom2	= 0x00080000;		// Chinese rom extension (?)
	static const uint ERom	= 0x001C0000;		// DVD player extensions (?)
	static const uint Hardware = 0x00010000;
	static const uint Scratch = 0x00004000;

	static const uint IopRam = 0x00200000;		// 2MB main ram on the IOP.
	static const uint IopHardware = 0x00010000;

	static const uint GSregs = 0x00002000;		// 8k for the GS registers and stuff.
}

typedef u8 mem8_t;
typedef u16 mem16_t;
typedef u32 mem32_t;
typedef u64 mem64_t;
typedef u128 mem128_t;

struct EEVM_MemoryAllocMess
{
	u8 Scratch[Ps2MemSize::Scratch];		// Scratchpad!
	u8 Main[Ps2MemSize::Base];				// Main memory (hard-wired to 32MB)
	u8 HW[Ps2MemSize::Hardware];			// Hardware registers
	u8 ROM[Ps2MemSize::Rom];				// Boot rom (4MB)
	u8 ROM1[Ps2MemSize::Rom1];				// DVD player
	u8 ROM2[Ps2MemSize::Rom2];				// Chinese extensions (?)
	u8 EROM[Ps2MemSize::ERom];				// DVD player extensions (?)

	// Two 1 megabyte (max DMA) buffers for reading and writing to high memory (>32MB).
	// Such accesses are not documented as causing bus errors but as the memory does
	// not exist, reads should continue to return 0 and writes should be discarded.
	// Probably.

	u8 ZeroRead[_1mb];
	u8 ZeroWrite[_1mb];
};

extern EEVM_MemoryAllocMess* eeMem;
