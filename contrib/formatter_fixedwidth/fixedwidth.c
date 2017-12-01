#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"

#include "access/formatter.h"
#include "catalog/pg_proc.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/typcache.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "commands/copy.h"
#include <unistd.h>

/* Do the module magic dance */
PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(fixedwidth_out);
PG_FUNCTION_INFO_V1(fixedwidth_in);
Datum fixedwidth_out(PG_FUNCTION_ARGS);
Datum fixedwidth_in(PG_FUNCTION_ARGS);

typedef struct formatConfig
{
	/*
	 * Normaly we would have only one list of structs, each struct containing three fields:
	 * name, size, index. The reason we use three lists here is because we work with the infrastructure
	 * function CopyGetAttnums, which expects as input a list of names and returns a list of indexes.
	 * fldIndexes - holds the index of each field fetched from the file, into the fields description array
	 * tupdesc->attr[...]
	 */
	List       *fldNames;
	List       *fldSizes;
	List       *fldIndexes;
	List       *fldNullsWithBlanks;
	int         fields_tot_size;
	
	/*
	 * formating parameters
	 */
	int         preserve_blanks;
	char       *null_value;
	char       *line_delimiter;
	int         line_delimiter_length;
	
	/*
	 * infrastructure variables required by postgres "type resolution" methods
	 */
	FmgrInfo   *conv_functions;
	Oid        *typioparams;
	
} FormatConfig;

typedef struct {
	int            ncols;
	Datum         *values;
	bool          *nulls;
	int            buflen;
	bytea         *buffer;
	StringInfoData one_val;
	StringInfoData one_field;	/* temporary string buffer for string expansion */
	int            lineno;
	bool		   convert; 	/* true - perform conversion on column value. false - don't */
} format_t;

static void 
init_format_t(format_t** data, int ncolumns, FunctionCallInfo fcinfo)
{
	*data            = palloc(sizeof(format_t));
	(*data)->ncols   = ncolumns;
	(*data)->values  = palloc(sizeof(Datum) * ncolumns);
	(*data)->nulls   = palloc(sizeof(bool) * ncolumns);
	(*data)->lineno  = 1;
	(*data)->convert = false;
	initStringInfo( &((*data)->one_val) );
	initStringInfo( &((*data)->one_field) );
	
	FORMATTER_SET_USER_CTX(fcinfo, *data);	
	
}

/*
 * extract_field
 *
 * extract a field value from a character string 'data_cursor'. If we
 * preserve blanks, then the entire field_total_length is extracted.
 * Otherwise, we extract all bytes except the trailing blanks. The field
 * value is then stored inside 'output'.
 */
static void
extract_field(char *data_cursor, int field_total_length, bool preserve_blanks, StringInfo output)
{
	int actual_length;
	
	/*
	 * the actual length of the string we will restore into the database depends whether
	 * we preserve_blanks or not.
	 */
	if (preserve_blanks)
	{
		actual_length = field_total_length;
	}
	else 
	{
		/*
		 * assume all field characters are blanks
		 */
		char *tail = data_cursor + field_total_length - 1;
		actual_length = 0;
		
		while (tail != data_cursor)
		{
			if (*tail != ' ')
			{
				actual_length = tail - data_cursor + 1;
				break;
			}
			tail--;
		}
		
		if ( (tail == data_cursor) && (*data_cursor != ' ') )
		{
			actual_length  = 1;
		}
	}
	
	/* store the extracted field value */
	appendBinaryStringInfo(output, data_cursor, actual_length);
}

static void
reset_format_in_config(FormatConfig *format_config)
{
	format_config->preserve_blanks = 0;
	format_config->null_value = NULL;
	format_config->line_delimiter = "\n";
	format_config->line_delimiter_length = strlen(format_config->line_delimiter);
	format_config->fldNames = NIL;
	format_config->fldSizes = NIL;
	format_config->fldIndexes = NIL;
	format_config->fldNullsWithBlanks = NIL;
	format_config->fields_tot_size = 0;
}

/*
 * load_format_config
 *
 * parse the user specified fixed width keywords. Currently supported
 * keywords are: 'preserve_blanks', 'line_delim' and 'null'. any other
 * unrecognized keyword is treated as a column name (and later on gets
 * verified as a valid column).
 */
static void
load_format_config(FormatConfig *format_config, FunctionCallInfo fcinfo)
{
	int   i;
	char *key;
	char *val;
	int   args_num = FORMATTER_GET_NUM_ARGS(fcinfo);
	
	reset_format_in_config(format_config);
	
	for (i = 1; i <= args_num; i++)
	{
		key = FORMATTER_GET_NTH_ARG_KEY(fcinfo, i);
		val = FORMATTER_GET_NTH_ARG_VAL(fcinfo, i);
		
		if ( strcasecmp("preserve_blanks", key) == 0)
		{
			if ( strcasecmp("on", val) == 0)
			{
				format_config->preserve_blanks = 1;
			}
		}
		else if ( strcasecmp("line_delim", key) == 0)
		{
			format_config->line_delimiter = val;
			format_config->line_delimiter_length = strlen(val);
		}
		else if ( strcasecmp("null", key) == 0)
		{
			format_config->null_value = val;
		}
		else
		{
			int size = atoi(val);
			format_config->fldNames = lappend(format_config->fldNames, makeString(key));
			format_config->fldSizes = lappend_int(format_config->fldSizes, size);
			format_config->fields_tot_size += size;
		}		
	}
}

/*
 * encoding_check_str
 *
 * for a given string 'str' of length 'len', check if performing
 * an encoding conversion will modify the original string or not
 * and return the answer. The input string remains *unmodified*.
 * While at it, the encoding converter also verifies that the
 * input string is valid in the client (external table) encoding.
 */
static bool
encoding_check_str(FunctionCallInfo fcinfo, char *str, int len, bool is_import)
{
	char	*cvt = NULL;

	FORMATTER_ENCODE_STRING(fcinfo, str, len, cvt, is_import);

	if (cvt != NULL && cvt != str)
	{
		pfree(cvt);
		return true;
	}

	return false;
}

/*
 * encoding_encode_strinfo
 *
 * convert a given stringinfo 'strinfo' to the appropriate (pre-defined)
 * encoding (encoding will only be done if really needed).
 */
static void
encoding_encode_strinfo(FunctionCallInfo fcinfo, StringInfo strinfo, bool is_import)
{
	char	*cvt = NULL;

	FORMATTER_ENCODE_STRING(fcinfo, strinfo->data, strinfo->len, cvt, is_import);

	if (cvt != NULL && cvt != strinfo->data)
	{
		/* transfer converted data back to strinfo */
		resetStringInfo(strinfo);
		appendStringInfoString(strinfo, cvt);
		pfree(cvt);
	}
}


static char*
make_null_val_with_blanks(char *value, int field_size)
{
	char *ret;
	char *cur;
	int actual_size = field_size + 1;
	int size = strlen(value);
	
	if ( size > field_size)
	{
		ereport(ERROR,
				(errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
				 errmsg("The size of the NULL value cannot be bigger than the field size")));
	}
	
	ret = (char*)palloc(actual_size);
	strcpy(ret, value);
	cur = ret + size;
	memset(cur, ' ', actual_size - size);
	ret[actual_size - 1] = '\0';
	
	return ret;
}

/*
 * make_val_with_blanks
 *
 * Pad one string value with blanks, so the size will corespond to the fixedwidth 
 * required by the format. Make sure to encode the string into external table
 * encoding before writing it out (if conversion is needed).
 *
 * Arguments:
 *  value           - the field value in string format 
 *  field_size      - the fixedwidth field size, that is required for the value, for it to be added on the output line
 *  buf             - The temporary field buffer used for field value expansion - so it will reach the fixedwidth size
 *
 * Returns:
 * 	blank padded    - padded value of size field_size, in case value is NULL the return string will contain only blanks
 *  value
 */
static char *
make_val_with_blanks(FunctionCallInfo fcinfo, char *value, int field_size, StringInfo buf)
{
	int			sz;

	resetStringInfo(buf);

	if (value)
	{
		appendStringInfoString(buf, value);

		/*
		 * convert value from server encoding to external table encoding. Since
		 * it is possible that the encoded string will vary in size from the
		 * original string we must re-adjust if necessary
		 */
		encoding_encode_strinfo(fcinfo, buf, false /* export */);
	}

	/*
	 * Error out if the value is too large, and pad with spaces if it's too
	 * small.
	 */
	sz = buf->len;
	if (sz > field_size)
		ereport(ERROR,
				(errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
				 errmsg("The size of the value cannot be bigger than the field size value: %s, size: %d, field_size %d",
						value, sz, field_size)));
	appendStringInfoFill(buf, field_size - sz, ' ');

	return buf->data;
}

static void
fill_null_with_blanks_list(FormatConfig *format_in_config)
{
	ListCell *curSize;
	int field_size;
	
	foreach(curSize, format_in_config->fldSizes)
	{
		field_size = lfirst_int(curSize);
		format_in_config->fldNullsWithBlanks = lappend(format_in_config->fldNullsWithBlanks, 
													   makeString(make_null_val_with_blanks(format_in_config->null_value, field_size)));
	}
}

/*
 * validate_format_params
 *
 * verifies that every field specified in the table creation list is also present in the formatting string
 * and vice versa
 */
static void
validate_format_params(FormatConfig *format_in_config, TupleDesc tupdesc)
{
	ListCell   *l;
	int num_fields_in_format_string = list_length(format_in_config->fldNames);
	int num_fields_in_table_list = tupdesc->natts;
	
	if (num_fields_in_format_string != num_fields_in_table_list)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_COLUMN),
				errmsg("the fixed width formatter requires a length specification for each one of the"
					   " external table columns being used (currently <%d>, however format string has <%d>",
					   num_fields_in_table_list, num_fields_in_format_string)));
	}
	
	foreach(l, format_in_config->fldNames)
	{
		int i;
		bool is_in_both_lists = false;
		char *name = strVal(lfirst(l));
		for (i = 0; i < num_fields_in_table_list; i++)
		{
			if (namestrcmp(&(tupdesc->attrs[i]->attname), name) == 0)
			{
				is_in_both_lists = true;
				break;
			}			
		}
		
		if (is_in_both_lists == false)
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("the fixed width formatter requires a length specification for each one of the "
							"external table columns being used (missing field <%s>", name)));			
		}
	}
}

static void
init_format_in_config(FormatConfig *format_in_config, int ncolumns, TupleDesc tupdesc, FunctionCallInfo fcinfo)
{
	load_format_config(format_in_config, fcinfo);
	validate_format_params(format_in_config, tupdesc);
	
	if (format_in_config->null_value != NULL)
	{
		fill_null_with_blanks_list(format_in_config);
	}
	
	format_in_config->conv_functions = FORMATTER_GET_CONVERSION_FUNCS(fcinfo);
	format_in_config->typioparams = FORMATTER_GET_TYPIOPARAMS(fcinfo);	
	format_in_config->fldIndexes = CopyGetAttnums(tupdesc, FORMATTER_GET_RELATION(fcinfo), format_in_config->fldNames);	
}

static void
init_format_out_config(FormatConfig *format_out_config, int ncolumns, TupleDesc tupdesc, FunctionCallInfo fcinfo)
{
	load_format_config(format_out_config, fcinfo);
	validate_format_params(format_out_config, tupdesc);
	
	if (format_out_config->null_value != NULL)
	{
		fill_null_with_blanks_list(format_out_config);
	}
	
	format_out_config->conv_functions = FORMATTER_GET_CONVERSION_FUNCS(fcinfo);	
	format_out_config->fldIndexes = CopyGetAttnums(tupdesc, FORMATTER_GET_RELATION(fcinfo), format_out_config->fldNames);	
}

static void 
get_tuple_info(TupleDesc tupdesc, int *r_ncolumns, format_t **r_myData, char **data, 
			   FunctionCallInfo fcinfo, FormatConfig *format_out_config)
{
	HeapTupleData		tuple;	
	HeapTupleHeader		rec	= PG_GETARG_HEAPTUPLEHEADER(0);	
	/* Get our internal description of the formatter */
	*r_ncolumns = tupdesc->natts;
	int ncolumns = *r_ncolumns;	
	*r_myData = (format_t *) FORMATTER_GET_USER_CTX(fcinfo);
	format_t *myData = *r_myData;
	
	if (myData == NULL)
	{
		myData          = palloc(sizeof(format_t));
		*r_myData       = myData;
		
		myData->ncols   = ncolumns;
		myData->values  = palloc(sizeof(Datum) * ncolumns);
		myData->nulls   = palloc(sizeof(bool) * ncolumns);
		initStringInfo( &(myData->one_field) );
		
		
		init_format_out_config(format_out_config, ncolumns, tupdesc, fcinfo);
		
		/* Determine required buffer size */
		myData->buflen = format_out_config->fields_tot_size + strlen(format_out_config->line_delimiter);
		myData->buflen = Max(128, myData->buflen);  /* allocate at least 128 bytes */
		myData->buffer = palloc(myData->buflen + VARHDRSZ);
		
		FORMATTER_SET_USER_CTX(fcinfo, myData);
	}
	if (myData->ncols != ncolumns)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("formatter_export: unexpected change of output record type")));	
	
	/* break the input tuple into fields */
	tuple.t_len = HeapTupleHeaderGetDatumLength(rec);
	ItemPointerSetInvalid(&(tuple.t_self));
	tuple.t_data = rec;
	heap_deform_tuple(&tuple, tupdesc, myData->values, myData->nulls);
	*data = VARDATA(myData->buffer);
	
}

static int
get_actual_line_size(FormatConfig *format_in_config, char *line_start, int cur_size, int tot_size, PG_FUNCTION_ARGS)
{
	int   row_size;
	int   actual_fields_size;
	int   remaining;
	char *line_end; 
	char *expected_delim_loc = line_start + format_in_config->fields_tot_size;
	
	/*
	 * the case where there is no line delimiter
	 */
	if ( 0 == format_in_config->line_delimiter_length )
	{
		return format_in_config->fields_tot_size;
	}

	if ( 1 == format_in_config->line_delimiter_length )
	{
		char delim = format_in_config->line_delimiter[0];
				
		if ( *expected_delim_loc == delim )
			line_end = expected_delim_loc;
		else
			line_end = strchr(line_start, delim);
		
	}
	else /* > 1 */
	{
		int i;
		bool as_expected = true;
		
		for ( i = 0; i < format_in_config->line_delimiter_length; i++)
		{
			if ( expected_delim_loc[i] != format_in_config->line_delimiter[i] )
			{
				as_expected = false;
				break;
			}
		}
		
		if ( as_expected )
			line_end = expected_delim_loc;
		else
			line_end = strstr(line_start, format_in_config->line_delimiter);
	}
	
	/*
	 * line_end will be 0, if strchr or strstr did not find the delimiter.
	 * In this case we throw an exception  ( unless this is the last line in the buffer )--> The line delimiter specified in
	 * FormatConfig must be present in the file.
	 */
	if ( 0 == line_end /*did not find delimiter*/ )
	{
		remaining = tot_size - cur_size - format_in_config->fields_tot_size;
		if (1 == remaining) /* we are at the last line so we cannot find a custom delimiter - we have an OS line delimiter here */
		{
			return (format_in_config->fields_tot_size + 1);
		}
		
		/*
		 * this is the case where the last line in the buffer is incomplete, that's why the end of line was not found.
		 * the rest of the line is in the next buffer
		 */  
		if ( (tot_size - cur_size) < format_in_config->fields_tot_size )
		{
			return tot_size - cur_size;
		}
		
		/* 
		 * if we are here, it means the file simply does not contain the line delimiter specified in the formatter string.
		 * so we throw an exception
		 */
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
	             errmsg("The line delimiter specified in the Formatter arguments: <%s> is not located in the data file", 
						format_in_config->line_delimiter)));			
	}

	actual_fields_size = line_end - line_start; 
	if ( actual_fields_size != format_in_config->fields_tot_size )
	{
		int total_actual_field_size = actual_fields_size + format_in_config->line_delimiter_length;
		
		FORMATTER_SET_BAD_ROW_DATA(fcinfo, line_start, total_actual_field_size);
		FORMATTER_SET_BYTE_NUMBER(fcinfo, total_actual_field_size);
		
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
	             errmsg("Expected line size from the formatting string: %d, but the actual size is: %d", 
						format_in_config->fields_tot_size, actual_fields_size)));			
	}
	
	row_size = actual_fields_size + format_in_config->line_delimiter_length;
	return row_size;
}


Datum 
fixedwidth_out(PG_FUNCTION_ARGS)
{
	TupleDesc           tupdesc;
	MemoryContext 		m, oldcontext;
	int                 ncolumns = 0;
	format_t           *myData;
	char               *data;
	int                 datlen = 0;
	ListCell           *curIdx; 
	ListCell           *curSize;
	int                 field_size;
	char               *mapped_val;
	char               *mapped_val_with_blanks;
	bool		        isnull;
	Datum		        value;
	int			        idx;
	static FormatConfig format_out_config;
	
	/* Must be called via the external table format manager */
	if (!CALLED_AS_FORMATTER(fcinfo))
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("fixedwidth_out: not called by format manager")));	
	
	
	tupdesc = FORMATTER_GET_TUPDESC(fcinfo);
	get_tuple_info(tupdesc, &ncolumns, &myData, &data, fcinfo, &format_out_config);
	
	/* =======================================================================
	 *                            MAIN FORMATTING CODE
	 * ======================================================================= */
	m = FORMATTER_GET_PER_ROW_MEM_CTX(fcinfo); 
	oldcontext = MemoryContextSwitchTo(m); 
	
	forboth(curIdx, format_out_config.fldIndexes, curSize, format_out_config.fldSizes)
	{
		field_size = lfirst_int(curSize);
		idx  = lfirst_int(curIdx) - 1;		
		isnull = myData->nulls[idx];
		value = myData->values[idx];

		if ( isnull )
		{
			mapped_val_with_blanks = make_val_with_blanks(fcinfo, format_out_config.null_value, field_size, &(myData->one_field));
		}
		else 
		{
			mapped_val = OutputFunctionCall(&format_out_config.conv_functions[idx], value);
			mapped_val_with_blanks = make_val_with_blanks(fcinfo, mapped_val, field_size, &(myData->one_field));
		}

		memcpy(&data[datlen], mapped_val_with_blanks, field_size);
		datlen += field_size;
	}
	
	memcpy(&data[datlen], format_out_config.line_delimiter, format_out_config.line_delimiter_length);
	datlen += format_out_config.line_delimiter_length;
	
	MemoryContextSwitchTo(oldcontext);
	/* ======================================================================= */
	
	SET_VARSIZE(myData->buffer, datlen + VARHDRSZ);
		
	PG_RETURN_BYTEA_P(myData->buffer);
}

/*
 * fixedwidth_in
 * each time this function is called, it builds one tuple from the input data buffer
 */
Datum 
fixedwidth_in(PG_FUNCTION_ARGS)
{	
	HeapTuple			tuple;
	TupleDesc           tupdesc;
	MemoryContext 		m, oldcontext;
	format_t           *myData;
	char               *data_buf;
	int                 ncolumns = 0;
	int			  		data_cur;
	int                 data_len;
	bool                saw_eof;
	bool				eof_is_lf;
	ListCell           *curIdx; 
	ListCell           *curSize;
	ListCell           *cur_null_with_blanks = NULL;	
	int		            remaining;
	int                 field_size;
	int   				row_size;
	char               *nullval;
	int			        idx;
	char               *null_val_with_blanks;
	static FormatConfig format_in_config;
	
	/* Must be called via the external table format manager */
	if (!CALLED_AS_FORMATTER(fcinfo))
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("fixedwidth_in: not called by format manager")));
		
	tupdesc = FORMATTER_GET_TUPDESC(fcinfo);
	
	/* Get our internal description of the formatter */
	ncolumns = tupdesc->natts;	
	myData = (format_t *) FORMATTER_GET_USER_CTX(fcinfo);
	
	if (myData == NULL)
	{
		init_format_in_config(&format_in_config, ncolumns, tupdesc, fcinfo);
		init_format_t(&myData, ncolumns, fcinfo);			
	}
	
	/* start clean */
	MemSet(myData->values, 0, ncolumns * sizeof(Datum));
	MemSet(myData->nulls, true, ncolumns * sizeof(bool));

	/* get our input data buf and number of valid bytes in it */
	data_buf = FORMATTER_GET_DATABUF(fcinfo);
	data_len = FORMATTER_GET_DATALEN(fcinfo); 
	data_cur = FORMATTER_GET_DATACURSOR(fcinfo);
	saw_eof  = FORMATTER_GET_SAW_EOF(fcinfo);

	eof_is_lf = (format_in_config.line_delimiter[0] == '\n' ? true : false);

	/* =======================================================================
	 *                            MAIN FORMATTING CODE
	 * ======================================================================= */	
	/*
	 * tuple data extraction is done in a separate memory context
	 */
	m = FORMATTER_GET_PER_ROW_MEM_CTX(fcinfo); 
	oldcontext = MemoryContextSwitchTo(m); 
		
	/*
	 * if data_cur == data_len, it means we finished the current buffer, we will not do any formatting,
	 * instead inside forboth loop we will fall inside "if (remaining < field_size)", so there is NO need to
	 * set the BAD_ROW_DATA error string ---> there will be no formatting errors that throw exceptions
	 */
	if (data_cur < data_len)
	{
		/* setting the line number for "line size" exceptions that might be thrown in get_actual_line_size */
		FORMATTER_SET_BAD_ROW_NUM(fcinfo, myData->lineno); 
		/*
		 * myData->lineno represents the line number in the datafile, when the file was opened
		 * with a conventional editor, so we increase the lineno only when the delimiter is LF
		 */
		if (eof_is_lf)
			myData->lineno++;	
		row_size = get_actual_line_size(&format_in_config, data_buf + data_cur, data_cur, data_len, fcinfo);

		FORMATTER_SET_BAD_ROW_DATA(fcinfo, data_buf + data_cur, row_size);
		FORMATTER_SET_BYTE_NUMBER(fcinfo, row_size);
	}
	else 
	{
		/*
		 * This line is not finish. Next buffer will bring the remaining of the line.
		 * So the line number shpuld not grow.
		 */
		if (eof_is_lf)
			myData->lineno--;			
		
		MemoryContextSwitchTo(oldcontext);
		FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA);				
	}
	
	/*
	 * Encoding of client data to server encoding.
	 *
	 * Ideally we would run a conversion over a line of data and be done.
	 * However, this may change the byte offsets and mess up with the fixed
	 * width of the input data.
	 *
	 * As we want to avoid encoding conversion when necessary (for performance)
	 * We first run a test on a whole line and see if it passes input encoding
	 * validation. if not, an error is emitted. if yes, we make a note whether
	 * the input string was actually modified or not and take note of it in the
	 * convert boolean. In most cases 'convert' will remain false and we're done.
	 * In cases where it is true we postpone the actual conversion of values to
	 * a later stage (per attribute) in order to keep the formatter clean.
	 */
	myData->convert = encoding_check_str(fcinfo, data_buf + data_cur, row_size, true);


	if (format_in_config.null_value != NULL)
		cur_null_with_blanks = list_head(format_in_config.fldNullsWithBlanks);
	
	forboth(curIdx, format_in_config.fldIndexes, curSize, format_in_config.fldSizes)
	{
		remaining	= 0;
		field_size = lfirst_int(curSize);
		nullval = format_in_config.null_value;		
		remaining = data_len - data_cur;
		
		if (remaining < field_size)
		{
			/*
			 * we will get here only in the case we are working without a line delimiter. Because "remaining smaller then fieldsize"
			 * means that our actual line is smaller than expected size, and if we have a line delimiter this problem will be discovered
			 * in function  get_actual_line_size which is called above.
			 */
			
			if (saw_eof && (remaining > 1))
			{
				data_cur += remaining;
				FORMATTER_SET_DATACURSOR(fcinfo, data_cur);
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("Last line in the file contains an incomplete tuple")));
				
			}
			else if (saw_eof && (remaining == 1))
			{
				/* we are in a case of no line delimiter, but the end of the file contains one EOL */
				data_cur += remaining;
				FORMATTER_SET_DATACURSOR(fcinfo, data_cur);
				MemoryContextSwitchTo(oldcontext);
				FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA);				
				
			}			 
			else 
			{
				/*
				 * This line is not finish. Next buffer will bring the remaining of the line.
				 * So the line number shpuld not grow.
				 */
				if (eof_is_lf)
					myData->lineno--;			
				
				MemoryContextSwitchTo(oldcontext);
				FORMATTER_RETURN_NOTIFICATION(fcinfo, FMT_NEED_MORE_DATA);				
			}

		}
		
		resetStringInfo(&(myData->one_val));
		
		idx  = lfirst_int(curIdx) - 1;
		
		if (format_in_config.preserve_blanks == 0)
		{
			/* extract field value while ignoring blanks */
			extract_field(data_buf + data_cur, field_size, false, &(myData->one_val));

			/*
			 * there are two (2) cases when we set value to null:
			 * a. there is a null value defined in the formatter arguments, and this value was found in the field
			 * b. there is no null value defined and the field contained only blanks
			 */ 
			if ( !( (nullval != NULL) && (strcmp(myData->one_val.data, nullval) == 0) )  &&
				 /* we are not in case a */
				 !( (nullval == NULL) && (myData->one_val.data[0] == '\0') ) )
				 /* and also not in case b */
			{			
				/* perform encoding conversion on field value if needed */
				if(myData->convert)
					encoding_encode_strinfo(fcinfo, &(myData->one_val), true);

				myData->values[idx] = InputFunctionCall(&format_in_config.conv_functions[idx],
											  myData->one_val.data,
											  format_in_config.typioparams[idx],
											  tupdesc->attrs[idx]->atttypmod);
				myData->nulls[idx] = false;
			}
		}
		else 
		{
			if (nullval == NULL || cur_null_with_blanks == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("A null_value was not defined. When preserve_blanks is on, a null_value \
								 must be defined in the formatter arguments string")));
			
			/* extract field value while treating blanks as data */
			extract_field(data_buf + data_cur, field_size, true, &(myData->one_val));

			null_val_with_blanks = strVal(lfirst(cur_null_with_blanks));
			cur_null_with_blanks = lnext(cur_null_with_blanks);
			
			if (strcmp(myData->one_val.data, null_val_with_blanks) != 0)
			{
				/* perform encoding conversion on field value if needed */
				if(myData->convert)
					encoding_encode_strinfo(fcinfo, &(myData->one_val), true);

				myData->values[idx] = InputFunctionCall(&format_in_config.conv_functions[idx],
													  myData->one_val.data,
													  format_in_config.typioparams[idx],
													  tupdesc->attrs[idx]->atttypmod);
				myData->nulls[idx] = false;
			}
		}
		data_cur += field_size;
	}	
	
	/*
	 * go over the line delimiter
	 */
	remaining = data_len - data_cur;
	if (remaining > 1)
	{
		data_cur += format_in_config.line_delimiter_length;
	}
	else if (remaining == 1)
	{
		data_cur += 1;
	}

	/*
	 * wrapping up
	 */
	MemoryContextSwitchTo(oldcontext);
	/* ======================================================================= */
	
	FORMATTER_SET_DATACURSOR(fcinfo, data_cur);
	tuple = heap_form_tuple(tupdesc, myData->values, myData->nulls);
	FORMATTER_SET_TUPLE(fcinfo, tuple);
	FORMATTER_RETURN_TUPLE(tuple);
}
