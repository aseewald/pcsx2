#include "PrecompiledHeader.h"
#include "blockmanager.h"
#include <xmmintrin.h>

typedef vector<CompiledBasicBlock*> BlockList;

BlockList all_blocks;


BlockList     bm_cache_lists[CACHE_SIZE];
CompiledBlockLookupInfo* bm_cache_blocks[CACHE_SIZE];

//looks up the lists
DynarecEntryPoint* __fastcall bm_LookupCode(u32 addr,CompiledBlockLookupInfo* fastlookup)
{
    u32 cache_idx=CACHE_HASH(addr);
    
	CompiledBlockLookupInfo* rv=0;//not found

    //=cache_blocks[cache_idx].Find(addr);
	BlockList* list=&bm_cache_lists[cache_idx];

	for (u32 i=0;i<list->size();i++)
	{
		if (list[0][i]->start==addr)
		{
			rv=list[0][i]->cBL;
			rv->lookups++;
			break;
		}
	}

	if(rv->lookups >fastlookup->lookups)
	{
		CompiledBasicBlock* fastblock=fastlookup->cBB;
		CompiledBasicBlock* rvblock=rv->cBB;

		//xmm can be used safely here, because this should never be called from within a block , only betwen blocks
		__m128* a=(__m128*)&rvblock->cBL_data;
		__m128* b=(__m128*)fastlookup;
		__m128* c=(__m128*)&fastblock->cBL_data;
		
		*c=*b;//copy back to block info (old block getting out of cache)
		*b=*a;//copy to cache info (new block getting in cache)
		
		//make the block point to cache
		rvblock->cBL=fastlookup;

		//all set!
	}

	return rv->Code;
}


CompiledBasicBlock* __fastcall bm_LookupBlock(u32 pc)
{
	return 0;
}

void __fastcall bm_DiscardBlock(CompiledBasicBlock* cBB)
{

}