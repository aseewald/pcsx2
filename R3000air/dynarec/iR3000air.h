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

#define IMPL_GetInterface() \
	static void GetInterface( InstructionEmitterAPI& api ) \
	{ \
		api.Optimizations = Optimizations; \
		api.Emit = Emit; \
	}
	
// Implements the GetInterface function with standard templating!  (used mostly by MemoryOps)
#define IMPL_GetInterfaceTee( mess ) \
	template< mess T > \
	static void GetInterface( InstructionEmitterAPI& api ) \
	{ \
		api.Optimizations = Optimizations; \
		api.Emit = Emit<T>; \
	}

// Some helpers for placebos (unimplemented ops)	
#define IMPL_RecInstAPI( name ) \
	void InstAPI::name() \
	{ \
		API.ConstNone	= rec##name##_ConstNone::GetInterface; \
		API.ConstRt		= rec##name##_ConstNone::GetInterface; \
		API.ConstRs		= rec##name##_ConstNone::GetInterface; \
	}

// Some helpers for placebos (unimplemented ops)	
#define IMPL_RecPlacebo( name ) \
	namespace rec##name##_ConstNone \
	{ \
		static void Optimizations( const IntermediateInstruction& info, OptimizationModeFlags& opts ) {	} \
		static void Emit( const IntermediateInstruction& info ) { } \
		IMPL_GetInterface() \
	} \
	IMPL_RecInstAPI( name );


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
// xAnyOperand - Can be direct, indirect, or immediate/const!
//
//
template< typename OperandType >
struct xAnyOperand
{
	u32 Imm;
	bool IsConst;
	xRegister<OperandType> RegDirect;
	ModSibStrict<OperandType> MemIndirect;

	xAnyOperand() :
		Imm( 0 ),
		IsConst( false ),
		RegDirect(),
		MemIndirect( 0 )
	{
	}

	xAnyOperand( const xRegister<OperandType>& direct ) :
		Imm( 0 ),
		IsConst( false ),
		RegDirect( direct ),
		MemIndirect( 0 )
	{
	}

	xAnyOperand( const ModSibStrict<OperandType>& indirect ) :
		Imm( 0 ),
		IsConst( false ),
		RegDirect(),
		MemIndirect( indirect )
	{
	}
};

namespace R3000A
{

class IntermediateInstruction;

// ------------------------------------------------------------------------
template< typename ContentType >
class xRegister32Array
{
public:
	ContentType Info[7];

	ContentType& operator[]( const xRegister32& src )
	{
		return Info[ src.Id ];
	}

	const ContentType& operator[]( const xRegister32& src ) const
	{
		return Info[ src.Id ];
	}
};

// ------------------------------------------------------------------------
class xRegUseFlags : xRegister32Array<bool>
{
public:
	using xRegister32Array<bool>::operator[];

	void operator()( const xRegister32& src )
	{
		Info[src.Id] = true;
	}
	
	// Uses no registers?  Clear the list baby!
	void None()
	{
		memzero_obj( Info );
	}
	
	// Modifies basic "front four" registers: eax, ebx, ecx, edx
	void Basic4()
	{
		Info[eax.Id] = true;
		Info[edx.Id] = true;
		Info[ecx.Id] = true;
		Info[ebx.Id] = true;
	}

};

//////////////////////////////////////////////////////////////////////////////////////////
//

struct xRegMess
{
	xRegister32 Reg;
	
};

struct RegisterMappings
{
	xRegister32 Rd, Rt, Rs, Hi, Lo;
	
	xRegister32& operator[]( RegField_t idx )
	{
		switch( idx )
		{
			case RF_Rd: return Rd;
			case RF_Rt: return Rt;
			case RF_Rs: return Rs;
			case RF_Hi: return Hi;
			case RF_Lo: return Lo;
			jNO_DEFAULT
		}
	}

	const xRegister32& operator[]( RegField_t idx ) const
	{
		switch( idx )
		{
			case RF_Rd: return Rd;
			case RF_Rt: return Rt;
			case RF_Rs: return Rs;
			case RF_Hi: return Hi;
			case RF_Lo: return Lo;
			jNO_DEFAULT
		}
	}
};

// ------------------------------------------------------------------------
struct OptimizationModeFlags
{
	// Specifies which x86 registers are modified by instruction code.  By default this
	// array is assigned all 'true', meaning it assumes full outwardly modification of
	// x86 registers.
	//
	// Notice: If the Optimization() function is NULL for the particular instruction
	// then the assumption is to mark all registers as *modified* (this assuption can be
	// used for Interpreter-handler callbacks for example).
	xRegUseFlags xModifiesReg;

	// Set to true to inform the recompiler that it can swap Rs and Rt freely.  This option
	// takes effect even if Rs or Rt have been forced to registers, or mapped to specific
	// registers.
	bool CommutativeSources;
	
	// Tells the recompiler that the Rs reg must be force-loaded into an x86 register.
	// (recompiler will flush another reg if needed, and load Rs in preparation for Emit
	// entry).  The register will be picked by the recompiler.
	// Note: ForceDirectRs and ForceIndirectRs are mututally exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceDirectRs;
	bool ForceDirectRt;

	// Tells recompiler Rs register must be forced to Memory (flushed).  This is commonly
	// used as an optimization guide for cases where all x86 registers are modified by the
	// instruction prior tot he instruction using Rs [LWL/SWL type memory ops, namely].
	// Note: ForceDirectRs and ForceIndirectRs are mututally exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceIndirectRs;
	bool ForceIndirectRt;

	// Forces Rs to the specific x86 register on entry.
	// Note: This setting overrides ForceDirectRs, and is mutually exclusive with ForceIndirectRs.
	// Setting this value to anything other than an empty register while ForceIndirecrRs is
	// true is an error, and will cause an assertion/exception at recompilation time.
	RegisterMappings MapInput;

	// Informs recompiler that Rs is preserved in the specified x86 register.  This allows the
	// recompiler to reuse Rs in subsequent instructions (when possible).
	RegisterMappings MapOutput;
};

struct InstructionEmitterAPI
{
	// Optimizations - Optional implementation -
	// Allows a recompiled function to specify optimization hints to the recompiler, and also
	// to force certain behaviors if they are needed for correct instruction generations (such
	// as forcing Rs or Rt to a register, for example).
	//
	// Remarks:
	//   If your instruction uses a GPR (like Rs), but the recompiled version clobbers all
	//   registers early on, then leave the GPR unmapped and load it when needed using
	//   info.MoveRsTo( reg ).  The recompiler will flush the reg to memory and then re-
	//   load it when requested.
	//
	//void (*MapRegisters)( const IntermediateInstruction& info, RegisterMappings& inmaps, RegisterMappings& outmaps );
	void (*Optimizations)( const IntermediateInstruction& info, OptimizationModeFlags& opts );
	
	// Emits code for an instruction.
	// The instruction should assume that the register mappings specified in MapRegisters.inmaps
	// are assigned on function entry.  Results should be written back to GPRs using functions
	// info.MoveToRt(), info.MoveToRd(), or info.MoveToHiLo() -- and need not match the opti-
	// mization hints given in MapRegister.outmaps.
	void (*Emit)( const IntermediateInstruction& info );
};

struct InstructionRecAPI
{
	// fully non-const emitter.
	void (*ConstNone)( InstructionEmitterAPI& api );

	// const status on Rt, Rs is non-const
	void (*ConstRt)( InstructionEmitterAPI& api );

	// const status on Rs, Rt is non-const
	void (*ConstRs)( InstructionEmitterAPI& api );

	// fully const (provided for Memory Write ops only, all others should optimize away)
	void (*ConstRsRt)( InstructionEmitterAPI& api );
	
	static void Error_ConstNone( InstructionEmitterAPI& api );
	static void Error_ConstRsRt( InstructionEmitterAPI& api );
	static void Error_ConstRs( InstructionEmitterAPI& api );
	static void Error_ConstRt( InstructionEmitterAPI& api );

	static void _const_error();
	
	// Clears the contents of the structure to the default values, which in this case
	// map to handlers that throw exceptions (LogicError)
	void Reset()
	{
		ConstNone	= Error_ConstNone;
		ConstRt		= Error_ConstRt;
		ConstRs		= Error_ConstRs;
		ConstRsRt	= Error_ConstRsRt;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class InstructionRecMess : public InstructionConstOpt
{
public:
	InstructionRecAPI API;

public:
	InstructionRecMess() : InstructionConstOpt( Opcode( 0 ) ) {}

	InstructionRecMess( const InstructionConstOpt& src ) :
		InstructionConstOpt( src )
	{
		// Default constructors should do everything we need.
	}
	
	void GetRecInfo();

public:
	INSTRUCTION_API()
};

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class IntermediateInstruction
{
public:
	// source operand can either be register or memory.
	xDirectOrIndirect32 Src[RF_Count];

	// Dest operand can either be register or memory.
	xDirectOrIndirect32 Dest[RF_Count];

	s32 ixImm;
	InstructionRecMess Inst;		// raw instruction information.
	RegisterMappings InMaps;
	RegisterMappings OutMaps;
	InstructionEmitterAPI Emitface;

public:
	IntermediateInstruction() {}

	void Assign( const InstructionConstOpt& src );

public:
	s32 GetImm() const { return ixImm; }
	
	bool IsConstRs() const { return Inst.IsConstInput.Rs; }
	bool IsConstRt() const { return Inst.IsConstInput.Rt; }
	bool IsConstRd() const { return Inst.IsConstInput.Rd; }

	bool SignExtendsResult() const { return Inst.SignExtendOnWrite; }

	int GetConstRs() const
	{
		jASSUME( Inst.IsConstInput.Rs );
		return Inst.ConstVal_Rs;
	}

	int GetConstRt() const
	{
		jASSUME( Inst.IsConstInput.Rt );
		return Inst.ConstVal_Rt;
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const xRegister32& destreg ) const 
	{
		if( IsConstRs() )
			xMOV( destreg, Inst.ConstVal_Rs );
		else
			xMOV( destreg, Src[RF_Rs] );
	}

	void MoveRtTo( const xRegister32& destreg ) const 
	{
		if( IsConstRt() )
			xMOV( destreg, Inst.ConstVal_Rt );
		else
			xMOV( destreg, Src[RF_Rt] );	
	}

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

	// Important!  You should always use this instead of the expanded Dest, as you must
	// be careful to check for instances of the zero register.
	void MoveToRt( const xDirectOrIndirect32& src ) const
	{
		if( Inst._Rt_ == 0 ) return;
		xMOV( Dest[RF_Rt], src );
	}

	void MoveToRd( const xDirectOrIndirect32& src ) const
	{
		if( Inst._Rd_ == 0 ) return;
		xMOV( Dest[RF_Rd], src );
	}
	
	// ------------------------------------------------------------------------
	void MoveToHiLo( const xRegister32& hireg, const xRegister32& loreg ) const
	{
		xMOV( Dest[RF_Hi], hireg );
		xMOV( Dest[RF_Lo], loreg );
	}

	// loads lo with a register, and loads hi with zero.
	void MoveToHiLo( const xRegister32& loreg ) const
	{
		xMOV( Dest[RF_Hi], 0 );
		xMOV( Dest[RF_Lo], loreg );
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


extern recBlockItemTemp m_blockspace;
extern void recIL_Block();

}
