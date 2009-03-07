/*
	sorry, but i dislike huge ,totally non informive, time consuming and useless comment headers, so check some other random file for licence details.

	block manager, handles blocks, proper block discading/freeing and all related cache information.
	This is the public api
*/

//btw, pragmas are fun
#pragma once
#include "Common.h"
#include <vector>
using namespace std;

typedef void DynarecEntryPoint();

struct CompiledBasicBlock;

//used for lookups -- needs to be as small as possible to avoid cache problems
struct CompiledBlockLookupInfo
{
	u32 start;                     //start pc
	DynarecEntryPoint* Code;       //Recompiled code 
	u32 lookups;                   //number of lookups
	CompiledBasicBlock* cBB;       //CompiledBasicBlock for more info
};

//This also should get store somewhere else for cache efficiency
//BranchBlockPtr is branch target for uncoditional and conditional branches
//NextBlockPtr is next block (by position) for conditional branches and calls.
struct CompiledBlockPredictionInfo
{
	void* BranchBlockPtr;          //DynarecEntryPoint*
	void* NextBlockPtr;            //DynarecEntryPoint* (conditional branch) or CompiledBlockInfo* (call)
};

//All the 'fat' things go there, this is only ever used on compiling and discarding blocks ...
struct CompiledBasicBlock
{
	u32 start;
	CompiledBlockLookupInfo* cBL;           //CompiledBlockInfo for this CompiledBasicBlock
	CompiledBlockPredictionInfo* cBP;

	vector<CompiledBasicBlock*> callers;    //list of blocks that reference this block by calling it.It is needed in order to 
											//undo any direct links

	u32 type;                               //for varius flags and/or type

	u32 BranchBlockAddrs;
	u32 NextBlockAddrs;

	CompiledBasicBlock* BranchBlock;
	CompiledBasicBlock* NextBlock;

	CompiledBlockLookupInfo cBL_data;
};

//returns 0 on error
//Allocate space for a block
CompiledBasicBlock* bm_AllocateBlock();

//Removes a block from all caches, and marks it for deletion.Can be called from within the rec safely ..
//*NOTE* the block may not be actually deleted anytime soon
void __fastcall bm_DiscardBlock(CompiledBasicBlock* cBB);

//Lookups generated code for a block and updates the cache structures.If the lookup fails, it returns a special function
//that will compile code.This CAN'T be called from within the a block.
DynarecEntryPoint* __fastcall bm_LookupCode(u32 addr,CompiledBlockLookupInfo* fastlookup);

//Lookups a compiled block, returns 0 on error.This doesn't use the cache and its gona be fairly slow.bm_LookupCode is meant for code lookups
CompiledBasicBlock* __fastcall bm_LookupBlock(u32 addr);

void bm_Init();
void bm_Reset();
void bm_Term();

#define CACHE_SIZE (16*1024)        //we'l have to experiment to find a good value here, 16k worked well for nullDC ...
#define CACHE_MASK (CACHE_SIZE-1)

#define CACHE_SHIFT 3               //we'l have to experiment with this as well. only values >= 2 make sense, as a block can't start on a non 4 byte
                                    //aligned position

#define CACHE_HASH(addr) (((addr)>>CACHE_SHIFT)&CACHE_MASK)

extern CompiledBlockLookupInfo* bm_cache_blocks[CACHE_SIZE];