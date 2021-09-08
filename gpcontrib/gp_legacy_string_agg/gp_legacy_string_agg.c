/*
 * gpcontrib/gp_legacy_string_agg/gp_legacy_string_agg.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */


PG_MODULE_MAGIC;

/*
* string_agg - Concatenates values and returns string.
*
* Syntax: string_agg(value text, delimiter text) RETURNS text
*
* Note: Any NULL values are ignored. The first-call delimiter isn't
* actually used at all, and on subsequent calls the delimiter precedes
* the associated value.
*/

/* subroutine to initialize state */
static StringInfo
makeStringAggState(FunctionCallInfo fcinfo)
{
	StringInfo	state;
	MemoryContext aggcontext;
	MemoryContext oldcontext;

	if (!AggCheckCallContext(fcinfo, &aggcontext))
	{
		/* cannot be called directly because of internal-type argument */
		elog(ERROR, "gp_legacy_string_agg_transfn called in non-aggregate context");
	}

	/*
	 * Create state in aggregate context.  It'll stay there across subsequent
	 * calls.
	 */
	oldcontext = MemoryContextSwitchTo(aggcontext);
	state = makeStringInfo();
	MemoryContextSwitchTo(oldcontext);

	return state;
}

static void
appendStringInfoText(StringInfo str, const text *t)
{
	appendBinaryStringInfo(str, VARDATA_ANY((void *) t), VARSIZE_ANY_EXHDR((void *) t));
}

/*
 * cstring_to_text_with_len
 *
 * Same as cstring_to_text except the caller specifies the string length;
 * the string need not be null_terminated.
 */
static text *
cstring_to_text_with_len(const char *s, int len)
{
	text	   *result = (text *) palloc(len + VARHDRSZ);

	SET_VARSIZE(result, len + VARHDRSZ);
	memcpy(VARDATA(result), s, len);

	return result;
}

/*
 * Since we use V1 function calling convention, all these functions have
 * the same signature as far as C is concerned.  We provide these prototypes
 * just to forestall warnings when compiled with gcc -Wmissing-prototypes.
 */
PG_FUNCTION_INFO_V1(gp_legacy_string_agg_transfn);
PG_FUNCTION_INFO_V1(gp_legacy_string_agg_finalfn);

Datum		gp_legacy_string_agg_transfn(PG_FUNCTION_ARGS);
Datum		gp_legacy_string_agg_finalfn(PG_FUNCTION_ARGS);


Datum
gp_legacy_string_agg_transfn(PG_FUNCTION_ARGS)
{
	StringInfo state = NULL;
	state = PG_ARGISNULL(0) ? NULL : (StringInfo) PG_GETARG_POINTER(0);
	/* Append the value unless null. */

	if (!PG_ARGISNULL(1))
	{
		/* On the first time through, we ignore the delimiter. */
		if (state == NULL)
			state = makeStringAggState(fcinfo);
		appendStringInfoText(state, PG_GETARG_TEXT_PP(1));          /* value */
	}

	/*
	* The transition type for string_agg() is declared to be "internal",
	* which is a pass-by-value type the same size as a pointer.
	*/
	PG_RETURN_POINTER(state);
}

Datum
gp_legacy_string_agg_finalfn(PG_FUNCTION_ARGS)
{
	StringInfo state;
	/* cannot be called directly because of internal-type argument */
	Assert(AggCheckCallContext(fcinfo, NULL));
	state = PG_ARGISNULL(0) ? NULL : (StringInfo) PG_GETARG_POINTER(0);

	elog(WARNING, "Deprecated call to string_agg(text), use string_agg(text, text) instead");

	if (state != NULL)
		PG_RETURN_TEXT_P(cstring_to_text_with_len(state->data, state->len));
	else
		PG_RETURN_NULL();
}
