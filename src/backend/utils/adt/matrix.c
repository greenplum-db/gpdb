#include "postgres.h"
#include "funcapi.h"

#include <ctype.h>

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
 * check to see if a int64 val has overflow in addition
*
* Overflow check.	We basically check to see if result / arg2 gives arg1
* again.  There are two cases where this fails: arg2 = 0 (which cannot
* overflow) and arg1 = INT64_MIN, arg2 = -1 (where the division itself
* will overflow and thus incorrectly match).
*
* Since the division is likely much more expensive than the actual
* multiplication, we'd like to skip it where possible.  The best bang for
* the buck seems to be to check whether both inputs are in the int32
* range; if so, no overflow is possible.  (But that only works if we
* really have a 64-bit int64 datatype...)
*
*/
#define CHECKINT64MULT(result, arg1, arg2)												\
do {																					\
	if (!(arg1 >= (int32) INT_MIN && arg1 <= (int32) INT_MAX &&							\
		  arg2 >= (int32) INT_MIN && arg2 <= (int32) INT_MAX) &&						\
		arg2 != 0 &&																	\
		(result / arg2 != arg1 || (arg2 == -1 && arg1 < 0 && result < 0)))				\
		ereport(ERROR,																	\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),							\
		  errmsg("int value out of range: overflow")));			\
} while(0)


/*
 * matrix_transpose - transpose an array[x][y] into an array[y][x].
 */
Datum 
matrix_transpose(PG_FUNCTION_ARGS)
{
	ArrayType   *m, *result;
	Oid         eltype;
	int         elsize, size, ndim, databytes;
	int         i,j;
	int16       typlen;
	bool        typbyval;
	char        typalign;
	char       *data_m, *data_r;
	
    m = PG_GETARG_ARRAYTYPE_P(0);
	ndim = ARR_NDIM(m);
	
	/* Sanity check, transpose only valid for two dimensional arrays. */
	if (ndim != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("argument must be a two dimensional array")));
	if (m->dataoffset || ARR_NULLBITMAP(m))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("null array element not allowed in this context")));
	if (ARR_ELEMTYPE(m) == InvalidOid)
		ereport(ERROR, 
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("could not determine anyarray/anyelement type because "
						"input has type \"unknown\"")));
	
	/* datatype and size of input matrix */
	eltype = ARR_ELEMTYPE(m);
	size   = ARR_SIZE(m);
	databytes = size - ARR_DATA_OFFSET(m);
	
	/* Allocate the result matrix */
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	result->ndim = ndim;
	result->dataoffset = 0;      /* We disallowed arrays with NULLS */
	result->elemtype = eltype;
	ARR_DIMS(result)[1] = ARR_DIMS(m)[0];
	ARR_DIMS(result)[0] = ARR_DIMS(m)[1];
	ARR_LBOUND(result)[1] = ARR_LBOUND(m)[0];
	ARR_LBOUND(result)[0] = ARR_LBOUND(m)[1];
	
	/* 
	 * If one of the dimensions was length 1 then it is a simple transpose
	 * and we can directly copy the data 
	 */
	if (ARR_DIMS(m)[0] == 1 || ARR_DIMS(m)[1] == 1)
	{
		memcpy(ARR_DATA_PTR(result), ARR_DATA_PTR(m), databytes);
		PG_RETURN_ARRAYTYPE_P(result);
	}
	
	/* Otherwise we have a more complicated data reshuffle */
	get_typlenbyvalalign(eltype, &typlen, &typbyval, &typalign);
	elsize = att_align_nominal(typlen, typalign);
	data_m = ARR_DATA_PTR(m);
	data_r = ARR_DATA_PTR(result);
	for (i = 0; i < ARR_DIMS(m)[1]; i++)
	{
		for (j = 0; j < ARR_DIMS(m)[0]; j++)
		{
			int index_m = i + j * ARR_DIMS(m)[1];
			if (typlen > 0)
			{
				char *seek = data_m + (elsize * index_m);
				memcpy(data_r, seek, elsize);
				data_r = (char *) att_align_nominal(data_r+elsize, typalign);
			}
			else 
			{
				char *seek = data_m;
				int k;
				for (k = 0; k < index_m; k++) 
				{
					seek += VARSIZE(seek);
					seek = (char*) att_align_nominal(seek, typalign);
				}
				memcpy(data_r, seek, VARSIZE(seek));
				data_r = (char *) att_align_nominal(data_r+VARSIZE(seek), typalign);
			}
		}
	}
	Assert(data_r - ARR_DATA_PTR(result) == databytes);
	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * matrix_multipy - array multiplication of two input arrays
 */
Datum matrix_multiply(PG_FUNCTION_ARGS)
{
	ArrayType  *m, *n, *result;
	Oid         eltype;
	int         elsize, size;
	int         i,j,k;
	int16       typlen;
	bool        typbyval;
	char        typalign;
	char       *data_m, *data_n, *data_r;
	
    m = PG_GETARG_ARRAYTYPE_P(0);
    n = PG_GETARG_ARRAYTYPE_P(1);
	eltype = ARR_ELEMTYPE(m);
	
	/* Do all error checking */
	if (ARR_NDIM(m) != 2 || ARR_NDIM(n) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("argument must be a two dimensional array")));
	if (ARR_DIMS(m)[1] != ARR_DIMS(n)[0])
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("non-conformable arrays")));
	if (m->dataoffset || ARR_NULLBITMAP(m) ||
		n->dataoffset || ARR_NULLBITMAP(n))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("null array element not allowed in this context")));
	if (!OidIsValid(ARR_ELEMTYPE(m)) || !OidIsValid(ARR_ELEMTYPE(n)))
		ereport(ERROR, 
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("could not determine anyarray/anyelement type because "
						"input has type \"unknown\"")));
	if (ARR_ELEMTYPE(m) != ARR_ELEMTYPE(n))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot multiply arrays of different element types")));
	if (eltype != INT2OID && eltype != INT4OID && eltype != INT8OID &&
		eltype != FLOAT4OID && eltype != FLOAT8OID /* && eltype != NUMERICOID */)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("datatype not supported for array multiplication")));
	
	/* datatype and size of input matrix */
	get_typlenbyvalalign(eltype, &typlen, &typbyval, &typalign);
	elsize = att_align_nominal(typlen, typalign);
	
	/* Output array is always either int8[] or float8[] */
	size = ARR_OVERHEAD_NONULLS(2);  /* overhead for two dimensional array */
	size += ARR_DIMS(m)[0] * ARR_DIMS(n)[1] * sizeof(int64);
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	result->ndim = 2;
	result->dataoffset = 0;          /* We dissallowed arrays with NULLS */
	ARR_DIMS(result)[0] = ARR_DIMS(m)[0];
	ARR_DIMS(result)[1] = ARR_DIMS(n)[1];
	ARR_LBOUND(result)[0] = 1;       /* may need better lbound handling? */
	ARR_LBOUND(result)[1] = 1;
	switch (eltype) {
		case INT2OID:
		case INT4OID:
		case INT8OID:
			result->elemtype = INT8OID;
			break;
		case FLOAT4OID:
		case FLOAT8OID:
			result->elemtype = FLOAT8OID;
			break;
		default:
			Assert(false);
	}
	data_r = ARR_DATA_PTR(result);
	memset(data_r, 0, ARR_DIMS(m)[0] * ARR_DIMS(n)[1] * sizeof(int64));

	/*
	 * Integer Overflow Detecting: We should check arithmetic overflow
	 * of int64 and float8; Below local variable is used to restore temporary
	 * arithmetic result.
	 *
	 */
	int64 int64_mul, int64_add;
	float8 float8_mul, float8_add;

	/*
	 * Three dimensional loop over result arrays two dimensional space and
	 * over the intersection dimension of the two input matrices.
	 * Locality is preserved for the m matrix.
	 *
	 * This will need to be reworked for variable length numeric elements.
	 */
	for (i = 0; i < ARR_DIMS(result)[0]; i++)
	{
		for (j = 0; j < ARR_DIMS(result)[1]; j++)
		{
			data_m = ARR_DATA_PTR(m) + i * ARR_DIMS(m)[1] * elsize;
			for (k = 0; k < ARR_DIMS(m)[1]; k++)
			{
				data_n = ARR_DATA_PTR(n) + k * ARR_DIMS(n)[1] * elsize;
				switch (eltype) {
					case INT2OID:
						/*
						 * int16 * int16 -> int64 will never overflow
						 * int64 + int64 -> int64 will overflow, need detection
						 */
						int64_mul = ((int16*)data_n)[j] * ((int16*)data_m)[k];
						int64_add = *(int64*)data_r + int64_mul;
						CHECKINTADD(int64_add, *(int64*)data_r, int64_mul);
						*(int64*)data_r = int64_add;
						break;
					case INT4OID:
						/*
						 * int32 * int32 -> int64 will never overflow
						 * int64 + int64 -> int64 will overflow, need detection
						 */
						int64_mul = ((int32*)data_n)[j] * ((int32*)data_m)[k];
						int64_add = *(int64*)data_r + int64_mul;
						CHECKINTADD(int64_add, *(int64*)data_r, int64_mul);
						*(int64*)data_r = int64_add;
						break;
					case INT8OID:
						/*
						 * int64 * int64 -> int64 could overflow, need detection
						 * int64 + int64 -> int64 will overflow, need detection
						 */
						int64_mul = ((int64*)data_n)[j] * ((int64*)data_m)[k];
						int64_add = *(int64*)data_r + int64_mul;
						CHECKINT64MULT(int64_mul, ((int64*)data_n)[j], ((int64*)data_m)[k]);
						CHECKINTADD(int64_add, *(int64*)data_r, int64_mul);
						*(int64*)data_r = int64_add;
						break;
					case FLOAT4OID:
						float8_mul = ((float*)data_n)[j] * ((float*)data_m)[k];
						float8_add = *(float8*)data_r + float8_mul;
						/*
						 * check overflow of multiply
						 */
						CHECKFLOATVAL(float8_mul, isinf(((float*)data_n)[j]) || isinf(((float*)data_m)[k]),
								((float*)data_n)[j] == 0 || ((float*)data_m)[k] == 0);
						CHECKFLOATVAL(float8_add, isinf(*(float8*)data_r) || isinf(float8_mul), true);
						*(float8*)data_r = float8_add;
						break;
					case FLOAT8OID:
						float8_mul = ((float8*)data_n)[j] * ((float8*)data_m)[k];
						float8_add = *(float8*)data_r + float8_mul;
						/*
						 * check overflow of multiply
						 */
						CHECKFLOATVAL(float8_mul, isinf(((float8*)data_n)[j]) || isinf(((float8*)data_m)[k]),
								((float8*)data_n)[j] == 0 || ((float8*)data_m)[k] == 0);
						CHECKFLOATVAL(float8_add, isinf(*(float8*)data_r) || isinf(float8_mul), true);
						*(float8*)data_r = float8_add;
						break;
					default:
						Assert(false);
				}
			}
			data_r += 8;
		}
	}
	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * matrix_add - array summation over two input arrays
 */
Datum matrix_add(PG_FUNCTION_ARGS)
{
	ArrayType  *m, *n;
	Oid         mtype, ntype;
	int         i, ndim, len;
	bool        transition_function;
	
	/* If we're in a transition function we can be smarter */
	transition_function = fcinfo->context && IsA(fcinfo->context, AggState);
	
	/* Validate arguments */
	if (PG_NARGS() != 2) 
    {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("matrix_add called with %d arguments", PG_NARGS())));
	}   
	
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
		get_call_result_type(fcinfo, &returntype, &tupdesc); 
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


/*
 * int8_matrix_smultiply - scalar multiple of input array by an int8
 */
Datum int8_matrix_smultiply(PG_FUNCTION_ARGS)
{
	ArrayType  *m, *result;
	int64      *data_r;
	int64       scalar;
	int         size, nelem, ndim;
	int         i;
	
	if (PG_NARGS() != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("matrix_smultiply_int8 called with %d arguments", 
						PG_NARGS())));
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();
	
	scalar = PG_GETARG_INT64(1);
    m = PG_GETARG_ARRAYTYPE_P(0);
	ndim = ARR_NDIM(m);
	
	/* Do all error checking */
	if (ARR_ELEMTYPE(m) != INT8OID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("matrix_smultiply_int8 datatype mismatch")));
	
	/* count number of elements */
	for (nelem = 1, i = 0; i < ndim; i++)
		nelem *= ARR_DIMS(m)[i];
	
	/* Make a copy of the input matrix */
	size = VARSIZE(m);
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	memcpy(result, m, size);
	
	/* And multiply by the scalar in place */
	data_r = (int64*) ARR_DATA_PTR(result);
	int64 mult_r;
	for (i = 0; i < nelem; i++){
		/* flow checking */
		mult_r = data_r[i] * scalar;
		CHECKINT64MULT(mult_r, data_r[i], scalar);
		data_r[i] = mult_r;
	}
	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * float8_matrix_smultiply - scalar multiple of input array by a float8
 */
Datum float8_matrix_smultiply(PG_FUNCTION_ARGS)
{
	ArrayType  *m, *result;
	float8     *data_r;
	float8      scalar;
	int         size, nelem, ndim;
	int         i;
	
	if (PG_NARGS() != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("matrix_smultiply_float8 called with %d arguments", 
						PG_NARGS())));
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();
	
	scalar = PG_GETARG_FLOAT8(1);
    m = PG_GETARG_ARRAYTYPE_P(0);
	ndim = ARR_NDIM(m);
	
	/* Do all error checking */
	if (ARR_ELEMTYPE(m) != FLOAT8OID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("matrix_smultiply_float8 datatype mismatch")));
	
	/* count number of elements */
	for (nelem = 1, i = 0; i < ndim; i++)
		nelem *= ARR_DIMS(m)[i];
	
	/* Make a copy of the input matrix */
	size = VARSIZE(m);
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	memcpy(result, m, size);
	
	/* And multiply by the scalar in place */
	data_r = (float8*) ARR_DATA_PTR(result);
	float8 mult_r;
	for (i = 0; i < nelem; i++){
		mult_r = data_r[i] * scalar;
		CHECKFLOATVAL(mult_r, isinf(data_r[i]) || isinf(scalar),
				data_r[i] == 0 || scalar == 0);
		data_r[i] = mult_r;
	}
	PG_RETURN_ARRAYTYPE_P(result);
}


