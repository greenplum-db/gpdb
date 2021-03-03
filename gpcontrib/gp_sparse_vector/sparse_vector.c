/*-------------------------------------------------------------------------
 *
 * sparse_vector.c
 *
 * Sparse Vector Datatype.  We would like to store sparse arrays in a terse
 * representation that fits in a small amount of memory.  We also want to be
 * able to compare the number of instances where the svec of one document
 * intersects another.

 * Copyright (c) 2010, Greenplum Software
 * Portions Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_sparse_vector/sparse_vector.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>

#include "postgres.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "catalog/pg_type.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"

#include "sparse_vector.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/fmgroids.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"

/*
 * Input and Output routines
 */

ArrayType *svec_return_array_internal(SvecType *svec)
{
	ArrayType *pgarray;
	SparseData sdata=sdata_from_svec(svec);
	double *array=sdata_to_float8arr(sdata);

	pgarray = construct_array((Datum *)array,
		sdata->total_value_count,FLOAT8OID,
		sizeof(float8),true,'d');

	pfree(array);
	return(pgarray);
}

/* 
 * Must serialize for binary communication with libpq via
 * creating a StringInfo and sending individual data items like:
 *	(from backend/libpq/pqformat.c):
 *              pq_beginmessage - initialize StringInfo buffer
 *              pq_sendbyte             - append a raw byte to a StringInfo buffer
 *              pq_sendint              - append a binary integer to a StringInfo buffer
 *              pq_sendint64    - append a binary 8-byte int to a StringInfo buffer
 *              pq_sendfloat4   - append a float4 to a StringInfo buffer
 *              pq_sendfloat8   - append a float8 to a StringInfo buffer
 *              pq_sendbytes    - append raw data to a StringInfo buffer
 *              pq_sendcountedtext - append a counted text string (with character set conversion)
 *              pq_sendtext             - append a text string (with conversion)
 *              pq_sendstring   - append a null-terminated text string (with conversion)
 *              pq_send_ascii_string - append a null-terminated text string (without conversion)
 *              pq_endmessage   - send the completed message to the frontend
 *
 */

/*
 *        svec_send            - converts text to binary format
 */
PG_FUNCTION_INFO_V1(svec_send);
Datum
svec_send(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata = sdata_from_svec(svec);

	pq_begintypsend(&buf);
	pq_sendint(&buf,sdata->type_of_data,sizeof(Oid));
	pq_sendint(&buf,sdata->unique_value_count,sizeof(int));
	pq_sendint(&buf,sdata->total_value_count,sizeof(int));
	pq_sendint(&buf,sdata->vals->len,sizeof(int));
	pq_sendint(&buf,sdata->index->len,sizeof(int));
	pq_sendbytes(&buf,sdata->vals->data,sdata->vals->len);
	pq_sendbytes(&buf,sdata->index->data,sdata->index->len);

	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}
/*
 *        svec_recv            - converts external binary format to text
 */
PG_FUNCTION_INFO_V1(svec_recv);
Datum
svec_recv(PG_FUNCTION_ARGS)
{
	StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
	SvecType *svec;

	SparseData sdata=makeEmptySparseData();;
	sdata->type_of_data       = pq_getmsgint(buf, sizeof(int));
	sdata->unique_value_count = pq_getmsgint(buf, sizeof(int));
	sdata->total_value_count  = pq_getmsgint(buf, sizeof(int));
	sdata->vals->len          = pq_getmsgint(buf, sizeof(int));
	sdata->index->len         = pq_getmsgint(buf, sizeof(int));
	sdata->vals->data         = (char *)pq_getmsgbytes(buf,sdata->vals->len);
	sdata->index->data        = (char *)pq_getmsgbytes(buf,sdata->index->len);
	svec = svec_from_sparsedata(sdata,true); //Note that this copies the data
//	freeSparseDataAndData(sdata);
	pfree(sdata);

	PG_RETURN_SVECTYPE_P(svec);
}

/*
 * Basic floating point operators like MIN,MAX
 */
PG_FUNCTION_INFO_V1( float8_min );
Datum float8_min(PG_FUNCTION_ARGS);
Datum
float8_min(PG_FUNCTION_ARGS)
{
	float8 left = PG_GETARG_FLOAT8(0);
	float8 right = PG_GETARG_FLOAT8(1);
	PG_RETURN_FLOAT8(MIN(left,right));
}

PG_FUNCTION_INFO_V1( float8_max );
Datum float8_max(PG_FUNCTION_ARGS);
Datum
float8_max(PG_FUNCTION_ARGS)
{
	float8 left = PG_GETARG_FLOAT8(0);
	float8 right = PG_GETARG_FLOAT8(1);
	PG_RETURN_FLOAT8(MAX(left,right));
}

/*
 * Returns an uncompressed Array
 */
PG_FUNCTION_INFO_V1( svec_return_array );
Datum
svec_return_array(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *pgarray = svec_return_array_internal(svec);
	PG_RETURN_ARRAYTYPE_P(pgarray);
}

PG_FUNCTION_INFO_V1(svec_out);
Datum
svec_out(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	char *result = svec_out_internal(svec);
	PG_RETURN_CSTRING(result);
}

char *
svec_out_internal(SvecType *svec)
{
	char *ix_string,*vals_string,*result;
	int ixlen,vslen;
	SparseData sdata=sdata_from_svec(svec);
	int64 *array_ix =sdata_index_to_int64arr(sdata);
	ArrayType *pgarray_ix,*pgarray_vals;

	pgarray_ix = construct_array((Datum *)array_ix,
		sdata->unique_value_count,INT8OID,
		sizeof(int64),true,'d');

	ix_string=DatumGetPointer(OidFunctionCall1(F_ARRAY_OUT,
				PointerGetDatum(pgarray_ix)));
	ixlen = strlen(ix_string);

	pgarray_vals = construct_array((Datum *)sdata->vals->data,
		sdata->unique_value_count,FLOAT8OID,
		sizeof(float8),true,'d');
	vals_string=DatumGetPointer(OidFunctionCall1(F_ARRAY_OUT,
				PointerGetDatum(pgarray_vals)));
	vslen = strlen(vals_string);

	result = (char *)palloc(sizeof(char)*(vslen+ixlen+1+1));

	sprintf(result,"%s:%s",ix_string,vals_string);
	pfree(ix_string);
	pfree(vals_string);
	pfree(array_ix);

	return(result);
}

PG_FUNCTION_INFO_V1(svec_in);
Datum
svec_in(PG_FUNCTION_ARGS)
{
	char *str = pstrdup(PG_GETARG_CSTRING(0));
	char *values;
	ArrayType *pgarray_vals,*pgarray_ix;
	double *vals;
	StringInfo index;
	int64 *u_index;
	int32_t num_values,total_value_count;
	SparseData sdata;
	SvecType *result;

	/* Read in the two arrays defining the Sparse Vector, first is the array
	 * of run lengths, the second is an array of the unique values.
	 *
	 * The input format is a pair of standard Postgres arrays separated by a colon,
	 * like this:
	 * 	{1,10,1,5,1}:{4.3,0,0.2,0,7.4}
	 */

	if ((values=strchr(str,':'))==NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid input string for svec")));
	} else {
		*values = '\0';
		values = values+1;
	}

	pgarray_vals = DatumGetArrayTypeP(OidFunctionCall3(F_ARRAY_IN,CStringGetDatum(values),
				ObjectIdGetDatum(FLOAT8OID),Int32GetDatum(-1)));

	/* Make an empty StringInfo because we have the data array already */
	vals = (double *)ARR_DATA_PTR(pgarray_vals);
	num_values = *(ARR_DIMS(pgarray_vals));

	pgarray_ix = DatumGetArrayTypeP(OidFunctionCall3(F_ARRAY_IN,CStringGetDatum(str),
				ObjectIdGetDatum(INT8OID),Int32GetDatum(-1)));

	u_index = (int64 *)ARR_DATA_PTR(pgarray_ix);
	if (num_values != *(ARR_DIMS(pgarray_ix)))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unique value count not equal to run length count")));
	}

	index = makeStringInfo();
	total_value_count = 0;
	for (int i=0;i<num_values;i++)
	{
		total_value_count+=u_index[i];
		append_to_rle_index(index,u_index[i]);
	}

	sdata = makeInplaceSparseData((char *)vals,index->data,
			num_values*sizeof(double),index->len,FLOAT8OID,
			num_values,total_value_count);
	sdata->type_of_data = FLOAT8OID;

	result = svec_from_sparsedata(sdata,true);
	if (total_value_count==1) result->dimension = -1; //Scalar

	pfree(str); /* str is allocated from a strdup */
	pfree(pgarray_ix);
	pfree(pgarray_vals);

	PG_RETURN_SVECTYPE_P(result);
}

SvecType *svec_from_sparsedata(SparseData sdata,bool trim)
{
	int size;

	if (trim)
	{
		/*
		 * Trim the extra space off of the StringInfo dynamic strings
		 * before serializing the SparseData
		 */
		sdata->vals->maxlen=sdata->vals->len;
		sdata->index->maxlen=sdata->index->len;
	}

	size=SVECHDRSIZE + SIZEOF_SPARSEDATASERIAL(sdata);

	SvecType *result = (SvecType *)palloc(size);
	SET_VARSIZE(result,size);
	serializeSparseData(SVEC_SDATAPTR(result),sdata);
	result->dimension = sdata->total_value_count;
	if (result->dimension==1) result->dimension=-1; //Scalar
	return (result);
}

SvecType *svec_from_float8arr(float8 *array, int dimension)
{
	SparseData sdata = float8arr_to_sdata(array,dimension);
	SvecType *result = svec_from_sparsedata(sdata,true);
	return result;
}

SvecType *makeEmptySvec(int allocation)
{
	int val_len = sizeof(float8)*allocation+1;
	int ind_len = 9*allocation+1;
	SvecType *svec;
	SparseData sdata = makeEmptySparseData();
	sdata->vals->data    = (char *)palloc(val_len);
	sdata->vals->len = 0;
	sdata->vals->maxlen  = val_len;
	sdata->index->data   = (char *)palloc(ind_len);
	sdata->index->len = 0;
	sdata->index->maxlen = ind_len;
	svec = svec_from_sparsedata(sdata,false);
	freeSparseDataAndData(sdata);
	return(svec);
}

SvecType *reallocSvec(SvecType *source)
{
	SvecType *svec;
	SparseData sdata = sdata_from_svec(source);
	int val_newmaxlen = MAX(9,2*(sdata->vals->maxlen));
	char *newvals = (char *)palloc(val_newmaxlen);
	int ind_newmaxlen = MAX(10,2*(sdata->index->maxlen));
	char *newindex = (char *)palloc(ind_newmaxlen);
	/*
	 * This space was never allocated with palloc, so we can't repalloc it!
	 */
	memcpy(newvals ,sdata->vals->data ,sdata->vals->len);
	memcpy(newindex,sdata->index->data,sdata->index->len);
	sdata->vals->data [sdata->vals->len]  = '\0';
	sdata->index->data[sdata->index->len] = '\0';
	sdata->vals->data    = newvals;
	sdata->vals->maxlen  = val_newmaxlen;
	sdata->index->data   = newindex;
	sdata->index->maxlen = ind_newmaxlen;
	svec = svec_from_sparsedata(sdata,false);
//	pfree(source);
	return(svec);
}


typedef struct
{
	SvecType *svec;
	SparseData sdata;
	int dimension;
	int absolute_value_position;
	int unique_value_position;
	int run_position;
	char *index_position;
}	svec_unnest_fctx;

PG_FUNCTION_INFO_V1(svec_unnest);
Datum
svec_unnest(PG_FUNCTION_ARGS)
{
        FuncCallContext *funcctx;
        svec_unnest_fctx *fctx;
        MemoryContext oldcontext;
	float8 result;
	SvecType *svec;
	int run_length=0;

        /* stuff done only on the first call of the function */
        if (SRF_IS_FIRSTCALL())
        {
                /* create a function context for cross-call persistence */
                funcctx = SRF_FIRSTCALL_INIT();

                /*
                 * switch to memory context appropriate for multiple function calls
                 */
                oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                /* allocate memory for user context */
                fctx = (svec_unnest_fctx *)
			palloc(sizeof(svec_unnest_fctx));

		svec   = PG_GETARG_SVECTYPE_P(0);
		fctx->sdata = sdata_from_svec(svec);

		/* set initial index into the sparse vector argument */
		fctx->dimension               = svec->dimension;
		fctx->absolute_value_position = 0;
		fctx->unique_value_position   = 0;
		fctx->index_position          = fctx->sdata->index->data;
		fctx->run_position            = 1;

                funcctx->user_fctx = fctx;

                MemoryContextSwitchTo(oldcontext);
        }

        /* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

        fctx = funcctx->user_fctx;

	run_length = compword_to_int8(fctx->index_position);

	if (fctx->dimension > fctx->absolute_value_position)
	{
		result = ((float8 *)(fctx->sdata->vals->data))
			[fctx->unique_value_position];

		fctx->absolute_value_position++;
		fctx->run_position++;
		if (fctx->run_position > run_length)
		{
			fctx->run_position = 1;
			fctx->unique_value_position++;
			fctx->index_position += int8compstoragesize(fctx->index_position);
		}

                /* send the result */
                SRF_RETURN_NEXT(funcctx, Float8GetDatum(result));
        }
        else
	{
                /* do when there is no more left */
                SRF_RETURN_DONE(funcctx);
	}
		
}




/*
 * TODO
 *
 * "R" basic matrix routines
 * 	From: file:///opt/local/var/macports/software/R/2.6.2_0/opt/local/lib/R/doc/manual/R-intro.html
 * 		The function crossprod() forms “crossproducts”, meaning that crossprod(X, y) is the
 * 		same as t(X) %*% y but the operation is more efficient. If the second argument to
 * 		crossprod() is omitted it is taken to be the same as the first.
 *
 * 		The meaning of diag() depends on its argument. diag(v), where v is a vector, gives
 * 		a diagonal matrix with elements of the vector as the diagonal entries. On the other
 * 		hand diag(M), where M is a matrix, gives the vector of main diagonal entries of M.
 * 		This is the same convention as that used for diag() in Matlab. Also, somewhat
 * 		confusingly, if k is a single numeric value then diag(k) is the k by k identity matrix!
 *
 * 		5.7.2 Linear equations and inversion
 *
 * 		Solving linear equations is the inverse of matrix multiplication. When after
 *      		> b <- A %*% x
 *      	only A and b are given, the vector x is the solution of that linear equation system. In R,
 *           		> solve(A,b)
 *           	solves the system, returning x (up to some accuracy loss). Note that in linear algebra,
 *           	formally x = A^{-1} %*% b where A^{-1} denotes the inverse of A, which can be computed by
 *              solve(A) but rarely is needed. Numerically, it is both inefficient and potentially unstable
 *              to compute x <- solve(A) %*% b instead of solve(A,b).
 *
 *              The quadratic form x %*% A^{-1} %*% x which is used in multivariate computations, should
 *              be computed by something like16 x %*% solve(A,x), rather than computing the inverse of A.
 *
 */


