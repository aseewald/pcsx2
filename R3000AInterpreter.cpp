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

#include "PsxCommon.h"
#include "Common.h"

using namespace R3000A;

// Used to flag delay slot instructions when throwig exceptions.
bool iopIsDelaySlot = false;

static bool branch2 = 0;
static u32 branchPC;

static void doBranch(s32 tar);	// forward declared prototype


/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  if(_i32(_rRs_) op 0) { _SetLink(31); doBranch(_BranchTarget_); }

void psxBGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
void psxBGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
void psxBGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
void psxBLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
void psxBLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
void psxBLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

void psxBEQ() {	RepBranchi32(==) }  // Branch if Rs == Rt
void psxBNE() {	RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ()   {               doBranch(_JumpTarget_); }
void psxJAL() {	_SetLink(31); doBranch(_JumpTarget_); /*spyFunctions();*/ }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR()   {                 doBranch(_u32(_rRs_)); }
void psxJALR() { if (_Rd_) { _SetLink(_Rd_); } doBranch(_u32(_rRs_)); }

///////////////////////////////////////////
// These macros are used to assemble the repassembler functions

static __forceinline void execI()
{
	psxRegs.code = iopMemRead32(psxRegs.pc);

	PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc));

	psxRegs.pc+= 4;
	psxRegs.cycle++;
	psxCycleEE-=8;

	psxBSC[psxRegs.code >> 26]();
}


static void doBranch(s32 tar) {
	branch2 = iopIsDelaySlot = true;
	branchPC = tar;
	execI();
	iopIsDelaySlot = false;
	psxRegs.pc = branchPC;

	iopEventTest();
}

static void intAlloc() {
}

static void intReset() {
}

static void intExecute() {
	for (;;) execI();
}

#ifdef _DEBUG
extern u32 psxdump;
extern void iDumpPsxRegisters(u32,u32);
#endif

static s32 intExecuteBlock( s32 eeCycles )
{
	psxBreak = 0;
	psxCycleEE = eeCycles;

	while (psxCycleEE > 0){
		branch2 = 0;
		while (!branch2) {
			execI();
        }
	}
	return psxBreak + psxCycleEE;
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

R3000Acpu psxInt = {
	intAlloc,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};
