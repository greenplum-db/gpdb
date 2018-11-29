#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "catalog/pg_type.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum int4_accum(PG_FUNCTION_ARGS);
extern Datum int8_add(PG_FUNCTION_ARGS);

extern Datum tran(PG_FUNCTION_ARGS);
extern Datum final(PG_FUNCTION_ARGS);
extern Datum retcomposite(PG_FUNCTION_ARGS);
extern Datum cons(PG_FUNCTION_ARGS);

const char *accum_str = "gpmrdemo:int4_accum()";
const char *add_str   = "gpmrdemo:int8_add()";

PG_FUNCTION_INFO_V1(int4_accum);
Datum
int4_accum(PG_FUNCTION_ARGS)
{
	int64   state;
	int32   value;

	/*
	 * GUARD against an incorrectly defined SQL function by verifying
	 * that the parameters are the types we are expecting:
	 *    int4_accum(int64, int32) => int64
	 */
	if (PG_NARGS() != 2)
	{
		elog(ERROR, "%s defined with %d arguments, expected 2",
			 accum_str, PG_NARGS() );
	}
	if (get_fn_expr_argtype(fcinfo->flinfo, 0) != INT8OID ||
		get_fn_expr_argtype(fcinfo->flinfo, 1) != INT4OID)
	{
		elog(ERROR, "%s defined with invalid types, expected (int8, int4)",
			 accum_str );
	}
	if (get_fn_expr_rettype(fcinfo->flinfo) != INT8OID)
	{
		elog(ERROR, "%s defined with invalid return type, expected int8",
			 accum_str );
	}

	/*
	 * GUARD against NULL input:
	 *  - IF both are null return NULL
	 *  - otherwise treat NULL as a zero value
	 */
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();
	state = PG_ARGISNULL(0) ? 0 : PG_GETARG_INT64(0);
	value = PG_ARGISNULL(1) ? 0 : PG_GETARG_INT32(1);

	/* Do the math and return the result */
	PG_RETURN_INT64(state + value);
}


PG_FUNCTION_INFO_V1(int8_add);
Datum
int8_add(PG_FUNCTION_ARGS)
{
	int64   state1;
	int64   state2;

	/*
	 * GUARD against an incorrectly defined SQL function by verifying
	 * that the parameters are the types we are expecting:
	 *    int8_add(int64, int64) => int64
	 */
	if (PG_NARGS() != 2)
	{
		elog(ERROR, "%s defined with %d arguments, expected 2",
			 add_str, PG_NARGS() );
	}
	if (get_fn_expr_argtype(fcinfo->flinfo, 0) != INT8OID ||
		get_fn_expr_argtype(fcinfo->flinfo, 1) != INT8OID)
	{
		elog(ERROR, "%s defined with invalid types, expected (int8, int8)",
			 add_str );
	}
	if (get_fn_expr_rettype(fcinfo->flinfo) != INT8OID)
	{
		elog(ERROR, "%s defined with invalid return type, expected int8",
			 add_str );
	}

	/*
	 * GUARD against NULL input:
	 *  - IF both are null return NULL
	 *  - otherwise treat NULL as a zero value
	 */
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();
	state1 = PG_ARGISNULL(0) ? 0 : PG_GETARG_INT64(0);
	state2 = PG_ARGISNULL(1) ? 0 : PG_GETARG_INT64(1);

	/* Do the math and return the result */
	PG_RETURN_INT64(state1 + state2);
}


PG_FUNCTION_INFO_V1(tran);
Datum
tran(PG_FUNCTION_ARGS)
{
	int state = PG_GETARG_INT32(0);
	int arg2 = PG_GETARG_INT32(1);

	if (state > 0)
		arg2 = state + arg2;
 
	PG_RETURN_INT32(arg2);
}

PG_FUNCTION_INFO_V1(cons);
Datum
cons(PG_FUNCTION_ARGS)
{
	int state = PG_GETARG_INT32(0);
	int arg2 = PG_GETARG_INT32(1);

	PG_RETURN_INT32(arg2 + state);
}

PG_FUNCTION_INFO_V1(final);
Datum
final(PG_FUNCTION_ARGS)
{
	int a = PG_GETARG_INT32(0);

	PG_RETURN_INT32(a*2);
}

PG_FUNCTION_INFO_V1(retcomposite);
Datum
retcomposite(PG_FUNCTION_ARGS)
{
    FuncCallContext     *funcctx;
    int                  call_cntr;
    int                  max_calls;
    TupleDesc            tupdesc;
    AttInMetadata       *attinmeta;

     /* stuff done only on the first call of the function */
     if (SRF_IS_FIRSTCALL())
     {
        MemoryContext   oldcontext;

        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();

        /* switch to memory context appropriate for multiple function calls */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /* total number of tuples to be returned */
        funcctx->max_calls = 1;

        /* Build a tuple descriptor for our result type */
        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                            "that cannot accept type record")));

        /*
         * generate attribute metadata needed later to produce tuples from raw
         * C strings
         */
        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        funcctx->attinmeta = attinmeta;

        MemoryContextSwitchTo(oldcontext);
    }

    /* stuff done on every call of the function */
    funcctx = SRF_PERCALL_SETUP();

    call_cntr = funcctx->call_cntr;
    max_calls = 1;
    attinmeta = funcctx->attinmeta;

    if (call_cntr < max_calls)    /* do when there is more left to send */
    {
        char       **values;
        HeapTuple    tuple;
        Datum        result;

        /*
         * Prepare a values array for building the returned tuple.
         * This should be an array of C strings which will
         * be processed later by the type input functions.
         */
        values = (char **) palloc(3 * sizeof(char *));
        values[0] = (char *) palloc(16 * sizeof(char));
        values[1] = (char *) palloc(16 * sizeof(char));
        values[2] = (char *) palloc(16 * sizeof(char));

        snprintf(values[0], 16, "%d", 1*  PG_GETARG_INT32(0));
        snprintf(values[1], 16, "%d", 2*  PG_GETARG_INT32(0));
        snprintf(values[2], 16, "%d", 3*  PG_GETARG_INT32(0));

        /* build a tuple */
        tuple = BuildTupleFromCStrings(attinmeta, values);

        /* make the tuple into a datum */
        result = HeapTupleGetDatum(tuple);

        SRF_RETURN_NEXT(funcctx, result);
    }
    else    /* do when there is no more left */
    {
        SRF_RETURN_DONE(funcctx);
    }
}
