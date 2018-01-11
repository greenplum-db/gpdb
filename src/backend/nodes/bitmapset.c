/*-------------------------------------------------------------------------
 *
 * bitmapset.c
 *	  PostgreSQL generic bitmap set package
 *
 * A bitmap set can represent any set of nonnegative integers, although
 * it is mainly intended for sets where the maximum value is not large,
 * say at most a few hundred.  By convention, a NULL pointer is always
 * accepted by all operations to represent the empty set.  (But beware
 * that this is not the only representation of the empty set.  Use
 * bms_is_empty() in preference to testing for NULL.)
 *
 *
 * Copyright (c) 2003-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/nodes/bitmapset.c,v 1.15 2009/01/01 17:23:43 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/bitmapset.h"
#include "access/hash.h"


#define WORDNUM(x)	((int)((unsigned)(x) >> BITS_PER_BITMAPWORD_LOG2))
#define BITNUM(x)	((int)((unsigned)(x) & (BITS_PER_BITMAPWORD - 1)))

#define BITMAPSET_SIZE(nwords)	\
	(offsetof(Bitmapset, words) + (nwords) * sizeof(bitmapword))

/*----------
 * This is a well-known cute trick for isolating the rightmost one-bit
 * in a word.  It assumes two's complement arithmetic.  Consider any
 * nonzero value, and focus attention on the rightmost one.  The value is
 * then something like
 *				xxxxxx10000
 * where x's are unspecified bits.  The two's complement negative is formed
 * by inverting all the bits and adding one.  Inversion gives
 *				yyyyyy01111
 * where each y is the inverse of the corresponding x.	Incrementing gives
 *				yyyyyy10000
 * and then ANDing with the original value gives
 *				00000010000
 * This works for all cases except original value = zero, where of course
 * we get zero.
 *----------
 */
#define RIGHTMOST_ONE(x) ((signedbitmapword) (x) & -((signedbitmapword) (x)))

#define HAS_MULTIPLE_ONES(x)	((bitmapword) RIGHTMOST_ONE(x) != (x))


/*
 * Lookup tables to avoid need for bit-by-bit groveling
 *
 * number_of_ones[x] gives the number of one-bits (0-8) in a byte value x.
 *
 * We could make these tables larger and reduce the number of iterations
 * in the functions that use them, but bytewise shifts and masks are
 * especially fast on many machines, so working a byte at a time seems best.
 */

static const uint8 number_of_ones[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};


/*
 * num_low_order_zero_bits
 */
static inline int
num_low_order_zero_bits(bitmapword w)
{
	int			x = 0;
	int			i;
	bitmapword	m;

	for (i = BITS_PER_BITMAPWORD / 2; i; i >>= 1)
	{							/* i = 16, 8, 4, 2, 1 */
		m = (1 << i) - 1;		/* m = 0xffff, 0xff, 0xf, 3, 1 */
		if ((w & m) == 0)
		{
			w >>= i;
			x += i;
		}
	}
	if (w == 0)					/* all 0 => return BITS_PER_BITMAPWORD */
		x++;
	return x;
}


/*
 * bms_copy - make a palloc'd copy of a bitmapset
 */
Bitmapset *
bms_copy(const Bitmapset *a)
{
	Bitmapset  *result;
	size_t		size;

	if (a == NULL)
		return NULL;
	size = BITMAPSET_SIZE(a->nwords);
	result = (Bitmapset *) palloc(size);
	memcpy(result, a, size);
	return result;
}

/*
 * bms_equal - are two bitmapsets equal?
 *
 * This is logical not physical equality; in particular, a NULL pointer will
 * be reported as equal to a palloc'd value containing no members.
 */
bool
bms_equal(const Bitmapset *a, const Bitmapset *b)
{
	const Bitmapset *shorter;
	const Bitmapset *longer;
	int			shortlen;
	int			longlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
	{
		if (b == NULL)
			return true;
		return bms_is_empty(b);
	}
	else if (b == NULL)
		return bms_is_empty(a);
	/* Identify shorter and longer input */
	if (a->nwords <= b->nwords)
	{
		shorter = a;
		longer = b;
	}
	else
	{
		shorter = b;
		longer = a;
	}
	/* And process */
	shortlen = shorter->nwords;
	for (i = 0; i < shortlen; i++)
	{
		if (shorter->words[i] != longer->words[i])
			return false;
	}
	longlen = longer->nwords;
	for (; i < longlen; i++)
	{
		if (longer->words[i] != 0)
			return false;
	}
	return true;
}


/*
 * bms_compare -- for sort to find groups of equal bitmapsets.
 *
 * See bms_equal for note on logical vs physical.  Note that the
 * arguments are pointers to Bitmapset -- a variable length structure,
 * thus this function is not directly usable by, e.g, qsort.
 */
int
bms_compare(const Bitmapset *a, const Bitmapset *b)
{
	const Bitmapset *longer;
	int			shortlen;
	int			longlen;
	int			sign;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
	{
		if (b == NULL || bms_is_empty(b))
			return 0;
		return -1;
	}
	else if (b == NULL)
	{
		if (bms_is_empty(a))
			return 0;
		else
			return 1;
	}

	/* Identify shorter and longer input */
	if (a->nwords <= b->nwords)
	{
		shortlen = a->nwords;
		longlen = b->nwords;
		longer = b;
		sign = -1;
	}
	else
	{
		shortlen = b->nwords;
		longlen = a->nwords;
		longer = a;
		sign = 1;
	}
	/* And process */
	for (i = 0; i < shortlen; i++)
	{
		if (a->words[i] < b->words[i])
			return -1;
		else if (a->words[i] > b->words[i])
			return 1;
	}
	for (; i < longlen; i++)
	{
		if (longer->words[i] != 0)
			return sign;
	}
	return 0;
}

/*
 * bms_make_singleton - build a bitmapset containing a single member
 */
Bitmapset *
bms_make_singleton(int x)
{
	Bitmapset  *result;
	int			wordnum,
				bitnum;

	if (x < 0)
		elog(ERROR, "negative bitmapset member not allowed");
	wordnum = WORDNUM(x);
	bitnum = BITNUM(x);
	result = (Bitmapset *) palloc0(BITMAPSET_SIZE(wordnum + 1));
	result->nwords = wordnum + 1;
	result->words[wordnum] = ((bitmapword) 1 << bitnum);
	return result;
}

/*
 * bms_free - free a bitmapset
 *
 * Same as pfree except for allowing NULL input
 */
void
bms_free(Bitmapset *a)
{
	if (a)
		pfree(a);
}


/*
 * These operations all make a freshly palloc'd result,
 * leaving their inputs untouched
 */


/*
 * bms_union - set union
 */
Bitmapset *
bms_union(const Bitmapset *a, const Bitmapset *b)
{
	Bitmapset  *result;
	const Bitmapset *other;
	int			otherlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return bms_copy(b);
	if (b == NULL)
		return bms_copy(a);
	/* Identify shorter and longer input; copy the longer one */
	if (a->nwords <= b->nwords)
	{
		result = bms_copy(b);
		other = a;
	}
	else
	{
		result = bms_copy(a);
		other = b;
	}
	/* And union the shorter input into the result */
	otherlen = other->nwords;
	for (i = 0; i < otherlen; i++)
		result->words[i] |= other->words[i];
	return result;
}

/*
 * bms_intersect - set intersection
 */
Bitmapset *
bms_intersect(const Bitmapset *a, const Bitmapset *b)
{
	Bitmapset  *result;
	const Bitmapset *other;
	int			resultlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL || b == NULL)
		return NULL;
	/* Identify shorter and longer input; copy the shorter one */
	if (a->nwords <= b->nwords)
	{
		result = bms_copy(a);
		other = b;
	}
	else
	{
		result = bms_copy(b);
		other = a;
	}
	/* And intersect the longer input with the result */
	resultlen = result->nwords;
	for (i = 0; i < resultlen; i++)
		result->words[i] &= other->words[i];
	return result;
}

/*
 * bms_difference - set difference (ie, A without members of B)
 */
Bitmapset *
bms_difference(const Bitmapset *a, const Bitmapset *b)
{
	Bitmapset  *result;
	int			shortlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return NULL;
	if (b == NULL)
		return bms_copy(a);
	/* Copy the left input */
	result = bms_copy(a);
	/* And remove b's bits from result */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
		result->words[i] &= ~b->words[i];
	return result;
}

/*
 * bms_is_subset - is A a subset of B?
 */
bool
bms_is_subset(const Bitmapset *a, const Bitmapset *b)
{
	int			shortlen;
	int			longlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return true;			/* empty set is a subset of anything */
	if (b == NULL)
		return bms_is_empty(a);
	/* Check common words */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
	{
		if ((a->words[i] & ~b->words[i]) != 0)
			return false;
	}
	/* Check extra words */
	if (a->nwords > b->nwords)
	{
		longlen = a->nwords;
		for (; i < longlen; i++)
		{
			if (a->words[i] != 0)
				return false;
		}
	}
	return true;
}

/*
 * bms_is_member - is X a member of A?
 */
bool
bms_is_member(int x, const Bitmapset *a)
{
	int			wordnum,
				bitnum;

	/* XXX better to just return false for x<0 ? */
	if (x < 0)
		elog(ERROR, "negative bitmapset member not allowed");
	if (a == NULL)
		return false;
	wordnum = WORDNUM(x);
	bitnum = BITNUM(x);
	if (wordnum >= a->nwords)
		return false;
	if ((a->words[wordnum] & ((bitmapword) 1 << bitnum)) != 0)
		return true;
	return false;
}

/*
 * bms_overlap - do sets overlap (ie, have a nonempty intersection)?
 */
bool
bms_overlap(const Bitmapset *a, const Bitmapset *b)
{
	int			shortlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL || b == NULL)
		return false;
	/* Check words in common */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
	{
		if ((a->words[i] & b->words[i]) != 0)
			return true;
	}
	return false;
}

/*
 * bms_nonempty_difference - do sets have a nonempty difference?
 */
bool
bms_nonempty_difference(const Bitmapset *a, const Bitmapset *b)
{
	int			shortlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return false;
	if (b == NULL)
		return !bms_is_empty(a);
	/* Check words in common */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
	{
		if ((a->words[i] & ~b->words[i]) != 0)
			return true;
	}
	/* Check extra words in a */
	for (; i < a->nwords; i++)
	{
		if (a->words[i] != 0)
			return true;
	}
	return false;
}

/*
 * bms_singleton_member - return the sole integer member of set
 *
 * Raises error if |a| is not 1.
 */
int
bms_singleton_member(const Bitmapset *a)
{
	int			result = -1;
	int			nwords;
	int			wordnum;

	if (a == NULL)
		elog(ERROR, "bitmapset is empty");
	nwords = a->nwords;
	for (wordnum = 0; wordnum < nwords; wordnum++)
	{
		bitmapword	w = a->words[wordnum];

		if (w != 0)
		{
			if (result >= 0 || HAS_MULTIPLE_ONES(w))
				elog(ERROR, "bitmapset has multiple members");
			result = num_low_order_zero_bits(w) + wordnum * BITS_PER_BITMAPWORD;
		}
	}
	if (result < 0)
		elog(ERROR, "bitmapset is empty");
	return result;
}

/*
 * bms_num_members - count members of set
 */
int
bms_num_members(const Bitmapset *a)
{
	int			result = 0;
	int			nwords;
	int			wordnum;

	if (a == NULL)
		return 0;
	nwords = a->nwords;
	for (wordnum = 0; wordnum < nwords; wordnum++)
	{
		bitmapword	w = a->words[wordnum];

		/* we assume here that bitmapword is an unsigned type */
		while (w != 0)
		{
			result += number_of_ones[w & 255];
			w >>= 8;
		}
	}
	return result;
}

/*
 * bms_membership - does a set have zero, one, or multiple members?
 *
 * This is faster than making an exact count with bms_num_members().
 */
BMS_Membership
bms_membership(const Bitmapset *a)
{
	BMS_Membership result = BMS_EMPTY_SET;
	int			nwords;
	int			wordnum;

	if (a == NULL)
		return BMS_EMPTY_SET;
	nwords = a->nwords;
	for (wordnum = 0; wordnum < nwords; wordnum++)
	{
		bitmapword	w = a->words[wordnum];

		if (w != 0)
		{
			if (result != BMS_EMPTY_SET || HAS_MULTIPLE_ONES(w))
				return BMS_MULTIPLE;
			result = BMS_SINGLETON;
		}
	}
	return result;
}

/*
 * bms_is_empty - is a set empty?
 *
 * This is even faster than bms_membership().
 */
bool
bms_is_empty(const Bitmapset *a)
{
	int			nwords;
	int			wordnum;

	if (a == NULL)
		return true;
	nwords = a->nwords;
	for (wordnum = 0; wordnum < nwords; wordnum++)
	{
		bitmapword	w = a->words[wordnum];

		if (w != 0)
			return false;
	}
	return true;
}


/*
 * These operations all "recycle" their non-const inputs, ie, either
 * return the modified input or pfree it if it can't hold the result.
 *
 * These should generally be used in the style
 *
 *		foo = bms_add_member(foo, x);
 */


/*
 * bms_add_member - add a specified member to set
 *
 * Input set is modified or recycled!
 */
Bitmapset *
bms_add_member(Bitmapset *a, int x)
{
	int			wordnum,
				bitnum;

	if (x < 0)
		elog(ERROR, "negative bitmapset member not allowed");
	if (a == NULL)
		return bms_make_singleton(x);
	wordnum = WORDNUM(x);
	bitnum = BITNUM(x);
	if (wordnum >= a->nwords)
	{
		/* Slow path: make a larger set and union the input set into it */
		Bitmapset  *result;
		int			nwords;
		int			i;

		result = bms_make_singleton(x);
		nwords = a->nwords;
		for (i = 0; i < nwords; i++)
			result->words[i] |= a->words[i];
		pfree(a);
		return result;
	}
	/* Fast path: x fits in existing set */
	a->words[wordnum] |= ((bitmapword) 1 << bitnum);
	return a;
}

bool
bms_covers_member(const Bitmapset *a, int x)
{
	int			wordnum;

	if (x < 0)
		elog(ERROR, "negative bitmapset member not allowed");
	if (a == NULL)
		return false;
	wordnum = WORDNUM(x);
	return (wordnum < a->nwords);
}

Bitmapset *
bms_resize(Bitmapset *a, int wc)
{
	Bitmapset  *result;

	if (a && a->nwords >= wc)
		return a;

	if (a)
	{
		result = (Bitmapset *) repalloc(a, BITMAPSET_SIZE(wc));
		/* do not access a again */
		MemSet(result->words + result->nwords, 0,
			   (wc - result->nwords) * sizeof(bitmapword));
		result->nwords = wc;
	}
	else
	{
		result = palloc0(BITMAPSET_SIZE(wc));
		result->nwords = wc;
	}
	return result;
}

/*
 * bms_del_member - remove a specified member from set
 *
 * No error if x is not currently a member of set
 *
 * Input set is modified in-place!
 */
Bitmapset *
bms_del_member(Bitmapset *a, int x)
{
	int			wordnum,
				bitnum;

	if (x < 0)
		elog(ERROR, "negative bitmapset member not allowed");
	if (a == NULL)
		return NULL;
	wordnum = WORDNUM(x);
	bitnum = BITNUM(x);
	if (wordnum < a->nwords)
		a->words[wordnum] &= ~((bitmapword) 1 << bitnum);
	return a;
}

/*
 * bms_add_members - like bms_union, but left input is recycled
 */
Bitmapset *
bms_add_members(Bitmapset *a, const Bitmapset *b)
{
	Bitmapset  *result;
	const Bitmapset *other;
	int			otherlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return bms_copy(b);
	if (b == NULL)
		return a;
	/* Identify shorter and longer input; copy the longer one if needed */
	if (a->nwords < b->nwords)
	{
		result = bms_copy(b);
		other = a;
	}
	else
	{
		result = a;
		other = b;
	}
	/* And union the shorter input into the result */
	otherlen = other->nwords;
	for (i = 0; i < otherlen; i++)
		result->words[i] |= other->words[i];
	if (result != a)
		pfree(a);
	return result;
}

/*
 * bms_int_members - like bms_intersect, but left input is recycled
 */
Bitmapset *
bms_int_members(Bitmapset *a, const Bitmapset *b)
{
	int			shortlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return NULL;
	if (b == NULL)
	{
		pfree(a);
		return NULL;
	}
	/* Intersect b into a; we need never copy */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
		a->words[i] &= b->words[i];
	for (; i < a->nwords; i++)
		a->words[i] = 0;
	return a;
}

/*
 * bms_del_members - like bms_difference, but left input is recycled
 */
Bitmapset *
bms_del_members(Bitmapset *a, const Bitmapset *b)
{
	int			shortlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return NULL;
	if (b == NULL)
		return a;
	/* Remove b's bits from a; we need never copy */
	shortlen = Min(a->nwords, b->nwords);
	for (i = 0; i < shortlen; i++)
		a->words[i] &= ~b->words[i];
	return a;
}

/*
 * bms_join - like bms_union, but *both* inputs are recycled
 */
Bitmapset *
bms_join(Bitmapset *a, Bitmapset *b)
{
	Bitmapset  *result;
	Bitmapset  *other;
	int			otherlen;
	int			i;

	/* Handle cases where either input is NULL */
	if (a == NULL)
		return b;
	if (b == NULL)
		return a;
	/* Identify shorter and longer input; use longer one as result */
	if (a->nwords < b->nwords)
	{
		result = b;
		other = a;
	}
	else
	{
		result = a;
		other = b;
	}
	/* And union the shorter input into the result */
	otherlen = other->nwords;
	for (i = 0; i < otherlen; i++)
		result->words[i] |= other->words[i];
	if (other != result)		/* pure paranoia */
		pfree(other);
	return result;
}

/*----------
 * bms_first_from - find first member of a set, starting at given index
 *
 * Returns -1 if no members >= x.	The set is not modified.
 *
 * This is intended as support for iterating through the members of a set.
 * The typical pattern is
 *
 *          x = bms_first_from(set, 0);
 *          while (x >= 0)
 *          {
 *              process member x;
 *              x = bms_first_from(set, x + 1);
 *          }
 *----------
 */
int
bms_first_from(const Bitmapset *a, int x)
{
	int			wordnum;
	bitmapword	w;

	if (a == NULL)
		return -1;

	wordnum = WORDNUM(x);

	if ((unsigned) wordnum >= (unsigned) a->nwords)
		return -1;

	w = a->words[wordnum] >> BITNUM(x);
	if (w & 1)
		return x;
	if (w)
		return x + num_low_order_zero_bits(w);

	for (wordnum++; wordnum < a->nwords; wordnum++)
	{
		w = a->words[wordnum];
		if (w)
			return wordnum * BITS_PER_BITMAPWORD + num_low_order_zero_bits(w);
	}
	return -1;
}

/*----------
 * bms_first_member - find and remove first member of a set
 *
 * Returns -1 if set is empty.	NB: set is destructively modified!
 *
 * This is intended as support for iterating through the members of a set.
 * The typical pattern is
 *
 *			tmpset = bms_copy(inputset);
 *			while ((x = bms_first_member(tmpset)) >= 0)
 *				process member x;
 *			bms_free(tmpset);
 *----------
 */
int
bms_first_member(Bitmapset *a)
{
	int			nwords;
	int			wordnum;

	if (a == NULL)
		return -1;
	nwords = a->nwords;
	for (wordnum = 0; wordnum < nwords; wordnum++)
	{
		bitmapword	w = a->words[wordnum];

		if (w != 0)
		{
			w = RIGHTMOST_ONE(w);
			a->words[wordnum] &= ~w;

			return num_low_order_zero_bits(w) + wordnum * BITS_PER_BITMAPWORD;
		}
	}
	return -1;
}

/*
 * bms_hash_value - compute a hash key for a Bitmapset
 *
 * Note: we must ensure that any two bitmapsets that are bms_equal() will
 * hash to the same value; in practice this means that trailing all-zero
 * words must not affect the result.  Hence we strip those before applying
 * hash_any().
 */
uint32
bms_hash_value(const Bitmapset *a)
{
	int			lastword;

	if (a == NULL)
		return 0;				/* All empty sets hash to 0 */
	for (lastword = a->nwords; --lastword >= 0;)
	{
		if (a->words[lastword] != 0)
			break;
	}
	if (lastword < 0)
		return 0;				/* All empty sets hash to 0 */
	return DatumGetUInt32(hash_any((const unsigned char *) a->words,
								   (lastword + 1) * sizeof(bitmapword)));
}
