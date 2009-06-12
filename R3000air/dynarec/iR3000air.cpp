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

namespace R3000A {

// Allocates 8 megs of ram for the IOP's generated x86 code.  [might be better set to 4 megs?]
static const uint xBlockCacheSize = 0x0800000;

struct xBlockLutAlloc
{
	uptr RAM[Ps2MemSize::IopRam / 4];
	uptr ROM[Ps2MemSize::Rom / 4];
	uptr ROM1[Ps2MemSize::Rom1 / 4];
};

static xBlockLutAlloc*	m_xBlockLutAlloc = NULL;
static u8**				m_xBlockLut = NULL;
static u8*				m_xBlock_CurPtr = NULL;

// Smallest page of the IOP's physical mapping of valid PC addresses (in this case determined
// by the size of ROM1 -- 256kb).
static const uint XlatePC_PageBitShift = 18;
static const uint XlatePC_PageSize = 0x40000;	// 1 << 18 [XlatePC_PageBitShift]
static const uint XlatePC_PageMask = XlatePC_PageSize - 1;

// Length of the translation table.
static const uint XlatePC_TableLength = 0x20000000 / XlatePC_PageSize;

// Translation table used to convert PC addresses into physical ram mappings.
PCSX2_ALIGNED16( static uptr m_tbl_TranslatePC[XlatePC_TableLength] );

iopRec_PersistentState	g_PersState;
iopRec_BlockState		g_BlockState;

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

	/*if( src.ramlen != 0 )
	{
		ValidationCopy.ExactAlloc( src.ramlen );
		memcpy( ValidationCopy.GetPtr(), src.ramcopy, src.ramlen*sizeof(u32) );
	}
	else
		ValidationCopy.Dispose();*/

	// Instruction / IL cache (which should never be zero)

	if( src.instlen != 0 )
	{
		IR.ExactAlloc( src.instlen );
		InstOrder.ExactAlloc( src.instlen );
		memcpy( IR.GetPtr(), src.icex, src.instlen*sizeof(InstConstInfoEx) );
	}
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
void __fastcall DivStallUpdater( int cycleAcc, int newstall )
{
	if( newstall == 0 ) return;		// instruction doesn't use the DivUnit.

	if( cycleAcc < iopRegs.DivUnitCycles )
		iopRegs.evtCycleCountdown -= iopRegs.DivUnitCycles - cycleAcc;

	iopRegs.DivUnitCycles = newstall;
}

// ------------------------------------------------------------------------
//
void DynGen_DivStallUpdate( int newstall, const xRegister32& tempreg=eax )
{
	// DivUnit Stalling occurs any time the current instruction has a non-zero
	// DivStall value.  Otherwise we just increment internal cycle counters
	// (which essentially behave as const-optimizations, and are written to
	// memory only when needed).

	if( newstall != 0 )
	{
		// Inline version:
		/*xMOV( tempreg, &iopRegs.DivUnitCycles );
		xSUB( tempreg, ir.DivUnit_GetCycleAccum() );
		xForwardJS8 skipStall;
		xSUB( &iopRegs.evtCycleCountdown, tempreg );
		skipStall.SetTarget();*/

		xMOV( ptr32[&iopRegs.DivUnitCycles], newstall );
	}
}

// ------------------------------------------------------------------------
// Reorders all delay slots with the branch instructions that follow them.
//
void recBlockItem::ReorderDelaySlots()
{
	DevAssume( !IR.IsDisposed(), "recBlockItem: Invalid object state when calling ReorderDelaySlots.  IR list is emptied." );

	// Note: ignore the last instruction, since if it's a branch it clearly doesn't have
	// a delay slot (it's a NOP that was optimized away).

	const uint LengthOneLess( IR.GetLength()-1 );
	for( uint i=0; i<LengthOneLess; ++i )
	{
		InstConstInfoEx& ir( IR[i] );

		if( ir.inst.HasDelaySlot() && IR[i+1].inst.IsDelaySlot() )
		{
			InstConstInfoEx& delayslot( IR[i+1] );

			// Check for branch dependency on Rd.
			
			bool isDependent = false;
			if( delayslot.inst.WritesReg( ir.inst.ReadsField( RF_Rs ) ) != RF_Unused )
				isDependent = true;
			if( delayslot.inst.WritesReg( ir.inst.ReadsField( RF_Rt ) ) != RF_Unused )
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

//////////////////////////////////////////////////////////////////////////////////////////
// ROM pages, and optimizing for them:
//
// Typically the ROm is run only once, and very briefly at that.  Analysis counted 184 ROM
// pages executed during a full startup, many of which were run only once.  Total ROM block
// executions (recompiled) tallied 18204, which is like practically nothing.  This analysis
// hlped me determine that there's no point in including special opts for ROM page re-
// compilation with the *possible* exception of just disabling recompilation for ROM pages
// entirely (ie, running them through the high-speed interpreter).
//
//////////////////////////////////////////////////////////////////////////////////////////
// Block (in-)validation Patterns of the IOP, and how to optimize for it:
//
// As it turns out, the IOP has a fail-safe mechanism for detecting and handling block
// invalidation.  In order for the R3000's cache to be cleared of any wrong-doing, the
// IOP must perform a special Cache Clearing ritual after any and all modifucations of 
// code pages.  This is detected by watching Bit 16 on the COP0's status register.  When
// the bit goes high, the IOP is in cache clearing mode.  When the bit goes back low, the
// IOP is returned to standard operation.
//
// So for fully comprehensive invalidation, we simply issue a recReset at each changing of
// the status bit.  This works in two fronts, since it also allows us to optimize memory
// accesses based on the assumption that the status of the COP0 Status bit 16 is *constant*
// .. which removes a cmp/jmp from the IOP's recompiled memory ops. :)
//
// Optimizing Block Invalidation:
// 
// The best approach (likely) for optimizing the COP0's Status16 bit going high is to use
// the interpreter for executing code during that time.  The code being executed is
// typically just a 4k memory clear which turns into an extended NOP in emulation terms
// since all SW ops are ignored during that time.  An optimized interpreter could dispatch
// the NOP'd SWs in rapid fashion and likely be much faster than the effort needed to
// generate, link, and execute x86 code for such a specialized routine.
//
// For posterity, I've included some additional info/analysis from before I was educated on
// this nifty bit's true purpose (and thus thought I had to come up with some more clever
// system for IOP invalidation):
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

extern void recIR_Pass2( const recBlockItem& irBlock );
extern void recIR_Pass3( uint numinsts );

static void recRecompile()
{
	// Look up the block...
	// (Mask the IOP address accordingly to account for the many various segments and
	//  mirrors).

	u32 masked_pc = iopRegs.pc & IopMemory::AddressMask;
	if( masked_pc < 0x800000 )
		masked_pc &= Ps2MemSize::IopRam-1;
	
	xBlocksMap::Blockmap_iterator blowme( g_PersState.xBlockMap.Map.find( masked_pc ) );

	memzero_obj( g_BlockState );
	//g_BlockState.pc = iopRegs.pc;

	if( blowme == g_PersState.xBlockMap.Map.end() )
	{
		//Console::WriteLn( "IOP First-pass block at PC: 0x%08x  (total blocks=%d)", params masked_pc, g_PersState.xBlockMap.Blocks.GetLength() );
		recIR_Block();

		jASSUME( iopRegs.evtCycleCountdown <= iopRegs.evtCycleDuration );
		if( iopRegs.evtCycleCountdown <= 0 )
			iopEvtSys.ExecutePendingEvents();

		//if( !IsIopRamPage( masked_pc ) )	// disable block checking for non-ram (rom, rom1, etc)
		//	m_blockspace.ramlen = 0;
		
		g_PersState.xBlockMap.Map[masked_pc] = g_PersState.xBlockMap.Blocks.GetLength();
		g_PersState.xBlockMap.Blocks.New().Assign( m_blockspace );
	}
	else
	{
		recBlockItem& mess( g_PersState.xBlockMap.Blocks[blowme->second] );

		if( !mess.IR.IsDisposed() )
		{
			// Second Pass Time -- Compile to x86 code!
			// ----------------------------------------

			//Console::WriteLn( "IOP Second-pass block at PC: 0x%08x  (total blocks=%d)", params masked_pc, g_PersState.xBlockMap.Blocks.GetLength() );

			// Integrity Verified... Generate X86.
			
			g_BlockState.xBlockPtr = m_xBlock_CurPtr;
			mess.ReorderDelaySlots();
			recIR_Pass2( mess );			
			recIR_Pass3( mess.IR.GetLength() );
			m_xBlock_CurPtr = xGetPtr();
			mess.IR.Dispose();
			
			uptr temp = m_tbl_TranslatePC[masked_pc>>XlatePC_PageBitShift];
			uptr* dispatch_ptr = (uptr*)(temp + (masked_pc & XlatePC_PageMask));
			*dispatch_ptr = (uptr)g_BlockState.xBlockPtr;
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
//

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
	DynFunc::JITCompile = xGetPtr();
	xCMP( ptr8[&iopRegs.IsExecuting], 0 );
	xForwardJE8 label_exitRec;
	xCALL( recRecompile );
	xForwardJump8 label_dispatcher;

	// ------------------------------------------------------------------------
	// CallEventHandler!
	// This either jumps to a RET instruction (rec exit), or falls through to the dispatcher.
	// The dispatcher is the common execution path, hence it's the fall-through choice.
	
	//label_callEvent.SetTarget();		// uncomment for conditional dispatcher profile test above.
	DynFunc::CallEventHandler = xGetPtr();
	xCALL( iopExecutePendingEvents );
	xCMP( ptr8[&iopRegs.IsExecuting], 0 );
	xForwardJE8 label_exitRec2;

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
	label_exitRec2.SetTarget();
	DynFunc::ExitRec = xGetPtr();
	xRET();
	
	HostSys::MemProtect( DynFunc::m_Dispatchers, 0x1000, Protect_ReadOnly, true );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void DynGen_BeginBranch( const IntermediateRepresentation& ir, recBlockItem& block )
{
}

void DynGen_BranchReg()
{
	xJMP( DynFunc::Dispatcher );
}

void DynGen_BranchImm( u32 newpc, JccComparisonType cctype )
{
	g_PersState.xBlockMap.AddLink( newpc, cctype );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static s32 recExecuteBlock( s32 eeCycles )
{
	iopRegs.IsExecuting = true;
	u32 eeCycleStart = iopRegs.GetCycle();
	iopEvtSys.ScheduleEvent( IopEvt_BreakForEE, (eeCycles/8)+1 );

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
	return eeCycles - ((iopRegs._cycle - eeCycleStart) * 8);
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

static void recAlloc()
{
	if( g_PersState.xBlockCache == NULL )
		g_PersState.xBlockCache = (u8*)SysMmapEx( 0x28000000, xBlockCacheSize, 0, "recAlloc(R3000A)" );

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

	const int RamPages	= Ps2MemSize::IopRam / XlatePC_PageSize;
	const int RomPages	= Ps2MemSize::Rom / XlatePC_PageSize;
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
	
	SafeSysMunmap( g_PersState.xBlockCache, xBlockCacheSize );
	safe_aligned_free( m_xBlockLutAlloc );
}

static void recReset()
{
	memset_8<0xcc, xBlockCacheSize>( g_PersState.xBlockCache );

	for( int i=0; i<sizeof(xBlockLutAlloc)/4; ++i )
		m_xBlockLut[i] = DynFunc::JITCompile;
		
	m_xBlock_CurPtr = g_PersState.xBlockCache;
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
