/*-------------------------------------------------------------------------
 * tupchunklist.c
 *	  The data-structures and functions for dealing with tuple chunk lists.
 *
 * Portions Copyright (c) 2005-2008, Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/tupchunklist.c
 *
 * Reviewers: jzhang, ftian, tkordas
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbmotion.h"


/* Appends a TupleChunkListItem to the end of a TupleChunkList.  The list's
* "num_chunks" value is incremented as well.
*/
void
appendChunkToTCList(TupleChunkList tcList, TupleChunkListItem tcItem)
{
	AssertArg(tcList != NULL);
	AssertArg(tcItem != NULL);

	/* Append the chunk to the list. */
	tcList->num_chunks++;

	if (tcList->p_last != NULL) /* List not empty. */
	{
		tcList->p_last->p_next = tcItem;
		tcList->p_last = tcItem;
	}
	else
		/* List is empty. */
	{
		Assert(tcList->p_first == NULL);
		tcList->p_first = tcItem;
		tcList->p_last = tcItem;
	}
}

static int	gp_interconnect_chunk_cache = 10;

TupleChunkListItem
getChunkFromCache(TupleChunkListCache *cache)
{
	TupleChunkListItem item;

	if (cache->items != NULL)
	{
		item = cache->items;
		cache->items = item->p_next;
		cache->len = cache->len - 1;
	}
	else
	{
		item = (TupleChunkListItem)
			palloc(sizeof(TupleChunkListItemData) + Gp_max_tuple_chunk_size);
	}

	MemSetAligned(item, 0, sizeof(TupleChunkListItemData) + 4);

	return item;
}

static void
putChunkToCache(TupleChunkListCache *cache, TupleChunkListItem item)
{
	if (cache->len > gp_interconnect_chunk_cache)
	{
		pfree(item);
	}
	else
	{
		item->p_next = cache->items;
		cache->items = item;
		cache->len = cache->len + 1;
	}
	return;
}

void
clearTCList(TupleChunkListCache *cache, TupleChunkList tcList)
{
	TupleChunkListItem tcItem,
				tcNext;

	AssertArg(tcList != NULL);

	tcItem = tcList->p_first;
	while (tcItem != NULL)
	{
		tcNext = tcItem->p_next;

		if (cache != NULL)
			putChunkToCache(cache, tcItem);
		else
			pfree(tcItem);

		tcItem = tcNext;
	}

	tcList->p_first = NULL;
	tcList->p_last = NULL;

	tcList->num_chunks = 0;
	tcList->serialized_data_length = 0;
}
