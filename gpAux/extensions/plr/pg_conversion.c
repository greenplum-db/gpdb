/*
 * PL/R - PostgreSQL support for R as a
 *	      procedural language (PL)
 *
 * Copyright (c) 2003-2013 by Joseph E. Conway
 * ALL RIGHTS RESERVED
 * 
 * Joe Conway <mail@joeconway.com>
 * 
 * Based on pltcl by Jan Wieck
 * and inspired by REmbeddedPostgres by
 * Duncan Temple Lang <duncan@research.bell-labs.com>
 * http://www.omegahat.org/RSPostgres/
 *
 * License: GPL version 2 or newer. http://www.gnu.org/copyleft/gpl.html
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * pg_conversion.c - functions for converting arguments from pg types to
 *                   R types, and for converting return values from R types
 *                   to pg types
 */
#include "plr.h"

static void pg_get_one_r(char *value, Oid arg_out_fn_oid, SEXP *obj,
																int elnum);
static SEXP get_r_vector(Oid typtype, int numels);
static Datum get_trigger_tuple(SEXP rval, plr_function *function,
									FunctionCallInfo fcinfo, bool *isnull);
static Datum get_tuplestore(SEXP rval, plr_function *function,
									FunctionCallInfo fcinfo, bool *isnull);
static Datum get_simple_array_datum(SEXP rval, Oid typelem, bool *isnull);
static Datum get_array_datum(SEXP rval, plr_function *function, int col, bool *isnull);
static Datum get_frame_array_datum(SEXP rval, plr_function *function, int col,
																bool *isnull);
static Datum get_md_array_datum(SEXP rval, int ndims, plr_function *function, int col,
																bool *isnull);
static Datum get_generic_array_datum(SEXP rval, plr_function *function, int col,
																bool *isnull);
static Tuplestorestate *get_frame_tuplestore(SEXP rval,
											 plr_function *function,
											 AttInMetadata *attinmeta,
											 MemoryContext per_query_ctx,
											 bool retset);
static Tuplestorestate *get_matrix_tuplestore(SEXP rval,
											 plr_function *function,
											 AttInMetadata *attinmeta,
											 MemoryContext per_query_ctx,
											 bool retset);
static Tuplestorestate *get_generic_tuplestore(SEXP rval,
											 plr_function *function,
											 AttInMetadata *attinmeta,
											 MemoryContext per_query_ctx,
											 bool retset);
static SEXP coerce_to_char(SEXP rval);

extern char *last_R_error_msg;

/*
 * given a scalar pg value, convert to a one row R vector
 */
SEXP
pg_scalar_get_r(Datum dvalue, Oid arg_typid, FmgrInfo arg_out_func)
{
	SEXP		result;

	/* add our value to it */
	if (arg_typid != BYTEAOID)
	{
		char	   *value;

		value = DatumGetCString(FunctionCall3(&arg_out_func,
											  dvalue,
								 			  (Datum) 0,
											  Int32GetDatum(-1)));

		/* get new vector of the appropriate type, length 1 */
		PROTECT(result = get_r_vector(arg_typid, 1));
		pg_get_one_r(value, arg_typid, &result, 0);
		UNPROTECT(1);
	}
	else
	{
		SEXP 	s, t, obj;
		int		status;
		Datum	dt_dvalue =  PointerGetDatum(PG_DETOAST_DATUM(dvalue));
		int		bsize = VARSIZE((bytea *) dt_dvalue);

		PROTECT(obj = get_r_vector(arg_typid, bsize));
		memcpy((char *) RAW(obj),
			   VARDATA((bytea *) dt_dvalue),
			   bsize);

		/*
		 * Need to construct a call to
		 * unserialize(rval)
		 */
		PROTECT(t = s = allocList(2));
		SET_TYPEOF(s, LANGSXP);
		SETCAR(t, install("unserialize"));
		t = CDR(t);
		SETCAR(t, obj);

		PROTECT(result = R_tryEval(s, R_GlobalEnv, &status));
		if(status != 0)
		{
			if (last_R_error_msg)
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("R interpreter expression evaluation error"),
						 errdetail("%s", last_R_error_msg)));
			else
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("R interpreter expression evaluation error"),
						 errdetail("R expression evaluation error caught in \"unserialize\".")));
		}

		UNPROTECT(3);
	}

	return result;
}


/*
 * Given an array pg value, convert to a multi-row R vector.
 */
SEXP
pg_array_get_r(Datum dvalue, FmgrInfo out_func, int typlen, bool typbyval, char typalign)
{
	/*
	 * Loop through and convert each scalar value.
	 * Use the converted values to build an R vector.
	 */
	SEXP		result;
	ArrayType  *v;
	Oid			element_type;
	int			i, j, k,
				nitems,
				nr = 1,
				nc = 1,
				nz = 1,
				ndim,
			   *dim;
	int			elem_idx = 0;
	Datum	   *elem_values;
	bool	   *elem_nulls;
	bool		fast_track_type;

	/* short-circuit for NULL datums */
	if (dvalue == (Datum) NULL)
		return R_NilValue;

	v = DatumGetArrayTypeP(dvalue);
	ndim = ARR_NDIM(v);
	element_type = ARR_ELEMTYPE(v);
	dim = ARR_DIMS(v);
	nitems = ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));

	switch (element_type)
	{
		case INT4OID:
		case FLOAT8OID:
			fast_track_type = true;
			break;
		default:
			fast_track_type = false;
	}

	/*
	 * Special case for pass-by-value data types, if the following conditions are met:
	 * 		designated fast_track_type
	 * 		no NULL elements
	 * 		1 dimensional array only
	 * 		at least one element
	 */
	if (fast_track_type &&
		 typbyval &&
		 !ARR_HASNULL(v) &&
		 (ndim == 1) &&
		 (nitems > 0))
	{
		char	   *p = ARR_DATA_PTR(v);

		/* get new vector of the appropriate type and length */
		PROTECT(result = get_r_vector(element_type, nitems));

		/* keep this in sync with switch above -- fast_track_type only */
		switch (element_type)
		{
			case INT4OID:
				Assert(sizeof(int) == 4);
				memcpy(INTEGER_DATA(result), p, nitems * sizeof(int));
				break;
			case FLOAT8OID:
				Assert(sizeof(double) == 8);
				memcpy(NUMERIC_DATA(result), p, nitems * sizeof(double));
				break;
			default:
				/* Everything else is error */
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("direct array passthrough attempted for unsupported type")));
		}

		if (ndim > 0)
		{
			SEXP	matrix_dims;

			/* attach dimensions */
			PROTECT(matrix_dims = allocVector(INTSXP, ndim));
			for (i = 0; i < ndim; i++)
				INTEGER_DATA(matrix_dims)[i] = dim[i];

			setAttrib(result, R_DimSymbol, matrix_dims);
			UNPROTECT(1);
		}

		UNPROTECT(1);	/* result */
	}
	else
	{
		deconstruct_array(v, element_type,
						  typlen, typbyval, typalign,
						  &elem_values, &elem_nulls, &nitems);

		/* array is empty */
		if (nitems == 0)
		{
			PROTECT(result = get_r_vector(element_type, nitems));
			UNPROTECT(1);

			return result;
		}

		if (ndim == 1)
			nr = nitems;
		else if (ndim == 2)
		{
			nr = dim[0];
			nc = dim[1];
		}
		else if (ndim == 3)
		{
			nr = dim[0];
			nc = dim[1];
			nz = dim[2];
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("greater than 3-dimensional arrays are not yet supported")));

		/* get new vector of the appropriate type and length */
		PROTECT(result = get_r_vector(element_type, nitems));

		/* Convert all values to their R form and build the vector */
		for (i = 0; i < nr; i++)
		{
			for (j = 0; j < nc; j++)
			{
				for (k = 0; k < nz; k++)
				{
					char	   *value;
					Datum		itemvalue;
					bool		isnull;
					int			idx = (k * nr * nc) + (j * nr) + i;

					isnull = elem_nulls[elem_idx];
					itemvalue = elem_values[elem_idx++];

					if (!isnull)
					{
						value = DatumGetCString(FunctionCall3(&out_func,
															  itemvalue,
															  (Datum) 0,
															  Int32GetDatum(-1)));
					}
					else
						value = NULL;

					/*
					 * Note that pg_get_one_r() replaces NULL values with
					 * the NA value appropriate for the data type.
					 */
					pg_get_one_r(value, element_type, &result, idx);
					if (value != NULL)
						pfree(value);
				}
			}
		}
		pfree(elem_values);
		pfree(elem_nulls);

		if (ndim > 0)
		{
			SEXP	matrix_dims;

			/* attach dimensions */
			PROTECT(matrix_dims = allocVector(INTSXP, ndim));
			for (i = 0; i < ndim; i++)
				INTEGER_DATA(matrix_dims)[i] = dim[i];

			setAttrib(result, R_DimSymbol, matrix_dims);
			UNPROTECT(1);
		}

		UNPROTECT(1);	/* result */
	}

	return result;
}

/*
 * Given an array pg datums, convert to a multi-row R vector.
 */
SEXP
pg_datum_array_get_r(Datum *elem_values, bool *elem_nulls, int numels, bool has_nulls,
					 Oid element_type, FmgrInfo out_func, bool typbyval)
{
	/*
	 * Loop through and convert each scalar value.
	 * Use the converted values to build an R vector.
	 */
	SEXP		result;
	int			i;
	bool		fast_track_type;

	switch (element_type)
	{
		case INT4OID:
		case FLOAT8OID:
			fast_track_type = true;
			break;
		default:
			fast_track_type = false;
	}

	/*
	 * Special case for pass-by-value data types, if the following conditions are met:
	 * 		designated fast_track_type
	 * 		no NULL elements
	 * 		1 dimensional array only
	 * 		at least one element
	 */
	if (fast_track_type &&
		 typbyval &&
		 !has_nulls &&
		 (numels > 0))
	{
		SEXP	matrix_dims;

		/* get new vector of the appropriate type and length */
		PROTECT(result = get_r_vector(element_type, numels));

		/* keep this in sync with switch above -- fast_track_type only */
		switch (element_type)
		{
			case INT4OID:
				Assert(sizeof(int) == 4);
				memcpy(INTEGER_DATA(result), elem_values, numels * sizeof(int));
				break;
			case FLOAT8OID:
				Assert(sizeof(double) == 8);
				memcpy(NUMERIC_DATA(result), elem_values, numels * sizeof(double));
				break;
			default:
				/* Everything else is error */
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("direct array passthrough attempted for unsupported type")));
		}

		/* attach dimensions */
		PROTECT(matrix_dims = allocVector(INTSXP, 1));
		INTEGER_DATA(matrix_dims)[0] = numels;
		setAttrib(result, R_DimSymbol, matrix_dims);
		UNPROTECT(1);

		UNPROTECT(1);	/* result */
	}
	else
	{
		SEXP	matrix_dims;

		/* array is empty */
		if (numels == 0)
		{
			PROTECT(result = get_r_vector(element_type, 0));
			UNPROTECT(1);

			return result;
		}

		/* get new vector of the appropriate type and length */
		PROTECT(result = get_r_vector(element_type, numels));

		/* Convert all values to their R form and build the vector */
		for (i = 0; i < numels; i++)
		{
			char	   *value;
			Datum		itemvalue;
			bool		isnull;

			isnull = elem_nulls[i];
			itemvalue = elem_values[i];

			if (!isnull)
			{
				value = DatumGetCString(FunctionCall3(&out_func,
													  itemvalue,
													  (Datum) 0,
													  Int32GetDatum(-1)));
			}
			else
				value = NULL;

			/*
			 * Note that pg_get_one_r() replaces NULL values with
			 * the NA value appropriate for the data type.
			 */
			pg_get_one_r(value, element_type, &result, i);
			if (value != NULL)
				pfree(value);
		}

		/* attach dimensions */
		PROTECT(matrix_dims = allocVector(INTSXP, 1));
		INTEGER_DATA(matrix_dims)[0] = numels;
		setAttrib(result, R_DimSymbol, matrix_dims);
		UNPROTECT(1);

		UNPROTECT(1);	/* result */
	}

	return result;
}

/*
 * Given an array of pg tuples, convert to an R list
 * the created object is not quite actually a data.frame
 */
SEXP
pg_tuple_get_r_frame(int ntuples, HeapTuple *tuples, TupleDesc tupdesc)
{
	int			nr = ntuples;
	int			nc = tupdesc->natts;
	int			nc_non_dropped = 0;
	int			df_colnum = 0;
	int			i = 0;
	int			j = 0;
	Oid			element_type;
	Oid			typelem;
	SEXP		names;
	SEXP		row_names;
	char		buf[256];
	SEXP		result;
	SEXP		fldvec;

	if (tuples == NULL || ntuples < 1)
		return R_NilValue;

	/* Count non-dropped attributes so we can later ignore the dropped ones */
	for (j = 0; j < nc; j++)
	{
		if (!tupdesc->attrs[j]->attisdropped)
			nc_non_dropped++;
	}

	/*
	 * Allocate the data.frame initially as a list,
	 * and also allocate a names vector for the column names
	 */
	PROTECT(result = NEW_LIST(nc_non_dropped));
	PROTECT(names = NEW_CHARACTER(nc_non_dropped));

	/*
	 * Loop by columns
	 */
	for (j = 0; j < nc; j++)		
	{
		int16		typlen;
		bool		typbyval;
		char		typdelim;
		Oid			typoutput,
					typioparam;
		FmgrInfo	outputproc;
		char		typalign;

		/* ignore dropped attributes */
		if (tupdesc->attrs[j]->attisdropped)
			continue;

		/* set column name */
		SET_COLUMN_NAMES;

		/* get column datatype oid */
		element_type = SPI_gettypeid(tupdesc, j + 1);

		/*
		 * Check to see if it is an array type. get_element_type will return
		 * InvalidOid instead of actual element type if the type is not a
		 * varlena array.
		 */
		typelem = get_element_type(element_type);

		/* get new vector of the appropriate type and length */
		if (typelem == InvalidOid)
			PROTECT(fldvec = get_r_vector(element_type, nr));
		else
		{
			PROTECT(fldvec = NEW_LIST(nr));
			get_type_io_data(typelem, IOFunc_output, &typlen, &typbyval,
							 &typalign, &typdelim, &typioparam, &typoutput);

			fmgr_info(typoutput, &outputproc);
		}

		/* loop rows for this column */
		for (i = 0; i < nr; i++)
		{
			if (typelem == InvalidOid)
			{
				/* not an array type */
				char	   *value;

				value = SPI_getvalue(tuples[i], tupdesc, j + 1);
				pg_get_one_r(value, element_type, &fldvec, i);
			}
			else
			{
				/* array type */
				Datum		dvalue;
				bool		isnull;
				SEXP		fldvec_elem;

				dvalue = SPI_getbinval(tuples[i], tupdesc, j + 1, &isnull);
				if (!isnull)
					PROTECT(fldvec_elem = pg_array_get_r(dvalue, outputproc, typlen, typbyval, typalign));
				else
					PROTECT(fldvec_elem = R_NilValue);

				SET_VECTOR_ELT(fldvec, i, fldvec_elem);
				UNPROTECT(1);
			}
		}

		SET_VECTOR_ELT(result, df_colnum, fldvec);
		UNPROTECT(1);
		df_colnum++;
	}

	/* attach the column names */
	setAttrib(result, R_NamesSymbol, names);

	/* attach row names - basically just the row number, zero based */
	PROTECT(row_names = allocVector(STRSXP, nr));
	for (i=0; i<nr; i++)
	{
		sprintf(buf, "%d", i+1);
		SET_STRING_ELT(row_names, i, COPY_TO_USER_STRING(buf));
	}
	setAttrib(result, R_RowNamesSymbol, row_names);

	/* finally, tell R we are a data.frame */
	setAttrib(result, R_ClassSymbol, mkString("data.frame"));

	UNPROTECT(3);
	return result;
}

/*
 * create an R vector of a given type and size based on pg output function oid
 */
static SEXP
get_r_vector(Oid typtype, int numels)
{
	SEXP	result;

	switch (typtype)
	{
		case OIDOID:
		case INT2OID:
		case INT4OID:
			/* 2 and 4 byte integer pgsql datatype => use R INTEGER */
			PROTECT(result = NEW_INTEGER(numels));
			break;
		case INT8OID:
		case FLOAT4OID:
		case FLOAT8OID:
		case CASHOID:
		case NUMERICOID:
			/*
			 * Other numeric types => use R REAL
			 * Note pgsql int8 is mapped to R REAL
			 * because R INTEGER is only 4 byte
			 */
			PROTECT(result = NEW_NUMERIC(numels));
			break;
		case BOOLOID:
			PROTECT(result = NEW_LOGICAL(numels));
			break;
		case BYTEAOID:
			PROTECT(result = NEW_RAW(numels));
			break;
		default:
			/* Everything else is defaulted to string */
			PROTECT(result = NEW_CHARACTER(numels));
	}
	UNPROTECT(1);

	return result;
}

/*
 * given a single non-array pg value, convert to its R value representation
 */
static void
pg_get_one_r(char *value, Oid typtype, SEXP *obj, int elnum)
{
	switch (typtype)
	{
		case OIDOID:
		case INT2OID:
		case INT4OID:
			/* 2 and 4 byte integer pgsql datatype => use R INTEGER */
			if (value)
				INTEGER_DATA(*obj)[elnum] = atoi(value);
			else
				INTEGER_DATA(*obj)[elnum] = NA_INTEGER;
			break;
		case INT8OID:
		case FLOAT4OID:
		case FLOAT8OID:
		case CASHOID:
		case NUMERICOID:
			/*
			 * Other numeric types => use R REAL
			 * Note pgsql int8 is mapped to R REAL
			 * because R INTEGER is only 4 byte
			 */
			if (value)
				NUMERIC_DATA(*obj)[elnum] = atof(value);
			else
				NUMERIC_DATA(*obj)[elnum] = NA_REAL;
			break;
		case BOOLOID:
			if (value)
				LOGICAL_DATA(*obj)[elnum] = ((*value == 't') ? 1 : 0);
			else
				LOGICAL_DATA(*obj)[elnum] = NA_LOGICAL;
			break;
		default:
			/* Everything else is defaulted to string */
			if (value)
				SET_STRING_ELT(*obj, elnum, COPY_TO_USER_STRING(value));
			else
				SET_STRING_ELT(*obj, elnum, NA_STRING);
	}
}

/*
 * given an R value, convert to its pg representation
 */
Datum
r_get_pg(SEXP rval, plr_function *function, FunctionCallInfo fcinfo)
{
	bool	isnull = false;
	Datum	result;

	if (function->result_typid != BYTEAOID &&
		(TYPEOF(rval) == CLOSXP ||
		 TYPEOF(rval) == PROMSXP ||
		 TYPEOF(rval) == LANGSXP ||
		 TYPEOF(rval) == ENVSXP))
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("incorrect function return type"),
				 errdetail("R return value type cannot be mapped to PostgreSQL return type."),
				 errhint("Try BYTEA as the PostgreSQL return type.")));

	if (CALLED_AS_TRIGGER(fcinfo))
		result = get_trigger_tuple(rval, function, fcinfo, &isnull);
	else if (function->result_istuple || fcinfo->flinfo->fn_retset)
		result = get_tuplestore(rval, function, fcinfo, &isnull);
	else
	{
		/* short circuit if return value is Null */
		if (rval == R_NilValue || isNull(rval))	/* probably redundant */
		{
			fcinfo->isnull = true;
			return (Datum) 0;
		}

		if (function->result_elem == 0)
			result = get_scalar_datum(rval, function->result_typid, function->result_in_func, &isnull);
		else
			result = get_array_datum(rval, function, 0, &isnull);

	}

	if (isnull)
		fcinfo->isnull = true;

	return result;
}

/*
 * Similar to r_get_pg, given an R value, convert to its pg representation
 * Other than scalar, currently only prepared to be used with simple 1D vector
 */
Datum
get_datum(SEXP rval, Oid typid, Oid typelem, FmgrInfo in_func, bool *isnull)
{
	Datum	result;

	/* short circuit if return value is Null */
	if (rval == R_NilValue || isNull(rval))	/* probably redundant */
	{
		*isnull = true;
		return (Datum) 0;
	}

	if (typelem == InvalidOid)
		result = get_scalar_datum(rval, typid, in_func, isnull);
	else
		result = get_simple_array_datum(rval, typelem, isnull);

	return result;
}

static Datum
get_trigger_tuple(SEXP rval, plr_function *function, FunctionCallInfo fcinfo, bool *isnull)
{
	TriggerData	   *trigdata = (TriggerData *) fcinfo->context;
	TupleDesc		tupdesc = trigdata->tg_relation->rd_att;
	AttInMetadata  *attinmeta;
	MemoryContext	fn_mcxt;
	MemoryContext	oldcontext;
	int				nc;
	int				nr;
	char		  **values;
	HeapTuple		tuple = NULL;
	int				i, j;
	int				nc_dropped = 0;
	int				df_colnum = 0;
	SEXP			result;
	SEXP			dfcol;

	/* short circuit statement level trigger which always returns NULL */
	if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
	{
		/* special for triggers, don't set isnull flag */
		*isnull = false;
		return (Datum) 0;
	}

	/* short circuit if return value is Null */
	if (rval == R_NilValue || isNull(rval))	/* probably redundant */
	{
		/* special for triggers, don't set isnull flag */
		*isnull = false;
		return (Datum) 0;
	}

	if (isFrame(rval))
		nc = length(rval);
	else if (isMatrix(rval))
		nc = ncols(rval);
	else
		nc = 1;

	PROTECT(dfcol = VECTOR_ELT(rval, 0));
	nr = length(dfcol);
	UNPROTECT(1);

	if (nr != 1)
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("incorrect function return type"),
				 errdetail("function return value cannot have more " \
						   "than one row")));

	/*
	 * Count number of dropped attributes so we can add them back to
	 * the return tuple
	 */
	for (j = 0; j < nc; j++)
	{
		if (tupdesc->attrs[j]->attisdropped)
			nc_dropped++;
	}

	/*
	 * Check to make sure we have the same number of columns
	 * to return as there are attributes in the return tuple.
	 * Note that we have to account for the number of dropped
	 * columns.
	 *
	 * Note we will attempt to coerce the R values into whatever
	 * the return attribute type is and depend on the "in"
	 * function to complain if needed.
	 */
	if (nc + nc_dropped != tupdesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("returned tuple structure does not match table " \
						"of trigger event")));

	fn_mcxt = fcinfo->flinfo->fn_mcxt;
	oldcontext = MemoryContextSwitchTo(fn_mcxt);

	attinmeta = TupleDescGetAttInMetadata(tupdesc);

	/* coerce columns to character in advance */
	PROTECT(result = NEW_LIST(nc));
	for (j = 0; j < nc; j++)
	{
		PROTECT(dfcol = VECTOR_ELT(rval, j));
		if(!isFactor(dfcol))
		{
			SEXP	obj;

			PROTECT(obj = coerce_to_char(dfcol));
			SET_VECTOR_ELT(result, j, obj);
			UNPROTECT(1);
		}
		else
		{
			SEXP 	t;

			for (t = ATTRIB(dfcol); t != R_NilValue; t = CDR(t))
			{
				if(TAG(t) == R_LevelsSymbol)
				{
					PROTECT(SETCAR(t, coerce_to_char(CAR(t))));
					UNPROTECT(1);
					break;
				}
			}
			SET_VECTOR_ELT(result, j, dfcol);
		}

		UNPROTECT(1);
	}

	values = (char **) palloc((nc + nc_dropped) * sizeof(char *));
	for(i = 0; i < nr; i++)
	{
		for (j = 0; j < nc + nc_dropped; j++)
		{
			/* insert NULL for dropped attributes */
			if (tupdesc->attrs[j]->attisdropped)
				values[j] = NULL;
			else
			{
				PROTECT(dfcol = VECTOR_ELT(result, df_colnum));

				if(isFactor(dfcol))
				{
					SEXP t;
					for (t = ATTRIB(dfcol); t != R_NilValue; t = CDR(t))
					{
						if(TAG(t) == R_LevelsSymbol)
						{
							SEXP	obj;
							int		idx = INTEGER(dfcol)[i] - 1;

							PROTECT(obj = CAR(t));
							values[j] = pstrdup(CHAR(STRING_ELT(obj, idx)));
							UNPROTECT(1);

							break;
						}
					}
				}
				else
				{
					if (STRING_ELT(dfcol, 0) != NA_STRING)
						values[j] = pstrdup(CHAR(STRING_ELT(dfcol, i)));
					else
						values[j] = NULL;
				}

				UNPROTECT(1);
				df_colnum++;
			}
		}

		/* construct the tuple */
		tuple = BuildTupleFromCStrings(attinmeta, values);

		for (j = 0; j < nc; j++)
			if (values[j] != NULL)
				pfree(values[j]);
	}
	UNPROTECT(1);
	MemoryContextSwitchTo(oldcontext);

	if (tuple)
	{
		*isnull = false;
		return PointerGetDatum(tuple);
	}
	else
	{
		/* special for triggers, don't set isnull flag */
		*isnull = false;
		return (Datum) 0;
	}
}

static Datum
get_tuplestore(SEXP rval, plr_function *function, FunctionCallInfo fcinfo, bool *isnull)
{
	bool			retset = fcinfo->flinfo->fn_retset;
	ReturnSetInfo  *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc		tupdesc;
	AttInMetadata  *attinmeta;
	MemoryContext	per_query_ctx;
	MemoryContext	oldcontext;
	int				nc;

	/* check to see if caller supports us returning a tuplestore */
	if (!rsinfo || !(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("materialize mode required, but it is not "
						"allowed in this context")));

	if (isFrame(rval))
		nc = length(rval);
	else if (isList(rval) || isNewList(rval))
		nc = length(rval);
	else if (isMatrix(rval))
		nc = ncols(rval);
	else
		nc = 1;

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* get the requested return tuple description */
	tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);

	/*
	 * Check to make sure we have the same number of columns
	 * to return as there are attributes in the return tuple.
	 *
	 * Note we will attempt to coerce the R values into whatever
	 * the return attribute type is and depend on the "in"
	 * function to complain if needed.
	 */
	if (nc != tupdesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("query-specified return tuple and "
						"function returned data.frame are not compatible")));

	attinmeta = TupleDescGetAttInMetadata(tupdesc);

	/* OK, go to work */
	rsinfo->returnMode = SFRM_Materialize;

	if (isFrame(rval))
		rsinfo->setResult = get_frame_tuplestore(rval, function, attinmeta, per_query_ctx, retset);
	else if (isList(rval) || isNewList(rval))
		rsinfo->setResult = get_frame_tuplestore(rval, function, attinmeta, per_query_ctx, retset);
	else if (isMatrix(rval))
		rsinfo->setResult = get_matrix_tuplestore(rval, function, attinmeta, per_query_ctx, retset);
	else
		rsinfo->setResult = get_generic_tuplestore(rval, function, attinmeta, per_query_ctx, retset);

	/*
	 * SFRM_Materialize mode expects us to return a NULL Datum. The actual
	 * tuples are in our tuplestore and passed back through
	 * rsinfo->setResult. rsinfo->setDesc is set to the tuple description
	 * that we actually used to build our tuples with, so the caller can
	 * verify we did what it was expecting.
	 */
	rsinfo->setDesc = tupdesc;
	MemoryContextSwitchTo(oldcontext);

	*isnull = true;
	return (Datum) 0;
}

Datum
get_scalar_datum(SEXP rval, Oid result_typid, FmgrInfo result_in_func, bool *isnull)
{
	Datum		dvalue;
	SEXP		obj;
	const char *value;

	/*
	 * Element type is zero, we don't have an array, so coerce to string
	 * and take the first element as a scalar
	 *
	 * Exception: if result type is BYTEA, we want to return the whole
	 * object in serialized form
	 */
	if (result_typid != BYTEAOID)
	{
		PROTECT(obj = coerce_to_char(rval));
		if (STRING_ELT(obj, 0) == NA_STRING)
		{
			UNPROTECT(1);
			*isnull = true;
			dvalue = (Datum) 0;
			return dvalue;
		}
		value = CHAR(STRING_ELT(obj, 0));
		UNPROTECT(1);
		
		if (value != NULL)
		{
			dvalue = FunctionCall3(&result_in_func,
									CStringGetDatum(value),
									ObjectIdGetDatum(0),
									Int32GetDatum(-1));
		}
		else
		{
			*isnull = true;
			dvalue = (Datum) 0;
		}
	}
	else
	{
		SEXP 	s, t;
		int		len, rsize, status;
		bytea  *result;
		char   *rptr;

		/*
		 * Need to construct a call to
		 * serialize(rval, NULL)
		 */
		PROTECT(t = s = allocList(3));
		SET_TYPEOF(s, LANGSXP);
		SETCAR(t, install("serialize")); t = CDR(t);
		SETCAR(t, rval); t = CDR(t);
		SETCAR(t, R_NilValue);

		PROTECT(obj = R_tryEval(s, R_GlobalEnv, &status));
		if(status != 0)
		{
			if (last_R_error_msg)
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("R interpreter expression evaluation error"),
						 errdetail("%s", last_R_error_msg)));
			else
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("R interpreter expression evaluation error"),
						 errdetail("R expression evaluation error caught in \"serialize\".")));
		}
		len = LENGTH(obj);

		rsize = VARHDRSZ + len;
		result = (bytea *) palloc(rsize);
		SET_VARSIZE(result, rsize);
		rptr = VARDATA(result);
		memcpy(rptr, (char *) RAW(obj), rsize - VARHDRSZ);

		UNPROTECT(2);

		dvalue = PointerGetDatum(result);
	}

	return dvalue;
}

static Datum
get_array_datum(SEXP rval, plr_function *function, int col, bool *isnull)
{
	SEXP	rdims;
	int		ndims;
	int		objlen = length(rval);

	if (objlen > 0)
	{
		/* two supported special cases */
		if (isFrame(rval))
			return get_frame_array_datum(rval, function,  col, isnull);
		else if (isMatrix(rval))
			return get_md_array_datum(rval, 2 /* matrix is 2D */, function, col, isnull);

		PROTECT(rdims = getAttrib(rval, R_DimSymbol));
		ndims = length(rdims);
		UNPROTECT(1);

		/* 2D and 3D arrays are specifically supported too */
		if (ndims == 2 || ndims == 3)
			return get_md_array_datum(rval, ndims, function, col, isnull);

		/* everything else */
		return get_generic_array_datum(rval, function, col, isnull);
	}
	else
	{
		/* create an empty array */
		return PointerGetDatum(construct_empty_array(function->result_elem));
	}
}

static Datum
get_frame_array_datum(SEXP rval, plr_function *function, int col, bool *isnull)
{
	Datum		dvalue;
	SEXP		obj;
	const char *value;
	Oid			result_elem;
	FmgrInfo	in_func;
	int			typlen;
	bool		typbyval;
	char		typalign;
	int			i;
	Datum	   *dvalues = NULL;
	ArrayType  *array;
	int			nr = 0;
	int			nc = length(rval);
#define FIXED_NUM_DIMS		2
	int			ndims = FIXED_NUM_DIMS;
	int			dims[FIXED_NUM_DIMS];
	int			lbs[FIXED_NUM_DIMS];
#undef FIXED_NUM_DIMS
	int			idx;
	SEXP		dfcol = NULL;
	int			j;
	bool	   *nulls = NULL;
	bool		have_nulls = FALSE;

	if (nc < 1)
		/* internal error */
		elog(ERROR, "plr: bad internal representation of data.frame");

	if (function->result_istuple)
	{
		result_elem = function->result_fld_elem_typid[col];
		in_func = function->result_fld_elem_in_func[col];
		typlen = function->result_fld_elem_typlen[col];
		typbyval = function->result_fld_elem_typbyval[col];
		typalign = function->result_fld_elem_typalign[col];
	}
	else
	{
		result_elem = function->result_elem;
		in_func = function->result_elem_in_func;
		typlen = function->result_elem_typlen;
		typbyval = function->result_elem_typbyval;
		typalign = function->result_elem_typalign;
	}
	
	for (j = 0; j < nc; j++)
	{
		if (TYPEOF(rval) == VECSXP)
			PROTECT(dfcol = VECTOR_ELT(rval, j));
		else if (TYPEOF(rval) == LISTSXP)
		{
			PROTECT(dfcol = CAR(rval));
			rval = CDR(rval);
		}
		else
			/* internal error */
			elog(ERROR, "plr: bad internal representation of data.frame");

		/*
		 * Not sure about this test. Need to reliably detect
		 * factors and do the alternative assignment ONLY for them.
		 * For the moment this locution seems to work correctly.
		 */
		if (ATTRIB(dfcol) == R_NilValue ||
			TYPEOF(CAR(ATTRIB(dfcol))) != STRSXP)
			PROTECT(obj = coerce_to_char(dfcol));
		else
			PROTECT(obj = coerce_to_char(CAR(ATTRIB(dfcol))));

		if (j == 0)
		{
			nr = length(obj);
			dvalues = (Datum *) palloc(nr * nc * sizeof(Datum));
			nulls = (bool *) palloc(nr * nc * sizeof(bool));
		}

		for(i = 0; i < nr; i++)
		{
			value = CHAR(STRING_ELT(obj, i));
			idx = ((i * nc) + j);

			if (STRING_ELT(obj, i) == NA_STRING || value == NULL)
			{
				nulls[idx] = TRUE;
				have_nulls = TRUE;
			}
			else
			{
				nulls[idx] = FALSE;
				dvalues[idx] = FunctionCall3(&in_func,
										CStringGetDatum(value),
										(Datum) 0,
										Int32GetDatum(-1));
			}
		}
		UNPROTECT(2);
	}

	dims[0] = nr;
	dims[1] = nc;
	lbs[0] = 1;
	lbs[1] = 1;

	if (!have_nulls)
		array = construct_md_array(dvalues, NULL, ndims, dims, lbs,
									result_elem, typlen, typbyval, typalign);
	else
		array = construct_md_array(dvalues, nulls, ndims, dims, lbs,
									result_elem, typlen, typbyval, typalign);

	dvalue = PointerGetDatum(array);

	return dvalue;
}

/* return simple, one dimensional array */
static Datum
get_simple_array_datum(SEXP rval, Oid typelem, bool *isnull)
{
	Datum		dvalue;
	SEXP		obj;
	SEXP		rdims;
	const char *value;
	int16		typlen;
	bool		typbyval;
	char		typdelim;
	Oid			typinput,
				typioparam;
	FmgrInfo	in_func;
	char		typalign;
	int			i;
	Datum	   *dvalues = NULL;
	ArrayType  *array;
	int			nitems;
	int		   *dims;
	int		   *lbs;
	bool	   *nulls;
	bool		have_nulls = FALSE;
	int			ndims = 1;

	dims = palloc(ndims * sizeof(int));
	lbs = palloc(ndims * sizeof(int));

	/*
	 * get the element type's in_func
	 */
	get_type_io_data(typelem, IOFunc_output, &typlen, &typbyval,
					 &typalign, &typdelim, &typioparam, &typinput);

	perm_fmgr_info(typinput, &in_func);

	PROTECT(rdims = getAttrib(rval, R_DimSymbol));
	if (length(rdims) > 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("greater than 1-dimensional arrays are " \
						"not supported in this context")));

	dims[0] = INTEGER(rdims)[0];
	lbs[0] = 1;
	UNPROTECT(1);

	nitems = dims[0];
	if (nitems == 0)
	{
		*isnull = true;
		return (Datum) 0;
	}

	dvalues = (Datum *) palloc(nitems * sizeof(Datum));
	nulls = (bool *) palloc(nitems * sizeof(bool));
	PROTECT(obj =  coerce_to_char(rval));

	for (i = 0; i < nitems; i++)
	{
		value = CHAR(STRING_ELT(obj, i));

		if (STRING_ELT(obj, i) == NA_STRING || value == NULL)
		{
			nulls[i] = TRUE;
			have_nulls = TRUE;
		}
		else
		{
			nulls[i] = FALSE;
			dvalues[i] = FunctionCall3(&in_func,
										CStringGetDatum(value),
										(Datum) 0,
										Int32GetDatum(-1));
		}
	}
	UNPROTECT(1);

	if (!have_nulls)
		array = construct_md_array(dvalues, NULL, ndims, dims, lbs,
									typelem, typlen, typbyval, typalign);
	else
		array = construct_md_array(dvalues, nulls, ndims, dims, lbs,
									typelem, typlen, typbyval, typalign);

	dvalue = PointerGetDatum(array);

	return dvalue;
}

static Datum
get_md_array_datum(SEXP rval, int ndims, plr_function *function, int col, bool *isnull)
{
	Datum		dvalue;
	SEXP		obj;
	SEXP		rdims;
	const char *value;
	Oid			result_elem;
	FmgrInfo	in_func;
	int			typlen;
	bool		typbyval;
	char		typalign;
	int			i, j, k;
	Datum	   *dvalues = NULL;
	ArrayType  *array;
	int			nitems;
	int			nr = 1;
	int			nc = 1;
	int			nz = 1;
	int		   *dims;
	int		   *lbs;
	int			idx;
	int			cntr = 0;
	bool	   *nulls;
	bool		have_nulls = FALSE;

	if (ndims > 0)
	{
		dims = palloc(ndims * sizeof(int));
		lbs = palloc(ndims * sizeof(int));
	}
	else
	{
		dims = NULL;
		lbs = NULL;
	}
	
	if (function->result_istuple)
	{
		result_elem = function->result_fld_elem_typid[col];
		in_func = function->result_fld_elem_in_func[col];
		typlen = function->result_fld_elem_typlen[col];
		typbyval = function->result_fld_elem_typbyval[col];
		typalign = function->result_fld_elem_typalign[col];
	}
	else
	{
		result_elem = function->result_elem;
		in_func = function->result_elem_in_func;
		typlen = function->result_elem_typlen;
		typbyval = function->result_elem_typbyval;
		typalign = function->result_elem_typalign;
	}

	PROTECT(rdims = getAttrib(rval, R_DimSymbol));
	for(i = 0; i < ndims; i++)
	{
		dims[i] = INTEGER(rdims)[i];
		lbs[i] = 1;

		switch (i)
		{
			case 0:
				nr = dims[i];
				break;
			case 1:
				nc = dims[i];
				break;
			case 2:
				nz = dims[i];
				break;
			default:
				/* anything higher is currently unsupported */
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("greater than 3-dimensional arrays are " \
								"not yet supported")));
		}

	}
	UNPROTECT(1);

	nitems = nr * nc * nz;
	dvalues = (Datum *) palloc(nitems * sizeof(Datum));
	nulls = (bool *) palloc(nitems * sizeof(bool));
	PROTECT(obj =  coerce_to_char(rval));

	for (i = 0; i < nr; i++)
	{
		for (j = 0; j < nc; j++)
		{
			for (k = 0; k < nz; k++)
			{
				int		arridx = cntr++;

				idx = (k * nr * nc) + (j * nr) + i;
				value = CHAR(STRING_ELT(obj, idx));

				if (STRING_ELT(obj, idx) == NA_STRING || value == NULL)
				{
					nulls[arridx] = TRUE;
					have_nulls = TRUE;
				}
				else
				{
					nulls[arridx] = FALSE;
					dvalues[arridx] = FunctionCall3(&in_func,
											CStringGetDatum(value),
											(Datum) 0,
											Int32GetDatum(-1));
				}
			}
		}
	}
	UNPROTECT(1);

	if (!have_nulls)
		array = construct_md_array(dvalues, NULL, ndims, dims, lbs,
									result_elem, typlen, typbyval, typalign);
	else
		array = construct_md_array(dvalues, nulls, ndims, dims, lbs,
									result_elem, typlen, typbyval, typalign);

	dvalue = PointerGetDatum(array);

	return dvalue;
}

static Datum
get_generic_array_datum(SEXP rval, plr_function *function, int col, bool *isnull)
{
	int			objlen = length(rval);
	Datum		dvalue;
	SEXP		obj;
	const char *value;
	Oid			result_elem;
	FmgrInfo	in_func;
	int			typlen;
	bool		typbyval;
	char		typalign;
	int			i;
	Datum	   *dvalues = NULL;
	ArrayType  *array;
#define FIXED_NUM_DIMS		1
	int			ndims = FIXED_NUM_DIMS;
	int			dims[FIXED_NUM_DIMS];
	int			lbs[FIXED_NUM_DIMS];
#undef FIXED_NUM_DIMS
	bool	   *nulls;
	bool		have_nulls = FALSE;
	bool		fast_track_type;
	bool		has_na = false;

	if (function->result_istuple)
	{
		result_elem = function->result_fld_elem_typid[col];
		in_func = function->result_fld_elem_in_func[col];
		typlen = function->result_fld_elem_typlen[col];
		typbyval = function->result_fld_elem_typbyval[col];
		typalign = function->result_fld_elem_typalign[col];
	}
	else
	{
		result_elem = function->result_elem;
		in_func = function->result_elem_in_func;
		typlen = function->result_elem_typlen;
		typbyval = function->result_elem_typbyval;
		typalign = function->result_elem_typalign;
	}

	/*
	 * Special case for pass-by-value data types, if the following conditions are met:
	 * 		designated fast_track_type
	 * 		no NULL/NA elements
	 */
	if (TYPEOF(rval) == INTSXP ||
		TYPEOF(rval) == REALSXP)
	{
		switch (TYPEOF(rval)) {
		case INTSXP:
			if (result_elem == INT4OID)
				fast_track_type = true;
			else
				fast_track_type = false;

			for (i = 0; i < objlen; i++)
			{
				if (INTEGER(rval)[i] == NA_INTEGER)
				{
					has_na = true;
					break;
				}
			}
			break;
		case REALSXP:
			if (result_elem == FLOAT8OID)
				fast_track_type = true;
			else
				fast_track_type = false;

			for (i = 0; i < objlen; i++)
			{
				if (ISNAN(REAL(rval)[i]))
				{
					has_na = true;
					break;
				}
			}
			break;
		default:
			fast_track_type = false;
			has_na = true;	/* does not really matter in this case */
		}
	}
	else
	{
		fast_track_type = false;
		has_na = true;	/* does not really matter in this case */
	}

	if (fast_track_type &&
		 typbyval &&
		 !has_na)
	{
		int32		nbytes = 0;
		int32		dataoffset;

		if (TYPEOF(rval) == INTSXP)
		{
			nbytes = objlen * sizeof(INTEGER_DATA(rval));
			dvalues = (Datum *) INTEGER_DATA(rval);
		}
		else if (TYPEOF(rval) == REALSXP)
		{
			nbytes = objlen * sizeof(NUMERIC_DATA(rval));
			dvalues = (Datum *) NUMERIC_DATA(rval);
		}
		else
			elog(ERROR, "attempted to passthrough invalid R datatype to Postgresql");

		dims[0] = objlen;
		lbs[0] = 1;
		dataoffset = 0;			/* marker for no null bitmap */

		array = (ArrayType *) palloc0(nbytes + ARR_OVERHEAD_NONULLS(ndims));
		SET_VARSIZE(array, nbytes + ARR_OVERHEAD_NONULLS(ndims));
		array->ndim = ndims;
		array->dataoffset = dataoffset;
		array->elemtype = result_elem;
		memcpy(ARR_DIMS(array), dims, ndims * sizeof(int));
		memcpy(ARR_LBOUND(array), lbs, ndims * sizeof(int));
		memcpy(ARR_DATA_PTR(array), dvalues, nbytes);

		dvalue = PointerGetDatum(array);
	}
	else
	{
		/* original code */
		dvalues = (Datum *) palloc(objlen * sizeof(Datum));
		nulls = (bool *) palloc(objlen * sizeof(bool));
		PROTECT(obj =  coerce_to_char(rval));

		/* Loop is needed here as result value might be of length > 1 */
		for(i = 0; i < objlen; i++)
		{
			value = CHAR(STRING_ELT(obj, i));

			if (STRING_ELT(obj, i) == NA_STRING || value == NULL)
			{
				nulls[i] = TRUE;
				have_nulls = TRUE;
			}
			else
			{
				nulls[i] = FALSE;
				dvalues[i] = FunctionCall3(&in_func,
										CStringGetDatum(value),
										(Datum) 0,
										Int32GetDatum(-1));
			}
		}
		UNPROTECT(1);

		dims[0] = objlen;
		lbs[0] = 1;

		if (!have_nulls)
			array = construct_md_array(dvalues, NULL, ndims, dims, lbs,
										result_elem, typlen, typbyval, typalign);
		else
			array = construct_md_array(dvalues, nulls, ndims, dims, lbs,
										result_elem, typlen, typbyval, typalign);

		dvalue = PointerGetDatum(array);
	}

	return dvalue;
}

static Tuplestorestate *
get_frame_tuplestore(SEXP rval,
					 plr_function *function,
					 AttInMetadata *attinmeta,
					 MemoryContext per_query_ctx,
					 bool retset)
{
	Tuplestorestate	   *tupstore;
	char			  **values;
	HeapTuple			tuple;
	TupleDesc			tupdesc = attinmeta->tupdesc;
	int					tupdesc_nc = tupdesc->natts;
	Form_pg_attribute  *attrs = tupdesc->attrs;
	MemoryContext		oldcontext;
	int					i, j;
	int					nr = 0;
	int					nc = length(rval);
	SEXP				dfcol;
	SEXP				result;

	if (nc != tupdesc_nc)
		ereport(ERROR,
		(errcode(ERRCODE_DATA_EXCEPTION),
			errmsg("actual and requested return type mismatch"),
			errdetail("Actual return type has %d columns, but " \
					  "requested return type has %d", nc, tupdesc_nc)));
		
	/* switch to appropriate context to create the tuple store */
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* initialize our tuplestore */
	tupstore = TUPLESTORE_BEGIN_HEAP;

	MemoryContextSwitchTo(oldcontext);

	/*
	 * If we return a set, get number of rows by examining the first column.
	 * Otherwise, stop at one row.
	 */
	if (retset)
	{
		if (isFrame(rval))
		{
			PROTECT(dfcol = VECTOR_ELT(rval, 0));
			nr = length(dfcol);
			UNPROTECT(1);
		}
		else if (isList(rval) || isNewList(rval))
			nr = 1;
	}
	else
		nr = 1;

	/* coerce columns to character in advance */
	PROTECT(result = NEW_LIST(nc));
	for (j = 0; j < nc; j++)
	{
		PROTECT(dfcol = VECTOR_ELT(rval, j));
		if((!isFactor(dfcol)) &&
		   ((attrs[j]->attndims == 0) ||
			(TYPEOF(dfcol) != VECSXP)))
		{
			SEXP	obj;

			PROTECT(obj = coerce_to_char(dfcol));
			SET_VECTOR_ELT(result, j, obj);
			UNPROTECT(1);
		}
		else if(attrs[j]->attndims != 0)	/* array data type */
		{
			SEXP	obj;

			PROTECT(obj = NEW_LIST(nr));
			for(i = 0; i < nr; i++)
			{
				SEXP	dfcolcell;
				SEXP	objcell;

				PROTECT(dfcolcell = VECTOR_ELT(dfcol, i));
				PROTECT(objcell = coerce_to_char(dfcolcell));
				SET_VECTOR_ELT(obj, i, objcell);
				UNPROTECT(2);
			}

			SET_VECTOR_ELT(result, j, obj);
			UNPROTECT(1);
		}
		else
		{
			SEXP 	t;

			for (t = ATTRIB(dfcol); t != R_NilValue; t = CDR(t))
			{
				if(TAG(t) == R_LevelsSymbol)
				{
					PROTECT(SETCAR(t, coerce_to_char(CAR(t))));
					UNPROTECT(1);
					break;
				}
			}
			SET_VECTOR_ELT(result, j, dfcol);
		}

		UNPROTECT(1);
	}

	values = (char **) palloc(nc * sizeof(char *));

	for(i = 0; i < nr; i++)
	{
		for (j = 0; j < nc; j++)
		{
			PROTECT(dfcol = VECTOR_ELT(result, j));

			if(isFactor(dfcol))
			{
				SEXP t;

				/*
				 * a factor is a special type of integer
				 * but must check for NA value first
				 */
				if (INTEGER_ELT(dfcol, i) != NA_INTEGER)
				{
					for (t = ATTRIB(dfcol); t != R_NilValue; t = CDR(t))
					{
						if(TAG(t) == R_LevelsSymbol)
						{
							SEXP	obj;
							int		idx = INTEGER(dfcol)[i] - 1;

							PROTECT(obj = CAR(t));
							values[j] = pstrdup(CHAR(STRING_ELT(obj, idx)));
							UNPROTECT(1);

							break;
						}
					}
				}
				else
					values[j] = NULL;
			}
			else
			{
				if ((attrs[j]->attndims != 0) || (STRING_ELT(dfcol, i) != NA_STRING))
				{
					if (attrs[j]->attndims == 0)
					{
						values[j] = pstrdup(CHAR(STRING_ELT(dfcol, i)));
					}
					else	/* array data type */
					{
						bool	isnull = false;
						Datum	arr_datum;

						if (TYPEOF(dfcol) != VECSXP)
							arr_datum = get_array_datum(dfcol, function, j, &isnull);
						else
							arr_datum = get_array_datum(VECTOR_ELT(dfcol,i), function, j, &isnull);

						if (isnull)
						{
							values[j] = NULL;
						}
						else
						{
							FunctionCallInfoData	fake_fcinfo;
							FmgrInfo				flinfo;
							Datum					dvalue;
							
							MemSet(&fake_fcinfo, 0, sizeof(fake_fcinfo));
							MemSet(&flinfo, 0, sizeof(flinfo));
							fake_fcinfo.flinfo = &flinfo;
							flinfo.fn_mcxt = CurrentMemoryContext;
							fake_fcinfo.context = NULL;
							fake_fcinfo.resultinfo = NULL;
							fake_fcinfo.isnull = false;
							fake_fcinfo.nargs = 1;
							fake_fcinfo.arg[0] = arr_datum;
							fake_fcinfo.argnull[0] = false;
							dvalue = (*array_out)(&fake_fcinfo);
							if (fake_fcinfo.isnull)
								values[j] = NULL;
							else
								values[j] = DatumGetCString(dvalue);
						}
					}
				}
				else
					values[j] = NULL;
			}

			UNPROTECT(1);
		}

		/* construct the tuple */
		tuple = BuildTupleFromCStrings(attinmeta, values);

		/* switch to appropriate context while storing the tuple */
		oldcontext = MemoryContextSwitchTo(per_query_ctx);

		/* now store it */
		tuplestore_puttuple(tupstore, tuple);

		/* now reset the context */
		MemoryContextSwitchTo(oldcontext);

		for (j = 0; j < nc; j++)
			if (values[j] != NULL)
				pfree(values[j]);
	}
	UNPROTECT(1);

	oldcontext = MemoryContextSwitchTo(per_query_ctx);
	tuplestore_donestoring(tupstore);
	MemoryContextSwitchTo(oldcontext);

	return tupstore;
}

static Tuplestorestate *
get_matrix_tuplestore(SEXP rval,
					 plr_function *function,
					 AttInMetadata *attinmeta,
					 MemoryContext per_query_ctx,
					 bool retset)
{
	Tuplestorestate	   *tupstore;
	char			  **values;
	HeapTuple			tuple;
	MemoryContext		oldcontext;
	SEXP				obj;
	int					i, j;
	int					nr;
	int					nc = ncols(rval);

	/* switch to appropriate context to create the tuple store */
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/*
	 * If we return a set, get number of rows.
	 * Otherwise, stop at one row.
	 */
	if (retset)
		nr = nrows(rval);
	else
		nr = 1;

	/* initialize our tuplestore */
	tupstore = TUPLESTORE_BEGIN_HEAP;

	MemoryContextSwitchTo(oldcontext);

	values = (char **) palloc(nc * sizeof(char *));

	PROTECT(obj =  coerce_to_char(rval));
	for(i = 0; i < nr; i++)
	{
		for (j = 0; j < nc; j++)
		{
			if (STRING_ELT(obj, (j * nr) + i) != NA_STRING)
				values[j] = (char *) CHAR(STRING_ELT(obj, (j * nr) + i));
			else
				values[j] = (char *) NULL;
		}

		/* construct the tuple */
		tuple = BuildTupleFromCStrings(attinmeta, values);

		/* switch to appropriate context while storing the tuple */
		oldcontext = MemoryContextSwitchTo(per_query_ctx);

		/* now store it */
		tuplestore_puttuple(tupstore, tuple);

		/* now reset the context */
		MemoryContextSwitchTo(oldcontext);
	}
	UNPROTECT(1);

	oldcontext = MemoryContextSwitchTo(per_query_ctx);
	tuplestore_donestoring(tupstore);
	MemoryContextSwitchTo(oldcontext);

	return tupstore;
}

static Tuplestorestate *
get_generic_tuplestore(SEXP rval,
					 plr_function *function,
					 AttInMetadata *attinmeta,
					 MemoryContext per_query_ctx,
					 bool retset)
{
	Tuplestorestate	   *tupstore;
	char			  **values;
	HeapTuple			tuple;
	MemoryContext		oldcontext;
	int					nr;
	int					nc = 1;
	SEXP				obj;
	int					i;

	/* switch to appropriate context to create the tuple store */
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/*
	 * If we return a set, get number of rows.
	 * Otherwise, stop at one row.
	 */
	if (retset)
		nr = length(rval);
	else
		nr = 1;

	/* initialize our tuplestore */
	tupstore = TUPLESTORE_BEGIN_HEAP;

	MemoryContextSwitchTo(oldcontext);

	values = (char **) palloc(nc * sizeof(char *));
	PROTECT(obj = coerce_to_char(rval));

	for(i = 0; i < nr; i++)
	{
		if (STRING_ELT(obj, i) != NA_STRING)
			values[0] = (char *) CHAR(STRING_ELT(obj, i));
		else
			values[0] = (char *) NULL;

		/* construct the tuple */
		tuple = BuildTupleFromCStrings(attinmeta, values);

		/* switch to appropriate context while storing the tuple */
		oldcontext = MemoryContextSwitchTo(per_query_ctx);

		/* now store it */
		tuplestore_puttuple(tupstore, tuple);

		/* now reset the context */
		MemoryContextSwitchTo(oldcontext);
	}
	UNPROTECT(1);

	oldcontext = MemoryContextSwitchTo(per_query_ctx);
	tuplestore_donestoring(tupstore);
	MemoryContextSwitchTo(oldcontext);

	return tupstore;
}

static SEXP
coerce_to_char(SEXP rval)
{
	SEXP	obj = NULL;

	switch (TYPEOF(rval))
	{
		case LISTSXP:
		case NILSXP:
		case SYMSXP:
		case VECSXP:
		case EXPRSXP:
		case LGLSXP:
		case INTSXP:
		case REALSXP:
		case CPLXSXP:
		case STRSXP:
		case RAWSXP:
			PROTECT(obj = AS_CHARACTER(rval));
			break;
		default:
			ereport(ERROR,
			(errcode(ERRCODE_DATA_EXCEPTION),
				errmsg("data type coercion error"),
				errdetail("R object is not an expected " \
						  "data type; examine your R code")));
	}
	UNPROTECT(1);

	return obj;
}
