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
	xAddressInfo m_MemIndirect;

public:
	xDirectOrIndirect() :
		m_RegDirect(), m_MemIndirect( 0 ) {}

	xDirectOrIndirect( const xRegister32& srcreg ) :
		m_RegDirect( srcreg ), m_MemIndirect( 0 ) {}

	xDirectOrIndirect( const xAddressInfo& srcmem ) :
		m_RegDirect(), m_MemIndirect( srcmem ) {}
	
	const xRegister32& GetReg() const { return m_RegDirect; }
	ModSibBase GetMem() const { return ptr[m_MemIndirect]; }
	ModSibStrict<u32> GetMem32() const { return ptr32[m_MemIndirect]; }
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
	xAddressInfo MemIndirect;
	u32 Imm;
	bool IsConst;
	
	xAnyOperand() :
		RegDirect(),
		MemIndirect( 0 ),
		Imm( 0 ),
		IsConst( false )
	{
	}

	xAnyOperand( const xRegister32& direct ) :
		RegDirect( direct ),
		MemIndirect( 0 ),
		Imm( 0 ),
		IsConst( false )
	{
	}

	xAnyOperand( const xAddressInfo& indirect ) :
		RegDirect(),
		MemIndirect( indirect ),
		Imm( 0 ),
		IsConst( false )
	{
	}

	ModSibBase GetMem() const { return ptr[MemIndirect]; }
	ModSibStrict<u32> GetMem32() const { return ptr32[MemIndirect]; }
};

namespace R3000A
{

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class IntermediateInstruction
{
public:
	// Source operands can either be const/non-const and can have either a register
	// or memory operand allocated to them (at least one but never both).  Even if
	// a register is const, it may be allocated an x86 register for performance
	// reasons [in the case of a const that is reused many times, for example]
	xAnyOperand Src[RF_Count];

	// Dest operand can either be register or memory.
	xDirectOrIndirect Dest[RF_Count];

	xImmOrReg ixImm;
	InstructionConstOpt Inst;		// raw instruction information.

public:
	IntermediateInstruction() :
		Inst( Opcode( 0 ) ) {}

public:
	s32 GetImm() const { return ixImm.GetImm(); }
	
	bool IsConstRs() const { return Inst.IsConstInput.Rs; }
	bool IsConstRt() const { return Inst.IsConstInput.Rt; }
	bool IsConstRd() const { return Inst.IsConstInput.Rd; }

	bool SignExtendsResult() const { return Inst.SignExtendOnWrite; }

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
		if( !Src[RF_Rs].RegDirect.IsEmpty() )
			xMOV( dest, Src[RF_Rs].RegDirect );

		else if( Src[RF_Rs].IsConst )
			xMOV( dest, Src[RF_Rs].Imm );

		else
			xMOV( dest, Src[RF_Rs].GetMem() );
	}

	void MoveRtTo( const xRegister32& dest ) const 
	{
		if( !Src[RF_Rt].RegDirect.IsEmpty() )
			xMOV( dest, Src[RF_Rt].RegDirect );

		else if( Src[RF_Rt].IsConst )
			xMOV( dest, Src[RF_Rt].Imm );

		else
			xMOV( dest, Src[RF_Rt].GetMem() );
	}

	/*
	// ------------------------------------------------------------------------
	void MoveRsTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( Src[RF_Rs].IsConst )
			xMOV( dest, Src[RF_Rs].Imm );

		else if( !Src[RF_Rs].RegDirect.IsEmpty() )
			xMOV( dest, Src[RF_Rs].RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, Src[RF_Rs].MemIndirect );
			xMOV( dest, tempreg );
		}
	}

	void MoveRtTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( Src[RF_Rt].IsConst )
			xMOV( dest, Src[RF_Rt].Imm );

		else if( !Src[RF_Rt].RegDirect.IsEmpty() )
			xMOV( dest, Src[RF_Rt].RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, Src[RF_Rs].MemIndirect );
			xMOV( dest, tempreg );
		}
	}*/
	
	
	// ------------------------------------------------------------------------
	template< typename T >
	void SignExtendedMove( const xRegister32& dest, const ModSibStrict<T>& src ) const
	{
		if( Inst.SignExtendOnWrite )
			xMOVSX( Dest[RF_Rt].GetReg(), src );
		else
			xMOVZX( Dest[RF_Rt].GetReg(), src );
	}

	template<>
	void SignExtendedMove<u32>( const xRegister32& dest, const ModSibStrict<u32>& src ) const
	{
		xMOV( dest, src );
	}

	template< typename T >
	void SignExtendEax() const
	{
		if( Inst.SignExtendOnWrite )
		{
			if( sizeof(T) == 1 )
				xMOVSX( eax, al );
			else
				xCWDE();
		}
		else
		{
			if( sizeof(T) == 1 )
				xMOVZX( eax, al );
			else
				xMOVZX( eax, ax );
		}
	}
	
	// Do nothing for 32-bit sign extension.
	template<> void SignExtendEax<u32>() const { }

	void MoveToRt( const xRegister32& src ) const
	{
		if( Inst._Rt_ == 0 ) return;

		if( Dest[RF_Rt].IsReg() )
			xMOV( Dest[RF_Rt].GetReg(), src );
		else
			xMOV( Dest[RF_Rt].GetMem(), src );
	}
	
	// ------------------------------------------------------------------------
	void MoveToHiLo( const xRegister32& hireg, const xRegister32& loreg ) const
	{
		if( Dest[RF_Hi].IsReg() )
			xMOV( Dest[RF_Hi].GetReg(), hireg );
		else
			xMOV( Dest[RF_Hi].GetMem(), hireg );

		if( Dest[RF_Lo].IsReg() )
			xMOV( Dest[RF_Lo].GetReg(), loreg );
		else
			xMOV( Dest[RF_Lo].GetMem(), loreg );
	}

	// loads lo with a register, and loads hi with zero.
	void MoveToHiLo( const xRegister32& loreg ) const
	{
		if( Dest[RF_Hi].IsReg() )
			xXOR( Dest[RF_Hi].GetReg(), Dest[RF_Hi].GetReg() );
		else
			xMOV( Dest[RF_Hi].GetMem32(), 0 );

		if( Dest[RF_Lo].IsReg() )
			xMOV( Dest[RF_Lo].GetReg(), loreg );
		else
			xMOV( Dest[RF_Lo].GetMem(), loreg );
	}
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

static const int MaxCyclesPerBlock = 128;

//////////////////////////////////////////////////////////////////////////////////////////
// recBlockItemTemp - Temporary workspace buffer used to reduce the number of heap allocations
// required during block recompilation.
//
struct recBlockItemTemp
{
	InstructionConstOpt inst[MaxCyclesPerBlock];
	u32 ramcopy[MaxCyclesPerBlock];
	int ramlen;
	int instlen;
};

//////////////////////////////////////////////////////////////////////////////////////////
//
struct recBlockItem : public NoncopyableObject
{
	uint x86len;	// length of the recompiled block
	uint clears;	// number of times this block has been cleared and recompiled
	
	// Intermediate language allocation.  If size is non-zero, then we're on our second pass
	// and the IL should be recompiled into x86 code for direct execution.
	SafeArray<InstructionConstOpt> IL;

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
		clears( 0 ),
		IL( "recBlockItem::IL" ),
		DependentLinks( 4, "recBlockItem::DependentLinks" ),
		ValidationCopy( "recBlockItem::ValidationCopy" )
	{
		IL.ChunkSize = 32;
		DependentLinks.ChunkSize = 8;
		ValidationCopy.ChunkSize = 32;
	}

	void Assign( const recBlockItemTemp& src );
};

//////////////////////////////////////////////////////////////////////////////////////////
//

struct RegisterMappings
{
	xRegister32 Rd, Rt, Rs, Hi, Lo;
};

class InstructionEmitter
{
public:
	InstructionEmitter() {}

	virtual void MapRegisters( const IntermediateInstruction& info, RegisterMappings& inmaps, RegisterMappings& outmaps ) const {}
	virtual void Emit( const IntermediateInstruction& info ) const {}
};

class InstructionRecompiler
{
public:
	InstructionRecompiler() {}

	// fully non-const emitter.
	virtual const InstructionEmitter& ConstNone() const=0;

	// const status on Rt, Rs is non-const
	virtual const InstructionEmitter& ConstRt() const=0;

	// const status on Rs, Rt is non-const
	virtual const InstructionEmitter& ConstRs() const=0;

	// fully const (provided for Memory Write ops only, all others should optimize away)
	virtual const InstructionEmitter& ConstRsRt() const;
};


//////////////////////////////////////////////////////////////////////////////////////////


extern recBlockItemTemp m_blockspace;

// ------------------------------------------------------------------------
extern void recIL_Block();


// ------------------------------------------------------------------------
// Memory Operations Instructions
//
extern const InstructionRecompiler
	&recLB, &recLH, &recLW, &recLWL, &recLWR,
	&recSB, &recSH, &recSW, &recSWL, &recSWR;

// ------------------------------------------------------------------------
// Jump / Branch instructions
//
extern void recJ( const IntermediateInstruction& info );
extern void recJR( const IntermediateInstruction& info );

// ------------------------------------------------------------------------
// Arithmetic Instructions
//

extern const InstructionRecompiler& recDIV;
extern const InstructionRecompiler& recDIVU;

}
