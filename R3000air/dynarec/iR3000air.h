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

#include "ix86/ix86.h"
#include <map>

using namespace x86Emitter;

#define ptrT xAddressIndexer<T>()

namespace R3000A
{
	extern void __fastcall DivStallUpdater( uint cycleAcc, int newstall );
}

//////////////////////////////////////////////////////////////////////////////////////////
// iopRecState - contains data representing the current known state of the emu during the
// process of block recompilation.  Information in this struct
//
struct iopRecState
{
	int BlockCycleAccum;
	int DivCycleAccum;
	u32 pc;		// pc for the currently recompiling/emitting instruction

	int GetScaledBlockCycles() const
	{
		return BlockCycleAccum * 1;
	}

	int GetScaledDivCycles() const
	{
		return DivCycleAccum * 1;
	}
	
	__releaseinline void DivCycleInc()
	{
		if( DivCycleAccum < 0x7f )		// cap it at 0x7f (anything over 35 is ignored anyway)
			DivCycleAccum++;
	}
	
	__releaseinline void IncCycleAccum()
	{
		BlockCycleAccum++;
		DivCycleInc();
	}
};

extern iopRecState m_RecState;


//////////////////////////////////////////////////////////////////////////////////////////
//
class xImmOrReg
{
	xRegister32 m_reg;
	u32 m_imm;
	
public:
	xImmOrReg() :
		m_reg(), m_imm( 0 ) { }

	xImmOrReg( u32 imm, const xRegister32& reg=xRegister32::Empty ) :
		m_reg( reg ), m_imm( imm ) { }
		
	const xRegister32& GetReg() const { return m_reg; }
	const u32 GetImm() const { return m_imm; }
	bool IsReg() const { return !m_reg.IsEmpty(); }
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class xDirectOrIndirect
{
	xRegister32 m_RegDirect;
	ModSibBase m_MemIndirect;

public:
	xDirectOrIndirect() :
		m_RegDirect(), m_MemIndirect( 0 ) {}

	xDirectOrIndirect( const xRegister32& srcreg ) :
		m_RegDirect( srcreg ), m_MemIndirect( 0 ) {}

	xDirectOrIndirect( const ModSibBase& srcmem ) :
		m_RegDirect(), m_MemIndirect( srcmem ) {}
	
	const xRegister32& GetReg() const { return m_RegDirect; }
	const ModSibBase& GetMem() const { return m_MemIndirect; }
	bool IsReg() const { return !m_RegDirect.IsEmpty(); }
};

//////////////////////////////////////////////////////////////////////////////////////////
// xAnyOperand -
//
// Note: Styled as a struct so that we can use convenient C-style array initializers.
//
struct xAnyOperand
{
	xRegister32 RegDirect;
	ModSibStrict<u32> MemIndirect;
	u32 Imm;
	bool IsConst;
	
	xAnyOperand() :
		RegDirect(),
		MemIndirect( 0 ),
		Imm( 0 ),
		IsConst( false )
	{
	}

	xAnyOperand( const ModSibStrict<u32>& indirect ) :
		RegDirect(),
		MemIndirect( indirect ),
		Imm( 0 ),
		IsConst( false )
	{
	}
};

namespace R3000A
{

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class IntermediateInstruction
{
	// Callback to emit the instruction in question.
	void (*Emitter)();

public:
	// Source operands can either be const/non-const and can have either a register
	// or memory operand allocated to them (at least one but never both).  Even if
	// a register is const, it may be allocated an x86 register for performance
	// reasons [in the case of a const that is reused many times, for example]

	xAnyOperand SrcRs;
	xAnyOperand SrcRt;
	xAnyOperand SrcRd;

	xAnyOperand SrcHi;
	xAnyOperand SrcLo;

	// Dest operands can either be register or memory.

	xDirectOrIndirect DestRs;
	xDirectOrIndirect DestRt;
	xDirectOrIndirect DestRd;

	xDirectOrIndirect DestHi;
	xDirectOrIndirect DestLo;

	xImmOrReg ixImm;
	bool SignExtendOnLoad;
	
	InstructionOptimizer Inst;		// raw instruction information.

protected:
	GprStatus m_IsConst;

public:
	IntermediateInstruction() :
		Inst( Opcode( 0 ) ) {}

public:
	s32 GetImm() const { return ixImm.GetImm(); }
	
	bool IsConstRs() const { return m_IsConst.Rs; }
	bool IsConstRt() const { return m_IsConst.Rt; }
	bool IsConstRd() const { return m_IsConst.Rd; }

	void AddImmTo( const xRegister32& dest ) const 
	{
		if( !ixImm.IsReg() )
			xADD( dest, ixImm.GetReg() );
		else
			xADD( dest, ixImm.GetImm() );
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const xRegister32& dest ) const 
	{
		if( SrcRs.IsConst )
			xMOV( dest, SrcRs.Imm );

		else if( !SrcRs.RegDirect.IsEmpty() )
			xMOV( dest, SrcRs.RegDirect );

		else
			xMOV( dest, SrcRs.MemIndirect );
	}

	void MoveRtTo( const xRegister32& dest ) const 
	{
		if( SrcRt.IsConst )
			xMOV( dest, SrcRt.Imm );

		else if( !SrcRt.RegDirect.IsEmpty() )
			xMOV( dest, SrcRt.RegDirect );

		else
			xMOV( dest, SrcRt.MemIndirect );
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( SrcRs.IsConst )
			xMOV( dest, SrcRs.Imm );

		else if( !SrcRs.RegDirect.IsEmpty() )
			xMOV( dest, SrcRs.RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, SrcRs.MemIndirect );
			xMOV( dest, tempreg );
		}
	}

	void MoveRtTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( SrcRt.IsConst )
			xMOV( dest, SrcRt.Imm );

		else if( !SrcRt.RegDirect.IsEmpty() )
			xMOV( dest, SrcRt.RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, SrcRs.MemIndirect );
			xMOV( dest, tempreg );
		}
	}

	// ------------------------------------------------------------------------
	template< typename T >
	void MoveToRt( const xRegister<T>& src, const xRegister32& tempreg ) const
	{
		if( DestRt.IsReg() )
		{
			if( SignExtendOnLoad )
				xMOVSX( DestRt.GetReg(), src );
			else
				xMOVZX( DestRt.GetReg(), src );
		}
		else
		{
			// pooh.. gotta move the 'hard' way :(
			// (src->temp->dest)

			if( SignExtendOnLoad )
				xMOVSX( tempreg, src );
			else
				xMOVZX( tempreg, src );

			xMOV( DestRt.GetMem(), src );
		}
	}

	template<>
	void MoveToRt<u32>( const xRegister<u32>& src, __unused const xRegister32& tempreg ) const
	{
		if( DestRt.IsReg() )
			xMOV( DestRt.GetReg(), src );
		else
			xMOV( DestRt.GetMem(), src );
	}

	template< typename T >
	void MoveToRt( const ModSibStrict<T>& src, const xRegister32& tempreg ) const
	{
		if( DestRt.IsReg() )
		{
			if( SignExtendOnLoad )
				xMOVSX( DestRt.GetReg(), src );
			else
				xMOVZX( DestRt.GetReg(), src );
		}
		else
		{
			// pooh.. gotta move the 'hard' way :(
			// (src->temp->dest)

			if( SignExtendOnLoad )
				xMOVSX( tempreg, src );
			else
				xMOVZX( tempreg, src );

			xMOV( DestRt.GetMem(), tempreg );
		}
	}

	template<>
	void MoveToRt<u32>( const ModSibStrict<u32>& src, const xRegister32& tempreg ) const
	{
		if( DestRt.IsReg() )
			xMOV( DestRt.GetReg(), src );
		else
		{
			// pooh.. gotta move the 'hard' way :(
			// (src->temp->dest)

			xMOV( tempreg, src );
			xMOV( DestRt.GetMem(), tempreg );
		}
	}
	
	// ------------------------------------------------------------------------
	void MoveToHiLo( const xRegister32& hireg, const xRegister32& loreg ) const
	{
		if( DestHi.IsReg() )
			xMOV( DestHi.GetReg(), hireg );
		else
			xMOV( DestHi.GetMem(), hireg );

		if( DestLo.IsReg() )
			xMOV( DestLo.GetReg(), loreg );
		else
			xMOV( DestLo.GetMem(), loreg );
	}

	// Hi, I clobber ECX (maybe), so watch it how you use me! :)
	void MoveToHiLo( u32 immhi, u32 immlo ) const
	{
		if( DestHi.IsReg() )
			xMOV( DestHi.GetReg(), immhi );
		else
		{
			xMOV( ecx, immhi );
			xMOV( DestHi.GetMem(), ecx );
		}

		if( DestLo.IsReg() )
			xMOV( DestLo.GetReg(), immlo );
		else
		{
			xMOV( ecx, immlo );
			xMOV( DestLo.GetMem(), ecx );
		}
	}

	template< typename T > void MoveToRt( const xRegister<T>& src ) const		{ MoveToRt( src, edx ); }
	template< typename T > void MoveToRt( const ModSibStrict<T>& src ) const	{ MoveToRt( src, edx ); }

	template<> void MoveToRt<u32>( const xRegister<u32>& src ) const			{ MoveToRt( src, edx ); }
	template<> void MoveToRt<u32>( const ModSibStrict<u32>& src ) const			{ MoveToRt( src, edx ); }
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class IntermediateBlock
{
};

//////////////////////////////////////////////////////////////////////////////////////////
// xJumpLink - houses the address of the jump instruction and the comparison type.  The
// address is the full instruction (not just the displacement portion), and comparison type
// is also provided so that we can write the entire jump using either j8 or j32 as needed.
//
struct xJumpLink
{
	u8* xPtr;					// address of the jump to be written
	JccComparisonType ccType;	// type of comparison to be used when writing jumps
	
	xJumpLink() {}
	
	xJumpLink( u8* x86ptr, JccComparisonType cctype ) :
		xPtr( x86ptr ),
		ccType( cctype )
	{
	}
	
	void SetTarget( const void* target ) const;

	void SetTarget( uptr target ) const
	{
		SetTarget( (void*)target );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
struct recBlockItem : public NoncopyableObject
{
	uint x86len;	// length of the recompiled block

	// Intermediate language allocation.  If size is non-zero, then we're on our second pass
	// and the IL should be recompiled into x86 code for direct execution.
	SafeList<InstructionOptimizer> IL;

	// A list of all block links dependent on this block.  If this block moves, then all links
	// in this list need to have their x86 jump instructions rewritten.
	SafeList<xJumpLink> DependentLinks;

	// This member contains a copy of the code originally recompiled, for use with
	// MMX/XMM optimized validation of blocks (and subsequent clearing if the block
	// in memory does not match the validation copy recorded when recompilation was
	// performed).
	SafeArray<u32> ValidationCopy;

	recBlockItem() :
		x86len( 0 ),
		IL( 16, "recBlockItem::IL" ),
		DependentLinks( 4, "recBlockItem::DepdendentLinks" ),
		ValidationCopy( 0, "recBlockItem::ValidationCopy" )
	{
	}
};

// ------------------------------------------------------------------------
extern void recIL_Block( SafeList<InstructionOptimizer>& iList );


// ------------------------------------------------------------------------
// Memory Operations Instructions
//
extern void recLB( const IntermediateInstruction& info );
extern void recLH( const IntermediateInstruction& info );
extern void recLW( const IntermediateInstruction& info );
extern void recLBU( const IntermediateInstruction& info );
extern void recLHU( const IntermediateInstruction& info );
extern void recLWL( const IntermediateInstruction& info );
extern void recLWR( const IntermediateInstruction& info );

extern void recSB( const IntermediateInstruction& info );
extern void recSH( const IntermediateInstruction& info );
extern void recSW( const IntermediateInstruction& info );
extern void recSWL( const IntermediateInstruction& info );
extern void recSWR( const IntermediateInstruction& info );

// ------------------------------------------------------------------------
// Jump / Branch instructions
//
extern void recJ( const IntermediateInstruction& info );
extern void recJR( const IntermediateInstruction& info );

// ------------------------------------------------------------------------
// Arithmetic Instructions
//
extern void recDIV( const IntermediateInstruction& info );
extern void recDIVU( const IntermediateInstruction& info );

}
