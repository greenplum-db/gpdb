#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"

#include "access/formatter.h"
#include "catalog/pg_proc.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/typcache.h"
#include "utils/syscache.h"

/* Do the module magic dance */
PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(formatter_export);
PG_FUNCTION_INFO_V1(formatter_import);

Datum formatter_export(PG_FUNCTION_ARGS);
Datum formatter_import(PG_FUNCTION_ARGS);


typedef struct {
	int        ncols;
	Datum     *values;
	bool      *nulls;
	int        buflen;
	bytea     *buffer;
} format_t;


/* 
 * Maximum size string to support, affects allocation size of the tuple buffer.
 * Only used for variable length strings.  For strings with a declared typmod
 * we allow that size even if it is larger than this.
 */
#define MAX_FORMAT_STRING 4096

/*
 * Our format converts all NULLs to real values, for floats that value is NaN
 */
#define NULL_FLOAT8_VALUE get_float8_nan()


Datum 
formatter_export(PG_FUNCTION_ARGS)
{
	HeapTupleHeader		rec	= PG_GETARG_HEAPTUPLEHEADER(0);
	TupleDesc           tupdesc;
	HeapTupleData		tuple;
	int                 ncolumns = 0;
	format_t           *myData;
	char               *data;
	int                 datlen;
	int                 i;

	/* Must be called via the external table format manager */
	if (!CALLED_AS_FORMATTER(fcinfo))
		elog(ERROR, "formatter_export: not called by format manager");
	
	tupdesc = FORMATTER_GET_TUPDESC(fcinfo);
	
	/* Get our internal description of the formatter */
	ncolumns = tupdesc->natts;
	myData = (format_t *) FORMATTER_GET_USER_CTX(fcinfo);
	if (myData == NULL)
	{
		myData          = palloc(sizeof(format_t));
		myData->ncols   = ncolumns;
		myData->values  = palloc(sizeof(Datum) * ncolumns);
		myData->nulls   = palloc(sizeof(bool) * ncolumns);
		
		/* Determine required buffer size */
		myData->buflen = 0;
		for (i = 0; i < ncolumns; i++)
		{
			Oid   type   = tupdesc->attrs[i]->atttypid;
			int32 typmod = tupdesc->attrs[i]->atttypmod;

			/* Don't know how to format dropped columns, error for now */
			if (tupdesc->attrs[i]->attisdropped)
				elog(ERROR, "formatter_export: dropped columns");

			switch (type)
			{
				case FLOAT8OID:
				{
					myData->buflen += sizeof(double);
					break;
				}
	
				case VARCHAROID:
				case BPCHAROID:
				case TEXTOID:
				{
					myData->buflen += (typmod > 0) ? typmod : MAX_FORMAT_STRING;
					break;
				}
					
				default:
				{
					elog(ERROR, "formatter_export error: unsupported data type");
					break;
				}
			}
		}

		myData->buflen = Max(128, myData->buflen);  /* allocate at least 128 bytes */
		myData->buffer = palloc(myData->buflen + VARHDRSZ);

		FORMATTER_SET_USER_CTX(fcinfo, myData);
	}
	if (myData->ncols != ncolumns)
		elog(ERROR, "formatter_export: unexpected change of output record type");

	/* break the input tuple into fields */
	tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
	ItemPointerSetInvalid(&(tuple.t_self));
	tuple.t_data = rec;
	heap_deform_tuple(&tuple, tupdesc, myData->values, myData->nulls);

	datlen = 0;
	data = VARDATA(myData->buffer);

	
	/* =======================================================================
	 *                            MAIN FORMATTING CODE
	 *
	 * Currently this code assumes:
	 *  - Homogoneos hardware => No need to convert data to network byte order
	 *  - Support for TEXT/VARCHAR/BPCHAR/FLOAT8 only
	 *  - Length Prefixed strings
	 *  - No end of record tags, checksums, or optimizations for alignment.
	 *  - NULL values are cast to some sensible default value (NaN, "")
	 *
	 * ======================================================================= */
	for (i = 0; i < ncolumns; i++)
	{
		Oid	  type    = tupdesc->attrs[i]->atttypid;
		int   typmod  = tupdesc->attrs[i]->atttypmod;
		Datum val     = myData->values[i];
		bool  nul     = myData->nulls[i];
		
		switch (type)
		{
			case FLOAT8OID:
			{
				float8 value;

				if (datlen + sizeof(value) >= myData->buflen)
					elog(ERROR, "formatter_export: buffer too small");
				
				if (nul)
					value = NULL_FLOAT8_VALUE;
				else
					value = DatumGetFloat8(val);

				memcpy(&data[datlen], &value, sizeof(value));
				datlen += sizeof(value);
				break;
			}

			case TEXTOID:
			case VARCHAROID:
			case BPCHAROID:
			{
				text  *str;
				int32  len;
			   
				if (nul)
				{
					str = NULL;
					len = 0;
				}
				else
				{
					str = DatumGetTextP(val);
					len = VARSIZE(str) - VARHDRSZ;
					if (typmod < 0)
						len  = Min(len, MAX_FORMAT_STRING);
				}

				if (datlen + sizeof(len) + len >= myData->buflen)
					elog(ERROR, "formatter_export: buffer too small");
				memcpy(&data[datlen], &len, sizeof(len));
				datlen += sizeof(len);

				if (len > 0)
				{
					memcpy(&data[datlen], VARDATA(str), len);
					datlen += len;
				}
				break;
			}

			default:
				elog(ERROR, "formatter_export: unsupported datatype");
				break;
		}	
	}
	/* ======================================================================= */
	
	SET_VARSIZE(myData->buffer, datlen + VARHDRSZ);
	PG_RETURN_BYTEA_P(myData->buffer);
}

Datum 
formatter_import(PG_FUNCTION_ARGS)
{
	HeapTuple			tuple;
	TupleDesc           tupdesc;
	MemoryContext 		m, oldcontext;
	format_t           *myData;
	char               *data_buf;
	int                 ncolumns = 0;
	int			  		data_cur;
	int                 data_len;
	int                 i;
	
	/* Must be called via the external table format manager */
	if (!CALLED_AS_FORMATTER(fcinfo))
		elog(ERROR, "formatter_import: not called by format manager");
	
	tupdesc = FORMATTER_GET_TUPDESC(fcinfo);
	
	/* Get our internal description of the formatter */
	ncolumns = tupdesc->natts;
	myData = (format_t *) FORMATTER_GET_USER_CTX(fcinfo);
	
	if (myData == NULL)
	{

		myData          = palloc(sizeof(format_t));
		myData->ncols   = ncolumns;
		myData->values  = palloc(sizeof(Datum) * ncolumns);
		myData->nulls   = palloc(sizeof(bool) * ncolumns);
		
		/* misc verification */
		for (i = 0; i < ncolumns; i++)
		{
			Oid   type   = tupdesc->attrs[i]->atttypid;
			//int32 typmod = tupdesc->attrs[i]->atttypmod;

			/* Don't know how to format dropped columns, error for now */
			if (tupdesc->attrs[i]->attisdropped)
				elog(ERROR, "formatter_import: dropped columns");

			switch (type)
			{
				case FLOAT8OID:
				case VARCHAROID:
				case BPCHAROID:
				case TEXTOID:
					break;
					
				default:
				{
					elog(ERROR, "formatter_import error: unsupported data type");
					break;
				}
			}
		}

		FORMATTER_SET_USER_CTX(fcinfo, myData);

	}
	if (myData->ncols != ncolumns)
		elog(ERROR, "formatter_import: unexpected change of output record type");

	/* get our input data buf and number of valid bytes in it */
	data_buf = FORMATTER_GET_DATABUF(fcinfo);
	data_len = FORMATTER_GET_DATALEN(fcinfo); 
	data_cur = FORMATTER_GET_DATACURSOR(fcinfo);
	
	/* start clean */
	MemSet(myData->values, 0, ncolumns * sizeof(Datum));
	MemSet(myData->nulls, true, ncolumns * sizeof(bool));
	
	/* =======================================================================
	 *                            MAIN FORMATTING CODE
	 *
	 * Currently this code assumes:
	 *  - Homogoneos hardware => No need to convert data to network byte order
	 *  - Support for TEXT/VARCHAR/BPCHAR/FLOAT8 only
	 *  - Length Prefixed strings
	 *  - No end of record tags, checksums, or optimizations for alignment.
	 *  - NULL values are cast to some sensible default value (NaN, "")
	 *
	 * ======================================================================= */
	m = FORMATTER_GET_PER_ROW_MEM_CTX(fcinfo);
	oldcontext = MemoryContextSwitchTo(m);

	for (i = 0; i < ncolumns; i++)
	{
		Oid		type    	= tupdesc->attrs[i]->atttypid;
		//int	typmod		= tupdesc->attrs[i]->atttypmod;
		int		remaining	= 0;
		int		attr_len 	= 0;
		
		remaining = data_len - data_cur;
		
		switch (type)
		{
			case FLOAT8OID:
			{
				float8 value;

				attr_len = sizeof(value);
				
				if (remaining < attr_len)
				{
					MemoryContextSwitchTo(oldcontext);
					FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA);
				}
				
				memcpy(&value, data_buf + data_cur, attr_len);
				
				if(value != NULL_FLOAT8_VALUE)
				{
					myData->nulls[i] = false;
					myData->values[i] = Float8GetDatum(value);
				}
				
				/* TODO: check for nan? */
				
				break;
			}

			case TEXTOID:
			case VARCHAROID:
			case BPCHAROID:
			{
				text*	value;
				int32	len;
				bool	nextlen = remaining >= sizeof(len);
				
				if (nextlen)
				{
					memcpy(&len, data_buf + data_cur, sizeof(len));

					if (len < 0)
						elog(ERROR, "invalid length of varlen datatype: %d",
									len);
				}

				/* if len or data bytes don't exist in this buffer, return */
				if (!nextlen || (nextlen && (remaining - sizeof(len) < len)))
				{
					MemoryContextSwitchTo(oldcontext);
					FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA);
				}
				
				if (len > 0)
				{
					value = (text *) palloc(len + VARHDRSZ);
					SET_VARSIZE(value, len + VARHDRSZ);

					memcpy(VARDATA(value), data_buf + data_cur + sizeof(len), len);
					
					myData->nulls[i] = false;
					myData->values[i] = PointerGetDatum(value);
				}

				attr_len = len + sizeof(len);
				
				break;
			}
			
			default:
				elog(ERROR, "formatter_import: unsupported datatype");
				break;
		}	
		
		/* add byte length of last attribute to the temporary cursor */
		data_cur += attr_len;
		
	}
	/* ======================================================================= */

	MemoryContextSwitchTo(oldcontext);

	FORMATTER_SET_DATACURSOR(fcinfo, data_cur);

	tuple = heap_form_tuple(tupdesc, myData->values, myData->nulls);

	/* hack... pass tuple here. don't free prev tuple - the executor does it  */
	((FormatterData*) fcinfo->context)->fmt_tuple = tuple;
	
	FORMATTER_RETURN_TUPLE(tuple);
}
