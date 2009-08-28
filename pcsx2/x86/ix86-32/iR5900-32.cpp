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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "Memory.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iR5900AritImm.h"
#include "iR5900Arit.h"
#include "iR5900MultDiv.h"
#include "iR5900Shift.h"
#include "iR5900Branch.h"
#include "iR5900Jump.h"
#include "iR5900LoadStore.h"
#include "iR5900Move.h"

#include "BaseblockEx.h"

#include "iMMI.h"
#include "iFPU.h"
#include "iCOP0.h"
#include "sVU_Micro.h"
#include "VU.h"
#include "VUmicro.h"

#include "sVU_zerorec.h"
#include "vtlb.h"

#include "SamplProf.h"

#include "NakedAsm.h"
#include "Dump.h"

using namespace x86Emitter;
using namespace R5900;

// used to disable register freezing during cpuBranchTests (registers
// are safe then since they've been completely flushed)
bool g_EEFreezeRegs = false;

u32 maxrecmem = 0;
uptr recLUT[0x10000];
uptr hwLUT[0x10000];

#define HWADDR(mem) (hwLUT[mem >> 16] + (mem))

u32 s_nBlockCycles = 0; // cycles of current block recompiling
//u8* dyna_block_discard_recmem=0;

u32 pc;			         // recompiler pc
int branch;		         // set for branch

PCSX2_ALIGNED16(GPR_reg64 g_cpuConstRegs[32]) = {0};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec

#define X86
static const int RECCONSTBUF_SIZE = 16384 * 2; // 64 bit consts in 32 bit units

static u8 *recMem = NULL;			// the recompiled blocks will be here
static u32* recConstBuf = NULL;			// 64-bit pseudo-immediates
static BASEBLOCK *recRAM = NULL;		// and the ptr to the blocks here
static BASEBLOCK *recROM = NULL;		// and here
static BASEBLOCK *recROM1 = NULL;		// also here
static u32 *recRAMCopy = NULL;
void JITCompile();
static BaseBlocks recBlocks((uptr)JITCompile);
static u8* recPtr = NULL;
static u32 *recConstBufPtr = NULL;
EEINST* s_pInstCache = NULL;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = NULL;
static BASEBLOCKEX* s_pCurBlockEx = NULL;
u32 s_nEndBlock = 0; // what pc the current block ends
static bool s_nBlockFF;

// save states for branches
GPR_reg64 s_saveConstRegs[32];
static u16 s_savex86FpuState;
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0, s_saveRegHasLive1 = 0, s_saveRegHasSignExt = 0;
static EEINST* s_psaveInstInfo = NULL;

static u32 s_savenBlockCycles = 0;

#ifdef PCSX2_DEBUG
static u32 dumplog = 0;
#else
#define dumplog 0
#endif

static void iBranchTest(u32 newpc = 0xffffffff);
static void ClearRecLUT(BASEBLOCK* base, int count);
static u32 eeScaleBlockCycles();

#ifdef PCSX2_VM_COISSUE
static u8 _eeLoadWritesRs(u32 tempcode)
{
	switch(tempcode>>26) {
		case 26: // ldl
		case 27: // ldr
		case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
		case 55: // LD
		case 30: // lq
			return ((tempcode>>21)&0x1f)==((tempcode>>16)&0x1f); // rs==rt
	}
	return 0;
}

static u8 _eeIsLoadStoreCoIssue(u32 firstcode, u32 secondcode)
{
	switch(firstcode>>26) {
		case 34: // lwl
			return (secondcode>>26)==38;
		case 38: // lwr
			return (secondcode>>26)==34;
		case 42: // swl
			return (secondcode>>26)==46;
		case 46: // swr
			return (secondcode>>26)==42;
		case 26: // ldl
			return (secondcode>>26)==27;
		case 27: // ldr
			return (secondcode>>26)==26;
		case 44: // sdl
			return (secondcode>>26)==45;
		case 45: // sdr
			return (secondcode>>26)==44;

		case 32: case 33: case 35: case 36: case 37: case 39:
		case 55: // LD

		// stores
		case 40: case 41: case 43:
		case 63: // sd
			return (secondcode>>26)==(firstcode>>26);

		case 30: // lq
		case 31: // sq
		case 49: // lwc1
		case 57: // swc1
		case 54: // lqc2
		case 62: // sqc2
			return (secondcode>>26)==(firstcode>>26);
	}
	return 0;
}

static u8 _eeIsLoadStoreCoX(u32 tempcode)
{
	switch( tempcode>>26 ) {
		case 30: case 31: case 49: case 57: case 55: case 63:
			return 1;
	}
	return 0;
}
#endif

void _eeFlushAllUnused()
{
	int i;
	for(i = 0; i < 34; ++i) {
		if( pc < s_nEndBlock ) {
			if( (g_pCurInstInfo[1].regs[i]&EEINST_USED) )
				continue;
		}
		else if( (g_pCurInstInfo[0].regs[i]&EEINST_USED) )
			continue;

		if( i < 32 && GPR_IS_CONST1(i) ) _flushConstReg(i);
		else {
			_deleteMMXreg(MMX_GPR+i, 1);
			_deleteGPRtoXMMreg(i, 1);
		}
	}

	//TODO when used info is done for FPU and VU0
	for(i = 0; i < iREGCNT_XMM; ++i) {
		if( xmmregs[i].inuse && xmmregs[i].type != XMMTYPE_GPRREG )
			_freeXMMreg(i);
	}
}

u32* _eeGetConstReg(int reg)
{
	assert( GPR_IS_CONST1( reg ) );

	if( g_cpuFlushedConstReg & (1<<reg) )
		return &cpuRegs.GPR.r[ reg ].UL[0];

	// if written in the future, don't flush
	if( _recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-pc)/4, XMMTYPE_GPRREG, reg) )
		return recGetImm64(g_cpuConstRegs[reg].UL[1], g_cpuConstRegs[reg].UL[0]);

	_flushConstReg(reg);
	return &cpuRegs.GPR.r[ reg ].UL[0];
}

void _eeMoveGPRtoR(x86IntRegType to, int fromgpr)
{
	if( fromgpr == 0 )
		XOR32RtoR( to, to );	// zero register should use xor, thanks --air
	else if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoR( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;

		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 && (xmmregs[mmreg].mode&MODE_WRITE)) {
			SSE2_MOVD_XMM_to_R(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 && (mmxregs[mmreg].mode&MODE_WRITE) ) {
			MOVD32MMXtoR(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(to, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
		}
	}
}

void _eeMoveGPRtoM(u32 to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoM( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;

		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_M32(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVDMMXtoM(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoM(to, EAX );
		}
	}
}

void _eeMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoRm( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;

		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_Rm(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVD32MMXtoRm(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoRm( to, EAX );
		}
	}
}

int _flushXMMunused()
{
	int i;
	for (i=0; i<iREGCNT_XMM; i++) {
		if (!xmmregs[i].inuse || xmmregs[i].needed || !(xmmregs[i].mode&MODE_WRITE) ) continue;

		if (xmmregs[i].type == XMMTYPE_GPRREG ) {
			//if( !(g_pCurInstInfo->regs[xmmregs[i].reg]&EEINST_USED) ) {
			if( !_recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-pc)/4, XMMTYPE_GPRREG, xmmregs[i].reg) ) {
				_freeXMMreg(i);
				xmmregs[i].inuse = 1;
				return 1;
			}
		}
	}

	return 0;
}

int _flushMMXunused()
{
	int i;
	for (i=0; i<iREGCNT_MMX; i++) {
		if (!mmxregs[i].inuse || mmxregs[i].needed || !(mmxregs[i].mode&MODE_WRITE) ) continue;

		if( MMX_ISGPR(mmxregs[i].reg) ) {
			//if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
			if( !_recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-pc)/4, XMMTYPE_GPRREG, mmxregs[i].reg-MMX_GPR) ) {
				_freeMMXreg(i);
				mmxregs[i].inuse = 1;
				return 1;
			}
		}
	}

	return 0;
}

int _flushUnusedConstReg()
{
	int i;
	for(i = 1; i < 32; ++i) {
		if( (g_cpuHasConstReg & (1<<i)) && !(g_cpuFlushedConstReg&(1<<i)) &&
			!_recIsRegWritten(g_pCurInstInfo+1, (s_nEndBlock-pc)/4, XMMTYPE_GPRREG, i) ) {

			// check if will be written in the future
			MOV32ItoM((uptr)&cpuRegs.GPR.r[i].UL[0], g_cpuConstRegs[i].UL[0]);
			MOV32ItoM((uptr)&cpuRegs.GPR.r[i].UL[1], g_cpuConstRegs[i].UL[1]);
			g_cpuFlushedConstReg |= 1<<i;
			return 1;
		}
	}

	return 0;
}

// Some of the generated MMX code needs 64-bit immediates but x86 doesn't
// provide this.  One of the reasons we are probably better off not doing
// MMX register allocation for the EE.
u32* recGetImm64(u32 hi, u32 lo)
{
	u32 *imm64; // returned pointer
	static u32 *imm64_cache[509];
	int cacheidx = lo % (sizeof imm64_cache / sizeof *imm64_cache);
	//static int count; count++;

	imm64 = imm64_cache[cacheidx];
	if (imm64 && imm64[0] == lo && imm64[1] == hi)
		return imm64;

	if (recConstBufPtr >= recConstBuf + RECCONSTBUF_SIZE) {
		// TODO: flag an error in recompilation which would reset the recompiler
		// immediately and recompile the current block again.  There is currently
		// no way to do this, so have a last ditch attempt at making things sane
		// and return some nonsense if that fails.
		for (u32 *p = recConstBuf; p < recConstBuf + RECCONSTBUF_SIZE; p += 2)
			if (p[0] == lo && p[1] == hi) {
				imm64_cache[cacheidx] = p;
				return p;
			}

		return recConstBuf;
	}

	imm64 = recConstBufPtr;
	recConstBufPtr += 2;
	imm64_cache[cacheidx] = imm64;

	imm64[0] = lo;
	imm64[1] = hi;

	//Console::Notice("Consts allocated: %d of %u", params (recConstBufPtr - recConstBuf) / 2, count);

	return imm64;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static const int REC_CACHEMEM = 0x01000000;
static void __fastcall dyna_block_discard(u32 start,u32 sz);

// memory allocation handle for the entire BASEBLOCK and stack allocations.
static u8* m_recBlockAlloc = NULL;

static const uint m_recBlockAllocSize =
	(((Ps2MemSize::Base + Ps2MemSize::Rom + Ps2MemSize::Rom1) / 4) * sizeof(BASEBLOCK))
+	RECCONSTBUF_SIZE * sizeof(u32) + Ps2MemSize::Base;

static void recAlloc()
{
	// Hardware Requirements Check...

	if ( !( x86caps.hasMultimediaExtensions  ) )
		throw Exception::HardwareDeficiency( "Processor doesn't support MMX" );

	if ( !( x86caps.hasStreamingSIMDExtensions ) )
		throw Exception::HardwareDeficiency( "Processor doesn't support SSE" );

	if ( !( x86caps.hasStreamingSIMD2Extensions ) )
		throw Exception::HardwareDeficiency( "Processor doesn't support SSE2" );

	if( recMem == NULL )
	{
		// Note: the VUrec depends on being able to grab an allocation below the 0x10000000 line,
		// so we give the EErec an address above that to try first as it's basemem address, hence
		// the 0x20000000 pick.

		const uint cachememsize = REC_CACHEMEM+0x1000;
		recMem = (u8*)SysMmapEx( 0x20000000, cachememsize, 0, "recAlloc(R5900)" );
	}

	if( recMem == NULL )
		throw Exception::OutOfMemory( "R5900-32 > failed to allocate recompiler memory." );

	// Goal: Allocate BASEBLOCKs for every possible branch target in PS2 memory.
	// Any 4-byte aligned address makes a valid branch target as per MIPS design (all instructions are
	// always 4 bytes long).

    if( m_recBlockAlloc == NULL )
		m_recBlockAlloc = (u8*) _aligned_malloc( m_recBlockAllocSize, 4096 );

	if( m_recBlockAlloc == NULL )
		throw Exception::OutOfMemory( "R5900-32 Init > Failed to allocate memory for BASEBLOCK tables." );

	u8* curpos = m_recBlockAlloc;
	recRAM = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Base / 4) * sizeof(BASEBLOCK);
	recROM = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom / 4) * sizeof(BASEBLOCK);
	recROM1 = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom1 / 4) * sizeof(BASEBLOCK);
	recConstBuf = (u32*)curpos; curpos += RECCONSTBUF_SIZE * sizeof(u32);
	recRAMCopy = (u32*)curpos;

	if( s_pInstCache == NULL )
	{
		s_nInstCacheSize = 128;
		s_pInstCache = (EEINST*)malloc( sizeof(EEINST) * s_nInstCacheSize );
	}

	if( s_pInstCache == NULL )
		throw Exception::OutOfMemory( "R5900-32 Init > failed to allocate memory for pInstCache." );

	// No errors.. Proceed with initialization:

	ProfilerRegisterSource( "EERec", recMem, REC_CACHEMEM+0x1000 );

	x86FpuState = FPU_STATE;
}

PCSX2_ALIGNED16( static u16 manual_page[Ps2MemSize::Base >> 12] );
PCSX2_ALIGNED16( static u8 manual_counter[Ps2MemSize::Base >> 12] );

////////////////////////////////////////////////////
void recResetEE( void )
{
	Console::Status( "Issuing EE/iR5900-32 Recompiler Reset [mem/structure cleanup]" );

	maxrecmem = 0;

	if (IsDevBuild)
		memset_8<0xcc, REC_CACHEMEM>(recMem); // 0xcc is INT3
	memzero_ptr<m_recBlockAllocSize - Ps2MemSize::Base>( m_recBlockAlloc ); // Excluding the 32mb ram copy
	memzero_ptr<RECCONSTBUF_SIZE * sizeof(u32)>(recConstBuf);
	memzero_obj( manual_page );
	memzero_obj( manual_counter );
	ClearRecLUT((BASEBLOCK*)m_recBlockAlloc,
		(((Ps2MemSize::Base + Ps2MemSize::Rom + Ps2MemSize::Rom1) / 4)));

	if( s_pInstCache )
		memset( s_pInstCache, 0, sizeof(EEINST)*s_nInstCacheSize );

	recBlocks.Reset();
	mmap_ResetBlockTracking();

#ifdef _MSC_VER
	__asm emms;
#else
    __asm__("emms");
#endif

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(recLUT, 0, 0, 0, i, 0);

	for ( int i = 0x0000; i < 0x0200; i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x0000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x2000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x3000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x8000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xa000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xb000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xc000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xd000, i, i);
	}

	for ( int i = 0x1fc0; i < 0x2000; i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for ( int i = 0x1e00; i < 0x1e04; i++ )
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	x86SetPtr(recMem);

	recPtr = recMem;
	recConstBufPtr = recConstBuf;
	x86FpuState = FPU_STATE;

	branch = 0;
	SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);
}

static void recShutdown( void )
{
	ProfilerTerminateSource( "EERec" );
	recBlocks.Reset();

	SafeSysMunmap( recMem, REC_CACHEMEM );
	safe_aligned_free( m_recBlockAlloc );
	recRAM = recROM = recROM1 = NULL;
	recConstBuf = NULL;
	recRAMCopy = NULL;

	safe_free( s_pInstCache );
	s_nInstCacheSize = 0;
}

#ifdef _MSC_VER
#pragma warning(disable:4731) // frame pointer register 'ebp' modified by inline assembly code
#endif

void recStep( void )
{
}

extern "C"
{
void recEventTest()
{
#ifdef PCSX2_DEVBUILD
	// dont' remove this check unless doing an official release
	if( g_globalXMMSaved || g_globalMMXSaved)
	{
		DevCon::Error("Pcsx2 Foopah!  Frozen regs have not been restored!!!");
		DevCon::Error("g_globalXMMSaved = %d,g_globalMMXSaved = %d",params g_globalXMMSaved, g_globalMMXSaved);
	}
	assert( !g_globalXMMSaved && !g_globalMMXSaved);
#endif

	// Perform counters, interrupts, and IOP updates:
	_cpuBranchTest_Shared();

#ifdef PCSX2_DEVBUILD
	assert( !g_globalXMMSaved && !g_globalMMXSaved);
#endif
}
}

////////////////////////////////////////////////////

static u32 g_lastpc = 0;
u32 g_EEDispatchTemp;

#ifdef _MSC_VER

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static __naked void JITCompile()
{
	__asm {
		mov esi, dword ptr [cpuRegs.pc]
		push esi
		call recRecompile
		add esp, 4
		mov ebx, esi
		shr esi, 16
		mov ecx, dword ptr [recLUT+esi*4]
		jmp dword ptr [ecx+ebx]
	}
}

static __naked void JITCompileInBlock()
{
	__asm {
		jmp JITCompile
	}
}

// called when jumping to variable pc address
static void __naked DispatcherReg()
{
	__asm {
		mov eax, dword ptr [cpuRegs.pc]
		mov ebx, eax
		shr eax, 16
		mov ecx, dword ptr [recLUT+eax*4]
		jmp dword ptr [ecx+ebx]
	}
}

// [TODO] : Replace these functions with x86Emitter-generated code and we can compound this
// function and DispatcherReg() into a fast fall-through case (removes the DispatcerReg jump
// in this function, since execution will just fall right into the DispatcherReg implementation).
//
static void __naked DispatcherEvent()
{
	__asm
	{
		call recEventTest;
		jmp DispatcherReg;
	}
}

void recExecute()
{
	g_EEFreezeRegs = true;

	// Enter an endless loop, which is only escapable via C++ exception handling.
	// The loop is needed because some things in the rec use "ret" as a shortcut to
	// invoking DispatcherReg.  These things are code bits which are called infrequently,
	// such as dyna_block_discard and dyna_page_reset.

	while( true )
	{
		// Optimization note : Compared pushad against manually pushing the regs one-by-one.
		// Manually pushing is faster, especially on Core2's and such. :)

		__asm
		{
			push ebx
			push esi
			push edi
			push ebp

			call DispatcherReg

			pop ebp
			pop edi
			pop esi
			pop ebx
		}
	}
	g_EEFreezeRegs = false;
}

#else // _MSC_VER

__forceinline void recExecute()
{
	// Optimization note : Compared pushad against manually pushing the regs one-by-one.
	// Manually pushing is faster, especially on Core2's and such. :)
	g_EEFreezeRegs = true;
	__asm__
	(
		".intel_syntax noprefix\n"
		"push ebx\n"
		"push esi\n"
		"push edi\n"
		"push ebp\n"

		"call DispatcherReg\n"

		"pop ebp\n"
		"pop edi\n"
		"pop esi\n"
		"pop ebx\n"
		".att_syntax\n"
	);
	g_EEFreezeRegs = false;
}

#endif

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

////////////////////////////////////////////////////
void recSYSCALL( void ) {
	MOV32ItoM( (uptr)&cpuRegs.code, cpuRegs.code );
	MOV32ItoM( (uptr)&cpuRegs.pc, pc );
	iFlushCall(FLUSH_NODESTROY);
	CALLFunc( (uptr)R5900::Interpreter::OpcodeImpl::SYSCALL );

	CMP32ItoM((uptr)&cpuRegs.pc, pc);
	j8Ptr[0] = JE8(0);
	ADD32ItoM((uptr)&cpuRegs.cycle, eeScaleBlockCycles());
	JMP32((uptr)DispatcherReg - ( (uptr)x86Ptr + 5 ));
	x86SetJ8(j8Ptr[0]);
	//branch = 2;
}

////////////////////////////////////////////////////
void recBREAK( void ) {
	MOV32ItoM( (uptr)&cpuRegs.code, cpuRegs.code );
	MOV32ItoM( (uptr)&cpuRegs.pc, pc );
	iFlushCall(FLUSH_EVERYTHING);
	CALLFunc( (uptr)R5900::Interpreter::OpcodeImpl::BREAK );

	CMP32ItoM((uptr)&cpuRegs.pc, pc);
	j8Ptr[0] = JE8(0);
	ADD32ItoM((uptr)&cpuRegs.cycle, eeScaleBlockCycles());
	RET();
	x86SetJ8(j8Ptr[0]);
	//branch = 2;
}

} } }		// end namespace R5900::Dynarec::OpcodeImpl

// Clears the recLUT table so that all blocks are mapped to the JIT recompiler by default.
static __releaseinline void ClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count; i++)
		base[i].SetFnptr((uptr)JITCompile);
}

void recClear(u32 addr, u32 size)
{
	BASEBLOCKEX* pexblock;
	BASEBLOCK* pblock;

	//why the hell?
#if 1
	// necessary since recompiler doesn't call femms/emms
#ifdef __INTEL_COMPILER
                __asm__("emms");
#else
        #ifdef _MSC_VER
                if (x86caps.has3DNOWInstructionExtensions) __asm femms;
                else __asm emms;
        #else
                if( x86caps.has3DNOWInstructionExtensions )__asm__("femms");
                else
                        __asm__("emms");
        #endif
#endif
#endif

	if ((addr) >= maxrecmem || !(recLUT[(addr) >> 16] + (addr & ~0xFFFFUL)))
		return;
	addr = HWADDR(addr);

	int blockidx = recBlocks.LastIndex(addr + size * 4 - 4);

	if (blockidx == -1)
		return;

	u32 lowerextent = (u32)-1, upperextent = 0, ceiling = (u32)-1;

	pexblock = recBlocks[blockidx + 1];
	if (pexblock)
		ceiling = pexblock->startpc;

	while (pexblock = recBlocks[blockidx]) {
		u32 blockstart = pexblock->startpc;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		pblock = PC_GETBLOCK(blockstart);

		if (pblock == s_pCurBlock) {
			blockidx--;
			continue;
		}

		if (blockend <= addr) {
			lowerextent = max(lowerextent, blockend);
			break;
		}

		lowerextent = min(lowerextent, blockstart);
		upperextent = max(upperextent, blockend);
		// This might end up inside a block that doesn't contain the clearing range,
		// so set it to recompile now.  This will become JITCompile if we clear it.
		pblock->SetFnptr((uptr)JITCompileInBlock);
		recBlocks.Remove(blockidx--);
	}

	upperextent = min(upperextent, ceiling);

#ifdef PCSX2_DEVBUILD
	for (int i = 0; pexblock = recBlocks[i]; i++) {
		if (s_pCurBlock == PC_GETBLOCK(pexblock->startpc))
			continue;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		if (pexblock->startpc >= addr && pexblock->startpc < addr + size * 4
		 || pexblock->startpc < addr && blockend > addr) {
			Console::Error("Impossible block clearing failure");
			jASSUME(0);
		}
	}
#endif

	if (upperextent > lowerextent)
		ClearRecLUT(PC_GETBLOCK(lowerextent), (upperextent - lowerextent) / 4);
}

// check for end of bios
void CheckForBIOSEnd()
{
	MOV32MtoR(EAX, (int)&cpuRegs.pc);

	CMP32ItoR(EAX, 0x00200008);
	j8Ptr[0] = JE8(0);

	CMP32ItoR(EAX, 0x00100008);
	j8Ptr[1] = JE8(0);

	// return
	j8Ptr[2] = JMP8(0);

	x86SetJ8( j8Ptr[0] );
	x86SetJ8( j8Ptr[1] );

	// bios end
	RET();

	x86SetJ8( j8Ptr[2] );
}

static int *s_pCode;

void SetBranchReg( u32 reg )
{
	branch = 1;

	if( reg != 0xffffffff ) {
//		if( GPR_IS_CONST1(reg) )
//			MOV32ItoM( (uptr)&cpuRegs.pc, g_cpuConstRegs[reg].UL[0] );
//		else {
//			int mmreg;
//
//			if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, reg, MODE_READ)) >= 0 ) {
//				SSE_MOVSS_XMM_to_M32((u32)&cpuRegs.pc, mmreg);
//			}
//			else if( (mmreg = _checkMMXreg(MMX_GPR+reg, MODE_READ)) >= 0 ) {
//				MOVDMMXtoM((u32)&cpuRegs.pc, mmreg);
//				SetMMXstate();
//			}
//			else {
//				MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ reg ].UL[ 0 ] );
//				MOV32RtoM((u32)&cpuRegs.pc, EAX);
//			}
//		}
		_allocX86reg(ESI, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
		_eeMoveGPRtoR(ESI, reg);

		recompileNextInstruction(1);

		if( x86regs[ESI].inuse ) {
			assert( x86regs[ESI].type == X86TYPE_PCWRITEBACK );
			MOV32RtoM((int)&cpuRegs.pc, ESI);
			x86regs[ESI].inuse = 0;
		}
		else {
			MOV32MtoR(EAX, (u32)&g_recWriteback);
			MOV32RtoM((int)&cpuRegs.pc, EAX);
		}
	}

//	CMP32ItoM((u32)&cpuRegs.pc, 0);
//	j8Ptr[5] = JNE8(0);
//	CALLFunc((uptr)tempfn);
//	x86SetJ8( j8Ptr[5] );

	iFlushCall(FLUSH_EVERYTHING);

	iBranchTest();
}

void SetBranchImm( u32 imm )
{
	branch = 1;

	assert( imm );

	// end the current block
	iFlushCall(FLUSH_EVERYTHING);
	iBranchTest(imm);
}

void SaveBranchState()
{
	s_savex86FpuState = x86FpuState;
	s_savenBlockCycles = s_nBlockCycles;
	memcpy(s_saveConstRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
	s_saveHasConstReg = g_cpuHasConstReg;
	s_saveFlushedConstReg = g_cpuFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;
	s_saveRegHasLive1 = g_cpuRegHasLive1;
	s_saveRegHasSignExt = g_cpuRegHasSignExt;

	// save all mmx regs
	memcpy_fast(s_saveMMXregs, mmxregs, sizeof(mmxregs));
	memcpy_fast(s_saveXMMregs, xmmregs, sizeof(xmmregs));
}

void LoadBranchState()
{
	x86FpuState = s_savex86FpuState;
	s_nBlockCycles = s_savenBlockCycles;

	memcpy(g_cpuConstRegs, s_saveConstRegs, sizeof(g_cpuConstRegs));
	g_cpuHasConstReg = s_saveHasConstReg;
	g_cpuFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo = s_psaveInstInfo;
	g_cpuRegHasLive1 = g_cpuPrevRegHasLive1 = s_saveRegHasLive1;
	g_cpuRegHasSignExt = g_cpuPrevRegHasSignExt = s_saveRegHasSignExt;

	// restore all mmx regs
	memcpy_fast(mmxregs, s_saveMMXregs, sizeof(mmxregs));
	memcpy_fast(xmmregs, s_saveXMMregs, sizeof(xmmregs));
}

void iFlushCall(int flushtype)
{
	_freeX86regs();

	if( flushtype & FLUSH_FREE_XMM )
		_freeXMMregs();
	else if( flushtype & FLUSH_FLUSH_XMM)
		_flushXMMregs();

	if( flushtype & FLUSH_FREE_MMX )
		_freeMMXregs();
	else if( flushtype & FLUSH_FLUSH_MMX)
		_flushMMXregs();

	if( flushtype & FLUSH_CACHED_REGS )
		_flushConstRegs();

	if (x86FpuState==MMX_STATE) {
		if (x86caps.has3DNOWInstructionExtensions) FEMMS();
		else EMMS();
		x86FpuState=FPU_STATE;
	}
}

//void testfpu()
//{
//	int i;
//	for(i = 0; i < 32; ++i ) {
//		if( fpuRegs.fpr[i].UL== 0x7f800000 || fpuRegs.fpr[i].UL == 0xffc00000) {
//			Console::WriteLn("bad fpu: %x %x %x", params i, cpuRegs.cycle, g_lastpc);
//		}
//
//		if( VU0.VF[i].UL[0] == 0xffc00000 || //(VU0.VF[i].UL[1]&0xffc00000) == 0xffc00000 ||
//			VU0.VF[i].UL[0] == 0x7f800000) {
//			Console::WriteLn("bad vu0: %x %x %x", params i, cpuRegs.cycle, g_lastpc);
//		}
//	}
//}


static u32 scaleBlockCycles_helper()
{
	// Note: s_nBlockCycles is 3 bit fixed point.  Divide by 8 when done!

	// Let's not scale blocks under 5-ish cycles.  This fixes countless "problems"
	// caused by sync hacks and such, since games seem to care a lot more about
	// these small blocks having accurate cycle counts.

	if( s_nBlockCycles <= (5<<3) || (EmuConfig.Speedhacks.EECycleRate == 0) )
		return s_nBlockCycles >> 3;

	uint scalarLow, scalarMid, scalarHigh;

	// Note: larger blocks get a smaller scalar, to help keep
	// them from becoming "too fat" and delaying branch tests.

	switch( EmuConfig.Speedhacks.EECycleRate )
	{
		case 0:	return s_nBlockCycles >> 3;

		case 1:		// Sync hack x1.5!
			scalarLow = 5;
			scalarMid = 7;
			scalarHigh = 5;
		break;

		case 2:		// Sync hack x2
			scalarLow = 7;
			scalarMid = 9;
			scalarHigh = 7;
		break;

		jNO_DEFAULT
	}

	const u32 temp = s_nBlockCycles * (
		(s_nBlockCycles <= (10<<3)) ? scalarLow :
		((s_nBlockCycles > (21<<3)) ? scalarHigh : scalarMid )
	);

	return temp >> (3+2);
}

static u32 eeScaleBlockCycles()
{
	// Ensures block cycles count is never less than 1:
	u32 retval = scaleBlockCycles_helper();
	return (retval < 1) ? 1 : retval;
}


// Generates dynarec code for Event tests followed by a block dispatch (branch).
// Parameters:
//   newpc - address to jump to at the end of the block.  If newpc == 0xffffffff then
//   the jump is assumed to be to a register (dynamic).  For any other value the
//   jump is assumed to be static, in which case the block will be "hardlinked" after
//   the first time it's dispatched.
//
//   noDispatch - When set true, then jump to Dispatcher.  Used by the recs
//   for blocks which perform exception checks without branching (it's enabled by
//   setting "branch = 2";
static void iBranchTest(u32 newpc)
{
	if( g_ExecBiosHack ) CheckForBIOSEnd();

	// Check the Event scheduler if our "cycle target" has been reached.
	// Equiv code to:
	//    cpuRegs.cycle += blockcycles;
	//    if( cpuRegs.cycle > g_nextBranchCycle ) { DoEvents(); }

	if (EmuConfig.Speedhacks.BIFC0 && s_nBlockFF)
	{
		xMOV(eax, ptr32[&g_nextBranchCycle]);
		xADD(ptr32[&cpuRegs.cycle], eeScaleBlockCycles());
		xCMP(eax, ptr32[&cpuRegs.cycle]);
		xCMOVL(eax, ptr32[&cpuRegs.cycle]);
		xMOV(ptr32[&cpuRegs.cycle], eax);

		xJMP( DispatcherEvent );
	}
	else
	{
		// Optimization -- we need to load cpuRegs.pc on static block links, but doing it inside
		// the if() block below (it would be paired with recBlocks.Link) breaks the sub/jcc
		// pairing that modern CPUs optimize (applies to all P4+ and AMD X2+ CPUs).  So let's do
		// it up here instead. :D

		if( newpc != 0xffffffff )
			xMOV( ptr32[&cpuRegs.pc], newpc );

		xMOV(eax, &cpuRegs.cycle);
		xADD(eax, eeScaleBlockCycles());
		xMOV(&cpuRegs.cycle, eax); // update cycles
		xSUB(eax, &g_nextBranchCycle);

		if (newpc == 0xffffffff)
		{
			xJNS( DispatcherEvent );
			xJMP( DispatcherReg );
		}
		else
		{
			recBlocks.Link( HWADDR(newpc), xJcc32( Jcc_Signed ) );
			xJMP( DispatcherEvent );
		}
	}

	/*
	else
	{
		xMOV(eax, &cpuRegs.cycle);
		xADD(eax, eeScaleBlockCycles());
		xMOV(&cpuRegs.cycle, eax); // update cycles
		xSUB(eax, &g_nextBranchCycle);
		if (!noDispatch)
		{
			if (newpc == 0xffffffff)
				xJS( DispatcherReg );
			else
			{
				xMOV( ptr32[&cpuRegs.pc], newpc );
				recBlocks.Link( HWADDR(newpc), xJcc32( Jcc_Signed ) );
			}
		}
	}
	xCALL( recEventTest );
	xJMP( DispatcherReg );

	*/
}

static void checkcodefn()
{
	int pctemp;

#ifdef _MSC_VER
	__asm mov pctemp, eax;
#else
    __asm__("movl %%eax, %[pctemp]" : [pctemp]"=m"(pctemp) );
#endif

	Console::Error("code changed! %x", params pctemp);
	assert(0);
}

void recompileNextInstruction(int delayslot)
{
	static u8 s_bFlushReg = 1;
	int i, count;

	s_pCode = (int *)PSM( pc );
	assert(s_pCode);

	if( IsDebugBuild )
		MOV32ItoR(EAX, pc);		// acts as a tag for delimiting recompiled instructions when viewing x86 disasm.

	cpuRegs.code = *(int *)s_pCode;
	pc += 4;

#if 0
#ifdef PCSX2_DEBUG
	CMP32ItoM((u32)s_pCode, cpuRegs.code);
	j8Ptr[0] = JE8(0);
	MOV32ItoR(EAX, pc);
	CALLFunc((uptr)checkcodefn);
	x86SetJ8( j8Ptr[ 0 ] );

	if( !delayslot ) {
		CMP32ItoM((u32)&cpuRegs.pc, s_pCurBlockEx->startpc);
		j8Ptr[0] = JB8(0);
		CMP32ItoM((u32)&cpuRegs.pc, pc);
		j8Ptr[1] = JA8(0);
		j8Ptr[2] = JMP8(0);
		x86SetJ8( j8Ptr[ 0 ] );
		x86SetJ8( j8Ptr[ 1 ] );
		PUSH32I(s_pCurBlockEx->startpc);
		ADD32ItoR(ESP, 4);
		x86SetJ8( j8Ptr[ 2 ] );
	}
#endif
#endif

	g_pCurInstInfo++;

	for(i = 0; i < iREGCNT_MMX; ++i) {
		if( mmxregs[i].inuse ) {
			assert( MMX_ISGPR(mmxregs[i].reg) );
			count = _recIsRegWritten(g_pCurInstInfo, (s_nEndBlock-pc)/4 + 1, XMMTYPE_GPRREG, mmxregs[i].reg-MMX_GPR);
			if( count > 0 ) mmxregs[i].counter = 1000-count;
			else mmxregs[i].counter = 0;
		}
	}

	for(i = 0; i < iREGCNT_XMM; ++i) {
		if( xmmregs[i].inuse ) {
			count = _recIsRegWritten(g_pCurInstInfo, (s_nEndBlock-pc)/4 + 1, xmmregs[i].type, xmmregs[i].reg);
			if( count > 0 ) xmmregs[i].counter = 1000-count;
			else xmmregs[i].counter = 0;
		}
	}

	const OPCODE& opcode = GetCurrentInstruction();

 	//assert( !(g_pCurInstInfo->info & EEINSTINFO_NOREC) );

	// if this instruction is a jump or a branch, exit right away
	if( delayslot ) {
		switch(_Opcode_) {
			case 1:
				switch(_Rt_) {
					case 0: case 1: case 2: case 3: case 0x10: case 0x11: case 0x12: case 0x13:
						Console::Notice("branch %x in delay slot!", params cpuRegs.code);
						_clearNeededX86regs();
						_clearNeededMMXregs();
						_clearNeededXMMregs();
						return;
				}
				break;

			case 2: case 3: case 4: case 5: case 6: case 7: case 0x14: case 0x15: case 0x16: case 0x17:
				Console::Notice("branch %x in delay slot!", params cpuRegs.code);
				_clearNeededX86regs();
				_clearNeededMMXregs();
				_clearNeededXMMregs();
				return;
		}
	}
	//If the COP0 DIE bit is disabled, double the cycles. Happens rarely.
	s_nBlockCycles += opcode.cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
	opcode.recompile();

	if( !delayslot ) {
		if( s_bFlushReg ) {
			//if( !_flushUnusedConstReg() ) {
				int flushed = 0;
				if( _getNumMMXwrite() > 3 ) flushed = _flushMMXunused();
				if( !flushed && _getNumXMMwrite() > 2 ) _flushXMMunused();
				s_bFlushReg = !flushed;
//			}
//			else s_bFlushReg = 0;
		}
		else s_bFlushReg = 1;
	}
	else s_bFlushReg = 1;

	//CHECK_XMMCHANGED();
	_clearNeededX86regs();
	_clearNeededMMXregs();
	_clearNeededXMMregs();

//	_freeXMMregs();
//	_freeMMXregs();
//	_flushCachedRegs();
//	g_cpuHasConstReg = 1;

	if (!delayslot && x86Ptr - recPtr > 0x1000)
		s_nEndBlock = pc;
}

static void printfn()
{
	static int lastrec = 0;
	static int curcount = 0;
	const int skip = 0;

	assert( !g_globalMMXSaved );
	assert( !g_globalXMMSaved );

    if( (dumplog&2) && g_lastpc != 0x81fc0 ) {//&& lastrec != g_lastpc ) {
		curcount++;

		if( curcount > skip ) {
			iDumpRegisters(g_lastpc, 1);
			curcount = 0;
		}

		lastrec = g_lastpc;
	}
}

u32 s_recblocks[] = {0};

void badespfn() {
	Console::Error("Bad esp!");
	assert(0);
}

// Called when a block under manual protection fails it's pre-execution integrity check.
void __fastcall dyna_block_discard(u32 start,u32 sz)
{
	DevCon::WriteLn("dyna_block_discard .. start=0x%08X  size=%d", params start, sz*4);
	recClear(start, sz);
}

// called when a block under manual protection has been run enough times to be a
// candidate for being reset under the faster vtlb write protection.
void __fastcall dyna_page_reset(u32 start,u32 sz)
{
	recClear(start & ~0xfffUL, 0x400);
	manual_counter[start >> 12]++;
	mmap_MarkCountedRamPage( start );
}

void recRecompile( const u32 startpc )
{
	u32 i = 0;
	u32 branchTo;
	u32 willbranch3 = 0;
	u32 usecop2;

#ifdef PCSX2_DEBUG
    //dumplog |= 4;
    if( dumplog & 4 )
		iDumpRegisters(startpc, 0);
#endif

	assert( startpc );

	// if recPtr reached the mem limit reset whole mem
	if ( ( (uptr)recPtr - (uptr)recMem ) >= REC_CACHEMEM-0x40000 || dumplog == 0xffffffff) {
		recResetEE();
	}
	if ( (recConstBufPtr - recConstBuf) >= RECCONSTBUF_SIZE - 64 ) {
		DevCon::WriteLn("EE recompiler stack reset");
		recResetEE();
	}

	x86SetPtr( recPtr );
	x86Align(16);
	recPtr = x86Ptr;

	s_nBlockFF = false;
	if (HWADDR(startpc) == 0x81fc0)
		s_nBlockFF = true;

	s_pCurBlock = PC_GETBLOCK(startpc);

	assert(s_pCurBlock->GetFnptr() == (uptr)JITCompile
		|| s_pCurBlock->GetFnptr() == (uptr)JITCompileInBlock);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
	assert(!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc));

	s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	assert(s_pCurBlockEx);

	branch = 0;

	// reset recomp state variables
	s_nBlockCycles = 0;
	pc = startpc;
	x86FpuState = FPU_STATE;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;
	g_cpuPrevRegHasLive1 = g_cpuRegHasLive1 = 0xffffffff;
	g_cpuPrevRegHasSignExt = g_cpuRegHasSignExt = 0;
	assert( g_cpuConstRegs[0].UD[0] == 0 );

	_initX86regs();
	_initXMMregs();
	_initMMXregs();

#ifdef PCSX2_DEBUG
	// for debugging purposes
	MOV32ItoM((uptr)&g_lastpc, pc);
	CALLFunc((uptr)printfn);

//	CMP32MtoR(EBP, (uptr)&s_uSaveEBP);
//	j8Ptr[0] = JE8(0);
//	CALLFunc((uptr)badespfn);
//	x86SetJ8(j8Ptr[0]);
#endif

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;

	while(1) {
		BASEBLOCK* pblock = PC_GETBLOCK(i);

		if(i != startpc)	// Block size truncation checks.
		{
			if( (i & 0xffc) == 0x0 )	// breaks blocks at 4k page boundaries
			{
				willbranch3 = 1;
				s_nEndBlock = i;

				//DevCon::Notice( "Pagesplit @ %08X : size=%d insts", params startpc, (i-startpc) / 4 );
				break;
			}

			if (pblock->GetFnptr() != (uptr)JITCompile && pblock->GetFnptr() != (uptr)JITCompileInBlock)
			{
				willbranch3 = 1;
				s_nEndBlock = i;
				break;
			}
		}

		//HUH ? PSM ? whut ? THIS IS VIRTUAL ACCESS GOD DAMMIT
		cpuRegs.code = *(int *)PSM(i);

		switch(cpuRegs.code >> 26) {
			case 0: // special
				if( _Funct_ == 8 || _Funct_ == 9 ) { // JR, JALR
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 1: // regimm

				if( _Rt_ < 4 || (_Rt_ >= 16 && _Rt_ < 20) ) {
					// branches
					branchTo = _Imm_ * 4 + i + 4;
					if( branchTo > startpc && branchTo < i ) s_nEndBlock = branchTo;
					else  s_nEndBlock = i+8;

					goto StartRecomp;
				}
				break;

			case 2: // J
			case 3: // JAL
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4: case 5: case 6: case 7:
			case 20: case 21: case 22: case 23:
				branchTo = _Imm_ * 4 + i + 4;
				if( branchTo > startpc && branchTo < i ) s_nEndBlock = branchTo;
				else  s_nEndBlock = i+8;

				goto StartRecomp;

			case 16: // cp0
				if( _Rs_ == 16 ) {
					if( _Funct_ == 24 ) { // eret
						s_nEndBlock = i+4;
						goto StartRecomp;
					}
				}
				// Fall through!
				// COP0's branch opcodes line up with COP1 and COP2's

			case 17: // cp1
			case 18: // cp2
				if( _Rs_ == 8 ) {
					// BC1F, BC1T, BC1FL, BC1TL
					// BC2F, BC2T, BC2FL, BC2TL
					branchTo = _Imm_ * 4 + i + 4;
					if( branchTo > startpc && branchTo < i ) s_nEndBlock = branchTo;
					else  s_nEndBlock = i+8;

					goto StartRecomp;
				}
				break;
		}

		i += 4;
	}

StartRecomp:

	// rec info //
	{
		EEINST* pcur;

		if( s_nInstCacheSize < (s_nEndBlock-startpc)/4+1 ) {
			free(s_pInstCache);
			s_nInstCacheSize = (s_nEndBlock-startpc)/4+10;
			s_pInstCache = (EEINST*)malloc(sizeof(EEINST)*s_nInstCacheSize);
			assert( s_pInstCache != NULL );
		}

		pcur = s_pInstCache + (s_nEndBlock-startpc)/4;
		_recClearInst(pcur);
		pcur->info = 0;

		for(i = s_nEndBlock; i > startpc; i -= 4 ) {
			cpuRegs.code = *(int *)PSM(i-4);
			pcur[-1] = pcur[0];

			BSCPropagate bsc( pcur[-1], pcur[0] );
			bsc.rprop();
			pcur--;
		}
	}

	// analyze instructions //
	{
		usecop2 = 0;
		g_pCurInstInfo = s_pInstCache;

		for(i = startpc; i < s_nEndBlock; i += 4) {
			g_pCurInstInfo++;
			cpuRegs.code = *(u32*)PSM(i);

			// cop2 //
			if( g_pCurInstInfo->info & EEINSTINFO_COP2 ) {

				if( !usecop2 ) {
					// init
					vucycle = 0;
					usecop2 = 1;
				}

				VU0.code = cpuRegs.code;
				_vuRegsCOP22( &VU0, &g_pCurInstInfo->vuregs );
				continue;
			}

			// fixme - This should be based on the cycle count of the current EE
			// instruction being analyzed.
			if( usecop2 ) vucycle++;

		}
		// This *is* important because g_pCurInstInfo is checked a bit later on and
		// if it's not equal to s_pInstCache it handles recompilation differently.
		// ... but the empty if() conditional inside the for loop is still amusing. >_<
		if( usecop2 ) {
			// add necessary mac writebacks
			g_pCurInstInfo = s_pInstCache;

			for(i = startpc; i < s_nEndBlock-4; i += 4) {
				g_pCurInstInfo++;

				if( g_pCurInstInfo->info & EEINSTINFO_COP2 ) {
				}
			}
		}
	}

#ifdef PCSX2_DEBUG
	// dump code
	for(i = 0; i < ArraySize(s_recblocks); ++i) {
		if( startpc == s_recblocks[i] ) {
			iDumpBlock(startpc, recPtr);
		}
	}

	if( (dumplog & 1) ) //|| usecop2 )
		iDumpBlock(startpc, recPtr);
#endif

	u32 sz = (s_nEndBlock-startpc) >> 2;
	u32 inpage_ptr = HWADDR(startpc);
	u32 inpage_sz  = sz*4;

	// note: blocks are guaranteed to reside within the confines of a single page.

	const int PageType = mmap_GetRamPageInfo( inpage_ptr );
	//const u32 pgsz = std::min(0x1000 - inpage_offs, inpage_sz);
	const u32 pgsz = inpage_sz;

	if(PageType!=-1)
	{
		if (PageType==0) {
			mmap_MarkCountedRamPage( inpage_ptr );
			manual_page[inpage_ptr >> 12] = 0;
		}
		else
		{
			xMOV( ecx, inpage_ptr );
			xMOV( edx, pgsz / 4 );
			//xMOV( eax, startpc );		// uncomment this to access startpc (as eax) in dyna_block_discard

			u32 lpc = inpage_ptr;
			u32 stg = pgsz;
			while(stg>0)
			{
				xCMP( ptr32[PSM(lpc)], *(u32*)PSM(lpc) );
				xJNE( dyna_block_discard );

				stg -= 4;
				lpc += 4;
			}

			// Tweakpoint!  3 is a 'magic' number representing the number of times a counted block
			// is re-protected before the recompiler gives up and sets it up as an uncounted (permanent)
			// manual block.  Higher thresholds result in more recompilations for blocks that share code
			// and data on the same page.  Side effects of a lower threshold: over extended gameplay
			// with several map changes, a game's overall performance could degrade.

			// (ideally, perhaps, manual_counter should be reset to 0 every few minutes?)

			if (startpc != 0x81fc0 && manual_counter[inpage_ptr >> 12] <= 3) {

				// Counted blocks add a weighted (by block size) value into manual_page each time they're
				// run.  If the block gets run a lot, it resets and re-protects itself in the hope
				// that whatever forced it to be manually-checked before was a 1-time deal.

				// Counted blocks have a secondary threshold check in manual_counter, which forces a block
				// to 'uncounted' mode if it's recompiled several time.  This protects against excessive
				// recompilation of blocks that reside on the same codepage as data.

				// fixme? Currently this algo is kinda dumb and results in the forced recompilation of a
				// lot of blocks before it decides to mark a 'busy' page as uncounted.  There might be
				// be a more clever approach that could streamline this process, by doing a first-pass
				// test using the vtlb memory protection (without recompilation!) to reprotect a counted
				// block.  But unless a new also is relatively simple in implementation, it's probably
				// not worth the effort (tests show that we have lots of recompiler memory to spare, and
				// that the current amount of recompilation is fairly cheap).

				xADD(ptr16[&manual_page[inpage_ptr >> 12]], sz);
				xJC( dyna_page_reset );

				// note: clearcnt is measured per-page, not per-block!
				//DbgCon::WriteLn( "Manual block @ %08X : size=%3d  page/offs=%05X/%03X  inpgsz=%d  clearcnt=%d",
				//	params startpc, sz, inpage_ptr>>12, inpage_ptr&0xfff, inpage_sz, manual_counter[inpage_ptr >> 12] );
			}
			else
			{
				DbgCon::Notice( "Uncounted Manual block @ %08X : size=%3d page/offs=%05X/%03X  inpgsz=%d",
					params startpc, sz, inpage_ptr>>12, inpage_ptr&0xfff, pgsz, inpage_sz );
			}

		}
	}

	// Finally: Generate x86 recompiled code!
	g_pCurInstInfo = s_pInstCache;
	while (!branch && pc < s_nEndBlock) {
		recompileNextInstruction(0);		// For the love of recursion, batman!
	}

#ifdef PCSX2_DEBUG
	if( (dumplog & 1) )
		iDumpBlock(startpc, recPtr);
#endif

	assert( (pc-startpc)>>2 <= 0xffff );
	s_pCurBlockEx->size = (pc-startpc)>>2;

	if (HWADDR(pc) <= Ps2MemSize::Base) {
		BASEBLOCKEX *oldBlock;
		int i;

		i = recBlocks.LastIndex(HWADDR(pc) - 4);
		while (oldBlock = recBlocks[i--]) {
			if (oldBlock == s_pCurBlockEx)
				continue;
			if (oldBlock->startpc >= HWADDR(pc))
				continue;
			if (oldBlock->startpc + oldBlock->size * 4 <= HWADDR(startpc))
				break;
			if (memcmp(&recRAMCopy[oldBlock->startpc / 4], PSM(oldBlock->startpc),
			           oldBlock->size * 4)) {
				recClear(startpc, (pc - startpc) / 4);
				s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
				assert(s_pCurBlockEx->startpc == HWADDR(startpc));
				break;
			}
		}

		memcpy(&recRAMCopy[HWADDR(startpc) / 4], PSM(startpc), pc - startpc);
	}

	s_pCurBlock->SetFnptr((uptr)recPtr);

	for(i = 1; i < (u32)s_pCurBlockEx->size; i++) {
		if ((uptr)JITCompile == s_pCurBlock[i].GetFnptr())
			s_pCurBlock[i].SetFnptr((uptr)JITCompileInBlock);
	}

	if( !(pc&0x10000000) )
		maxrecmem = std::max( (pc&~0xa0000000), maxrecmem );

	if( branch == 2 )
	{
		// Branch type 2 - This is how I "think" this works (air):
		// Performs a branch/event test but does not actually "break" the block.
		// This allows exceptions to be raised, and is thus sufficient for
		// certain types of things like SYSCALL, EI, etc.  but it is not sufficient
		// for actual branching instructions.

		iFlushCall(FLUSH_EVERYTHING);
		iBranchTest();
	}
	else
	{
		if( branch )
			assert( !willbranch3 );

		if( willbranch3 || !branch) {

			iFlushCall(FLUSH_EVERYTHING);

			// Split Block concatenation mode.
			// This code is run when blocks are split either to keep block sizes manageable
			// or because we're crossing a 4k page protection boundary in ps2 mem.  The latter
			// case can result in very short blocks which should not issue branch tests for
			// performance reasons.

			int numinsts = (pc - startpc) / 4;
			if( numinsts > 6 )
				iBranchTest(pc);
			else
			{
				xMOV( ptr32[&cpuRegs.pc], pc );
				xADD( ptr32[&cpuRegs.cycle], eeScaleBlockCycles() );
				recBlocks.Link( HWADDR(pc), xJcc32() );
			}
		}
	}

	assert( x86Ptr < recMem+REC_CACHEMEM );
	assert( recConstBufPtr < recConstBuf + RECCONSTBUF_SIZE );
	assert( x86FpuState == 0 );

	assert(x86Ptr - recPtr < 0x10000);
	s_pCurBlockEx->x86size = x86Ptr - recPtr;

	recPtr = x86Ptr;

	assert( (g_cpuHasConstReg&g_cpuFlushedConstReg) == g_cpuHasConstReg );

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

R5900cpu recCpu = {
	recAlloc,
	recResetEE,
	recStep,
	recExecute,
	recClear,
	recShutdown
};
