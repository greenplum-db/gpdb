/*-------------------------------------------------------------------------
 *
 * tuplesort_mkheap.c
 *	  Multi level key heap.
 *
 * Portions Copyright (c) Greenplum Inc, 2008-2009.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/sort/tuplesort_mkheap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "access/genam.h"
#include "utils/tuplesort.h"
#include "utils/tuplesort_mk.h"
#include "utils/tuplesort_mk_details.h"
#include "utils/datum.h"
#include "utils/builtins.h"
#include "miscadmin.h"

#include "cdb/cdbvars.h"

/*
 * Multi-Key Heap.
 *
 * This is a heap, plus an array.  The array (called lvtops) holds the first N keys of the top heap element.
 *	  N is the entry's level
 *	  For other entries, their level indicates for how many of those N keys they match the top heap element
 *
 * For example, a heap with 3 multi-key entries:
 *	  ['bob','barker','impostor']		<== this is the top entry of the heap
 *	  ['bob','barker','price is right']
 *	  ['bob','evans', 'sausage']
 *
 * So, our lvtops could have:
 *	  ['bob','barker']
 *
 *	  And our entries would have the levels:
 *		   2: top entry of the heap so indicates # of values in lvtops
 *		   2: matches the top entry/lvtops on the first two keys ('bob' and 'barker')
 *		   1: matches the top entry/lvtops on first key ('bob')
 *
 *	  Note that lvtops could also have the following, with the given entries
 *	  ['bob','barker','impostor']
 *		   3: top entry of the heap so indicates # of values in lvtops
 *		   2: matches the top entry/lvtops on the first two keys ('bob' and 'barker')
 *		   1: matches the top entry/lvtops on first key ('bob')
 *
 *	Therefore, level tells you something useful about the relationship between values.
 *
 *	Note also that because we CANNOT insert values SMALLER than the top of the heap,
 *	   the level of an entry (say, e) never goes down -- because that would require lvtops
 *	   values to become smaller which is impossible without adding a smaller value!
 *
 * The purpose of this weird heap is that:
 *	  1) we do not do any unnecessary lv expansions -- meaning, many tuples in a multi-key comparison differ
 *		 only in first few keys so no need to materialize the later keys and comparable keys.
 *	  2) the compare is cheap because once you've determined the level of a row then you can compare using only level
 *
 * Testing note: see attachment on MPP-7231 for some useful testing code
 *
 *
 *
 * NOTE ON UNIQUENESS CHECKING:
 *
 * When required to enforce uniqueness, by necessity we can only do that either when:
 *	  a) a value is inserted and we detect that it equals the root then uniqueness is violated
 *	  b) at any point, if one of the root's children exactly matches the root then uniqueness is violated
 *
 * However, when inserting a value that is greater than the root then we can't check whether it is
 *	  equal to some other value in the heap.  Therefore, the heap may contain duplicate values at
 *	  any point in time.
 *
 * For this reason, we do NOT try to check uniqueness when building from array -- it would take time and
 *	  we are assuming that duplicates inside the heaps will be later detected by case b) above.
 */


/* During merge phase, we check for interrupts every COMPARES_BETWEEN_INTERRUPT_CHECKS comparisons */
#define COMPARES_BETWEEN_INTERRUPT_CHECKS 100000
/* Counter to keep track of how many comparisons since last CHECK_FOR_INTERRUPTS */
static uint32 compare_count = 0;

static void mkheap_heapify(MKHeap *mkheap, bool doLv0);
static bool mke_has_duplicates_with_root(MKHeap *mkheap);

static inline int
LEFT(int n)
{
	Assert(n >= 0);
	return (n << 1) + 1;
}
static inline int
RIGHT(int n)
{
	Assert(n >= 0);
	return (n << 1) + 2;
}
static inline int
PARENT(int n)
{
	Assert(n > 0);
	return (n - 1) >> 1;
}

/*
 * Heap compare.
 *	Rule:
 *	1. Empty entry is always bigger, so empty cells will be at the bottom
 *	2. A entry is expanded at lv, means 0 - (lv-1) is the same as heaptop.
 *	3. From 2, deeper level is smaller.
 *	4. Smaller run is always smaller.  Why we do this after 2. and 3.?
 *		When a entry is inserted, we don't know this entry belongs to
 *		current run on the next run yet.
 *	5. Same run, same level, then we compare the null flag.  Pg 8.2 (that is,
 *		   what we are using, always use null last.)   We do have a null first bit
 *		   reserved.
 *	6. Compare non null datum.
 *
 * These rules guarantee the heap top is minimal of current run.  However, at
 * deeper level, mkheap_compare return 0 does not mean the two entry are really
 * equal.  They are just equal up to a certain level.
 *
 * Step 1 - 5 are captured in compflags.
 */
static inline int32
mkheap_compare(MKHeap *heap, MKEntry *a, MKEntry *b)
{
	/*
	 * Check for interrupts every COMPARES_BETWEEN_INTERRUPT_CHECKS
	 * comparisons
	 */
	if (compare_count++ % COMPARES_BETWEEN_INTERRUPT_CHECKS == 0)
	{
		CHECK_FOR_INTERRUPTS();
	}

	int32		ret = (a->compflags & (~MKE_CF_NULLBITS)) -
	(b->compflags & (~MKE_CF_NULLBITS));

	if (ret == 0)
	{
		int32		limitmask = heap->mkctxt->limitmask;
		bool		needcmp = false;

		ret = a->compflags - b->compflags;

		/* need comparison if non-null values compare equal by compflags */
		needcmp = (ret == 0) && (!mke_is_null(a));

		if (needcmp)
		{
			/*
			 * two entries were equal according to flags.  This means the
			 * first lv entries are the same
			 *
			 * So now compare the actual Datums for the next level (which is
			 * level at index lv)
			 */
			int32		lv = mke_get_lv(a);

			Assert(lv < heap->mkctxt->total_lv);
			Assert(lv == mke_get_lv(b));
			ret = tupsort_compare_datum(a, b, heap->mkctxt->lvctxt + lv, heap->mkctxt);
		}

		/*
		 * limitmask is either 0 or -1.  If 0 then ~limitmask is all 1s, else
		 * it's all zeroes so this is equivalanet to        uselimit ? -ret :
		 * ret
		 *
		 * So this causes a REVERSE sort -- which is what tuplesort_limit_sort
		 * wants and because for tuplesort_limit_insert we actually want to
		 * POP the HIGHER values, not the lower ones since we are trying to
		 * find the smallest elements
		 *
		 * There's no need to reverse the initial comparison of this function
		 * because it is considering only differences in levels and runs,
		 * which already represent the reversed order.
		 *
		 * todo: bug when ret is the min possible int val (so negating it
		 * leaves it unchanged).  (and earlier comp does not guarantee -1,0,1
		 * as results) todo: rename limitmask to reverseSortMask...
		 */
		ret = (ret & (~limitmask)) + ((-ret) & limitmask);
	}

	return ret;
}

#ifdef USE_ASSERT_CHECKING
extern void mkheap_verify_heap(MKHeap *heap, int top);
#else
#define mkheap_verify_heap(heap, top)
#endif

/*
 * Create a multi-key heap from an array of entries
 *
 * entries: the values to convert to a heap.  This array will be under mkheap's ownership
 * alloc_sz: the allocation size of entries: that is, how much room the array has.
 * cnt: the number of elements in entries which should be used to build the heap
 * mkctxt: description of the heap to build
 *
 * If alloc_sz is zero then entries must be NULL
 */
MKHeap *
mkheap_from_array(MKEntry *entries, int alloc_sz, int cnt, MKContext *mkctxt)
{
	MKHeap	   *heap = (MKHeap *) palloc(sizeof(MKHeap));

	Assert(mkctxt);
	Assert(alloc_sz >= cnt);
	AssertEquivalent(entries != NULL, cnt > 0);
	AssertEquivalent(!entries, cnt == 0);

	heap->mkctxt = mkctxt;
	heap->lvtops = palloc0(mkctxt->total_lv * sizeof(MKEntry));

	heap->readers = NULL;
	heap->nreader = 0;

	AssertImply(alloc_sz == 0, !entries);
	Assert(cnt >= 0 && cnt <= alloc_sz);

	heap->p = entries;
	heap->alloc_size = alloc_sz;
	heap->count = cnt;
	heap->maxentry = cnt;

#ifdef USE_ASSERT_CHECKING
	{
		int			i;

		for (i = 0; i < cnt; ++i)
		{
			Assert(mke_get_lv(entries + i) == 0);
			Assert(mke_get_reader(entries + i) == 0);
		}
	}
#endif

	/*
	 * note: see NOTE ON UNIQUENESS CHECKING  at the top of this file for
	 * information about why we don't check uniqueness here
	 */

	mk_prepare_array(entries, 0, cnt - 1, 0, mkctxt);
	mkheap_heapify(heap, true);
	return heap;
}

/*
 * Create a mkheap from an array of readers, which are assumed to provide their
 *	  values in sorted order.
 *
 * Invoking this causes each reader to be invoked once to get the first value.
 */
MKHeap *
mkheap_from_reader(MKHeapReader *readers, int nreader, MKContext *ctxt)
{
	int			i,
				j;
	MKHeap	   *heap = (MKHeap *) palloc(sizeof(MKHeap));

	Assert(readers && nreader > 0);
	Assert(ctxt);

	heap->mkctxt = ctxt;
	heap->lvtops = palloc0(ctxt->total_lv * sizeof(MKEntry));
	heap->readers = readers;
	heap->nreader = nreader;

	/* Create initial heap. */
	heap->p = palloc0(sizeof(MKEntry) * (nreader + 1));
	heap->alloc_size = nreader + 1;

	for (i = 0, j = 0; i < nreader; ++i)
	{
		MKEntry    *e = heap->p + j;
		bool		fOK = readers[i].reader(readers[i].mkhr_ctxt, e);

		if (fOK)
		{
			mke_set_reader(e, i);
			++j;
		}
	}

	heap->count = j;
	heap->maxentry = j;

	/* at least one reader had a value so convert the values to the heap */
	if (j > 0)
	{
		mk_prepare_array(heap->p, 0, j - 1, 0, ctxt);
		mkheap_heapify(heap, true);
	}

	return heap;
}

/*
 * Destroy mkheap.  Free entry arrays at each level.
 *
 * Does NOT free the entries themselves.
 */
void
mkheap_destroy(MKHeap *mkheap)
{
	if (mkheap->p)
		pfree(mkheap->p);

	if (mkheap->lvtops)
		pfree(mkheap->lvtops);

	pfree(mkheap);
}

/*
 * MKHeap siftdown an entry, starting with a hole.
 */
static void
mkheap_siftdown(MKHeap *heap, int hole, MKEntry *e)
{
	int			c;
	int			left;
	int			right;
	int			child;

	static int	roundrobin = 0;

	Assert(hole >= 0 && hole < heap->maxentry);

	while (1)
	{
		left = LEFT(hole);
		right = RIGHT(hole);
		child = left;

		/* determine into which child to try sifting */
		if (right < heap->maxentry)
		{
			c = mkheap_compare(heap, heap->p + left, heap->p + right);

			if (c == 0)
				child = left + ((roundrobin++) & 1);
			else if (c > 0)
				child = right;
		}

		/* sift there if possible */
		if (child < heap->maxentry)
		{
			c = mkheap_compare(heap, e, heap->p + child);

			if (c > 0)
			{
				heap->p[hole] = heap->p[child];
				hole = child;
				continue;
			}
		}

		/* We are in right position */
		heap->p[hole] = *e;
		return;
	}

	Assert(!"Never here");
}

/*
 * Prepare heap (expand lv) at postion cur.
 */
static void
mkheap_prep_siftdown_lv(MKHeap *mkheap, int lv, int cur, MKContext *mkctxt)
{
	bool		expchild = false;

	int			c;

	Assert(cur < mkheap->maxentry);
	Assert(cur >= 0);

	/* If necessary, expand right */
	if (RIGHT(cur) < mkheap->maxentry)
	{
		c = mkheap_compare(mkheap, mkheap->p + cur, mkheap->p + RIGHT(cur));
		if (c == 0)
		{
			mkheap_prep_siftdown_lv(mkheap, lv, RIGHT(cur), mkctxt);
			expchild = true;
		}
	}

	/* If necessary, expand left */
	if (LEFT(cur) < mkheap->maxentry)
	{
		c = mkheap_compare(mkheap, mkheap->p + cur, mkheap->p + LEFT(cur));
		if (c == 0)
		{
			mkheap_prep_siftdown_lv(mkheap, lv, LEFT(cur), mkctxt);
			expchild = true;
		}
	}

	Assert(mke_get_lv(mkheap->p + cur) == lv - 1);
	if (mkheap->mkctxt->fetchForPrep)
		tupsort_prepare(mkheap->p + cur, mkctxt, lv);

	mke_set_lv(mkheap->p + cur, lv);

	if (expchild)
	{
		MKEntry		tmp = mkheap->p[cur];

		mkheap_siftdown(mkheap, cur, &tmp);
	}
}

/*
 * Update all lvtops entries. The lvtops entries whose array index is not greater
 * than the level value stored in the current top element in the heap, update them
 * with the value from the top element in the heap. This function also cleans up
 * the remaining lvtops entries.
 *
 * This is needed after the current top entry in the heap is removed from the heap, since
 * some lvtops' ptr may point to the tuple that could be freed when its associated entry
 * is removed from the heap.
 */
static void
mkheap_update_lvtops(MKHeap *mkheap)
{
	int			top_lv = mke_get_lv(mkheap->p);

	for (int lv = 0; lv < mkheap->mkctxt->total_lv; lv++)
	{
		MKEntry    *lvEntry = mkheap->lvtops + lv;
		MKEntry    *srcEntry = NULL;

		if (lv <= top_lv)
		{
			srcEntry = mkheap->p;

			if (mkheap->mkctxt->fetchForPrep)
				tupsort_prepare(mkheap->p, mkheap->mkctxt, lv);
		}

		(*mkheap->mkctxt->cpfr) (lvEntry, srcEntry, mkheap->mkctxt->lvctxt + lv);

		/* Set the correct level */
		mke_set_lv(lvEntry, lv);
	}
}

/**
 * Save the value for mkheap's current top (at the level entered in the current top) into lvtops.
 */
static void
mkheap_save_lvtop(MKHeap *mkheap)
{
	int			lv;
	MKLvContext *lvctxt;

	Assert(mkheap->count > 0);

	lv = mke_get_lv(mkheap->p);
	lvctxt = mkheap->mkctxt->lvctxt + lv;

	Assert(lv < mkheap->mkctxt->total_lv);

	/* Free the old one and copy in the new one */
	(*mkheap->mkctxt->cpfr) (mkheap->lvtops + lv, mkheap->p, lvctxt);
}

static inline bool
mkheap_need_heapify(MKHeap *mkheap)
{
    return (
            /* More than one entry */
            mkheap_cnt(mkheap) > 1 &&
            /* Not fully expanded */
            mke_get_lv(mkheap->p) < mkheap->mkctxt->total_lv-1 &&
            /* Cannot determine by comp flag */
            (
             (1 < mkheap->maxentry && mkheap->p[0].compflags == mkheap->p[1].compflags)
             ||
             (2 < mkheap->maxentry && mkheap->p[0].compflags == mkheap->p[2].compflags)
            )
           );
}

static void
mkheap_heapify(MKHeap *mkheap, bool doLv0)
{
	int			i;
	MKEntry		tmp;

	/* Build heap, expanded already at lv 0 */
	if (doLv0)
	{
		for (i = PARENT(mkheap->maxentry); i >= 0; --i)
		{
			tmp = mkheap->p[i];
			mkheap_siftdown(mkheap, i, &tmp);
		}
	}

	/* Populate deeper levels */
	while (mke_get_lv(mkheap->p) < mkheap->mkctxt->total_lv - 1)
	{
		int			nlv = mke_get_lv(mkheap->p) + 1;

		Assert(nlv < mkheap->mkctxt->total_lv);

		mkheap_save_lvtop(mkheap);
		mkheap_prep_siftdown_lv(mkheap, nlv, 0, mkheap->mkctxt);
	}

#ifdef USE_ASSERT_CHECKING
	if (gp_mk_sort_check)
		mkheap_verify_heap(mkheap, 0);
#endif
}

static bool
mke_has_duplicates_with_root(MKHeap *mkheap)
{
	/** since the heap is heapified, only if the level is at the max can
	 *	 we have a duplicate
	 */
	if (mke_get_lv(mkheap->p) + 1 != mkheap->mkctxt->total_lv)
		return false;

	/* see if we have children which exactly match the root */
	if (RIGHT(0) < mkheap->maxentry)
	{
		int			cmp = mkheap_compare(mkheap, mkheap->p, mkheap->p + RIGHT(0));

		if (cmp == 0)
			return true;
	}

	if (LEFT(0) < mkheap->maxentry)
	{
		int			cmp = mkheap_compare(mkheap, mkheap->p, mkheap->p + LEFT(0));

		if (cmp == 0)
			return true;
	}
	return false;
}

/*
 * Insert an entry and perhaps return the top element of the heap in *e
 *
 * Comparison happens from the specified level to the end of levels, as needed:
 *	 Return < 0 if smaller than heap top; *e is unchanged
 *	 Return = 0 if eq to heap top ; *e is unchanged (but will have value equal to the heap top)
 *	 Return > 0 if successfully inserted; *e is populated with the removed heap top
 *
 * If 0 would be returned but the heap is marked as needing uniqueness enforcement, error is generated instead
 */
static int
mkheap_putAndGet_impl(MKHeap *mkheap, MKEntry *e)
{
	int			c = 0;
	int			toplv;
	MKEntry		tmp;

	/* can't put+get from an empty heap */
	Assert(mkheap->count > 0);

	if (mkheap->mkctxt->enforceUnique &&
		mke_has_duplicates_with_root(mkheap))
	{
		/**
		 * See NOTE ON UNIQUENESS CHECKING in the comment at the top of the file
		 * for information about why we check for duplicates here
		 */
		Datum		values[INDEX_MAX_KEYS];
		bool		isnull[INDEX_MAX_KEYS];

		index_deform_tuple((IndexTuple) mkheap->p->ptr, mkheap->mkctxt->tupdesc, values, isnull);
		ereport(ERROR,
				(errcode(ERRCODE_UNIQUE_VIOLATION),
				 errmsg("could not create unique index \"%s\"",
						RelationGetRelationName(mkheap->mkctxt->indexRel)),
				 errdetail("Key %s is duplicated.",
						 BuildIndexValueDescription(mkheap->mkctxt->indexRel,
													values, isnull))));
	}

	if (mke_is_empty(e))
	{
		/*
		 * adding an empty (sentinel): just remove from count and fallthrough
		 * to where top is removed
		 */
		--mkheap->count;
	}
	else if (mke_get_run(e) != mke_get_run(mkheap->p))
	{
		/*
		 * this code assumes that the new one, with lower run, is LARGER than
		 * the top -- so it must be larger run
		 */
		Assert(mke_get_run(e) > mke_get_run(mkheap->p));

		/*
		 * when the runs differ it is because we attempted once with the runs
		 * equal. So if level is zero then:  the level was zero AND validly
		 * prepared for the previous run -- and there is no need to prep again
		 */
		if (mke_get_lv(e) != 0)
		{
			/* Not same run, at least prepare lv 0 */
			if (mkheap->mkctxt->fetchForPrep)
				tupsort_prepare(e, mkheap->mkctxt, 0);
			mke_set_lv(e, 0);
		}

		/*
		 * now fall through and let top be returned, new one is also inserted
		 * so no change to count
		 */
	}
	else
	{
		/* same run so figure out where it fits in relation to the heap top */
		int			lv = 0;

		toplv = mke_get_lv(mkheap->p);
		mke_set_lv(e, lv);

		/* populate level until we differ from the top element of the heap */
		while (lv < toplv)
		{
			if (mkheap->mkctxt->fetchForPrep)
				tupsort_prepare(e, mkheap->mkctxt, lv);
			c = mkheap_compare(mkheap, e, mkheap->lvtops + lv);
			if (c != 0)
				break;

			mke_set_lv(e, ++lv);
		}

		/* smaller than top */
		if (c < 0)
			return -1;

		/*
		 * we have not done e->lv == toplv yet since we increment at the end
		 * of the previous loop.  Do it now.
		 */
		Assert(mke_get_lv(e) == lv);
		if (lv == toplv)
		{
			if (mkheap->mkctxt->fetchForPrep)
				tupsort_prepare(e, mkheap->mkctxt, lv);
			c = mkheap_compare(mkheap, e, mkheap->p);
			if (c < 0)
				return -1;
		}

		if (c == 0)
		{
			/*
			 * Equal and at top level.
			 *
			 * This means that e is less-than/equal to all entries except the
			 * heap top.
			 */
			Assert(mke_get_lv(e) == lv);
			Assert(lv == mke_get_lv(mkheap->p));

			/*
			 * Expand more levels of lvtop in the current top and the new one
			 * until we detect a difference.
			 */
			while (lv < mkheap->mkctxt->total_lv - 1)
			{
				mkheap_save_lvtop(mkheap);

				++lv;

				/* expand top */
				if (mkheap->mkctxt->fetchForPrep)
					tupsort_prepare(mkheap->p, mkheap->mkctxt, lv);

				/* expand new element */
				if (mkheap->mkctxt->fetchForPrep)
					tupsort_prepare(e, mkheap->mkctxt, lv);

				mke_set_lv(mkheap->p, lv);
				mke_set_lv(e, lv);

				c = mkheap_compare(mkheap, e, mkheap->p);
				if (c != 0)
					break;
			}

			if (c <= 0)
			{
				/*
				 * if new one is less than current top then we just return
				 * that negative comparison
				 */

				/*
				 * if new one equals the current top then we could do an
				 * insert and immediate removal -- but it won't matter so we
				 * simply return right away, leaving *e untouched
				 */

				/* enforce uniqueness first */
				if (c == 0 && mkheap->mkctxt->enforceUnique)
				{
					Datum		values[INDEX_MAX_KEYS];
					bool		isnull[INDEX_MAX_KEYS];

					index_deform_tuple((IndexTuple) mkheap->p->ptr, mkheap->mkctxt->tupdesc, values, isnull);
					ereport(ERROR,
							(errcode(ERRCODE_UNIQUE_VIOLATION),
							 errmsg("could not create unique index \"%s\"",
						  RelationGetRelationName(mkheap->mkctxt->indexRel)),
							 errdetail("Key %s is duplicated.",
						 BuildIndexValueDescription(mkheap->mkctxt->indexRel,
													values, isnull))));
				}

				return c;
			}
		}
	}

	/*
	 * Now, I am bigger than top but not definitely smaller/equal to all other
	 * entries
	 *
	 * So we will: return top as *e do heap shuffling to restore heap ordering
	 */
	tmp = *e;
	*e = mkheap->p[0];

	/* Sift down a hole to bottom of (current or next) run, depends on tmp.run */
	mkheap_siftdown(mkheap, 0, &tmp);

	if (mkheap_need_heapify(mkheap))
		mkheap_heapify(mkheap, false);

	if (mkheap->count > 0)
	{
		mkheap_update_lvtops(mkheap);
	}

#ifdef USE_ASSERT_CHECKING
	if (gp_mk_sort_check)
		mkheap_verify_heap(mkheap, 0);
#endif

	return 1;
}

/*
 * Insert with run.  Runs provide a way for values SMALLER than
 *		the top element to be inserted -- they get pushed to
 *		the next run and then insert (and since 'run' will be part of
 *		their comparison then they will definitely be inserted).
 *
 *		If the entry is greater than the top, we can
 *		insert it using the same run.
 *
 *		If the entry is less than the heap top, we will
 *		put it to the next run, and insert it.
 *
 *		Both non-equal cases will ends up with the entry
 *		inserted, and return a postive number.
 *
 *		If the entry equal to the heaptop, we will NOT
 *		insert the element, but return 0.  Caller need to
 *		handle this.  In case of sort build runs, that means
 *		e can be written to current run immediately.
 *
 * Note: run must be passed in as equal to the run of the top element of the heap.
 *		 Perhaps change the function interface so this is not required (make the
 *		 function itself peek at the top run and return whether or not the top run
 *		 of the heap was increased by this action
 */
int
mkheap_putAndGet_run(MKHeap *mkheap, MKEntry *e, int16 run)
{
	int			ins;

	/* try insertion into specified run */
	Assert(!mke_is_empty(e));
	mke_set_run(e, run);
	ins = mkheap_putAndGet_impl(mkheap, e);

	/* success! Greater than or Equal. */
	if (ins >= 0)
	{
		return ins;
	}

	/* failed, put into next run */
	mke_set_run(e, run + 1);
	ins = mkheap_putAndGet_impl(mkheap, e);

	Assert(ins > 0);
	return ins;
}

int
mkheap_putAndGet_reader(MKHeap *mkheap, MKEntry *out)
{
	int			n;
	bool		fOK;
	int			ins;

	/* First, check to see if the heap is empty */
	MKEntry    *e = mkheap_peek(mkheap);

	if (!e)
	{
		mke_set_empty(out);
		return -1;
	}

	Assert(!mke_is_empty(e));

	/* _reader code is not compatible with the run code */
	Assert(mke_get_run(e) == 0);

	/* Figure out the reader that provided the current top of the heap */
	n = mke_get_reader(e);
	Assert(n >= 0 && n < mkheap->nreader);

	/* Read in a new one that will replenish the current top of the heap */
	fOK = mkheap->readers[n].reader(mkheap->readers[n].mkhr_ctxt, out);
	if (fOK)
	{
		mke_set_reader(out, n);
	}
	else
	{
		mke_set_empty(out);
	}
	Assert(mke_get_run(out) == 0);

	/*
	 * now insert it and pop the top of the heap or, if equal to the top, just
	 * don't do the insert
	 *
	 * So, on success (ins >= 0) then *out will refer to the element which is
	 * NOT in the heap (which could be either the result of mkheap_peek above
	 * OR the result of the .reader() call, depending)
	 */
	ins = mkheap_putAndGet_impl(mkheap, out);

	Assert(ins >= 0);
	Assert(mke_get_reader(out) == n);
	Assert(!mke_is_empty(out));

	return ins;
}

/*
 * Get the next value.  On success, a non-negative value is returned and *out is populated with the value
 *	 that was on the top of the heap.
 *
 * if this is an array-backed heap then *out is inserted into the heap.  If it's a reader-backed heap then
 *	  *out is ignored on input.
 */
int
mkheap_putAndGet(MKHeap *mkheap, MKEntry *out)
{
	int			ret = 0;

	Assert(out);

	/*
	 * fetch from appropriate source
	 *
	 * note that these two cases don't behave the same in terms of how *out is
	 * treated. mkheap_putAndGet_reader should be called mkheap_get_reader --
	 * it never puts the input value mkheap_putAndGet_impl will put *out if
	 * it's not empty, and then do the get.
	 */
	if (mkheap->nreader > 0)
		ret = mkheap_putAndGet_reader(mkheap, out);
	else
		ret = mkheap_putAndGet_impl(mkheap, out);

	/* check: underlying call must have enforced uniquness */
	AssertImply(mkheap->mkctxt->enforceUnique, ret != 0);

	/* free *out */
	if (mkheap->mkctxt->cpfr)
		(mkheap->mkctxt->cpfr) (out, NULL, mkheap->mkctxt->lvctxt + mke_get_lv(out));
	return ret;
}

/**
 * Return the top of the heap or NULL if the heap is empty
 */
MKEntry *
mkheap_peek(MKHeap *mkheap)
{
	if (mkheap->count == 0)
		return NULL;

	Assert(!mke_is_empty(mkheap->p));
	return mkheap->p;
}

#ifdef USE_ASSERT_CHECKING
void
mkheap_verify_heap(MKHeap *heap, int top)
{
	int			empty_cnt = 0;
	int			i;
	MKEntry		e;

	if (heap->count == 0)
		return;

	Assert(heap->count > 0);
	Assert(heap->maxentry > 0);
	Assert(heap->count <= heap->maxentry);
	Assert(heap->maxentry <= heap->alloc_size);

	e = heap->p[0];
	mke_set_lv(&e, 0);
	mke_clear_refc_copied(&e);

	/* Checking for lvtops */
	for (i = 0; i < mke_get_lv(heap->p); ++i)
	{
		int			c;

		/* Too much trouble dealing with ref counters.  Just don't */

		/*
		 * if(!mke_test_flag(heap->lvtops+i, MKE_RefCnt | MKE_Copied)) {
		 */
		mke_set_lv(&e, i);
		if (heap->mkctxt->fetchForPrep)
			tupsort_prepare(&e, heap->mkctxt, i);

		c = mkheap_compare(heap, &e, heap->lvtops + i);
		Assert(c == 0);

		/*
		 * }
		 */
	}

	/* Verify Heap property */
	for (i = top; i < heap->maxentry; ++i)
	{
		int			left = LEFT(i);
		int			right = RIGHT(i);
		int			cl,
					cr;

		if (mke_is_empty(heap->p + i))
			++empty_cnt;

		if (left >= heap->maxentry)
			continue;

		cl = mkheap_compare(heap, heap->p + i, heap->p + left);
		Assert(cl <= 0);
		if (i == 0 && cl == 0)
			Assert(mke_get_lv(heap->p) == heap->mkctxt->total_lv - 1);

		if (right >= heap->maxentry)
			continue;

		cr = mkheap_compare(heap, heap->p + i, heap->p + right);
		Assert(cr <= 0);
		if (i == 0 && cr == 0)
			Assert(mke_get_lv(heap->p) == heap->mkctxt->total_lv - 1);
	}
}

#endif
