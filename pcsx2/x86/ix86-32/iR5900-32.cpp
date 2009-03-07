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
#include "iMMI.h"
#include "iFPU.h"
#include "iCOP0.h"
#include "iVUmicro.h"
#include "VU.h"
#include "VUmicro.h"

#include "iVUzerorec.h"
#include "vtlb.h"

#include "SamplProf.h"
#include "Paths.h"

#include "NakedAsm.h"

using namespace R5900;

// used to disable register when outside of a dynarec block, since only dynarec blocks violate the abi
bool g_EEFreezeRegs = false;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec

static const int RECSTACK_SIZE = 0x00010000;

static u8 *recMem = NULL;			// the recompiled blocks will be here
static u8* recPtr = NULL, *recStackPtr = NULL;


#ifdef _DEBUG
static u32 dumplog = 0;
#else
#define dumplog 0
#endif

static const int REC_CACHEMEM = 0x01000000;


static void recAlloc() 
{
	// Hardware Requirements Check...

	if ( !( cpucaps.hasMultimediaExtensions  ) )
		throw Exception::HardwareDeficiency( _( "Processor doesn't support MMX" ) );

	if ( !( cpucaps.hasStreamingSIMDExtensions ) )
		throw Exception::HardwareDeficiency( _( "Processor doesn't support SSE" ) );

	if ( !( cpucaps.hasStreamingSIMD2Extensions ) )
		throw Exception::HardwareDeficiency( _( "Processor doesn't support SSE2" ) );

	if( recMem == NULL )
	{
		// Note: the VUrec depends on being able to grab an allocatione below the 0x10000000 line,
		// so we give the EErec an address above that to try first as it's basemem address, hence
		// the 0x20000000 pick.

		const uint cachememsize = REC_CACHEMEM+0x1000;
		recMem = (u8*)SysMmapEx( 0x20000000, cachememsize, 0, "recAlloc(R5900)" );
	}

	if( recMem == NULL )
		throw Exception::OutOfMemory( "R5900-32 > failed to allocate recompiler memory." );

	// No errors.. Proceed with initialization:

	ProfilerRegisterSource( "EERec", recMem, REC_CACHEMEM+0x1000 );

	x86FpuState = FPU_STATE;
}

////////////////////////////////////////////////////
void recResetEE()
{
	DbgCon::Status( "iR5900-32 > Resetting recompiler memory and structures." );

	//0xcc is actually better ....
	memset_8<0xcc, REC_CACHEMEM>(recMem);

	mmap_ResetBlockTracking();

	//This could be useful as forward braches are predicted as not taken
	//It was commented out due to some lame buffer overrun tho

	//x86SetPtr(recMem+REC_CACHEMEM);
	//dyna_block_discard_recmem=(u8*)x86Ptr[0];
	//JMP32( (uptr)&dyna_block_discard - ( (u32)x86Ptr[0] + 5 ));

	x86SetPtr(recMem);

	recPtr = recMem;
	
	SetCPUState(Config.sseMXCSR, Config.sseVUMXCSR);
}

static void recShutdown()
{
	ProfilerTerminateSource( "EERec" );

	SafeSysMunmap( recMem, REC_CACHEMEM );
}

void recStep()
{
	printf("Interesting ..\n");
}

static __forceinline bool recEventTest()
{
#ifdef PCSX2_DEVBUILD
    // dont' remove this check unless doing an official release
    if( g_globalXMMSaved || g_globalMMXSaved)
		DevCon::Error("Pcsx2 Foopah!  Frozen regs have not been restored!!!");
	assert( !g_globalXMMSaved && !g_globalMMXSaved);
#endif

	// Perform counters, ints, and IOP updates:
	bool retval = _cpuBranchTest_Shared();

#ifdef PCSX2_DEVBUILD
	assert( !g_globalXMMSaved && !g_globalMMXSaved);
#endif
	return retval;
}

////////////////////////////////////////////////////

//What are these doing here ?
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
//	JMP32((uptr)DispatcherReg - ( (uptr)x86Ptr[0] + 5 ));
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


//compiler state
struct recState
{
	u32 startpc;
	u32 endpc;
	u32 cycles;
	u32 type;
};

#include "..\blockmanager.h"
/*
	block header :
	mov eax,const
	sub [cycles],eax;
*/
void* rec_resolve_block;
void* rec_do_update;
void* rec_exit_loop;

void __naked recDiscardBlock(u32 rpc,CompiledBasicBlock* cBB)
{
	__asm
	{
		mov pc,ecx;
		mov ecx,edx;
		call bm_DiscardBlock;
		jmp [rec_resolve_block];
	}
}
u32 cycle_count;
#define SOME_CONSTANT 512;

__declspec(align(64)) void __naked recLoop()
{
	__asm
	{
		//aligned +0

		//save registers ..
		pushad;
		align 16 // +16

		//setup some stuff
		mov rec_resolve_block,offset resolve_block;
		align 16//+32

		mov rec_do_update,offset do_update;
		align 16//+48

		mov rec_exit_loop,offset exit_loop;
		align 16	//+64 (=+0 !)
		//we really need 64 byte align here ..

tehloop:
		mov g_EEFreezeRegs,1;

resolve_block:
		//pc should be on ecx
		
		//calculate the hash
		mov edx,ecx;
		and edx,CACHE_MASK<<CACHE_SHIFT;
		shr edx,(CACHE_SHIFT-2);
		
		//lea here saves a few bytes later on
		lea edx,[bm_cache_blocks+edx];

		//is this the correct block ?
		mov eax,[edx+4];
		//cmp/jnz are fused on c2ds
		cmp [edx],ecx;
		jnz full_lookup;
		
		//if it is, add the counter and jump to it
		//add ? or inc ?
		add [edx+8],1;
		jmp eax;

		//this is the slow path, when the cache fails
full_lookup:
		call bm_LookupCode;
		jmp eax;

		//calls recEventTest
do_update:
		add eax,SOME_CONSTANT;
		add [cycle_count],eax;

		mov g_EEFreezeRegs,0;
		call recEventTest;

		test eax,eax;
		jnz tehloop;

exit_loop:
		mov g_EEFreezeRegs,0;
		//restore registers ..
		popad;
	}
}


void recExecute()
{
	recLoop();
}

void recExecuteBios()
{
	recResetEE();
	//set some magic flag
	recLoop();
	recResetEE();
}
R5900cpu recCpu =
{
	recAlloc,
	recResetEE,
	0,				//sorry, no stepping
	recExecute,
	recExecuteBios,
	0,				//sorry, no clears
	recShutdown
};
