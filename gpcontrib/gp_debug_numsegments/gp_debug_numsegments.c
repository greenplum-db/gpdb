/*-------------------------------------------------------------------------
 *
 * gp_debug_numsegments.c
 *	  Debugging helpers to get / set default numsegment when creating tables.
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "utils/builtins.h"
#include "cdb/cdbutil.h"
#include "catalog/pg_type.h"


#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum gp_debug_set_create_table_default_numsegments(PG_FUNCTION_ARGS);
extern Datum gp_debug_reset_create_table_default_numsegments(PG_FUNCTION_ARGS);
extern Datum gp_debug_get_create_table_default_numsegments(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_debug_set_create_table_default_numsegments);
PG_FUNCTION_INFO_V1(gp_debug_reset_create_table_default_numsegments);
PG_FUNCTION_INFO_V1(gp_debug_get_create_table_default_numsegments);

static int	reset_numsegments = GP_DEFAULT_NUMSEGMENTS_FULL;

/*
 * Set the default numsegments when creating tables.
 *
 * It accepts one argument, in text or integer format.
 *
 * Valid values for text argument:
 * - 'FULL': all the segments;
 * - 'RANDOM': pick a random set of segments each time;
 * - 'MINIMAL': the minimal set of segments;
 *
 * For integer argument the valid range is [1, gp_num_contents_in_cluster].
 */
Datum
gp_debug_set_create_table_default_numsegments(PG_FUNCTION_ARGS)
{
	Oid			argtypeoid = InvalidOid;
	const char *str = NULL;
	int			numsegments = -1;

	Assert(1 == PG_NARGS());

	argtypeoid = get_fn_expr_argtype(fcinfo->flinfo, 0);

	/* Accept argument in integer / text format */
	Assert(argtypeoid == INT4OID || argtypeoid == TEXTOID);

	if (argtypeoid == INT4OID)
	{
		numsegments = PG_GETARG_INT32(0);
	}
	else
	{
		str = text_to_cstring(PG_GETARG_TEXT_P(0));
		Assert(str != NULL);
	}

	if (str == NULL)
	{
		if (numsegments < 1 || numsegments > getgpsegmentCount())
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("invalid integer value for default numsegments: %d",
							numsegments),
					 errhint("Valid range: [1, %d (gp_num_contents_in_cluster)]",
							 getgpsegmentCount())));

		gp_create_table_default_numsegments = numsegments;
	}
	else if (pg_strcasecmp(str, "full") == 0)
		gp_create_table_default_numsegments = GP_DEFAULT_NUMSEGMENTS_FULL;
	else if (pg_strcasecmp(str, "random") == 0)
		gp_create_table_default_numsegments = GP_DEFAULT_NUMSEGMENTS_RANDOM;
	else if (pg_strcasecmp(str, "minimal") == 0)
		gp_create_table_default_numsegments = GP_DEFAULT_NUMSEGMENTS_MINIMAL;
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("invalid text value for default numsegments: '%s'", str),
				 errhint("Valid values: 'full', 'minimal', 'random'")));
	}

	return gp_debug_get_create_table_default_numsegments(fcinfo);
}

/*
 * Reset the default numsegments when creating tables.
 *
 * It accepts zero or one argument, in text or integer format.
 *
 * Valid values for text argument:
 * - 'FULL': all the segments;
 * - 'RANDOM': pick a random set of segments each time;
 * - 'MINIMAL': the minimal set of segments;
 *
 * For integer argument the valid range is [1, gp_num_contents_in_cluster].
 *
 * When there is zero argument it reset the default numsegments directly.
 */
Datum
gp_debug_reset_create_table_default_numsegments(PG_FUNCTION_ARGS)
{
	if (1 == PG_NARGS())
	{
		gp_debug_set_create_table_default_numsegments(fcinfo);

		reset_numsegments = gp_create_table_default_numsegments;
	}
	else
	{
		Assert(0 == PG_NARGS());

		gp_create_table_default_numsegments = reset_numsegments;
	}

	PG_RETURN_VOID();
}

/*
 * Get the default numsegments when creating a table.
 */
Datum
gp_debug_get_create_table_default_numsegments(PG_FUNCTION_ARGS)
{
	char		buf[64];
	const char *result = NULL;

	switch (gp_create_table_default_numsegments)
	{
		case GP_DEFAULT_NUMSEGMENTS_FULL:
			result = "FULL";
			break;
		case GP_DEFAULT_NUMSEGMENTS_RANDOM:
			result = "RANDOM";
			break;
		case GP_DEFAULT_NUMSEGMENTS_MINIMAL:
			result = "MINIMAL";
			break;
		default:
			snprintf(buf, sizeof(buf), "%d",
					 gp_create_table_default_numsegments);
			result = buf;
			break;
	}

	PG_RETURN_DATUM(CStringGetTextDatum(result));
}
