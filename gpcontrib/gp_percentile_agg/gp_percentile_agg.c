/*
 * gpcontrib/gp_percentile_agg/gp_percentile_agg.c
 *
 * Copyright (c) 2022-Present VMware, Inc. or its affiliates.
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include <math.h>

#include "catalog/pg_aggregate.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/tlist.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/tuplesort.h"
#include "../backend/utils/adt/orderedsetaggs.c"

PG_MODULE_MAGIC;

/*
 * Since we use V1 function calling convention, all these functions have
 * the same signature as far as C is concerned.  We provide these prototypes
 * just to forestall warnings when compiled with gcc -Wmissing-prototypes.
 */
PG_FUNCTION_INFO_V1(gp_percentile_cont_float8_transition);
PG_FUNCTION_INFO_V1(gp_percentile_cont_interval_transition);
PG_FUNCTION_INFO_V1(gp_percentile_cont_timestamp_transition);
PG_FUNCTION_INFO_V1(gp_percentile_cont_timestamptz_transition);
PG_FUNCTION_INFO_V1(gp_percentile_disc_transition);
PG_FUNCTION_INFO_V1(gp_percentile_final);

Datum gp_percentile_cont_float8_transition(PG_FUNCTION_ARGS);
Datum gp_percentile_cont_interval_transition(PG_FUNCTION_ARGS);
Datum gp_percentile_cont_timestamp_transition(PG_FUNCTION_ARGS);
Datum gp_percentile_cont_timestamptz_transition(PG_FUNCTION_ARGS);
Datum gp_percentile_disc_transition(PG_FUNCTION_ARGS);
Datum gp_percentile_final(PG_FUNCTION_ARGS);

/*
 * Generic transition function for gp_percentile_cont
 * with a single input column in which we want to suppress nulls
 * This assumes the input tuples are already sorted
 */
static Datum
gp_percentile_cont_transition(FunctionCallInfo fcinfo,
		       LerpFunc lerpfunc)
{
	int64        first_row;
	int64        second_row;

	/* Return state for NULL inputs of val*/
	if (PG_ARGISNULL(1) && !PG_ARGISNULL(0))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));

	/* Ignore NULL inputs for val, percent and total_count*/
	if (PG_ARGISNULL(1) || PG_ARGISNULL(2) || PG_ARGISNULL(3))
		PG_RETURN_NULL();

	double percentile = PG_GETARG_FLOAT8(2);
	if (percentile < 0 || percentile > 1 || isnan(percentile))
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("percentile value %g is not between 0 and 1",
						percentile)));

	Datum prev_state = PG_GETARG_DATUM(0);
	Datum val = PG_GETARG_DATUM(1);
	Datum return_state = prev_state;
	int64 total_rows = PG_GETARG_INT64(3);
	int64 peer_count = PG_GETARG_INT64(4);
	first_row = (int64) floor(percentile * (total_rows - 1) + 1);
	second_row = (int64) ceil(percentile * (total_rows - 1) + 1);
	double proportion = (percentile * (total_rows - 1)) - floor(percentile * (total_rows - 1));
	int64 *cnt;

	if(first_row == second_row)
		proportion = 0;

	if (!fcinfo->flinfo->fn_extra)
	{
		cnt = (int64 *) MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, sizeof(int64));
		*cnt = 1;
		fcinfo->flinfo->fn_extra = cnt;
	}
	else
	{
		cnt = (int64 *) fcinfo->flinfo->fn_extra;
	}

	if(*cnt <= first_row && first_row < *cnt + peer_count)
	{
		return_state = val;
	}
	else if(*cnt <= second_row && second_row < *cnt + peer_count)
	{
		return_state = lerpfunc(prev_state, val, proportion);
	}
	*cnt = *cnt + peer_count;

	if(*cnt > total_rows)
	{
		/* Clean up, so the next group can see NULL for fn_extra */
		pfree(cnt);
		fcinfo->flinfo->fn_extra = NULL;
	}

	PG_RETURN_DATUM(return_state);
}

/*
 * gp_percentile_cont(float8, float8, bigint)    - continuous percentile
 */
Datum
gp_percentile_cont_float8_transition(PG_FUNCTION_ARGS)
{
	return gp_percentile_cont_transition(fcinfo, float8_lerp);
}

/*
 * gp_percentile_cont(interval, float8, bigint)    - continuous percentile
 */
Datum
gp_percentile_cont_interval_transition(PG_FUNCTION_ARGS)
{
	return gp_percentile_cont_transition(fcinfo, interval_lerp);
}

/*
 * gp_percentile_cont(timestamp, float8, bigint)    - continuous percentile
 */
Datum
gp_percentile_cont_timestamp_transition(PG_FUNCTION_ARGS)
{
	return gp_percentile_cont_transition(fcinfo, timestamp_lerp);
}

/*
 * gp_percentile_cont(timestamptz, float8, bigint)    - continuous percentile
 */
Datum
gp_percentile_cont_timestamptz_transition(PG_FUNCTION_ARGS)
{
	return gp_percentile_cont_transition(fcinfo, timestamptz_lerp);
}

/*
 * Transition function for gp_percentile_disc  - discrete percentile
 * This assumes the input tuples are already sorted
 */
Datum
gp_percentile_disc_transition(PG_FUNCTION_ARGS)
{
	int64        rownum;

	/* Return state for NULL inputs of val*/
	if (PG_ARGISNULL(1) && !PG_ARGISNULL(0))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));

	/* Ignore NULL inputs for val, percent and total_count*/
	if (PG_ARGISNULL(1) || PG_ARGISNULL(2) || PG_ARGISNULL(3))
			PG_RETURN_NULL();

	double percentile = PG_GETARG_FLOAT8(2);
	if (percentile < 0 || percentile > 1 || isnan(percentile))
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("percentile value %g is not between 0 and 1",
						percentile)));
	Datum prev_state = PG_GETARG_DATUM(0);
	Datum val = PG_GETARG_DATUM(1);
	Datum return_state = prev_state;
	int64 total_rows = PG_GETARG_INT64(3);
	int64 peer_count = PG_GETARG_INT64(4);
	rownum = (int64) ceil(percentile * (total_rows));
	int64 *cnt;

	if (!fcinfo->flinfo->fn_extra)
	{
		cnt = (int64 *) MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, sizeof(int64));
		*cnt = 1;
		fcinfo->flinfo->fn_extra = cnt;
		if (percentile == 0.0)
			rownum = 1;
	}
	else
	{
		cnt = (int64 *) fcinfo->flinfo->fn_extra;
	}

	if(*cnt <= rownum && rownum < *cnt + peer_count)
	{
		return_state = val;
	}

	*cnt = *cnt + peer_count;

	if(*cnt > total_rows)
	{
		/* Clean up, so the next group can see NULL for fn_extra */
		pfree(cnt);
		fcinfo->flinfo->fn_extra = NULL;
	}

	PG_RETURN_DATUM(return_state);
}

/*
 * Final function for gp_percentile
 */
Datum
gp_percentile_final(PG_FUNCTION_ARGS)
{
	/* Get and check the percentile argument */
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	PG_RETURN_DATUM(PG_GETARG_DATUM(0));
}

