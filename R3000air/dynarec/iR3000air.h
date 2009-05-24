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
		api.RegMapInfo = RegMapInfo; \
		api.Emit = Emit; \
	}
	
// Implements the GetInterface function with standard templating!  (used mostly by MemoryOps)
#define IMPL_GetInterfaceTee( mess ) \
	template< mess T > \
	static void GetInterface( InstructionEmitterAPI& api ) \
	{ \
		api.RegMapInfo = RegMapInfo; \
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
		static void RegMapInfo( IntermediateRepresentation& info ) {	} \
		static void Emit( const IntermediateRepresentation& info ) { } \
		IMPL_GetInterface() \
	} \
	IMPL_RecInstAPI( name );


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

class IntermediateRepresentation;

extern void __fastcall DivStallUpdater( uint cycleAcc, int newstall );

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


// ------------------------------------------------------------------------
template< typename ContentType >
class xRegisterArray32
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
class xRegUseFlags : xRegisterArray32<bool>
{
public:
	using xRegisterArray32<bool>::operator[];

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

	// Default constructor: clobber all four basic registers (eax, edx, ecx, ebx).
	xRegUseFlags()
	{
		Basic4();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//

template< typename T >
struct RegFieldArray
{
	T Rd, Rt, Rs, Hi, Lo;

	__forceinline T& operator[]( RegField_t idx )
	{
		jASSUME( ((uint)idx) < RF_Count );
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

	__forceinline const T& operator[]( RegField_t idx ) const
	{
		jASSUME( ((uint)idx) < RF_Count );
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

	__forceinline T& operator[]( uint idx )
	{
		jASSUME( idx < RF_Count );
		return operator[]( (RegField_t)idx );
	}

	__forceinline const T& operator[]( int idx ) const
	{
		jASSUME( idx < RF_Count );
		return operator[]( (RegField_t)idx );
	}
};

static const int RegCount_Mappable = 5;
static const int RegCount_Temps = 5;

enum DynExitMap_t
{
	DynEM_Unmapped = -1,
	DynEM_Temp0 = 0,
	DynEM_Temp1,
	DynEM_Temp2,
	DynEM_Temp3,
	
	DynEM_Rs,
	DynEM_Rt,
	DynEM_Hi,
	DynEM_Lo,
	
	// Rd not included since it's never mapped on entry/source.	
};

// ------------------------------------------------------------------------
// Describes register mapping information, on a per-gpr or per-field (Rs Rd Rt) basis.
//
struct DynRegMapInfo_Entry
{
	// Tells the recompiler that the reg must be force-loaded into an x86 register on entry.
	// (recompiler will flush another reg if needed, and load this field in preparation for
	// Emit entry).  The x86 register will be picked by the recompiler.
	// Note: ForceDirect and ForceIndirect are mutually exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceDirect;

	// Tells recompiler the given instruction register field must be forced to Memory
	// (flushed).  This is commonly used as an optimization guide for cases where all x86
	// registers are modified by the instruction prior to the instruction using Rs [LWL/SWL
	// type memory ops, namely].
	// Note: ForceDirect and ForceIndirect are mutually exclusive.  Setting both to true
	// is an error, and will cause an assertion/exception.
	bool ForceIndirect;

	// Specifies known "valid" mappings on exit from the instruction emitter.  The recompiler
	// will use this to map registers more efficiently and avoid unnecessary register swapping.
	// Notes:
	//   * this is a suggestion only, and the recompiler reserves the right to map destinations
	//     however it sees fit.
	//
	//   * By default the recompiler will assume (prefer) the register used for Rs as matching
	//     Rt/Rd on exit, which is what most instructions do.
	//
	DynExitMap_t ExitMap;

	DynRegMapInfo_Entry() :
		ForceDirect( false ),
		ForceIndirect( false ),
		ExitMap( DynEM_Unmapped )
	{
	}
};

static const xRegister32 xRegAny( -2 );

struct StrictRegMapInfo_Entry
{
	// Maps a GPR into to the specific x86 register on entry to the instruction's emitter.
	// If a GPR is mapped to Empty here, it will be flushed to memory, and the Instruction
	// recompiler will be provisioned with an indirect register mapping.
	// If a GPR is mapped to xRegAny, the recompiler will select the best (most ideal) register
	// for the GPR that isn't a register already explicitly mapped to another field.
	xRegister32 EntryMap;

	// Specifies known "valid" mappings on exit from the instruction emitter.  The recompiler
	// will use this to map registers more efficiently and avoid unnecessary register swapping.
	// Note: this is a suggestion only, and the recompiler reserves the right to map destinations
	// however it sees fit.
	xRegister32 ExitMap;
	
	StrictRegMapInfo_Entry() :
		EntryMap(), ExitMap()
	{
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class RegMapInfo_Dynamic
{
public:
	class xRegInUse_t
	{
	protected:
		bool m_values[RegCount_Mappable];

	public:
		xRegInUse_t()
		{
			memzero_obj( m_values );
		}

		bool& operator[]( const xRegister32& reg );
		const bool& operator[]( const xRegister32& reg ) const;

		bool& operator[]( uint idx )
		{
			jASSUME( idx < RegCount_Mappable );
			return m_values[idx];
		}

		const bool& operator[]( uint idx ) const
		{
			jASSUME( idx < RegCount_Mappable );
			return m_values[idx];
		}
	};

public:
	// Array contents cover: Forced Direct or Indirect booleans for each GPR field, and
	// output register mappings for the dest fields.
	RegFieldArray<DynRegMapInfo_Entry> GprFields;
	
	// Set any of these true to allocate a temporary register to that slot.  Allocated
	// registers are always from the pool of eax, edx, ecx, ebx -- so it's assured your
	// allocated register will have low and high forms (al, bl, cl, dl, etc).
	//
	// Note: Setting all four of these true will only work if you are *not* using other
	// forms of register mapping, since you can't map eax (for example) and expect to
	// get four temp regs as well.  The recompiler will assert/exception.
	bool AllocTemp[4];

	// Each entry corresponds to the register in the m_tempRegs array.
	xRegInUse_t xRegInUse;

public:
	__forceinline DynRegMapInfo_Entry& operator[]( RegField_t idx )
	{
		return GprFields[idx];
	}

	__forceinline const DynRegMapInfo_Entry& operator[]( RegField_t idx ) const
	{
		return GprFields[idx];
	}

	RegMapInfo_Dynamic()
	{
		memzero_obj( AllocTemp );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
struct RegMapInfo_Strict
{
	// Specifies which x86 registers are modified by instruction code.  By default this
	// array is assigned all 'true', meaning it assumes full outwardly modification of
	// x86 registers.
	xRegUseFlags ClobbersReg;

	// Entry and exit map specifications for each GPR field.
	RegFieldArray<StrictRegMapInfo_Entry> GprFields;

	__forceinline StrictRegMapInfo_Entry& operator[]( RegField_t idx )
	{
		return GprFields[idx];
	}

	__forceinline const StrictRegMapInfo_Entry& operator[]( RegField_t idx ) const
	{
		return GprFields[idx];
	}
	
	void EntryMapHiLo( const xRegister32& srchi, const xRegister32& srclo )
	{
		GprFields.Hi.EntryMap = srchi;
		GprFields.Lo.EntryMap = srclo;
	}

	void ExitMapHiLo( const xRegister32& srchi, const xRegister32& srclo )
	{
		GprFields.Hi.ExitMap = srchi;
		GprFields.Lo.ExitMap = srclo;
	}

};

// ------------------------------------------------------------------------
// Register mapping options for each instruction emitter implementation
//
class RegisterMappingOptions
{
public:
	bool IsStrictMode;

	RegMapInfo_Dynamic DynMap;
	RegMapInfo_Strict StatMap;

	// Set to true to inform the recompiler that it can swap Rs and Rt freely.  This option
	// takes effect even if Rs or Rt have been forced to registers, or mapped to specific
	// registers.
	bool CommutativeSources;

	RegisterMappingOptions() :
		IsStrictMode( false ),
		CommutativeSources( false )
	{
	}

	RegMapInfo_Dynamic& UseDynMode()		{ IsStrictMode = false; return DynMap; }
	RegMapInfo_Strict& UseStrictMode()		{ IsStrictMode = true; return StatMap; }

	/*bool IsRequiredRegOnEntry( const xRegister32& reg ) const
	{
		for( int i=0; i<RF_Count; ++i )
		{
			if( GprFields[i].EntryMap == reg ) return true;
		}

		return false;
	}*/
};

// ------------------------------------------------------------------------
//
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
	void (*RegMapInfo)( IntermediateRepresentation& info );
	
	// Emits code for an instruction.
	// The instruction should assume that the register mappings specified in MapRegisters.inmaps
	// are assigned on function entry.  Results should be written back to GPRs using functions
	// info.MoveToRt(), info.MoveToRd(), or info.MoveToHiLo() -- and need not match the opti-
	// mization hints given in MapRegister.outmaps.
	void (*Emit)( const IntermediateRepresentation& info );
};


// ------------------------------------------------------------------------
//
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

// Temporary register from slot zero (32-bit variety)
#define tmp0reg info.TempReg(0)
#define tmp1reg info.TempReg(1)		// temp 32-bit register from slot 1
#define tmp2reg info.TempReg(2)		// temp 32-bit register from slot 2
#define tmp3reg info.TempReg(3)		// temp 32-bit register from slot 3

// Temporary register from slot zero (8-bit variety)
// Note: this is the *lo* register, such as AL, BL, etc.
#define tmp0reg8 info.TempReg8(0)
#define tmp1reg8 info.TempReg8(1)		// temp 8-bit register from slot 1
#define tmp2reg8 info.TempReg8(2)		// temp 8-bit register from slot 2
#define tmp3reg8 info.TempReg8(3)		// temp 8-bit register from slot 3

#define RegRs info.Src[RF_Rs]
#define RegRt info.Src[RF_Rt]

#define DestRegRd info.Dest[RF_Rd]
#define DestRegRt info.Dest[RF_Rd]

// Rt is ambiguous -- it can be Src or Dest style:

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class IntermediateRepresentation
{
public:
	// Mapping State of the GPRs upon entry to the recompiled instruction code generator.
	// The recompiler will do necessary "busywork" needed to move registers from the
	// dest mapping of the previous instruction to the src mapping of this instruction.
	// source operand can either be register or memory.
	xDirectOrIndirect32 Src[34];

	// Mapping state of the GPRs upon exit of the recompiled instruction code generator.
	// The recompiler will do necessary "busywork" needed to move registers from the
	// dest mapping here to the src mapping of the next instruction.
	// Dest operand can either be register or memory.
	xDirectOrIndirect32 Dest[34];

	s32 ixImm;
	InstructionRecMess Inst;		// raw instruction information.
	InstructionEmitterAPI Emitface;
	RegisterMappingOptions RegOpts;
	
	// Set true by the recompiler if the CommunativeSources flag is true and the recompiler
	// found it advantageous to swap them.  Typically code needn't check this flag, however
	// it can be useful for instructions that use conditionals (xCMP, etc) and need to swap
	// the conditional accordingly.
	bool IsSwappedSources;

	bool m_do_not_touch[34];

protected:
	xRegister32 m_TempReg[4];
	xRegister8 m_TempReg8[4];

public:
	IntermediateRepresentation() {}

	IntermediateRepresentation( const InstructionConstOpt& src ) :
		Inst( src ),
		ixImm( Inst.SignExtendsImm() ? Inst._Opcode_.Imm() : Inst._Opcode_.ImmU() ),
		IsSwappedSources( false )
	{
		memzero_obj( m_do_not_touch );
		Inst.GetRecInfo();

		if( Inst.IsConstRt() && Inst.IsConstRs() )
		{
			Inst.API.ConstRsRt( Emitface );
		}
		else if( Inst.IsConstRt() )
		{
			Inst.API.ConstRt( Emitface );
		}
		else if( Inst.IsConstRs() )
		{
			Inst.API.ConstRs( Emitface );
		}
		else
		{
			Inst.API.ConstNone( Emitface );
		}

		Emitface.RegMapInfo( *this );
	}
	
	xRegister32 DynRegs_FindUnmappable( const xDirectOrIndirect32 maparray[34] ) const;
	void		DynRegs_UnmapForcedIndirects( const RegMapInfo_Dynamic& dyno );
	void		DynRegs_AssignTempReg( int tempslot, int regslot );

	void StrictRegs_UnmapClobbers( const RegMapInfo_Strict& stro );


	void MapTemporaryRegs();
	void UnmapTemporaryRegs();

	void UnmapReg( xDirectOrIndirect32 maparray[34], const xRegister32& reg );
	bool IsMappedReg( const xDirectOrIndirect32 maparray[34], const xRegister32& reg ) const;
	xRegister32 FindFreeTempReg( xDirectOrIndirect32 maparray[34] ) const;

	const xRegister32& TempReg( uint tempslot ) const
	{
		jASSUME( tempslot < 4 );
		jASSUME( !m_TempReg[tempslot].IsEmpty() );	// make sure register was allocated properly!
		return m_TempReg[tempslot];
	}

	const xRegister8& TempReg8( uint tempslot ) const
	{
		jASSUME( tempslot < 4 );
		jASSUME( !m_TempReg8[tempslot].IsEmpty() );	// make sure register was allocated properly!
		return m_TempReg8[tempslot];
	}

public:
	s32 GetImm() const { return ixImm; }

	bool SignExtendsResult() const { return Inst.SignExtendResult(); }

	bool IsConstRs() const { return Inst.IsConstRs(); }
	bool IsConstRt() const { return Inst.IsConstRt(); }

	int GetConstRs() const
	{
		jASSUME( Inst.IsConstRs() );
		return Inst.ConstVal_Rs;
	}

	int GetConstRt() const
	{
		jASSUME( Inst.IsConstRt() );
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

	// ------------------------------------------------------------------------
	template< typename T >
	void SignExtendedMove( const xRegister32& dest, const ModSibStrict<T>& src ) const
	{
		if( Inst.SignExtendResult() )
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
		if( SignExtendsResult() )
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

	// Important!  You should generally use this instead of the expanded Dest, as you
	// must be careful to check for instances of the zero register.
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
extern iopRecState m_RecState;

extern void recIR_Block();

}
