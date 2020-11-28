#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "a314.h"
#include "fix_mem_region.h"
#include "protocol.h"

struct MemChunkList
{
	struct MemChunk *first;
	struct MemChunk *last;
	ULONG free;
};

void add_chunk(struct MemChunkList *l, struct MemChunk *mc)
{
	if (l->first == NULL)
		l->first = mc;
	else
		l->last->mc_Next = mc;
	l->last = mc;
	l->free += mc->mc_Bytes;
}

struct MemHeader *split_region(struct MemHeader *lower, ULONG split_at)
{
	struct MemHeader *upper = (struct MemHeader *)AllocMem(sizeof(struct MemHeader), MEMF_PUBLIC | MEMF_CLEAR);

	struct MemChunkList ll = {NULL, NULL, 0};
	struct MemChunkList ul = {NULL, NULL, 0};

	struct MemChunk *mc = lower->mh_First;

	while (mc != NULL)
	{
		struct MemChunk *next_chunk = mc->mc_Next;
		mc->mc_Next = NULL;

		ULONG start = (ULONG)mc;
		ULONG end = start + mc->mc_Bytes;

		if (end <= split_at)
			add_chunk(&ll, mc);
		else if (split_at <= start)
			add_chunk(&ul, mc);
		else
		{
			mc->mc_Bytes = split_at - start;
			add_chunk(&ll, mc);

			struct MemChunk *new_chunk = (struct MemChunk *)split_at;
			new_chunk->mc_Next = NULL;
			new_chunk->mc_Bytes = end - split_at;
			add_chunk(&ul, new_chunk);
		}
		mc = next_chunk;
	}

	upper->mh_Node.ln_Type = NT_MEMORY;
	upper->mh_Node.ln_Pri = lower->mh_Node.ln_Pri;
	upper->mh_Node.ln_Name = lower->mh_Node.ln_Name; // Use a custom name?
	upper->mh_Attributes = lower->mh_Attributes;

	lower->mh_First = ll.first;
	upper->mh_First = ul.first;

	upper->mh_Lower = (APTR)split_at;
	upper->mh_Upper = lower->mh_Upper;
	lower->mh_Upper = (APTR)split_at;

	lower->mh_Free = ll.free;
	upper->mh_Free = ul.free;

	return upper;
}

BOOL overlap(struct MemHeader *mh, ULONG lower, ULONG upper)
{
	return lower < (ULONG)(mh->mh_Upper) && (ULONG)(mh->mh_Lower) < upper;
}

void mark_region_a314(ULONG address, ULONG size)
{
	struct List *memlist = &(SysBase->MemList);

	for (struct Node *node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct MemHeader *mh = (struct MemHeader *)node;
		if (overlap(mh, address, address + size))
		{
			if ((ULONG)mh->mh_Lower < address)
			{
				mh->mh_Attributes &= ~MEMF_A314;
				mh = split_region(mh, address);
			}
			else
				Remove((struct Node *)mh);

			if (address + size < (ULONG)mh->mh_Upper)
			{
				struct MemHeader *new_mh = split_region(mh, address + size);
				new_mh->mh_Attributes &= ~MEMF_A314;
				Enqueue(memlist, (struct Node *)new_mh);
			}

			mh->mh_Node.ln_Pri = -20;
			mh->mh_Attributes |= MEMF_A314;
			Enqueue(memlist, (struct Node *)mh);
			return;
		}
	}
}

BOOL fix_memory()
{
	Forbid();
	mark_region_a314(PISTORM_BASE, PISTORM_SIZE);
	Permit();
	return TRUE;
}

ULONG translate_address_a314(__reg("a0") void *address)
{
	ULONG offset = (ULONG)address - PISTORM_BASE;
	if (offset < PISTORM_SIZE)
		return offset;
	return -1;
}
