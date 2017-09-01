/*-------------------------------------------------------------------------
 * tupchunklist.h
 *	   The data-structures and functions for dealing with tuple chunk lists.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/tupchunklist.h
 *-------------------------------------------------------------------------
 */
#ifndef TUPCHUNKLIST_H
#define TUPCHUNKLIST_H

#include "access/htup.h"
#include "access/tupdesc.h"
#include "cdb/tupchunk.h"

/*-------------------*/
/* Tuple Chunk Lists */
/*-------------------*/


/* An item in a tuple-chunk list.  Chunk data is just a sequence of bytes.
* A "tuple-chunk" struct doesn't make sense for several reasons:
*	- The struct would need to be packed, not word-aligned, to keep from
*	  sending too much stuff.  This can be tricky to achieve across various
*	  compilers.
*	- Multi-byte integers need to be transmitted in network byte order, which
*	  may not match hardware byte order.
* Therefore, it's just a chunk of bytes in the tuple-chunk list items.
*/
typedef struct TupleChunkListItemData
{
	/* Pointer to next item in the list. */
	struct TupleChunkListItemData *p_next;

	/*
	 * Length of chunk, including header.  We could calculate this value, but
	 * why trouble ourselves???
	 */
	uint32		chunk_length;

	/*
	 * pointer to any "shared" data, used to pass back items 'in place'
	 * without copying
	 */
	char	   *inplace;

	/* Variable-length data portion of the struct. */
	uint8		chunk_data[];
} TupleChunkListItemData,
		   *TupleChunkListItem;


/* A list of tuple-chunks, in order, for a single HeapTuple.
*
* NOTE:  This structure is intended to hold tuple-chunks for ONLY ONE tuple
*		 at a time!!!  Tuple chunks have no information that indicates which
*		 tuple they are a part of, because they are expected to be sent and
*		 received in order, and without loss.  So, BEWARE.
*/
typedef struct TupleChunkListData
{
	/*
	 * If we ever want to generate these lists on an incremental basis (would
	 * be desirable for very large tuples, so that we use less memory during
	 * the process - although other things in Postgres would probably break
	 * too), the HeapTuple and the TupleDesc will probably need to be stored
	 * in this structure.
	 */

	/* Pointers to start and end of list. */
	TupleChunkListItem p_first;
	TupleChunkListItem p_last;

	/* Count of chunks in the list. */
	int			num_chunks;

	/* Total bytes of serialized tuple data, not including chunk headers. */
	int			serialized_data_length;
	int			max_chunk_length;

} TupleChunkListData,
		   *TupleChunkList;

extern TupleChunkListItem makeTupleChunkListItem(TupleChunkType chunkType,
					   void *pChunkData, uint16 chunkDataSize);

/* Add another item to the end of a tuple chunk list. */
extern void appendChunkToTCList(TupleChunkList tcList, TupleChunkListItem tcItem);

/* Provide a chunk-list-item cache */
typedef struct {
	int len;
	TupleChunkListItem items;
} TupleChunkListCache;

extern TupleChunkListItem getChunkFromCache(TupleChunkListCache *cache);

/* Remove the contents of a TupleChunkList, and reset its state to "empty." */
extern void clearTCList(TupleChunkListCache *cache, TupleChunkList tcList);

#endif   /* TUPCHUNKLIST_H */
