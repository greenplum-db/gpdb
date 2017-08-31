/*-------------------------------------------------------------------------
 *
 * percentile.c
 *	  Support functions for inverse distribution functions.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/adt/percentile.c
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"

/*
 * Information for percentile functions.
 * The prev_value is only for percentile_cont, where we need to remember the
 * prior value to interpolate two values.  The target position is the
 * row position we want given the total row count and percentage.
 * The ceiled and floored target positions are rounded target positions.
 * The row number tracks the current logical row position.
 */
typedef struct
{
	Datum		prev_value;		/* the target value at the prior row */
	float8		tp;				/* target position */
	float8		ctp;			/* ceiled target position */
	float8		ftp;			/* floored target position */
	int64		rn;				/* current row number */
} PercentileInfo;

/*
 * transition function for percentile_cont().
 *
 * The actual arguments are:
 *		(state_value, percentage, target_value, peer_count, total_count)
 *
 * The result of percentile_cont() is the interpolated value from
 * value expressions at consecutive rows that are indicated by the
 * argument, in the order specified by the WITHIN GROUP.
 *
 * We compute these values at the first stage of this transition:
 *
 *	tp = (total_count - 1) * percentage + 1
 *	ftp = floor(tp)
 *	ctp = ceil(tp)
 *	rn0 = (current logical row position)
 *	rn1 = rn0 + peer_count
 *	tv = (target value)
 *
 * And the result is calculated as:
 *
 *	result = SUM(
 *		CASE
 *			WHEN rn0 <= ftp AND rn1 > ctp THEN tv
 *			WHEN rn0 <= ftp AND rn1 > ftp THEN tv * (ctp - tp)
 *			WHEN rn0 >= ctp THEN tv * (tp - ftp)
 *		END
 *	)
 *
 * Note that we use FmgrInfo's fn_extra to store the per-group information.
 * fn_extra is not initialized by the executor in the group boundaries,
 * so we clean it when the value is found.  That said, we assume we
 * always found the required value in each group, and if not, something
 * is wrong.
 */
Datum
percentile_cont_trans(PG_FUNCTION_ARGS)
{
	int64				pc = PG_GETARG_INT64(3);
	int64				rn0, rn1;
	PercentileInfo	   *info;

	if (!PG_ARGISNULL(0))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));

	/* Ignore NULL inputs for percentage and target value */
	if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
		PG_RETURN_NULL();

	if (!fcinfo->flinfo->fn_extra)
	{
		float8				percentage = PG_GETARG_FLOAT8(1);
		int64				tc = PG_GETARG_INT64(4);

		if (percentage < 0.0 || percentage > 1.0)
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("input is out of range"),
					 errhint("Argument to percentile function must be between 0.0 and 1.0.")));
		info = (PercentileInfo *) MemoryContextAllocZero(
				fcinfo->flinfo->fn_mcxt, sizeof(PercentileInfo));
		info->tp = (tc - 1) * percentage + 1;
		info->ftp = floor(info->tp);
		info->ctp = ceil(info->tp);
		info->rn = 1;
		fcinfo->flinfo->fn_extra = info;
	}
	else
		info = (PercentileInfo *) fcinfo->flinfo->fn_extra;

	rn0 = info->rn;
	rn1 = rn0 + pc;
	info->rn += pc;
	if (rn0 <= info->ftp && rn1 > info->ctp)
	{
		/* Clean up, so the next group can see NULL for fn_extra */
		pfree(info);
		fcinfo->flinfo->fn_extra = NULL;

		PG_RETURN_DATUM(PG_GETARG_DATUM(2));
	}
	else if (rn0 <= info->ftp && rn1 > info->ftp)
	{
		Oid			resulttype;
		bool		byval;
		int16		len;
		MemoryContext oldcontext;

		resulttype = get_fn_expr_rettype(fcinfo->flinfo);
		get_typlenbyval(resulttype, &len, &byval);

		oldcontext = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
		info->prev_value = datumCopy(PG_GETARG_DATUM(2), byval, len);
		MemoryContextSwitchTo(oldcontext);
	}
	else if (rn0 >= info->ctp)
	{
		Datum		prev = info->prev_value;
		Datum		tv = PG_GETARG_DATUM(2);
		float8		tp = info->tp;
		float8		ctp = info->ctp;
		float8		ftp = info->ftp;
		Oid			resulttype = get_fn_expr_rettype(fcinfo->flinfo);

		/* Clean up, so the next group can see NULL for fn_extra */
		pfree(info);
		fcinfo->flinfo->fn_extra = NULL;

		if (resulttype == FLOAT8OID)
		{
			PG_RETURN_FLOAT8(DatumGetFloat8(prev) * (ctp - tp) +
								DatumGetFloat8(tv) * (tp - ftp));
		}
		else if (resulttype == TIMESTAMPOID)
		{
			Datum	interval;

			interval = DirectFunctionCall2(timestamp_mi, tv, prev);
			interval = DirectFunctionCall2(interval_mul, interval,
										   Float8GetDatum(tp - ftp));
			PG_RETURN_DATUM(DirectFunctionCall2(timestamp_pl_interval,
								prev, interval));
		}
		else if (resulttype == TIMESTAMPTZOID)
		{
			Datum	interval;

			interval = DirectFunctionCall2(timestamp_mi, tv, prev);
			interval = DirectFunctionCall2(interval_mul, interval,
										   Float8GetDatum(tp - ftp));
			PG_RETURN_DATUM(DirectFunctionCall2(timestamptz_pl_interval,
								prev, interval));
		}
		else if (resulttype == INTERVALOID)
		{
			Datum	val1, val2;

			val1 = DirectFunctionCall2(interval_mul, prev, Float8GetDatum(ctp - tp));
			val2 = DirectFunctionCall2(interval_mul, tv, Float8GetDatum(tp - ftp));

			PG_RETURN_DATUM(DirectFunctionCall2(interval_pl, val1, val2));
		}
		else /* Should not happen. */
			elog(ERROR, "unexpected result type: %d", (int) resulttype);
	}

	PG_RETURN_NULL();
}

/*
 * transition function for percentile_disc().
 *
 * The actual arguments are:
 *		(state_value, percentage, target_value, peer_count, total_count)
 *
 * The result of percentile_disc() is the the first value whose position
 * in the cumulative distribution of values, specified by the WITHIN GROUP
 * clause, is equal to or greater than the percentage specified.  And
 * the definition of the cumulative distribution, i.e. CUME_DIST(), is
 * following.
 *
 *	count(*) OVER (ORDER BY tv) / count(*) OVER ()
 *
 * Let accum_count be the numerator and total_count be the denominator.
 * Let p be the percentage given as the argument.  Now, the result of
 * percentile_disc() is at the first row position that satisfies
 *
 *	p >= accum_count / total_count
 *
 * Converting this to
 *
 *	p * total_count >= accum_count
 *
 * and because the left hand side is fraction row position, we take ceil
 *
 *	ceil(p * total_count)
 *
 * This is the row position where the result is at.
 */
Datum
percentile_disc_trans(PG_FUNCTION_ARGS)
{
	int64				pc = PG_GETARG_INT64(3);
	int64				rn0, rn1;
	PercentileInfo	   *info;

	if (!PG_ARGISNULL(0))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));

	/* Ignore NULL inputs for percentage and target value */
	if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
		PG_RETURN_NULL();

	if (!fcinfo->flinfo->fn_extra)
	{
		float8				percentage = PG_GETARG_FLOAT8(1);
		int64				tc = PG_GETARG_INT64(4);

		if (percentage < 0.0 || percentage > 1.0)
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("input is out of range"),
					 errhint("Argument to percentile function must be between 0.0 and 1.0.")));
		info = (PercentileInfo *) MemoryContextAllocZero(
				fcinfo->flinfo->fn_mcxt, sizeof(PercentileInfo));
		if (percentage == 0.0)
			info->tp = 1.0;
		else
			info->tp = ceil(tc * percentage);
		info->ctp = info->ftp = info->tp;
		info->rn = 1;
		fcinfo->flinfo->fn_extra = info;
	}
	else
		info = (PercentileInfo *) fcinfo->flinfo->fn_extra;

	rn0 = info->rn;
	rn1 = rn0 + pc;
	info->rn += pc;
	if (rn0 <= info->tp && rn1 > info->tp)
	{
		/* Clean up, so the next group can see NULL for fn_extra */
		pfree(info);
		fcinfo->flinfo->fn_extra = NULL;

		PG_RETURN_DATUM(PG_GETARG_DATUM(2));
	}

	PG_RETURN_NULL();
}
