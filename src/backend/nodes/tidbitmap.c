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
 *	  $PostgreSQL: pgsql/src/backend/nodes/tidbitmap.c,v 1.16 2009/01/01 17:23:43 momjian Exp $
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
static bool tbm_iterate_hash(HashBitmap *tbm, TBMIterateResult *output);
static PagetableEntry *tbm_next_page(HashBitmap *tbm, bool *more);

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
	HASHBM_EMPTY,				/* no hashtable, nentries == 0 */
	HASHBM_ONE_PAGE,			/* entry1 contains the single entry */
	HASHBM_HASH					/* pagetable is valid, entry1 is not */
} TBMStatus;

/*
 * Here is the representation for a whole HashBitmap.
 */
struct HashBitmap
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
	PagetableEntry entry1;		/* used when status == HASHBM_ONE_PAGE */
	/* the remaining fields are used while producing sorted output: */
	PagetableEntry **spages;	/* sorted exact-page list, or NULL */
	PagetableEntry **schunks;	/* sorted lossy-chunk list, or NULL */
	int			spageptr;		/* next spages index */
	int			schunkptr;		/* next schunks index */
	int			schunkbit;		/* next bit to check in current schunk */

	/* CDB: Statistics for EXPLAIN ANALYZE */
	struct Instrumentation *instrument;
	Size		bytesperentry;
};

/* A struct to hide away HashBitmap state for a streaming bitmap */
typedef struct HashStreamOpaque
{
	HashBitmap *tbm;  /* HashStreamOpaque will not take the ownership of freeing HashBitmap */
	PagetableEntry *entry;
}	HashStreamOpaque;

/* Local function prototypes */
static void tbm_union_page(HashBitmap *a, const PagetableEntry *bpage);
static bool tbm_intersect_page(HashBitmap *a, PagetableEntry *apage,
				   const HashBitmap *b);
static const PagetableEntry *tbm_find_pageentry(const HashBitmap *tbm,
				   BlockNumber pageno);

static PagetableEntry *tbm_get_pageentry(HashBitmap *tbm, BlockNumber pageno);
static bool tbm_page_is_lossy(const HashBitmap *tbm, BlockNumber pageno);
static void tbm_mark_page_lossy(HashBitmap *tbm, BlockNumber pageno);
static void tbm_lossify(HashBitmap *tbm);
static int	tbm_comparator(const void *left, const void *right);
static bool tbm_stream_block(StreamNode * self, PagetableEntry *e);
static void tbm_stream_free(StreamNode * self);
static void tbm_stream_set_instrument(StreamNode * self, struct Instrumentation *instr);
static void tbm_stream_upd_instrument(StreamNode * self);

/*
 * tbm_create - create an initially-empty bitmap
 *
 * The bitmap will live in the memory context that is CurrentMemoryContext
 * at the time of this call.  It will be limited to (approximately) maxbytes
 * total memory consumption.
 */
HashBitmap *
tbm_create(long maxbytes)
{
	HashBitmap *tbm;
	long		nbuckets;

	/*
	 * Ensure that we don't have heap tuple offsets going beyond (INT16_MAX +
	 * 1) or 32768. The executor iterates only over the first 32K tuples for
	 * lossy bitmap pages [MPP-24326].
	 */
	COMPILE_ASSERT(MaxHeapTuplesPerPage <= (INT16_MAX + 1));

	/*
	 * Create the HashBitmap struct.
	 */
	tbm = (HashBitmap *) palloc0(sizeof(HashBitmap));

	tbm->type = T_HashBitmap;	/* Set NodeTag */
	tbm->mcxt = CurrentMemoryContext;
	tbm->status = HASHBM_EMPTY;
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
tbm_create_pagetable(HashBitmap *tbm)
{
	HASHCTL		hash_ctl;

	Assert(tbm->status != HASHBM_HASH);
	Assert(tbm->pagetable == NULL);

	/* Create the hashtable proper */
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(BlockNumber);
	hash_ctl.entrysize = sizeof(PagetableEntry);
	hash_ctl.hash = tag_hash;
	hash_ctl.hcxt = tbm->mcxt;
	tbm->pagetable = hash_create("HashBitmap",
								 128,	/* start small and extend */
								 &hash_ctl,
								 HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

	/* If entry1 is valid, push it into the hashtable */
	if (tbm->status == HASHBM_ONE_PAGE)
	{
		PagetableEntry *page;
		bool		found;

		page = (PagetableEntry *) hash_search(tbm->pagetable,
											  (void *) &tbm->entry1.blockno,
											  HASH_ENTER, &found);
		Assert(!found);
		memcpy(page, &tbm->entry1, sizeof(PagetableEntry));
	}

	tbm->status = HASHBM_HASH;
}

/*
 * tbm_free - free a HashBitmap
 */
void
tbm_free(HashBitmap *tbm)
{
	if (tbm->instrument)
		tbm_bitmap_upd_instrument((Node *) tbm);
	if (tbm->pagetable)
		hash_destroy(tbm->pagetable);
	if (tbm->spages)
		pfree(tbm->spages);
	if (tbm->schunks)
		pfree(tbm->schunks);
	pfree(tbm);
}


/*
 * tbm_upd_instrument - Update the Instrumentation attached to a HashBitmap.
 */
static void
tbm_upd_instrument(HashBitmap *tbm)
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
 *	Attach caller's Instrumentation object to a HashBitmap, unless the
 *	HashBitmap already has one.  We want the statistics to be associated
 *	with the plan node which originally created the bitmap, rather than a
 *	downstream consumer of the bitmap.
 */
static void
tbm_set_instrument(HashBitmap *tbm, struct Instrumentation *instr)
{
	if (instr == NULL ||
		tbm->instrument == NULL)
	{
		tbm->instrument = instr;
		tbm_upd_instrument(tbm);
	}
}	/* tbm_set_instrument */


/*
 * tbm_add_tuples - add some tuple IDs to a HashBitmap
 */
void
tbm_add_tuples(HashBitmap *tbm, const ItemPointer tids, int ntids,
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
 * tbm_union - set union
 *
 * a is modified in-place, b is not changed
 */
void
tbm_union(HashBitmap *a, const HashBitmap *b)
{
	Assert(!a->iterating);
	/* Nothing to do if b is empty */
	if (b->nentries == 0)
		return;
	/* Scan through chunks and pages in b, merge into a */
	if (b->status == HASHBM_ONE_PAGE)
		tbm_union_page(a, &b->entry1);
	else
	{
		HASH_SEQ_STATUS status;
		PagetableEntry *bpage;

		Assert(b->status == HASHBM_HASH);
		hash_seq_init(&status, b->pagetable);
		while ((bpage = (PagetableEntry *) hash_seq_search(&status)) != NULL)
			tbm_union_page(a, bpage);
	}
}

/* Process one page of b during a union op */
static void
tbm_union_page(HashBitmap *a, const PagetableEntry *bpage)
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
tbm_intersect(HashBitmap *a, const HashBitmap *b)
{
	Assert(!a->iterating);
	/* Nothing to do if a is empty */
	if (a->nentries == 0)
		return;

	a->nentries_hwm = Max(a->nentries_hwm, a->nentries);

	/* Scan through chunks and pages in a, try to match to b */
	if (a->status == HASHBM_ONE_PAGE)
	{
		if (tbm_intersect_page(a, &a->entry1, b))
		{
			/* Page is now empty, remove it from a */
			Assert(!a->entry1.ischunk);
			a->npages--;
			a->nentries--;
			Assert(a->nentries == 0);
			a->status = HASHBM_EMPTY;
		}
	}
	else
	{
		HASH_SEQ_STATUS status;
		PagetableEntry *apage;

		Assert(a->status == HASHBM_HASH);
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
tbm_intersect_page(HashBitmap *a, PagetableEntry *apage, const HashBitmap *b)
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
 * tbm_is_empty - is a HashBitmap completely empty?
 */
bool
tbm_is_empty(const HashBitmap *tbm)
{
	return (tbm->nentries == 0);
}

/*
 * tbm_begin_iterate - prepare to iterate through a HashBitmap
 *
 * NB: after this is called, it is no longer allowed to modify the contents
 * of the bitmap.  However, you can call this multiple times to scan the
 * contents repeatedly.
 */
void
tbm_begin_iterate(HashBitmap *tbm)
{
	HASH_SEQ_STATUS status;
	PagetableEntry *page;
	int			npages;
	int			nchunks;

	tbm->iterating = true;

	/*
	 * Reset iteration pointers.
	 */
	tbm->spageptr = 0;
	tbm->schunkptr = 0;
	tbm->schunkbit = 0;

	/*
	 * Nothing else to do if no entries, nor if we don't have a hashtable.
	 */
	if (tbm->nentries == 0 || tbm->status != HASHBM_HASH)
		return;

	/*
	 * Create and fill the sorted page lists if we didn't already.
	 */
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
		qsort(tbm->spages, npages, sizeof(PagetableEntry *), tbm_comparator);
	if (nchunks > 1)
		qsort(tbm->schunks, nchunks, sizeof(PagetableEntry *), tbm_comparator);
}

/*
 * tbm_iterate - scan through next page of a HashBitmap or a StreamBitmap.
 */
bool
tbm_iterate(Node *tbm, TBMIterateResult *output)
{
	Assert(IsA(tbm, HashBitmap) || IsA(tbm, StreamBitmap));

	switch (tbm->type)
	{
		case T_HashBitmap:
			{
				HashBitmap *hashBitmap = (HashBitmap *) tbm;

				if (!hashBitmap->iterating)
					tbm_begin_iterate(hashBitmap);

				return tbm_iterate_hash(hashBitmap, output);
			}
		case T_StreamBitmap:
			{
				StreamBitmap *streamBitmap = (StreamBitmap *) tbm;
				bool		status;
				StreamNode *s;

				s = streamBitmap->streamNode;

				status = bitmap_stream_iterate((void *) s, &(streamBitmap->entry));

				/* XXX: perhaps we should only do this if status == true ? */
				tbm_iterate_page(&(streamBitmap->entry), output);

				return status;
			}
		default:
			elog(ERROR, "unrecoganized node type");
	}

	return false;
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
 * tbm_iterate_hash - scan through next page of a HashBitmap
 *
 * Gets a TBMIterateResult representing one page, or NULL if there are
 * no more pages to scan.  Pages are guaranteed to be delivered in numerical
 * order.  If result->ntuples < 0, then the bitmap is "lossy" and failed to
 * remember the exact tuples to look at on this page --- the caller must
 * examine all tuples on the page and check if they meet the intended
 * condition.
 *
 * If 'output' is NULL, simple advance the HashBitmap by one.
 */
static bool
tbm_iterate_hash(HashBitmap *tbm, TBMIterateResult *output)
{
	PagetableEntry *e;
	bool		more;

	e = tbm_next_page(tbm, &more);
	if (more && e)
	{
		tbm_iterate_page(e, output);
		return true;
	}
	return false;
}

/*
 * tbm_next_page - actually traverse the HashBitmap
 *
 * Store the next block of matches in nextpage.
 */

static PagetableEntry *
tbm_next_page(HashBitmap *tbm, bool *more)
{
	Assert(tbm->iterating);

	*more = true;

	/*
	 * If lossy chunk pages remain, make sure we've advanced schunkptr/
	 * schunkbit to the next set bit.
	 */
	while (tbm->schunkptr < tbm->nchunks)
	{
		PagetableEntry *chunk = tbm->schunks[tbm->schunkptr];
		int			schunkbit = tbm->schunkbit;

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
			tbm->schunkbit = schunkbit;
			break;
		}
		/* advance to next chunk */
		tbm->schunkptr++;
		tbm->schunkbit = 0;
	}

	/*
	 * If both chunk and per-page data remain, must output the numerically
	 * earlier page.
	 */
	if (tbm->schunkptr < tbm->nchunks)
	{
		PagetableEntry *chunk = tbm->schunks[tbm->schunkptr];
		PagetableEntry *nextpage;
		BlockNumber chunk_blockno;

		chunk_blockno = chunk->blockno + tbm->schunkbit;
		if (tbm->spageptr >= tbm->npages ||
			chunk_blockno < tbm->spages[tbm->spageptr]->blockno)
		{
			/* Return a lossy page indicator from the chunk */
			nextpage = (PagetableEntry *) palloc(sizeof(PagetableEntry));
			nextpage->ischunk = true;
			nextpage->blockno = chunk_blockno;
			nextpage->recheck = true;
			tbm->schunkbit++;
			return nextpage;
		}
	}

	if (tbm->spageptr < tbm->npages)
	{
		PagetableEntry *e;

		/* In ONE_PAGE state, we don't allocate an spages[] array */
		if (tbm->status == HASHBM_ONE_PAGE)
			e = &tbm->entry1;
		else
			e = tbm->spages[tbm->spageptr];

		tbm->spageptr++;
		return e;
	}

	/* Nothing more in the bitmap */
	*more = false;
	return NULL;
}

/*
 * tbm_find_pageentry - find a PagetableEntry for the pageno
 *
 * Returns NULL if there is no non-lossy entry for the pageno.
 */
static const PagetableEntry *
tbm_find_pageentry(const HashBitmap *tbm, BlockNumber pageno)
{
	const PagetableEntry *page;

	if (tbm->nentries == 0)		/* in case pagetable doesn't exist */
		return NULL;

	if (tbm->status == HASHBM_ONE_PAGE)
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
tbm_get_pageentry(HashBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	bool		found;

	if (tbm->status == HASHBM_EMPTY)
	{
		/* Use the fixed slot */
		page = &tbm->entry1;
		found = false;
		tbm->status = HASHBM_ONE_PAGE;
	}
	else
	{
		if (tbm->status == HASHBM_ONE_PAGE)
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
tbm_page_is_lossy(const HashBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	BlockNumber chunk_pageno;
	int			bitno;

	/* we can skip the lookup if there are no lossy chunks */
	if (tbm->nchunks == 0)
		return false;
	Assert(tbm->status == HASHBM_HASH);

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
tbm_mark_page_lossy(HashBitmap *tbm, BlockNumber pageno)
{
	PagetableEntry *page;
	bool		found;
	BlockNumber chunk_pageno;
	int			bitno;
	int			wordnum;
	int			bitnum;

	/* We force the bitmap into hashtable mode whenever it's lossy */
	if (tbm->status != HASHBM_HASH)
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
tbm_lossify(HashBitmap *tbm)
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
	Assert(tbm->status == HASHBM_HASH);

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

	foreach(cell, self->input)
	{
		StreamNode *inp = (StreamNode *) lfirst(cell);

		if (inp->free)
			inp->free(inp);
	}
	list_free(self->input);
	pfree(self);
}

static void
opstream_set_instrument(StreamNode *self, struct Instrumentation *instr)
{
	ListCell   *cell;

	foreach(cell, self->input)
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

	foreach(cell, self->input)
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
	op->pull = bitmap_stream_iterate;
	op->nextblock = 0;
	op->input = list_make2(n1, n2);
	op->free = opstream_free;
	op->set_instrument = opstream_set_instrument;
	op->upd_instrument = opstream_upd_instrument;
	return (void *) op;
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
			OpStream   *o = (OpStream *) n;

			o->input = lappend(o->input, node);
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
 * tbm_create_stream_node() - turn a HashBitmap into a stream
 */

StreamNode *
tbm_create_stream_node(HashBitmap *tbm)
{
	IndexStream *is;
	HashStreamOpaque *op;

	is = (IndexStream *) palloc0(sizeof(IndexStream));
	op = (HashStreamOpaque *) palloc(sizeof(HashStreamOpaque));

	is->type = BMS_INDEX;
	is->nextblock = 0;
	is->pull = tbm_stream_block;
	is->free = tbm_stream_free;
	is->set_instrument = tbm_stream_set_instrument;
	is->upd_instrument = tbm_stream_upd_instrument;

	op->tbm = tbm;
	op->entry = NULL;

	is->opaque = (void *) op;

	return is;
}

/*
 * tbm_stream_block() - Fetch the next block from HashBitmap stream
 *
 * Notice that the IndexStream passed in as opaque will tell us the
 * desired block to stream. If the block requrested is greater than or equal
 * to the block we've cached inside the HashStreamOpaque, return that.
 */

static bool
tbm_stream_block(StreamNode *self, PagetableEntry *e)
{
	IndexStream *is = self;
	HashStreamOpaque *op = (HashStreamOpaque *) is->opaque;
	HashBitmap *tbm = op->tbm;
	PagetableEntry *next = op->entry;
	bool		more;

	/* have we already got an entry? */
	if (next && is->nextblock <= next->blockno)
	{
		memcpy(e, next, sizeof(PagetableEntry));
		return true;
	}

	if (!tbm->iterating)
		tbm_begin_iterate(tbm);

	/* we need a new entry */
	op->entry = tbm_next_page(tbm, &more);
	if (more)
	{
		Assert(op->entry);
		memcpy(e, op->entry, sizeof(PagetableEntry));
	}
	is->nextblock++;
	return more;
}

static void
tbm_stream_free(StreamNode *self)
{
	HashStreamOpaque *op = (HashStreamOpaque *) self->opaque;
	/*
	 * op->tbm is actually a reference to node->bitmap from BitmapIndexScanState
	 * BitmapIndexScanState would have freed the op->tbm already so we shouldn't
	 * access now.
	 */
	pfree(op);
	pfree(self);
}

static void
tbm_stream_set_instrument(StreamNode *self, struct Instrumentation *instr)
{
	tbm_set_instrument(((HashStreamOpaque *) self->opaque)->tbm, instr);
}

static void
tbm_stream_upd_instrument(StreamNode *self)
{
	tbm_upd_instrument(((HashStreamOpaque *) self->opaque)->tbm);
}


/*
 * bitmap_stream_iterate()
 *
 * This is a generic iterator for bitmap streams. The function doesn't
 * know anything about the streams it is actually iterating.
 *
 * Returns false when no more results can be obtained, otherwise true.
 */

bool
bitmap_stream_iterate(StreamNode *n, PagetableEntry *e)
{
	bool		res = false;

	MemSet(e, 0, sizeof(PagetableEntry));

	if (n->type == BMS_INDEX)
	{
		IndexStream *is = (IndexStream *) n;

		res = is->pull((void *) is, e);
	}
	else if (n->type == BMS_OR || n->type == BMS_AND)
	{
		/*
		 * There are two ways we can do this: either, we could maintain our
		 * own top level BatchWords structure and pull blocks out of that OR
		 * we could maintain batch words for each sub map and union/intersect
		 * those together to get the resulting page entries.
		 *
		 * Now, BatchWords are specific to bitmap indexes so we'd have to
		 * translate HashBitmaps. All the infrastructure is available to
		 * translate bitmap indexes into the HashBitmap mechanism so we'll do
		 * that for now.
		 */
		ListCell   *map;
		OpStream   *op = (OpStream *) n;
		BlockNumber minblockno;
		ListCell   *cell;
		int			wordnum;
		List	   *matches;
		bool		empty;


		/*
		 * First, iterate through each input bitmap stream and save the block
		 * which is returned. HashBitmaps are designed such that they do not
		 * return blocks with no matches -- that is, say a HashBitmap has
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
		Assert(PointerIsValid(op->input));
		foreach(map, op->input)
		{
			StreamNode *in = (StreamNode *) lfirst(map);
			PagetableEntry *new;
			bool		r;

			new = (PagetableEntry *) palloc(sizeof(PagetableEntry));

			/* set the desired block */
			in->nextblock = op->nextblock;
			r = in->pull((void *) in, new);

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
					op->nextblock = minblockno + 1;		/* seems safe */
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
					op->nextblock = minblockno + 1;
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

				op->nextblock = minblockno;
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
			op->nextblock = minblockno + 1;
	}
	return res;
}


/*
 * --------- These functions accept either HashBitmap or StreamBitmap ---------
 */


/*
 * tbm_bitmap_free - free a HashBitmap or StreamBitmap
 */
void
tbm_bitmap_free(Node *bm)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_HashBitmap:
			tbm_free((HashBitmap *) bm);
			break;
		case T_StreamBitmap:
			{
				StreamBitmap *sbm = (StreamBitmap *) bm;
				StreamNode *sn = sbm->streamNode;

				sbm->streamNode = NULL;
				if (sn &&
					sn->free)
					sn->free(sn);

				pfree(sbm);

				break;
			}
		default:
			Assert(0);
	}
}	/* tbm_bitmap_free */


/*
 * tbm_bitmap_set_instrument - attach caller's Instrumentation object to bitmap
 */
void
tbm_bitmap_set_instrument(Node *bm, struct Instrumentation *instr)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_HashBitmap:
			tbm_set_instrument((HashBitmap *) bm, instr);
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
}	/* tbm_bitmap_set_instrument */


/*
 * tbm_bitmap_upd_instrument - update stats in caller's Instrumentation object
 *
 * Some callers don't bother to tbm_free() their bitmaps, but let the storage
 * be reclaimed when the MemoryContext is reset.  Such callers should use this
 * function to make sure the statistics are transferred to the Instrumentation
 * object before the bitmap goes away.
 */
void
tbm_bitmap_upd_instrument(Node *bm)
{
	if (bm == NULL)
		return;

	switch (bm->type)
	{
		case T_HashBitmap:
			tbm_upd_instrument((HashBitmap *) bm);
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
}	/* tbm_bitmap_upd_instrument */

void
tbm_convert_appendonly_tid_out(ItemPointer psudeoHeapTid, AOTupleId *aoTid)
{
	/* UNDONE: For now, just copy. */
	memcpy(aoTid, psudeoHeapTid, SizeOfIptrData);
}
