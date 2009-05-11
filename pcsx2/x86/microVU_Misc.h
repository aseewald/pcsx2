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

//------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------

#define declareAllVariables											\
initVariable( _somePrefix_,	u32,   mVU_absclip,	0x7fffffff );		\
initVariable( _somePrefix_,	u32,   mVU_signbit,	0x80000000 );		\
initVariable( _somePrefix_,	u32,   mVU_minvals,	0xff7fffff );		\
initVariable( _somePrefix_,	u32,   mVU_maxvals,	0x7f7fffff );		\
initVariable( _somePrefix_, u32,   mVU_one,		0x3f800000 );		\
initVariable( _somePrefix_, u32,   mVU_T1,		0x3f7ffff5 );		\
initVariable( _somePrefix_, u32,   mVU_T2,		0xbeaaa61c );		\
initVariable( _somePrefix_, u32,   mVU_T3,		0x3e4c40a6 );		\
initVariable( _somePrefix_, u32,   mVU_T4,		0xbe0e6c63 );		\
initVariable( _somePrefix_, u32,   mVU_T5,		0x3dc577df );		\
initVariable( _somePrefix_, u32,   mVU_T6,		0xbd6501c4 );		\
initVariable( _somePrefix_, u32,   mVU_T7,		0x3cb31652 );		\
initVariable( _somePrefix_, u32,   mVU_T8,		0xbb84d7e7 );		\
initVariable( _somePrefix_, u32,   mVU_Pi4,		0x3f490fdb );		\
initVariable( _somePrefix_, u32,   mVU_S2,		0xbe2aaaa4 );		\
initVariable( _somePrefix_, u32,   mVU_S3,		0x3c08873e );		\
initVariable( _somePrefix_, u32,   mVU_S4,		0xb94fb21f );		\
initVariable( _somePrefix_, u32,   mVU_S5,		0x362e9c14 );		\
initVariable( _somePrefix_, u32,   mVU_E1,		0x3e7fffa8 );		\
initVariable( _somePrefix_, u32,   mVU_E2,		0x3d0007f4 );		\
initVariable( _somePrefix_, u32,   mVU_E3,		0x3b29d3ff );		\
initVariable( _somePrefix_, u32,   mVU_E4,		0x3933e553 );		\
initVariable( _somePrefix_, u32,   mVU_E5,		0x36b63510 );		\
initVariable( _somePrefix_,	u32,   mVU_E6,		0x353961ac );		\
initVariable( _somePrefix_, float, mVU_FTOI_4,	16.0 );				\
initVariable( _somePrefix_, float, mVU_FTOI_12,	4096.0 );			\
initVariable( _somePrefix_, float, mVU_FTOI_15,	32768.0 );			\
initVariable( _somePrefix_, float, mVU_ITOF_4,	0.0625f );			\
initVariable( _somePrefix_, float, mVU_ITOF_12,	0.000244140625 );	\
initVariable( _somePrefix_, float, mVU_ITOF_15,	0.000030517578125 );

#define _somePrefix_ PCSX2_ALIGNED16_EXTERN
#define initVariable(aprefix, atype, aname, avalue) aprefix (const atype aname [4]);
declareAllVariables
#undef	_somePrefix_
#undef	initVariable

#define _somePrefix_ PCSX2_ALIGNED16
#define initVariable(aprefix, atype, aname, avalue) aprefix (const atype aname [4])	= {avalue, avalue, avalue, avalue};

//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ ((mVU->code >> 16) & 0x1F)  // The ft part of the instruction register 
#define _Fs_ ((mVU->code >> 11) & 0x1F)  // The fs part of the instruction register 
#define _Fd_ ((mVU->code >>  6) & 0x1F)  // The fd part of the instruction register

#define _It_ ((mVU->code >> 16) & 0xF)   // The it part of the instruction register 
#define _Is_ ((mVU->code >> 11) & 0xF)   // The is part of the instruction register 
#define _Id_ ((mVU->code >>  6) & 0xF)   // The id part of the instruction register

#define _X	 ((mVU->code>>24) & 0x1)
#define _Y	 ((mVU->code>>23) & 0x1)
#define _Z	 ((mVU->code>>22) & 0x1)
#define _W	 ((mVU->code>>21) & 0x1)

#define _XYZW_SS	(_X+_Y+_Z+_W==1)
#define _X_Y_Z_W	(((mVU->code >> 21 ) & 0xF ))
#define _xyzw_ACC	((_XYZW_SS && !_X) ? 15 : _X_Y_Z_W)

#define _bc_	 (mVU->code & 0x3)
#define _bc_x	((mVU->code & 0x3) == 0)
#define _bc_y	((mVU->code & 0x3) == 1)
#define _bc_z	((mVU->code & 0x3) == 2)
#define _bc_w	((mVU->code & 0x3) == 3)

#define _Fsf_	((mVU->code >> 21) & 0x03)
#define _Ftf_	((mVU->code >> 23) & 0x03)

#define _Imm5_	(s16)(((mVU->code & 0x400) ? 0xfff0 : 0) | ((mVU->code >> 6) & 0xf))
#define _Imm11_	(s32)((mVU->code & 0x400) ? (0xfffffc00 | (mVU->code & 0x3ff)) : mVU->code & 0x3ff)
#define _Imm12_	(((mVU->code >> 21) & 0x1) << 11) | (mVU->code & 0x7ff)
#define _Imm15_	(((mVU->code >> 10) & 0x7800) | (mVU->code & 0x7ff))
#define _Imm24_	(u32)(mVU->code & 0xffffff)

#define _Ibit_ (1<<31)
#define _Ebit_ (1<<30)
#define _Mbit_ (1<<29)
#define _Dbit_ (1<<28)
#define _Tbit_ (1<<27)
#define _MDTbit_ 0 //( _Mbit_ | _Dbit_ | _Tbit_ ) // ToDo: Implement this stuff...

#define getVUmem(x)	(((vuIndex == 1) ? (x & 0x3ff) : ((x >= 0x400) ? (x & 0x43f) : (x & 0xff))) * 16)
#define offsetSS	((_X) ? (0) : ((_Y) ? (4) : ((_Z) ? 8: 12)))

#define xmmT1	0 // Temp Reg
#define xmmFs	1 // Holds the Value of Fs (writes back result Fd)
#define xmmFt	2 // Holds the Value of Ft
#define xmmACC	3 // Holds ACC
#define xmmMax	4 // Holds mVU_maxvals
#define xmmMin	5 // Holds mVU_minvals
#define xmmT2	6 // Temp Reg?
#define xmmPQ	7 // Holds the Value and Backup Values of P and Q regs

#define mmxVI1	0 // Holds VI 1
#define mmxVI2	1 // Holds VI 2
#define mmxVI3	2 // Holds VI 3
#define mmxVI4	3 // Holds VI 4
#define mmxVI5	4 // Holds VI 5
#define mmxVI6	5 // Holds VI 6
#define mmxVI7	6 // Holds VI 7
#define mmxVI8	7 // Holds VI 8

#define gprT1	0 // Temp Reg
#define gprT2	1 // Temp Reg
#define gprR	2 // R Reg
#define gprF0	3 // Status Flag 0
#define gprESP	4 // Don't use?
#define gprF1	5 // Status Flag 1
#define gprF2	6 // Status Flag 2
#define gprF3	7 // Status Flag 3

// Template Stuff
#define mVUx (vuIndex ? &microVU1 : &microVU0)
#define microVUt(aType) template<int vuIndex> __forceinline aType
#define microVUx(aType) template<int vuIndex> aType
#define microVUf(aType) template<int vuIndex, int recPass> aType
#define microVUq(aType) template<int vuIndex, int recPass>  __forceinline aType

// Define Passes
#define pass1 if (recPass == 0)
#define pass2 if (recPass == 1)
#define pass3 if (recPass == 2)
#define pass4 if (recPass == 3)

// Misc Macros...
#define mVUcurProg	 mVU->prog.prog[mVU->prog.cur]
#define mVUblocks	 mVU->prog.prog[mVU->prog.cur].block
#define mVUallocInfo mVU->prog.prog[mVU->prog.cur].allocInfo
#define mVUbranch	 mVUallocInfo.branch
#define mVUcycles	 mVUallocInfo.cycles
#define mVUcount	 mVUallocInfo.count
#define mVUpBlock	 mVUallocInfo.pBlock
#define mVUblock	 mVUallocInfo.block
#define mVUregs		 mVUallocInfo.block.pState
#define mVUregsTemp	 mVUallocInfo.regsTemp
#define iPC			 mVUallocInfo.curPC
#define mVUflagInfo	 mVUregs.needExactMatch //mVUallocInfo.flagInfo
#define mVUinfo		 mVUallocInfo.info[iPC / 2]
#define mVUstall	 mVUallocInfo.stall[iPC / 2]
#define mVUstartPC	 mVUallocInfo.startPC
#define xPC			 ((iPC / 2) * 8)
#define curI		 mVUcurProg.data[iPC]
#define setCode()	 { mVU->code = curI; }
#define incPC(x)	 { iPC = ((iPC + x) & (mVU->progSize-1)); setCode(); }
#define incPC2(x)	 { iPC = ((iPC + x) & (mVU->progSize-1)); }
#define incCycles(x) { mVUincCycles<vuIndex>(x); }
#define bSaveAddr	 (((xPC + (2 * 8)) & ((vuIndex) ? 0x3ff8:0xff8)) / 8)
#define branchAddr	 ((xPC + 8 + (_Imm11_ * 8)) & ((vuIndex) ? 0x3ff8:0xff8))
#define shufflePQ	 (((mVU->p) ? 0xb0 : 0xe0) | ((mVU->q) ? 0x01 : 0x04))

// Pass 1 uses these to set mVUinfo
#define _isNOP		 (1<<0) // Skip Lower Instruction
#define _isBranch	 (1<<1) // Cur Instruction is a Branch
#define _isEOB		 (1<<2) // End of Block
#define _isBdelay	 (1<<3) // Cur Instruction in Branch Delay slot
#define _isSflag	 (1<<4) // Cur Instruction uses status flag
#define _writeQ		 (1<<5)
#define _readQ		 (1<<6)
#define _writeP		 (1<<7)
#define _readP		 (1<<7) // same as writeP
#define _doFlags	 (3<<8)
#define _doMac		 (1<<8)
#define _doStatus	 (1<<9)
#define _fmInstance	 (3<<10) // Mac		Write Instance
#define _fsInstance	 (3<<12) // Status	Write Instance
#define _fcInstance	 (3<<14) // Clip	Write Instance
#define _fpsInstance (3<<12) // Prev.S.	Write Instance
#define _fpcInstance (3<<14) // Prev.C.	Write Instance
#define _fvmInstance (3<<16) // Mac		Read Instance (at T-stage for lower instruction)
#define _fvsInstance (3<<18) // Status	Read Instance (at T-stage for lower instruction)
#define _fvcInstance (3<<20) // Clip	Read Instance (at T-stage for lower instruction)
#define _backupVI	 (1<<22) // Backup VI reg to memory if modified before branch (branch uses old VI value unless opcode is ILW or ILWR)
#define _memReadIs	 (1<<23) // Read Is (VI reg) from memory (used by branches)
#define _memReadIt	 (1<<24) // Read If (VI reg) from memory (used by branches)
#define _writesVI	 (1<<25) // Current Instruction writes to VI
#define _swapOps	 (1<<26) // Runs Lower Instruction Before Upper Instruction
#define _isFSSET	 (1<<27) // Cur Instruction is FSSET
#define _doDivFlag	 (1<<28) // Transfer Div flag to Status Flag
#define _doClip		 (1<<29)
#define _noWriteVF	 (1<<30) // Don't write back the result of a lower op to VF reg if upper op writes to same reg (or if VF = 0)

// Pass 2 uses these to read mVUinfo
#define isNOP		 (mVUinfo & (1<<0))
#define isBranch	 (mVUinfo & (1<<1))
#define isEOB		 (mVUinfo & (1<<2))
#define isBdelay	 (mVUinfo & (1<<3))
#define isSflag		 (mVUinfo & (1<<4))
#define writeQ		((mVUinfo >> 5) & 1)
#define readQ		((mVUinfo >> 6) & 1)
#define writeP		((mVUinfo >> 7) & 1)
#define readP		((mVUinfo >> 7) & 1) // same as writeP
#define doFlags		 (mVUinfo & (3<<8))
#define doMac		 (mVUinfo & (1<<8))
#define doStatus	 (mVUinfo & (1<<9))
#define fmInstance	((mVUinfo >> 10) & 3)
#define fsInstance	((mVUinfo >> 12) & 3)
#define fpsInstance	((((mVUinfo>>12) & 3) - 1) & 0x3)
#define fcInstance	((mVUinfo >> 14) & 3)
#define fpcInstance	((((mVUinfo>>14) & 3) - 1) & 0x3)
#define fvmInstance	((mVUinfo >> 16) & 3)
#define fvsInstance	((mVUinfo >> 18) & 3)
#define fvcInstance	((mVUinfo >> 20) & 3)
#define backupVI	 (mVUinfo & (1<<22))
#define memReadIs	 (mVUinfo & (1<<23))
#define memReadIt	 (mVUinfo & (1<<24))
#define writesVI	 (mVUinfo & (1<<25))
#define swapOps		 (mVUinfo & (1<<26))
#define isFSSET		 (mVUinfo & (1<<27))
#define doDivFlag	 (mVUinfo & (1<<28))
#define doClip		 (mVUinfo & (1<<29))
#define noWriteVF	 (mVUinfo & (1<<30))

// Pass 3 Helper Macros
#define _Fsf_String	 ((_Fsf_ == 3) ? "w" : ((_Fsf_ == 2) ? "z" : ((_Fsf_ == 1) ? "y" : "x")))
#define _Ftf_String	 ((_Ftf_ == 3) ? "w" : ((_Ftf_ == 2) ? "z" : ((_Ftf_ == 1) ? "y" : "x")))
#define xyzwStr(x,s) (_X_Y_Z_W == x) ? s :
#define _XYZW_String (xyzwStr(1, "w") (xyzwStr(2, "z") (xyzwStr(3, "zw") (xyzwStr(4, "y") (xyzwStr(5, "yw") (xyzwStr(6, "yz") (xyzwStr(7, "yzw") (xyzwStr(8, "x") (xyzwStr(9, "xw") (xyzwStr(10, "xz") (xyzwStr(11, "xzw") (xyzwStr(12, "xy") (xyzwStr(13, "xyw") (xyzwStr(14, "xyz") "xyzw"))))))))))))))
#define _BC_String	 (_bc_x ? "x" : (_bc_y ? "y" : (_bc_z ? "z" : "w")))
#define mVUlogFtFs() { mVUlog(".%s vf%02d, vf%02d", _XYZW_String, _Ft_, _Fs_); }
#define mVUlogFd()	 { mVUlog(".%s vf%02d, vf%02d", _XYZW_String, _Fd_, _Fs_); }
#define mVUlogACC()	 { mVUlog(".%s ACC, vf%02d", _XYZW_String, _Fs_); }
#define mVUlogFt()	 { mVUlog(", vf%02d", _Ft_); }
#define mVUlogBC()	 { mVUlog(", vf%02d%s", _Ft_, _BC_String); }
#define mVUlogI()	 { mVUlog(", I"); }
#define mVUlogQ()	 { mVUlog(", Q"); }
#define mVUlogCLIP() { mVUlog("w.xyz vf%02d, vf%02dw", _Fs_, _Ft_); }

// Flag Info
//#define __NeedExact	 (1<<12)
//#define __ExactMatch (mVUregs.needExactMatch & (1<<12))
#define __Status	 (mVUflagInfo & (0xf<<0))
#define __Mac		 (mVUflagInfo & (0xf<<4))
#define __Clip		 (mVUflagInfo & (0xf<<8))

// Store VI regs in mmx regs?
#define isMMX(_VIreg_)	0 //(_VIreg_ >= 1 && _VIreg_ <=8)
#define mmVI(_VIreg_)	(_VIreg_ - 1)

// Debug Stuff...
#ifdef mVUdebug
#define mVUprint Console::Status
#define mVUdebug1() {											\
	if (curI & _Ibit_)	{ SysPrintf("microVU: I-bit set!\n"); }	\
	if (curI & _Ebit_)	{ SysPrintf("microVU: E-bit set!\n"); }	\
	if (curI & _Mbit_)	{ SysPrintf("microVU: M-bit set!\n"); }	\
	if (curI & _Dbit_)	{ SysPrintf("microVU: D-bit set!\n"); }	\
	if (curI & _Tbit_)	{ SysPrintf("microVU: T-bit set!\n"); }	\
}
#else
#define mVUprint 0&&
#define mVUdebug1() {}
#endif

// Program Logging...
#ifdef mVUlogProg
#define mVUlog __mVULog<vuIndex>
#define mVUdumpProg __mVUdumpProgram<vuIndex>
#else
#define mVUlog 0&&
#define mVUdumpProg 0&&
#endif

// Cache Limit Check
#define mVUcacheCheck(ptr, start, limit) {  \
	uptr diff = ptr - start; \
	if (diff >= limit) { Console::Error("microVU Error: Program went over its cache limit. Size = 0x%x", params diff); } \
}
