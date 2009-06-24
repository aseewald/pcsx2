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

namespace R3000A {

// ------------------------------------------------------------------------
// swapCond - optional parameter, when set to true, that swaps the comparison type
//   such that Less becomes GreaterOrEaqual, and Equal becomes NotEqual (etc).
//
JccComparisonType BccToJcc( BccComparisonType bcc, bool swapCond )
{
	return (JccComparisonType)( (int)bcc ^ (int)swapCond );
}

// ------------------------------------------------------------------------
//         W-T-Crap?  Time to generate x86 code?!  *scrreeech*
// ------------------------------------------------------------------------

extern u8* m_xBlock_CurPtr;

// Function called from the recompiler during Dev/Debug builds (when enabled) which
// verifies the status of const optimizations.  The validation checks are performed
// at the end of every block.
//
static void __fastcall ConstSelfCheckFailure( u32 blockpc, MipsGPRs_t gpridx )
{
	// Note: Const self-check failures are probably fatal, since it's possible that
	// corrupted const optimizations during the execution of the block have led to
	// faulty instruction processing.

	throw Exception::LogicError( fmt_string(
		"IOPrec Const Self-check failure @ 0x%08X on GPR %d[%s]",
		blockpc, gpridx, Diag_GetGprName( gpridx )
	) );
}

// ------------------------------------------------------------------------
// Pre-instruction Flush Stage
// Flush SrcMaps marked as requiring a flush (handles register mappings that
// get clobbered during instruction execution).
//
void iopRec_x86BlockState::_DynGen_PreInstructionFlush( const x86IntRep& ir, xRegisterArray32<bool>& xIsDirty )
{
	for( int gpr=1; gpr<34; ++gpr )
	{
		if( IsEdiMappedTo(gpr) ) continue;
		const xRegister32& xreg( ir.Src[gpr].GetReg() );
		if( xreg.IsEmpty() ) continue;

		if( ir.xForceFlush[xreg] && xIsDirty[xreg] )
		{
			xMOV( ir.Dest[gpr].GetMem(), xreg );
			xIsDirty[xreg] = false;
		}
	}
}

// ------------------------------------------------------------------------
// Special case "Strict Mode" handler, for Rs==Rt cases.
// This handler is needed because Strict Mode can map the same source GPR
// to many x86 registers.  Our SrcMap/DestMap system only supports mapping
// one x86 to each GPR.  Special cases needing more than one such handler
// should be dealt with here.
//
void x86IntRep::_DynGen_MapStrictMultimap() const
{
	if( !RegOpts.IsStrictMode ) return;

	int gpr_rs = Inst.ReadsField( RF_Rs );
	int gpr_rt = Inst.ReadsField( RF_Rt );

	if( (gpr_rs == gpr_rt) && (gpr_rs != -1) )
	{
		const xRegister32& rsmap( Src[gpr_rs].GetReg() );
		const RegMapInfo_Strict& stro( RegOpts.StatMap );
		DynarecAssert( !rsmap.IsEmpty(), "Strict mapping failure on Rs==Rt case; Both input fields are unmapped.", true );
		const xRegister32& destreg( (rsmap != stro.EntryMap[RF_Rs]) ? stro.EntryMap[RF_Rs] : stro.EntryMap[RF_Rt] );

		// Sanity check: Make sure the instruction's requested mapping for
		// RF_Rt is sufficiently unbound and unloaded.  Even though the mapper
		// fails to map both registers to the same GPR, it should still succeed
		// in unmapping them from any other GPRs.

		DynarecAssert( !IsMappedReg( Src, destreg ), "Strict mapper failed to unmap needed registers for Rs==Rt case.", true );

		if( rsmap != stro.EntryMap[RF_Rs] )
			xMOV( stro.EntryMap[RF_Rs], rsmap );
		else
			xMOV( stro.EntryMap[RF_Rt], rsmap );
	}
}

// ------------------------------------------------------------------------
//
void iopRec_x86BlockState::_DynGen_MapRegisters( const x86IntRep& ir, const x86IntRep& previr, xRegisterArray32<bool>& xIsDirty )
{
	xDirectOrIndirect32 prevmap[34];
	memcpy_fast( prevmap, previr.Dest, sizeof( prevmap ) );
	
	// (note: zero reg is included -- needed for some store or Div instructions,
	// but should otherwise be unmapped for anything else since Dynamic allocators
	// won't bother to map consts in any case.)

	for( int dg=0; dg<34; ++dg )
	{
		const MipsGPRs_t dgpr = (MipsGPRs_t)dg;
		const xDirectOrIndirect32* target = ir.Src;
		if( prevmap[dgpr] == target[dgpr] ) continue;

		if( target[dgpr].IsDirect() )
		{
			if( IsEdiMappedTo( dgpr ) )
			{
				// Special handler for reloading from EDI
				xMOV( target[dgpr].GetReg(), edi );
				prevmap[dgpr] = target[dgpr];
				continue;
			}

			// Xchg/Order checks on direct targets: It's possible for registers in a src map to
			// be dependent on each other, in which case movs must be ordered to avoid over-writing
			// a register before it's moved to it's destination.  For example, if eax->ecx and
			// ecx->edx, then it's imperative that ecx be moved *first*, otherwise it'll end up
			// holding the value of eax instead.

			const InstructionConstOptimizer& mips( ir.Inst.MipsInst );

			for( int sg=0; sg<34; ++sg )
			{
				const MipsGPRs_t sgpr = (MipsGPRs_t)sg;
				
				// Scan a copy of the previr's output registers to see if the current target
				// register had any previous assignments that need to be dealt with first.

				if( (dgpr == sgpr) || (target[dgpr] != prevmap[sgpr]) ) continue;

				if( prevmap[dgpr] == target[sgpr] )
				{
					Console::Notice( "IOPrec: Exchange Condition Detected." );
					prevmap[sgpr] = target[sgpr];
					prevmap[dgpr] = target[dgpr];	// this'll turn the xMOV below into a no-op. :)
				}
				else
				{
					if( mips.IsConstGpr[sgpr] )
						xMOV( target[sgpr], mips.ConstVal[sgpr] );
					else
						xMOV( target[sgpr], prevmap[sgpr] );
				}
				prevmap[sgpr] = target[sgpr];		// mark it as done.
			}

			if( target[dgpr] != prevmap[dgpr] )		// second check needed in case of out-of-order loading above.
			{
				if( mips.IsConstGpr[dgpr] )
					xMOV( target[dgpr], mips.ConstVal[dgpr] );
				else
					xMOV( target[dgpr], prevmap[dgpr] );

				prevmap[dgpr] = target[dgpr];		// mark it as done.
			}
		}
		else if( xIsDirty[prevmap[dgpr].GetReg()] )		// only flush if reg is dirty
		{
			jASSUME( prevmap[dgpr].IsDirect() );
			xMOV( target[dgpr].GetMem(), prevmap[dgpr].GetReg() );
			xIsDirty[prevmap[dgpr].GetReg()] = false;
			prevmap[dgpr] = target[dgpr];
		}
	}
}

// ------------------------------------------------------------------------
//
static void _DynGen_ConstSelfCheck()
{
	// [TODO] Finish the Const Self-Check Implementation Below ...
	// Generate x86 code to self-check the status of the const GPR values as known at
	// compilation time (now!) against the actual values saved into the iopRegs GPRs.
	// This feature isn't really needed right now since we don't have any fancy const
	// optimizations.  But if we add stuff later to optimize consts to registers when
	// appropriate then it would be a good idea to include a validation check of optimized
	// behavior.
	
	/*
	xMOV( ecx, g_BlockState.BlockStartPC );		// parameter 1 to ConstSelfCheckFailure
	for( int gpr=1; gpr<34; gpr++ )
	{
		const xRegister32& xreg( last.Dest[gpr].GetReg() );
		
		if( last.m_constinfoex.IsConst(gpr) )
		{
			xCMP( ir.GetMemIndexer( gpr ), last.m_constinfoex.ConstVal[gpr] );
			xJNE( ConstSelfCheckFailure );
		}
	}*/	
}

// ------------------------------------------------------------------------
//
static void _DynGen_EventTest()
{
	xSUB( ptr32[&iopRegs.evtCycleCountdown], g_BlockState.GetScaledBlockCycles() );
	xJLE( DynFunc::CallEventHandler );
}

Registers s_intRegsResult;

// ------------------------------------------------------------------------
// Self-checking against the interpreter!  [Clever and slow]
// Runs the code both as the interpreter and the recompiler, and checks the register state
// results of both passes at the end of the block.  Assert on differences.
//
static void _SelfCheckInterp_Setup()
{
	Registers savedRegs = iopRegs;
	//recIR_FirstPassInterpreter();

	s_intRegsResult = iopRegs;

	// Restore state for the recompiler pass.
	iopRegs = savedRegs;
}

// ------------------------------------------------------------------------
static void _SelfCheckInterp_Assert()
{
	if( memcmp(&s_intRegsResult, &iopRegs, sizeof( iopRegs )) != 0 )
	{
		assert( false );
	}
}

// ------------------------------------------------------------------------
static void recIR_Flush( const x86IntRep& ir, xRegisterArray32<bool>& xIsDirty )
{
	for( int g=1; g<34; g++ )
	{
		const MipsGPRs_t gpr = (MipsGPRs_t)g;
		const xRegister32& xreg( ir.Dest[gpr].GetReg() );

		if( !xreg.IsEmpty() && xIsDirty[xreg.Id] )
		{
			xMOV( x86IntRep::GetMemIndexer( gpr ), ir.Dest[gpr].GetReg() );
			xIsDirty[xreg.Id] = false;
		}
		else if( ir.IsConst(gpr) )
		{
			xMOV( x86IntRep::GetMemIndexer( gpr ), ir.GetConstVal(gpr) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void iopRec_x86BlockState::EmitSomeExecutableGoodness()
{
	xSetPtr( g_BlockState.xBlockPtr );
	_DynGen_EventTest();
	
	//xCALL( _SelfCheckInterp_Setup );

	DynGen_InitMappedRegs();

	// Dirty tracking for x86 registers -- if the register is written, it becomes
	// dirty and needs to be flushed.  If it's not dirty then no flushing is needed
	// (dirty implies read-only).

	xRegisterArray32<bool> xIsDirty;
	for( int i=0; i<xIsDirty.Length(); ++i )
		xIsDirty[i] = false;

	for( int i=0; i<m_xir.GetLength(); ++i )
	{
		// EBP is 'magic' (read-only) and never flushes or writes back to memory:
		xIsDirty.ebp = false;

		const x86IntRep& ir( m_xir[i] );
		const InstructionConstOptimizer& mips( ir.Inst.MipsInst );

		if( !mips.IsConstPc() )
			recIR_Flush( ir, xIsDirty );

		if( IsDevBuild )
		{
			// Debugging helper: Marks the current instruction's PC.
			// Doubling up on the XOR preserves EAX status. :D
			xXOR( eax, mips._Pc_ );
			xXOR( eax, mips._Pc_ );
		}

		// ------------------------------------------------------------------------
		// Map Entry Registers:

		if( i == 0 )
		{
			// Initial mapping from memory / const / edi
			// (zero reg is included -- needed for some store ops, but should otherwise be unmapped)

			for( int g=0; g<34; g++ )
			{
				const MipsGPRs_t gpr = (MipsGPRs_t)g;
				if( ir.Src[gpr].IsIndirect() ) continue;

				if( IsEdiMappedTo( gpr ) )
					xMOV( ir.Src[gpr], edi );

				else if( mips.IsConstGpr[gpr] )
					xMOV( ir.Src[gpr], mips.ConstVal[gpr] );

				else
					xMOV( ir.Src[gpr].GetReg(), x86IntRep::GetMemIndexer(gpr) );
			}
		}
		else
		{
			_DynGen_MapRegisters( ir, m_xir[i-1], xIsDirty );
			ir._DynGen_MapStrictMultimap();
			_DynGen_PreInstructionFlush( ir, xIsDirty );
		}

		ir.Emit();
		
		// Update Dirty Status on Exit, for any written regs.
		// [TODO] : This belongs in IrInst_Flush?

		for( int cf=0; cf<RF_Count; ++cf )
		{
			RegField_t curfield = (RegField_t)cf;
			int gpr = ir.Inst.WritesField( curfield );
			if( gpr == -1 ) continue;

			if( ir.Dest[gpr].IsDirect() )
				xIsDirty[ir.Dest[gpr].GetReg()] = true;
		}
	}

	recIR_Flush( m_xir[m_xir.GetLength()-1], xIsDirty );

	//xCALL( _SelfCheckInterp_Assert );
	//Console::Notice( "Stage done!" );
}

}