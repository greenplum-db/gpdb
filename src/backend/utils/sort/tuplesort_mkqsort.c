/*-------------------------------------------------------------------------
 *
 * tuplesort_mkqsort.c
 *	  Multi level key quick sort.
 *
 * A multi level key Bentley McIlroy 3-way partitioning quick sort.
 * 	See [1] J. Bentley, M. McIlroy.  Engineering a sort function,
 * 		Software Practice and Experience, Vol 23(11) Nov, 1993.
 * 	    [2] R. Sedgewick, J. Bentley. Quicksort is optimal.
 *
 * Portions Copyright (c) Greenplum Inc, 2008.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/sort/tuplesort_mkqsort.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "access/genam.h"
#include "utils/tuplesort.h"
#include "utils/tuplesort_mk.h"
#include "utils/tuplesort_mk_details.h"

#include "miscadmin.h"

#ifdef MKQSORT_VERIFY 
extern void mkqsort_verify(MKEntry *a, int l, int r, MKContext *mkctxt);
#endif

/**
 * Given an array, swap the entries at a[i] and a[j]
 */
static inline void mkqs_swap(MKEntry *a, int i, int j)
{
	if(i!=j)
	{
		MKEntry tmp = a[i];
		a[i] = a[j];
		a[j] = tmp;
	}
}

/**
 * Compare the two entries, only does comparison using compFlags and the level in the context
 */
static inline int32 mkqs_comp(MKEntry *a, MKEntry *b, MKLvContext *ctxt, MKContext *mkctxt)
{
	int ret = a->compflags - b->compflags;

	if (ret == 0 && !mke_is_null(a))
		ret = tupsort_compare_datum(a, b, ctxt, mkctxt);

	return ret;
}

/**
 *  Return the median of three multi-key entries
 */
static inline MKEntry *mkqs_med3(MKEntry *a, MKEntry *b, MKEntry *c, MKLvContext *ctxt, MKContext *mkctxt)
{
	return mkqs_comp(a, b, ctxt, mkctxt) < 0 ?
                        (mkqs_comp(b, c, ctxt, mkctxt) < 0 ? b : mkqs_comp(a, c, ctxt, mkctxt) < 0 ? c : a )
                        :
                        (mkqs_comp(b, c, ctxt, mkctxt) > 0 ? b : mkqs_comp(a, c, ctxt, mkctxt) > 0 ? c : a )
                        ;
}

/**
 * Choose a pivot from the array of elements (a) from between left (inclusive) and right (inclusive).
 *
 * lv/comp/ctxt combine to allow comparison.  Note that, as usual with the multi-key structure, comparison is only done
 *   up to a certain level.
 */
static inline MKEntry *mkqs_choose_pivot(MKEntry *a, int left, int right, int lv, MKLvContext *ctxt, MKContext *mkctxt)
{
	int s;
	int n = right - left + 1;
	MKEntry *pl, *pm, *pn;

	Assert(n >= 0);
	
	pm = a + (left + n/2);

	if(n > 7)
	{
		pl = a+left;
		pn = a+right;

		if(n > 40)
		{
			s = n/8;
			pl = mkqs_med3(pl, pl+s, pl+s*2, ctxt, mkctxt);
			pm = mkqs_med3(pm-s, pm, pm+s, ctxt, mkctxt);
			pn = mkqs_med3(pn-s*2, pn-s, pn, ctxt, mkctxt);
		}

		pm = mkqs_med3(pl, pm, pn, ctxt, mkctxt);
	}

	return pm;
}

/*
 * Three way partition quick sort.
 *
 * left and right are both INCLUSIVE
 */
static void mk_qsort_part3(MKEntry *a, int left, int right, int lv, MKContext *mkqs_ctxt, int *lastInLowOut, int *firstInHighOut)
{
	/* p, q points to the last equals we put to both end */
	int p = left-1;
	int q = right+1;

	int leftIndex,rightIndex;

	int k;
    MKLvContext *lvctxt = mkqs_ctxt->lvctxt + lv;

	MKEntry v = {0, };
	MKEntry *pv;

	Assert(left < right);

	/*
	 * leftIndex points to the known <= upper,
	 * rightIndex points to the known >= lower.
	 * 
	 * Initially, we know nothing, so point them to out of bounds.
	 */
	leftIndex = left-1;
	rightIndex = right+1;

	/* Choose a pivot */
	pv = mkqs_choose_pivot(a, left, right, lv, lvctxt, mkqs_ctxt);

	/* Save pivot in v */ 
	/* Hacking a ref count */
	if(mke_is_refc(pv))
		mkqs_ctxt->cpfr(&v, pv, lvctxt);
	else 
		v = *pv;

	while(1)
	{
		int tmpLeftIndex, cmp;

		cmp = -1;
		while(1)
		{
			/* increase leftIndex until a[leftIndex] >= pivot */
			while(++leftIndex < rightIndex &&
				 (cmp = mkqs_comp(a+leftIndex, &v, lvctxt, mkqs_ctxt)) < 0)
			{
				Assert(leftIndex >= left && leftIndex<= right);
				Assert(rightIndex >= left && rightIndex<= right+1);
				Assert(leftIndex <= rightIndex);
			}

			/* now i refers to the first value that is >= the pivot */
			if(leftIndex == rightIndex || cmp > 0)
				break;

			/* since leftIndex equals the pivot, put the equals one at the beginning and move one there to take its place. */
			mkqs_swap(a, ++p, leftIndex);
		}

		if(leftIndex == rightIndex)
		{ 
			/**
			 * Caught up, back down and done loop
			 */
			--leftIndex;
			break; 
		}

		/**
		 *  now, p points to the last value of a run of values equal to the pivot
		 *  leftIndex points one beyond a run of values <= the pivot (and between p+1 and leftIndex they are < the pivot)
		 */

		/* tmpLeftIndex points at the last of those <= pivot */
		tmpLeftIndex = leftIndex-1;

		/* same case as above, but from the upper end and transferring equal values to the q end */
		cmp = 1;
		while(1)
		{
			while(--rightIndex > tmpLeftIndex &&
					(cmp = mkqs_comp(&v, a+rightIndex, lvctxt, mkqs_ctxt)) < 0)
			{
				Assert(tmpLeftIndex >= left-1 && tmpLeftIndex <= right);
				Assert(rightIndex >= left && rightIndex<= right);
				Assert(tmpLeftIndex <= rightIndex);
			}

			if(tmpLeftIndex == rightIndex || cmp > 0)
				break;

			mkqs_swap(a, rightIndex, --q);
		}

		if(tmpLeftIndex == rightIndex)
		{
			--leftIndex; /* make leftIndex = tmpLeftIndex */
			++rightIndex; /* move rightIndex up to avoid overlap */
			break;
		}

		/* at this point, a[leftIndex] > pivot and a[rightIndex] < pivot, so swap them */
		/* note: there is an extra compare between leftIndex and pivot, and rightIndex and pivot because
		 * we don't alter leftIndex and rightIndex here.  But doing so may complicate other code */
		Assert(leftIndex < rightIndex);
		mkqs_swap(a, leftIndex, rightIndex);

		if(leftIndex+1 == rightIndex)
			break;
	}

	/* Now leftIndex points to where the run of values <= the pivot ends
     * And rightIndex points to where the run of values >= the pivot begins
     */
	Assert(leftIndex+1 == rightIndex);
	Assert(rightIndex>=left && leftIndex<=right);

	/**
	 * Push all the equal values at the beginning to the middle (at i)
	 */
	for(k=left; k<=p; ++k, --leftIndex)
	{
		Assert(k>=left && k<=right);
		Assert(leftIndex>=left && leftIndex<=right);
		mkqs_swap(a, k, leftIndex);
	}

	/**
	 * Push all the equal values at the end to the middle (at j)
	 */
	for(k=right; k>=q; --k, ++rightIndex)
	{
		Assert(k>=left && k<=right);
		Assert(rightIndex>=left && rightIndex<=right);
		mkqs_swap(a, rightIndex, k);
	}

	/*
	 * Now leftIndex points to the last value of the run of values < the pivot
	 * And rightIndex points to the last value of the run of values > the pivot
	 */
	Assert(leftIndex >= left-1 && leftIndex < right);
	Assert(rightIndex > left && rightIndex <= right+1);
	Assert(rightIndex >= leftIndex+2);

	/* Hack ref count */
	if(mke_is_refc(&v))
		mkqs_ctxt->cpfr(&v, NULL, lvctxt);

	*lastInLowOut = leftIndex;
	*firstInHighOut = rightIndex;
}

void mk_qsort_impl(MKEntry *a, int left, int right, int lv, bool lvdown, MKContext *ctxt, bool seenNull)
{
	int lastInLow;
	int firstInHigh;

	Assert(ctxt);
	Assert(lv < ctxt->total_lv);

	CHECK_FOR_INTERRUPTS();

	if (QueryFinishPending)
		return;

	if(right <= left)
		return;
	
	/* Prepare at level lv */
	if(lvdown)
        mk_prepare_array(a, left, right, lv, ctxt);

	/* 
	 * According to Bentley & McIlroy [1] (1993), using insert sort for case 
	 * n < 7 is a significant saving.  However, according to Sedgewick & 
	 * Bentley [2] (2002), the wisdom of new millenium is not to special case
	 * smaller cases.  Here, we do not special case it because we want to save
	 * memtuple_getattr, and expensive comparisons that has been prepared.
	 *
	 * XXX Find out why we have a new wisdom in [2] and impl. & compare.
	 */
	mk_qsort_part3(a, left, right, lv, ctxt, &lastInLow, &firstInHigh);

	/* recurse to left chunk */
	mk_qsort_impl(a, left, lastInLow, lv, false, ctxt, seenNull);

	/* recurse to middle (equal) chunk */
	if(lv < ctxt->total_lv-1)
	{
		/*
		 * [lastInLow+1,firstInHigh-1] defines the pivot region which was all equal at level lv.  So increase the level and compare that region!
		 */
		mk_qsort_impl(a, lastInLow+1, firstInHigh-1, lv+1, true, ctxt, seenNull || mke_is_null(a+lastInLow+1)); /* a + lastInLow + 1 points to the pivot */
	}
	else
	{
		/* values are all equal to the deepest level...no need for more compares, but check uniqueness if requested */
		if(firstInHigh-1 > lastInLow+1 &&
				!seenNull &&
				!mke_is_null(a+lastInLow+1)) /* a + lastInLow + 1 points to the pivot */
		{
			if ( ctxt->enforceUnique )
			{
				Datum	values[INDEX_MAX_KEYS];
				bool	isnull[INDEX_MAX_KEYS];
		
				index_deform_tuple((IndexTuple)(a+lastInLow+1)->ptr, ctxt->tupdesc, values, isnull);
				ereport(ERROR,
						(errcode(ERRCODE_UNIQUE_VIOLATION),
						 errmsg("could not create unique index \"%s\"",
								RelationGetRelationName(ctxt->indexRel)),
						 errdetail("Key %s is duplicated.",
								   BuildIndexValueDescription(ctxt->indexRel,
															  values, isnull))));
			}
			else if ( ctxt->unique)
			{
				int toFreeIndex;
				for ( toFreeIndex = lastInLow + 2; toFreeIndex < firstInHigh; toFreeIndex++) /* +2 because we want to keep one around! */
				{
					MKEntry *toFree = a + toFreeIndex;
					if ( ctxt->cpfr)
						ctxt->cpfr(toFree, NULL, ctxt->lvctxt + lv); // todo: verify off-by-one
					ctxt->freeTup(toFree);
					mke_set_empty(toFree);
				}
			}
		}
	}

	/* recurse to right chunk */
	mk_qsort_impl(a, firstInHigh, right, lv, false, ctxt, seenNull);

#ifdef MKQSORT_VERIFY 
	if(lv == 0)
		mkqsort_verify(a, left, right, ctxt);
#endif
}

#ifdef MKQSORT_VERIFY 
static int mkqsort_comp_entry_all_lv(MKEntry *a, MKEntry *b, MKContext *mkctxt)
{
	int i;
	int c;

	MKEntry aa = {0, }; 
	MKEntry bb = {0, }; 

	(*mkctxt->cpfr)(&aa, a, mk_get_ctxt(mkctxt, mke_get_lv(a)));
	(*mkctxt->cpfr)(&bb, b, mk_get_ctxt(mkctxt, mke_get_lv(b)));

	for(i=0; i<mkctxt->total_lv; ++i)
	{
		MKPrepare prep = mkctxt->prep[i];

		void *ctxt = mk_get_ctxt(mkctxt, i);
        int32 nf1, nf2;

		if(prep)
		{
			(prep)(&aa, ctxt);
			(prep)(&bb, ctxt);
		}

        nf1 = mke_get_nullbits(&aa);
        nf2 = mke_get_nullbits(&bb);

        c = nf1 - nf2;
        if (c!=0)
            return c;

        if (!mke_is_null(&aa))
        {
            Assert(!mke_is_null(&bb));
            c = (comp)(&aa, &bb, ctxt);

            if (c!=0)
                return c;
        }
	}

	return 0;
}

void mkqsort_verify(MKEntry *a, int i, int j, MKContext *mkctxt)
{
	int c;
	for(; i<j; ++i)
	{
		c = mkqsort_comp_entry_all_lv(a+i, a+i+1, mkctxt);
		Assert(c <= 0);
	}
}
#endif
