#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "catalog/pg_type.h"      /* oids of known types */

/* Do the module magic dance */
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
PG_FUNCTION_INFO_V1(int4_accum);
PG_FUNCTION_INFO_V1(int8_add);


/* Declare the functions as module exports */
Datum int4_accum(PG_FUNCTION_ARGS);
Datum int8_add(PG_FUNCTION_ARGS);

const char* accum_str = "gpmrdemo:int4_accum()";
const char* add_str   = "gpmrdemo:int8_add()";




/* Define the functions */
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

