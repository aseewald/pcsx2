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

#include <stdio.h>

union IntSign32
{
	s32 SL;
	u32 UL;

	s16 SS[2];
	u16 US[2];
};

namespace R3000Air
{

union GPRRegs
{
	struct
	{
		IntSign32
			r0, at, v0, v1, a0, a1, a2, a3,
			t0, t1, t2, t3, t4, t5, t6, t7,
			s0, s1, s2, s3, s4, s5, s6, s7,
			t8, t9, k0, k1, gp, sp, s8, ra,
			hi, lo; // hi needs to be at index 32! don't change
	} n;
	IntSign32 r[34]; /* Lo, Hi in r[33] and r[32] */
};

union CP0Regs
{
	struct
	{
		u32
			Index,     Random,    EntryLo0,  EntryLo1,
			Context,   PageMask,  Wired,     Reserved0,
			BadVAddr,  Count,     EntryHi,   Compare,
			Status,    Cause,     EPC,       PRid,
			Config,    LLAddr,    WatchLO,   WatchHI,
			XContext,  Reserved1, Reserved2, Reserved3,
			Reserved4, Reserved5, ECC,       CacheErr,
			TagLo,     TagHi,     ErrorEPC,  Reserved6;
	} n;
	IntSign32 r[32];
};

struct SVector2D {
	s16 x, y;
};

struct SVector2Dz {
	s16 z, pad;
};

struct SVector3D {
	s16 x, y, z, pad;
};

struct LVector3D {
	s16 x, y, z, pad;
};

struct CBGR {
	u8 r, g, b, c;
};

struct SMatrix3D {
	s16 m11, m12, m13, m21, m22, m23, m31, m32, m33, pad;
};

union CP2Data
{
	struct
	{
		SVector3D   v0, v1, v2;
		CBGR        rgb;
		s32         otz;
		s32         ir0, ir1, ir2, ir3;
		SVector2D   sxy0, sxy1, sxy2, sxyp;
		SVector2Dz  sz0, sz1, sz2, sz3;
		CBGR        rgb0, rgb1, rgb2;
		s32         reserved;
		s32         mac0, mac1, mac2, mac3;
		u32         irgb, orgb;
		s32         lzcs, lzcr;
	} n;
	u32 r[32];
};

union CP2Ctrl
{
	struct
	{
		SMatrix3D rMatrix;
		s32      trX, trY, trZ;
		SMatrix3D lMatrix;
		s32      rbk, gbk, bbk;
		SMatrix3D cMatrix;
		s32      rfc, gfc, bfc;
		s32      ofx, ofy;
		s32      h;
		s32      dqa, dqb;
		s32      zsf3, zsf4;
		s32      flag;
	} n;
	u32 r[32];
};

struct Registers
{
	GPRRegs GPR;		// General Purpose Registers
	CP0Regs CP0;		// Coprocessor0 Registers
	CP2Data CP2D; 		// Cop2 data registers
	CP2Ctrl CP2C; 		// Cop2 control registers
	u32 pc;				// Program counter
	//u32 code;			// The instruction
	u32 cycle;
	u32 interrupt;
	u32 sCycle[32];		// start cycle for signaled ints
	s32 eCycle[32];		// cycle delta for signaled ints (sCycle + eCycle == branch cycle)

	s32 NextCounter;
	u32 NextsCounter;

	// Controls when branch tests are performed.
	u32 NextBranchCycle;

	// old wacky data related to the PS1 GPU?
	//u32 _msflag[32];
	//u32 _smflag[32];
};

PCSX2_ALIGNED16_EXTERN(Registers iopRegs);

struct GprConstStatus
{
	bool Rd:1;
	bool Rs:1;
	bool Rt:1;
	bool Hi:1;
	bool Lo:1;
	bool Link:1;	// ra - the link register
};

#undef _Funct_
#undef _Opcode_
#undef _Rd_
#undef _Rs_
#undef _Rt_
#undef _Sa_
#undef _PC_

//////////////////////////////////////////////////////////////////////////////////////////
//
#define INSTRUCTION_API(retval, postfix) \
	retval BGEZ() postfix; \
	retval BGEZAL() postfix; \
	retval BGTZ() postfix; \
	retval BLEZ() postfix; \
	retval BLTZ() postfix; \
	retval BLTZAL() postfix; \
	retval BEQ() postfix; \
	retval BNE() postfix; \
 \
	retval J() postfix; \
	retval JAL() postfix; \
	retval JR() postfix; \
	retval JALR() postfix; \
 \
	retval ADDI() postfix; \
	retval ADDIU() postfix; \
	retval ANDI() postfix; \
	retval ORI() postfix; \
	retval XORI() postfix; \
	retval SLTI() postfix; \
	retval SLTIU() postfix; \
 \
	retval ADD() postfix; \
	retval ADDU() postfix; \
	retval SUB() postfix; \
	retval SUBU() postfix; \
	retval AND() postfix; \
	retval OR() postfix; \
	retval XOR() postfix; \
	retval NOR() postfix; \
	retval SLT() postfix; \
	retval SLTU() postfix; \
 \
	retval DIV() postfix; \
	retval DIVU() postfix; \
	retval MULT() postfix; \
	retval MULTU() postfix; \
 \
	retval SLL() postfix; \
	retval SRA() postfix; \
	retval SRL() postfix; \
	retval SLLV() postfix; \
	retval SRAV() postfix; \
	retval SRLV() postfix; \
	retval LUI() postfix; \
 \
	retval MFHI() postfix; \
	retval MFLO() postfix; \
	retval MTHI() postfix; \
	retval MTLO() postfix; \
 \
	retval BREAK() postfix; \
	retval SYSCALL() postfix; \
	retval RFE() postfix; \
 \
	retval LB() postfix; \
	retval LBU() postfix; \
	retval LH() postfix; \
	retval LHU() postfix; \
	retval LW() postfix; \
	retval LWL() postfix; \
	retval LWR() postfix; \
 \
	retval SB() postfix; \
	retval SH() postfix; \
	retval SW() postfix; \
	retval SWL() postfix; \
	retval SWR() postfix; \
 \
	retval MFC0() postfix; \
	retval CFC0() postfix; \
	retval MTC0() postfix; \
	retval CTC0() postfix; \
	retval Unknown() postfix;


//////////////////////////////////////////////////////////////////////////////////////////
//
// This enumerator is used to describe the display of each instruction/opcode.
// All instructions are allowed three parameters.
enum ParamType
{
	Param_None,
	Param_Rt,
	Param_Rs,
	Param_Rd,
	Param_Sa,

	Param_Fs,			// Used by Cop0 load/store

	Param_Hi,
	Param_Lo,
	Param_HiLo,			// 64 bit operand (allowed as destination only)

	Param_Imm,
	Param_Imm16,		// Immediate, shifted up by 16. [used by LUI]

	Param_AddrImm,		// Address Immediate (Rs + Imm()), used by load/store
	Param_BranchOffset,
	Param_JumpTarget,	// 26 bit immediate used as a jump target
	Param_RsJumpTarget	// 32 bit register used as a jump target
};

struct InstDiagInfo
{
	const char* Name;			// display name
	const ParamType Param[3];	// parameter mappings

	// indicates if this is an unsigned operation or not.
	// Source registers will be displayed as unsigned (zero-extended) if set.
	const bool IsUnsigned;
};


//////////////////////////////////////////////////////////////////////////////////////////
// This object is immutable.  Do not modify, just create new ones. :)
// See R3000AirInstruction.cpp for code implementations.
class Instruction 
{

	//////////////////////////////////////////////////////////////////////////////////////
	// Instance Variables (public and private)

public:
	const u32 U32;		// the whole instruction as a U32.

	// Precached values for the instruction (makes debugging and processing simpler).
	// Optimization: All values are stored as u8's to reduce memory overhead of this class.
	const u8 _Funct_;
	const u8 _Sa_;
	const u8 _Rd_;
	const u8 _Rt_;
	const u8 _Rs_;
	const u8 _Opcode_;

	const u32 _Pc_;		// program counter for this specific instruction

	const bool IsDelaySlot;

	GprConstStatus IsConstInput;
	GprConstStatus IsConstOutput;

protected:
	// maps to the psxRegs GPR storage area (actual value of the GPR)
	IntSign32& RdValue;
	// maps to the psxRegs GPR storage area (actual value of the GPR)
	IntSign32& RtValue;
	// maps to the psxRegs GPR storage area (actual value of the GPR)
	IntSign32& RsValue;

	bool m_IsBranchType;	// set true by the interpretation process when the instruction is a branching instruction.
	bool m_Branching;		// set true by the interpretation process when the program has branched.
	u32 m_NextPC;			// new PC after instruction has finished execution.

	const InstDiagInfo* m_Syntax;

	//////////////////////////////////////////////////////////////////////////////////////
	// Instances Methods and Functions (public and private)

public:
	Instruction( u32 srcPc, GprConstStatus constStatus, bool isDelaySlot=false );
	Instruction( u32 srcPc, bool isDelaySlot=false );
	template< typename T> static void Process( T& Inst );

	string GetParamName( const uint pidx ) const;
	string GetParamValue( const uint pidx ) const;
	string GetDisasm() const;
	string GetValuesComment() const;


	// Returns true if this instruction is a branch/jump type (which means it
	// will have a delay slot which should execute prior to the jump but *after*
	// the branch instruction's target has been calculated).
	const bool IsBranchType() const;

	const u32 GetNextPC() const { return m_NextPC; }

	// -------------------------------------------------------------------------
	// Infrequently used APIs for grabbing portions of the opcode.  These are
	// not cached since only a small handful of instructions use them.

	// Returns the target portion of the opcode (26 bit immediate)
	uint Target() const { return U32 & 0x03ffffff; }

	// Sign-extended immediate
	s32 Imm() const { return (s16)U32; }

	// Zero-extended immediate
	u32 ImmU() const { return (u16)U32; }

	u32 AddrImm() const { return RsValue.UL + Imm(); }

	// Calculates the target for a jump instruction (26 bit immediate added with the upper 4 bits of
	// the current pc address)
	uint JumpTarget() const { return (Target()<<2) + ((_Pc_+4) & 0xf0000000); }
	u32 BranchTarget() const { return (_Pc_+4) + (Imm() * 4); }

	u32 TrapCode() const { return (u16)(U32 >> 6); };
	
protected:
	
	IntSign32& HiValue() const { return iopRegs.GPR.n.hi; }
	IntSign32& LoValue() const { return iopRegs.GPR.n.lo; }
	IntSign32& FsValue() const { return iopRegs.CP0.r[_Rd_]; }

	// Applies the const flag to Rd based on the const status of Rs and Rt;
	void SetConstRd_OnRsRt();

	// Sets the link to the next instruction in the given GPR
	// (defaults to the link register if none specified)
	void SetLink();

	void SetLinkRd();
	
	// Used internally by branching instructions to signal that the following instruction
	// should be treated as a delay slot.
	void SetBranchInst();
	
	void DoBranch( u32 jumptarg );
	void DoBranch();
	void MultHelper( u64 result );

	// -------------------------------------------------------------------------
	// Helpers used to initialize the object state.

	u8 Funct() const { return U32 & 0x3F; }
	u8 Sa() const { return (U32 >> 6) & 0x1F; }
	u8 Rd() const { return (U32 >> 11) & 0x1F; }
	u8 Rt() const { return (U32 >> 16) & 0x1F; }
	u8 Rs() const { return (U32 >> 21) & 0x1F; }
	u8 Opcode() const { return U32 >> 26; }
	
protected:
	template< typename T > static void _dispatch_SPECIAL( T& Inst );
	template< typename T > static void _dispatch_REGIMM( T& Inst );
	template< typename T > static void _dispatch_COP0( T& Inst );
	template< typename T > static void _dispatch_COP2( T& Inst );

};

//////////////////////////////////////////////////////////////////////////////////////////
//                        IOP Interpreted Instruction Tables
//
class InstructionInterpreter : public Instruction
{
public:
	static void Process( Instruction& inst )
	{
		Instruction::Process( (InstructionInterpreter&) inst );
	}

public:
	INSTRUCTION_API(void, )
	
protected:
	// used by MULT and MULTU.
	void MultHelper( u64 result );
};

//////////////////////////////////////////////////////////////////////////////////////////
//                         IOP Diagnostic Information Tables
//
class InstructionDiagnostic : public Instruction
{
public:
	static void Process( Instruction& inst )
	{
		Instruction::Process( (InstructionDiagnostic&) inst );
	}

protected:
	InstDiagInfo MakeInfoS( const char* name, ParamType one=Param_None, ParamType two=Param_None, ParamType three=Param_None ) const;
	InstDiagInfo MakeInfoU( const char* name, ParamType one=Param_None, ParamType two=Param_None, ParamType three=Param_None ) const;

public:
	INSTRUCTION_API(void, )
};

}
