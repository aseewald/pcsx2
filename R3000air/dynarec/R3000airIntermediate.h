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

namespace R3000A {

// ------------------------------------------------------------------------
//
enum IrInstructionId
{
	#pragma region Comparison
	// Generates code for branching to a target which is a known const address.  The
	// resulting codegen is a static link or, in some cases, no code at all (working on
	// the assumption that the following block in execution is compiled directly
	// after this block).
	// [supports both conditional and unconditional branching]
	IrInst_Branch,
	// Generates code for branching to a target which cannot be determined until runtime.
	// (supports both conditional and unconditional brands of such branches)
	IrInst_BranchReg,
	IrInst_Set,
	IrInst_SetImm,
	#pragma endregion // Instructions which use CompareType

	#pragma region Arithmetic
	IrInst_Add,
	IrInst_Sub,
	IrInst_AddImm,
	IrInst_ShiftLeft,
	IrInst_ShiftRight,
	IrInst_ShiftLeftImm,
	IrInst_ShiftRightImm,
	IrInst_Mult,
	IrInst_Div,
	#pragma endregion

	#pragma region Bitwise
	IrInst_And,
	IrInst_Or,
	IrInst_Xor,
	IrInst_Nor,
	IrInst_AndImm,
	IrInst_OrImm,
	IrInst_XorImm,
	#pragma endregion
	
	#pragma region MemoryOps
	// Address translation prefix, typically paired with a single memory operation but
	// sometimes used as a single translation for many subsequent operations that use
	// the same base register indexer.
	IrInst_TranslateAddr,

	IrInst_LoadByte,
	IrInst_LoadHalfword,
	IrInst_LoadWord,
	IrInst_LoadWordLeft,
	IrInst_LoadWordRight,
	
	IrInst_StoreByte,
	IrInst_StoreHalfword,
	IrInst_StoreWord,
	IrInst_StoreWordLeft,
	IrInst_StoreWordRight,
	#pragma endregion

	#pragma region RegisterMovement
	// (these are handled entirely at the register mapping / writeback level)
	//Inst_MoveFromLo,
	//Inst_MoveFromHi,
	//Inst_MoveToLo,
	//Inst_MoveToHi,

	IrInst_MFC0,
	IrInst_MTC0,
	#pragma endregion // To/From COP0 and Hi/Lo

	#pragma region Miscellaneous

	// Raises an immediate exception, used for SYSCALL, BREAK, and TRAP (if supported)
	IrInst_Exception,

	// Non-general implementation of RFE (fully 1:1 correlation to MIPS)
	IrInst_RFE,
	
	// Generates code for performing DivUnit stalls.
	IrInst_DivUnitStall,
	
	// Implements the ZeroEx handler call, which performs IOP Bios bootup progress/
	// status dumps.
	IrInst_ZeroEx,
	
	// IR-level NOPs do have a purpose!  They serve as register mapping markers,
	// most typically used by MoveTo/MoveFrom instructions to map reigsters accordingly.
	IrInst_NOP,
	#pragma endregion // misc instructions that have no better home
};

// ------------------------------------------------------------------------
// MIPS branch comparison types.
// This is basically a limited subset of the comparison types available on the x86
// CPUs (minus sign/overflow/parity).  For simplicity of x86 translation I've left
// the numerical assignments match those of the x86 types so that a direct typecast
// translation suffices.
//
enum BccComparisonType
{
	Bcc_Unconditional	= -1,
	Bcc_Below			= 0x2,
	Bcc_AboveOrEqual	= 0x3,
	Bcc_Equal			= 0x4,
	Bcc_NotEqual		= 0x5,
	Bcc_BelowOrEqual	= 0x6,
	Bcc_Above			= 0x7,
	Bcc_Less			= 0xc,
	Bcc_GreaterOrEqual	= 0xd,
	Bcc_LessOrEqual		= 0xe,
	Bcc_Greater			= 0xf,
};

//////////////////////////////////////////////////////////////////////////////////////////
//
struct GprConstStatus
{
	// [5 bytes to contain 34 bits]
	u8 m_Bits[5];

	GprConstStatus()
	{
		memzero_obj( m_Bits );
		Set( GPR_r0 );			// gpr 0 is always const
	}

	const bool operator[]( MipsGPRs_t gpridx ) const
	{
		int idx = (int)gpridx;
		return !!( m_Bits[idx/8] & (1<<(idx&7)) );
	}
	
	void Set( MipsGPRs_t gpridx )
	{
		int idx = (int)gpridx;
		m_Bits[idx/8] |= 1<<(idx&7);
	}
	
	void Clear( MipsGPRs_t gpridx )
	{
		int idx = (int)gpridx;
		m_Bits[idx/8] &= ~(1<<(idx&7));
	}

	void Set( MipsGPRs_t gpridx, bool status )
	{
		if( status )
			Set( gpridx );
		else
			Clear( gpridx );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
// InstructionConstOptimizer : This class's MIPS decoder implementation collects complete constant
// propagation information.
//
class InstructionConstOptimizer : public InstructionDiagnostic
{
public:
	// Const status of all GPRs; storage of all GPRs is done to allow the recompiler the ability
	// to track complete const register save/restore status on a per-instruction basis (assists
	// in advanced exception handling and debugging).
	GprConstStatus IsConstGpr;

	// ConstVal - Stores constant status for all GPRs for this instruction.
	// Note: I store all 34 GPRs on purpose, even tho the instruction itself will only
	// need to know it's own regfields at dyngen time.  The rest are used for generating
	// register state info for advanced exception handling and recovery from recompiled
	// code.
	u32 ConstVal[34];

protected:
	// Exception code raised by the instruction; valid only if m_IsConstException is true.
	u32 m_ConstExceptionCode;

	// Indicates if the instruction causes exceptions with certainty
	bool m_IsConstException:1;
	
	// flagged TRUE for instructions that branch unconditionally (m_VectorPC is a known const)
	bool m_IsConstPc:1;

public:
	InstructionConstOptimizer() {}

	InstructionConstOptimizer( const Opcode& opcode ) :
		InstructionDiagnostic( opcode )
	,	IsConstGpr()
	,	m_IsConstException( false )
	,	m_IsConstPc( true )
	{
	}

	void Assign( const Opcode& opcode, const GprConstStatus& constStatus );

public:
	void Process();

	bool UpdateExternalConstStatus( GprConstStatus& gpr_IsConst ) const;

	bool IsConstRs() const { return ReadsRs() && IsConstGpr[_Rs_]; }
	bool IsConstRt() const { return ReadsRt() && IsConstGpr[_Rt_]; }
	bool IsConstField( RegField_t field ) const;
	bool IsConstPc() const { return m_IsConstPc; }
	
	bool CausesConstException() const { return m_IsConstException; }
	
protected:
	void DoConditionalBranch( bool cond );
	void RaiseException( uint code );
	bool ConditionalException( uint code, bool cond );
};

//////////////////////////////////////////////////////////////////////////////////////////
// First-stage IntermediateRepresentation; cpu-independent instruction decoding and info
// expansion (and reduction in some cases) over the more raw MIPS-form InstructionConstOptimizer.
//
class IntermediateRepresentation
{
public:
	void DynarecAssert( bool condition, const char* msg, bool inReleaseMode=false ) const;

public:
	// The original MIPS instruction state recorded during first pass interpretation.
	// (includes const status, VectorPC, and similar)
	const InstructionConstOptimizer& MipsInst;

	// Intermediate Instruction Identifier, used for dispatching and generating IR code
	// during second/third pass recompilation.
	IrInstructionId	InstId;
	
	// Comparison type used by Compare and Set instructions
	BccComparisonType CompareType;

	// Branch target used by branching instructions.
	u32 BranchTarget;

	MipsGPRs_t DestGpr;

	// Set true when the following instruction has a read dependency on the Rd gpr
	// of this instruction.  The regmapper will map the value of Rd prior to instruction
	// execution to a fixed register (and preserve it).  The next instruction in the 
	// list will read Rs or Rt from the Dependency slot instead.
	bool DelayedDependencyRd;

	// Intermediate Representation dependency indexer, for each valid readable field.
	// The values of this array are indexes for the instructions that the field being
	// read is dependent on.  When re-ordering instructions, this instruction must be
	// ordered *after* any instruction listed here.
	// [TODO: not implemented yet -- instruction reordering comes with side effects
	//  such as breaking the ability to have accurate dynamic exception handling]
	//int iRepDep[RF_Count];

	IntermediateRepresentation() :
		MipsInst( *(InstructionConstOptimizer*)0 )
	,	DestGpr( GPR_r0 )
	{
	}

	IntermediateRepresentation( const InstructionConstOptimizer& src, IrInstructionId id, MipsGPRs_t destgpr=GPR_r0 ) : 
		MipsInst( src )
	,	InstId( id )
	,	DestGpr( destgpr )
	{
	}

	IntermediateRepresentation( const InstructionConstOptimizer& src, IrInstructionId id, BccComparisonType bcc, u32 target ) : 
		MipsInst( src )
	,	InstId( id )
	,	CompareType( bcc )
	,	BranchTarget( target )
	,	DestGpr( GPR_r0 )
	{
	}
	
	MipsGPRs_t ReadsField( RegField_t field ) const
	{
		return MipsInst.ReadsField( field );
	}

	MipsGPRs_t WritesField( RegField_t field ) const
	{
		return MipsInst.WritesField( field );
	}
	
	int GetConstRs() const
	{
		jASSUME( MipsInst.IsConstRs() );
		return MipsInst.ConstVal[MipsInst._Rs_];
	}

	int GetConstRt() const
	{
		jASSUME( MipsInst.IsConstRt() );
		return MipsInst.ConstVal[MipsInst._Rt_];
	}
};

}