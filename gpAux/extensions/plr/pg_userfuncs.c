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
 * pg_userfuncs.c - User visible PostgreSQL functions
 */
#include "plr.h"

extern MemoryContext plr_SPI_context;

#ifndef WIN32
extern char **environ;
#endif

static ArrayType *plr_array_create(FunctionCallInfo fcinfo,
								   int numelems, int elem_start);

/*-----------------------------------------------------------------------------
 * plr_version :
 *		output PL/R version string
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_version);
Datum
plr_version(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(PG_STR_GET_TEXT(PLR_VERSION));
}

/*-----------------------------------------------------------------------------
 * reload_modules :
 *		interface to allow plr_modules to be reloaded on demand
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(reload_plr_modules);
Datum
reload_plr_modules(PG_FUNCTION_ARGS)
{
	MemoryContext	plr_caller_context = CurrentMemoryContext;

	if (SPI_connect() != SPI_OK_CONNECT)
		elog(ERROR, "SPI_connect failed");
	plr_SPI_context = CurrentMemoryContext;
	MemoryContextSwitchTo(plr_caller_context);

	plr_load_modules();

	if (SPI_finish() != SPI_OK_FINISH)
		elog(ERROR, "SPI_finish failed");

	PG_RETURN_TEXT_P(PG_STR_GET_TEXT("OK"));
}

/*-----------------------------------------------------------------------------
 * install_rcmd :
 *		interface to allow user defined R functions to be called from other
 *		R functions
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(install_rcmd);
Datum
install_rcmd(PG_FUNCTION_ARGS)
{
	char *cmd = PG_TEXT_GET_STR(PG_GETARG_TEXT_P(0));

	load_r_cmd(cmd);

	PG_RETURN_TEXT_P(PG_STR_GET_TEXT("OK"));
}


/*-----------------------------------------------------------------------------
 * array :
 *		form a one-dimensional array given starting elements
 *		FIXME: does not handle NULL array elements
 *		       this function should be obsoleted by similar
 *		       backend functionality
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_array);
Datum
plr_array(PG_FUNCTION_ARGS)
{
	ArrayType  *result;

	result = plr_array_create(fcinfo, PG_NARGS(), 0);

	PG_RETURN_ARRAYTYPE_P(result);
}

/*-----------------------------------------------------------------------------
 * array_push :
 *		push an element onto the end of a one-dimensional array
 *		FIXME: does not handle NULL array elements
 *		       this function should be obsoleted by similar
 *		       backend functionality
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_array_push);
Datum
plr_array_push(PG_FUNCTION_ARGS)
{
	ArrayType  *v;
	Datum		newelem;
	int		   *dimv,
			   *lb, ub;
	ArrayType  *result;
	int			indx;
	Oid			element_type;
	int16		typlen;
	bool		typbyval;
	char		typalign;

	v = PG_GETARG_ARRAYTYPE_P(0);
	newelem = PG_GETARG_DATUM(1);

	/* Sanity check: do we have a one-dimensional array */
	if (ARR_NDIM(v) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("input must be one-dimensional array")));

	lb = ARR_LBOUND(v);
	dimv = ARR_DIMS(v);
	ub = dimv[0] + lb[0] - 1;
	indx = ub + 1;

	element_type = ARR_ELEMTYPE(v);
	/* Sanity check: do we have a non-zero element type */
	if (element_type == 0)
		/* internal error */
		elog(ERROR, "invalid array element type");

	get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	result = array_set(v, 1, &indx, newelem, FALSE, -1,
						typlen, typbyval, typalign);

	PG_RETURN_ARRAYTYPE_P(result);
}

/*-----------------------------------------------------------------------------
 * array_accum :
 *		accumulator to build an array from input values -- when used in
 *		conjunction with plr functions that accept an array, and output
 *		a statistic, this can be used to create custom aggregates.
 *		FIXME: does not handle NULL array elements
 *		       this function should be obsoleted by similar
 *		       backend functionality
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_array_accum);
Datum
plr_array_accum(PG_FUNCTION_ARGS)
{
	Datum		v;
	Datum		newelem;
	ArrayType  *result;

	/* return NULL if both arguments are NULL */
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();

	/* create a new array from the second argument if first is NULL */
	if (PG_ARGISNULL(0))
		PG_RETURN_ARRAYTYPE_P(plr_array_create(fcinfo, 1, 1));

	/* return the first argument if the second is NULL */
	if (PG_ARGISNULL(1))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P_COPY(0));

	v = PG_GETARG_DATUM(0);
	newelem = PG_GETARG_DATUM(1);

	result = DatumGetArrayTypeP(DirectFunctionCall2(plr_array_push, v, newelem));

	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * actually does the work for array(), and array_accum() if it is given a null
 * input array.
 *
 * numelems and elem_start allow the function to be shared given the differing
 * arguments accepted by array() and array_accum(). With array(), all function
 * arguments are used for array construction -- therefore elem_start is 0 and
 * numelems is the number of function arguments. With array_accum(), we are
 * always initializing the array with a single element given to us as argument
 * number 1 (i.e. the second argument).
 *
 */
static ArrayType *
plr_array_create(FunctionCallInfo fcinfo, int numelems, int elem_start)
{
	Oid			funcid = fcinfo->flinfo->fn_oid;
	Datum	   *dvalues = (Datum *) palloc(numelems * sizeof(Datum));
	int16		typlen;
	bool		typbyval;
	Oid			typinput;
	Oid			element_type;
	char		typalign;
	int			i;
	HeapTuple	tp;
	Oid			functypeid;
	Oid		   *funcargtypes;
	ArrayType  *result;

	if (numelems == 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("at least one value required to construct an array")));

	/*
	 * Get the type metadata for the array return type and its elements
	 */
	tp = SearchSysCache(PROCOID,
						ObjectIdGetDatum(funcid),
						0, 0, 0);
	if (!HeapTupleIsValid(tp))
		/* internal error */
		elog(ERROR, "function OID %u does not exist", funcid);

	functypeid = ((Form_pg_proc) GETSTRUCT(tp))->prorettype;
	getTypeInputInfo(functypeid, &typinput, &element_type);

	get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	funcargtypes = FUNCARGTYPES(tp);

	/*
	 * the first function argument(s) may not be one of our array elements,
	 * but the caller is responsible to ensure we get nothing but array
	 * elements once they start coming
	 */
	for (i = elem_start; i < elem_start + numelems; i++)
		if (funcargtypes[i] != element_type)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("argument %d datatype not " \
							"compatible with return data type", i + 1)));

	ReleaseSysCache(tp);

	for (i = 0; i < numelems; i++)
		dvalues[i] = PG_GETARG_DATUM(elem_start + i);

	result = construct_array(dvalues, numelems, element_type,
							 typlen, typbyval, typalign);

	return result;
}

/*-----------------------------------------------------------------------------
 * plr_environ :
 *		utility function to display the environment under which the
 *		postmaster is running.
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_environ);
Datum
plr_environ(PG_FUNCTION_ARGS)
{
	ReturnSetInfo	   *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	Tuplestorestate	   *tupstore;
	HeapTuple			tuple;
	TupleDesc			tupdesc;
	AttInMetadata	   *attinmeta;
	MemoryContext		per_query_ctx;
	MemoryContext		oldcontext;
	char			   *var_name;
	char			   *var_val;
	char			   *values[2];
#ifndef WIN32
	char			  **current_env;
#else
	char			   *buf;
	LPTSTR				envstr;
    int					count = 0;
	int					i;
#endif

	/* check to see if caller supports us returning a tuplestore */
	if (!rsinfo || !(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("materialize mode required, but it is not "
						"allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* get the requested return tuple description */
	tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);

	/*
	 * Check to make sure we have a reasonable tuple descriptor
	 */
	if (tupdesc->natts != 2 ||
		tupdesc->attrs[0]->atttypid != TEXTOID ||
		tupdesc->attrs[1]->atttypid != TEXTOID)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("query-specified return tuple and "
						"function return type are not compatible")));

	/* OK to use it */
	attinmeta = TupleDescGetAttInMetadata(tupdesc);

	/* let the caller know we're sending back a tuplestore */
	rsinfo->returnMode = SFRM_Materialize;

	/* initialize our tuplestore */
	tupstore = TUPLESTORE_BEGIN_HEAP;

#ifndef WIN32
	for (current_env = environ;
		 current_env != NULL && *current_env != NULL;
		 current_env++)
	{
		Size	name_len;

		var_val = strchr(*current_env, '=');
		if (!var_val)
			continue;

		name_len = var_val - *current_env;
		var_name = (char *) palloc0(name_len + 1);
		memcpy(var_name, *current_env, name_len);

		values[0] = var_name;
		values[1] = var_val + 1;

		tuple = BuildTupleFromCStrings(attinmeta, values);
		tuplestore_puttuple(tupstore, tuple);
		pfree(var_name);
	}
#else	
	buf = GetEnvironmentStrings();
	envstr = buf;

	while (true)
	{
		if (*envstr == 0)
			break;
		while (*envstr != 0)
			envstr++;
		envstr++;
		count++;
	}
	/* reset pointer to the environment buffer */
	envstr = buf;

	while(*buf == '=')
		buf++;
	for (i = 0; i < count; i++)
	{
		Size	name_len;

		var_val = strchr(buf, '=');
		if (!var_val)
			continue;

		name_len = var_val - buf;
		var_name = (char *) palloc0(name_len + 1);
		memcpy(var_name, buf, name_len);

		values[0] = var_name;
		values[1] = var_val + 1;

		tuple = BuildTupleFromCStrings(attinmeta, values);
		tuplestore_puttuple(tupstore, tuple);
		pfree(var_name);
		
		while(*buf != '\0')
			buf++;
		buf++;
	}

	FreeEnvironmentStrings(envstr);
#endif
	
	/*
	 * no longer need the tuple descriptor reference created by
	 * TupleDescGetAttInMetadata()
	 */
	ReleaseTupleDesc(tupdesc);

	tuplestore_donestoring(tupstore);
	rsinfo->setResult = tupstore;

	/*
	 * SFRM_Materialize mode expects us to return a NULL Datum. The actual
	 * tuples are in our tuplestore and passed back through
	 * rsinfo->setResult. rsinfo->setDesc is set to the tuple description
	 * that we actually used to build our tuples with, so the caller can
	 * verify we did what it was expecting.
	 */
	rsinfo->setDesc = tupdesc;
	MemoryContextSwitchTo(oldcontext);

	return (Datum) 0;
}

/*-----------------------------------------------------------------------------
 * plr_set_rhome :
 *		utility function to set the R_HOME environment variable under
 *		which the postmaster is running.
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_set_rhome);
Datum
plr_set_rhome(PG_FUNCTION_ARGS)
{
	char		   *rhome = PG_TEXT_GET_STR(PG_GETARG_TEXT_P(0));
	size_t			rh_len = strlen(rhome);

	if (rh_len)
	{
		char			   *rhenv;
		MemoryContext		oldcontext;

		/* Needs to live until/unless we explicitly delete it */
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		rhenv = palloc(8 + rh_len);
		MemoryContextSwitchTo(oldcontext);

		sprintf(rhenv, "R_HOME=%s", rhome);
		putenv(rhenv);
	}

	PG_RETURN_TEXT_P(PG_STR_GET_TEXT("OK"));
}

/*-----------------------------------------------------------------------------
 * plr_unset_rhome :
 *		utility function to unset the R_HOME environment variable under
 *		which the postmaster is running.
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_unset_rhome);
Datum
plr_unset_rhome(PG_FUNCTION_ARGS)
{
	unsetenv("R_HOME");

	PG_RETURN_TEXT_P(PG_STR_GET_TEXT("OK"));
}

/*-----------------------------------------------------------------------------
 * plr_set_display :
 *		utility function to set the DISPLAY environment variable under
 *		which the postmaster is running.
 *----------------------------------------------------------------------------
 */
PG_FUNCTION_INFO_V1(plr_set_display);
Datum
plr_set_display(PG_FUNCTION_ARGS)
{
	char		   *display = PG_TEXT_GET_STR(PG_GETARG_TEXT_P(0));
	size_t			d_len = strlen(display);

	if (d_len)
	{
		char			   *denv;
		MemoryContext		oldcontext;

		/* Needs to live until/unless we explicitly delete it */
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		denv = palloc(9 + d_len);
		MemoryContextSwitchTo(oldcontext);

		sprintf(denv, "DISPLAY=%s", display);
		putenv(denv);
	}

	PG_RETURN_TEXT_P(PG_STR_GET_TEXT("OK"));
}

/*-----------------------------------------------------------------------------
 * plr_get_raw :
 *		utility function to ...
 *----------------------------------------------------------------------------
 */
extern char *last_R_error_msg;
 
PG_FUNCTION_INFO_V1(plr_get_raw);
Datum
plr_get_raw(PG_FUNCTION_ARGS)
{
	SEXP	result;
	SEXP 	s, t, obj;
	int		status;
	bytea  *bvalue = PG_GETARG_BYTEA_P(0);
	int		len, rsize;
	bytea  *bresult;
	char   *brptr;

	PROTECT(obj = NEW_RAW(VARSIZE(bvalue)));
	memcpy((char *) RAW(obj), VARDATA(bvalue), VARSIZE(bvalue));

	/*
	 * Need to construct a call to
	 * unserialize(rval)
	 */
	PROTECT(t = s = allocList(2));
	SET_TYPEOF(s, LANGSXP);
	SETCAR(t, install("unserialize")); t = CDR(t);
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

	len = LENGTH(result);
	rsize = VARHDRSZ + len;
	bresult = (bytea *) palloc(rsize);
	SET_VARSIZE(bresult, rsize);
	brptr = VARDATA(bresult);
	memcpy(brptr, (char *) RAW(result), rsize - VARHDRSZ);

	UNPROTECT(3);

	PG_RETURN_BYTEA_P(bresult);
}
