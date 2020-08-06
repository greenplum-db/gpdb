#include "postgres.h"
#include "funcapi.h"

#include <ctype.h>
#include <math.h>

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

#define SAMESIGN(a,b)	(((a) < 0) == ((b) < 0))

/*
 * check to see if a float4/8 val has underflowed or overflowed
 */
#define CHECKFLOATVAL(val, inf_is_valid, zero_is_valid)			\
do {															\
	if (isinf(val) && !(inf_is_valid))							\
		ereport(ERROR,											\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),	\
		  errmsg("value out of range: overflow")));				\
																\
	if ((val) == 0.0 && !(zero_is_valid))						\
		ereport(ERROR,											\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),	\
		 errmsg("value out of range: underflow")));				\
} while(0)


/*
 * check to see if a int16/32/64 val has overflow in addition
 */
#define CHECKINTADD(result, arg1, arg2)							\
do {															\
	if (SAMESIGN(arg1, arg2) && !SAMESIGN(result, arg1))		\
		ereport(ERROR,											\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),	\
		  errmsg("int value out of range: overflow")));				\
} while(0)

/*
 * matrix_add - array summation over two input arrays
 */
Datum
matrix_add(PG_FUNCTION_ARGS)
{
	ArrayType  *m;
	ArrayType  *n;
	Oid			mtype;
	Oid			ntype;
	int			i;
	int			ndim;
	int			len;
	bool		transition_function;
	
	/* If we're in a transition function we can be smarter */
	transition_function = fcinfo->context && IsA(fcinfo->context, AggState);
	
	/* Validate arguments */
	if (PG_NARGS() != 2) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("matrix_add called with %d arguments", PG_NARGS())));
	
	/* 
	 * This function is sometimes strict, and sometimes not in order to deal
	 * with needing to upconvert datatypes in an aggregate function.
	 */
	if (fcinfo->flinfo->fn_strict && (PG_ARGISNULL(0) || PG_ARGISNULL(1)))
		PG_RETURN_NULL();

	/*
	 * When we are upconverting we always upconvert to the datatype of the
	 * first argument, so the first argument is a safe return value 
	 */
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}

	n = PG_GETARG_ARRAYTYPE_P(1);
	ndim  = ARR_NDIM(n);
	ntype = ARR_ELEMTYPE(n);

	if (ndim == 0)
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}

	/* Typecheck the input arrays, we only handle fixed length numeric data. */
	if (ntype != INT2OID   && ntype != INT4OID && ntype != INT8OID && 
		ntype != FLOAT4OID && ntype != FLOAT8OID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("matrix_add: unsupported datatype")));

	/* count total number of elements */
	for (len = 1, i = 0; i < ndim; i++)
		len *= ARR_DIMS(n)[i];

	if (PG_ARGISNULL(0))
	{
		int       size;
		int       elsize;
		Oid       returntype;
		TupleDesc tupdesc;
		
		/* Determine what our return type should be */
		(void) get_call_result_type(fcinfo, &returntype, &tupdesc);
		switch (returntype)
		{
			case INT2ARRAYOID:
				mtype = INT2OID;
				elsize = sizeof(int16);
				break;
			case INT4ARRAYOID:
				mtype = INT4OID;
				elsize = sizeof(int32);
				break;
			case INT8ARRAYOID:
				mtype = INT8OID;
				elsize = sizeof(int64);
				break;
			case FLOAT4ARRAYOID:
				mtype = FLOAT4OID;
				elsize = sizeof(float4);
				break;
			case FLOAT8ARRAYOID:
				mtype = FLOAT8OID;
				elsize = sizeof(float8);
				break;
			default:
				ereport(ERROR, 
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("matrix_add: return datatype lookup failure")));

				/* Completely useless code that fixes compiler warnings */
				mtype = INT2OID;
				elsize = sizeof(int16);

		}

		/* Allocate the state matrix */
		size  = ARR_OVERHEAD_NONULLS(ndim) + len * elsize;
		m = (ArrayType *) palloc(size);
		SET_VARSIZE(m, size);
		m->ndim = ndim;
		m->dataoffset = 0;
		m->elemtype = mtype;
		for (i = 0; i < ndim; i++)
		{
			ARR_DIMS(m)[i] = ARR_DIMS(n)[i];
			ARR_LBOUND(m)[i] = 1;
		}
		memset(ARR_DATA_PTR(m), 0, len * elsize);
	}
	else
	{
		m = PG_GETARG_ARRAYTYPE_P(0);
		mtype = ARR_ELEMTYPE(m);
		if (ndim != ARR_NDIM(m))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("matrix_add: Dimensionality of both arrays must match")));
		for (i = 0; i < ndim; i++)
		{
			if (ARR_DIMS(m)[i] != ARR_DIMS(n)[i])
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("matrix_add: non-conformable arrays")));
		}
		if (ARR_NULLBITMAP(m) || ARR_NULLBITMAP(n))
			ereport(ERROR,
					(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
					 errmsg("matrix_add: null array element not allowed in this context")));

		/* Typecheck the input arrays, we only handle fixed length numeric data. */
		if (ntype != INT2OID   && ntype != INT4OID && ntype != INT8OID && 
			ntype != FLOAT4OID && ntype != FLOAT8OID)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("matrix_add: unsupported datatype")));
	}
	
	/*
	 * Overflow check.	If the sign of inputs are different, then their sum
	 * cannot overflow.  If the inputs are of the same sign, their sum had
	 * better be that sign too.
	 */
	/* Transition function updates in place, otherwise allocate result */
	if (transition_function) 
	{
		switch (mtype)
		{
			case INT2OID:
			{
				int16 *data_m = (int16*) ARR_DATA_PTR(m);
				/*	plus result, need to check overflow*/
				int16 result;
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/*
							 * return type of plus of two int16 is int32,
							 * we should cast to int16 explicitly
							 */
							result = (int16) (data_m[i] + data_n[i]);
							/* overflow checking*/
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					default:
						ereport(ERROR,
								(errcode(ERRCODE_DATATYPE_MISMATCH),
								 errmsg("matrix_add: can not downconvert state")));
				}
				break;
			}
			case INT4OID:
			{
				int32 *data_m = (int32*) ARR_DATA_PTR(m);
				/*	plus result, need to check overflow*/
				int32 result;
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							result = data_m[i] + data_n[i];
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							result = data_m[i] + data_n[i];
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					default:
						ereport(ERROR,
								(errcode(ERRCODE_DATATYPE_MISMATCH),
								 errmsg("matrix_add: can not downconvert state")));
				}
				break;
			}
			case INT8OID:
			{
				int64 *data_m = (int64*) ARR_DATA_PTR(m);
				/*	plus result, need to check overflow*/
				int64 result;
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							result = data_m[i] + data_n[i];
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							result = data_m[i] + data_n[i];
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							result = data_m[i] + data_n[i];
							CHECKINTADD(result, data_m[i], data_n[i]);
							data_m[i] = result;
						}
						break;
					}
					default:
						ereport(ERROR,
								(errcode(ERRCODE_DATATYPE_MISMATCH),
								 errmsg("matrix_add: can not downconvert state")));
				}
				break;
			}
			case FLOAT4OID:
			{
				float4 *data_m = (float4*) ARR_DATA_PTR(m);
				float4 add_r;
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							/* explicit upcasting */
							data_m[i] += (float4)data_n[i];
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							/* explicit upcasting */
							data_m[i] += (float4)data_n[i];
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							/* explicit upcasting */
							data_m[i] += (float4)data_n[i];
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							add_r = data_m[i] + data_n[i];
							CHECKFLOATVAL(add_r, isinf(data_m[i]) || isinf(data_n[i]), true);
							data_m[i] = add_r;
						}
						break;
					}
					default:
						ereport(ERROR,
								(errcode(ERRCODE_DATATYPE_MISMATCH),
								 errmsg("matrix_add: can not downconvert state")));
				}
				break;
			}
			case FLOAT8OID:
			{
				float8 *data_m = (float8*) ARR_DATA_PTR(m);
				float8 add_r;
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_m[i] += data_n[i];
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_m[i] += data_n[i];
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							/* explicit upcasting */
							data_m[i] += (float8)data_n[i];
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_m[i] += data_n[i];
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* overflow checking */
							add_r = data_m[i] + data_n[i];
							CHECKFLOATVAL(add_r, isinf(data_m[i]) || isinf(data_n[i]), true);
							data_m[i] = add_r;
						}
						break;
					}
					default:
						ereport(ERROR,
								(errcode(ERRCODE_DATATYPE_MISMATCH),
								 errmsg("matrix_add: can not downconvert state")));
				}
				break;
			}
			default:
				Assert(false);
		}
		PG_RETURN_ARRAYTYPE_P(m);
	}
	else
	{
		ArrayType *result;
		Oid        rtype  = InvalidOid;
		int        elsize = 0;
		int        size;

		/* Result type for non-transition function is the higher of the two input types */
		if (ntype == FLOAT8OID || mtype == FLOAT8OID)
		{
			rtype  = FLOAT8OID;
			elsize = sizeof(float8);
		}
		else if (ntype == FLOAT4OID || mtype == FLOAT4OID)
		{
			rtype  = FLOAT4OID;
			elsize = sizeof(float4);
		}
		else if (ntype == INT8OID || mtype == INT8OID)
		{
			rtype  = INT8OID;
			elsize = sizeof(int64);
		}
		else if (ntype == INT4OID || mtype == INT4OID)
		{
			rtype  = INT4OID;
			elsize = sizeof(int32);
		}
		else if (ntype == INT2OID || mtype == INT2OID)
		{
			rtype  = INT2OID;
			elsize = sizeof(int16);
		}
		Assert(rtype != InvalidOid && elsize > 0);

		size = ARR_OVERHEAD_NONULLS(ndim) + len * elsize;
		result = (ArrayType *) palloc(size);
		SET_VARSIZE(result, size);
		result->ndim = ndim;
		result->dataoffset = 0;          /* We dissallowed arrays with NULLS */
		result->elemtype = rtype;
		for (i = 0; i < ndim; i++)
		{
			ARR_DIMS(result)[i] = ARR_DIMS(n)[i];
			ARR_LBOUND(result)[i] = 1;
		}
		switch (mtype)
		{
			case INT2OID:
			{
				int16 *data_m = (int16*) ARR_DATA_PTR(m);
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						int16 *data_r = (int16*) ARR_DATA_PTR(result);
						Assert(rtype == INT2OID);
						for (i = 0; i < len; i++){
							data_r[i] = (int16) (data_m[i] + data_n[i]);
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						int32 *data_r = (int32*) ARR_DATA_PTR(result);
						Assert(rtype == INT4OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						int64 *data_r = (int64*) ARR_DATA_PTR(result);
						Assert(rtype == INT8OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = (float4)data_m[i] + data_n[i];
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						float8 *data_r = (float8*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT8OID);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + data_n[i];
						break;
					}
					default:
						Assert(false);
				}
				break;
			}
			case INT4OID:
			{
				int32 *data_m = (int32*) ARR_DATA_PTR(m);
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						int32 *data_r = (int32*) ARR_DATA_PTR(result);
						Assert(rtype == INT4OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						int32 *data_r = (int32*) ARR_DATA_PTR(result);
						Assert(rtype == INT4OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						int64 *data_r = (int64*) ARR_DATA_PTR(result);
						Assert(rtype == INT8OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = (float4)data_m[i] + data_n[i];
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						float8 *data_r = (float8*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT8OID);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + data_n[i];
						break;
					}
					default:
						Assert(false);
				}
				break;
			}
			case INT8OID:
			{
				int64 *data_m = (int64*) ARR_DATA_PTR(m);
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						int64 *data_r = (int64*) ARR_DATA_PTR(result);
						Assert(rtype == INT8OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						int64 *data_r = (int64*) ARR_DATA_PTR(result);
						Assert(rtype == INT8OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						int64 *data_r = (int64*) ARR_DATA_PTR(result);
						Assert(rtype == INT8OID);
						for (i = 0; i < len; i++){
							data_r[i] = data_m[i] + data_n[i];
							/* overflow checking */
							CHECKINTADD(data_r[i], data_m[i], data_n[i]);
						}
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = (float4)data_m[i] + data_n[i];
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						float8 *data_r = (float8*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT8OID);
						for (i = 0; i < len; i++)
							data_r[i] = (float4)data_m[i] + data_n[i];
						break;
					}
					default:
						Assert(false);
				}
				break;
			}
			case FLOAT4OID:
			{
				float4 *data_m = (float4*) ARR_DATA_PTR(m);
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + (float4)data_n[i];
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + (float4)data_n[i];
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + (float4)data_n[i];
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						float4 *data_r = (float4*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT4OID);
						for (i = 0; i < len; i++){
							/* flow checking */
							data_r[i] = data_m[i] + data_n[i];
							CHECKFLOATVAL(data_r[i], isinf(data_m[i] ) || isinf(data_n[i]), true);
						}
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						float8 *data_r = (float8*) ARR_DATA_PTR(result);
						Assert(rtype == FLOAT8OID);
						for (i = 0; i < len; i++){
							/* flow checking */
							data_r[i] = data_m[i] + data_n[i];
							CHECKFLOATVAL(data_r[i], isinf(data_m[i] ) || isinf(data_n[i]), true);
						}
						break;
					}
					default:
						Assert(false);
				}
				break;
			}
			case FLOAT8OID:
			{
				float8 *data_m = (float8*) ARR_DATA_PTR(m);
				float8 *data_r = (float8*) ARR_DATA_PTR(result);
				Assert(rtype == FLOAT8OID);
				switch (ntype)
				{
					case INT2OID:
					{
						int16 *data_n = (int16*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + data_n[i];
						break;
					}
					case INT4OID:
					{
						int32 *data_n = (int32*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + data_n[i];
						break;
					}
					case INT8OID:
					{
						int64 *data_n = (int64*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + (float4)data_n[i];
						break;
					}
					case FLOAT4OID:
					{
						float4 *data_n = (float4*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++)
							data_r[i] = data_m[i] + data_n[i];
						break;
					}
					case FLOAT8OID:
					{
						float8 *data_n = (float8*) ARR_DATA_PTR(n);
						for (i = 0; i < len; i++){
							/* flow checking */
							data_r[i] = data_m[i] + data_n[i];
							CHECKFLOATVAL(data_r[i], isinf(data_m[i] ) || isinf(data_n[i]), true);
						}
						break;
					}
					default:
						Assert(false);
				}
				break;
			}
			default:
				Assert(false);
		}
		PG_RETURN_ARRAYTYPE_P(result);
	} 
}
