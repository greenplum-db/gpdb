/*-------------------------------------------------------------------------
 *
 * tidbitmap.h
 *	  PostgreSQL tuple-id (TID) bitmap package
 *
 * This module provides bitmap data structures that are spiritually
 * similar to Bitmapsets, but are specially adapted to store sets of
 * tuple identifiers (TIDs), or ItemPointers.  In particular, the division
 * of an ItemPointer into BlockNumber and OffsetNumber is catered for.
 * Also, since we wish to be able to store very large tuple sets in
 * memory with this data structure, we support "lossy" storage, in which
 * we no longer remember individual tuple offsets on a page but only the
 * fact that a particular page needs to be visited.
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 2003-2009, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/include/nodes/tidbitmap.h,v 1.11 2009/06/11 14:49:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TIDBITMAP_H
#define TIDBITMAP_H

#include "c.h"
#include "access/htup.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "storage/itemptr.h"
#include "storage/bufpage.h"
#include "access/appendonlytid.h"

struct Instrumentation;                 /* #include "executor/instrument.h" */


/*
 * The maximum number of tuples per page is not large (typically 256 with
 * 8K pages, or 1024 with 32K pages).  So there's not much point in making
 * the per-page bitmaps variable size.	We just legislate that the size
 * is this:
 */

// UNDONE: Until we convert TidBitmap to handle AO TIDS, lets raise this the 64k
// #define MAX_TUPLES_PER_PAGE  MaxHeapTuplesPerPage
#define MAX_TUPLES_PER_PAGE  65536

/*
 * When we have to switch over to lossy storage, we use a data structure
 * with one bit per page, where all pages having the same number DIV
 * PAGES_PER_CHUNK are aggregated into one chunk.  When a chunk is present
 * and has the bit set for a given page, there must not be a per-page entry
 * for that page in the page table.
 *
 * We actually store both exact pages and lossy chunks in the same hash
 * table, using identical data structures.	(This is because dynahash.c's
 * memory management doesn't allow space to be transferred easily from one
 * hashtable to another.)  Therefore it's best if PAGES_PER_CHUNK is the
 * same as MAX_TUPLES_PER_PAGE, or at least not too different.	But we
 * also want PAGES_PER_CHUNK to be a power of 2 to avoid expensive integer
 * remainder operations.  So, define it like this:
 */
#define PAGES_PER_CHUNK  (BLCKSZ / 32)

/* A -1 ntuples in TBMIterateResult indicates a lossy bitmap page */
#define BITMAP_IS_LOSSY -1

/* The bitmap unit size can be adjusted by changing these declarations: */
#define TBM_BITS_PER_BITMAPWORD 64
typedef uint64 tbm_bitmapword;		/* must be an unsigned type */

/* number of active words for an exact page: */
#define WORDS_PER_PAGE	((MAX_TUPLES_PER_PAGE - 1) / TBM_BITS_PER_BITMAPWORD + 1)
/* number of active words for a lossy chunk: */
#define WORDS_PER_CHUNK  ((PAGES_PER_CHUNK - 1) / TBM_BITS_PER_BITMAPWORD + 1)

/*
 * different node types for streaming bitmaps
 */

typedef enum StreamType
{
    BMS_INDEX,      /* pull the data from the index itself */
    BMS_AND,        /* AND together input streams */
    BMS_OR         /* OR together input streams */
} StreamType;


/*
 * The hashtable entries are represented by this data structure.  For
 * an exact page, blockno is the page number and bit k of the bitmap
 * represents tuple offset k+1.  For a lossy chunk, blockno is the first
 * page in the chunk (this must be a multiple of PAGES_PER_CHUNK) and
 * bit k represents page blockno+k.  Note that it is not possible to
 * have exact storage for the first page of a chunk if we are using
 * lossy storage for any page in the chunk's range, since the same
 * hashtable entry has to serve both purposes.
 */
typedef struct PagetableEntry
{
	BlockNumber blockno;		/* page number (hashtable key) */
	bool		ischunk;		/* T = lossy storage, F = exact */
	bool		recheck;		/* should the tuples be rechecked? */
	tbm_bitmapword	words[Max(WORDS_PER_PAGE, WORDS_PER_CHUNK)];
} PagetableEntry;

/*
 * Actual bitmap representation is private to tidbitmap.c.	Callers can
 * do IsA(x, TIDBitmap) on it, but nothing else.
 */
typedef struct TIDBitmap TIDBitmap;

/* Likewise, TBMIterator is private */
typedef struct TBMIterator TBMIterator;

/*
 * Stream bitmap representation.
 */
typedef struct StreamBitmap
{
	NodeTag			type;		/* to make it a valid Node */
	struct StreamNode      *streamNode; /* state internal to stream implementation */
    struct Instrumentation *instrument; /* CDB: stats for EXPLAIN ANALYZE */
} StreamBitmap;

/* A version of TBMIterator for StreamBitmap */
typedef struct StreamBMIterator StreamBMIterator;

/* A version of TBMIterator that handles both TIDBitmap and StreamBitmap */
typedef struct GenericBMIterator GenericBMIterator;

/*
 * Stream object.
 */
typedef struct StreamNode
{
	StreamType      type;       /* one of: BMS_INDEX, BMS_AND, BMS_OR */
	void		   *opaque;		/* implementation-specific data */
	void 		  (*begin_iterate)(struct StreamNode *self, StreamBMIterator *iterator);
	void          (*free)(struct StreamNode *self);
	void          (*set_instrument)(struct StreamNode *self, struct Instrumentation *instr);
	void          (*upd_instrument)(struct StreamNode *self);
} StreamNode;

/*
 * Storage for state specific to the streaming of blocks from the index
 * itself.
 */
typedef struct StreamNode   IndexStream;

/*
 * Storage for streaming of multiple index streams which need to be
 * AND or OR'd together
 */
typedef struct StreamNode   OpStream;

/* Result structure for tbm_iterate */
typedef struct
{
	BlockNumber blockno;		/* page number containing tuples */
	int			ntuples;		/* -1 indicates lossy result */
	bool		recheck;		/* should the tuples be rechecked? */
	/* Note: recheck is always true if ntuples < 0 */
	OffsetNumber offsets[1];	/* VARIABLE LENGTH ARRAY */
} TBMIterateResult;				/* VARIABLE LENGTH STRUCT */

/* Make this visible for bitmap.c */
struct StreamBMIterator
{
	const struct StreamNode *node;	/* the root node to iterate out of */
	union
	{
		TBMIterator	   *hash;		/* for IndexStream */
		List		   *stream;		/* for OpStream */
	} input;						/* input iterator(s) */
	void			   *opaque;		/* for the implementation in bitmap.c */

	PagetableEntry	   *nextentry;	/* for IndexStream, a pointer to the next cached entry */
	BlockNumber			nextblock;	/* block number we're up to */
	PagetableEntry		entry;		/* storage for a page of tids in this stream bitmap */

	bool    	      (*pull)(struct StreamBMIterator *self, PagetableEntry *e);
	void			  (*end_iterate)(struct StreamBMIterator *self);

	TBMIterateResult	output;		/* MUST BE LAST (because variable-size) */
};

/* function prototypes in nodes/tidbitmap.c */
extern TIDBitmap *tbm_create(long maxbytes);
extern void tbm_free(TIDBitmap *tbm);

extern void tbm_add_tuples(TIDBitmap *tbm,
			   const ItemPointer tids, int ntids,
			   bool recheck);
extern void tbm_add_page(TIDBitmap *tbm, BlockNumber pageno);
extern void tbm_union(TIDBitmap *a, const TIDBitmap *b);
extern void tbm_intersect(TIDBitmap *a, const TIDBitmap *b);
extern bool tbm_is_empty(const TIDBitmap *tbm);

extern TBMIterator *tbm_begin_iterate(TIDBitmap *tbm);
extern TBMIterateResult *tbm_iterate(TBMIterator *iterator);
extern void tbm_end_iterate(TBMIterator *iterator);

extern void stream_move_node(StreamBitmap *strm, StreamBitmap *other, StreamType kind);
extern void stream_add_node(StreamBitmap *strm, StreamNode *node, StreamType kind);
extern StreamNode *tbm_create_stream_node(TIDBitmap *tbm);

/* These functions accept either a TIDBitmap or a StreamBitmap... */
extern GenericBMIterator *tbm_generic_begin_iterate(Node *bm);
extern TBMIterateResult *tbm_generic_iterate(GenericBMIterator *iterator);
extern void tbm_generic_end_iterate(GenericBMIterator *iterator);
extern void tbm_generic_free(Node *bm);
extern void tbm_generic_set_instrument(Node *bm, struct Instrumentation *instr);
extern void tbm_generic_upd_instrument(Node *bm);

extern void tbm_convert_appendonly_tid_out(ItemPointer psudeoHeapTid, AOTupleId *aoTid);


#endif   /* TIDBITMAP_H */
