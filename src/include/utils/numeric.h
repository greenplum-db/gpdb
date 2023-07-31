/*-------------------------------------------------------------------------
 *
 * numeric.h
 *	  Definitions for the exact numeric data type of Postgres
 *
 * Original coding 1998, Jan Wieck.  Heavily revised 2003, Tom Lane.
 *
 * Copyright (c) 1998-2019, PostgreSQL Global Development Group
 *
 * src/include/utils/numeric.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PG_NUMERIC_H_
#define _PG_NUMERIC_H_

#include "fmgr.h"

/*
 * Limit on the precision (and hence scale) specifiable in a NUMERIC typmod.
 * Note that the implementation limit on the length of a numeric value is
 * much larger --- beware of what you use this for!
 */
#define NUMERIC_MAX_PRECISION		1000

/*
 * Internal limits on the scales chosen for calculation results
 */
#define NUMERIC_MAX_DISPLAY_SCALE	NUMERIC_MAX_PRECISION
#define NUMERIC_MIN_DISPLAY_SCALE	0

#define NUMERIC_MAX_RESULT_SCALE	(NUMERIC_MAX_PRECISION * 2)

/*
 * For inherently inexact calculations such as division and square root,
 * we try to get at least this many significant digits; the idea is to
 * deliver a result no worse than float8 would.
 */
#define NUMERIC_MIN_SIG_DIGITS		16

/* The actual contents of Numeric are private to numeric.c */
struct NumericData;
typedef struct NumericData *Numeric;
typedef struct Int8TransTypeData
{
	int64		count;
	int64		sum;
} Int8TransTypeData;

/* ----------
 * Fast sum accumulator.
 *
 * NumericSumAccum is used to implement SUM(), and other standard aggregates
 * that track the sum of input values.  It uses 32-bit integers to store the
 * digits, instead of the normal 16-bit integers (with NBASE=10000).  This
 * way, we can safely accumulate up to NBASE - 1 values without propagating
 * carry, before risking overflow of any of the digits.  'num_uncarried'
 * tracks how many values have been accumulated without propagating carry.
 *
 * Positive and negative values are accumulated separately, in 'pos_digits'
 * and 'neg_digits'.  This is simpler and faster than deciding whether to add
 * or subtract from the current value, for each new value (see sub_var() for
 * the logic we avoid by doing this).  Both buffers are of same size, and
 * have the same weight and scale.  In accum_sum_final(), the positive and
 * negative sums are added together to produce the final result.
 *
 * When a new value has a larger ndigits or weight than the accumulator
 * currently does, the accumulator is enlarged to accommodate the new value.
 * We normally have one zero digit reserved for carry propagation, and that
 * is indicated by the 'have_carry_space' flag.  When accum_sum_carry() uses
 * up the reserved digit, it clears the 'have_carry_space' flag.  The next
 * call to accum_sum_add() will enlarge the buffer, to make room for the
 * extra digit, and set the flag again.
 *
 * To initialize a new accumulator, simply reset all fields to zeros.
 *
 * The accumulator does not handle NaNs.
 * ----------
 */
typedef struct NumericSumAccum
{
	int			ndigits;
	int			weight;
	int			dscale;
	int			num_uncarried;
	bool		have_carry_space;
	int32	   *pos_digits;
	int32	   *neg_digits;
} NumericSumAccum;

typedef struct NumericAggState
{
	bool		calcSumX2;		/* if true, calculate sumX2 */
	MemoryContext agg_context;	/* context we're calculating in */
	int64		N;				/* count of processed numbers */
	NumericSumAccum sumX;		/* sum of processed numbers */
	NumericSumAccum sumX2;		/* sum of squares of processed numbers */
	int			maxScale;		/* maximum scale seen so far */
	int64		maxScaleCount;	/* number of values seen with maximum scale */
	int64		NaNcount;		/* count of NaN values (not included in N!) */
} NumericAggState;

extern NumericAggState *makeNumericAggState(FunctionCallInfo fcinfo, bool calcSumX2);
extern NumericAggState *makeNumericAggStateCurrentContext(bool calcSumX2);

#ifdef HAVE_INT128
typedef struct Int128AggState
{
	bool		calcSumX2;		/* if true, calculate sumX2 */
	int64		N;				/* count of processed numbers */
	int128		sumX;			/* sum of processed numbers */
	int128		sumX2;			/* sum of squares of processed numbers */
} Int128AggState;
extern Int128AggState *makeInt128AggState(FunctionCallInfo fcinfo, bool calcSumX2);
extern Int128AggState *makeInt128AggStateCurrentContext(bool calcSumX2);
typedef Int128AggState PolyNumAggState;
#define makePolyNumAggState makeInt128AggState
#define makePolyNumAggStateCurrentContext makeInt128AggStateCurrentContext
#else
typedef NumericAggState PolyNumAggState;
#define makePolyNumAggState makeNumericAggState
#define makePolyNumAggStateCurrentContext makeNumericAggStateCurrentContext
#endif

/*
 * fmgr interface macros
 */

#define DatumGetNumeric(X)		  ((Numeric) PG_DETOAST_DATUM(X))
#define DatumGetNumericCopy(X)	  ((Numeric) PG_DETOAST_DATUM_COPY(X))
#define NumericGetDatum(X)		  PointerGetDatum(X)
#define PG_GETARG_NUMERIC(n)	  DatumGetNumeric(PG_GETARG_DATUM(n))
#define PG_GETARG_NUMERIC_COPY(n) DatumGetNumericCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_NUMERIC(x)	  return NumericGetDatum(x)
extern double numeric_to_double_no_overflow(Numeric num);
extern int cmp_numerics(Numeric num1, Numeric num2);
extern float8 numeric_li_fraction(Numeric x, Numeric x0, Numeric x1, 
								  bool *eq_bounds, bool *eq_abscissas);
extern Numeric numeric_li_value(float8 f, Numeric y0, Numeric y1);
extern void do_numeric_accum(NumericAggState *state, Numeric newval);
/*
 * Utility functions in numeric.c
 */
extern bool numeric_is_nan(Numeric num);
extern int16 *numeric_digits(Numeric num);
extern int numeric_len(Numeric num);
int32		numeric_maximum_size(int32 typmod);
extern char *numeric_out_sci(Numeric num, int scale);
extern char *numeric_normalize(Numeric num);

extern Numeric numeric_add_opt_error(Numeric num1, Numeric num2,
									 bool *have_error);
extern Numeric numeric_sub_opt_error(Numeric num1, Numeric num2,
									 bool *have_error);
extern Numeric numeric_mul_opt_error(Numeric num1, Numeric num2,
									 bool *have_error);
extern Numeric numeric_div_opt_error(Numeric num1, Numeric num2,
									 bool *have_error);
extern Numeric numeric_mod_opt_error(Numeric num1, Numeric num2,
									 bool *have_error);
extern int32 numeric_int4_opt_error(Numeric num, bool *error);

#endif							/* _PG_NUMERIC_H_ */
