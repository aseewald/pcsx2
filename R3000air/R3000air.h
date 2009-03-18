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

#define __instinline __releaseinline		// MSVC still fails to inline as much as it should

union IntSign32
{
	s32 SL;
	u32 UL;

	s16 SS[2];
	u16 US[2];

	s8 SB[2];
	u8 UB[2];
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

	u32 pc;				// Program counter for the next instruction fetch
	u32 VectorPC;		// pc to vector to after the next instruction fetch
	bool IsDelaySlot;
	
	u32 cycle;
	u32 interrupt;
	u32 sCycle[32];		// start cycle for signaled ints
	s32 eCycle[32];		// cycle delta for signaled ints (sCycle + eCycle == branch cycle)

	s32 NextCounter;
	u32 NextsCounter;

	// Controls when branch tests are performed.
	u32 NextBranchCycle;
	
	u32 DivUnitCycles;	// number of cycles pending on the current div unit instruction (mult and div both run in the same unit)

	// old wacky data related to the PS1 GPU?
	//u32 _msflag[32];
	//u32 _smflag[32];
	
	// Sets a new PC in "abrupt" fashion (without consideration for delay slot).
	// Effectively cancels the delay slot instruction, making this ideal for use
	// in raising exceptions.
	void SetExceptionPC( u32 newpc )
	{
		//pc = newpc;
		VectorPC = newpc;
		IsDelaySlot = false;
	}
	
};

PCSX2_ALIGNED16_EXTERN(Registers iopRegs);

union GprStatus
{
	union
	{
		bool Value;

		struct  
		{
			bool Rd:1;
			bool Rs:1;
			bool Rt:1;
			bool Fs:1;		// the COP0 register as a source/dest
			bool Hi:1;
			bool Lo:1;
			bool Link:1;	// ra - the link register
			bool Memory:1;
		};
	};
};

#undef _Funct_
#undef _Basecode_
#undef _Rd_
#undef _Rs_
#undef _Rt_
#undef _Sa_
#undef _PC_
#undef _Opcode_

//////////////////////////////////////////////////////////////////////////////////////////
//
#define INSTRUCTION_API() \
	void BGEZ(); \
	void BGEZAL(); \
	void BGTZ(); \
	void BLEZ(); \
	void BLTZ(); \
	void BLTZAL(); \
	void BEQ(); \
	void BNE(); \
 \
	void J(); \
	void JAL(); \
	void JR(); \
	void JALR(); \
 \
	void ADDI(); \
	void ADDIU(); \
	void ANDI(); \
	void ORI(); \
	void XORI(); \
	void SLTI(); \
	void SLTIU(); \
 \
	void ADD(); \
	void ADDU(); \
	void SUB(); \
	void SUBU(); \
	void AND(); \
	void OR(); \
	void XOR(); \
	void NOR(); \
	void SLT(); \
	void SLTU(); \
 \
	void DIV(); \
	void DIVU(); \
	void MULT(); \
	void MULTU(); \
 \
	void SLL(); \
	void SRA(); \
	void SRL(); \
	void SLLV(); \
	void SRAV(); \
	void SRLV(); \
	void LUI(); \
 \
	void MFHI(); \
	void MFLO(); \
	void MTHI(); \
	void MTLO(); \
 \
	void BREAK(); \
	void SYSCALL(); \
	void RFE(); \
 \
	void LB(); \
	void LBU(); \
	void LH(); \
	void LHU(); \
	void LW(); \
	void LWL(); \
	void LWR(); \
 \
	void SB(); \
	void SH(); \
	void SW(); \
	void SWL(); \
	void SWR(); \
 \
	void MFC0(); \
	void CFC0(); \
	void MTC0(); \
	void CTC0(); \
	void Unknown();


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
//
struct Opcode
{
	u32 U32;

	u8 Funct() const { return U32 & 0x3F; }
	u8 Sa() const { return (U32 >> 6) & 0x1F; }
	u8 Rd() const { return (U32 >> 11) & 0x1F; }
	u8 Rt() const { return (U32 >> 16) & 0x1F; }
	u8 Rs() const { return (U32 >> 21) & 0x1F; }
	u8 Basecode() const { return U32 >> 26; }

	// Returns the target portion of the opcode (26 bit immediate)
	uint Target() const { return U32 & 0x03ffffff; }

	// Sign-extended immediate
	s32 Imm() const { return (s16)U32; }

	// Zero-extended immediate
	u32 ImmU() const { return (u16)U32; }
	u32 TrapCode() const { return (u16)(U32 >> 6); };

	Opcode( u32 src ) :
		U32( src ) {}
};

//////////////////////////////////////////////////////////////////////////////////////////
// This object is immutable.  Do not modify, just create new ones. :)
// See R3000AirInstruction.cpp for code implementations.
//
// Note on R3000A cycle counting: it's best to assume *all* R3000A instructions as a single
// cycle.  Loads have a delay slot (NOP), and the mult and div instructions have their own
// pipeline that runs in parallel to the rest of the CPU (typically mult/div are paired
// with a 13 instruction computation of overflow).
//
struct Instruction 
{
public:
	// ------------------------------------------------------------------------
	// Precached values for the instruction (makes debugging and processing simpler).

	const Opcode _Opcode_;	// the instruction opcode

	const u32 _Rd_;
	const u32 _Rt_;
	const u32 _Rs_;
	const u32 _Pc_;		// program counter for this specific instruction

protected:
	// ------------------------------------------------------------------------
	// Interpretation status vars, updated post-Process, and accessable via read-only
	// public accessors.

	u32 m_NextPC;		// new PC after instruction has finished execution.
	u32 m_DivStall;		// indicates the cycle stall of the current instruction if the DIV pipe is already in use.
	bool m_IsBranchType;

public:
	Instruction( const Opcode& opcode ) :
		_Opcode_( opcode )
	,	_Rd_( opcode.Rd() )
	,	_Rt_( opcode.Rt() )
	,	_Rs_( opcode.Rs() )
	,	_Pc_( iopRegs.pc )
	,	m_NextPC( iopRegs.VectorPC + 4 )
	,	m_DivStall( 0 )
	,	m_IsBranchType( false )
	{
	}

public:
	// ------------------------------------------------------------------------
	// Public Instances Methods and Functions

	template< typename T> static void Process( T& Inst );

	// Returns true if this instruction is a branch/jump type (which means it
	// will have a delay slot which should execute prior to the jump but *after*
	// the branch instruction's target has been calculated).
	const bool IsBranchType() const;

	const u32 GetNextPC() const { return m_NextPC; }

	const u32 GetDivStall() const { return m_DivStall; }

	// ------------------------------------------------------------------------
	// APIs for grabbing portions of the opcodes.  These are just passthrough
	// functions to the Opcode type.

	u32 Funct() const { return _Opcode_.Funct(); }
	u32 Basecode() const { return _Opcode_.Basecode(); }
	u32 Sa() const { return _Opcode_.Sa(); }

	// Returns the target portion of the opcode (26 bit immediate)
	uint Target() const { return _Opcode_.Target(); }

	// Sign-extended immediate
	s32 Imm() const { return _Opcode_.Imm(); }

	// Zero-extended immediate
	u32 ImmU() const { return _Opcode_.ImmU(); }

	u32 TrapCode() const { return _Opcode_.TrapCode(); }

	// Calculates the target for a jump instruction (26 bit immediate added with the upper 4 bits of
	// the current pc address)
	uint JumpTarget() const { return (Target()<<2) + ((_Pc_+4) & 0xf0000000); }
	u32 BranchTarget() const { return (_Pc_+4) + (Imm() * 4); }
	u32 AddrImm() { return GetRs().UL + Imm(); }
	
	// Sets the link to the next instruction in the link register (Ra)
	void SetLink();

	// Sets the link to the next instruction into the Rd register
	void SetLinkRd();
	
	// Used internally by branching instructions to signal that the following instruction
	// should be treated as a delay slot. (assigns psxRegs.IsDelaySlot)
	void SetBranchInst();

	void DoBranch( u32 jumptarg );
	void DoBranch();

	INSTRUCTION_API()

protected:
	void MultHelper( u64 result );

	// ------------------------------------------------------------------------
	// Register value retrieval methods.  Use these to read/write the registers
	// specified by the Opcode.  There are two types: the const "RegValue()" brand
	// which do not propagate const/optimization data, and the GetReg() form,
	// which is non-const and propagates optimization flags.

	const IntSign32 RdValue() const { return iopRegs.GPR.r[_Rd_]; }
	const IntSign32 RtValue() const { return iopRegs.GPR.r[_Rt_]; }
	const IntSign32 RsValue() const { return iopRegs.GPR.r[_Rs_]; }
	const IntSign32 HiValue() const { return iopRegs.GPR.n.hi; }
	const IntSign32 LoValue() const { return iopRegs.GPR.n.lo; }
	const IntSign32 FsValue() const { return iopRegs.CP0.r[_Rd_]; }

	// HiLo are always written as unsigned.
	void SetHiLo( u32 hi, u32 lo ) { SetLo_UL( lo ); SetHi_UL( hi ); }

	// ------------------------------------------------------------------------
	// Begin Virtual API
	
	virtual const IntSign32 GetRd() { return iopRegs.GPR.r[_Rd_]; }
	virtual const IntSign32 GetRt() { return iopRegs.GPR.r[_Rt_]; }
	virtual const IntSign32 GetRs() { return iopRegs.GPR.r[_Rs_]; }
	virtual const IntSign32 GetHi() { return iopRegs.GPR.n.hi; }
	virtual const IntSign32 GetLo() { return iopRegs.GPR.n.lo; }
	virtual const IntSign32 GetFs() { return iopRegs.CP0.r[_Rd_]; }

	virtual void SetRd_SL( s32 src ) { if(!_Rd_) return; iopRegs.GPR.r[_Rd_].SL = src; }
	virtual void SetRt_SL( s32 src ) { if(!_Rt_) return; iopRegs.GPR.r[_Rt_].SL = src; }
	virtual void SetRs_SL( s32 src ) { if(!_Rs_) return; iopRegs.GPR.r[_Rs_].SL = src; }
	virtual void SetHi_SL( s32 src ) { iopRegs.GPR.n.hi.SL = src; }
	virtual void SetLo_SL( s32 src ) { iopRegs.GPR.n.lo.SL = src; }
	virtual void SetFs_SL( s32 src ) { iopRegs.CP0.r[_Rd_].SL = src; }
	virtual void SetLink( u32 addr ) { iopRegs.GPR.n.ra.UL = addr; }

	virtual void SetRd_UL( u32 src ) { if(!_Rd_) return; iopRegs.GPR.r[_Rd_].UL = src; }
	virtual void SetRt_UL( u32 src ) { if(!_Rt_) return; iopRegs.GPR.r[_Rt_].UL = src; }
	virtual void SetRs_UL( u32 src ) { if(!_Rs_) return; iopRegs.GPR.r[_Rs_].UL = src; }
	virtual void SetHi_UL( u32 src ) { iopRegs.GPR.n.hi.UL = src; }
	virtual void SetLo_UL( u32 src ) { iopRegs.GPR.n.lo.UL = src; }
	virtual void SetFs_UL( u32 src ) { iopRegs.CP0.r[_Rd_].UL = src; }

	virtual u8  MemoryRead8( u32 addr );
	virtual u16 MemoryRead16( u32 addr );
	virtual u32 MemoryRead32( u32 addr );

	virtual void MemoryWrite8( u32 addr, u8 val );
	virtual void MemoryWrite16( u32 addr, u16 val );
	virtual void MemoryWrite32( u32 addr, u32 val );

	virtual void RaiseException( uint code );
	virtual void _RFE();	// virtual implementation of RFE
	
	// used to flag instructions which have a "critical" side effect elsewhere in emulation-
	// land -- such as modifying COP0 registers, or other functions which cannot be safely
	// optimized.
	virtual void HasSideEffects() const {}

	// -------------------------------------------------------------------------
	// Helpers used to initialize the object state.

	template< typename T > static void _dispatch_SPECIAL( T& Inst );
	template< typename T > static void _dispatch_REGIMM( T& Inst );
	template< typename T > static void _dispatch_COP0( T& Inst );
	template< typename T > static void _dispatch_COP2( T& Inst );
};


//////////////////////////////////////////////////////////////////////////////////////////
// This version of the instruction interprets the instruction and propagates optimization
// and const status along the way.
//
class InstructionOptimizer : public Instruction
{
protected:
	bool m_HasSideEffects;
	GprStatus m_ReadsGPR;
	GprStatus m_WritesGPR;

public:
	InstructionOptimizer( const Opcode& opcode ) :
		Instruction( opcode )
	,	m_HasSideEffects( false )
	{
		m_ReadsGPR.Value = false;
		m_WritesGPR.Value = false;
	}

protected:
	// ------------------------------------------------------------------------
	// Optimization / Const propagation API.
	// This API is not implemented to any functionality by default, so that the standard
	// interpreter won't have to do more work than is needed.  To enable the extended optimization
	// information, use an InstructionOptimizer instead.

	const IntSign32 GetRd() { m_ReadsGPR.Rd = true; return iopRegs.GPR.r[_Rd_]; }
	const IntSign32 GetRt() { m_ReadsGPR.Rt = true; return iopRegs.GPR.r[_Rt_]; }
	const IntSign32 GetRs() { m_ReadsGPR.Rs = true; return iopRegs.GPR.r[_Rs_]; }
	const IntSign32 GetHi() { m_ReadsGPR.Hi = true; return iopRegs.GPR.n.hi; }
	const IntSign32 GetLo() { m_ReadsGPR.Lo = true; return iopRegs.GPR.n.lo; }
	const IntSign32 GetFs() { m_ReadsGPR.Fs = true; return iopRegs.CP0.r[_Rd_]; }

	virtual void SetRd_SL( s32 src ) { if(!_Rd_) return; m_WritesGPR.Rd = true; iopRegs.GPR.r[_Rd_].SL = src; }
	virtual void SetRt_SL( s32 src ) { if(!_Rt_) return; m_WritesGPR.Rt = true; iopRegs.GPR.r[_Rt_].SL = src; }
	virtual void SetRs_SL( s32 src ) { if(!_Rs_) return; m_WritesGPR.Rs = true; iopRegs.GPR.r[_Rs_].SL = src; }
	virtual void SetHi_SL( s32 src ) { m_WritesGPR.Hi = true; iopRegs.GPR.n.hi.SL = src; }
	virtual void SetLo_SL( s32 src ) { m_WritesGPR.Lo = true; iopRegs.GPR.n.lo.SL = src; }
	virtual void SetFs_SL( s32 src ) { m_WritesGPR.Fs = true; iopRegs.CP0.r[_Rd_].SL = src; }
	virtual void SetLink( u32 addr ) { m_WritesGPR.Link = true; iopRegs.GPR.n.ra.UL = addr; }

	virtual void SetRd_UL( u32 src ) { if(!_Rd_) return; m_WritesGPR.Rd = true; iopRegs.GPR.r[_Rd_].UL = src; }
	virtual void SetRt_UL( u32 src ) { if(!_Rt_) return; m_WritesGPR.Rt = true; iopRegs.GPR.r[_Rt_].UL = src; }
	virtual void SetRs_UL( u32 src ) { if(!_Rs_) return; m_WritesGPR.Rs = true; iopRegs.GPR.r[_Rs_].UL = src; }
	virtual void SetHi_UL( u32 src ) { m_WritesGPR.Hi = true; iopRegs.GPR.n.hi.UL = src; }
	virtual void SetLo_UL( u32 src ) { m_WritesGPR.Lo = true; iopRegs.GPR.n.lo.UL = src; }
	virtual void SetFs_UL( u32 src ) { m_WritesGPR.Fs = true; iopRegs.CP0.r[_Rd_].UL = src; }

	virtual u8  MemoryRead8( u32 addr );
	virtual u16 MemoryRead16( u32 addr );
	virtual u32 MemoryRead32( u32 addr );

	virtual void MemoryWrite8( u32 addr, u8 val );
	virtual void MemoryWrite16( u32 addr, u16 val );
	virtual void MemoryWrite32( u32 addr, u32 val );

	void HasSideEffects() { m_HasSideEffects = true; }
};

//////////////////////////////////////////////////////////////////////////////////////////
//                         IOP Diagnostic Information Tables
//
class InstructionDiagnostic : public InstructionOptimizer
{
protected:
	const InstDiagInfo* m_Syntax;

public:
	InstructionDiagnostic( const Opcode& opcode ) :
		InstructionOptimizer( opcode )
	,	m_Syntax( NULL )
	{
	}

	__releaseinline void Process()
	{
		// Two phase processing for information collection:
		//  First phase uses the interpreter to collect register read/write information.
		//  Second phase uses the Diagnostic to collect parameter display order.
		
		Instruction::Process( (InstructionOptimizer&) *this );
		Instruction::Process( *this );
	}

	bool ParamIsRead( const uint pidx ) const;
	void GetParamName( const uint pidx, string& dest ) const;
	void GetParamValue( const uint pidx, string& dest ) const;

	void GetDisasm( string& dest ) const;
	void GetValuesComment( string& dest ) const;

protected:
	void SetRd_SL( s32 src ) { if(!_Rd_) return; m_WritesGPR.Rd = true; }
	void SetRt_SL( s32 src ) { if(!_Rt_) return; m_WritesGPR.Rt = true; }
	void SetRs_SL( s32 src ) { if(!_Rs_) return; m_WritesGPR.Rs = true; }
	void SetHi_SL( s32 src ) { m_WritesGPR.Hi = true; }
	void SetLo_SL( s32 src ) { m_WritesGPR.Lo = true; }
	void SetFs_SL( s32 src ) { m_WritesGPR.Fs = true; }
	void SetLink( u32 addr ) { m_WritesGPR.Link = true; }

	void SetRd_UL( u32 src ) { if(!_Rd_) return; m_WritesGPR.Rd = true; }
	void SetRt_UL( u32 src ) { if(!_Rt_) return; m_WritesGPR.Rt = true; }
	void SetRs_UL( u32 src ) { if(!_Rs_) return; m_WritesGPR.Rs = true; }
	void SetHi_UL( u32 src ) { m_WritesGPR.Hi = true; }
	void SetLo_UL( u32 src ) { m_WritesGPR.Lo = true; }
	void SetFs_UL( u32 src ) { m_WritesGPR.Fs = true; }

	u8  MemoryRead8( u32 addr )  { m_ReadsGPR.Memory = true; return 0; }
	u16 MemoryRead16( u32 addr ) { m_ReadsGPR.Memory = true; return 0; }
	u32 MemoryRead32( u32 addr ) { m_ReadsGPR.Memory = true; return 0; }

	void MemoryWrite8( u32 addr, u8 val )   { m_WritesGPR.Memory = true; }
	void MemoryWrite16( u32 addr, u16 val ) { m_WritesGPR.Memory = true; }
	void MemoryWrite32( u32 addr, u32 val ) { m_WritesGPR.Memory = true; }

	void RaiseException( uint code ) { }
	void _RFE() { }

protected:
	InstDiagInfo MakeInfoS( const char* name, ParamType one=Param_None, ParamType two=Param_None, ParamType three=Param_None ) const;
	InstDiagInfo MakeInfoU( const char* name, ParamType one=Param_None, ParamType two=Param_None, ParamType three=Param_None ) const;

public:
	INSTRUCTION_API()
};

}
