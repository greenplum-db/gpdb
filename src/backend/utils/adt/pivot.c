#include "postgres.h"
#include "funcapi.h"

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

/* array primitives for looping that should have already existed */
typedef struct _array_iter {
	ArrayType  *array;
	char       *ptr;
	int32       index;
	int32       max;
	int16       typlen;
	bool        typbyval;
	char        typalign;
} array_iter;
void array_loop(ArrayType *array, int32 start, array_iter *iter);
bool array_next(array_iter *iter, Datum *value, bool *isna);


/* Internal static helper functions */
static Datum oid_pivot_accum(FunctionCallInfo fcinfo, Oid type);


/*
 * External facing wrapper functions to allow for proper type extraction
 */
Datum 
int4_pivot_accum(PG_FUNCTION_ARGS)
{
	return oid_pivot_accum(fcinfo, INT4OID);
}
Datum 
int8_pivot_accum(PG_FUNCTION_ARGS)
{
	return oid_pivot_accum(fcinfo, INT8OID);
}
Datum 
float8_pivot_accum(PG_FUNCTION_ARGS)
{
	return oid_pivot_accum(fcinfo, FLOAT8OID);
}


void array_loop(ArrayType *array, int32 start, array_iter *iter)
{
	iter->array = array;
	iter->ptr   = ARR_DATA_PTR(array);
	iter->max   = ARR_DIMS(array)[0];
	get_typlenbyvalalign(ARR_ELEMTYPE(array), 
						 &iter->typlen,
						 &iter->typbyval,
						 &iter->typalign);

	/* If we are starting in the middle of the array, then scan forward */
	start = start - ARR_LBOUND(array)[0];
	if (start <= 0)
		iter->index = start;
	else
	{
		/* 
		 * could probably be more efficient for fixed length arrays, but
		 * they would still require adjustments for nulls.
		 */
		iter->index = 0;
		while (start--)
		{
			Datum d;
			bool  isna;
			array_next(iter, &d, &isna);
		}
	}
}

bool array_next(array_iter *iter, Datum *value, bool *isna)
{
	bits8 *nulls;

	if (iter->index >= iter->max)
	{
		*value = (Datum) 0;
		*isna  = true;
		return false;
	}
	
	if (iter->index < 0)
	{
		*value = (Datum) 0;
		*isna  = true;
		iter->index++;
		return true;
	}
	
	nulls = ARR_NULLBITMAP(iter->array);
	if (nulls && !(nulls[iter->index / 8] & (1 << (iter->index % 8))))
	{
		*value = (Datum) 0;
		*isna  = true;
		iter->index++;
		return true;
	}

	*isna = false;
	
	if (iter->typlen > 0)
	{ /* fixed length */

		if (iter->typlen <= 8)
		{
			switch (iter->typlen)
			{
				case 1:
					*value = Int8GetDatum(*((int8*) iter->ptr));
					break;
				case 2:
					*value = Int16GetDatum(*((int16*) iter->ptr));
					break;
				case 4:
					*value = Int32GetDatum(*((int16*) iter->ptr));
					break;
				case 8:
					*value = Int64GetDatum(*((int16*) iter->ptr));
					break;
					
				default:
					elog(ERROR, "unexpected data type");
					break;
			}
		}
		else
		{
			*value = PointerGetDatum(iter->ptr);			
		}
		iter->ptr += iter->typlen;
	}
	else
	{ /* variable length */
		*value = PointerGetDatum(iter->ptr);
		iter->ptr += VARSIZE(iter->ptr);
	}
	iter->ptr = (char*) att_align_nominal(iter->ptr, iter->typalign);
	iter->index++;
	return true;
}



/* 
 * pivot_find() - Searchs an array of labels for a matching value.
 *
 * Returns: index of found value, or -1 if not found
 *
 * It may eventually do something smarter than a linear scan, but
 * for now this is sufficient given that we don't know anything about the
 * order of 'labels'.
 *
 */
static int pivot_find(ArrayType *labels, text *attr) 
{
	char    *labelsp;
	int      i, nelem, asize;
	int16    typlen;
	bool     typbyval;
	char     typalign;
	
	if (ARR_ELEMTYPE(labels) != TEXTOID)
		ereport(ERROR, 
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("pivot_accum: labels are not type text")));

	/* Text alignment properties */
	get_typlenbyvalalign(TEXTOID, &typlen, &typbyval, &typalign);
	
	/* Get the size of the input attribute, we'll use this for fast compares */
	asize = VARSIZE(attr);
	
	/* The labels array is an array of varying length text, scan it adding
	   the length of the previous entry until we are done or we have found
	   a match. */
	labelsp = (char *) ARR_DATA_PTR(labels);
	nelem = ARR_DIMS(labels)[0];
	for (i = 0; i < nelem; i++)
	{
		int lsize = VARSIZE(labelsp);
		if (asize == lsize && !memcmp(attr, labelsp, lsize))
			return i;  /* Found */
		labelsp  = labelsp + lsize;
		labelsp  = (char*) att_align_nominal(labelsp, typalign);
	}
	return -1;  /* Not found */
}


/*
 * pivot_accum() - Pivot and accumulate
 */
static Datum oid_pivot_accum(FunctionCallInfo fcinfo, Oid type)
{
	ArrayType  *data;
	ArrayType  *labels;
	text       *attr;
	int         i;
	
	/* Simple argument validation */
	if (PG_NARGS() != 4)
		ereport(ERROR, 
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("pivot_accum called with %d input arguments",
						PG_NARGS())));
	if (PG_ARGISNULL(1) || PG_ARGISNULL(2) || PG_ARGISNULL(3))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}
    labels = PG_GETARG_ARRAYTYPE_P(1);
	attr   = PG_GETARG_TEXT_P(2);
	
	/* Do nothing if the attr isn't in the labels array. */
	if ((i = pivot_find(labels, attr)) < 0)
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}
	
	/* Get the data array, or make it if null */
	if (!PG_ARGISNULL(0))
	{
		data = PG_GETARG_ARRAYTYPE_P(0);
		Assert(ARR_DIMS(labels)[0] == ARR_DIMS(data)[0]);
	}
	else
	{
		int elsize, size, nelem;
		Oid eltype = type;

		switch (type) {
			case INT4OID:
				/* addition of two int4 should be int8, otherwise it may cause overflow in addition*/
				elsize = 8;
				eltype = INT8OID;
				break;
			case INT8OID:
			case FLOAT8OID:
				elsize = 8; 
				break;
			default:
				elsize = 0;  /* Fixes complier warnings */
				Assert(false);
		}
		nelem = ARR_DIMS(labels)[0];
		size = nelem * elsize + ARR_OVERHEAD_NONULLS(1);
		data = (ArrayType *) palloc(size);
		SET_VARSIZE(data, size);
		data->ndim = 1;
		data->dataoffset = 0;
		data->elemtype = eltype;
		ARR_DIMS(data)[0] = nelem;
		ARR_LBOUND(data)[0] = 1;
		memset(ARR_DATA_PTR(data), 0, nelem * elsize);
	}
	
	
	/*   
	 * Should we think about upconverting the arrays? Or is the assumption that 
	 * the pivot isn't usually doing much aggregation?
	 */
	switch (type) {
		case INT4OID:
		{
			int64 *datap = (int64*) ARR_DATA_PTR(data);
			int32  value = PG_GETARG_INT32(3);
			datap[i] += value;
			break;
		}
		case INT8OID:
		{
			int64 *datap = (int64*) ARR_DATA_PTR(data);
			int64  value = PG_GETARG_INT64(3);
			datap[i] += value;
			break;
		}
		case FLOAT8OID:
		{
			float8 *datap = (float8*) ARR_DATA_PTR(data);
			float8  value = PG_GETARG_FLOAT8(3);
			datap[i] += value;
			break;
		}
		default:
			Assert(false);
	}
	PG_RETURN_ARRAYTYPE_P(data);
}
