/*
 * Mksort (multiple-key sort) is an alternative of standard qsort algorithm,
 * which has better performance for particular sort scenarios, i.e. the
 * data set has multiple keys to be sorted.
 *
 * The sorting algorithm blends Quicksort and radix sort; Like regular
 * Quicksort, it partitions its input into sets less than and greater than a
 * given value; like radix sort, it moves on to the next field once the current
 * input is known to be equal in the given field.
 *
 * The implementation is based on the paper:
 *   Jon L. Bentley and Robert Sedgewick, "Fast Algorithms for Sorting and
 *   Searching Strings", Jan 1997
 *
 * Some improvements which is related to additional handling for equal tuples
 * have been adapted to keep consistency with the implementations of postgres
 * qsort and mksort on GPDB6.
 *
 * To be consistent with qsort_tuple.c, this file is included by tuplesort.c,
 * rather than compiled separately.
 *
 * For now, mksort_tuple() is called in tuplesort_sort_memtuples() as a
 * replacement of qsort_tuple() when specific conditions are satisfied.
 */

/* Swap two tuples in sort tuple array */
static inline void
mksort_swap(int a,
			int b,
			SortTuple *x)
{
	if (a == b)
		return;
	SortTuple t = x[a];
	x[a] = x[b];
	x[b] = t;
}

/* Swap tuples by batch in sort tuple array */
static inline void
mksort_vec_swap(int a,
				int b,
				int size,
				SortTuple *x)
{
	while (size-- > 0)
	{
		mksort_swap(a, b, x);
		a++;
		b++;
	}
}

/*
 * Check whether current datum (at specified tuple and depth) is null
 * Note that the input x means a specified tuple provided by caller but not
 * a tuple array, so tupleIndex is unnecessary
 */
static inline bool
check_datum_null(SortTuple *x, int depth, Tuplesortstate *state)
{
	Datum datum;
	bool isNull;

	/* Since we have a specified tuple, the tupleIndex is always 0 */
	state->mksortGetDatumFunc(x, 0, depth, state, &datum, &isNull);
	return isNull;
}

#ifdef MKSORT_VERIFY
/*
 * Verify whether the SortTuple list is ordered or not on a specified depth
 */
static void
mksort_verify(SortTuple		  *x,
			  int			   n,
			  int			   depth,
			  Tuplesortstate  *state)
{
	Datum datum1, datum2;
	bool isNull1, isNull2;
	int32 res;
	SortSupport sortKey;

	Assert(state->mksortGetDatumFunc);

	/* Get the sort key for current depth */
	sortKey = state->sortKeys + depth;

	for (int i = 0;i < n - 1;i++)
	{
		state->mksortGetDatumFunc(x, i, depth, state, &datum1, &isNull1);
		state->mksortGetDatumFunc(x, i + 1, depth, state, &datum2, &isNull2);
		res = ApplySortComparator(datum1,
								  isNull1,
								  datum2,
								  isNull2,
								  sortKey);
		Assert(res <= 0);
	}
}
#endif

/*
 * Major of multi-key sort
 *
 * seenNull indicates whether we have seen NULL in any datum we checked
 */
static void
mksort_tuple(SortTuple           *x,
			 size_t               n,
			 int                  depth,
			 SortTupleComparator  cmp_tuple,
			 Tuplesortstate      *state,
			 bool				  seenNull)
{
	/*
	 * In the process, the tuple array consists of five parts:
	 * left equal, less, not-processed, greater, right equal
	 *
	 * lessStart indicates the first position of less part
	 * lessEnd indicates the next position after less part
	 * greaterStart indicates the prior position before greater part
	 * greaterEnd indicates the latest position of greater part
	 * the range between lessEnd and greaterStart (inclusive) is not-processed
	 */
	int lessStart, lessEnd, greaterStart, greaterEnd, tupCount;
	int32 dist;
	Datum pivot;
	bool isPivotNull;
	SortSupport sortKey;
	bool isDatumNull;

	Assert(depth <= state->nKeys);
	Assert(state->sortKeys);
	Assert(state->mksortGetDatumFunc);

	if (n <= 1)
		return;

	/* If we have exceeded the max depth, return immediately */
	if (depth == state->nKeys)
		return;

	CHECK_FOR_INTERRUPTS();

	if (QueryFinishPending)
		return;

	/* Get the sort key for current depth */
	sortKey = state->sortKeys + depth;

	/* Select pivot by random and move it to the first position */
	lessStart = ((int64)((float8) random() / (float8) MAX_RANDOM_VALUE)) % n;
	mksort_swap(0, lessStart, x);
	state->mksortGetDatumFunc(x, 0, depth, state, &pivot, &isPivotNull);

	lessStart = 1;
	lessEnd = 1;
	greaterStart = n - 1;
	greaterEnd = n - 1;

	/* Sort the array to three parts: lesser, equal, greater */
	while (true)
	{
		Datum datum;
		bool isDatumNull;

		CHECK_FOR_INTERRUPTS();

		/* Compare the left end of the array */
		while (lessEnd <= greaterStart)
		{
			/* Get the datum at lessEnd */
			state->mksortGetDatumFunc(x, lessEnd, depth, state, &datum, &isDatumNull);

			dist = ApplySortComparator(datum,
									   isDatumNull,
									   pivot,
									   isPivotNull,
									   sortKey);

			if (dist > 0)
				break;

			/* If the datum is equal to pivot, move it to lessStart */
			if (dist == 0)
			{
				mksort_swap(lessEnd, lessStart, x);
				lessStart++;
			}
			lessEnd++;
		}

		/* Compare the right end of the array */
		while (lessEnd <= greaterStart)
		{
			/* Get the datum at greaterStart */
			state->mksortGetDatumFunc(x,
									  greaterStart,
									  depth,
									  state,
									  &datum,
									  &isDatumNull);

			dist = ApplySortComparator(datum,
									   isDatumNull,
									   pivot,
									   isPivotNull,
									   sortKey);

			if (dist < 0)
				break;

			/* If the datum is equal to pivot, move it to greaterEnd */
			if (dist == 0)
			{
				mksort_swap(greaterStart, greaterEnd, x);
				greaterEnd--;
			}
			greaterStart--;
		}

		if (lessEnd > greaterStart)
			break;
		mksort_swap(lessEnd, greaterStart, x);
		lessEnd++;
		greaterStart--;
	}

	/*
	 * Now the array has four parts:
	 *   left equal, lesser, greater, right equal
	 * Note greaterStart is less than lessEnd now
	 */

	/* Move the left equal part to median */
	dist = Min(lessStart, lessEnd - lessStart);
	mksort_vec_swap(0, lessEnd - dist, dist, x);

	/* Move the right equal part to median */
	dist = Min(greaterEnd - greaterStart, n - greaterEnd - 1);
	mksort_vec_swap(lessEnd, n - dist, dist, x);

	/*
	 * Now the array has three parts:
	 *   lesser, equal, greater
	 * Note that one or two parts may have no element at all.
	 */

	/* Recursively sort the lesser part */

	/* dist means the size of less part */
	dist = lessEnd - lessStart;
	mksort_tuple(x,
				 dist,
				 depth,
				 cmp_tuple,
				 state,
				 seenNull);

	/* Recursively sort the equal part */

	/*
	 * (x + dist) means the first tuple in the equal part
	 * Since all tuples have equal datums at current depth, we just check any one
	 * of them to determine whether we have seen null datum.
	 */
	isDatumNull = check_datum_null(x + dist, depth, state);

	/* (lessStart + n - greaterEnd - 1) means the size of equal part */
	tupCount = lessStart + n - greaterEnd - 1;

	if (depth < state->nKeys - 1)
	{
		mksort_tuple(x + dist,
					 tupCount,
					 depth + 1,
					 cmp_tuple,
					 state,
					 seenNull || isDatumNull);
	} else {
		/*
		 * We have reach the max depth: Call mksortHandleDupFunc to handle duplicated
		 * tuples if necessary, e.g. checking uniqueness or extra comparing
		 */

		/* If enforceUnique is enabled, there must be mksortHandleDupFunc */
		AssertImply(state->enforceUnique, state->mksortHandleDupFunc);

		/*
		 * Call mksortHandleDupFunc if:
		 *   1. mksortHandleDupFunc is filled
		 *   2. the size of equal part > 1
		 */
		if (state->mksortHandleDupFunc &&
			(tupCount > 1))
		{
			state->mksortHandleDupFunc(x + dist,
									   tupCount,
									   seenNull || isDatumNull,
									   state);
		}
	}

	/* Recursively sort the greater part */

	/* dist means the size of greater part */
	dist = greaterEnd - greaterStart;
	mksort_tuple(x + n - dist,
				 dist,
				 depth,
				 cmp_tuple,
				 state,
				 seenNull);

#ifdef MKSORT_VERIFY
	mksort_verify(x,
				  n,
				  depth,
				  state);
#endif
}
