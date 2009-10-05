/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2009  PCSX2 Dev Team
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

// Micro VU recompiler! - author: cottonvibes(@gmail.com)

#include "PrecompiledHeader.h"
#include "microVU.h"

//------------------------------------------------------------------
// Micro VU - Global Variables
//------------------------------------------------------------------

__aligned16 microVU microVU0;
__aligned16 microVU microVU1;

#define __four(val)		{ val, val, val, val }

const __aligned(32) mVU_Globals mVUglob = 
{
	// absclip / signbit
	// minvals / maxlavs
	__four( 0x7fffffff ), __four( 0x80000000 ),
	__four( 0xff7fffff ), __four( 0x7f7fffff ),

	__four( 0x3f800000 ),		// ONE!
	__four( 0x3f490fdb ),		// PI4!

	// T1 T2 T3 T4
	// T5 T6 T7 T8
	__four( 0x3f7ffff5 ), __four( 0xbeaaa61c ), __four( 0x3e4c40a6 ), __four( 0xbe0e6c63 ),
	__four( 0x3dc577df ), __four( 0xbd6501c4 ), __four( 0x3cb31652 ), __four( 0xbb84d7e7 ),

	// S2 S3 S4 S5
	__four( 0xbe2aaaa4 ), __four( 0x3c08873e ), __four( 0xb94fb21f ), __four( 0x362e9c14 ),

	// E1 E2 E3
	// E4 E5 E6
	__four( 0x3e7fffa8 ), __four( 0x3d0007f4 ), __four( 0x3b29d3ff ),
	__four( 0x3933e553 ), __four( 0x36b63510 ), __four( 0x353961ac ),

	// FTOI_4 / 12 / 15
	// ITOF_4 / 12 / 15
	__four( 16.0 ),		__four( 4096.0 ),			__four( 32768.0 ),
	__four( 0.0625f ),	__four( 0.000244140625 ),	__four( 0.000030517578125 )
};

//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------

// Only run this once per VU! ;)
microVUt(void) mVUinit(VURegs* vuRegsPtr, int vuIndex) {

	microVU* mVU = mVUx;
	memset(&mVU->prog, 0, sizeof(mVU->prog));

	mVU->regs			= vuRegsPtr;
	mVU->index			= vuIndex;
	mVU->vuMemSize		= (vuIndex ? 0x4000 : 0x1000);
	mVU->microMemSize	= (vuIndex ? 0x4000 : 0x1000);
	mVU->progSize		= (vuIndex ? 0x4000 : 0x1000) / 4;
	mVU->cache			= NULL;
	mVU->cacheSize		= mVUcacheSize;
	mVU->prog.max		= mMaxProg - 1;
	mVU->prog.prog		= (microProgram*)_aligned_malloc(sizeof(microProgram)*(mVU->prog.max+1), 64);
	mVU->prog.progList	= new int[mMaxProg];
	mVU->regAlloc		= new microRegAlloc(mVU->regs);
	mVUprint((vuIndex) ? "microVU1: init" : "microVU0: init");

	mVU->cache = SysMmapEx((vuIndex ? 0x5f240000 : 0x5e240000), mVU->cacheSize + 0x1000, 0, (vuIndex ? "Micro VU1" : "Micro VU0"));
	if (!mVU->cache) throw Exception::OutOfMemory( "microVU Error: Failed to allocate recompiler memory!" );
	
	memset(mVU->cache, 0xcc, mVU->cacheSize + 0x1000);
	memset(mVU->prog.prog, 0, sizeof(microProgram)*(mVU->prog.max+1));
	
	// Setup Entrance/Exit Points
	x86SetPtr(mVU->cache);
	mVUdispatcherA(mVU);
	mVUdispatcherB(mVU);
	mVUemitSearch();
	mVUreset(mVU);
}

// Resets Rec Data
microVUt(void) mVUreset(mV) {

	mVUprint((mVU->index) ? "microVU1: reset" : "microVU0: reset");

	// Clear All Program Data
	//memset(&mVU->prog, 0, sizeof(mVU->prog));
	memset(&mVU->prog.lpState, 0, sizeof(mVU->prog.lpState));

	// Program Variables
	mVU->prog.cleared	=  1;
	mVU->prog.isSame	= -1;
	mVU->prog.cur		= -1;
	mVU->prog.total		= -1;
	mVU->prog.curFrame	=  0;
	mVU->prog.max		= mMaxProg - 1;

	// Setup Dynarec Cache Limits for Each Program
	u8* z = (mVU->cache + 0x1000); // Dispatcher Code is in first page of cache
	mVU->prog.x86start	= z;
	mVU->prog.x86ptr	= z;
	mVU->prog.x86end	= (u8*)((uptr)z + (uptr)(mVU->cacheSize - (_1mb * 3))); // 3mb "Safe Zone"

	for (int i = 0; i <= mVU->prog.max; i++) {
		if (!mVU->index) mVUclearProg<0>(i);
		else			 mVUclearProg<1>(i);
		mVU->prog.progList[i] = i;
	}
}

// Free Allocated Resources
microVUt(void) mVUclose(mV) {

	mVUprint((mVU->index) ? "microVU1: close" : "microVU0: close");

	if (mVU->cache) { HostSys::Munmap(mVU->cache, mVU->cacheSize); mVU->cache = NULL; }

	// Delete Programs and Block Managers
	if (mVU->prog.prog) {
		for (int i = 0; i <= mVU->prog.max; i++) {
			for (u32 j = 0; j < (mVU->progSize / 2); j++) {
				safe_delete(mVU->prog.prog[i].block[j]);
			}
		}
		safe_aligned_free(mVU->prog.prog);
	}
	safe_delete_array(mVU->prog.progList);
	safe_delete(mVU->regAlloc);
}

// Clears Block Data in specified range
microVUt(void) mVUclear(mV, u32 addr, u32 size) {
	//if (!mVU->prog.cleared) {
		//memset(&mVU->prog.lpState, 0, sizeof(mVU->prog.lpState));
		mVU->prog.cleared = 1; // Next execution searches/creates a new microprogram
	//}
}

//------------------------------------------------------------------
// Micro VU - Private Functions
//------------------------------------------------------------------

// Clears program data
microVUf(void) mVUclearProg(int progIndex) {
	microVU* mVU = mVUx;
	mVUprogI.used	   = 0;
	mVUprogI.isDead    = 1;
	mVUprogI.isOld	   = 1;
	mVUprogI.frame	   = mVU->prog.curFrame;
	for (int j = 0; j <= mVUprogI.ranges.max; j++) {
		mVUprogI.ranges.range[j][0]	= -1; // Set range to 
		mVUprogI.ranges.range[j][1]	= -1; // indeterminable status
		mVUprogI.ranges.total		= -1;
	}
	for (u32 i = 0; i < (mVU->progSize / 2); i++) {
		safe_delete(mVUprogI.block[i]);
	}
}

// Caches Micro Program
microVUf(void) mVUcacheProg(int progIndex) {
	microVU* mVU = mVUx;
	memcpy_fast(mVU->prog.prog[progIndex].data, mVU->regs->Micro, mVU->microMemSize);
	mVUdumpProg(progIndex);
}

// Sorts the program list (Moves progIndex to Beginning of ProgList)
microVUt(void) mVUsortProg(mV, int progIndex) {
	int* temp  = new int[mVU->prog.max+1];
	int offset = 0;
	for (int i = 0; i <= (mVU->prog.max-1); i++) {
		if (progIndex == mVU->prog.progList[i]) offset = 1;
		temp[i+1] = mVU->prog.progList[i+offset];
	}
	temp[0] = progIndex;
	delete[] mVU->prog.progList;
	mVU->prog.progList = temp;
}

// Finds the least used program, (if program list full clears and returns an old program; if not-full, returns free program)
microVUf(int) mVUfindLeastUsedProg() {
	microVU* mVU = mVUx;

	for (int i = 0; i <= mVU->prog.max; i++) {
		if (mVU->prog.prog[i].isDead) {
			mVU->prog.total++;
			mVUcacheProg<vuIndex>(i); // Cache Micro Program
			mVU->prog.prog[i].isDead = 0;
			mVU->prog.prog[i].isOld  = 0;
			mVU->prog.prog[i].used	 = 1;
			mVUsortProg(mVU, i);
			Console.Notice("microVU%d: Cached MicroPrograms = [%03d] [%03d]", vuIndex, i+1, mVU->prog.total+1);
			return i;
		}
	}

	static int clearIdx = 0;
	int pIdx = clearIdx;
	for (int i = 0; i < ((mVU->prog.max+1)/4); i++) {
		mVUclearProg<vuIndex>(clearIdx);
		clearIdx = aWrap(clearIdx+1, mVU->prog.max);
	}
	mVU->prog.total -= ((mVU->prog.max+1)/4)-1;
	mVUcacheProg<vuIndex>(pIdx); // Cache Micro Program
	mVU->prog.prog[pIdx].isDead	= 0;
	mVU->prog.prog[pIdx].isOld	= 0;
	mVU->prog.prog[pIdx].used	= 1;
	mVUsortProg(mVU, pIdx);
	Console.Notice("microVU%d: Cached MicroPrograms = [%03d] [%03d]", vuIndex, pIdx+1, mVU->prog.total+1);
	return pIdx;
}

// Finds and Ages/Kills Programs if they haven't been used in a while.
microVUt(void) mVUvsyncUpdate(mV) {
	for (int i = 0; i <= mVU->prog.max; i++) {
		if (mVU->prog.prog[i].isDead) continue;
		if (mVU->prog.prog[i].used) {
			mVU->prog.prog[i].used  = 0;
			mVU->prog.prog[i].frame = mVU->prog.curFrame;
		}
		else if (((mVU->prog.curFrame - mVU->prog.prog[i].frame) >= (360 * 10)) && (i != mVU->prog.cur)) {
			mVU->prog.total--;
			if (!mVU->index) mVUclearProg<0>(i);
			else			 mVUclearProg<1>(i);
			DevCon.Status("microVU%d: Killing Dead Program [%03d]", mVU->index, i+1);
		}
		else if (((mVU->prog.curFrame - mVU->prog.prog[i].frame) >= (30  *  1)) && !mVU->prog.prog[i].isOld) {
			mVU->prog.prog[i].isOld = 1;
			//DevCon.Status("microVU%d: Aging Old Program [%03d]", mVU->index, i+1);
		}
	}
	mVU->prog.curFrame++;
}

microVUf(bool) mVUcmpPartial(int progIndex) {
	microVU* mVU = mVUx;
	for (int i = 0; i <= mVUprogI.ranges.total; i++) {
		if ((mVUprogI.ranges.range[i][0] < 0)
		||  (mVUprogI.ranges.range[i][1] < 0)) { DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU->index, i, mVUprogI.ranges.total); }
		if (memcmp_mmx(cmpOffset(mVUprogI.data), cmpOffset(mVU->regs->Micro), ((mVUprogI.ranges.range[i][1] + 8) - mVUprogI.ranges.range[i][0]))) {
			return 0;
		}
	}
	return 1;
}

// Compare Cached microProgram to mVU->regs->Micro
microVUf(bool) mVUcmpProg(int progIndex, const bool checkOld, const bool cmpWholeProg) {
	microVU* mVU = mVUx;
	if (!mVUprogI.isDead && (checkOld == mVUprogI.isOld)) {
		if ((cmpWholeProg && !memcmp_mmx((u8*)mVUprogI.data, mVU->regs->Micro, mVU->microMemSize))
		|| (!cmpWholeProg && mVUcmpPartial<vuIndex>(progIndex))) {
			mVU->prog.cur = progIndex;
			mVU->prog.cleared = 0;
			mVU->prog.isSame = cmpWholeProg ? 1 : -1;
			mVU->prog.prog[progIndex].used	= 1;
			mVU->prog.prog[progIndex].isOld	= 0;
			return 1;
		}
	}
	return 0;
}

// Searches for Cached Micro Program and sets prog.cur to it (returns 1 if program found, else returns 0)
microVUf(int) mVUsearchProg() {
	microVU* mVU = mVUx;
	if (mVU->prog.cleared) { // If cleared, we need to search for new program
		for (int i = mVU->prog.max; i >= 0; i--) {
			if (mVUcmpProg<vuIndex>(mVU->prog.progList[i], 0, 0)) 
				return 1; // Check Young Programs
		}
		for (int i = mVU->prog.max; i >= 0; i--) {
			if (mVUcmpProg<vuIndex>(mVU->prog.progList[i], 1, 0)) 
				return 1; // Check Old Programs
		}
		mVU->prog.cur = mVUfindLeastUsedProg<vuIndex>(); // If cleared and program not found, make a new program instance
		mVU->prog.cleared = 0;
		mVU->prog.isSame  = 1;
		return 0;
	}
	mVU->prog.prog[mVU->prog.cur].used	= 1;
	mVU->prog.prog[mVU->prog.cur].isOld	= 0;
	return 1; // If !cleared, then we're still on the same program as last-time ;)
}

//------------------------------------------------------------------
// Wrapper Functions - Called by other parts of the Emu
//------------------------------------------------------------------

void initVUrec (VURegs* vuRegs, const int vuIndex) { mVUinit(vuRegs, vuIndex); }
void closeVUrec(const int vuIndex)				   { mVUclose(mVUx); }
void resetVUrec(const int vuIndex)				   { mVUreset(mVUx); }
void vsyncVUrec(const int vuIndex)				   { mVUvsyncUpdate(mVUx); }

void __fastcall clearVUrec(u32 addr, u32 size, const int vuIndex) { 
	mVUclear(mVUx, addr, size); 
}

void __fastcall runVUrec(u32 startPC, u32 cycles, const int vuIndex) {
	if (!vuIndex) ((mVUrecCall)microVU0.startFunct)(startPC, cycles);
	else		  ((mVUrecCall)microVU1.startFunct)(startPC, cycles);
}

