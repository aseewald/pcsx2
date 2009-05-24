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

using namespace x86Emitter;

namespace R3000A {

iopRecState m_RecState;

//////////////////////////////////////////////////////////////////////////////////////////
//
class xBlocksMap
{
public:
	typedef std::map<u32, s32> Blockmap_t;
	typedef Blockmap_t::iterator Blockmap_iterator;

	SafeList<recBlockItem> Blocks;

	// Mapping of pc (u32) to recBlockItem* (s32).  The recBlockItem pointers are not absolute!
	// They are relative to the Blocks array above.
	// Implementation Note: This could be replaced with a hash and would, likely, be more
	// efficient.  However for the hash to be efficient it needs to ensure a fairly regular
	// dispersal of the IOP code addresses across the span of a 32 bit hash, and I'm just too
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
//

// Allocates 8 megs of ram for the IOP's generated x86 code.  [might be better set to 4 megs?]
static const uint xBlockCacheSize = 0x0800000;

struct xBlockLutAlloc
{
	uptr RAM[Ps2MemSize::IopRam / 4];
	uptr ROM[Ps2MemSize::Rom / 4];
	uptr ROM1[Ps2MemSize::Rom1 / 4];
};

static xBlockLutAlloc* m_xBlockLutAlloc = NULL;
static u8** m_xBlockLut = NULL;
static xBlocksMap m_xBlockMap;

namespace DynFunc
{
	// allocate one page for our naked dispatcher functions.
	// this *must* be a full page, since we'll give it execution permission later.
	// If it were smaller than a page we'd end up allowing execution rights on some
	// other vars additionally (bad!).
	PCSX2_ALIGNED( 0x1000, static u8 m_Dispatchers[0x1000] );

	u8* JITCompile = NULL;
	u8* CallEventHandler = NULL;
	u8* Dispatcher = NULL;
	u8* ExitRec = NULL;		// just a ret!
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void recBlockItem::Assign( const recBlockItemTemp& src )
{
	// Note: unchecked pages (ROM, ROM1, or other read-only non-RAM pages) specify a
	// ramlen of 0.. so handle it accordingly:

	if( src.ramlen != 0 )
	{
		ValidationCopy.ExactAlloc( src.ramlen );
		memcpy( ValidationCopy.GetPtr(), src.ramcopy, src.ramlen*sizeof(u32) );
	}
	else
		ValidationCopy.Dispose();

	// Instruction / IL cache (which should never be zero)

	jASSUME( src.instlen != 0 );
	IL.ExactAlloc( src.instlen );
	memcpy( IL.GetPtr(), src.inst, src.instlen*sizeof(InstructionConstOpt) );
	clears++;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void xJumpLink::SetTarget( const void* target ) const 
{
	u8* oldptr = xGetPtr();
	xSetPtr( xPtr );
	xJcc( ccType, target );
	xSetPtr( oldptr );
}

recBlockItem& xBlocksMap::_getItem( u32 pc )
{
	Blockmap_iterator blah( Map.find( pc ) );

	if( blah == Map.end() )
	{
		// Create a new block in the Blocks array for our item
		Map[pc] = Blocks.GetLength();
		return Blocks.New();
	}
	else
		return Blocks[blah->second];
}

void xBlocksMap::AddLink( u32 pc, JccComparisonType cctype )
{
	recBlockItem& destItem( _getItem( pc ) );

	const xJumpLink& newlink( destItem.DependentLinks.AddNew( xJumpLink( xGetPtr(), cctype ) ) );
	newlink.SetTarget( m_xBlockLut[pc / 4] );

	if( destItem.x86len != 0 )
	{
		// Sanity Check: if block is already compiled, then the BlockPtr should *not* be dispatcher.
		jASSUME( m_xBlockLut[pc / 4] != DynFunc::Dispatcher );
	}
}


// ------------------------------------------------------------------------
// cycleAcc - accumulated cycles since last stall update.
//
void __fastcall DivStallUpdater( uint cycleAcc, int newstall )
{
	if( cycleAcc < iopRegs.DivUnitCycles )
		iopRegs.cycle += iopRegs.DivUnitCycles - cycleAcc;

	iopRegs.DivUnitCycles = newstall;
}

// ------------------------------------------------------------------------
//
void DynGen_DivStallUpdate( int stallcycles, const xRegister32& tempreg=eax )
{
	// DivUnit Stalling occurs any time the current instruction has a non-zero
	// DivStall value.  Otherwise we just increment internal cycle counters
	// (which essentially behave as const-optimizations, and are written to
	// memory only when needed).

	if( stallcycles != 0 )
	{
		// Inline version:
		xMOV( tempreg, &iopRegs.DivUnitCycles );
		xSUB( tempreg, m_RecState.GetScaledDivCycles() );
		xForwardJS8 skipStall;
		xADD( &iopRegs.cycle, tempreg );
		skipStall.SetTarget();

		m_RecState.DivCycleAccum = 0;
	}
	else
		m_RecState.DivCycleInc();
}

static __forceinline bool intExceptionTest()
{
	if( psxHu32(0x1078) == 0 ) return false;
	if( (psxHu32(0x1070) & psxHu32(0x1074)) == 0 ) return false;
	if( (iopRegs.CP0.n.Status & 0xFE01) < 0x401 ) return false;

	iopException(0, iopRegs.IsDelaySlot );
	return true;
}

static u32 EventHandler()
{
	if( iopTestCycle( iopRegs.NextsCounter, iopRegs.NextCounter ) )
	{
		psxRcntUpdate();
	}

	// start the next branch at the next counter event by default
	// the interrupt code below will assign nearer branches if needed.
	iopRegs.NextBranchCycle = iopRegs.NextsCounter+iopRegs.NextCounter;

	if (iopRegs.interrupt)
	{
		iopEventTestIsActive = true;
		_iopTestInterrupts();
		iopEventTestIsActive = false;
	}

	if( intExceptionTest() )
	{
		iopRegs.pc			 = iopRegs.VectorPC;
		iopRegs.VectorPC	+= 4;
		iopRegs.IsDelaySlot	 = false;
	}

	if( iopTestCycle( iopRegs.eeCycleStart, iopRegs.eeCycleDelta ) ) return 1;

	iopSetNextBranch( iopRegs.eeCycleStart, iopRegs.eeCycleDelta );
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//

// Block (in-)validation Patterns of the IOP, and how to optimize for it:
//
// Currently the IOP recompiler uses a simple (and fail-safe) approach of manually checking
// the integrity of a block when the block is run.  I've done quite a bit of careful re-
// search in deciding on this approach.
//
//
// Q: Why not Virtual Protection like what the EErec uses?
// A: The IOP tends to mix code and data in the same 4k page with relative certainty, such
//    that very few pages won't end up being manually-checked anyway.  I suspect this is
//    for two reasons; the IOP's code is likely often written in ASM as much as it is
//    in C, and the modules are self-contained replacable units which probably just store
//    their data at a location relative to the code (and well-packed to conserve memory,
//    since the IOP only has 2MB).
//
// Q: Why not clear invalidated blocks on write?
// A: Complexity, for one.  Self-checked manual blocks are safe against all forms of
//    invalidation, including DMA transfers and writes from the EE's special mapping of
//    IOP ram.  Using clears means having to carefully ensure all code that can write IOP's
//    ram is accompanied with a Clear call.  Furthermore, block lookups are generally slow
//    and not very cache friendly .. so there's a (good) chance manual self-checks are
//    the same speed or faster anyway.
// 

extern void recIR_Pass2( const SafeArray<InstructionConstOpt>& iList );

static void recRecompile()
{
	// Look up the block...
	// (Mask the IOP address accordingly to account for the many various segments and
	//  mirrors).

	u32 masked_pc = iopRegs.pc & IopMemory::AddressMask;
	if( masked_pc < 0x800000 )
		masked_pc &= Ps2MemSize::IopRam-1;
	
	xBlocksMap::Blockmap_iterator blowme( m_xBlockMap.Map.find( masked_pc ) );

	memzero_obj( m_RecState );
	//m_RecState.pc = iopRegs.pc;

	if( blowme == m_xBlockMap.Map.end() )
	{
		//Console::WriteLn( "IOP First-pass block at PC: 0x%08x  (total blocks=%d)", params masked_pc, m_xBlockMap.Blocks.GetLength() );
		recIR_Block();

		if( !IsIopRamPage( masked_pc ) )	// disable block checking for non-ram (rom, rom1, etc)
			m_blockspace.ramlen = 0;
		
		m_xBlockMap.Map[masked_pc] = m_xBlockMap.Blocks.GetLength();
		m_xBlockMap.Blocks.New().Assign( m_blockspace );
	}
	else
	{
		recBlockItem& mess( m_xBlockMap.Blocks[blowme->second] );

		int numinsts = mess.IL.GetLength();
		if( numinsts != 0 )
		{
			// Second Pass Time -- Compile to x86 code!
			// ----------------------------------------

			if(mess.ValidationCopy.GetLength() != 0)
			{
				// Integrity check --> if the program code has been altered just repeat the 
				// first pass interpreter.

				const u32* maskptr = (u32*)IopMemory::iopGetPhysPtr( masked_pc );
				const u32* validptr = (u32*)mess.ValidationCopy.GetPtr();

				/*if(	(memcmp( validptr, maskptr, mess.ValidationCopy.GetSizeInBytes()) != 0) )
				{
					Console::WriteLn( "-/- IOP Clearing block at PC: 0x%08x", params masked_pc );

					recIR_Block();
					mess.Assign( m_blockspace );
					return;
				}*/
			}

			//Console::WriteLn( "IOP Second-pass block at PC: 0x%08x  (total blocks=%d)", params masked_pc, m_xBlockMap.Blocks.GetLength() );

			// Integrity Verified... Generate X86.

			recIR_Pass2( mess.IL );
			mess.IL.Dispose();
		}
		recIR_Block();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
//


// Smallest page of the IOP's physical mapping of valid PC addresses (in this case determined
// by the size of ROM1 -- 256kb).
static const uint XlatePC_PageBitShift = 18;
static const uint XlatePC_PageSize = 0x40000;	// 1 << 18 [XlatePC_PageBitShift]
static const uint XlatePC_PageMask = XlatePC_PageSize - 1;

// Length of the translation table.
static const uint XlatePC_TableLength = 0x20000000 / XlatePC_PageSize;

// Translation table used to convert PC addresses into physical ram mappings.
uptr m_tbl_TranslatePC[XlatePC_TableLength];

static void DynGen_Dispatcher()
{
	xMOV( eax, &iopRegs.pc );
	xMOV( ebx, eax );

	// Mask out the "bottom" 2 bits so that they're cleared when we shift EAX later.
	// [alternative is shifting EAX completely and using a SIB index, but that's not
	//  nearly as clever!]

	uint eaxMask = IopMemory::AddressMask & ~(3<<(XlatePC_PageBitShift-2));
	xAND( eax, eaxMask );
	xAND( ebx, XlatePC_PageMask );

	xSHR( eax, XlatePC_PageBitShift-2 );
	xMOV( ecx, ptr[m_tbl_TranslatePC + eax] );
	xJMP( ptr32[ebx + ecx] );
}

static void DynGen_Functions()
{
	// In case init gets called multiple times:
	HostSys::MemProtect( DynFunc::m_Dispatchers, 0x1000, Protect_ReadWrite, false );
	memset8_obj<0xcc>( DynFunc::m_Dispatchers );
	xSetPtr( DynFunc::m_Dispatchers );

	// ------------------------------------------------------------------------
	// JITCompile
	//
	// Note: perform an event test!  If an event is pending, handle it first (which will
	// then re-dispatch based on the new PC coming out of the event handler)

	DynFunc::JITCompile = xGetPtr();
	xMOV( eax, &iopRegs.cycle );
	xSUB( eax, &iopRegs.NextBranchCycle );
	xForwardJNS8 label_callEvent;
	xCALL( recRecompile );
	xForwardJump8 label_dispatcher;

	// ------------------------------------------------------------------------
	// CallEventHandler!
	// This either jumps to a RET instruction (rec exit), or falls through to the dispatcher.
	// The dispatcher is the common execution path, hence it's the fall-through choice.
	
	label_callEvent.SetTarget();		// uncomment for conditional dispatcher profile test above.
	DynFunc::CallEventHandler = xGetPtr();
	xCALL( EventHandler );
	xTEST( eax, eax );
	xForwardJNZ8 label_exitRec;

	// ------------------------------------------------------------------------
	// Dispatcher!
	// assign the dispatcher to 64 bytes, which allows us to access it using either indirect
	// (DynFunc::Dispatcher), or direct (DynFunc::m_Dispatchers[64]) addressing.  The direct
	// mode is used from the recExecuteBlock as it's more efficient.

	label_dispatcher.SetTarget();		// comment for conditional dispatcher test above
	//label_dispatcher2.SetTarget();
	DynFunc::Dispatcher = xGetPtr();
	DynGen_Dispatcher();

	// ------------------------------------------------------------------------
	// ExitRec
	// This ret() is reached if EventHandler returns non-zero, which signals that the
	// IOP code execution needs to break, and return control to the EE.
	//
	label_exitRec.SetTarget();
	DynFunc::ExitRec = xGetPtr();
	xRET();
	
	HostSys::MemProtect( DynFunc::m_Dispatchers, 0x1000, Protect_ReadOnly, true );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void DynGen_BeginBranch( const IntermediateRepresentation& il, recBlockItem& block )
{
	xMOV( ptr32[iopRegs.pc], il.Inst._Pc_ );
	xMOV( eax, &iopRegs.cycle );
	xADD( eax, m_RecState.GetScaledBlockCycles() );
	xMOV( &iopRegs.cycle, eax );
	xSUB( eax, &iopRegs.NextBranchCycle );
	xJNS( DynFunc::CallEventHandler );
}

void DynGen_BranchReg()
{
	xJMP( DynFunc::Dispatcher );
}

void DynGen_BranchImm( u32 newpc, JccComparisonType cctype )
{
	m_xBlockMap.AddLink( newpc, cctype );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static s32 recExecuteBlock( s32 eeCycles )
{
	iopRegs.IsExecuting = true;
	iopRegs.eeCycleStart = iopRegs.cycle;
	iopRegs.eeCycleDelta = eeCycles/8;

	iopSetNextBranchDelta( iopRegs.eeCycleDelta );

	// Optimization note : Compared pushad against manually pushing the regs one-by-one.
	// Manually pushing is faster, especially on Core2's and such. :)

	__asm
	{
		push ebx
		push esi
		push edi
		//push ebp		// probably not needed.

		call [DynFunc::Dispatcher]

		//pop ebp
		pop edi
		pop esi
		pop ebx
	}
	
	iopRegs.IsExecuting = false;
	return eeCycles - ((iopRegs.cycle - iopRegs.eeCycleStart) * 8);
}

static void recExecute()
{
	assert( false );
	throw Exception::LogicError( "Don't do this !  IOP can't support your wishes!" );
}

static void TranslatePC_SetPages( u32* target, u32 startpc, int size )
{
	// make sure start and length are aligned to our page granularity:
	jASSUME( (startpc % XlatePC_PageSize) == 0 );
	jASSUME( (size % XlatePC_PageSize) == 0 );
	
	int startpage = startpc / XlatePC_PageSize;
	int numpages = size / XlatePC_PageSize;

	// note: we do BitShift-2 because there are 4 bytes per valid PC
	// (so we shift them off to shorten the table)
	for( int i=0; i<numpages; ++i )
		m_tbl_TranslatePC[i+startpage] = (uptr)&target[i << (XlatePC_PageBitShift-2)];
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static u8* m_xBlockCache = NULL;

static void recAlloc()
{
	if( m_xBlockCache == NULL )
		m_xBlockCache = (u8*)SysMmapEx( 0x28000000, xBlockCacheSize, 0, "recAlloc(R3000A)" );

	if( m_xBlockLutAlloc == NULL )
		m_xBlockLutAlloc = (xBlockLutAlloc*) _aligned_malloc( sizeof( xBlockLutAlloc ), 4096 );

	if( m_xBlockLutAlloc == NULL )
		throw Exception::OutOfMemory( "iR3000A Init > xBlockLut allocation failed (out of memory?)" );

	// m_xBlockLut serves as a 'flat' indexer into the BlockLutAlloc's various parts and pieces.
	m_xBlockLut = (u8**)m_xBlockLutAlloc;

	// Initialize the PC Translation Table!
	// -------------------------------------
	// The translation table is based on the IOP Memory Map, and covers just those areas of
	// memory that map to RAM or ROM.  [executable code sources]
	// Each element in the TranslatePC table covers a 256k page of memory.

	const int RamPages = Ps2MemSize::IopRam / XlatePC_PageSize;
	const int RomPages = Ps2MemSize::Rom / XlatePC_PageSize;
	const int Rom1Pages = Ps2MemSize::Rom1 / XlatePC_PageSize;

	// regular ram is a 2mb mapping, mirrored four times across the lower portion of RAM.
	for( int i=0; i<RamPages; ++i )
		TranslatePC_SetPages( m_xBlockLutAlloc->RAM, i*Ps2MemSize::IopRam, Ps2MemSize::IopRam );

	TranslatePC_SetPages( m_xBlockLutAlloc->ROM,  0x1fc00000, Ps2MemSize::Rom );
	TranslatePC_SetPages( m_xBlockLutAlloc->ROM1, 0x1e000000, Ps2MemSize::Rom1 );

	DynGen_Functions();

	//ProfilerRegisterSource( "IOPRec", m_xBlockCache, xBlockCacheSize );
}

static void recShutdown()
{
	//ProfilerTerminateSource( "IOPRec" );
	
	SafeSysMunmap( m_xBlockCache, xBlockCacheSize );
	safe_aligned_free( m_xBlockLutAlloc );
}

static void recReset()
{
	memset_8<0xcc, xBlockCacheSize>( m_xBlockCache );

	for( int i=0; i<sizeof(xBlockLutAlloc)/4; ++i )
		m_xBlockLut[i] = DynFunc::JITCompile;
}

static void recClear( u32, u32 )
{
}

}

R3000Acpu psxRec = {
	R3000A::recAlloc,
	R3000A::recReset,
	R3000A::recExecute,
	R3000A::recExecuteBlock,
	R3000A::recClear,
	R3000A::recShutdown
};
