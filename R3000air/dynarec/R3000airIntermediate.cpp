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
#include "IopCommon.h"

#include "iR3000air.h"

#include "../R3000airInstruction.inl"
#include "../R3000airOpcodeImpl.inl"
#include "../R3000airOpcodeDispatcher.inl"

using namespace x86Emitter;

namespace R3000A {

iopRec_FirstPassConstAnalysis m_blockspace;

// ------------------------------------------------------------------------
// DynarecAssert: Generates an exception if the given condition is not true.  Exception's
// description contains "IOPrec assumption failed on instruction 'INST': [usr msg]".
//
// inReleaseMode - Exception is generated in Devel and Debug builds only by default.
//   Pass 'true' as the inReleaseMode parameter to enable exception checking in Release
//   builds as well (ie, all builds).
//
void R3000A::IntermediateRepresentation::DynarecAssert( bool condition, const char* msg, bool inReleaseMode ) const
{
	// [TODO]  Add a new exception type that allows specialized handling of dynarec-level
	// exceptions, so that the exception handler can save the state of the emulation to
	// a special savestate.  Such a savestate *should* be fully intact since dynarec errors
	// occur during codegen, and prior to executing bad code.  Thus, once such bugs are
	// fixed, the emergency savestate can be resumed successfully. :)
	//
	// [TODO]  Add a recompiler state/info dump to this, so that we can log PC, surrounding
	// code, and other fun stuff!

	if( (inReleaseMode || IsDevBuild) && !condition )
	{
		throw Exception::AssertionFailure( fmt_string(
			"IOPrec assertion failed on instruction '%s': %s", MipsInst.GetName(), msg
		) );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
class IrMipsDecoder
{
public:
	// The original MIPS instruction state recorded during first pass interpretation.
	// (includes const status, VectorPC such)
	const InstructionConstOptimizer& MipsInst;

	// Target list where IR instructions are written.
	SafeList<IntermediateRepresentation>& irList;
	
	u32 _Rt_;		// needed by OpcodeDispatcher
	u32 _Rs_;		// needed by OpcodeDispatcher

	IrMipsDecoder( const InstructionConstOptimizer& mipsinst, SafeList<IntermediateRepresentation>& destlist ) :
		MipsInst( mipsinst )
	,	irList( destlist )
	,	_Rt_( mipsinst._Rt_ )
	,	_Rs_( mipsinst._Rs_ )
	{
		OpcodeDispatcher( *this );
	}

	void Add( IrInstructionId id )
	{
		new (&irList.New()) IntermediateRepresentation( MipsInst, id );

		if( MipsInst.GetDivStall() != 0 )
		{
			new (&irList.New()) IntermediateRepresentation( MipsInst, IrInst_DivUnitStall );
		}
	}
	
	void Add( IrInstructionId id, BccComparisonType bcc )
	{
		new (&irList.New()) IntermediateRepresentation( MipsInst, id );
		irList.GetLast().CompareType = bcc;
	}
	
	// ------------------------------------------------------------------------
	//
	void AddJump( u32 target )
	{
		// Rule of the First Pass Interpreter: Unconditional jump targets can only come in one of two forms:
		//  * Known target that's been compiled to x86 code.
		//  * Known target that has been run processed into IR.
		// In the second scenario, the targeted block will be compiled directly beneath this one, so
		// no actual branch code needs to be generated.

		if( g_PersState.xGetBlockPtr( target ) != NULL )
		{
			// block path already exists, so just statically link to it.

			new (&irList.New())
				IntermediateRepresentation( MipsInst, IrInst_Branch, Bcc_Unconditional, MipsInst.JumpTarget() );
		}
	}

	// ------------------------------------------------------------------------
	//
	void AddJumpReg()
	{
		if( MipsInst.IsConstRs() )
			AddJump( MipsInst.ConstVal[MipsInst._Rs_] );
		else
		{
			new (&irList.New())
				IntermediateRepresentation( MipsInst, IrInst_BranchReg );
		}
	}

	// ------------------------------------------------------------------------
	//
	void AddBranch( BccComparisonType bcc )
	{
		if( MipsInst.IsConstBranch() )
		{
			// Conditional Branching Override: A lot of MIPS code likes to use conditional branch
			// instructions and test against the zero register, instead of using the J instruction.
			// (some are optimized by the first pass interpreters, others are handled here)

			AddJump( MipsInst.BranchTarget() );
			return;
		}

		// First-pass interpreter has "discovered" which path the branch is taking, and we'll
		// assume that this is the common path.  The codegen for the common path can be inverted
		// depending on the state of the blocks being linked:
		//
		//   (a) Does the preferred path target a block already compiled in x86 space?
		//      yes -> Link directly to the existing block.
		//   (b) Does the preferred path target a block with existing IR compilation?
		//      yes -> Link to the non-preferred path and compile preferred path *directly*
		//             below this block (no jump linking needed) [inverts conditional]
		//
		// I set up vars for Condition (A), and then test for (B) and invert the conditional if
		// (B) is met.

		u32 pcFallthru		= MipsInst._Pc_ + 8;
		u32 pcTarget		= MipsInst.BranchTarget();
		bool branchTaken	= (MipsInst.GetVectorPC() != pcFallthru);

		if( g_PersState.xGetBlockPtr( pcTarget ) == NULL )
		{
			branchTaken = !branchTaken;
		}

		// [TODO] : Recompiler Inlining Heuristics!  Look up the blocks and check their IR code
		// length combined with this IR's code length.  Inlining might be a favorable option if
		// the linked-to block is short (weighted along with this one).

		new (&irList.New()) IntermediateRepresentation( MipsInst, IrInst_Branch,
			(BccComparisonType)((int)bcc ^ (int)branchTaken), branchTaken ? pcFallthru : pcTarget
		);
		
		u32 fallthruTarget = !branchTaken ? pcFallthru : pcTarget;

		if( g_PersState.xGetBlockPtr( fallthruTarget ) != NULL )
		{
			// Fallthrough path already exists, so just statically link to it.

			new (&irList.New()) IntermediateRepresentation( MipsInst, IrInst_Branch,
				Bcc_Unconditional, fallthruTarget
			);
		}
		else
		{
			// 
			//jASSUME( g_PersState.xGetBlockIR( altTarget ) != NULL );
		}

	}
	
	void SetName( const char* name ) { }
	u32 Basecode() const { return MipsInst.Basecode(); }
	u32 Funct() const { return MipsInst.Funct(); }

	INSTRUCTION_API()
};


__releaseinline void IrMipsDecoder::BGEZ()	// Branch if Rs >= 0
{
	AddBranch( Bcc_GreaterOrEqual );
}

__releaseinline void IrMipsDecoder::BGTZ()	// Branch if Rs >  0
{
	AddBranch( Bcc_GreaterOrEqual );
}

__releaseinline void IrMipsDecoder::BLEZ()	// Branch if Rs <= 0
{
	AddBranch( Bcc_LessOrEqual );
}

__releaseinline void IrMipsDecoder::BLTZ()	// Branch if Rs <  0
{
	AddBranch( Bcc_Less );
}

__releaseinline void IrMipsDecoder::BGEZAL()	// Branch if Rs >= 0 and link
{
	BGEZ();
}

__releaseinline void IrMipsDecoder::BLTZAL()	// Branch if Rs <  0 and link
{
	BLTZ();
}

__releaseinline void IrMipsDecoder::BEQ()		// Branch if Rs == Rt
{
	AddBranch( Bcc_Equal );
}

__releaseinline void IrMipsDecoder::BNE()		// Branch if Rs != Rt
{
	AddBranch( Bcc_NotEqual );
}

__releaseinline void IrMipsDecoder::J()
{
	AddJump( MipsInst.JumpTarget() );
}

__releaseinline void IrMipsDecoder::JAL()
{
	AddJump( MipsInst.JumpTarget() );
}

__releaseinline void IrMipsDecoder::JR()
{
	AddJumpReg();
}

__releaseinline void IrMipsDecoder::JALR()
{
	AddJumpReg();
}

// Rt = Rs + Im 	(Exception on Integer Overflow)
__releaseinline void IrMipsDecoder::ADDI()
{
	Add( IrInst_AddImm );
}

__releaseinline void IrMipsDecoder::ADDIU()
{
	if( IsDevBuild )
		Add( IrInst_ZeroEx );
	Add( IrInst_AddImm );
}

__releaseinline void IrMipsDecoder::ANDI()	// Rt = Rs And Im
{
	Add( IrInst_AndImm );
}

__releaseinline void IrMipsDecoder::ORI()		// Rt = Rs Or  Im
{
	Add( IrInst_OrImm );
}

__releaseinline void IrMipsDecoder::XORI()	// Rt = Rs Xor Im
{
	Add( IrInst_XorImm );
}

__releaseinline void IrMipsDecoder::SLTI()	// Rt = Rs < Im		(Signed)
{
	Add( IrInst_SetImm, Bcc_Less );
}

__releaseinline void IrMipsDecoder::SLTIU()	// Rt = Rs < Im		(Unsigned)
{
	Add( IrInst_SetImm, Bcc_Below );
}

// Rd = Rs + Rt		(Exception on Integer Overflow)
__instinline void IrMipsDecoder::ADD()
{
	Add( IrInst_Add );
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
__instinline void IrMipsDecoder::SUB()
{
	Add( IrInst_Sub );
}

__instinline void IrMipsDecoder::ADDU()	// Rd = Rs + Rt
{
	Add( IrInst_Add );
}

__instinline void IrMipsDecoder::SUBU()	// Rd = Rs - Rt
{
	Add( IrInst_Sub );
}

__instinline void IrMipsDecoder::AND()		// Rd = Rs And Rt
{
	Add( IrInst_And );
}

__instinline void IrMipsDecoder::OR()		// Rd = Rs Or  Rt
{
	Add( IrInst_Or );
}

__instinline void IrMipsDecoder::XOR()		// Rd = Rs Xor Rt
{
	Add( IrInst_Xor );
}

__instinline void IrMipsDecoder::NOR()		// Rd = Rs Nor Rt
{
	Add( IrInst_Nor );
}

__instinline void IrMipsDecoder::SLT()		// Rd = Rs < Rt		(Signed)
{
	Add( IrInst_Set, Bcc_Less );
}

__instinline void IrMipsDecoder::SLTU()	// Rd = Rs < Rt		(Unsigned)
{
	Add( IrInst_Set, Bcc_Below );
}

__instinline void IrMipsDecoder::DIV()
{
	Add( IrInst_Div );
}

__instinline void IrMipsDecoder::DIVU()
{
	Add( IrInst_Div );
}

__instinline void IrMipsDecoder::MULT()
{
	Add( IrInst_Mult );
}

__instinline void IrMipsDecoder::MULTU()
{
	Add( IrInst_Mult );
}

__instinline void IrMipsDecoder::SLL()		// Rd = Rt << sa
{
	Add( IrInst_ShiftLeft );
}

__instinline void IrMipsDecoder::SRA()		// Rd = Rt >> sa (arithmetic) [signed]
{
	Add( IrInst_ShiftRight );
}

__instinline void IrMipsDecoder::SRL()		// Rd = Rt >> sa (logical) [unsigned]
{
	Add( IrInst_ShiftRight );
}

__instinline void IrMipsDecoder::SLLV()	// Rd = Rt << rs
{
	Add( IrInst_ShiftLeft );
} 

__instinline void IrMipsDecoder::SRAV()	// Rd = Rt >> rs (arithmetic)
{
	Add( IrInst_ShiftRight );
}

__instinline void IrMipsDecoder::SRLV()	// Rd = Rt >> rs (logical)
{
	Add( IrInst_ShiftRight );
}

__instinline void IrMipsDecoder::LUI()	// Rt = Im << 16  (lower 16 bits zeroed)
{
	DevAssert( false, "IOPrec Logic Error: LUI encountered at IR stage (instructio should always be const-optimized!)" );
}

__instinline void IrMipsDecoder::MFHI()	// Rd = Hi
{
	Add( IrInst_NOP );
}

__instinline void IrMipsDecoder::MFLO()	 // Rd = Lo
{
	Add( IrInst_NOP );
}

__instinline void IrMipsDecoder::MTHI()	// Hi = Rs
{
	Add( IrInst_NOP );
}

__instinline void IrMipsDecoder::MTLO()	// Lo = Rs
{
	Add( IrInst_NOP );
}

__instinline void IrMipsDecoder::BREAK()
{
	Add( IrInst_Exception );
}

__instinline void IrMipsDecoder::SYSCALL()
{
	Add( IrInst_Exception );
}

__instinline void IrMipsDecoder::RFE()
{
	Add( IrInst_RFE );
}

__instinline void IrMipsDecoder::LB()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadByte );
}

__instinline void IrMipsDecoder::LBU()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadByte );
}

__instinline void IrMipsDecoder::LH()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadHalfword );
}

__instinline void IrMipsDecoder::LHU()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadHalfword );
}

__instinline void IrMipsDecoder::LW()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadWord );
}

__instinline void IrMipsDecoder::LWL()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadWordLeft );
}

__instinline void IrMipsDecoder::LWR()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_LoadWordRight );
}

__instinline void IrMipsDecoder::SB()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_StoreByte );
}

__instinline void IrMipsDecoder::SH()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_StoreHalfword );
}

__instinline void IrMipsDecoder::SW()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_StoreWord );

}

__instinline void IrMipsDecoder::SWL()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_StoreWordLeft );
}

__instinline void IrMipsDecoder::SWR()
{
	Add( IrInst_TranslateAddr );
	Add( IrInst_StoreWordRight );
}

__instinline void IrMipsDecoder::MFC0()
{
	Add( IrInst_MFC0 );
}

__instinline void IrMipsDecoder::CFC0()
{
	Add( IrInst_MFC0 );
}

__instinline void IrMipsDecoder::MTC0()
{
	Add( IrInst_MTC0 );
}

__instinline void IrMipsDecoder::CTC0()
{
	Add( IrInst_MTC0 );
}

__instinline void IrMipsDecoder::Unknown()
{
	// already handled by first pass interps
}

// ------------------------------------------------------------------------
// Reordering instructions simply involves moving instructions, when possible, so that
// registers written are read back as soon as possible.  Typically this will allow the
// register mapper to optimize things better, since registers will have a higher % of
// reuse between instructions.
//
// Re-ordering non-const_address memory operations with other non-const_address memory
// ops is expressly prohibited because reads from and writes to registers can cause 
// changes in behavior.  Const memory operations can be re-ordered but only if they
// are direct memory operations.
//
void _recIR_Reorder()
{

}
// ------------------------------------------------------------------------
// Reorders all delay slots with the branch instructions that follow them.
//
void iopRec_IntermediateState::ReorderDelaySlots()
{
	// Note: ignore the last instruction, since if it's a branch it clearly doesn't have
	// a delay slot (it's a NOP that was optimized away).

	const uint LengthOneLess( GetLength()-1 );
	for( uint i=0; i<LengthOneLess; ++i )
	{
		IntermediateRepresentation& ir( inst[i] );

		if( ir.MipsInst.HasDelaySlot() && inst[i+1].MipsInst.IsDelaySlot() )
		{
			IntermediateRepresentation& delayslot( inst[i+1] );

			// Check for branch dependency on Rd.

			bool isDependent = false;
			if( delayslot.MipsInst.WritesReg( ir.MipsInst.ReadsField( RF_Rs ) ) != RF_Unused )
				isDependent = true;
			if( delayslot.MipsInst.WritesReg( ir.MipsInst.ReadsField( RF_Rt ) ) != RF_Unused )
				isDependent = true;

			delayslot.DelayedDependencyRd = isDependent;

			// Reorder instructions :D

			InstOrder[i] = i+1;
			InstOrder[i+1] = i;
			++i;
		}
		else
		{
			InstOrder[i] = i;
		}
	}

	InstOrder[LengthOneLess] = LengthOneLess;
}

// ------------------------------------------------------------------------
//
void iopRec_IntermediateState::GenerateIR( const recBlockItem& block )
{
	const int numinsts = block.InstOptInfo.GetLength();

	for( int i=0; i<numinsts; ++i )
	{
		IrMipsDecoder( block.InstOptInfo[i], inst );
	}
	
	ReorderDelaySlots();
}

}
