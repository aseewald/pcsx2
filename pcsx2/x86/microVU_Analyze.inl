/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2009  Pcsx2 Team
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
#ifdef PCSX2_MICROVU

//------------------------------------------------------------------
// Micro VU - Pass 1 Functions
//------------------------------------------------------------------

//------------------------------------------------------------------
// FMAC1 - Normal FMAC Opcodes
//------------------------------------------------------------------

#define aReg(x) mVUregs.VF[x]
#define bReg(x, y) mVUregsTemp.VFreg[y] = x; mVUregsTemp.VF[0]
#define aMax(x, y) ((x > y) ? x : y)

#define analyzeReg1(reg) {									\
	if (reg) {												\
		if (_X) { mVUstall = aMax(mVUstall, aReg(reg).x); }	\
		if (_Y) { mVUstall = aMax(mVUstall, aReg(reg).y); }	\
		if (_Z) { mVUstall = aMax(mVUstall, aReg(reg).z); }	\
		if (_W) { mVUstall = aMax(mVUstall, aReg(reg).w); } \
	}														\
}

#define analyzeReg2(reg, isLowOp) {				\
	if (reg) {									\
		if (_X) { bReg(reg, isLowOp).x = 4; }	\
		if (_Y) { bReg(reg, isLowOp).y = 4; }	\
		if (_Z) { bReg(reg, isLowOp).z = 4; }	\
		if (_W) { bReg(reg, isLowOp).w = 4; }	\
	}											\
}

microVUt(void) mVUanalyzeFMAC1(int Fd, int Fs, int Ft) {
	microVU* mVU = mVUx;
	mVUinfo |= _doStatus;
	analyzeReg1(Fs);
	analyzeReg1(Ft);
	analyzeReg2(Fd, 0);
}

//------------------------------------------------------------------
// FMAC2 - ABS/FTOI/ITOF Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeFMAC2(int Fs, int Ft) {
	microVU* mVU = mVUx;
	analyzeReg1(Fs);
	analyzeReg2(Ft, 0);
}

//------------------------------------------------------------------
// FMAC3 - BC(xyzw) FMAC Opcodes
//------------------------------------------------------------------

#define analyzeReg3(reg) {											\
	if (reg) {														\
		if (_bc_x)		{ mVUstall = aMax(mVUstall, aReg(reg).x); } \
		else if (_bc_y) { mVUstall = aMax(mVUstall, aReg(reg).y); }	\
		else if (_bc_z) { mVUstall = aMax(mVUstall, aReg(reg).z); }	\
		else			{ mVUstall = aMax(mVUstall, aReg(reg).w); } \
	}																\
}

microVUt(void) mVUanalyzeFMAC3(int Fd, int Fs, int Ft) {
	microVU* mVU = mVUx;
	mVUinfo |= _doStatus;
	analyzeReg1(Fs);
	analyzeReg3(Ft);
	analyzeReg2(Fd, 0);
}

//------------------------------------------------------------------
// FMAC4 - Clip FMAC Opcode
//------------------------------------------------------------------

#define analyzeReg4(reg) {								 \
	if (reg) { mVUstall = aMax(mVUstall, aReg(reg).w); } \
}

microVUt(void) mVUanalyzeFMAC4(int Fs, int Ft) {
	microVU* mVU = mVUx;
	mVUinfo |= _doClip;
	analyzeReg1(Fs);
	analyzeReg4(Ft);
}

//------------------------------------------------------------------
// IALU - IALU Opcodes
//------------------------------------------------------------------

#define analyzeVIreg1(reg)			{ if (reg) { mVUstall = aMax(mVUstall, mVUregs.VI[reg]); } }
#define analyzeVIreg2(reg, aCycles)	{ if (reg) { mVUregsTemp.VIreg = reg; mVUregsTemp.VI = aCycles; mVUinfo |= _writesVI; mVU->VIbackup[0] = reg; } }

microVUt(void) mVUanalyzeIALU1(int Id, int Is, int It) {
	microVU* mVU = mVUx;
	if (!Id) { mVUinfo |= _isNOP; }
	analyzeVIreg1(Is);
	analyzeVIreg1(It);
	analyzeVIreg2(Id, 1);
}

microVUt(void) mVUanalyzeIALU2(int Is, int It) {
	microVU* mVU = mVUx;
	if (!It) { mVUinfo |= _isNOP; }
	analyzeVIreg1(Is);
	analyzeVIreg2(It, 1);
}

//------------------------------------------------------------------
// MR32 - MR32 Opcode
//------------------------------------------------------------------

// Flips xyzw stalls to yzwx
#define analyzeReg6(reg) {									\
	if (reg) {												\
		if (_X) { mVUstall = aMax(mVUstall, aReg(reg).y); }	\
		if (_Y) { mVUstall = aMax(mVUstall, aReg(reg).z); }	\
		if (_Z) { mVUstall = aMax(mVUstall, aReg(reg).w); }	\
		if (_W) { mVUstall = aMax(mVUstall, aReg(reg).x); } \
	}														\
}

microVUt(void) mVUanalyzeMR32(int Fs, int Ft) {
	microVU* mVU = mVUx;
	if (!Ft) { mVUinfo |= _isNOP; }
	analyzeReg6(Fs);
	analyzeReg2(Ft, 1);
}

//------------------------------------------------------------------
// FDIV - DIV/SQRT/RSQRT Opcodes
//------------------------------------------------------------------

#define analyzeReg5(reg, fxf) {										\
	if (reg) {														\
		switch (fxf) {												\
			case 0: mVUstall = aMax(mVUstall, aReg(reg).x); break;	\
			case 1: mVUstall = aMax(mVUstall, aReg(reg).y); break;	\
			case 2: mVUstall = aMax(mVUstall, aReg(reg).z); break;	\
			case 3: mVUstall = aMax(mVUstall, aReg(reg).w); break;	\
		}															\
	}																\
}

#define analyzeQreg(x) { mVUregsTemp.q = x; mVUstall = aMax(mVUstall, mVUregs.q); }
#define analyzePreg(x) { mVUregsTemp.p = x; mVUstall = aMax(mVUstall, ((mVUregs.p) ? (mVUregs.p - 1) : 0)); }

microVUt(void) mVUanalyzeFDIV(int Fs, int Fsf, int Ft, int Ftf, u8 xCycles) {
	microVU* mVU = mVUx;
	mVUprint("microVU: DIV Opcode");
	analyzeReg5(Fs, Fsf);
	analyzeReg5(Ft, Ftf);
	analyzeQreg(xCycles);
}

//------------------------------------------------------------------
// EFU - EFU Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeEFU1(int Fs, int Fsf, u8 xCycles) {
	microVU* mVU = mVUx;
	mVUprint("microVU: EFU Opcode");
	analyzeReg5(Fs, Fsf);
	analyzePreg(xCycles);
}

microVUt(void) mVUanalyzeEFU2(int Fs, u8 xCycles) {
	microVU* mVU = mVUx;
	mVUprint("microVU: EFU Opcode");
	analyzeReg1(Fs);
	analyzePreg(xCycles);
}

//------------------------------------------------------------------
// MFP - MFP Opcode
//------------------------------------------------------------------

microVUt(void) mVUanalyzeMFP(int Ft) {
	microVU* mVU = mVUx;
	if (!Ft) { mVUinfo |= _isNOP; }
	analyzeReg2(Ft, 1);
}

//------------------------------------------------------------------
// MOVE - MOVE Opcode
//------------------------------------------------------------------

microVUt(void) mVUanalyzeMOVE(int Fs, int Ft) {
	microVU* mVU = mVUx;
	if (!Ft || (Ft == Fs)) { mVUinfo |= _isNOP; }
	if (mVUregsTemp.VFreg[0] == Fs) { mVUinfo |= _swapOps; } 
	analyzeReg1(Fs);
	analyzeReg2(Ft, 1);
}


//------------------------------------------------------------------
// LQx - LQ/LQD/LQI Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeLQ(int Ft, int Is, bool writeIs) {
	microVU* mVU = mVUx;
	analyzeVIreg1(Is);
	analyzeReg2(Ft, 1);
	if (!Ft)	 { mVUinfo |= (writeIs && Is) ? _noWriteVF : _isNOP; }
	if (writeIs) { analyzeVIreg2(Is, 1); }
}

//------------------------------------------------------------------
// SQx - SQ/SQD/SQI Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeSQ(int Fs, int It, bool writeIt) {
	microVU* mVU = mVUx;
	analyzeReg1(Fs);
	analyzeVIreg1(It);
	if (writeIt) { analyzeVIreg2(It, 1); }
	if (mVUregsTemp.VFreg[0] == Fs) { mVUinfo |= _swapOps; } 
}

//------------------------------------------------------------------
// R*** - R Reg Opcodes
//------------------------------------------------------------------

#define analyzeRreg() { mVUregsTemp.r = 1; }

microVUt(void) mVUanalyzeR1(int Fs, int Fsf) {
	microVU* mVU = mVUx;
	analyzeReg5(Fs, Fsf);
	analyzeRreg();
}

microVUt(void) mVUanalyzeR2(int Ft, bool canBeNOP) {
	microVU* mVU = mVUx;
	if (!Ft) { mVUinfo |= ((canBeNOP) ? _isNOP : _noWriteVF); }
	analyzeReg2(Ft, 1);
	analyzeRreg();
}

//------------------------------------------------------------------
// Sflag - Status Flag Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeSflag(int It) {
	microVU* mVU = mVUx;
	if (!It) { mVUinfo |= _isNOP; }
	else {
		mVUinfo |= _swapOps;
		if (mVUcount < 4)	{ mVUpBlock->pState.needExactMatch |= 0xf /*<< mVUcount*/; }
		if (mVUcount >= 1)	{ incPC2(-2); mVUinfo |= _isSflag; incPC2(2); }
		// Note: _isSflag is used for status flag optimizations.
		// Do to stalls, it can only be set one instruction prior to the status flag read instruction
		// if we were guaranteed no-stalls were to happen, it could be set 4 instruction prior.
	}
	analyzeVIreg2(It, 1);
}

microVUt(void) mVUanalyzeFSSET() {
	microVU* mVU = mVUx;
	mVUinfo |= _isFSSET;
	// mVUinfo &= ~_doStatus;
	// Note: I'm not entirely sure if the non-sticky flags
	// should be taken from the current upper instruction
	// or if they should be taken from the previous instruction
	// Uncomment the above line if the latter-case is true
}

//------------------------------------------------------------------
// Mflag - Mac Flag Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeMflag(int Is, int It) {
	microVU* mVU = mVUx;
	if (!It) { mVUinfo |= _isNOP; }
	else { // Need set _doMac for 4 previous Ops (need to do all 4 because stalls could change the result needed)
		mVUinfo |= _swapOps;
		if (mVUcount < 4) { mVUpBlock->pState.needExactMatch |= 0xf << (/*mVUcount +*/ 4); }
		int curPC = iPC;
		for (int i = mVUcount, j = 0; i > 1; i--, j++) {
			incPC2(-2);
			if (doStatus) { mVUinfo |= _doMac; if (j >= 3) { break; } }
		}
		iPC = curPC;
	}
	analyzeVIreg1(Is);
	analyzeVIreg2(It, 1);
}

//------------------------------------------------------------------
// Cflag - Clip Flag Opcodes
//------------------------------------------------------------------

microVUt(void) mVUanalyzeCflag() {
	microVU* mVU = mVUx;
	mVUinfo |= _swapOps;
	if (mVUcount < 4) { mVUpBlock->pState.needExactMatch |= 0xf << (/*mVUcount +*/ 8); }
}

//------------------------------------------------------------------
// XGkick
//------------------------------------------------------------------

#define analyzeXGkick1()  { mVUstall = aMax(mVUstall, mVUregs.xgkick); }
#define analyzeXGkick2(x) { mVUregsTemp.xgkick = x; }

microVUt(void) mVUanalyzeXGkick(int Fs, int xCycles) {
	microVU* mVU = mVUx;
	analyzeVIreg1(Fs);
	analyzeXGkick1();
	analyzeXGkick2(xCycles);
}

//------------------------------------------------------------------
// Branches - Branch Opcodes
//------------------------------------------------------------------

#define analyzeBranchVI(reg, infoVal) {													\
	if (reg && (mVUcount > 0)) { /* Ensures branch is not first opcode in block */		\
		incPC2(-2);																		\
		if (writesVI && (reg == mVU->VIbackup[0])) { /* If prev Op modified VI reg */	\
			mVUinfo |= _backupVI;														\
			incPC2(2);																	\
			mVUinfo |= infoVal;															\
		}																				\
		else { incPC2(2); }																\
	}																					\
}

microVUt(void) mVUanalyzeBranch1(int Is) {
	microVU* mVU = mVUx;
	if (mVUregs.VI[Is] || mVUstall)	{ analyzeVIreg1(Is); }
	else							{ analyzeBranchVI(Is, _memReadIs); }
}

microVUt(void) mVUanalyzeBranch2(int Is, int It) {
	microVU* mVU = mVUx;
	if (mVUregs.VI[Is] || mVUregs.VI[It] || mVUstall) { analyzeVIreg1(Is); analyzeVIreg1(It); }
	else											  { analyzeBranchVI(Is, _memReadIs); analyzeBranchVI(It, _memReadIt);}
}

#endif //PCSX2_MICROVU
