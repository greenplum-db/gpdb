/*-------------------------------------------------------------------------
 *
 * operators.c
 *
 * Copyright (c) 2010, Greenplum Software
 * Portions Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_sparse_vector/operators.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "postgres.h"
#include "utils/array.h"
#include "catalog/pg_type.h"
#include "utils/numeric.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "access/hash.h"

#include "sparse_vector.h"

void check_dimension(SvecType *svec1, SvecType *svec2, char *msg);

Datum svec_dimension(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_dimension );

Datum
svec_dimension(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	PG_RETURN_INT32(svec->dimension);
}

Datum svec_concat_replicate(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_concat_replicate);

Datum
svec_concat_replicate(PG_FUNCTION_ARGS)
{
	int multiplier = PG_GETARG_INT32(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec);
	SparseData sdata = makeEmptySparseData();
	char *vals,*index;
	int l_val_len = left->vals->len;
	int l_ind_len = left->index->len;
	int val_len=l_val_len*multiplier;
	int ind_len=l_ind_len*multiplier;

	vals = (char *)palloc(sizeof(char)*val_len);
	index = (char *)palloc(sizeof(char)*ind_len);

	for (int i=0;i<multiplier;i++)
	{
		memcpy(vals+i*l_val_len,left->vals->data,l_val_len);
		memcpy(index+i*l_ind_len,left->index->data,l_ind_len);
	}

	sdata->vals  = makeStringInfoFromData(vals,val_len);
	sdata->index = makeStringInfoFromData(index,ind_len);
	sdata->type_of_data = left->type_of_data;
	sdata->unique_value_count = multiplier * left->unique_value_count;
	sdata->total_value_count  = multiplier * left->total_value_count;

	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(sdata,true));
}


Datum svec_concat(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_concat );
Datum
svec_concat(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0) && (!PG_ARGISNULL(1)))
	{
		PG_RETURN_SVECTYPE_P(PG_GETARG_SVECTYPE_P(1));
	} else if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
	{
		PG_RETURN_NULL();
	} else if (PG_ARGISNULL(1))
	{
		PG_RETURN_SVECTYPE_P(PG_GETARG_SVECTYPE_P(0));
	} else
	{

		SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
		SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
		SparseData left  = sdata_from_svec(svec1);
		SparseData right = sdata_from_svec(svec2);
		SparseData sdata = makeEmptySparseData();
		char *vals,*index;
		int l_val_len = left->vals->len;
		int r_val_len = right->vals->len;
		int l_ind_len = left->index->len;
		int r_ind_len = right->index->len;
		int val_len=l_val_len+r_val_len;
		int ind_len=l_ind_len+r_ind_len;

		vals = (char *)palloc(sizeof(char)*val_len);
		index = (char *)palloc(sizeof(char)*ind_len);

		memcpy(vals          ,left->vals->data,l_val_len);
		memcpy(vals+l_val_len,right->vals->data,r_val_len);
		memcpy(index,          left->index->data,l_ind_len);
		memcpy(index+l_ind_len,right->index->data,r_ind_len);

		sdata->vals  = makeStringInfoFromData(vals,val_len);
		sdata->index = makeStringInfoFromData(index,ind_len);
		sdata->type_of_data = left->type_of_data;
		sdata->unique_value_count = left->unique_value_count+
			right->unique_value_count;
		sdata->total_value_count  = left->total_value_count+
			right->total_value_count;

		PG_RETURN_SVECTYPE_P(svec_from_sparsedata(sdata,true));
	}
}


Datum svec_eq(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_eq );
Datum
svec_eq(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	PG_RETURN_BOOL(sparsedata_eq(left,right));
}

static int32_t
svec_l2_cmp_internal(SvecType *svec1, SvecType *svec2)
{
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	double magleft  = l2norm_sdata_values_double(left);
	double magright = l2norm_sdata_values_double(right);
	int result;
	if (magleft < magright)
	{
		result = -1;
	}
	else if (magleft > magright)
	{
		result = 1;
	}
	else
	{
		result = 0;
	}
	PG_RETURN_INT32(result);
}
Datum svec_l2_cmp(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_cmp );
Datum svec_l2_cmp(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	PG_RETURN_INT32(svec_l2_cmp_internal(svec1,svec2));
}
Datum svec_l2_lt(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_lt );
Datum svec_l2_lt(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL((result == -1) ? 1 : 0);
}
Datum svec_l2_le(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_le );
Datum svec_l2_le(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL(((result == -1)||(result == 0)) ? 1 : 0);
}
Datum svec_l2_eq(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_eq );
Datum svec_l2_eq(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL((result == 0) ? 1 : 0);
}
Datum svec_l2_ne(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_ne );
Datum svec_l2_ne(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL((result != 0) ? 1 : 0);
}
Datum svec_l2_gt(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_gt );
Datum svec_l2_gt(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL((result == 1) ? 1 : 0);
}
Datum svec_l2_ge(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_l2_ge );
Datum svec_l2_ge(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	int result = svec_l2_cmp_internal(svec1,svec2);
	PG_RETURN_BOOL(((result == 0) || (result == 1)) ? 1 : 0);
}

/*
 * Do one of subtract, add, multiply, divide or modulo depending on value
 * of operation(0,1,2,3,4)
 */
SvecType *
svec_operate_on_sdata_pair(int scalar_args,int operation,SparseData left,SparseData right)
{
	SparseData sdata=NULL;
	float8 *left_vals =(float8 *)(left->vals->data);
	float8 *right_vals=(float8 *)(right->vals->data);
	float8 data_result;

	switch(scalar_args) {
		case 0: 		//neither arg is scalar
			sdata = op_sdata_by_sdata(operation,left,right);
			break;
		case 1:			//left arg is scalar
			
			sdata = op_sdata_by_scalar_copy(operation,(char *)left_vals,
					right,1);
			break;
		case 2:			//right arg is scalar
			sdata = op_sdata_by_scalar_copy(operation,(char *)right_vals,
					left,2);
			break;
		case 3:			//both args are scalar
			switch (operation)
			{
				case 1:
				default:
					data_result = left_vals[0]+right_vals[0];
					break;
				case 2:
					data_result = left_vals[0]*right_vals[0];
					break;
				case 3:
					data_result = left_vals[0]/right_vals[0];
					break;
				case 4:
					data_result = ((int)left_vals[0])%((int)right_vals[0]);
					break;
			}
			return(svec_make_scalar(data_result,1));
			break;
	}
	return(svec_from_sparsedata(sdata,true));
}


SvecType *
op_svec_by_svec_internal(int operation, SvecType *svec1, SvecType *svec2)
{
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);

	int scalar_args=check_scalar(IS_SCALAR(svec1),IS_SCALAR(svec2));

	return(svec_operate_on_sdata_pair(scalar_args,operation,left,right));

}

/*
 * Do exponentiation, only makes sense if the left is a vector and the right
 * is a scalar or if both are scalar
 */
static SvecType *
pow_svec_by_scalar_internal(SvecType *svec1, SvecType *svec2)
{
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	SparseData sdata=NULL;
	double *left_vals=(double *)(left->vals->data);
	double *right_vals=(double *)(right->vals->data);
	double data_result;

	int scalar_args=check_scalar(IS_SCALAR(svec1),IS_SCALAR(svec2));

	switch(scalar_args) {
		case 0: 		//neither arg is scalar
		case 1:			//left arg is scalar
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("svec exponentiation is undefined when the right argument is a vector")));
			break;
		case 2:			//right arg is scalar
			if (right_vals[0] == 2.) //recognize the squared case as special
			{
			  sdata = square_sdata(left);
			}  else if (right_vals[0] == 3.) //recognize the cubed case as special
			{
			  sdata = cube_sdata(left);
			}  else if (right_vals[0] == 4.) //recognize the quad case as special
			{
			  sdata = quad_sdata(left);
			} else {
			  sdata = pow_sdata_by_scalar(left,(char *)right_vals);
			}
			break;
		case 3:			//both args are scalar
			data_result = pow(left_vals[0],right_vals[0]);
			return(svec_make_scalar(data_result,1));
			break;
	}

	return(svec_from_sparsedata(sdata,true));
}

PG_FUNCTION_INFO_V1( svec_pow );
Datum
svec_pow(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	check_dimension(svec1,svec2,"svec_pow");
	SvecType *result = pow_svec_by_scalar_internal(svec1,svec2);
	PG_RETURN_SVECTYPE_P(result);
}
PG_FUNCTION_INFO_V1( svec_minus );
Datum
svec_minus(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	check_dimension(svec1,svec2,"svec_minus");
	SvecType *result = op_svec_by_svec_internal(0,svec1,svec2);
	PG_RETURN_SVECTYPE_P(result);
}
PG_FUNCTION_INFO_V1( svec_plus );
Datum
svec_plus(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	check_dimension(svec1,svec2,"svec_plus");
	SvecType *result = op_svec_by_svec_internal(1,svec1,svec2);
	PG_RETURN_SVECTYPE_P(result);
}
PG_FUNCTION_INFO_V1( svec_mult );
Datum
svec_mult(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	check_dimension(svec1,svec2,"svec_mult");
	SvecType *result = op_svec_by_svec_internal(2,svec1,svec2);
	PG_RETURN_SVECTYPE_P(result);
}
PG_FUNCTION_INFO_V1( svec_div );
Datum
svec_div(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	check_dimension(svec1,svec2,"svec_div");
	SvecType *result = op_svec_by_svec_internal(3,svec1,svec2);
	PG_RETURN_SVECTYPE_P(result);
}

/*
 * Count the number of non-zero entries in the input vector
 * Right argument is capped at 1, then added to the left
 */
PG_FUNCTION_INFO_V1( svec_count );

Datum
svec_count(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	double *right_vals=(double *)(right->vals->data);
	SvecType *result;
	double *clamped_vals;
	SparseData right_clamped,sdata_result;

	int scalar_args=check_scalar(IS_SCALAR(svec1),IS_SCALAR(svec2));
	check_dimension(svec1,svec2,"svec_count");

	/* Clamp the right vector values to 1.
	 */
	switch (scalar_args)
	{
		case 1:			//left arg is scalar
			/*
			 * If the left argument is a scalar, this is almost certainly the
			 * first call to the routine, and we need a zero vector for the
			 * beginning of the accumulation of the correct dimension.
			 */
			left = makeSparseDataFromDouble(0.,right->total_value_count);
			/* FALLTHROUGH */

		case 0: 		//neither arg is scalar
		case 2:			//right arg is scalar

			/* Create an array of values either 1 or 0 depending on whether
			 * the right vector has a non-zero value in it
			 */
			clamped_vals =
				(double *)palloc0(sizeof(double)*(right->unique_value_count));

			for (int i=0;i<(right->unique_value_count);i++)
			{
				if (right_vals[i]!=0.) clamped_vals[i]=1.;
			}
			right_clamped = makeInplaceSparseData((char *)clamped_vals,right->index->data,
					right->vals->len,right->index->len,FLOAT8OID,
					right->unique_value_count,right->total_value_count);

			/* Create the output SVEC */
			sdata_result = op_sdata_by_sdata(1,left,right_clamped);
			result = svec_from_sparsedata(sdata_result,true);

			pfree(clamped_vals);
			pfree(right_clamped);

			PG_RETURN_SVECTYPE_P(result);
			break;
		case 3:			//both args are scalar
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("svec count is undefined when both arguments are scalar")));
			PG_RETURN_SVECTYPE_P(svec1);
			break;
	}
}

PG_FUNCTION_INFO_V1( svec_dot );

Datum
svec_dot(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	SparseData mult_result;
	double accum;
	check_dimension(svec1,svec2,"svec_dot");

	mult_result = op_sdata_by_sdata(2,left,right);
	accum = sum_sdata_values_double(mult_result);
	freeSparseDataAndData(mult_result);

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_l2norm );

Datum
svec_l2norm(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = l2norm_sdata_values_double(sdata);

	PG_RETURN_FLOAT8(accum);
}


PG_FUNCTION_INFO_V1( svec_l1norm );

Datum
svec_l1norm(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = l1norm_sdata_values_double(sdata);

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_summate );

Datum
svec_summate(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = sum_sdata_values_double(sdata);

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_log );

Datum
svec_log(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P_COPY(0);
	double *vals = (double *)SVEC_VALS_PTR(svec);
	int unique_value_count=SVEC_UNIQUE_VALCNT(svec);

	for (int i=0;i<unique_value_count;i++) vals[i] = log(vals[i]);

	PG_RETURN_SVECTYPE_P(svec);

}

/*
 * Cast from int2,int4,int8,float4,float8 scalar to SvecType
 */
PG_FUNCTION_INFO_V1( svec_cast_int2 );
Datum svec_cast_int2(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT16(0);
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}
PG_FUNCTION_INFO_V1( svec_cast_int4 );
Datum svec_cast_int4(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT32(0);
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}
PG_FUNCTION_INFO_V1( svec_cast_int8 );
Datum svec_cast_int8(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT64(0);
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}
PG_FUNCTION_INFO_V1( svec_cast_float4 );
Datum svec_cast_float4(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_FLOAT4(0);
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}
PG_FUNCTION_INFO_V1( svec_cast_float8 );
Datum svec_cast_float8(PG_FUNCTION_ARGS) {
	float8 value=PG_GETARG_FLOAT8(0);
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}
PG_FUNCTION_INFO_V1( svec_cast_numeric );
Datum svec_cast_numeric(PG_FUNCTION_ARGS) {
	Datum num=PG_GETARG_DATUM(0);
	float8 value;
	value = DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow,num));
	PG_RETURN_SVECTYPE_P(svec_make_scalar(value,1));
}

/*
 * Cast from int2,int4,int8,float4,float8 scalar to float8[]
 */
PG_FUNCTION_INFO_V1( float8arr_cast_int2 );
Datum float8arr_cast_int2(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT16(0);
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}
PG_FUNCTION_INFO_V1( float8arr_cast_int4 );
Datum float8arr_cast_int4(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT32(0);
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}
PG_FUNCTION_INFO_V1( float8arr_cast_int8 );
Datum float8arr_cast_int8(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_INT64(0);
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}
PG_FUNCTION_INFO_V1( float8arr_cast_float4 );
Datum float8arr_cast_float4(PG_FUNCTION_ARGS) {
	float8 value=(float8 )PG_GETARG_FLOAT4(0);
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}
PG_FUNCTION_INFO_V1( float8arr_cast_float8 );
Datum float8arr_cast_float8(PG_FUNCTION_ARGS) {
	float8 value=PG_GETARG_FLOAT8(0);
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}
PG_FUNCTION_INFO_V1( float8arr_cast_numeric );
Datum float8arr_cast_numeric(PG_FUNCTION_ARGS) {
	Datum num=PG_GETARG_DATUM(0);
	float8 value;
	value = DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow,num));
	PG_RETURN_ARRAYTYPE_P(svec_return_array_internal(svec_make_scalar(value,1)));
}


SvecType *svec_make_scalar(float8 value, int dimension) {
	SparseData sdata=float8arr_to_sdata(&value,1);
	SvecType *result=svec_from_sparsedata(sdata,true);
	result->dimension=-dimension;
	return result;
}

void check_dimension(SvecType *svec1, SvecType *svec2, char *msg) {
	/* If the array dimensions aren't the same, operation has no meaning unless one of 
	 * the inputs is a constant
	 */
	if ((!IS_SCALAR(svec1)) &&
	    (!IS_SCALAR(svec2)) &&
			(svec1->dimension != svec2->dimension)) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("%s: array dimension of inputs are not the same: dim1=%d, dim2=%d\n",
						msg, svec1->dimension, svec2->dimension)));
	}
}

PG_FUNCTION_INFO_V1( svec_cast_float8arr );

Datum
svec_cast_float8arr(PG_FUNCTION_ARGS) {
	ArrayType *A_PG = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *output_svec;

	if (ARR_ELEMTYPE(A_PG) != FLOAT8OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("svec_cast_float8arr only defined over float8[]")));
	if (ARR_NDIM(A_PG) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("svec_cast_float8arr only defined over 1 dimensional arrays")));

	if (ARR_NULLBITMAP(A_PG))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("svec_cast_float8arr does not allow null bitmaps on arrays")));


	/* Extract array */
	{
		int dimension = ARR_DIMS(A_PG)[0];
		float8 *array = (float8 *)ARR_DATA_PTR(A_PG);

		/* Create the output SVEC */
		SparseData sdata = float8arr_to_sdata(array,dimension);
		output_svec = svec_from_sparsedata(sdata,true);
	}

	PG_RETURN_SVECTYPE_P(output_svec);
}

/*
 * Provide some operators for Postgres FLOAT8OID arrays
 */
/*
 * Equality
 */
static bool
float8arr_equals_internal(ArrayType *left, ArrayType *right)
{
        int dimleft = ARR_NDIM(left), dimright = ARR_NDIM(right);
        int *dimsleft = ARR_DIMS(left), *dimsright = ARR_DIMS(right);
	int numleft = ArrayGetNItems(dimleft,dimsleft);
	int numright = ArrayGetNItems(dimright,dimsright);
        double *vals_left=(double *)ARR_DATA_PTR(left), *vals_right=(double *)ARR_DATA_PTR(right);
        bits8 *bitmap_left=ARR_NULLBITMAP(left), *bitmap_right=ARR_NULLBITMAP(right);
        int   bitmask=1;

        if ((dimsleft!=dimsright) || (numleft!=numright))
	{
		return(false);
	}

	/*
	 * Note that we are only defined for FLOAT8OID
	 */
        //get_typlenbyvalalign(ARR_ELEMTYPE(array),
        //                                         &typlen, &typbyval, &typalign);

	/*
	 * First we'll check to see if the null bitmaps are equivalent
	 */
	if (bitmap_left)
		if (! bitmap_right) return(false);
	if (bitmap_right)
		if (! bitmap_left) return(false);

	if (bitmap_left)
	{
        	for (int i=0; i<numleft; i++)
		{
                	if ((*bitmap_left & bitmask) == 0)
                		if ((*bitmap_left & bitmask) != 0)
			  		return(false);
                        bitmask <<= 1;
                        if (bitmask == 0x100)
                        {
                                bitmap_left++;
                                bitmask = 1;
                        }
		}
	}

	/*
	 * Now we check for equality of all array values
	 */
       	for (int i=0; i<numleft; i++)
		if (vals_left[i] != vals_right[i]) return(false);

        return(true);
}

Datum float8arr_equals(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_equals);

Datum
float8arr_equals(PG_FUNCTION_ARGS) {
	ArrayType *left  = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *right = PG_GETARG_ARRAYTYPE_P(1);

	PG_RETURN_BOOL(float8arr_equals_internal(left,right));
}

/*
 * Returns a SparseData formed from a dense float8[] in uncompressed format.
 * This is useful for creating a SparseData without processing that can be
 * used by the SparseData processing routines.
 */
static SparseData
sdata_uncompressed_from_float8arr_internal(ArrayType *array)
{
        int dim = ARR_NDIM(array);
        int *dims = ARR_DIMS(array);
	int num = ArrayGetNItems(dim,dims);
        double *vals =(double *)ARR_DATA_PTR(array);
        bits8 *bitmap = ARR_NULLBITMAP(array);
        int   bitmask=1;
	SparseData result = makeInplaceSparseData(
			(char *)vals,NULL,
			num*sizeof(float8),0,FLOAT8OID,
			num,num);

	/*
	 * Convert null items into zeros
	 */
	if (bitmap)
	{
        	for (int i=0; i<num; i++)
		{
                	if ((*bitmap& bitmask) == 0)
				vals[i] = 0.;
                        bitmask <<= 1;
                        if (bitmask == 0x100)
                        {
                                bitmap++;
                                bitmask = 1;
                        }
		}
	}
	return(result);
}

/*
 * L1 Norm
 */
Datum float8arr_l1norm(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_l1norm);

Datum
float8arr_l1norm(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = l1norm_sdata_values_double(sdata);
	pfree(sdata);
	PG_RETURN_FLOAT8(result);
}

Datum float8arr_summate(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_summate);

Datum
float8arr_summate(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = sum_sdata_values_double(sdata);
	pfree(sdata);
	PG_RETURN_FLOAT8(result);
}


/*
 * L2 Norm
 */
Datum float8arr_l2norm(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_l2norm);

Datum
float8arr_l2norm(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = l2norm_sdata_values_double(sdata);
	pfree(sdata);
	PG_RETURN_FLOAT8(result);
}


/*
 * Dot product
 */
Datum float8arr_dot(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_dot);

Datum
float8arr_dot(PG_FUNCTION_ARGS) {
	ArrayType *arr_left   = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr_right  = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left  = sdata_uncompressed_from_float8arr_internal(arr_left);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr_right);
	SparseData mult_result;
	double accum;

	mult_result = op_sdata_by_sdata(2,left,right);
	accum = sum_sdata_values_double(mult_result);
	freeSparseData(left);
	freeSparseData(right);
	freeSparseDataAndData(mult_result);

	PG_RETURN_FLOAT8(accum);
}

/*
 * Permute the basic operators (minus,plus,mult,div) between SparseData and float8[]
 *
 * For each function, make a version that takes the left and right args as
 * each type (without copies)
 */
PG_FUNCTION_INFO_V1( float8arr_minus_float8arr );
Datum
float8arr_minus_float8arr(PG_FUNCTION_ARGS)
{
	ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left  = sdata_uncompressed_from_float8arr_internal(arr1);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr2);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,0,left,right));
}
PG_FUNCTION_INFO_V1( svec_minus_float8arr );
Datum
svec_minus_float8arr(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left = sdata_from_svec(svec);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,0,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_minus_svec );
Datum
float8arr_minus_svec(PG_FUNCTION_ARGS)
{
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData right = sdata_from_svec(svec);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,0,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_plus_float8arr );
Datum
float8arr_plus_float8arr(PG_FUNCTION_ARGS)
{
	ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left  = sdata_uncompressed_from_float8arr_internal(arr1);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr2);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,1,left,right));
}
PG_FUNCTION_INFO_V1( svec_plus_float8arr );
Datum
svec_plus_float8arr(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left = sdata_from_svec(svec);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,1,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_plus_svec );
Datum
float8arr_plus_svec(PG_FUNCTION_ARGS)
{
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData right = sdata_from_svec(svec);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,1,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_mult_float8arr );
Datum
float8arr_mult_float8arr(PG_FUNCTION_ARGS)
{
	ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left  = sdata_uncompressed_from_float8arr_internal(arr1);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr2);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	SvecType *svec = svec_operate_on_sdata_pair(scalar_args,2,left,right);
	PG_RETURN_SVECTYPE_P(svec);
}
PG_FUNCTION_INFO_V1( svec_mult_float8arr );
Datum
svec_mult_float8arr(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left = sdata_from_svec(svec);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	SvecType *result = svec_operate_on_sdata_pair(scalar_args,2,left,right);
	PG_RETURN_SVECTYPE_P(result);
}
PG_FUNCTION_INFO_V1( float8arr_mult_svec );
Datum
float8arr_mult_svec(PG_FUNCTION_ARGS)
{
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData right = sdata_from_svec(svec);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,2,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_div_float8arr );
Datum
float8arr_div_float8arr(PG_FUNCTION_ARGS)
{
	ArrayType *arr1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left  = sdata_uncompressed_from_float8arr_internal(arr1);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr2);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,3,left,right));
}
PG_FUNCTION_INFO_V1( svec_div_float8arr );
Datum
svec_div_float8arr(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(1);
	SparseData left = sdata_from_svec(svec);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,3,left,right));
}
PG_FUNCTION_INFO_V1( float8arr_div_svec );
Datum
float8arr_div_svec(PG_FUNCTION_ARGS)
{
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData right = sdata_from_svec(svec);
	int scalar_args = check_scalar(SDATA_IS_SCALAR(left),SDATA_IS_SCALAR(right));
	PG_RETURN_SVECTYPE_P(svec_operate_on_sdata_pair(scalar_args,3,left,right));
}
PG_FUNCTION_INFO_V1( svec_dot_float8arr );
Datum
svec_dot_float8arr(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(1);
	SparseData right = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData left = sdata_from_svec(svec);
	SparseData mult_result;
	double accum;
	mult_result = op_sdata_by_sdata(2,left,right);
	accum = sum_sdata_values_double(mult_result);
	freeSparseData(right);
	freeSparseDataAndData(mult_result);

	PG_RETURN_FLOAT8(accum);
}
PG_FUNCTION_INFO_V1( float8arr_dot_svec);
Datum
float8arr_dot_svec(PG_FUNCTION_ARGS)
{
	ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_uncompressed_from_float8arr_internal(arr);
	SparseData right = sdata_from_svec(svec);
	SparseData mult_result;
	double accum;
	mult_result = op_sdata_by_sdata(2,left,right);
	accum = sum_sdata_values_double(mult_result);
	freeSparseData(left);
	freeSparseDataAndData(mult_result);

	PG_RETURN_FLOAT8(accum);
}


/*
 * Hash function for float8[]
 */
static int
float8arr_hash_internal(ArrayType *array)
{
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double l1norm = l1norm_sdata_values_double(sdata);
	int arr_hash = DirectFunctionCall1(hashfloat8, Float8GetDatumFast(l1norm));
	pfree(sdata);
	return(arr_hash);
}

Datum float8arr_hash(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_hash);

Datum
float8arr_hash(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);

	PG_RETURN_INT32(float8arr_hash_internal(array));
}

/*
 * Aggregate function svec_pivot takes it's float8 argument and appends it
 * to the state variable (an svec) to produce the concatenated return variable.
 * The StringInfo variables within the state variable svec are used in a way
 * that minimizes the number of memory re-allocations.
 *
 * Note that the first time this is called, the state variable should be null.
 */
PG_FUNCTION_INFO_V1( svec_pivot );
Datum
svec_pivot(PG_FUNCTION_ARGS)
{
	SvecType *svec;
	SparseData sdata;
	float8 value;

	if (PG_ARGISNULL(1))
	{
		value = 0.;
	} else
	{
		value = PG_GETARG_FLOAT8(1);
	}

	if (! PG_ARGISNULL(0))
	{
		svec = PG_GETARG_SVECTYPE_P(0);
	} else {	//first call, construct a new svec
		/*
		 * Allocate space for the unique values and index
		 *
		 * Note that we do this manually because we are going to
		 * manage the memory allocations for the StringInfo structures
		 * manually within this aggregate so that we can preserve
		 * the intermediate state without re-serializing until there is
		 * a need to re-alloc, at which point we will re-serialize to
		 * form the returned state variable.
		 */
		svec = makeEmptySvec(1);
	}
	sdata = sdata_from_svec(svec);

	/*
	 * Add the incoming float8 value to the svec.
	 *
	 * First check to see if there is room in both the data area and index
	 * and if there isn't, re-alloc and recreate the svec
	 */
	if (   ((sdata->vals->len + sizeof(float8)+1) > sdata->vals->maxlen)
	    || ((sdata->index->len + 9 +1)            > sdata->index->maxlen) )
	{
		svec = reallocSvec(svec);
		sdata = sdata_from_svec(svec);
	}
	/*
	 * Now let's check to see if we're adding a new value or appending to the last
	 * run.  If the incoming value is the same as the last value, just increment
	 * the last run.  Note that we need to use the index cursor to find where the
	 * last index counter is located.
	 */
	{
		char *index_location;
		int old_index_storage_size;
		int64 run_count;
		float8 last_value=-100000;
		bool new_run;

		if (sdata->index->len==0) //New vector
		{
			new_run=true;
			index_location = sdata->index->data;
			sdata->index->cursor = 0;
			run_count = 0;
		} else
		{
			index_location = sdata->index->data + sdata->index->cursor;
			old_index_storage_size = int8compstoragesize(index_location);
			run_count = compword_to_int8(index_location);
			last_value = *((float8 *)(sdata->vals->data+(sdata->vals->len-sizeof(float8))));

			if (last_value == value)
			{
				new_run=false;
			} else {
				new_run=true;
			}
		}
		if (!new_run)
		{
			run_count++;
			int8_to_compword(run_count,index_location);
			sdata->index->len += (int8compstoragesize(index_location)
					- old_index_storage_size);
			sdata->total_value_count++;
		} else {
			add_run_to_sdata((char *)&value,1,sizeof(float8),sdata);
			char *i_ptr=sdata->index->data;
			int len=0;
			for (int j=0;j<sdata->unique_value_count-1;j++)
			{
				len+=int8compstoragesize(i_ptr);
				i_ptr+=int8compstoragesize(i_ptr);
			}
			sdata->index->cursor = len;
		}
	}

	PG_RETURN_SVECTYPE_P(svec);
}

#define RANDOM_RANGE	(((double)random())/(2147483647.+1))
#define RANDOM_INT(x,y)	((int)(x)+(int)(((y+1)-(x))*RANDOM_RANGE))
#define SWAPVAL(x,y,temp)	{ (temp) = (x); (x) = (y); (y) = (temp); }
#define SWAP(x,y,tmp,size)	{ memcpy((tmp),(x),(size)); memcpy((x),(y),(size)); memcpy((y),(tmp),(size)); }
#define SWAPN(lists,nlists,widths,tmp,I,J) \
{ \
	for (int III=0;III<nlists;III++) /* This should be unrolled as nlists will be small */ \
	{ \
	  	memcpy((tmp)[III]                  ,(lists)[III]+I*(widths)[III],(widths)[III]); \
		memcpy((lists)[III]+I*(widths)[III],(lists)[III]+J*(widths)[III],(widths)[III]); \
		memcpy((lists)[III]+J*(widths)[III],(tmp)[III]                  ,(widths)[III]); \
	} \
}
/*
 * Implements the partition selection algorithm with randomized selection
 *
 * From: http://en.wikipedia.org/wiki/Selection_algorithm#Linear_general_selection_algorithm_-_.22Median_of_Medians_algorithm.22
 *
 * Arguments:
 * 	char **lists:	A list of lists, the first of which contains the values used for pivots
 * 			the 2nd and further lists will be pivoted alongside the first.
 * 			A common usage would be to have the first list point to an array
 * 			of values, then the second would point to another char ** list of
 * 			strings.  The second list would have it's pointer values moved
 * 			around as part of the pivots, and the index location where the
 * 			partition value (say for the median) occurs would allow a reference
 * 			to the associated strings in the second list.
 * 	size_t nlists	the number of lists
 * 	size_t *widths	An array of widths, one for each list
 * 	int left,right	The left and right boundary of the list to be pivoted
 * 	int pivotIndex	The index around which to pivot the list.  A common use-case is
 * 			to choose pivotIndex = listLength/2, then the pivot will provide
 * 			the median location.
 * 	int (*compar)	A comparison function for the first list, which takes two pointers
 * 			to values in the first list and returns 0,-1 or 1 when the first
 * 			value is equal, less than or greater than the second.
 * 	char **tmp 	A list of temporary variables, allocated with the size of the value
 * 			in each list
 * 	void *pvalue	Pointers to temporary variable allocated with the width of the
 * 			values of the first list.
 */
static int
partition_pivot(char **lists, size_t nlists, size_t *widths,
		int left, int right, int pivotIndex,
		int (*compar)(const void *, const void *),
		char **tmp, void *pvalue)
{
	int storeIndex = left;

	memcpy(pvalue,lists[0]+pivotIndex*widths[0],widths[0]);

	SWAPN(lists,nlists,widths,tmp,pivotIndex,right) // Move pivot to end
	for (int i=left;i<right;i++)
	{
		if (compar(lists[0]+i*widths[0],pvalue) <= 0)
		{
			SWAPN(lists,nlists,widths,tmp,i,storeIndex)
			storeIndex++;
		}
	}
	SWAPN(lists,nlists,widths,tmp,storeIndex,right) // Move pivot to its final place
	return(storeIndex);
}

/*
 * The call interface to partition_select has one complicated looking feature:
 * you must pass in a "Real Index Calculation" function that will return an integer
 * corresponding to the actual partition index.  This is used to enable the same routine to
 * work with dense and compressed structures.  This function can just return the input
 * integer unmodified if using a dense array of values as input.
 * The arguments to realIndexCalc() should be:
 * 	int: 		the pivot index (returned from the pivot function)
 * 	char **:	the list of lists
 * 	size_t:		the number of lists
 * 	size_t *:	the width of each value in the list
 */
static int
partition_select (char **lists, size_t nlists, size_t *widths,
		int left, int right, int k,
		int (*compar)(const void *, const void *),
		int (*realIndexCalc)(const int, const char **, const size_t, const size_t *))
{
	int pivotIndex,pivotNewIndex,realIndex;
	char **tmp,*pvalue;
	int maxlen = right;

	/*
	 * Allocate memory for the temporary variables
	 */
	tmp = (char **)palloc(nlists*sizeof(char *));
	for (int i=0;i<nlists;i++)
	{
		tmp[i] = (void *)palloc(widths[i]);
	}
	pvalue = (char *)palloc(widths[0]);

	while (1)
	{
		pivotIndex = RANDOM_INT(left,right);
		pivotNewIndex = partition_pivot(lists,nlists,widths,
					left,right,pivotIndex,
					compar,
					tmp,pvalue);
		realIndex = realIndexCalc(pivotNewIndex,
				(const char **)lists,nlists,widths);
		int nextRealIndex = realIndexCalc(MIN(maxlen,pivotNewIndex+1),
	                                (const char **)lists,nlists,widths);
		if ((realIndex <= k) && (k < nextRealIndex ))
		{
			break;
		} else if (k < realIndex)
		{
			right = pivotNewIndex-1;
		} else
		{
			left = pivotNewIndex+1;
			if (left >= maxlen) 
			{
				pivotNewIndex = maxlen;
				break;
			}
		}
	}

	/*
	 * Free temporary variables
	 */
	for (int i=0;i<nlists;i++)
	{
		pfree(tmp[i]);
	}
	pfree(tmp);
	pfree(pvalue);

	return(pivotNewIndex); //This index is in the compressed coordinate system
}

static int
compar_float8(const void *left,const void *right)
{if(*(float8 *)left<*(float8 *)right){return -1;}else if(*(float8 *)left==*(float8 *)right){return 0;}else{return 1;}}

static int
real_index_calc_dense(const int idx,const char **lists,const size_t nlists,const size_t *widths) {return idx;}

static int
real_index_calc_sparse_RLE(const int idx,const char **lists,const size_t nlists,const size_t *widths)
{
	int index=0;
	for (int i=0;i<idx;i++)
	{
		index = index + ((int64 *)(lists[1]))[i];
	}
	/*
	 * The index calculation corresponds to the beginning
	 * of the run located at (idx).
	 */
	return (index);
}

static int
float8arr_partition_internal(float8 *array,int len,int k)
{
	size_t width=sizeof(float8);
	char *list = (char *)array;
	int index = partition_select(&list,1,&width,
				0,len-1,
			   	k,compar_float8,
				real_index_calc_dense);
	return (index);
}

Datum float8arr_median(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1( float8arr_median);

Datum
float8arr_median(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	int index,median_index = (sdata->total_value_count-1)/2;

	index = float8arr_partition_internal((double *)(sdata->vals->data),
					     sdata->total_value_count,
					     median_index);

	PG_RETURN_FLOAT8(((float8 *)(sdata->vals->data))[index]);
}

Datum svec_median(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1( svec_median);

Datum
svec_median(PG_FUNCTION_ARGS) {
	SvecType *svec  = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata = sdata_from_svec(svec);
	int index,median_index = (sdata->total_value_count-1)/2;
	char *i_ptr;
	int64 *rle_index;

	if (sdata->index->data != NULL) //Sparse vector
	{
		/*
	 	 * We need to create an uncompressed run length index to
	 	 * feed to the partition select routine
	 	 */
		rle_index = (int64 *)palloc(sizeof(int64)*(sdata->unique_value_count));
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			rle_index[i] = compword_to_int8(i_ptr);
		}
		/*
		 * Allocate the outer "list of lists"
		 */
		char **lists = (char **)palloc(sizeof(char *)*2);
		lists[0] = sdata->vals->data;
		lists[1] = (char *)rle_index;
		size_t *widths = (size_t *)palloc(sizeof(size_t)*2);
		widths[0] = sizeof(float8);
		widths[1] = sizeof(int64);
		index = partition_select(lists,2,widths,
					0,sdata->unique_value_count-1,
			   		median_index,compar_float8,
					real_index_calc_sparse_RLE);
		/*
		 * Convert the uncompressed index into the compressed index
		 */
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			int8_to_compword(rle_index[i],i_ptr);
		}

		pfree(lists);
		pfree(widths);
		pfree(rle_index);
	} else
	{
		index = float8arr_partition_internal((double *)(sdata->vals->data),
				sdata->total_value_count,
				median_index);
	}

	PG_RETURN_FLOAT8(((float8 *)(sdata->vals->data))[index]);
}
