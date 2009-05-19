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
#include "../R3000airOpcodeTables.inl"

using namespace x86Emitter;

namespace R3000A
{

//////////////////////////////////////////////////////////////////////////////////////////
// Builds the intermediate const and regalloc resolutions.
//
void recIL_Expand( IntermediateInstruction& iInst )
{
	const InstructionConstOpt& dudley = iInst.Inst;
	
	if( dudley.ReadsRd() )
	{
	}
}

recBlockItemTemp m_blockspace;

#ifdef PCSX2_DEVBUILD
static string m_disasm;
static string m_comment;
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Generates IL for an entire block of code.
//
void recIL_Block()
{
	bool delaySlot = false;

	m_blockspace.ramlen = 0;
	m_blockspace.instlen = 0;

	bool gpr_IsConst[34] = { true, false };	// GPR0 is always const!

	do 
	{
		Opcode opcode( iopMemDirectRead32( iopRegs.pc ) );
		m_blockspace.ramcopy[m_blockspace.ramlen++] = opcode.U32;

		if( opcode.U32 == 0 )	// Iggy on the NOP please!  (Iggy Nop!)
		{
			PSXCPU_LOG( "NOP%s", iopRegs.IsDelaySlot ? "\n" : "" );

			iopRegs.pc			 = iopRegs.VectorPC;
			iopRegs.VectorPC	+= 4;
			iopRegs.IsDelaySlot	 = false;
		}
		else
		{
			#ifdef PCSX2_DEVBUILD
			if( (varLog & 0x00100000) && iopRegs.cycle > 0x40000 )
			{
				InstructionDiagnostic diag( opcode );
				diag.Process();
				diag.GetDisasm( m_disasm );
				diag.GetValuesComment( m_comment );

				if( m_comment.empty() )
					PSXCPU_LOG( "%s%s", m_disasm.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
				else
					PSXCPU_LOG( "%-34s ; %s%s", m_disasm.c_str(), m_comment.c_str(), iopRegs.IsDelaySlot ? "\n" : "" );
			}
			#endif

			InstructionConstOpt& inst( m_blockspace.inst[m_blockspace.instlen] );
			inst.Assign( opcode, gpr_IsConst );
			inst.Process();

			// Update const status for registers.  The const status of all written registers is
			// based on the const status of the read registers.  If the operation reads from
			// memory or from an Fs register, then const status is always false.
			
			bool constStatus;
			
			if( inst.ReadsMemory() || inst.ReadsFs() )
				constStatus = false;
			else
			{
				constStatus = 
					//(inst.ReadsRd() ? gpr_IsConst[inst._Rd_] : true) &&
					(inst.ReadsRt() ? gpr_IsConst[inst._Rt_] : true) &&
					(inst.ReadsRs() ? gpr_IsConst[inst._Rs_] : true) &&
					(inst.ReadsHi() ? gpr_IsConst[32] : true) &&
					(inst.ReadsLo() ? gpr_IsConst[33] : true);
			}

			if( inst.WritesRd() ) gpr_IsConst[inst._Rd_] = constStatus;
			if( inst.WritesRt() ) gpr_IsConst[inst._Rt_] = constStatus;
			if( inst.WritesRs() ) gpr_IsConst[inst._Rs_] = constStatus;

			jASSUME( gpr_IsConst[0] == true );		// GPR 0 should *always* be const

			if( inst.WritesLink() ) gpr_IsConst[31] = constStatus;
			if( inst.WritesHi() ) gpr_IsConst[32] = constStatus;
			if( inst.WritesLo() ) gpr_IsConst[33] = constStatus;

			if( !constStatus || inst.WritesMemory() || inst.IsBranchType() || inst.HasSideEffects() )
				m_blockspace.instlen++;

			// prep the iopRegs for the next instruction fetch -->
			// 
			// Typically VectorPC points to the delay slot instruction on branches, and the GetNextPC()
			// references the *target* of the branch.  Thus the delay slot is processed on the next
			// pass (unless an exception occurs), and *then* we vector to the branch target.  In other
			// words, VectorPC acts as an instruction prefetch, only we prefetch just the PC (doing a
			// full-on instruction prefetch is less efficient).
			//
			// note: In the case of raised exceptions, VectorPC and GetNextPC() can be overridden during
			//  instruction processing above.

			iopRegs.pc			= iopRegs.VectorPC;
			iopRegs.VectorPC	= inst.GetNextPC();
			iopRegs.IsDelaySlot	= inst.IsBranchType();

			// Test for DivUnit Stalls.
			// Note: DivStallUpdater applies the stall directly to the iopRegs, instead of using the
			// RecState's cycle accumulator.  This is because the same thing is done by the recs, and
			// is designed to allow proper handling of stalls across branches.

			if( inst.GetDivStall() != 0 )
				DivStallUpdater( m_RecState.DivCycleAccum, inst.GetDivStall() );

			// RFE, SYSCALL and BREAK cause exceptions, which should terminate block recompilation:
			if( inst.HasSideEffects() )
				delaySlot = true;
		}

		m_RecState.IncCycleAccum();
		if( delaySlot ) break;
		delaySlot = iopRegs.IsDelaySlot;

	} while( m_RecState.BlockCycleAccum < MaxCyclesPerBlock );

	jASSUME( m_blockspace.instlen != 0 );

	iopRegs.cycle += m_RecState.BlockCycleAccum;
}

//////////////////////////////////////////////////////////////////////////////////////////


static const xAddressReg GPR_xIndexReg( esi );

// ------------------------------------------------------------------------
// returns an indirect x86 memory operand referencing the requested register.
//
xAddressInfo GPR_GetMemIndexer( uint gpridx )
{
	// There are 34 GPRs counting HI/LO, so the GPR indexer (ESI) points to
	// the HI/LO pair and uses negative addressing for the regular 32 GPRs.

	return GPR_xIndexReg - ((32-gpridx)*4);
}

// buffer used to process intermediate instructions.
static IntermediateInstruction m_intermediates[MaxCyclesPerBlock];


static const char* RegField_ToString( RegField_t field )
{
	switch( field )
	{
		case RF_Rd: return "Rd";
		case RF_Rt: return "Rt";
		case RF_Rs: return "Rs";
		case RF_Hi: return "Hi";
		case RF_Lo: return "Lo";
		
		jNO_DEFAULT
	}
}

namespace Analytics
{
	int RegMapped_TypeA = 0;
	int RegMapped_TypeB = 0;
}

static void MapSourceField( RegField_t field, int valid_uses[34],
	const InstructionConstOpt& iListCur,
	const InstructionConstOpt& iListPrev,
	IntermediateInstruction& curIL,
	IntermediateInstruction& prevIL )
{
	int gpr_read = iListCur.ReadsField( field );
	if( gpr_read >= 0 )
	{
		RegField_t prevfield = iListPrev.WritesReg( iListCur.RegField( field ) );
		if( prevfield != RF_Unused )
		{
			// Success!  Map it:

			curIL.Src[field]		= ( field == RF_Hi ) ? edx : eax;
			prevIL.Dest[prevfield]	= ( field == RF_Hi ) ? edx : eax;

			Analytics::RegMapped_TypeA++;
			Console::Status( "IOP Register Mapped : %s -> %s (total: %d)", params RegField_ToString(prevfield), RegField_ToString(field), Analytics::RegMapped_TypeA );
		}
		else
		{
			bool constStatus;
			switch( field )
			{
				case RF_Rd:	constStatus = iListCur.IsConstInput.Rd; break;
				case RF_Rt:	constStatus = iListCur.IsConstInput.Rt; break;
				case RF_Rs:	constStatus = iListCur.IsConstInput.Rs; break;
				case RF_Hi:	constStatus = iListCur.IsConstInput.Hi; break;
				case RF_Lo:	constStatus = iListCur.IsConstInput.Lo; break;
				jNO_DEFAULT
			}

			valid_uses[gpr_read] += constStatus ? 3 : 4;
		}
	}
}

// ------------------------------------------------------------------------
// Intermediate Pass 2 -- Assigns regalloc prior to the x86 codegen.
//
void recIL_Pass2( const SafeArray<InstructionConstOpt>& iList )
{
	// First instruction starts out as a blank slate:
	const InstructionConstOpt& first( iList[0] );
	const Opcode& effop( first._Opcode_ );

	const int numinsts = iList.GetLength();

	for( int i=0; i<numinsts; i++ )
	{
		IntermediateInstruction& fnew( m_intermediates[i] );
		
		fnew.Src[RF_Rs] = GPR_GetMemIndexer( effop.Rs() );
		fnew.Src[RF_Rt] = GPR_GetMemIndexer( effop.Rt() );
		fnew.Src[RF_Rd] = GPR_GetMemIndexer( effop.Rd() );
		fnew.Src[RF_Hi] = GPR_GetMemIndexer( 32 );		// Hi
		fnew.Src[RF_Lo] = GPR_GetMemIndexer( 33 );		// Lo

		// Assign destination registers

		fnew.Dest[RF_Rs] = GPR_GetMemIndexer( effop.Rs() );
		fnew.Dest[RF_Rt] = GPR_GetMemIndexer( effop.Rt() );
		fnew.Dest[RF_Rd] = GPR_GetMemIndexer( effop.Rd() );
		fnew.Dest[RF_Hi] = GPR_GetMemIndexer( 32 );		// Hi
		fnew.Dest[RF_Lo] = GPR_GetMemIndexer( 33 );		// Lo
	}

	// ------------------------------------------------------------------------
	// Simple Single-Step Reg mapping:
	// If the next instruction uses a dest register from the previous instruction,
	// then set the dest of prev instruction to eax, and the source of the next to eax.

	// Note: hi has special case logic that maps to edx when possible.

	int valid_uses[34] = {0};

	for( int i=1; i<numinsts; ++i )
	{
		for( int rf=0; rf<RF_Count; ++rf )
			MapSourceField( (RegField_t)rf, valid_uses, iList[i], iList[i-1], m_intermediates[i], m_intermediates[i-1] );
	}

	// ------------------------------------------------------------------------
	// Extended Reg mapping into EDI / EBP:
	// Primary goal of this form of reg mapping is to optimize memory/stack operations,
	// which typically load a register with a source address and then re-use that register
	// as a base register across several memory operations.
	
	// Typically blocks are short and fairly specialized in function, and I bank on this
	// assumption to keep reg mapping simple.  I make the assumption that whichever reg(s)
	// have the most valid reads across the span of a full block is the reg(s) we want to
	// map to our Extended Reg(s).
	
	// Valid uses include anything not already optimized using the first-pass single-step
	// register mapping above.

	int high_count	= 0;
	int high_idx	= 0;

	for( int i=1; i<34; i++ )
	{
		if( high_count < valid_uses[i] )
		{
			high_count = valid_uses[i];
			high_idx = i;
		}
	}

	high_count /= 4;
	if( high_idx != 0 && high_count > 2 )
	{
		Console::WriteLn( "Best Register Found @ %d [%d uses]", params high_idx, high_count );

		// traverse back through the register map and set all srcs and dests that match high_idx
		// to use EDI.

		for( int i=0; i<numinsts-1; ++i )
		{
			IntermediateInstruction& cur( m_intermediates[i] );

			if( high_idx == 32 )
			{
				cur.Src[RF_Hi] = edi;
				cur.Dest[RF_Hi] = edi;
			}
			else if( high_idx == 33 )
			{
				cur.Src[RF_Lo] = edi;
				cur.Dest[RF_Lo] = edi;
			}
			else
			{
				if( cur.Inst._Rd_ == high_idx )
				{
					cur.Src[RF_Rd] = edi;
					cur.Dest[RF_Rd] = edi;
				}

				if( cur.Inst._Rt_ == high_idx )
				{
					cur.Src[RF_Rt] = edi;
					cur.Dest[RF_Rt] = edi;
				}

				if( cur.Inst._Rs_ == high_idx )
				{
					cur.Src[RF_Rs] = edi;
					cur.Dest[RF_Rs] = edi;
				}
			}
		}
	}
}

}
