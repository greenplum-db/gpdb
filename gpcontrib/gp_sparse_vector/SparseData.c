/*-------------------------------------------------------------------------
 *
 * SparseData.c
 *
 * Copyright (c) 2010, Greenplum Software
 * Portions Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_sparse_vector/SparseData.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "SparseData.h"

/*------------------------------------------------------------------------------
 * Create a SparseData structure
 *
 * There are ways to do this:
 * 	First way allocates empty dynamic StringInfo of unknown initial sizes
 * 	Second way creates a copy of an existing SparseData
 * 	Third way creates a SparseData with zero storage in it's StringInfos
 * 	Fourth way creates a SparseData in place using pointers to the vals and
 * 		index data
 */
SparseData makeSparseData(void)
{
	/* Allocate the struct */
	SparseData sdata = (SparseData)palloc(sizeof(SparseDataStruct));
	/* Allocate the included elements */
	sdata->vals  = makeStringInfo();
	sdata->index = makeStringInfo();
	sdata->vals->len = 0;
	sdata->index->len = 0;
	sdata->vals->cursor = 0;
	sdata->index->cursor = 0;
	sdata->unique_value_count=0;
	sdata->total_value_count=0;
	sdata->type_of_data = FLOAT8OID;
	return sdata;
}
SparseData makeEmptySparseData(void)
{
	/* Allocate the struct */
	SparseData sdata = makeSparseData();
	/*
	 * Free the data area
	 */
	pfree(sdata->vals->data);
	pfree(sdata->index->data);
	sdata->vals->data  = palloc(1);
	sdata->index->data = palloc(1);
	sdata->vals->maxlen=0;
	sdata->index->maxlen=0;
	return sdata;
}
SparseData makeInplaceSparseData(char *vals, char *index,
		int datasize, int indexsize, Oid datatype,
		int unique_value_count, int total_value_count)
{
	SparseData sdata = makeEmptySparseData();
	sdata->unique_value_count = unique_value_count;
	sdata->total_value_count  = total_value_count;
	sdata->vals->data = vals;
	sdata->vals->len = datasize;
	sdata->vals->maxlen = sdata->vals->len;
	sdata->index->data = index;
	sdata->index->len = indexsize;
	sdata->index->maxlen = sdata->index->len;
	sdata->type_of_data = datatype;
	return(sdata);
}
/*------------------------------------------------------------------------------
 * Copy a SparseData structure including it's data and index
 */
SparseData makeSparseDataCopy(SparseData source_sdata)
{
	/* Allocate the struct */
	SparseData sdata = (SparseData)palloc(sizeof(SparseDataStruct));
	/* Allocate the included elements */
	sdata->vals = copyStringInfo(source_sdata->vals);
	sdata->index = copyStringInfo(source_sdata->index);
	sdata->type_of_data       = source_sdata->type_of_data;
	sdata->unique_value_count = source_sdata->unique_value_count;
	sdata->total_value_count  = source_sdata->total_value_count;
	return sdata;
}
/*
 * Create a SparseData of size dimension from a constant
 */
SparseData makeSparseDataFromDouble(double constant,int64 dimension)
{
	char *bytestore=(char *)palloc(sizeof(char)*9);
	SparseData sdata = float8arr_to_sdata(&constant,1);
	int8_to_compword(dimension,bytestore); /* create compressed version of
					          int8 value */
	pfree(sdata->index->data);
	sdata->index->data = bytestore;
	sdata->index->len = int8compstoragesize(bytestore);
	sdata->total_value_count=dimension;
	if (sdata->index->maxlen < int8compstoragesize(bytestore))
		elog(ERROR, "internal error: double value exceeds sparse data max length");

	return(sdata);
}

void
freeSparseData(SparseData sdata)
{
	pfree(sdata->vals);
	pfree(sdata->index);
	pfree(sdata);
}

void
freeSparseDataAndData(SparseData sdata)
{
	pfree(sdata->vals->data);
	pfree(sdata->index->data);
	freeSparseData(sdata);
}

/*------------------------------------------------------------------------------
 * Copy a StringInfo structure
 */
StringInfo copyStringInfo(StringInfo sinfo) {
	StringInfo result;
	char *data;
	if (sinfo->data == NULL)
	{
		data = NULL;
	} else
	{
		data   = (char *)palloc(sizeof(char)*(sinfo->len+1));
		memcpy(data,sinfo->data,sinfo->len);
		data[sinfo->len] = '\0';
	}
	result = makeStringInfoFromData(data,sinfo->len);
	return (result);
}
/*
 * make a StringInfo from a data pointer and length
 */
StringInfo makeStringInfoFromData(char *data,int len) {
	StringInfo sinfo;
	sinfo = (StringInfo)palloc(sizeof(StringInfoData));
	sinfo->data   = data;
	sinfo->len    = len;
	sinfo->maxlen = len;
	sinfo->cursor = 0;
	return sinfo;
}
/*------------------------------------------------------------------------------
 * Convert a double[] to a SparseData compressed format, representing duplicate
 * values in the double[] by a count and a single value.
 * Note that special double values like denormalized numbers and exceptions
 * like NaN are treated like any other value - if there are duplicates, the
 * value of the special number is preserved and they are counted.
 *------------------------------------------------------------------------------
 */
SparseData float8arr_to_sdata(double *array, int count) {
	char *run_val=(char *)array;
	int64 run_len=1;
	SparseData sdata = makeSparseData();

	sdata->type_of_data=FLOAT8OID;

	for (int i=1; i<count; i++) {
		char *curr_val=(char *)(array+i);
			if (memcmp(curr_val,run_val,sizeof(float8)))
			{ /*run is interrupted, initiate new run */
				/* package up the finished run */
				add_run_to_sdata(run_val, run_len, sizeof(float8), sdata);
				/* mark beginning of new run */
				run_val=curr_val;
				run_len=1;
			} else
			{ /* we're still in the same run */
				run_len++;
			}
	}
	add_run_to_sdata(run_val, run_len, sizeof(float8), sdata); /* package up the last run */

	/* Add the final tallies */
	sdata->unique_value_count = sdata->vals->len/sizeof(double);
	sdata->total_value_count = count;

	return sdata;
}

SparseData arr_to_sdata(char *array, size_t width, Oid type_of_data, int count) {
	char *run_val=array;
	int64 run_len=1;
	SparseData sdata = makeSparseData();

	sdata->type_of_data=type_of_data;

	for (int i=1; i<count; i++) {
		char *curr_val=array+i;
			if (memcmp(curr_val,run_val,width))
			{ /*run is interrupted, initiate new run */
				/* package up the finished run */
				add_run_to_sdata(run_val, run_len, width, sdata);
				/* mark beginning of new run */
				run_val=curr_val;
				run_len=1;
			} else
			{ /* we're still in the same run */
				run_len++;
			}
	}
	add_run_to_sdata(run_val, run_len, width, sdata); /* package up the last run */

	/* Add the final tallies */
	sdata->unique_value_count = sdata->vals->len/width;
	sdata->total_value_count = count;

	return sdata;
}

/*------------------------------------------------------------------------------
 * Convert a SparseData compressed structure to a float8[]
 *------------------------------------------------------------------------------
 */
double *sdata_to_float8arr(SparseData sdata) {
	double *array;
	int j, aptr;
	char *iptr;

	if (sdata->type_of_data != FLOAT8OID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("data type of SparseData is not FLOAT64")));
	}

	array = (double *) palloc(sizeof(double) * sdata->total_value_count);
	iptr = sdata->index->data;
	aptr = 0;
	for (int i=0; i<sdata->unique_value_count; i++) {
		for (j=0;j<compword_to_int8(iptr);j++,aptr++) {
			array[aptr] = ((double *)(sdata->vals->data))[i];
		}
		iptr+=int8compstoragesize(iptr);
	}

	if ((aptr) != sdata->total_value_count) 
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("array size is incorrect, is: %d and should be %d",
						aptr,sdata->total_value_count)));
	}

	return array;
}

int64 *sdata_index_to_int64arr(SparseData sdata)
{
	char *iptr;
	int64 *array_ix = (int64 *)palloc(
			sizeof(int64)*(sdata->unique_value_count));

	iptr = sdata->index->data;
	for (int i=0; i<sdata->unique_value_count; i++,
			iptr+=int8compstoragesize(iptr)) {
		array_ix[i] = compword_to_int8(iptr);
	}
	return(array_ix);
}

void serializeSparseData(char *target, SparseData source)
{
	/* SparseDataStruct header */
	memcpy(target,source,SIZEOF_SPARSEDATAHDR);
	/* Two StringInfo structures describing the data and index */
	memcpy(SDATA_DATA_SINFO(target), source->vals,sizeof(StringInfoData));
	memcpy(SDATA_INDEX_SINFO(target),source->index,sizeof(StringInfoData));
	/* The unique data values */
	memcpy(SDATA_VALS_PTR(target),source->vals->data,source->vals->maxlen);
	/* The index values */
	memcpy(SDATA_INDEX_PTR(target),source->index->data,source->index->maxlen);

	/*
	 * Set pointers to the data areas of the serialized structure
	 * First the two StringInfo structures contained in the SparseData,
	 * then the data areas inside each of the two StringInfos.
	 */
	((SparseData)target)->vals = (StringInfo)SDATA_DATA_SINFO(target);
	((SparseData)target)->index = (StringInfo)SDATA_INDEX_SINFO(target);
	((StringInfo)(SDATA_DATA_SINFO(target)))->data  = SDATA_VALS_PTR(target);
	if (source->index->data != NULL)
	{
		((StringInfo)(SDATA_INDEX_SINFO(target)))->data = SDATA_INDEX_PTR(target);
	} else
	{
		((StringInfo)(SDATA_INDEX_SINFO(target)))->data = NULL;
	}
}
