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

#include "R3000airIntermediate.h"
#include "iR3000airRegMapping.h"

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
#define IMPL_RecPlacebo( name ) \
class rec##name : public x86IntRep { \
public: \
	rec##name( const IntermediateRepresentation& src ) : x86IntRep( src ) {} \
	void Emit() const {} \
}


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

namespace R3000A {

using namespace x86Emitter;
class x86IntRep;

extern void __fastcall DivStallUpdater( int cycleAcc, int newstall );

// ------------------------------------------------------------------------
// Temporary register from slot zero (32-bit variety)
//
#define tmp0reg TempReg(0)
#define tmp1reg TempReg(1)		// temp 32-bit register from slot 1
#define tmp2reg TempReg(2)		// temp 32-bit register from slot 2
#define tmp3reg TempReg(3)		// temp 32-bit register from slot 3

// Temporary register from slot zero (8-bit variety)
// Note: this is the *lo* register, such as AL, BL, etc.
#define tmp0reg8 TempReg8(0)
#define tmp1reg8 TempReg8(1)		// temp 8-bit register from slot 1
#define tmp2reg8 TempReg8(2)		// temp 8-bit register from slot 2
#define tmp3reg8 TempReg8(3)		// temp 8-bit register from slot 3

#define RegRs SrcField(RF_Rs)
#define RegRt SrcField(RF_Rt)

#define DestRegRd DestField(RF_Rd)
#define DestRegRt DestField(RF_Rt)

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class x86IntRep
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

	// Strict indicator for each gpr's source mapping.  When 'true' secondary and
	// tertiary passes through the recompiler will not unmap these entries under any
	// circumstance.  Anything left 'false' can be unmapped and left as an indirect.
	bool RequiredSrcMapping[34];
	
	// Optional forced flush of volatile x86 registers during Strict mapping operations.
	// Registers in this list are flushed *prior* to instruction execution, instead of
	// being handled as writebacks at the conclusion of instruction execution.
	// Note: Currently xForceFlush and Dest(map) are somewhat redundant in purpose.
	// I'm sure there's a way to simplify and remove one or the other, but my head's
	// unable to find it currently.
	xTempRegsArray32<bool> xForceFlush;

	IntermediateRepresentation& Inst;
	RegisterMappingOptions RegOpts;
	s32 ixImm;
	
	// Set true by the recompiler if the CommunativeSources flag is true and the recompiler
	// found it advantageous to swap them.  Typically code needn't check this flag, however
	// it can be useful for instructions that use conditionals (xCMP, etc) and need to swap
	// the conditional accordingly.
	bool IsSwappedSources;
	
	// Special dependency slot mapping of EBP.  When mapped, EBP is a *read only* register
	// which is not flushed to memory.
	MipsGPRs_t m_EbpLoadMap;
	MipsGPRs_t m_EbpReadMap;

protected:
	xRegister32 m_TempReg[4];
	xRegister8 m_TempReg8[4];


public:
	x86IntRep() :
		Inst( *(IntermediateRepresentation*)0 )		// One of the more evil hacks you'll ever see?
	{}

	x86IntRep( const xAddressReg& idxreg );
	x86IntRep( const IntermediateRepresentation& src );

	void DynarecAssert( bool condition, const char* msg, bool inReleaseMode=false ) const
	{
		Inst.DynarecAssert( condition, msg, inReleaseMode );
	}

	const xDirectOrIndirect32& SrcField( RegField_t field ) const
	{
		int gpr = Inst.ReadsField( field );
		DynarecAssert( gpr != -1, "Attempted to read from a field that is not a valid input field for this instruction." );

		return Src[gpr];
	}

	const xDirectOrIndirect32& DestField( RegField_t field ) const
	{
		int gpr = Inst.WritesField( field );
		DynarecAssert( gpr != -1, "Attempted to write to a field that is not a valid outpt field for this instruction." );
		return Dest[gpr];
	}

	const xRegister32& TempReg( uint tempslot ) const
	{
		jASSUME( tempslot < 4 );
		DynarecAssert( !m_TempReg[tempslot].IsEmpty(),
			"Referenced an unallocated temp register slot.", true );
		return m_TempReg[tempslot];
	}

	const xRegister8& TempReg8( uint tempslot ) const
	{
		jASSUME( tempslot < 4 );
		DynarecAssert( !m_TempReg[tempslot].IsEmpty(),
			"Referenced an unallocated temp register slot.", true );
		return m_TempReg8[tempslot];
	}

	// --------------------------------------------------------
	//            Register Mapping / Allocation API
	// --------------------------------------------------------
	static ModSibStrict<u32> GetMemIndexer( MipsGPRs_t gpridx );
	static ModSibStrict<u32> GetMemIndexer( int gpridx );

	void SetFlushingState();
	
	//void		DynRegs_UnmapForcedIndirects( const RegMapInfo_Dynamic& dyno );
	void		DynRegs_AssignTempReg( int tempslot, const xRegister32& reg );

	void StrictRegs_UnmapClobbers( const RegMapInfo_Strict& stro );

	void MapTemporaryRegs();
	void UnmapTemporaryRegs();

	void UnmapReg( xDirectOrIndirect32 maparray[34], const xRegister32& reg );
	bool IsMappedReg( const xDirectOrIndirect32 maparray[34], const xRegister32& reg ) const;
	xRegister32 FindFreeTempReg( xDirectOrIndirect32 maparray[34] ) const;

	void _DynGen_MapStrictMultimap() const;

public:
	s32 GetImm() const { return ixImm; }

	bool SignExtendsResult() const { return Inst.MipsInst.SignExtendResult(); }

	bool IsConstRs() const { return Inst.MipsInst.IsConstRs(); }
	bool IsConstRt() const { return Inst.MipsInst.IsConstRt(); }

	int GetConstRs() const
	{
		return Inst.GetConstRs();
	}

	int GetConstRt() const
	{
		return Inst.GetConstRt();
	}
	
	bool IsConst( MipsGPRs_t gpr ) const
	{
		return Inst.MipsInst.IsConstGpr[gpr];
	}
	
	int GetConstVal( MipsGPRs_t gpr ) const
	{
		return Inst.MipsInst.ConstVal[gpr];
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const xRegister32& destreg ) const 
	{
		if( IsConstRs() )
			xMOV( destreg, GetConstRs() );
		else
			xMOV( destreg, SrcField( RF_Rs ) );
	}

	void MoveRtTo( const xRegister32& destreg ) const 
	{
		if( IsConstRt() )
			xMOV( destreg, GetConstRt() );
		else
			xMOV( destreg, SrcField( RF_Rt ) );	
	}

	// ------------------------------------------------------------------------
	template< typename T >
	void SignExtendedMove( const xRegister32& dest, const ModSibStrict<T>& src ) const
	{
		if( SignExtendsResult() )
			xMOVSX( dest, src );
		else
			xMOVZX( dest, src );
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

	// ------------------------------------------------------------------------
	// Important!  You should generally use this instead of the expanded Dest, as you
	// must be careful to check for instances of the zero register.
	//
	void MoveToRt( const xDirectOrIndirect32& src ) const
	{
		if( Inst.MipsInst._Rt_ == 0 ) return;
		xMOV( DestField(RF_Rt), src );
	}

	void MoveToRd( const xDirectOrIndirect32& src ) const
	{
		if( Inst.MipsInst._Rd_ == 0 ) return;
		xMOV( DestField(RF_Rd), src );
	}
	
	// ------------------------------------------------------------------------
	void MoveToHiLo( const xRegister32& hireg, const xRegister32& loreg ) const
	{
		// [TODO] : Add liveness checks for Hi/Lo status.
		xMOV( DestField(RF_Hi), hireg );
		xMOV( DestField(RF_Lo), loreg );
	}

	// loads lo with a register, and loads hi with zero.
	void MoveToHiLo( const xRegister32& loreg ) const
	{
		// [TODO] : Add liveness checks for Hi/Lo status.
		xMOV( DestField(RF_Hi), 0 );
		xMOV( DestField(RF_Lo), loreg );
	}

	// Emit -- Emits code for an instruction.
	// The instruction should assume that the register mappings specified in MapRegisters.inmaps
	// are assigned on function entry.  Results should be written back to GPRs using functions
	// info.MoveToRt(), info.MoveToRd(), or info.MoveToHiLo() -- and need not match the opti-
	// mization hints given in MapRegister.outmaps.
	virtual void Emit() const;
	
	static void PlacementNew( u8** dest, const IntermediateRepresentation& src );
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

static const int MaxCyclesPerBlock			= 64;
static const int MaxInstructionsPerBlock	= 128;

//////////////////////////////////////////////////////////////////////////////////////////
// iopRec_FirstPassConstAnalysis - Temporary workspace buffer used to reduce the number of heap allocations
// required during block recompilation.
//
struct iopRec_FirstPassConstAnalysis
{
	InstructionConstOptimizer icex[MaxInstructionsPerBlock];
	int instlen;

	void InterpretBlock();
};

//////////////////////////////////////////////////////////////////////////////////////////
//
struct recBlockItem : public NoncopyableObject
{
	uint x86len;	// length of the recompiled block
	uint clears;	// number of times this block has been cleared and recompiled
	
	// First-pass result allocation.  If size is non-zero, then we're on our second pass
	// and this data should be recompiled into x86 code for direct execution.
	SafeArray<InstructionConstOptimizer> InstOptInfo;
	
	// A list of all block links dependent on this block.  If this block moves, then all links
	// in this list need to have their x86 jump instructions rewritten.
	SafeList<xJumpLink> DependentLinks;

	recBlockItem() :
		x86len( 0 )
	,	clears( 0 )
	,	InstOptInfo( "recBlockItem::InstOptInfo" )
	,	DependentLinks( 4, "recBlockItem::DependentLinks" )
	{
		InstOptInfo.ChunkSize = 32;
		DependentLinks.ChunkSize = 8;
	}
	
	void Assign( const iopRec_FirstPassConstAnalysis& src );
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class iopRec_IntermediateState
{
public:
	SafeList<IntermediateRepresentation> inst;

	// instruction execution order, used to re-order delay slots (may be used for more
	// advanced reordering later)
	SafeList<int> InstOrder;

public:
	iopRec_IntermediateState() :
		inst( "IntermediateBlock::inst" )
	{
		inst.ChunkSize = 32;
		InstOrder.ChunkSize = 32;
	}

	void GenerateIR( const recBlockItem& block );
	int GetLength() const { return inst.GetLength(); }

	const IntermediateRepresentation& GetInst( uint idx ) const
	{
		jASSUME( (idx < (uint)GetLength()) && (InstOrder[idx] < GetLength()) );
		return inst[InstOrder[idx]];
	}

	IntermediateRepresentation& GetInst( uint idx )
	{
		jASSUME( (idx < (uint)GetLength()) && (InstOrder[idx] < GetLength()) );
		return inst[InstOrder[idx]];
	}

protected:
	void ReorderDelaySlots();
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class xBlocksMap
{
public:
	typedef std::map<u32, s32> Blockmap_t;
	typedef Blockmap_t::iterator Blockmap_iterator;
	typedef Blockmap_t::const_iterator Blockmap_iterator_const;

	SafeList<recBlockItem> Blocks;

	// Mapping of pc (u32) to recBlockItem* (s32).  The recBlockItem pointers are not absolute!
	// They are relative to the Blocks array above.
	//
	// Implementation Note: This could be replaced with a hash and would, likely, be more
	// efficient.  However for the hash to be efficient it needs to ensure a fairly regular
	// dispersal of the IOP pc addresses across the span of a 32 bit hash, and I'm just too
	// lazy to bother figuring such an algorithm out right now.  [this is not speed-critical
	// code anyway, so wouldn't much matter even if it were faster]
	Blockmap_t Map;

public:
	xBlocksMap() :
		Blocks( 4096, "recBlocksMap::Blocks" ),
		Map()
	{
		Blocks.New();
	}

	// pc - ps2 address target of the x86 jump instruction
	// x86addr - x86 address of the x86 jump instruction
	void AddLink( u32 pc, JccComparisonType cctype );
	
protected:
	recBlockItem& _getItem( u32 pc );
};

//////////////////////////////////////////////////////////////////////////////////////////
// iopRec_PersistentState - houses variables which persist across the entire duration of 
// the emulation session.  Most of these vars are only cleared/reset upon a call to
// recReset.
//
struct iopRec_PersistentState
{
	// Block Cache is a largwe, flat, execution-allowed array.  x86 code is written here
	// during recompilation and then is run during execution.  Code is written in blocks
	// which typically span from an entry point (jump/branch target) to an exit point
	// (jump or branch instruction).
	u8*			xBlockCache;

	// Pointer to the current/next block in the xBlockCache.  During Block recompilation this
	// points to the current block.  At the end of Block recompilation it is assigned a
	// pointer to the position where the *next* block is to be emitted.
	u8*			xBlockPtr;
	
	// The blockmap!  Consists of an allocation of blocks plus an associative mapping of
	// iopRegs.pc to the block compilation in question, for fast pc->x86ptr block lookups.
	xBlocksMap	xBlockMap;

	iopRec_PersistentState() :
		xBlockCache( NULL )
	,	xBlockPtr( NULL )
	,	xBlockMap()
	{
	}
	
	s8* xGetBlockPtr( u32 pc ) const
	{
		xBlocksMap::Blockmap_iterator_const block = xBlockMap.Map.find( pc );
		if( block == xBlockMap.Map.end() ) return NULL;
		return (s8*)block->second;
	}
};

struct GPR_UsePair
{
	int gpr;
	int used;
};

//////////////////////////////////////////////////////////////////////////////////////////
// iopRecState - contains data representing the current known state of the emu during the
// process of block recompilation.  This struct is cleared when the recompilation of a
// block begins, and tracks data for that block across both Interp and Rec passes.
//
struct iopRec_BlockState
{
	int BlockCycleAccum;
	int DivCycleAccum;
	u32 pc;				// pc for the currently recompiling/emitting instruction
	u8* xBlockPtr;		// base address of the current block being generated (pointer to the recBlock array)

	// -------------------------------------------------------------------
	int GetScaledBlockCycles() const
	{
		// [TODO] : Implement speedhacking.
		return BlockCycleAccum * 1;
	}

	// ------------------------------------------------------------------------
	int GetScaledDivCycles() const
	{
		return DivCycleAccum * 1;
	}

	// ------------------------------------------------------------------------
	__releaseinline void IncCycleAccum()
	{
		BlockCycleAccum++;
		DivCycleAccum++;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class iopRec_x86BlockState
{
public:
	SafeList< x86IntRep > m_xir;
	
protected:
	int gpr_map_edi;	// block-wide mapping of EDI to a GPR (-1 for no mapping)

public:
	void AssignBlock( const iopRec_IntermediateState& irBlock );
	void RegisterMapper();
	void EmitSomeExecutableGoodness();

protected:
	void MapBlockWideRegisters( const iopRec_IntermediateState& irBlock );

	// -------------------------------------------------------------------
	bool HasMappedEdi() const
	{
		return gpr_map_edi != -1;
	}

	// -------------------------------------------------------------------
	void SetMapEdi( const GPR_UsePair& high )
	{
		gpr_map_edi = ( high.used < 1 || high.gpr == 0 ) ? -1 : high.gpr;
	}

	// -------------------------------------------------------------------
	bool IsEdiMappedTo( uint gpr ) const
	{
		return gpr == gpr_map_edi;
	}

	// -------------------------------------------------------------------
	// Generates x86 code for loading block-scoped register mappings.
	// (currently only Edi, may include Ebp at a later date).
	void DynGen_InitMappedRegs() const
	{
		if( gpr_map_edi > 0 )
			xMOV( edi, x86IntRep::GetMemIndexer( gpr_map_edi ) );
	}

	const xRegister32& GetMappableReg( uint idx ) const;
	const int GetMappableRegsLength() const { return HasMappedEdi() ? 5 : 4; }

	void PerformDynamicRegisterMapping( x86IntRep& cir );
	void PerformDynamicRegisterMapping_Exit( x86IntRep& cir );
	void PerformStrictRegisterMapping( x86IntRep& cir );
	void PerformStrictRegisterMapping_Exit( x86IntRep& cir );
	void ForceDestFlushes( x86IntRep& cir );
	
	void _DynGen_PreInstructionFlush( const x86IntRep& ir, xRegisterArray32<bool>& xIsDirty );
	void _DynGen_MapRegisters( const x86IntRep& ir, const x86IntRep& previr, xRegisterArray32<bool>& xIsDirty );
};


//////////////////////////////////////////////////////////////////////////////////////////

extern iopRec_PersistentState	g_PersState;
extern iopRec_BlockState		g_BlockState;

namespace DynFunc
{
	extern u8* JITCompile;
	extern u8* CallEventHandler;
	extern u8* Dispatcher;
	extern u8* ExitRec;		// just a ret!
}

extern JccComparisonType BccToJcc( BccComparisonType bcc, bool swapCond=false );

extern void recIR_FirstPassInterpreter();

}
