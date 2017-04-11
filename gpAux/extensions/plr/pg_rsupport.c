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
 * pg_rsupport.c - Postgres support for use within plr functions
 */
#include "plr.h"

extern MemoryContext plr_SPI_context;
extern char *last_R_error_msg;

static SEXP rpgsql_get_results(int ntuples, SPITupleTable *tuptable);
static void rsupport_error_callback(void *arg);

/* The information we cache prepared plans */
typedef struct saved_plan_desc
{
	void	   *saved_plan;
	int			nargs;
	Oid		   *typeids;
	Oid		   *typelems;
	FmgrInfo   *typinfuncs;
}	saved_plan_desc;

/*
 * Functions used in R
 *****************************************************************************/
void
throw_pg_notice(const char **msg)
{
	/* skip error CONTEXT for explicitly called messages */
	SAVE_PLERRCONTEXT;

	if (msg && *msg)
		elog(NOTICE, "%s", *msg);
	else
		elog(NOTICE, "%s", "");

	RESTORE_PLERRCONTEXT;
}

/*
 * plr_quote_literal() - quote literal strings that are to
 *			  be used in SPI_exec query strings
 */
SEXP
plr_quote_literal(SEXP rval)
{
	const char *value;
	text	   *value_text;
	text	   *result_text;
	SEXP		result;

	/* extract the C string */
	PROTECT(rval =  AS_CHARACTER(rval));
	value = CHAR(STRING_ELT(rval, 0));

	/* convert using the pgsql quote_literal function */
	value_text = PG_STR_GET_TEXT(value);
	result_text = DatumGetTextP(DirectFunctionCall1(quote_literal, PointerGetDatum(value_text)));

	/* copy result back into an R object */
	PROTECT(result = NEW_CHARACTER(1));
	SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(PG_TEXT_GET_STR(result_text)));
	UNPROTECT(2);

	return result;
}

/*
 * plr_quote_literal() - quote identifiers that are to
 *			  be used in SPI_exec query strings
 */
SEXP
plr_quote_ident(SEXP rval)
{
	const char *value;
	text	   *value_text;
	text	   *result_text;
	SEXP		result;

	/* extract the C string */
	PROTECT(rval =  AS_CHARACTER(rval));
	value = CHAR(STRING_ELT(rval, 0));

	/* convert using the pgsql quote_literal function */
	value_text = PG_STR_GET_TEXT(value);
	result_text = DatumGetTextP(DirectFunctionCall1(quote_ident, PointerGetDatum(value_text)));

	/* copy result back into an R object */
	PROTECT(result = NEW_CHARACTER(1));
	SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(PG_TEXT_GET_STR(result_text)));
	UNPROTECT(2);

	return result;
}

/*
 * plr_SPI_exec - The builtin SPI_exec command for the R interpreter
 */
SEXP
plr_SPI_exec(SEXP rsql)
{
	int				spi_rc = 0;
	char			buf[64];
	const char	   *sql;
	int				count = 0;
	int				ntuples;
	SEXP			result = NULL;
	MemoryContext	oldcontext;
	PREPARE_PG_TRY;

	/* set up error context */
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.exec");

	PROTECT(rsql =  AS_CHARACTER(rsql));
	sql = CHAR(STRING_ELT(rsql, 0));
	UNPROTECT(1);
	if (sql == NULL)
		error("%s", "cannot exec empty query");

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);

	/*
	 * trap elog/ereport so we can let R finish up gracefully
	 * and generate the error once we exit the interpreter
	 */
	PG_TRY();
	{
		/* Execute the query and handle return codes */
		spi_rc = SPI_exec(sql, count);
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();
	
	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);

	switch (spi_rc)
	{
		case SPI_OK_UTILITY:
			snprintf(buf, sizeof(buf), "%d", 0);
			SPI_freetuptable(SPI_tuptable);

			PROTECT(result = NEW_CHARACTER(1));
			SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(buf));
			UNPROTECT(1);
			break;
			
		case SPI_OK_SELINTO:
		case SPI_OK_INSERT:
		case SPI_OK_DELETE:
		case SPI_OK_UPDATE:
			snprintf(buf, sizeof(buf), "%d", SPI_processed);
			SPI_freetuptable(SPI_tuptable);

			PROTECT(result = NEW_CHARACTER(1));
			SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(buf));
			UNPROTECT(1);
			break;
			
		case SPI_OK_SELECT:
			ntuples = SPI_processed;
			if (ntuples > 0)
			{
				result = rpgsql_get_results(ntuples, SPI_tuptable);
				SPI_freetuptable(SPI_tuptable);
			}
			else
				result = R_NilValue;
			break;
			
		case SPI_ERROR_ARGUMENT:
			error("SPI_exec() failed: SPI_ERROR_ARGUMENT");
			break;
			
		case SPI_ERROR_UNCONNECTED:
			error("SPI_exec() failed: SPI_ERROR_UNCONNECTED");
			break;
			
		case SPI_ERROR_COPY:
			error("SPI_exec() failed: SPI_ERROR_COPY");
			break;
			
		case SPI_ERROR_CURSOR:
			error("SPI_exec() failed: SPI_ERROR_CURSOR");
			break;
			
		case SPI_ERROR_TRANSACTION:
			error("SPI_exec() failed: SPI_ERROR_TRANSACTION");
			break;
			
		case SPI_ERROR_OPUNKNOWN:
			error("SPI_exec() failed: SPI_ERROR_OPUNKNOWN");
			break;
			
		default:
			error("SPI_exec() failed: %d", spi_rc);
			break;
	}
			
	POP_PLERRCONTEXT;
	return result;
}

static SEXP
rpgsql_get_results(int ntuples, SPITupleTable *tuptable)
{
	SEXP	result;
	ERRORCONTEXTCALLBACK;

	/* set up error context */
	PUSH_PLERRCONTEXT(rsupport_error_callback, "rpgsql_get_results");

	if (tuptable != NULL)
	{
		HeapTuple	   *tuples = tuptable->vals;
		TupleDesc		tupdesc = tuptable->tupdesc;

		result = pg_tuple_get_r_frame(ntuples, tuples, tupdesc);
	}
	else
		result = R_NilValue;

	POP_PLERRCONTEXT;

	return result;
}

/*
 * plr_SPI_prepare - The builtin SPI_prepare command for the R interpreter
 */
SEXP
plr_SPI_prepare(SEXP rsql, SEXP rargtypes)
{
	const char		   *sql;
	int					nargs;
	int					i;
	Oid				   *typeids = NULL;
	Oid				   *typelems = NULL;
	FmgrInfo		   *typinfuncs = NULL;
	void			   *pplan = NULL;
	void			   *saved_plan;
	saved_plan_desc	   *plan_desc;
	SEXP				result;
	MemoryContext		oldcontext;
	PREPARE_PG_TRY;

	/* set up error context */
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.prepare");

	/* switch to long lived context to create plan description */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	plan_desc = (saved_plan_desc *) palloc(sizeof(saved_plan_desc));

	MemoryContextSwitchTo(oldcontext);

	PROTECT(rsql =  AS_CHARACTER(rsql));
	sql = CHAR(STRING_ELT(rsql, 0));
	UNPROTECT(1);
	if (sql == NULL)
		error("%s", "cannot prepare empty query");

	PROTECT(rargtypes = AS_INTEGER(rargtypes));
	if (!isVector(rargtypes) || !isInteger(rargtypes))
		error("%s", "second parameter must be a vector of PostgreSQL datatypes");

	/* deal with case of no parameters for the prepared query */
	if (rargtypes == R_MissingArg || INTEGER(rargtypes)[0] == NA_INTEGER)
		nargs = 0;
	else
		nargs = length(rargtypes);

	if (nargs < 0)	/* can this even happen?? */
		error("%s", "second parameter must be a vector of PostgreSQL datatypes");

	if (nargs > 0)
	{
		/* switch to long lived context to create plan description elements */
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);

		typeids = (Oid *) palloc(nargs * sizeof(Oid));
		typelems = (Oid *) palloc(nargs * sizeof(Oid));
		typinfuncs = (FmgrInfo *) palloc(nargs * sizeof(FmgrInfo));

		MemoryContextSwitchTo(oldcontext);

		for (i = 0; i < nargs; i++)
		{
			int16		typlen;
			bool		typbyval;
			char		typdelim;
			Oid			typinput,
						typelem;
			char		typalign;
			FmgrInfo	typinfunc;

			typeids[i] = INTEGER(rargtypes)[i];

			/* switch to long lived context to create plan description elements */
			oldcontext = MemoryContextSwitchTo(TopMemoryContext);

			get_type_io_data(typeids[i], IOFunc_input, &typlen, &typbyval,
							 &typalign, &typdelim, &typelem, &typinput);
			typelems[i] = get_element_type(typeids[i]);

			MemoryContextSwitchTo(oldcontext);

			/* perm_fmgr_info already uses TopMemoryContext */
			perm_fmgr_info(typinput, &typinfunc);
			typinfuncs[i] = typinfunc;
		}
	}
	else
		typeids = NULL;

	UNPROTECT(1);

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);

	/*
	 * trap elog/ereport so we can let R finish up gracefully
	 * and generate the error once we exit the interpreter
	 */
	PG_TRY();
	{
		/* Prepare plan for query */
		pplan = SPI_prepare(sql, nargs, typeids);
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();

	if (pplan == NULL)
	{
		char		buf[128];
		char	   *reason;

		switch (SPI_result)
		{
			case SPI_ERROR_ARGUMENT:
				reason = "SPI_ERROR_ARGUMENT";
				break;

			case SPI_ERROR_UNCONNECTED:
				reason = "SPI_ERROR_UNCONNECTED";
				break;

			case SPI_ERROR_COPY:
				reason = "SPI_ERROR_COPY";
				break;

			case SPI_ERROR_CURSOR:
				reason = "SPI_ERROR_CURSOR";
				break;

			case SPI_ERROR_TRANSACTION:
				reason = "SPI_ERROR_TRANSACTION";
				break;

			case SPI_ERROR_OPUNKNOWN:
				reason = "SPI_ERROR_OPUNKNOWN";
				break;

			default:
				snprintf(buf, sizeof(buf), "unknown RC %d", SPI_result);
				reason = buf;
				break;
		}

		/* internal error */
		error("SPI_prepare() failed: %s", reason);
	}

	/* SPI_saveplan already uses TopMemoryContext */
	saved_plan = SPI_saveplan(pplan);
	if (saved_plan == NULL)
	{
		char		buf[128];
		char	   *reason;

		switch (SPI_result)
		{
			case SPI_ERROR_ARGUMENT:
				reason = "SPI_ERROR_ARGUMENT";
				break;

			case SPI_ERROR_UNCONNECTED:
				reason = "SPI_ERROR_UNCONNECTED";
				break;

			default:
				snprintf(buf, sizeof(buf), "unknown RC %d", SPI_result);
				reason = buf;
				break;
		}

		/* internal error */
		error("SPI_saveplan() failed: %s", reason);
	}

	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);

	/* no longer need this */
	SPI_freeplan(pplan);

	plan_desc->saved_plan = saved_plan;
	plan_desc->nargs = nargs;
	plan_desc->typeids = typeids;
	plan_desc->typelems = typelems;
	plan_desc->typinfuncs = typinfuncs;

	result = R_MakeExternalPtr(plan_desc, R_NilValue, R_NilValue);

	POP_PLERRCONTEXT;
	return result;
}

/*
 * plr_SPI_execp - The builtin SPI_execp command for the R interpreter
 */
SEXP
plr_SPI_execp(SEXP rsaved_plan, SEXP rargvalues)
{
	saved_plan_desc	   *plan_desc = (saved_plan_desc *) R_ExternalPtrAddr(rsaved_plan);
	void			   *saved_plan = plan_desc->saved_plan;
	int					nargs = plan_desc->nargs;
	Oid				   *typeids = plan_desc->typeids;
	Oid				   *typelems = plan_desc->typelems;
	FmgrInfo		   *typinfuncs = plan_desc->typinfuncs;
	int					i;
	Datum			   *argvalues = NULL;
	char			   *nulls = NULL;
	bool				isnull = false;
	SEXP				obj;
	int					spi_rc = 0;
	char				buf[64];
	int					count = 0;
	int					ntuples;
	SEXP				result = NULL;
	MemoryContext		oldcontext;
	PREPARE_PG_TRY;

	/* set up error context */
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.execp");

	if (nargs > 0)
	{
		if (!Rf_isVectorList(rargvalues))
			error("%s", "second parameter must be a list of arguments " \
						"to the prepared plan");

		if (length(rargvalues) != nargs)
			error("list of arguments (%d) is not the same length " \
				  "as that of the prepared plan (%d)",
				  length(rargvalues), nargs);

		argvalues = (Datum *) palloc(nargs * sizeof(Datum));
		nulls = (char *) palloc(nargs * sizeof(char));
	}

	for (i = 0; i < nargs; i++)
	{
		PROTECT(obj = VECTOR_ELT(rargvalues, i));

		argvalues[i] = get_datum(obj, typeids[i], typelems[i], typinfuncs[i], &isnull);
		if (!isnull)
			nulls[i] = ' ';
		else
			nulls[i] = 'n';

		UNPROTECT(1);
	}

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);

	/*
	 * trap elog/ereport so we can let R finish up gracefully
	 * and generate the error once we exit the interpreter
	 */
	PG_TRY();
	{
		/* Execute the plan */
		spi_rc = SPI_execp(saved_plan, argvalues, nulls, count);
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();

	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);

	/* check the result */
	switch (spi_rc)
	{
		case SPI_OK_UTILITY:
			snprintf(buf, sizeof(buf), "%d", 0);
			SPI_freetuptable(SPI_tuptable);

			PROTECT(result = NEW_CHARACTER(1));
			SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(buf));
			UNPROTECT(1);

			break;
			
		case SPI_OK_SELINTO:
		case SPI_OK_INSERT:
		case SPI_OK_DELETE:
		case SPI_OK_UPDATE:
			snprintf(buf, sizeof(buf), "%d", SPI_processed);
			SPI_freetuptable(SPI_tuptable);

			PROTECT(result = NEW_CHARACTER(1));
			SET_STRING_ELT(result, 0, COPY_TO_USER_STRING(buf));
			UNPROTECT(1);

			break;
			
		case SPI_OK_SELECT:
			ntuples = SPI_processed;
			if (ntuples > 0)
			{
				result = rpgsql_get_results(ntuples, SPI_tuptable);
				SPI_freetuptable(SPI_tuptable);
			}
			else
				result = R_NilValue;
			break;
			
		case SPI_ERROR_ARGUMENT:
			error("SPI_execp() failed: SPI_ERROR_ARGUMENT");
			break;
			
		case SPI_ERROR_UNCONNECTED:
			error("SPI_execp() failed: SPI_ERROR_UNCONNECTED");
			break;
			
		case SPI_ERROR_COPY:
			error("SPI_execp() failed: SPI_ERROR_COPY");
			break;
			
		case SPI_ERROR_CURSOR:
			error("SPI_execp() failed: SPI_ERROR_CURSOR");
			break;
			
		case SPI_ERROR_TRANSACTION:
			error("SPI_execp() failed: SPI_ERROR_TRANSACTION");
			break;
			
		case SPI_ERROR_OPUNKNOWN:
			error("SPI_execp() failed: SPI_ERROR_OPUNKNOWN");
			break;
			
		default:
			error("SPI_execp() failed: %d", spi_rc);
			break;
	}

	POP_PLERRCONTEXT;
	return result;
}

/*
 * plr_SPI_lastoid - return the last oid. To be used after insert queries.
 */
SEXP
plr_SPI_lastoid(void)
{
	SEXP	result;

	PROTECT(result = NEW_INTEGER(1));
	INTEGER_DATA(result)[0] = SPI_lastoid;
	UNPROTECT(1);

	return result;
}

/*
 * Takes the prepared plan rsaved_plan and creates a cursor 
 * for it using the values specified in ragvalues.
 *
 */
SEXP
plr_SPI_cursor_open(SEXP cursor_name_arg,SEXP rsaved_plan, SEXP rargvalues)
{
	saved_plan_desc	   *plan_desc = (saved_plan_desc *) R_ExternalPtrAddr(rsaved_plan);
	void			   *saved_plan = plan_desc->saved_plan;
	int					nargs = plan_desc->nargs;
	Oid				   *typeids = plan_desc->typeids;
	FmgrInfo		   *typinfuncs = plan_desc->typinfuncs;
	int					i;
	Datum			   *argvalues = NULL;
	char			   *nulls = NULL;
	bool				isnull = false;
	SEXP				obj;
	SEXP				result = NULL;
	MemoryContext		oldcontext;
	char				cursor_name[64];
	Portal				portal=NULL;
	PREPARE_PG_TRY;
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.cursor_open");

	/* Divide rargvalues */
	if (nargs > 0)
	{
		if (!Rf_isVectorList(rargvalues))
			error("%s", "second parameter must be a list of arguments " \
						"to the prepared plan");

		if (length(rargvalues) != nargs)
			error("list of arguments (%d) is not the same length " \
				  "as that of the prepared plan (%d)",
				  length(rargvalues), nargs);

		argvalues = (Datum *) palloc(nargs * sizeof(Datum));
		nulls = (char *) palloc(nargs * sizeof(char));
	}

	for (i = 0; i < nargs; i++)
	{
		PROTECT(obj = VECTOR_ELT(rargvalues, i));

		argvalues[i] = get_scalar_datum(obj, typeids[i], typinfuncs[i], &isnull);
		if (!isnull)
			nulls[i] = ' ';
		else
			nulls[i] = 'n';

		UNPROTECT(1);
	}
	strncpy(cursor_name, CHAR(STRING_ELT(cursor_name_arg,0)), 64);

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);

	/*
	 * trap elog/ereport so we can let R finish up gracefully
	 * and generate the error once we exit the interpreter
	 */
	PG_TRY();
	{
		/* Open the cursor */
		portal = SPI_cursor_open(cursor_name,saved_plan, argvalues, nulls,1);

	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();

	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);

	if(portal==NULL) 
		error("SPI_cursor_open() failed");
	else 
		result = R_MakeExternalPtr(portal, R_NilValue, R_NilValue);

	POP_PLERRCONTEXT;
	return result;
}

SEXP
plr_SPI_cursor_fetch(SEXP cursor_in,SEXP forward_in, SEXP rows_in)
{
	Portal				portal=NULL;
	int					ntuples;
	SEXP				result = NULL;
	MemoryContext		oldcontext;
	int					forward;
	int					rows;
	PREPARE_PG_TRY;
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.cursor_fetch");

	portal = R_ExternalPtrAddr(cursor_in);
	if(!IS_LOGICAL(forward_in))
	{
		error("pg.spi.cursor_fetch arg2 must be boolean");
		return result;
	}
	if(!IS_INTEGER(rows_in))
	{
		error("pg.spi.cursor_fetch arg3 must be an integer");
		return result;
	}
	forward = LOGICAL_DATA(forward_in)[0];
	rows  = INTEGER_DATA(rows_in)[0];

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);
	PG_TRY();
	{
		/* Open the cursor */
		SPI_cursor_fetch(portal,forward,rows);
		
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();
	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);

	/* check the result */
	ntuples = SPI_processed;
	if (ntuples > 0)
	{
		result = rpgsql_get_results(ntuples, SPI_tuptable);
		SPI_freetuptable(SPI_tuptable);
	}
	else
		result = R_NilValue;

	POP_PLERRCONTEXT;
	return result;
}

void
plr_SPI_cursor_close(SEXP cursor_in)
{
	Portal				portal=NULL;
	MemoryContext		oldcontext;
	PREPARE_PG_TRY;
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.cursor_close");

	portal = R_ExternalPtrAddr(cursor_in);

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);
	PG_TRY();
	{
		/* Open the cursor */
		SPI_cursor_close(portal);
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();
	/* back to caller's memory context */
	MemoryContextSwitchTo(oldcontext);
}

void
plr_SPI_cursor_move(SEXP cursor_in,SEXP forward_in, SEXP rows_in)
{
	Portal				portal=NULL;
	MemoryContext		oldcontext;
	int					forward;
	int					rows;
	PREPARE_PG_TRY;
	PUSH_PLERRCONTEXT(rsupport_error_callback, "pg.spi.cursor_move");

	portal = R_ExternalPtrAddr(cursor_in);
	if(!IS_LOGICAL(forward_in))
	{
		error("pg.spi.cursor_move arg2 must be boolean");
		return;
	}
	if(!IS_INTEGER(rows_in))
	{
		error("pg.spi.cursor_move arg3 must be an integer");
		return;
	}
	forward = LOGICAL(forward_in)[0];
	rows  = INTEGER(rows_in)[0];

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);
	PG_TRY();
	{
		/* Open the cursor */
		SPI_cursor_move(portal, forward, rows);
	}
	PLR_PG_CATCH();
	PLR_PG_END_TRY();

	/* back to caller's	 memory context */
	MemoryContextSwitchTo(oldcontext);
}

void
throw_r_error(const char **msg)
{
	if (msg && *msg)
		last_R_error_msg = pstrdup(*msg);
	else
		last_R_error_msg = pstrdup("caught error calling R function");
}

/*
 * error context callback to let us supply a call-stack traceback
 */
static void
rsupport_error_callback(void *arg)
{
	if (arg)
		errcontext("In R support function %s", (char *) arg);
}

