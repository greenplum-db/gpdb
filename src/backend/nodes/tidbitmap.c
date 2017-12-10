/*-------------------------------------------------------------------------
 *
 * tidbitmap.c
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
 * The "lossy" storage uses one bit per disk page, so at the standard 8K
 * BLCKSZ, we can represent all pages in 64Gb of disk space in about 1Mb
 * of memory.  People pushing around tables of that size should have a
 * couple of Mb to spare, so we don't worry about providing a second level
 * of lossiness.  In theory we could fall back to page ranges at some
 * point, but for now that seems useless complexity.
 *
 *
 * Copyright (c) 2003-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/nodes/tidbitmap.c,v 1.19 2009/06/11 14:48:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/htup.h"
#include "access/bitmap.h"		/* XXX: remove once pull_stream is generic */
#include "executor/instrument.h"	/* Instrumentation */
#include "nodes/bitmapset.h"
#include "nodes/tidbitmap.h"
#include "storage/bufpage.h"
#include "utils/hsearch.h"

#define WORDNUM(x)	((x) / TBM_BITS_PER_BITMAPWORD)
#define BITNUM(x)	((x) % TBM_BITS_PER_BITMAPWORD)

static bool tbm_iterate_page(PagetableEntry *page, TBMIterateResult *output);
static PagetableEntry *tbm_next_page(TBMIterator *iterator, bool *more);
static void tbm_upd_instrument(TIDBitmap *tbm);

/*
 * dynahash.c is optimized for relatively large, long-lived hash tables.
 * This is not ideal for TIDBitMap, particularly when we are using a bitmap
 * scan on the inside of a nestloop join: a bitmap may well live only long
 * enough to accumulate one entry in such cases.  We therefore avoid creating
 * an actual hashtable until we need two pagetable entries.  When just one
 * pagetable entry is needed, we store it in a fixed field of TIDBitMap.
 * (NOTE: we don't get rid of the hashtable if the bitmap later shrinks down
 * to zero or one page again.  So, status can be TBM_HASH even when nentries
 * is zero or one.)
 */
typedef enum
{
	TBM_EMPTY,					/* no hashtable, nentries == 0 */
	TBM_ONE_PAGE,				/* entry1 contains the single entry */
	TBM_HASH					/* pagetable is valid, entry1 is not */
} TBMStatus;

/*
 * Here is the representation for a whole TIDBitMap:
 */
struct TIDBitmap
{
	NodeTag		type;			/* to make it a valid Node */
	MemoryContext mcxt;			/* memory context containing me */
	TBMStatus	status;			/* see codes above */
	HTAB	   *pagetable;		/* hash table of PagetableEntry's */
	int			nentries;		/* number of entries in pagetable */
	int			nentries_hwm;	/* high-water mark for number of entries */
	int			maxentries;		/* limit on same to meet maxbytes */
	int			npages;			/* number of exact entries in pagetable */
	int			nchunks;		/* number of lossy entries in pagetable */
	bool		iterating;		/* tbm_begin_iterate called? */
	PagetableEntry entry1;		/* used when status == TBM_ONE_PAGE */
	/* these are valid when iterating is true: */
	PagetableEntry **spages;	/* sorted exact-page list, or NULL */
	PagetableEntry **schunks;	/* sorted lossy-chunk list, or NULL */

	/* CDB: Statistics for EXPLAIN ANALYZE */
	struct Instrumentation *instrument;
	Size		bytesperentry;
};

/*
 * When iterating over a bitmap in sorted order, a TBMIterator is used to
 * track our progress.	There can be several iterators scanning the same
 * bitmap concurrently.  Note that the bitmap becomes read-only as soon as
 * any iterator is created.
 */
struct TBMIterator
{
	TIDBitmap  *tbm;			/* TIDBitmap we're iterating over */
	int			spageptr;		/* next spages index */
	int			schunkptr;		/* next schunks index */
	int			schunkbit;		/* next bit to check in current schunk */
	TBMIterateResult output;	/* MUST BE LAST (because variable-size) */
};

struct GenericBMIterator
{
	const Node *bm;				/* [TID|Stream]Bitmap we're iterating over */
	union
	{
		TBMIterator		 *hash;		/* iterator for TIDBitmap implementation */
		StreamBMIterator *stream;	/* iterator for StreamBitmap implementation */
	} impl;
};

/* Local function prototypes */
static void tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage);
static bool tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage,
				   const TIDBitmap *b);
static const PagetableEntry *tbm_find_pageentry(const TIDBitmap *tbm,
				   BlockNumber pageno);
static PagetableEntry *tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno);
static bool tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno);
static void tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno);
static void tbm_lossify(TIDBitmap *tbm);
static int	tbm_comparator(const void *left, const void *right);
static void tbm_stream_set_instrument(StreamNode * self, struct Instrumentation *instr);
static void tbm_stream_upd_instrument(StreamNode * self);

static StreamBMIterator *tbm_stream_begin_iterate(StreamNode *node);
static void tbm_stream_end_iterate(StreamBMIterator *iterator);

/* IndexStream callbacks */
static void index_stream_begin_iterate(StreamNode *self, StreamBMIterator *iterator);
static bool tbm_stream_block(StreamBMIterator *iterator, PagetableEntry *e);
static void index_stream_end_iterate(StreamBMIterator *self);
static void tbm_stream_free(StreamNode *self);

/* OpStream callbacks */
static void opstream_begin_iterate(StreamNode *self, StreamBMIterator *iterator);
static bool opstream_iterate(StreamBMIterator *iterator, PagetableEntry *e);
static void opstream_end_iterate(StreamBMIterator *self);
static void opstream_free(StreamNode *self);

/*
 * tbm_create - create an initially-empty bitmap
 *
 * The bitmap will live in the memory context that is CurrentMemoryContext
 * at the time of this call.  It will be limited to (approximately) maxbytes
 * total memory consumption.
 */
TIDBitmap *
tbm_create(long maxbytes)
{
	TIDBitmap  *tbm;
	long		nbuckets;

	/*
	 * Ensure that we don't have heap tuple offsets going beyond (INT16_MAX +
	 * 1) or 32768. The executor iterates only over the first 32K tuples for
	 * lossy bitmap pages [MPP-24326].
	 */
	COMPILE_ASSERT(MaxHeapTuplesPerPage <= (INT16_MAX + 1));

	/* Create the TIDBitmap struct and zero all its fields */
	tbm = makeNode(TIDBitmap);

	tbm->mcxt = CurrentMemoryContext;
	tbm->status = TBM_EMPTY;
	tbm->instrument = NULL;

	/*
	 * Estimate number of hashtable entries we can have within maxbytes. This
	 * estimates the hash overhead at MAXALIGN(sizeof(HASHELEMENT)) plus a
	 * pointer per hash entry, which is crude but good enough for our purpose.
	 * Also count an extra Pointer per entry for the arrays created during
	 * iteration readout.
	 */
	tbm->bytesperentry =
		(MAXALIGN(sizeof(HASHELEMENT)) + MAXALIGN(sizeof(PagetableEntry))
		 + sizeof(Pointer) + sizeof(Pointer));
	nbuckets = maxbytes / tbm->bytesperentry;
	nbuckets = Min(nbuckets, INT_MAX - 1);		/* safety limit */
	nbuckets = Max(nbuckets, 16);		/* sanity limit */
	tbm->maxentries = (int) nbuckets;

	return tbm;
}

/*
 * Actually create the hashtable.  Since this is a moderately expensive
 * proposition, we don't do it until we have to.
 */
static void
tbm_create_pagetable(TIDBitmap *tbm)
{
	HASHCTL		hash_ctl;

	Assert(tbm->status != TBM_HASH);
	Assert(tbm->pagetable == NULL);

	/* Create the hashtable proper */
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(BlockNumber);
	hash_ctl.entrysize = sizeof(PagetableEntry);
	hash_ctl.hash = tag_hash;
	hash_ctl.hcxt = tbm->mcxt;
	tbm->pagetable = hash_create("TIDBitmap",
								 128,	/* start small and extend */
								 &hash_ctl,
								 HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

	/* If entry1 is valid, push it into the hashtable */
	if (tbm->status == TBM_ONE_PAGE)
	{
		PagetableEntry *page;
		bool		found;

		page = (PagetableEntry *) hash_search(tbm->pagetable,
											  (void *) &tbm->entry1.blockno,
											  HASH_ENTER, &found);
		Assert(!found);
		memcpy(page, &tbm->entry1, sizeof(PagetableEntry));
	}

	tbm->status = TBM_HASH;
}

/*
 * tbm_free - free a TIDBitmap
 */
void
tbm_free(TIDBitmap *tbm)
{
	if (tbm->instrument)
		tbm_upd_instrument(tbm);
	if (tbm->pagetable)
		hash_destroy(tbm->pagetable);
	if (tbm->spages)
		pfree(tbm->spages);
	if (tbm->schunks)
		pfree(tbm->schunks);
	pfree(tbm);
}


/*
 * tbm_upd_instrument - Update the Instrumentation attached to a TIDBitmap.
 */
static void
tbm_upd_instrument(TIDBitmap *tbm)
{
	Instrumentation *instr = tbm->instrument;
	Size		workmemused;

	if (!instr)
		return;

	/* Update page table high-water mark. */
	tbm->nentries_hwm = Max(tbm->nentries_hwm, tbm->nentries);

	/* How much of our work_mem quota was actually used? */
	workmemused = tbm->nentries_hwm * tbm->bytesperentry;
	instr->workmemused = Max(instr->workmemused, workmemused);
}	/* tbm_upd_instrument */


/*
 * tbm_set_instrument
 *	Attach caller's Instrumentation object to a TIDBitmap, unless the
 *	TIDBitmap already has one.  We want the statistics to be associated
 *	with the plan node which originally created the bitmap, rather than a
 *	downstream consumer of the bitmap.
 */
static void
tbm_set_instrument(TIDBitmap *tbm, struct Instrumentation *instr)
{
	if (instr == NULL ||
		tbm->instrument == NULL)
	{
		tbm->instrument = instr;
		tbm_upd_instrument(tbm);
	}
}	/* tbm_set_instrument */


/*
 * tbm_add_tuples - add some tuple IDs to a TIDBitmap
 */
void
tbm_add_tuples(TIDBitmap *tbm, const ItemPointer tids, int ntids,
			   bool recheck)
{
	int			i;

	Assert(!tbm->iterating);
	for (i = 0; i < ntids; i++)
	{
		BlockNumber blk = ItemPointerGetBlockNumber(tids + i);
		OffsetNumber off = ItemPointerGetOffsetNumber(tids + i);
		PagetableEntry *page;
		int			wordnum,
					bitnum;

		/* safety check to ensure we don't overrun bit array bounds */

		/* UNDONE: Turn this off until we convert this module to AO TIDs. */
#if 0
		if (off < 1 || off > MAX_TUPLES_PER_PAGE)
			elog(ERROR, "tuple offset out of range: %u", off);
#endif

		if (tbm_page_is_lossy(tbm, blk))
			continue;			/* whole page is already marked */

		page = tbm_get_pageentry(tbm, blk);

		if (page->ischunk)
		{
			/* The page is a lossy chunk header, set bit for itself */
			wordnum = bitnum = 0;
		}
		else
		{
			/* Page is exact, so set bit for individual tuple */
			wordnum = WORDNUM(off - 1);
			bitnum = BITNUM(off - 1);
		}
		page->words[wordnum] |= ((tbm_bitmapword) 1 << bitnum);
		page->recheck |= recheck;

		if (tbm->nentries > tbm->maxentries)
			tbm_lossify(tbm);
	}
}

/*
 * tbm_add_page - add a whole page to a TIDBitmap
 *
 * This causes the whole page to be reported (with the recheck flag)
 * when the TIDBitmap is scanned.
 */
void
tbm_add_page(TIDBitmap *tbm, BlockNumber pageno)
{
	/* Enter the page in the bitmap, or mark it lossy if already present */
	tbm_mark_page_lossy(tbm, pageno);
	/* If we went over the memory limit, lossify some more pages */
	if (tbm->nentries > tbm->maxentries)
		tbm_lossify(tbm);
}

/*
 * tbm_union - set union
 *
 * a is modified in-place, b is not changed
 */
void
tbm_union(TIDBitmap *a, const TIDBitmap *b)
{
	Assert(!a->iterating);
	/* Nothing to do if b is empty */
	if (b->nentries == 0)
		return;
	/* Scan through chunks and pages in b, merge into a */
	if (b->status == TBM_ONE_PAGE)
		tbm_union_page(a, &b->entry1);
	else
	{
		HASH_SEQ_STATUS status;
		PagetableEntry *bpage;

		Assert(b->status == TBM_HASH);
		hash_seq_init(&status, b->pagetable);
		while ((bpage = (PagetableEntry *) hash_seq_search(&status)) != NULL)
			tbm_union_page(a, bpage);
	}
}

/* Process one page of b during a union op */
static void
tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage)
{
	PagetableEntry *apage;
	int			wordnum;

	if (bpage->ischunk)
	{
		/* Scan b's chunk, mark each indicated page lossy in a */
		for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
		{
			tbm_bitmapword w = bpage->words[wordnum];

			if (w != 0)
			{
				BlockNumber pg;

				pg = bpage->blockno + (wordnum * TBM_BITS_PER_BITMAPWORD);
				while (w != 0)
				{
					if (w & 1)
						tbm_mark_page_lossy(a, pg);
					pg++;
					w >>= 1;
				}
			}
		}
	}
	else if (tbm_page_is_lossy(a, bpage->blockno))
	{
		/* page is already lossy in a, nothing to do */
		return;
	}
	else
	{
		apage = tbm_get_pageentry(a, bpage->blockno);
		if (apage->ischunk)
		{
			/* The page is a lossy chunk header, set bit for itself */
			apage->words[0] |= ((tbm_bitmapword) 1 << 0);
		}
		else
		{
			/* Both pages are exact, merge at the bit level */
			for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
				apage->words[wordnum] |= bpage->words[wordnum];
		}
	}

	if (a->nentries > a->maxentries)
		tbm_lossify(a);
}

/*
 * tbm_intersect - set intersection
 *
 * a is modified in-place, b is not changed
 */
void
tbm_intersect(TIDBitmap *a, const TIDBitmap *b)
{
	Assert(!a->iterating);
	/* Nothing to do if a is empty */
	if (a->nentries == 0)
		return;

	a->nentries_hwm = Max(a->nentries_hwm, a->nentries);

	/* Scan through chunks and pages in a, try to match to b */
	if (a->status == TBM_ONE_PAGE)
	{
		if (tbm_intersect_page(a, &a->entry1, b))
		{
			/* Page is now empty, remove it from a */
			Assert(!a->entry1.ischunk);
			a->npages--;
			a->nentries--;
			Assert(a->nentries == 0);
			a->status = TBM_EMPTY;
		}
	}
	else
	{
		HASH_SEQ_STATUS status;
		PagetableEntry *apage;

		Assert(a->status == TBM_HASH);
		hash_seq_init(&status, a->pagetable);
		while ((apage = (PagetableEntry *) hash_seq_search(&status)) != NULL)
		{
			if (tbm_intersect_page(a, apage, b))
			{
				/* Page or chunk is now empty, remove it from a */
				if (apage->ischunk)
					a->nchunks--;
				else
					a->npages--;
				a->nentries--;
				if (hash_search(a->pagetable,
								(void *) &apage->blockno,
								HASH_REMOVE, NULL) == NULL)
					elog(ERROR, "hash table corrupted");
			}
		}
	}
}

/*
 * Process one page of a during an intersection op
 *
 * Returns TRUE if apage is now empty and should be deleted from a
 */
static bool
tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage, const TIDBitmap *b)
{
	const PagetableEntry *bpage;
	int			wordnum;

	if (apage->ischunk)
	{
		/* Scan each bit in chunk, try to clear */
		bool		candelete = true;

		for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
		{
			tbm_bitmapword w = apage->words[wordnum];

			if (w != 0)
			{
				tbm_bitmapword neww = w;
				BlockNumber pg;
				int			bitnum;

				pg = apage->blockno + (wordnum * TBM_BITS_PER_BITMAPWORD);
				bitnum = 0;
				while (w != 0)
				{
					if (w & 1)
					{
						if (!tbm_page_is_lossy(b, pg) &&
							tbm_find_pageentry(b, pg) == NULL)
						{
							/* Page is not in b at all, lose lossy bit */
							neww &= ~((tbm_bitmapword) 1 << bitnum);
						}
					}
					pg++;
					bitnum++;
					w >>= 1;
				}
				apage->words[wordnum] = neww;
				if (neww != 0)
					candelete = false;
			}
		}
		return candelete;
	}
	else if (tbm_page_is_lossy(b, apage->blockno))
	{
		/*
		 * Some of the tuples in 'a' might not satisfy the quals for 'b', but
		 * because the page 'b' is lossy, we don't know which ones. Therefore
		 * we mark 'a' as requiring rechecks, to indicate that at most those
		 * tuples set in 'a' are matches.
		 */
		apage->recheck = true;
		return false;
	}
	else
	{
		bool		candelete = true;

		bpage = tbm_find_pageentry(b, apage->blockno);
		if (bpage != NULL)
		{
			/* Both pages are exact, merge at the bit level */
			Assert(!bpage->ischunk);
			for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
			{
				apage->words[wordnum] &= bpage->words[wordnum];
				if (apage->words[wordnum] != 0)
					candelete = false;
			}
			apage->recheck |= bpage->recheck;
		}
		/* If there is no matching b page, we can just delete the a page */
		return candelete;
	}
}

/*
 * tbm_is_empty - is a TIDBitmap completely empty?
 */
bool
tbm_is_empty(const TIDBitmap *tbm)
{
	return (tbm->nentries == 0);
}

/*
 * tbm_begin_iterate - prepare to iterate through a TIDBitmap
 *
 * The TBMIterator struct is created in the caller's memory context.
 * For a clean shutdown of the iteration, call tbm_end_iterate; but it's
 * okay to just allow the memory context to be released, too.  It is caller's
 * responsibility not to touch the TBMIterator anymore once the TIDBitmap
 * is freed.
 *
 * NB: after this is called, it is no longer allowed to modify the contents
 * of the bitmap.  However, you can call this multiple times to scan the
 * contents repeatedly, including parallel scans.
 */
TBMIterator *
tbm_begin_iterate(TIDBitmap *tbm)
{
	TBMIterator *iterator;

	/*
	 * Create the TBMIterator struct, with enough trailing space to serve the
	 * needs of the TBMIterateResult sub-struct.
	 */
	iterator = (TBMIterator *) palloc(sizeof(TBMIterator) +
								 MAX_TUPLES_PER_PAGE * sizeof(OffsetNumber));
	iterator->tbm = tbm;

	/*
	 * Initialize iteration pointers.
	 */
	iterator->spageptr = 0;
	iterator->schunkptr = 0;
	iterator->schunkbit = 0;

	/*
	 * If we have a hashtable, create and fill the sorted page lists, unless
	 * we already did that for a previous iterator.  Note that the lists are
	 * attached to the bitmap not the iterator, so they can be used by more
	 * than one iterator.
	 */
	if (tbm->status == TBM_HASH && !tbm->iterating)
	{
		HASH_SEQ_STATUS status;
		PagetableEntry *page;
		int			npages;
		int			nchunks;

		if (!tbm->spages && tbm->npages > 0)
			tbm->spages = (PagetableEntry **)
				MemoryContextAlloc(tbm->mcxt,
								   tbm->npages * sizeof(PagetableEntry *));
		if (!tbm->schunks && tbm->nchunks > 0)
			tbm->schunks = (PagetableEntry **)
				MemoryContextAlloc(tbm->mcxt,
								   tbm->nchunks * sizeof(PagetableEntry *));

		hash_seq_init(&status, tbm->pagetable);
		npages = nchunks = 0;
		while ((page = (PagetableEntry *) hash_seq_search(&status)) != NULL)
		{
			if (page->ischunk)
				tbm->schunks[nchunks++] = page;
			else
				tbm->spages[npages++] = page;
		}
		Assert(npages == tbm->npages);
		Assert(nchunks == tbm->nchunks);
		if (npages > 1)
			qsort(tbm->spages, npages, sizeof(PagetableEntry *),
				  tbm_comparator);
		if (nchunks > 1)
			qsort(tbm->schunks, nchunks, sizeof(PagetableEntry *),
				  tbm_comparator);
	}

	tbm->iterating = true;

	return iterator;
}

/*
 * tbm_stream_begin_iterate - prepare to iterate through a StreamBitmap
 */
static StreamBMIterator *
tbm_stream_begin_iterate(StreamNode *node)
{
	/*
	 * Create the StreamBMIterator struct, with enough trailing space to serve
	 * the needs of the TBMIterateResult sub-struct.
	 */
	StreamBMIterator *iterator = palloc0(sizeof(StreamBMIterator) +
								 MAX_TUPLES_PER_PAGE * sizeof(OffsetNumber));

	iterator->node = node;
	node->begin_iterate(node, iterator);

	return iterator;
}

/*
 * tbm_generic_begin_iterate - prepare to iterate through either a TIDBitmap or
 * a StreamBitmap
 *
 * The GenericBMIterator struct is created in the caller's memory context.
 * For a clean shutdown of the iteration, call tbm_generic_end_iterate.
 * It is caller's responsibility not to touch the iterator anymore once the
 * Node passed to this function is freed.
 *
 * Similarly to tbm_begin_iterate, after this is called, it is no longer allowed
 * to modify the contents of the bitmap.  However, you can call this multiple
 * times to scan the contents repeatedly, including parallel scans.
 */
GenericBMIterator *
tbm_generic_begin_iterate(Node *bm)
{
	GenericBMIterator *iterator;

	Assert(IsA(bm, TIDBitmap) || IsA(bm, StreamBitmap));

	/* Allocate space. */
	iterator = palloc(sizeof(*iterator));
	iterator->bm = bm;

	switch (bm->type)
	{
		case T_TIDBitmap:
			iterator->impl.hash = tbm_begin_iterate((TIDBitmap *) bm);
			break;

		case T_StreamBitmap:
		{
			StreamBitmap *sbm = (StreamBitmap *) bm;
			iterator->impl.stream = tbm_stream_begin_iterate(sbm->streamNode);
			break;
		}

		default:
			elog(ERROR, "invalid node type");
	}

	return iterator;
}

/*
 * tbm_generic_iterate - scan through next page of a TIDBitmap or a
 * StreamBitmap.
 */
TBMIterateResult *
tbm_generic_iterate(GenericBMIterator *iterator)
{
	const Node *tbm = iterator->bm;

	Assert(IsA(tbm, TIDBitmap) || IsA(tbm, StreamBitmap));

	switch (tbm->type)
	{
		case T_TIDBitmap:
			{
				const TIDBitmap *hashBitmap = (const TIDBitmap *) tbm;
				TBMIterator *hashIterator = iterator->impl.hash;

				Assert(hashIterator->tbm == hashBitmap);
				Assert(hashBitmap->iterating);

				return tbm_iterate(hashIterator);
			}
		case T_StreamBitmap:
			{
				StreamBMIterator *streamIterator = iterator->impl.stream;
				TBMIterateResult *output = NULL;

				MemSet(&streamIterator->entry, 0, sizeof(PagetableEntry));
				if (streamIterator->pull(streamIterator, &streamIterator->entry))
				{
					output = &streamIterator->output;
					tbm_iterate_page(&streamIterator->entry, output);
				}

				return output;
			}
		default:
			elog(ERROR, "unrecoganized node type");
	}

	return NULL;
}

/*
 * tbm_iterate_page - get a TBMIterateResult from a given PagetableEntry.
 */
static bool
tbm_iterate_page(PagetableEntry *page, TBMIterateResult *output)
{
	int			ntuples;
	int			wordnum;

	if (page->ischunk)
	{
		ntuples = -1;
	}

	else
	{
		/* scan bitmap to extract individual offset numbers */
		ntuples = 0;
		for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
		{
			tbm_bitmapword w = page->words[wordnum];

			if (w != 0)
			{
				int			off = wordnum * TBM_BITS_PER_BITMAPWORD + 1;

				while (w != 0)
				{
					if (w & 1)
						output->offsets[ntuples++] = (OffsetNumber) off;
					off++;
					w >>= 1;
				}
			}
		}
	}

	output->blockno = page->blockno;
	output->ntuples = ntuples;

	return true;
}

/*
 * tbm_iterate - scan through next page of a TIDBitmap
 *
 * Gets a TBMIterateResult representing one page, or NULL if there are
 * no more pages to scan.  Pages are guaranteed to be delivered in numerical
 * order.  If result->ntuples < 0, then the bitmap is "lossy" and failed to
 * remember the exact tuples to look at on this page --- the caller must
 * examine all tuples on the page and check if they meet the intended
 * condition.
 */
TBMIterateResult *
tbm_iterate(TBMIterator *iterator)
{
	PagetableEntry *e;
	bool		more;
	TBMIterateResult *output = &(iterator->output);

	e = tbm_next_page(iterator, &more);
	if (more && e)
	{
		tbm_iterate_page(e, output);
		return output;
	}
	return NULL;
}

/*
 * tbm_next_page - actually traverse the TIDBitmap
 *
 * Store the next block of matches in nextpage.
 */

static PagetableEntry *
tbm_next_page(TBMIterator *iterator, bool *more)
{
	TIDBitmap  *tbm = iterator->tbm;
	Assert(tbm->iterating);

	*more = true;

	/*
	 * If lossy chunk pages remain, make sure we've advanced schunkptr/
	 * schunkbit to the next set bit.
	 */
	while (iterator->schunkptr < tbm->nchunks)
	{
		PagetableEntry *chunk = tbm->schunks[iterator->schunkptr];
		int			schunkbit = iterator->schunkbit;

		while (schunkbit < PAGES_PER_CHUNK)
		{
			int			wordnum = WORDNUM(schunkbit);
			int			bitnum = BITNUM(schunkbit);

			if ((chunk->words[wordnum] & ((tbm_bitmapword) 1 << bitnum)) != 0)
				break;
			schunkbit++;
		}
		if (schunkbit < PAGES_PER_CHUNK)
		{
			iterator->schunkbit = schunkbit;
			break;
		}
		/* advance to next chunk */
		iterator->schunkptr++;
		iterator->schunkbit = 0;
	}

	/*
	 * If both chunk and per-page data remain, must output the numerically
	 * earlier page.
	 */
	if (iterator->schunkptr < tbm->nchunks)
	{
		PagetableEntry *chunk = tbm->schunks[iterator->schunkptr];
		PagetableEntry *nextpage;
		BlockNumber chunk_blockno;

		chunk_blockno = chunk->blockno + iterator->schunkbit;
		if (iterator->spageptr >= tbm->npages ||
			chunk_blockno < tbm->spages[iterator->spageptr]->blockno)
		{
			/* Return a lossy page indicator from the chunk */
			nextpage = (PagetableEntry *) palloc(sizeof(PagetableEntry));
			nextpage->ischunk = true;
			nextpage->blockno = chunk_blockno;
			nextpage->recheck = true;
			iterator->schunkbit++;
			return nextpage;
		}
	}

	if (iterator->spageptr < tbm->npages)
	{
		PagetableEntry *e;

		/* In ONE_PAGE state, we don't allocate an spages[] array */
		if (tbm->status == TBM_ONE_PAGE)
			e = &tbm->entry1;
		else
			e = tbm->spages[iterator->spageptr];

		iterator->spageptr++;
		return e;
	}

	/* Nothing more in the bitmap */
	*more = false;
	return NULL;
}

/*
 * tbm_end_iterate - finish an iteration over a TIDBitmap
 *
 * Currently this is just a pfree, but it might do more someday.  (For
 * instance, it could be useful to count open iterators and allow the
 * bitmap to return to read/write status when there are no more iterators.)
 */
void
tbm_end_iterate(TBMIterator *iterator)
{
	pfree(iterator);
}

/*
 * tbm_stream_end_iterate - finish an iteration over a StreamBitmap
 */
static void
tbm_stream_end_iterate(StreamBMIterator *iterator)
{
	/* end_iterate() will clean up whatever begin_iterate() set up. */
	iterator->end_iterate(iterator);
	pfree(iterator);
}

/*
 * tbm_generic_end_iterate - finish an iteration over a TIDBitmap or
 * StreamBitmap
 */
void
tbm_generic_end_iterate(GenericBMIterator *iterator)
{
	const Node *bm = iterator->bm;

	switch (bm->type)
	{
		case T_TIDBitmap:
			tbm_end_iterate(iterator->impl.hash);
			break;

		case T_StreamBitmap:
		{
			tbm_stream_end_iterate(iterator->impl.stream);
			break;
		}

		default:
			Assert((bm->type == T_TIDBitmap)
				   || (bm->type == T_StreamBitmap));
	}

	pfree(iterator);
}

/*
 * tbm_find_pageentry - find a PagetableEntry for the pageno
 *
 * Returns NULL if there is no non-lossy entry for the pageno.
 */
static const PagetableEntry *
tbm_find_pageentry(const TIDBitmap *tbm, BlockNumber pageno)
{
	const PagetableEntry *page;

	if (tbm->nentries == 0)		/* in case pagetable doesn't exist */
		return NULL;

	if (tbm->status == TBM_ONE_PAGE)
	{
		page = &tbm->entry1;
		if (page->blockno != pageno)
			return NULL;
		Assert(!page->ischunk);
		return page;
	}

	page = (PagetableEntry *) hash_search(tbm->pagetable,
										  (void *) &pageno,
										  HASH_FIND, NULL);
	if (page == NULL)
		return NULL;
	if (page->ischunk)
		return NULL;			/* don't want a lossy chunk header */
	return page;
}

/*
 * tbm_get_pageentry - find or create a PagetableEntry for the pageno
 *
 * If new, the entry is marked as an exact (non-chunk) entry.
 *
 * This may cause the table to exceed the desired memory size.	It is
 * up to the caller to call tbm_lossify() at the next safe point if so.
 */
static PagetableEntry *
tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	bool		found;

	if (tbm->status == TBM_EMPTY)
	{
		/* Use the fixed slot */
		page = &tbm->entry1;
		found = false;
		tbm->status = TBM_ONE_PAGE;
	}
	else
	{
		if (tbm->status == TBM_ONE_PAGE)
		{
			page = &tbm->entry1;
			if (page->blockno == pageno)
				return page;
			/* Time to switch from one page to a hashtable */
			tbm_create_pagetable(tbm);
		}

		/* Look up or create an entry */
		page = (PagetableEntry *) hash_search(tbm->pagetable,
											  (void *) &pageno,
											  HASH_ENTER, &found);
	}

	/* Initialize it if not present before */
	if (!found)
	{
		MemSet(page, 0, sizeof(PagetableEntry));
		page->blockno = pageno;
		/* must count it too */
		tbm->nentries++;
		tbm->npages++;
	}

	return page;
}

/*
 * tbm_page_is_lossy - is the page marked as lossily stored?
 */
static bool
tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	BlockNumber chunk_pageno;
	int			bitno;

	/* we can skip the lookup if there are no lossy chunks */
	if (tbm->nchunks == 0)
		return false;
	Assert(tbm->status == TBM_HASH);

	bitno = pageno % PAGES_PER_CHUNK;
	chunk_pageno = pageno - bitno;
	page = (PagetableEntry *) hash_search(tbm->pagetable,
										  (void *) &chunk_pageno,
										  HASH_FIND, NULL);
	if (page != NULL && page->ischunk)
	{
		int			wordnum = WORDNUM(bitno);
		int			bitnum = BITNUM(bitno);

		if ((page->words[wordnum] & ((tbm_bitmapword) 1 << bitnum)) != 0)
			return true;
	}
	return false;
}

/*
 * tbm_mark_page_lossy - mark the page number as lossily stored
 *
 * This may cause the table to exceed the desired memory size.	It is
 * up to the caller to call tbm_lossify() at the next safe point if so.
 */
static void
tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	bool		found;
	BlockNumber chunk_pageno;
	int			bitno;
	int			wordnum;
	int			bitnum;

	/* We force the bitmap into hashtable mode whenever it's lossy */
	if (tbm->status != TBM_HASH)
		tbm_create_pagetable(tbm);

	bitno = pageno % PAGES_PER_CHUNK;
	chunk_pageno = pageno - bitno;

	/*
	 * Remove any extant non-lossy entry for the page.	If the page is its own
	 * chunk header, however, we skip this and handle the case below.
	 */
	if (bitno != 0)
	{
		if (hash_search(tbm->pagetable,
						(void *) &pageno,
						HASH_REMOVE, NULL) != NULL)
		{
			/* It was present, so adjust counts */
			tbm->nentries_hwm = Max(tbm->nentries_hwm, tbm->nentries);
			tbm->nentries--;
			tbm->npages--;		/* assume it must have been non-lossy */
		}
	}

	/* Look up or create entry for chunk-header page */
	page = (PagetableEntry *) hash_search(tbm->pagetable,
										  (void *) &chunk_pageno,
										  HASH_ENTER, &found);

	/* Initialize it if not present before */
	if (!found)
	{
		MemSet(page, 0, sizeof(PagetableEntry));
		page->blockno = chunk_pageno;
		page->ischunk = true;
		/* must count it too */
		tbm->nentries++;
		tbm->nchunks++;
	}
	else if (!page->ischunk)
	{
		/* chunk header page was formerly non-lossy, make it lossy */
		MemSet(page, 0, sizeof(PagetableEntry));
		page->blockno = chunk_pageno;
		page->ischunk = true;
		/* we assume it had some tuple bit(s) set, so mark it lossy */
		page->words[0] = ((tbm_bitmapword) 1 << 0);
		/* adjust counts */
		tbm->nchunks++;
		tbm->npages--;
	}

	/* Now set the original target page's bit */
	wordnum = WORDNUM(bitno);
	bitnum = BITNUM(bitno);
	page->words[wordnum] |= ((tbm_bitmapword) 1 << bitnum);
}

/*
 * tbm_lossify - lose some information to get back under the memory limit
 */
static void
tbm_lossify(TIDBitmap *tbm)
{
	HASH_SEQ_STATUS status;
	PagetableEntry *page;

	/*
	 * XXX Really stupid implementation: this just lossifies pages in
	 * essentially random order.  We should be paying some attention to the
	 * number of bits set in each page, instead.
	 *
	 * Since we are called as soon as nentries exceeds maxentries, we should
	 * push nentries down to significantly less than maxentries, or else we'll
	 * just end up doing this again very soon.  We shoot for maxentries/2.
	 */
	Assert(!tbm->iterating);
	Assert(tbm->status == TBM_HASH);

	hash_seq_init(&status, tbm->pagetable);
	while ((page = (PagetableEntry *) hash_seq_search(&status)) != NULL)
	{
		if (page->ischunk)
			continue;			/* already a chunk header */

		/*
		 * If the page would become a chunk header, we won't save anything by
		 * converting it to lossy, so skip it.
		 */
		if ((page->blockno % PAGES_PER_CHUNK) == 0)
			continue;

		/* This does the dirty work ... */
		tbm_mark_page_lossy(tbm, page->blockno);

		if (tbm->nentries <= tbm->maxentries)
		{
			/* we have done enough */
			hash_seq_term(&status);
			break;
		}

		/*
		 * Note: tbm_mark_page_lossy may have inserted a lossy chunk into the
		 * hashtable.  We can continue the same seq_search scan since we do
		 * not care whether we visit lossy chunks or not.
		 */
	}

	/*
	 * With a big bitmap and small work_mem, it's possible that we cannot
	 * get under maxentries.  Again, if that happens, we'd end up uselessly
	 * calling tbm_lossify over and over.  To prevent this from becoming a
	 * performance sink, force maxentries up to at least double the current
	 * number of entries.  (In essence, we're admitting inability to fit
	 * within work_mem when we do this.)  Note that this test will not fire
	 * if we broke out of the loop early; and if we didn't, the current
	 * number of entries is simply not reducible any further.
	 */
	if (tbm->nentries > tbm->maxentries / 2)
		tbm->maxentries = Min(tbm->nentries, (INT_MAX - 1) / 2) * 2;
}

/*
 * qsort comparator to handle PagetableEntry pointers.
 */
static int
tbm_comparator(const void *left, const void *right)
{
	BlockNumber l = (*((const PagetableEntry **) left))->blockno;
	BlockNumber r = (*((const PagetableEntry **) right))->blockno;

	if (l < r)
		return -1;
	else if (l > r)
		return 1;
	return 0;
}

/*
 * functions related to streaming
 */

static void
opstream_free(StreamNode *self)
{
	ListCell   *cell;
	List	   *input = self->opaque;

	foreach(cell, input)
	{
		StreamNode *inp = (StreamNode *) lfirst(cell);

		if (inp->free)
			inp->free(inp);
	}
	list_free(input);
	pfree(self);
}

static void
opstream_set_instrument(StreamNode *self, struct Instrumentation *instr)
{
	ListCell   *cell;
	List	   *input = self->opaque;

	foreach(cell, input)
	{
		StreamNode *inp = (StreamNode *) lfirst(cell);

		if (inp->set_instrument)
			inp->set_instrument(inp, instr);
	}
}

static void
opstream_upd_instrument(StreamNode *self)
{
	ListCell   *cell;
	List	   *input = self->opaque;

	foreach(cell, input)
	{
		StreamNode *inp = (StreamNode *) lfirst(cell);

		if (inp->upd_instrument)
			inp->upd_instrument(inp);
	}
}

static OpStream *
make_opstream(StreamType kind, StreamNode *n1, StreamNode *n2)
{
	OpStream   *op;

	Assert(kind == BMS_OR || kind == BMS_AND);
	Assert(PointerIsValid(n1));

	op = (OpStream *) palloc0(sizeof(OpStream));
	op->type = kind;
	op->opaque = list_make2(n1, n2);
	op->begin_iterate = opstream_begin_iterate;
	op->free = opstream_free;
	op->set_instrument = opstream_set_instrument;
	op->upd_instrument = opstream_upd_instrument;
	return op;
}

/*
 * stream_move_node - move a streamnode from StreamBitMap (source)'s streamnode
 * to given StreamBitMap(destination). Also transfer the ownership of source streamnode by
 * resetting to NULL.
 */
void
stream_move_node(StreamBitmap *destination, StreamBitmap *source, StreamType kind)
{
	Assert(NULL != destination);
	Assert(NULL != source);
	stream_add_node(destination,
			source->streamNode, kind);
	/* destination owns the streamNode and hence resetting it to NULL for source->streamNode. */
	source->streamNode = NULL;
}


/*
 * stream_add_node() - add a new node to a bitmap stream
 * node is a base node -- i.e., an index/external
 * kind is one of BMS_INDEX, BMS_OR or BMS_AND
 */

void
stream_add_node(StreamBitmap *sbm, StreamNode *node, StreamType kind)
{
	/* CDB: Tell node where to put its statistics for EXPLAIN ANALYZE. */
	if (node->set_instrument)
		node->set_instrument(node, sbm->instrument);

	/* initialised */
	if (sbm->streamNode)
	{
		StreamNode *n = sbm->streamNode;

		/* StreamNode is already an index, transform to OpStream */
		if ((n->type == BMS_AND && kind == BMS_AND) ||
			(n->type == BMS_OR && kind == BMS_OR))
		{
			/* n->opaque is our list of inputs; append to it */
			n->opaque = lappend(n->opaque, node);
		}
		else if ((n->type == BMS_AND && kind != BMS_AND) ||
				 (n->type == BMS_OR && kind != BMS_OR) ||
				 (n->type == BMS_INDEX))
		{
			sbm->streamNode = make_opstream(kind, sbm->streamNode, node);
		}
		else
			elog(ERROR, "unknown stream type %i", (int) n->type);
	}
	else
	{
		if (kind == BMS_INDEX)
			sbm->streamNode = node;
		else
			sbm->streamNode = make_opstream(kind, node, NULL);
	}
}

/*
 * tbm_create_stream_node() - turn a TIDBitmap into a stream
 */

StreamNode *
tbm_create_stream_node(TIDBitmap *tbm)
{
	IndexStream *is;

	is = (IndexStream *) palloc0(sizeof(IndexStream));

	is->type = BMS_INDEX;
	is->opaque = tbm;
	is->begin_iterate = index_stream_begin_iterate;
	is->free = tbm_stream_free;
	is->set_instrument = tbm_stream_set_instrument;
	is->upd_instrument = tbm_stream_upd_instrument;

	return is;
}

/*
 * IndexStream iteration callbacks
 */

static void
index_stream_begin_iterate(StreamNode *self, StreamBMIterator *iterator)
{
	TIDBitmap *tbm = self->opaque;

	iterator->pull = tbm_stream_block;
	iterator->end_iterate = index_stream_end_iterate;

	/* Begin iterating on the underlying TIDBitmap. */
	iterator->input.hash = tbm_begin_iterate(tbm);
}

static void
index_stream_end_iterate(StreamBMIterator *self)
{
	tbm_end_iterate(self->input.hash);
}

/*
 * tbm_stream_block() - Fetch the next block from TIDBitmap stream
 *
 * Notice that the IndexStream passed in as opaque will tell us the
 * desired block to stream. If the block requrested is greater than or equal
 * to the block we've cached inside the iterator, return that.
 */

static bool
tbm_stream_block(StreamBMIterator *iterator, PagetableEntry *e)
{
	TBMIterator *hashIterator = iterator->input.hash;
	PagetableEntry *next = iterator->nextentry;
	bool		more;

	Assert(iterator->node->type == BMS_INDEX);

	/* have we already got an entry? */
	if (next && iterator->nextblock <= next->blockno)
	{
		memcpy(e, next, sizeof(PagetableEntry));
		return true;
	}

	/* we need a new entry */
	iterator->nextentry = tbm_next_page(hashIterator, &more);
	if (more)
	{
		Assert(iterator->nextentry);
		memcpy(e, iterator->nextentry, sizeof(PagetableEntry));
	}
	iterator->nextblock++;
	return more;
}

static void
tbm_stream_free(StreamNode *self)
{
	/*
	 * self->opaque is actually a reference to node->bitmap from BitmapIndexScanState
	 * BitmapIndexScanState would have freed the self->opaque already so we shouldn't
	 * access now.
	 */
	pfree(self);
}

static void
tbm_stream_set_instrument(StreamNode *self, struct Instrumentation *instr)
{
	TIDBitmap *tbm = self->opaque;
	Assert(self->type == BMS_INDEX);
	tbm_set_instrument(tbm, instr);
}

static void
tbm_stream_upd_instrument(StreamNode *self)
{
	TIDBitmap *tbm = self->opaque;
	Assert(self->type == BMS_INDEX);
	tbm_upd_instrument(tbm);
}

/*
 * OpStream iteration callbacks
 */

static void
opstream_begin_iterate(StreamNode *self, StreamBMIterator *iterator)
{
	List	 *input = self->opaque;
	ListCell *cell;

	iterator->pull = opstream_iterate;
	iterator->end_iterate = opstream_end_iterate;

	/* Recursively initialize an iterator for each StreamNode. */
	foreach(cell, input)
	{
		StreamNode 		 *inNode = lfirst(cell);
		StreamBMIterator *inIter = tbm_stream_begin_iterate(inNode);

		iterator->input.stream = lappend(iterator->input.stream, inIter);
	}
}

static void
opstream_end_iterate(StreamBMIterator *self)
{
	ListCell *cell;

	/* Recursively free all iterators in the stream "tree". */
	foreach(cell, self->input.stream)
	{
		StreamBMIterator *inIter = lfirst(cell);
		tbm_stream_end_iterate(inIter);
	}
	list_free(self->input.stream);
}

/*
 * opstream_iterate()
 *
 * This is an iterator for OpStreams. The function doesn't
 * know anything about the streams it is actually iterating.
 *
 * Returns false when no more results can be obtained, otherwise true.
 */
static bool
opstream_iterate(StreamBMIterator *iterator, PagetableEntry *e)
{
	const StreamNode   *n = iterator->node;
	bool				res = false;

	/*
	 * There are two ways we can do this: either, we could maintain our
	 * own top level BatchWords structure and pull blocks out of that OR
	 * we could maintain batch words for each sub map and union/intersect
	 * those together to get the resulting page entries.
	 *
	 * Now, BatchWords are specific to bitmap indexes so we'd have to
	 * translate TIDBitmaps. All the infrastructure is available to
	 * translate bitmap indexes into the TIDBitmap mechanism so we'll do
	 * that for now.
	 */
	ListCell   *map;
	BlockNumber minblockno;
	ListCell   *cell;
	int			wordnum;
	List	   *matches;
	bool		empty;


	/*
	 * First, iterate through each input bitmap stream and save the block
	 * which is returned. TIDBitmaps are designed such that they do not
	 * return blocks with no matches -- that is, say a TIDBitmap has
	 * matches for block 1, 4 and 5 it store matches only for those
	 * blocks. Therefore, we may have one stream return a match for block
	 * 10, another for block 15 and another yet for block 10 again. In
	 * this case, we cannot include block 15 in the union/intersection
	 * because it represents matches on some page later in the scan. We'll
	 * get around to it in good time.
	 *
	 * In this case, if we're doing a union, we perform the operation
	 * without reference to block 15. If we're performing an intersection
	 * we cannot perform it on block 10 because we didn't get any matches
	 * for block 10 for one of the streams: the intersection with fail.
	 * So, we set the desired block (op->nextblock) to block 15 and loop
	 * around to the `restart' label.
	 */
restart:
	e->blockno = InvalidBlockNumber;
	empty = false;
	matches = NIL;
	minblockno = InvalidBlockNumber;
	Assert(PointerIsValid(iterator->input.stream));
	foreach(map, iterator->input.stream)
	{
		StreamBMIterator *inIter = lfirst(map);
		PagetableEntry *new;
		bool		r;

		new = (PagetableEntry *) palloc0(sizeof(PagetableEntry));

		/* set the desired block */
		inIter->nextblock = iterator->nextblock;
		r = inIter->pull(inIter, new);

		/*
		 * Let to caller know we got a result from some input bitmap. This
		 * doesn't hold true if we're doing an intersection, and that is
		 * handled below
		 */
		res = res || r;

		/* only include a match if the pull function tells us to */
		if (r)
		{
			if (minblockno == InvalidBlockNumber)
				minblockno = new->blockno;
			else if (n->type == BMS_OR)
				minblockno = Min(minblockno, new->blockno);
			else
				minblockno = Max(minblockno, new->blockno);
			matches = lappend(matches, (void *) new);
		}
		else
		{
			pfree(new);

			if (n->type == BMS_AND)
			{
				/*
				 * No more results for this stream and since we're doing
				 * an intersection we wont get any valid results from now
				 * on, so tell our caller that
				 */
				iterator->nextblock = minblockno + 1;	/* seems safe */
				return false;
			}
			else if (n->type == BMS_OR)
				continue;
		}
	}

	/*
	 * Now we iterate through the actual matches and perform the desired
	 * operation on those from the same minimum block
	 */
	foreach(cell, matches)
	{
		PagetableEntry *tmp = (PagetableEntry *) lfirst(cell);

		if (tmp->blockno == minblockno)
		{
			if (e->blockno == InvalidBlockNumber)
			{
				memcpy(e, tmp, sizeof(PagetableEntry));
				continue;
			}

			/* already initialised, so OR together */
			if (tmp->ischunk == true)
			{
				/*
				 * Okay, new entry is lossy so match our output as lossy
				 */
				e->ischunk = true;
				/* XXX: we can just return now... I think :) */
				iterator->nextblock = minblockno + 1;
				list_free_deep(matches);
				return res;
			}
			/* union/intersect existing output and new matches */
			for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
			{
				if (n->type == BMS_OR)
					e->words[wordnum] |= tmp->words[wordnum];
				else
					e->words[wordnum] &= tmp->words[wordnum];
			}
		}
		else if (n->type == BMS_AND)
		{
			/*
			 * One of our input maps didn't return a block for the desired
			 * block number so, we loop around again.
			 *
			 * Notice that we don't set the next block as minblockno + 1.
			 * We don't know if the other streams will find a match for
			 * minblockno, so we cannot skip past it yet.
			 */

			iterator->nextblock = minblockno;
			empty = true;
			break;
		}
	}
	if (empty)
	{
		/* start again */
		empty = false;
		MemSet(e->words, 0, sizeof(tbm_bitmapword) * WORDS_PER_PAGE);
		list_free_deep(matches);
		goto restart;
	}
	else
		list_free_deep(matches);
	if (res)
		iterator->nextblock = minblockno + 1;

	return res;
}


/*
 * --------- These functions accept either TIDBitmap or StreamBitmap ---------
 */


/*
 * tbm_generic_free - free a TIDBitmap or StreamBitmap
 */
void
tbm_generic_free(Node *bm)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_TIDBitmap:
			tbm_free((TIDBitmap *) bm);
			break;
		case T_StreamBitmap:
			{
				StreamBitmap *sbm = (StreamBitmap *) bm;
				StreamNode *sn = sbm->streamNode;

				sbm->streamNode = NULL;
				if (sn && sn->free)
					sn->free(sn);

				pfree(sbm);

				break;
			}
		default:
			Assert(0);
	}
}	/* tbm_generic_free */


/*
 * tbm_generic_set_instrument - attach caller's Instrumentation object to bitmap
 */
void
tbm_generic_set_instrument(Node *bm, struct Instrumentation *instr)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_TIDBitmap:
			tbm_set_instrument((TIDBitmap *) bm, instr);
			break;
		case T_StreamBitmap:
			{
				StreamBitmap *sbm = (StreamBitmap *) bm;

				if (sbm->streamNode &&
					sbm->streamNode->set_instrument)
					sbm->streamNode->set_instrument(sbm->streamNode, instr);
				break;
			}
		default:
			Assert(0);
	}
}	/* tbm_generic_set_instrument */


/*
 * tbm_generic_upd_instrument - update stats in caller's Instrumentation object
 *
 * Some callers don't bother to tbm_free() their bitmaps, but let the storage
 * be reclaimed when the MemoryContext is reset.  Such callers should use this
 * function to make sure the statistics are transferred to the Instrumentation
 * object before the bitmap goes away.
 */
void
tbm_generic_upd_instrument(Node *bm)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_TIDBitmap:
			tbm_upd_instrument((TIDBitmap *) bm);
			break;
		case T_StreamBitmap:
			{
				StreamBitmap *sbm = (StreamBitmap *) bm;

				if (sbm->streamNode &&
					sbm->streamNode->upd_instrument)
					sbm->streamNode->upd_instrument(sbm->streamNode);
				break;
			}
		default:
			Assert(0);
	}
}	/* tbm_generic_upd_instrument */

void
tbm_convert_appendonly_tid_out(ItemPointer psudeoHeapTid, AOTupleId *aoTid)
{
	/* UNDONE: For now, just copy. */
	memcpy(aoTid, psudeoHeapTid, SizeOfIptrData);
}
