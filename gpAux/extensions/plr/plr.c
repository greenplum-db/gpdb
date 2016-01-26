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
 * plr.c - Language handler and support functions
 */
#include "plr.h"

PG_MODULE_MAGIC;

/*
 * Global data
 */
MemoryContext plr_caller_context;
MemoryContext plr_SPI_context = NULL;
HTAB *plr_HashTable = (HTAB *) NULL;
char *last_R_error_msg = NULL;

static bool	plr_pm_init_done = false;
static bool	plr_be_init_done = false;

/* namespace OID for the PL/R language handler function */
static Oid plr_nspOid = InvalidOid;

int R_SignalHandlers = 1;  /* Exposed in R_interface.h */

/*
 * defines
 */

/* real max is 3 (for "PLR") plus number of characters in an Oid */
#define MAX_PRONAME_LEN		NAMEDATALEN

#define OPTIONS_NULL_CMD	"options(error = expression(NULL))"
#define THROWRERROR_CMD \
			"pg.throwrerror <-function(msg) " \
			"{" \
			"  msglen <- nchar(msg);" \
			"  if (substr(msg, msglen, msglen + 1) == \"\\n\")" \
			"    msg <- substr(msg, 1, msglen - 1);" \
			"  .C(\"throw_r_error\", as.character(msg));" \
			"}"
#define OPTIONS_THROWRERROR_CMD \
			"options(error = expression(pg.throwrerror(geterrmessage())))"
#define THROWNOTICE_CMD \
			"pg.thrownotice <-function(msg) " \
			"{.C(\"throw_pg_notice\", as.character(msg))}"
#define THROWERROR_CMD \
			"pg.throwerror <-function(msg) " \
			"{stop(msg, call. = FALSE)}"
#define OPTIONS_THROWWARN_CMD \
			"options(warning.expression = expression(pg.thrownotice(last.warning)))"
#define QUOTE_LITERAL_CMD \
			"pg.quoteliteral <-function(sql) " \
			"{.Call(\"plr_quote_literal\", sql)}"
#define QUOTE_IDENT_CMD \
			"pg.quoteident <-function(sql) " \
			"{.Call(\"plr_quote_ident\", sql)}"
#define SPI_EXEC_CMD \
			"pg.spi.exec <-function(sql) {.Call(\"plr_SPI_exec\", sql)}"
#define SPI_PREPARE_CMD \
			"pg.spi.prepare <-function(sql, argtypes = NA) " \
			"{.Call(\"plr_SPI_prepare\", sql, argtypes)}"
#define SPI_EXECP_CMD \
			"pg.spi.execp <-function(sql, argvalues = NA) " \
			"{.Call(\"plr_SPI_execp\", sql, argvalues)}"
#define SPI_CURSOR_OPEN_CMD \
			"pg.spi.cursor_open<-function(cursor_name,plan,argvalues=NA) " \
			"{.Call(\"plr_SPI_cursor_open\",cursor_name,plan,argvalues)}"
#define SPI_CURSOR_FETCH_CMD \
			"pg.spi.cursor_fetch<-function(cursor,forward,rows) " \
			"{.Call(\"plr_SPI_cursor_fetch\",cursor,forward,rows)}"
#define SPI_CURSOR_MOVE_CMD \
			"pg.spi.cursor_move<-function(cursor,forward,rows) " \
			"{.Call(\"plr_SPI_cursor_move\",cursor,forward,rows)}"
#define SPI_CURSOR_CLOSE_CMD \
			"pg.spi.cursor_close<-function(cursor) " \
			"{.Call(\"plr_SPI_cursor_close\",cursor)}"
#define SPI_LASTOID_CMD \
			"pg.spi.lastoid <-function() " \
			"{.Call(\"plr_SPI_lastoid\")}"
#define SPI_DBDRIVER_CMD \
			"dbDriver <-function(db_name)\n" \
			"{return(NA)}"
#define SPI_DBCONN_CMD \
			"dbConnect <- function(drv,user=\"\",password=\"\",host=\"\",dbname=\"\",port=\"\",tty =\"\",options=\"\")\n" \
			"{return(NA)}"
#define SPI_DBSENDQUERY_CMD \
			"dbSendQuery <- function(conn, sql) {\n" \
			"plan <- pg.spi.prepare(sql)\n" \
			"cursor_obj <- pg.spi.cursor_open(\"plr_cursor\",plan)\n" \
			"return(cursor_obj)\n" \
			"}"
#define SPI_DBFETCH_CMD \
			"fetch <- function(rs,n) {\n" \
			"data <- pg.spi.cursor_fetch(rs, TRUE, as.integer(n))\n" \
			"return(data)\n" \
			"}"
#define SPI_DBCLEARRESULT_CMD \
			"dbClearResult <- function(rs) {\n" \
			"pg.spi.cursor_close(rs)\n" \
			"}"
#define SPI_DBGETQUERY_CMD \
			"dbGetQuery <-function(conn, sql) {\n" \
			"data <- pg.spi.exec(sql)\n" \
			"return(data)\n" \
			"}"
#define SPI_DBREADTABLE_CMD \
			"dbReadTable <- function(con, name, row.names = \"row_names\", check.names = TRUE) {\n" \
			"data <- dbGetQuery(con, paste(\"SELECT * from\", name))\n" \
			"return(data)\n" \
			"}"
#define SPI_DBDISCONN_CMD \
			"dbDisconnect <- function(con)\n" \
			"{return(NA)}"
#define SPI_DBUNLOADDRIVER_CMD \
			"dbUnloadDriver <-function(drv)\n" \
			"{return(NA)}"
#define SPI_FACTOR_CMD \
			"pg.spi.factor <- function(arg1) {\n" \
			"  for (col in 1:ncol(arg1)) {\n" \
			"    if (!is.numeric(arg1[,col])) {\n" \
			"      arg1[,col] <- factor(arg1[,col])\n" \
			"    }\n" \
			"  }\n" \
			"  return(arg1)\n" \
			"}"
#define REVAL \
			"pg.reval <- function(arg1) {eval(parse(text = arg1))}"
#define PG_STATE_FIRSTPASS \
			"pg.state.firstpass <- TRUE"

#define CurrentTriggerData ((TriggerData *) fcinfo->context)


/*
 * static declarations
 */
static void plr_atexit(void);
static void plr_load_builtins(Oid funcid);
static void plr_init_all(Oid funcid);
static Datum plr_trigger_handler(PG_FUNCTION_ARGS);
static Datum plr_func_handler(PG_FUNCTION_ARGS);
static plr_function *compile_plr_function(FunctionCallInfo fcinfo);
static plr_function *do_compile(FunctionCallInfo fcinfo,
								HeapTuple procTup,
								plr_func_hashkey *hashkey);
static SEXP plr_parse_func_body(const char *body);
static SEXP plr_convertargs(plr_function *function, Datum *arg, bool *argnull, FunctionCallInfo fcinfo);
static void plr_error_callback(void *arg);
static Oid getNamespaceOidFromFunctionOid(Oid fnOid);
static bool haveModulesTable(Oid nspOid);
static char *getModulesSql(Oid nspOid);
#ifdef HAVE_WINDOW_FUNCTIONS
static void WinGetFrameData(WindowObject winobj, int argno, Datum *dvalues, bool *isnull, int *numels, bool *has_nulls);
#endif
//static void plr_resolve_polymorphic_argtypes(int numargs,
//											 Oid *argtypes, char *argmodes,
//											 Node *call_expr, bool forValidator,
//											 const char *proname);

static char **fetchArgNames(HeapTuple procTup, int nargs);

/*
 * plr_call_handler -	This is the only visible function
 *						of the PL interpreter. The PostgreSQL
 *						function manager and trigger manager
 *						call this function for execution of
 *						PL/R procedures.
 */
PG_FUNCTION_INFO_V1(plr_call_handler);

Datum
plr_call_handler(PG_FUNCTION_ARGS)
{
	Datum			retval;

	/* save caller's context */
	plr_caller_context = CurrentMemoryContext;

	if (SPI_connect() != SPI_OK_CONNECT)
		elog(ERROR, "SPI_connect failed");
	plr_SPI_context = CurrentMemoryContext;
	MemoryContextSwitchTo(plr_caller_context);

	/* initialize R if needed */
	plr_init_all(fcinfo->flinfo->fn_oid);

	if (CALLED_AS_TRIGGER(fcinfo))
		retval = plr_trigger_handler(fcinfo);
	else
		retval = plr_func_handler(fcinfo);

	return retval;
}

void
load_r_cmd(const char *cmd)
{
	SEXP		cmdSexp,
				cmdexpr;
	int			i,
				status;

	/*
	 * Init if not already done. This can happen when PL/R is not preloaded
	 * and reload_plr_modules() or install_rcmd() is called by the user prior
	 * to any PL/R functions.
	 */
	if (!plr_pm_init_done)
		plr_init();

	PROTECT(cmdSexp = NEW_CHARACTER(1));
	SET_STRING_ELT(cmdSexp, 0, COPY_TO_USER_STRING(cmd));
	PROTECT(cmdexpr = R_PARSEVECTOR(cmdSexp, -1, &status));
	if (status != PARSE_OK) {
		UNPROTECT(2);
		if (last_R_error_msg)
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter parse error"),
					 errdetail("%s", last_R_error_msg)));
		else
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter parse error"),
					 errdetail("R parse error caught in \"%s\".", cmd)));
	}

	/* Loop is needed here as EXPSEXP may be of length > 1 */
	for(i = 0; i < length(cmdexpr); i++)
	{
		R_tryEval(VECTOR_ELT(cmdexpr, i), R_GlobalEnv, &status);
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
						 errdetail("R expression evaluation error caught " \
								   "in \"%s\".", cmd)));
		}
	}

	UNPROTECT(2);
}

/*
 * plr_cleanup() - Let the embedded interpreter clean up after itself
 *
 * DO NOT make this static --- it has to be registered as an on_proc_exit()
 * callback
 */
void
PLR_CLEANUP
{
	char   *buf;
	char   *tmpdir = getenv("R_SESSION_TMPDIR");

	R_dot_Last();
	R_RunExitFinalizers();
	KillAllDevices();

	if(tmpdir)
	{
		int		rv;
		/*
		 * length needed = 'rm -rf ""' == 9
		 * plus 1 for NULL terminator
		 * plus length of dir string
		 */
		buf = (char *) palloc(9 + 1 + strlen(tmpdir));
		sprintf(buf, "rm -rf \"%s\"", tmpdir);

		/* ignoring return value */
		rv = system(buf);
		if (rv != 0)
			; /* do nothing */
	}
}

static void
plr_atexit(void)
{
	/* only react during plr startup */
	if (plr_pm_init_done)
		return;

	ereport(ERROR,
			(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("the R interpreter did not initialize"),
			 errhint("R_HOME must be correct in the environment " \
					 "of the user that starts the postmaster process.")));
}


/*
 * plr_init() - Initialize all that's safe to do in the postmaster
 *
 * DO NOT make this static --- it has to be callable by preload
 */
void
plr_init(void)
{
	char	   *r_home;
	int			rargc;
	char	   *rargv[] = {"PL/R", "--slave", "--silent", "--no-save", "--no-restore"};

	/* refuse to init more than once */
	if (plr_pm_init_done)
		return;

	/* refuse to start if R_HOME is not defined */
	r_home = getenv("R_HOME");
	if (r_home == NULL)
	{
		size_t		rh_len = strlen(R_HOME_DEFAULT);

		/* see if there is a compiled in default R_HOME */
		if (rh_len)
		{
			char	   *rhenv;
			MemoryContext		oldcontext;

			/* Needs to live until/unless we explicitly delete it */
			oldcontext = MemoryContextSwitchTo(TopMemoryContext);
			rhenv = palloc(8 + rh_len);
			MemoryContextSwitchTo(oldcontext);

			sprintf(rhenv, "R_HOME=%s", R_HOME_DEFAULT);
			putenv(rhenv);
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("environment variable R_HOME not defined"),
					 errhint("R_HOME must be defined in the environment " \
							 "of the user that starts the postmaster process.")));
	}

	rargc = sizeof(rargv)/sizeof(rargv[0]);

	/*
	 * register an exit callback to handle the case where R does not initialize
	 * and just exits with R_suicide()
	 */
	atexit(plr_atexit);

	/*
	 * Stop R using its own signal handlers
	 */
	R_SignalHandlers = 0;

	/*
	 * When initialization fails, R currently exits. Check the return
	 * value anyway in case this ever gets fixed
	 */
	if (!Rf_initEmbeddedR(rargc, rargv))
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("the R interpreter did not initialize"),
				 errhint("R_HOME must be correct in the environment " \
						 "of the user that starts the postmaster process.")));

	/* arrange for automatic cleanup at proc_exit */
	on_proc_exit(plr_cleanup, 0);

#ifndef WIN32
	/*
	 * Force non-interactive mode since R may not do so.
	 * See comment in Rembedded.c just after R_Interactive = TRUE:
	 * "Rf_initialize_R set this based on isatty"
	 * If Postgres still has the tty attached, R_Interactive remains TRUE
	 */
	R_Interactive = false;
#endif


	plr_pm_init_done = true;
}

/*
 * plr_load_builtins() - load "builtin" PL/R functions into R interpreter
 */
static void
plr_load_builtins(Oid funcid)
{
	int			j;
	char	   *cmd;
	char	   *cmds[] =
	{
		/* first turn off error handling by R */
		OPTIONS_NULL_CMD,

		/* set up the postgres error handler in R */
		THROWRERROR_CMD,
		OPTIONS_THROWRERROR_CMD,
		THROWNOTICE_CMD,
		THROWERROR_CMD,
		OPTIONS_THROWWARN_CMD,

		/* install the commands for SPI support in the interpreter */
		QUOTE_LITERAL_CMD,
		QUOTE_IDENT_CMD,
		SPI_EXEC_CMD,
		SPI_PREPARE_CMD,
		SPI_EXECP_CMD,
		SPI_CURSOR_OPEN_CMD,
		SPI_CURSOR_FETCH_CMD,
		SPI_CURSOR_MOVE_CMD,
		SPI_CURSOR_CLOSE_CMD,
		SPI_LASTOID_CMD,
		SPI_DBDRIVER_CMD,
		SPI_DBCONN_CMD,
		SPI_DBSENDQUERY_CMD,
		SPI_DBFETCH_CMD,
		SPI_DBCLEARRESULT_CMD,
		SPI_DBGETQUERY_CMD,
		SPI_DBREADTABLE_CMD,
		SPI_DBDISCONN_CMD,
		SPI_DBUNLOADDRIVER_CMD,
		SPI_FACTOR_CMD,

		/* handy predefined R functions */
		REVAL,

		/* terminate */
		NULL
	};

	/*
	 * temporarily turn off R error reporting -- it will be turned back on
	 * once the custom R error handler is installed from the plr library
	 */
	load_r_cmd(cmds[0]);

	/* next load the plr library into R */
	load_r_cmd(get_load_self_ref_cmd(funcid));

	/*
	 * run the rest of the R bootstrap commands, being careful to start
	 * at cmds[1] since we already executed cmds[0]
	 */
	for (j = 1; (cmd = cmds[j]); j++)
		load_r_cmd(cmds[j]);
}

/*
 * plr_load_modules() - Load procedures from
 *				  		table plr_modules (if it exists)
 *
 * The caller is responsible to ensure SPI has already been connected
 * DO NOT make this static --- it has to be callable by reload_plr_modules()
 */
void
plr_load_modules(void)
{
	int				spi_rc;
	char		   *cmd;
	int				i;
	int				fno;
	MemoryContext	oldcontext;
	char		   *modulesSql;

	/* switch to SPI memory context */
	SWITCHTO_PLR_SPI_CONTEXT(oldcontext);

	/*
	 * Check if table plr_modules exists
	 */
	if (!haveModulesTable(plr_nspOid))
	{
		/* clean up if SPI was used, and regardless restore caller's context */
		CLEANUP_PLR_SPI_CONTEXT(oldcontext);
		return;
	}

	/* plr_modules table exists -- get SQL code extract table's contents */
	modulesSql = getModulesSql(plr_nspOid);

	/* Read all the row's from it in the order of modseq */
	spi_rc = SPI_exec(modulesSql, 0);

	/* modulesSql no longer needed -- cleanup */
	pfree(modulesSql);

	if (spi_rc != SPI_OK_SELECT)
		/* internal error */
		elog(ERROR, "plr_init_load_modules: select from plr_modules failed");

	/* If there's nothing, no modules exist */
	if (SPI_processed == 0)
	{
		SPI_freetuptable(SPI_tuptable);
		/* clean up if SPI was used, and regardless restore caller's context */
		CLEANUP_PLR_SPI_CONTEXT(oldcontext);
		return;
	}

	/*
	 * There is at least on module to load. Get the
	 * source from the modsrc load it in the R interpreter
	 */
	fno = SPI_fnumber(SPI_tuptable->tupdesc, "modsrc");

	for (i = 0; i < SPI_processed; i++)
	{
		cmd = SPI_getvalue(SPI_tuptable->vals[i],
							SPI_tuptable->tupdesc, fno);

		if (cmd != NULL)
		{
			load_r_cmd(cmd);
			pfree(cmd);
		}
	}
	SPI_freetuptable(SPI_tuptable);

	/* clean up if SPI was used, and regardless restore caller's context */
	CLEANUP_PLR_SPI_CONTEXT(oldcontext);
}

static void
plr_init_all(Oid funcid)
{
	MemoryContext		oldcontext;

	/* everything initialized needs to live until/unless we explicitly delete it */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	/* execute postmaster-startup safe initialization */
	if (!plr_pm_init_done)
		plr_init();

	/*
	 * Any other initialization that must be done each time a new
	 * backend starts:
	 */
	if (!plr_be_init_done)
	{
		/* load "builtin" R functions */
		plr_load_builtins(funcid);

		/* obtain & store namespace OID of PL/R language handler */
		plr_nspOid = getNamespaceOidFromFunctionOid(funcid);

		/* try to load procedures from plr_modules */
		plr_load_modules();

		plr_be_init_done = true;
	}

	/* switch back to caller's context */
	MemoryContextSwitchTo(oldcontext);
}

static Datum
plr_trigger_handler(PG_FUNCTION_ARGS)
{
	plr_function  *function;
	SEXP			fun;
	SEXP			rargs;
	SEXP			rvalue;
	Datum			retval;
	Datum			arg[FUNC_MAX_ARGS];
	bool			argnull[FUNC_MAX_ARGS];
	TriggerData	   *trigdata = (TriggerData *) fcinfo->context;
	TupleDesc		tupdesc = trigdata->tg_relation->rd_att;
	Datum		   *dvalues;
	ArrayType	   *array;
#define FIXED_NUM_DIMS		1
	int				ndims = FIXED_NUM_DIMS;
	int				dims[FIXED_NUM_DIMS];
	int				lbs[FIXED_NUM_DIMS];
#undef FIXED_NUM_DIMS
	TRIGGERTUPLEVARS;
	ERRORCONTEXTCALLBACK;
	int				i;

	if (trigdata->tg_trigger->tgnargs > 0)
		dvalues = palloc(trigdata->tg_trigger->tgnargs * sizeof(Datum));
	else
		dvalues = NULL;
	
	/* Find or compile the function */
	function = compile_plr_function(fcinfo);

	/* set up error context */
	PUSH_PLERRCONTEXT(plr_error_callback, function->proname);

	/*
	 * Build up arguments for the trigger function. The data types
	 * are mostly hardwired in advance
	 */
	/* first is trigger name */
	arg[0] = DirectFunctionCall1(textin,
				 CStringGetDatum(trigdata->tg_trigger->tgname));
	argnull[0] = false;

	/* second is trigger relation oid */
	arg[1] = ObjectIdGetDatum(trigdata->tg_relation->rd_id);
	argnull[1] = false;

	/* third is trigger relation name */
	arg[2] = DirectFunctionCall1(textin,
				 CStringGetDatum(get_rel_name(trigdata->tg_relation->rd_id)));
	argnull[2] = false;

	/* fourth is when trigger fired, i.e. BEFORE or AFTER */
	if (TRIGGER_FIRED_BEFORE(trigdata->tg_event))
		arg[3] = DirectFunctionCall1(textin,
				 CStringGetDatum("BEFORE"));
	else if (TRIGGER_FIRED_AFTER(trigdata->tg_event))
		arg[3] = DirectFunctionCall1(textin,
				 CStringGetDatum("AFTER"));
	else
		/* internal error */
		elog(ERROR, "unrecognized tg_event");
	argnull[3] = false;

	/*
	 * fifth is level trigger fired, i.e. ROW or STATEMENT
	 * sixth is operation that fired trigger, i.e. INSERT, UPDATE, or DELETE
	 * seventh is NEW, eigth is OLD
	 */
	if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
	{
		arg[4] = DirectFunctionCall1(textin,
				 CStringGetDatum("STATEMENT"));

		if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
			arg[5] = DirectFunctionCall1(textin, CStringGetDatum("INSERT"));
		else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
			arg[5] = DirectFunctionCall1(textin, CStringGetDatum("DELETE"));
		else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
			arg[5] = DirectFunctionCall1(textin, CStringGetDatum("UPDATE"));
		else
			/* internal error */
			elog(ERROR, "unrecognized tg_event");

		arg[6] = (Datum) 0;
		argnull[6] = true;

		arg[7] = (Datum) 0;
		argnull[7] = true;
	}
	else if (TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
	{
		arg[4] = DirectFunctionCall1(textin,
				 CStringGetDatum("ROW"));

		if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
			SET_INSERT_ARGS_567;
		else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
			SET_DELETE_ARGS_567;
		else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
			SET_UPDATE_ARGS_567;
		else
			/* internal error */
			elog(ERROR, "unrecognized tg_event");
	}
	else
		/* internal error */
		elog(ERROR, "unrecognized tg_event");

	argnull[4] = false;
	argnull[5] = false;

	/*
	 * finally, ninth argument is a text array of trigger arguments
	 */
	for (i = 0; i < trigdata->tg_trigger->tgnargs; i++)
		dvalues[i] = DirectFunctionCall1(textin,
						 CStringGetDatum(trigdata->tg_trigger->tgargs[i]));

	dims[0] = trigdata->tg_trigger->tgnargs;
	lbs[0] = 1;
	array = construct_md_array(dvalues, NULL, ndims, dims, lbs,
								TEXTOID, -1, false, 'i');

	arg[8] = PointerGetDatum(array);
	argnull[8] = false;

	/*
	 * All done building args; from this point it is just like
	 * calling a non-trigger function, except we need to be careful
	 * that the return value tuple is the same tupdesc as the trigger tuple.
	 */
	PROTECT(fun = function->fun);

	/* Convert all call arguments */
	PROTECT(rargs = plr_convertargs(function, arg, argnull, fcinfo));

	/* Call the R function */
	PROTECT(rvalue = call_r_func(fun, rargs));

	/*
	 * Convert the return value from an R object to a Datum.
	 * We expect r_get_pg to do the right thing with missing or empty results.
	 */
	if (SPI_finish() != SPI_OK_FINISH)
		elog(ERROR, "SPI_finish failed");
	retval = r_get_pg(rvalue, function, fcinfo);

	POP_PLERRCONTEXT;
	UNPROTECT(3);

	return retval;
}

static Datum
plr_func_handler(PG_FUNCTION_ARGS)
{
	plr_function  *function;
	SEXP			fun;
	SEXP			rargs;
	SEXP			rvalue;
	Datum			retval;
	ERRORCONTEXTCALLBACK;

	/* Find or compile the function */
	function = compile_plr_function(fcinfo);

	/* set up error context */
	PUSH_PLERRCONTEXT(plr_error_callback, function->proname);

	PROTECT(fun = function->fun);

	/* Convert all call arguments */
	PROTECT(rargs = plr_convertargs(function, fcinfo->arg, fcinfo->argnull, fcinfo));

	/* Call the R function */
	PROTECT(rvalue = call_r_func(fun, rargs));

	/*
	 * Convert the return value from an R object to a Datum.
	 * We expect r_get_pg to do the right thing with missing or empty results.
	 */
	if (SPI_finish() != SPI_OK_FINISH)
		elog(ERROR, "SPI_finish failed");
	retval = r_get_pg(rvalue, function, fcinfo);

	POP_PLERRCONTEXT;
	UNPROTECT(3);

	return retval;
}


/* ----------
 * compile_plr_function
 *
 * Note: it's important for this to fall through quickly if the function
 * has already been compiled.
 * ----------
 */
plr_function *
compile_plr_function(FunctionCallInfo fcinfo)
{
	Oid					funcOid = fcinfo->flinfo->fn_oid;
	HeapTuple			procTup;
	Form_pg_proc		procStruct;
	plr_function	   *function;
	plr_func_hashkey	hashkey;
	bool				hashkey_valid = false;
	ERRORCONTEXTCALLBACK;

	/*
	 * Lookup the pg_proc tuple by Oid; we'll need it in any case
	 */
	procTup = SearchSysCache(PROCOID,
							 ObjectIdGetDatum(funcOid),
							 0, 0, 0);
	if (!HeapTupleIsValid(procTup))
		/* internal error */
		elog(ERROR, "cache lookup failed for proc %u", funcOid);

	procStruct = (Form_pg_proc) GETSTRUCT(procTup);

	/* set up error context */
	PUSH_PLERRCONTEXT(plr_error_callback, NameStr(procStruct->proname));

	/*
	 * See if there's already a cache entry for the current FmgrInfo.
	 * If not, try to find one in the hash table.
	 */
	function = (plr_function *) fcinfo->flinfo->fn_extra;

	if (!function)
	{
		/* First time through in this backend?  If so, init hashtable */
		if (!plr_HashTable)
			plr_HashTableInit();

		/* Compute hashkey using function signature and actual arg types */
		compute_function_hashkey(fcinfo, procStruct, &hashkey);
		hashkey_valid = true;

		/* And do the lookup */
		function = plr_HashTableLookup(&hashkey);

		/*
		 * first time through for this statement, set
		 * firstpass to TRUE
		 */
		load_r_cmd(PG_STATE_FIRSTPASS);
	}

	if (function)
	{
		bool	function_valid;

		/* We have a compiled function, but is it still valid? */
		if (function->fn_xmin == HeapTupleHeaderGetXmin(procTup->t_data) &&
			ItemPointerEquals(&function->fn_tid, &procTup->t_self))
			function_valid = true;
		else
			function_valid = false;

		if (!function_valid)
		{
			/*
			 * Nope, drop the hashtable entry.  XXX someday, free all the
			 * subsidiary storage as well.
			 */
			plr_HashTableDelete(function);

			/* free some of the subsidiary storage */
			xpfree(function->proname);
			R_ReleaseObject(function->fun);
			xpfree(function);

			function = NULL;
		}
	}

	/*
	 * If the function wasn't found or was out-of-date, we have to compile it
	 */
	if (!function)
	{
		/*
		 * Calculate hashkey if we didn't already; we'll need it to store
		 * the completed function.
		 */
		if (!hashkey_valid)
			compute_function_hashkey(fcinfo, procStruct, &hashkey);

		/*
		 * Do the hard part.
		 */
		function = do_compile(fcinfo, procTup, &hashkey);
	}

	ReleaseSysCache(procTup);

	/*
	 * Save pointer in FmgrInfo to avoid search on subsequent calls
	 */
	fcinfo->flinfo->fn_extra = (void *) function;

	POP_PLERRCONTEXT;

	/*
	 * Finally return the compiled function
	 */
	return function;
}


/*
 * This is the slow part of compile_plr_function().
 */
static plr_function *
do_compile(FunctionCallInfo fcinfo,
		   HeapTuple procTup,
		   plr_func_hashkey *hashkey)
{
	Form_pg_proc			procStruct = (Form_pg_proc) GETSTRUCT(procTup);
	Datum					prosrcdatum;
	bool					isnull;
	bool					is_trigger = CALLED_AS_TRIGGER(fcinfo) ? true : false;
	plr_function		   *function = NULL;
	Oid						fn_oid = fcinfo->flinfo->fn_oid;
	char					internal_proname[MAX_PRONAME_LEN];
	char				   *proname;
	Oid						result_typid;
	HeapTuple				langTup;
	HeapTuple				typeTup;
	Form_pg_language		langStruct;
	Form_pg_type			typeStruct;
	StringInfo				proc_internal_def = makeStringInfo();
	StringInfo				proc_internal_args = makeStringInfo();
	char				   *proc_source;
	MemoryContext			oldcontext;
	char				   *p;

	/* grab the function name */
	proname = NameStr(procStruct->proname);

	/* Build our internal proc name from the functions Oid */
	sprintf(internal_proname, "PLR%u", fn_oid);

	/*
	 * analyze the functions arguments and returntype and store
	 * the in-/out-functions in the function block and create
	 * a new hashtable entry for it.
	 *
	 * Then load the procedure into the R interpreter.
	 */

	/* the function structure needs to live until we explicitly delete it */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	/* Allocate a new procedure description block */
	function = (plr_function *) palloc(sizeof(plr_function));
	if (function == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));

	MemSet(function, 0, sizeof(plr_function));

	function->proname = pstrdup(proname);
	function->fn_xmin = HeapTupleHeaderGetXmin(procTup->t_data);
	function->fn_tid = procTup->t_self;

#ifdef HAVE_WINDOW_FUNCTIONS
	/* Flag for window functions */
	function->iswindow = procStruct->proiswindow;
#endif

	/* Lookup the pg_language tuple by Oid*/
	langTup = SearchSysCache(LANGOID,
							 ObjectIdGetDatum(procStruct->prolang),
							 0, 0, 0);
	if (!HeapTupleIsValid(langTup))
	{
		xpfree(function->proname);
		xpfree(function);
		/* internal error */
		elog(ERROR, "cache lookup failed for language %u",
			 procStruct->prolang);
	}
	langStruct = (Form_pg_language) GETSTRUCT(langTup);
	function->lanpltrusted = langStruct->lanpltrusted;
	ReleaseSysCache(langTup);

	/* get the functions return type */
	if (procStruct->prorettype == ANYARRAYOID ||
		procStruct->prorettype == ANYELEMENTOID)
	{
		result_typid = get_fn_expr_rettype(fcinfo->flinfo);
		if (result_typid == InvalidOid)
			result_typid = procStruct->prorettype;
	}
	else
		result_typid = procStruct->prorettype;

	/*
	 * Get the required information for input conversion of the
	 * return value.
	 */
	if (!is_trigger)
	{
		function->result_typid = result_typid;
		typeTup = SearchSysCache(TYPEOID,
								 ObjectIdGetDatum(function->result_typid),
								 0, 0, 0);
		if (!HeapTupleIsValid(typeTup))
		{
			xpfree(function->proname);
			xpfree(function);
			/* internal error */
			elog(ERROR, "cache lookup failed for return type %u",
				 procStruct->prorettype);
		}
		typeStruct = (Form_pg_type) GETSTRUCT(typeTup);

		/* Disallow pseudotype return type except VOID or RECORD */
		/* (note we already replaced ANYARRAY/ANYELEMENT) */
		if (typeStruct->typtype == 'p')
		{
			if (procStruct->prorettype == VOIDOID ||
				procStruct->prorettype == RECORDOID)
				 /* okay */ ;
			else if (procStruct->prorettype == TRIGGEROID)
			{
				xpfree(function->proname);
				xpfree(function);
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("trigger functions may only be called as triggers")));
			}
			else
			{
				xpfree(function->proname);
				xpfree(function);
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("plr functions cannot return type %s",
								format_type_be(procStruct->prorettype))));
			}
		}

		if (typeStruct->typrelid != InvalidOid ||
			procStruct->prorettype == RECORDOID)
			function->result_istuple = true;

		perm_fmgr_info(typeStruct->typinput, &(function->result_in_func));

		if (function->result_istuple)
		{
			int16			typlen;
			bool			typbyval;
			char			typdelim;
			Oid				typinput,
							typelem;
			FmgrInfo		inputproc;
			char			typalign;
			TupleDesc		tupdesc;
			int				i;
			ReturnSetInfo  *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
			
			/* check to see if caller supports us returning a tuplestore */
			if (!rsinfo || !(rsinfo->allowedModes & SFRM_Materialize))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
				 		errmsg("materialize mode required, but it is not "
								"allowed in this context")));

			tupdesc = rsinfo->expectedDesc;
			function->result_natts = tupdesc->natts;
			
			function->result_fld_elem_typid = (Oid *)
											palloc0(function->result_natts * sizeof(Oid));
			function->result_fld_elem_in_func = (FmgrInfo *)
											palloc0(function->result_natts * sizeof(FmgrInfo));
			function->result_fld_elem_typlen = (int *)
											palloc0(function->result_natts * sizeof(int));
			function->result_fld_elem_typbyval = (bool *)
											palloc0(function->result_natts * sizeof(bool));
			function->result_fld_elem_typalign = (char *)
											palloc0(function->result_natts * sizeof(char));
	
			for (i = 0; i < function->result_natts; i++)
			{
				function->result_fld_elem_typid[i] = get_element_type(tupdesc->attrs[i]->atttypid);
				if (OidIsValid(function->result_fld_elem_typid[i]))
				{
					get_type_io_data(function->result_fld_elem_typid[i], IOFunc_input,
										&typlen, &typbyval, &typalign,
										&typdelim, &typelem, &typinput);
		
					perm_fmgr_info(typinput, &inputproc);
		
					function->result_fld_elem_in_func[i] = inputproc;
					function->result_fld_elem_typbyval[i] = typbyval;
					function->result_fld_elem_typlen[i] = typlen;
					function->result_fld_elem_typalign[i] = typalign;
				}
			}
		}
		else
		{
			/*
			 * Is return type an array? get_element_type will return InvalidOid
			 * instead of actual element type if the type is not a varlena array.
			 */
			if (OidIsValid(get_element_type(function->result_typid)))
				function->result_elem = typeStruct->typelem;
			else	/* not an array */
				function->result_elem = InvalidOid;
			
			/*
			 * if we have an array type, get the element type's in_func
			 */
			if (function->result_elem != InvalidOid)
			{
				int16		typlen;
				bool		typbyval;
				char		typdelim;
				Oid			typinput,
							typelem;
				FmgrInfo	inputproc;
				char		typalign;
	
				get_type_io_data(function->result_elem, IOFunc_input,
										&typlen, &typbyval, &typalign,
										&typdelim, &typelem, &typinput);
	
				perm_fmgr_info(typinput, &inputproc);
	
				function->result_elem_in_func = inputproc;
				function->result_elem_typbyval = typbyval;
				function->result_elem_typlen = typlen;
				function->result_elem_typalign = typalign;
			}
		}
		ReleaseSysCache(typeTup);
	}
	else /* trigger */
	{
		function->result_typid = TRIGGEROID;
		function->result_istuple = true;
		function->result_elem = InvalidOid;
	}

	/*
	 * Get the required information for output conversion
	 * of all procedure arguments
	 */
	if (!is_trigger)
	{
		int		i;
		// bool		forValidator = false;
		// int			numargs;
		// Oid		   *argtypes;
		// char	  **argnames;
		// char	   *argmodes;

		// numargs = get_func_arg_info(procTup,
		// 							&argtypes, &argnames, &argmodes);

		// plr_resolve_polymorphic_argtypes(numargs, argtypes, argmodes,
		// 									 fcinfo->flinfo->fn_expr,
		// 									 forValidator,
		// 									 function->proname);
		GET_ARG_NAMES;

		function->nargs = procStruct->pronargs;
		for (i = 0; i < function->nargs; i++)
		{
			// char		argmode = argmodes ? argmodes[i] : PROARGMODE_IN;

			// if (argmode != PROARGMODE_IN &&
			// 	argmode != PROARGMODE_INOUT &&
			// 	argmode != PROARGMODE_VARIADIC)
			// 	continue;

			/*
			 * Since we already did the replacement of polymorphic
			 * argument types by actual argument types while computing
			 * the hashkey, we can just use those results.
			 */
			function->arg_typid[i] = hashkey->argtypes[i];

			typeTup = SearchSysCache(TYPEOID,
						ObjectIdGetDatum(function->arg_typid[i]),
									 0, 0, 0);
			if (!HeapTupleIsValid(typeTup))
			{
				Oid		arg_typid = function->arg_typid[i];

				xpfree(function->proname);
				xpfree(function);
				/* internal error */
				elog(ERROR, "cache lookup failed for argument type %u", arg_typid);
			}
			typeStruct = (Form_pg_type) GETSTRUCT(typeTup);

			/* Disallow pseudotype argument
			 * note we already replaced ANYARRAY/ANYELEMENT
			 */
			if (typeStruct->typtype == 'p')
			{
				Oid		arg_typid = function->arg_typid[i];

				xpfree(function->proname);
				xpfree(function);
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("plr functions cannot take type %s",
								format_type_be(arg_typid))));
			}

			if (typeStruct->typrelid != InvalidOid)
				function->arg_is_rel[i] = 1;
			else
				function->arg_is_rel[i] = 0;

			perm_fmgr_info(typeStruct->typoutput, &(function->arg_out_func[i]));

			/* save argument typbyval in case we need for optimization in conversions */
			function->arg_typbyval[i] = typeStruct->typbyval;

			/*
			 * Is argument type an array? get_element_type will return InvalidOid
			 * instead of actual element type if the type is not a varlena array.
			 */
			if (OidIsValid(get_element_type(function->arg_typid[i])))
				function->arg_elem[i] = typeStruct->typelem;
			else	/* not ant array */
				function->arg_elem[i] = InvalidOid;

			if (i > 0)
				appendStringInfo(proc_internal_args, ",");
			SET_ARG_NAME;

			ReleaseSysCache(typeTup);

			if (function->arg_elem[i] != InvalidOid)
			{
				int16		typlen;
				bool		typbyval;
				char		typdelim;
				Oid			typoutput,
							typelem;
				FmgrInfo	outputproc;
				char		typalign;

				get_type_io_data(function->arg_elem[i], IOFunc_output,
										 &typlen, &typbyval, &typalign,
										 &typdelim, &typelem, &typoutput);

				perm_fmgr_info(typoutput, &outputproc);

				function->arg_elem_out_func[i] = outputproc;
				function->arg_elem_typbyval[i] = typbyval;
				function->arg_elem_typlen[i] = typlen;
				function->arg_elem_typalign[i] = typalign;
			}
		}
		FREE_ARG_NAMES;

#ifdef HAVE_WINDOW_FUNCTIONS
		if (function->iswindow)
		{
			for (i = 0; i < function->nargs; i++)
			{
				appendStringInfo(proc_internal_args, ",");
				SET_FRAME_ARG_NAME;
			}
			SET_FRAME_XARG_NAMES;
		}
#endif
	}
	else
	{
		int16		typlen;
		bool		typbyval;
		char		typdelim;
		Oid			typoutput,
					typelem;
		FmgrInfo	outputproc;
		char		typalign;

		function->nargs = TRIGGER_NARGS;

		/* take care of the only non-TEXT first */
		get_type_io_data(OIDOID, IOFunc_output,
								 &typlen, &typbyval, &typalign,
								 &typdelim, &typelem, &typoutput);

		function->arg_typid[1] = OIDOID;
		function->arg_elem[1] = InvalidOid;
		function->arg_is_rel[1] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[1]));

		get_type_io_data(TEXTOID, IOFunc_output,
								 &typlen, &typbyval, &typalign,
								 &typdelim, &typelem, &typoutput);

		function->arg_typid[0] = TEXTOID;
		function->arg_elem[0] = InvalidOid;
		function->arg_is_rel[0] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[0]));

		function->arg_typid[2] = TEXTOID;
		function->arg_elem[2] = InvalidOid;
		function->arg_is_rel[2] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[2]));

		function->arg_typid[3] = TEXTOID;
		function->arg_elem[3] = InvalidOid;
		function->arg_is_rel[3] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[3]));

		function->arg_typid[4] = TEXTOID;
		function->arg_elem[4] = InvalidOid;
		function->arg_is_rel[4] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[4]));

		function->arg_typid[5] = TEXTOID;
		function->arg_elem[5] = InvalidOid;
		function->arg_is_rel[5] = 0;
		perm_fmgr_info(typoutput, &(function->arg_out_func[5]));

		function->arg_typid[6] = RECORDOID;
		function->arg_elem[6] = InvalidOid;
		function->arg_is_rel[6] = 1;

		function->arg_typid[7] = RECORDOID;
		function->arg_elem[7] = InvalidOid;
		function->arg_is_rel[7] = 1;

		function->arg_typid[8] = TEXTARRAYOID;
		function->arg_elem[8] = TEXTOID;
		function->arg_is_rel[8] = 0;
		get_type_io_data(function->arg_elem[8], IOFunc_output,
								 &typlen, &typbyval, &typalign,
								 &typdelim, &typelem, &typoutput);
		perm_fmgr_info(typoutput, &outputproc);
		function->arg_elem_out_func[8] = outputproc;
		function->arg_elem_typbyval[8] = typbyval;
		function->arg_elem_typlen[8] = typlen;
		function->arg_elem_typalign[8] = typalign;

		/* trigger procedure has fixed args */
		appendStringInfo(proc_internal_args,
						"pg.tg.name,pg.tg.relid,pg.tg.relname,pg.tg.when,"
						"pg.tg.level,pg.tg.op,pg.tg.new,pg.tg.old,pg.tg.args");
	}

	/*
	 * Create the R command to define the internal
	 * procedure
	 */
	appendStringInfo(proc_internal_def,
					 "%s <- function(%s) {",
					 internal_proname,
					 proc_internal_args->data);

	/* Add user's function definition to proc body */
	prosrcdatum = SysCacheGetAttr(PROCOID, procTup,
								  Anum_pg_proc_prosrc, &isnull);
	if (isnull)
		elog(ERROR, "null prosrc");
	proc_source = DatumGetCString(DirectFunctionCall1(textout, prosrcdatum));

	/*
	 * replace any carriage returns with either a space or a newline,
	 * as appropriate
	 */
	p = proc_source;
	while (*p != '\0')
	{
		if (p[0] == '\r')
		{
			if (p[1] == '\n')
				/* for crlf sequence, write over the cr with a space */
				*p++ = ' ';
			else
				/* otherwise write over the cr with a nl */
				*p++ = '\n';
		}
		else
			p++;
	}

	/* parse or find the R function */
	if(proc_source && proc_source[0])
		appendStringInfo(proc_internal_def, "%s}", proc_source);
	else
		appendStringInfo(proc_internal_def, "%s(%s)}",
						 function->proname,
						 proc_internal_args->data);
	function->fun = plr_parse_func_body(proc_internal_def->data);

	R_PreserveObject(function->fun);

	pfree(proc_source);
	freeStringInfo(proc_internal_def);

	/* test that this is really a function. */
	if(function->fun == R_NilValue)
	{
		xpfree(function->proname);
		xpfree(function);
		/* internal error */
		elog(ERROR, "cannot create internal procedure %s",
			 internal_proname);
	}

	/* switch back to the context we were called with */
	MemoryContextSwitchTo(oldcontext);

	/*
	 * add it to the hash table
	 */
	plr_HashTableInsert(function, hashkey);

	return function;
}

static SEXP
plr_parse_func_body(const char *body)
{
	SEXP	rbody;
	SEXP	fun;
    SEXP	tmp;
	int		status;

	PROTECT(rbody = mkString(body));
	PROTECT(tmp = R_PARSEVECTOR(rbody, -1, &status));

	if (tmp != R_NilValue)
		PROTECT(fun = VECTOR_ELT(tmp, 0));
	else
		PROTECT(fun = R_NilValue);

	if (status != PARSE_OK)
	{
		UNPROTECT(3);
		if (last_R_error_msg)
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter parse error"),
					 errdetail("%s", last_R_error_msg)));
		else
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter parse error"),
					 errdetail("R parse error caught " \
							   "in \"%s\".", body)));
	}

	UNPROTECT(3);
	return(fun);
}

SEXP
call_r_func(SEXP fun, SEXP rargs)
{
	int		i;
	int		errorOccurred;
	SEXP	obj,
			args,
			call,
			ans;
	long	n = length(rargs);

	if(n > 0)
	{
		PROTECT(obj = args = allocList(n));
		for (i = 0; i < n; i++)
		{
			SETCAR(obj, VECTOR_ELT(rargs, i));
			obj = CDR(obj);
		}
		UNPROTECT(1);
        /*
         * NB: the headers of both R and Postgres define a function
         * called lcons, so use the full name to be precise about what
         * function we're calling.
         */
		PROTECT(call = Rf_lcons(fun, args));
	}
	else
	{
		PROTECT(call = allocVector(LANGSXP,1));
		SETCAR(call, fun);
	}

	ans = R_tryEval(call, R_GlobalEnv, &errorOccurred);
	UNPROTECT(1);

	if(errorOccurred)
	{
		if (last_R_error_msg)
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter expression evaluation error"),
					 errdetail("%s", last_R_error_msg)));
		else
			ereport(ERROR,
					(errcode(ERRCODE_DATA_EXCEPTION),
					 errmsg("R interpreter expression evaluation error")));
	}
	return ans;
}

static SEXP
plr_convertargs(plr_function *function, Datum *arg, bool *argnull, FunctionCallInfo fcinfo)
{
	int		i;
	int		m = 1;
	int		c = 0;
	SEXP	rargs,
			el;

#ifdef HAVE_WINDOW_FUNCTIONS
	if (function->iswindow)
	{
		/*
		 * For WINDOW functions, create an array of R objects with
		 * the number of elements equal to twice the number of arguments.
		 */
		m = 2;
		c = 2;
	}
#endif

	/*
	 * Create an array of R objects with the number of elements
	 * as a function of the number of arguments.
	 */
	PROTECT(rargs = allocVector(VECSXP, c + (m * function->nargs)));

	/*
	 * iterate over the arguments, convert each of them and put them in
	 * the array.
	 */
	for (i = 0; i < function->nargs; i++)
	{
#ifdef HAVE_WINDOW_FUNCTIONS
		if (!function->iswindow)
		{
#endif
			if (argnull[i])
			{
				/* fast track for null arguments */
				PROTECT(el = R_NilValue);
			}
			else if (function->arg_is_rel[i])
			{
				/* for tuple args, convert to a one row data.frame */
				CONVERT_TUPLE_TO_DATAFRAME;
			}
			else if (function->arg_elem[i] == InvalidOid)
			{
				/* for scalar args, convert to a one row vector */
				Datum		dvalue = arg[i];
				Oid			arg_typid = function->arg_typid[i];
				FmgrInfo	arg_out_func = function->arg_out_func[i];

				PROTECT(el = pg_scalar_get_r(dvalue, arg_typid, arg_out_func));
			}
			else
			{
				/* better be a pg array arg, convert to a multi-row vector */
				Datum		dvalue = (Datum) PG_DETOAST_DATUM(arg[i]);
				FmgrInfo	out_func = function->arg_elem_out_func[i];
				int			typlen = function->arg_elem_typlen[i];
				bool		typbyval = function->arg_elem_typbyval[i];
				char		typalign = function->arg_elem_typalign[i];

				PROTECT(el = pg_array_get_r(dvalue, out_func, typlen, typbyval, typalign));
			}
			SET_VECTOR_ELT(rargs, i, el);
			UNPROTECT(1);
#ifdef HAVE_WINDOW_FUNCTIONS
		}
		else
		{
			Datum			dvalue;
			bool			isnull;
			WindowObject	winobj = PG_WINDOW_OBJECT();

			/* get datum for the current row of the window frame */
			dvalue = WinGetFuncArgInFrame(winobj, i, 0, WINDOW_SEEK_CURRENT, false, &isnull, NULL);

			if (isnull)
			{
				/* fast track for null arguments */
				PROTECT(el = R_NilValue);
			}
			else if (function->arg_is_rel[i])
			{
				/* keep compiler quiet */
				el = R_NilValue;

				elog(ERROR, "Tuple arguments not supported in PL/R Window Functions");
			}
			else if (function->arg_elem[i] == InvalidOid)
			{
				/* for scalar args, convert to a one row vector */
				Oid			arg_typid = function->arg_typid[i];
				FmgrInfo	arg_out_func = function->arg_out_func[i];

				PROTECT(el = pg_scalar_get_r(dvalue, arg_typid, arg_out_func));
			}
			else
			{
				/* better be a pg array arg, convert to a multi-row vector */
				FmgrInfo	out_func = function->arg_elem_out_func[i];
				int			typlen = function->arg_elem_typlen[i];
				bool		typbyval = function->arg_elem_typbyval[i];
				char		typalign = function->arg_elem_typalign[i];

				dvalue = (Datum) PG_DETOAST_DATUM(dvalue);
				PROTECT(el = pg_array_get_r(dvalue, out_func, typlen, typbyval, typalign));
			}
			SET_VECTOR_ELT(rargs, i, el);
			UNPROTECT(1);
		}
#endif
	}

#ifdef HAVE_WINDOW_FUNCTIONS
	/* now get an array of datums for the entire window frame for each argument */
	if (function->iswindow)
	{
		WindowObject	winobj = PG_WINDOW_OBJECT();
		int64			totalrows = WinGetPartitionRowCount(winobj);
		int				numels = 0;

		for (i = 0; i < function->nargs; i++)
		{
			Datum		   *dvalues = palloc0(totalrows * sizeof(Datum));
			bool		   *isnulls = palloc0(totalrows * sizeof(bool));
			Oid				datum_typid;
			FmgrInfo		datum_out_func;
			bool			datum_typbyval;
			bool			has_nulls;

			WinGetFrameData(winobj, i, dvalues, isnulls, &numels, &has_nulls);

			datum_typid = function->arg_typid[i];
			datum_out_func = function->arg_out_func[i];
			datum_typbyval = function->arg_typbyval[i];
			PROTECT(el = pg_datum_array_get_r(dvalues, isnulls, numels, has_nulls,
											  datum_typid, datum_out_func, datum_typbyval));

			/*
			 * We already set function->nargs arguments
			 * so we must start with a function->nargs
			 */
			SET_VECTOR_ELT(rargs, function->nargs + i, el);

			UNPROTECT(1);
		}

		/* fnumrows */
		PROTECT(el = NEW_NUMERIC(1));
		NUMERIC_DATA(el)[0] = (double) numels;
		SET_VECTOR_ELT(rargs, m * function->nargs + 0, el);
		UNPROTECT(1);

		/* prownum */
		PROTECT(el = NEW_NUMERIC(1));
		NUMERIC_DATA(el)[0] = (double) WinGetCurrentPosition(winobj) + 1;;
		SET_VECTOR_ELT(rargs, m * function->nargs + 1, el);
		UNPROTECT(1);
	}
#endif

	UNPROTECT(1);
	return(rargs);
}

/*
 * error context callback to let us supply a call-stack traceback
 */
static void
plr_error_callback(void *arg)
{
	if (arg)
		errcontext("In PL/R function %s", (char *) arg);
}


/*
 * Fetch the argument names, if any, from the proargnames field of the
 * pg_proc tuple.  Results are palloc'd.
 *
 * Borrowed from src/pl/plpgsql/src/pl_comp.c
 */
static char **
fetchArgNames(HeapTuple procTup, int nargs)
{
	Datum		argnamesDatum;
	bool		isNull;
	Datum	   *elems;
	int			nelems;
	char	  **result;
	int			i;

	if (nargs == 0)
		return NULL;

	argnamesDatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_proargnames,
									&isNull);
	if (isNull)
		return NULL;

	deconstruct_array(DatumGetArrayTypeP(argnamesDatum),
					  TEXTOID, -1, false, 'i',
					  &elems, NULL, &nelems);

	if (nelems != nargs)		/* should not happen */
		elog(ERROR, "proargnames must have the same number of elements as the function has arguments");

	result = (char **) palloc(sizeof(char *) * nargs);

	for (i=0; i < nargs; i++)
		result[i] = DatumGetCString(DirectFunctionCall1(textout, elems[i]));

	return result;
}

/*
 * getNamespaceOidFromFunctionOid - Returns the OID of the namespace for the
 * language handler function for the postgresql function with the OID equal
 * to the input argument.
 */
static Oid
getNamespaceOidFromFunctionOid(Oid fnOid)
{
	HeapTuple			procTuple;
	HeapTuple			langTuple;
	Form_pg_proc		procStruct;
	Form_pg_language	langStruct;
	Oid					langOid;
	Oid					hfnOid;
	Oid					nspOid;

	/* Lookup the pg_proc tuple for the called function by OID */
	procTuple = SearchSysCache(PROCOID, ObjectIdGetDatum(fnOid), 0, 0, 0);

	if (!HeapTupleIsValid(procTuple))
		/* internal error */
		elog(ERROR, "cache lookup failed for function %u", fnOid);

	procStruct = (Form_pg_proc) GETSTRUCT(procTuple);
	langOid = procStruct->prolang;
	ReleaseSysCache(procTuple);

	/* Lookup the pg_language tuple by OID */
	langTuple = SearchSysCache(LANGOID, ObjectIdGetDatum(langOid), 0, 0, 0);
	if (!HeapTupleIsValid(langTuple))
		/* internal error */
		elog(ERROR, "cache lookup failed for language %u", langOid);

	langStruct = (Form_pg_language) GETSTRUCT(langTuple);
	hfnOid = langStruct->lanplcallfoid;
	ReleaseSysCache(langTuple);

	/* Lookup the pg_proc tuple for the language handler by OID */
	procTuple = SearchSysCache(PROCOID, ObjectIdGetDatum(hfnOid), 0, 0, 0);

	if (!HeapTupleIsValid(procTuple))
		/* internal error */
		elog(ERROR, "cache lookup failed for function %u", hfnOid);

	procStruct = (Form_pg_proc) GETSTRUCT(procTuple);
	nspOid = procStruct->pronamespace;
	ReleaseSysCache(procTuple);

	return nspOid;
}

/*
 * haveModulesTable(Oid) -- Check if table plr_modules exists in the namespace
 * designated by the OID input argument.
 */
static bool
haveModulesTable(Oid nspOid)
{
	StringInfo		sql = makeStringInfo();
	char		   *sql_format = "SELECT NULL "
								 "FROM pg_catalog.pg_class "
								 "WHERE "
								 "relname = 'plr_modules' AND "
								 "relnamespace = %u";
    int  spiRc;

	appendStringInfo(sql, sql_format, nspOid);

	spiRc = SPI_exec(sql->data, 1);
	if (spiRc != SPI_OK_SELECT)
		/* internal error */
		elog(ERROR, "haveModulesTable: select from pg_class failed");

	return SPI_processed == 1;
}

/*
 * getModulesSql(Oid) - Builds and returns SQL needed to extract contents from
 * plr_modules table.  The table must exist in the namespace designated by the
 * OID input argument.  Results are ordered by the "modseq" field.
 *
 * IMPORTANT: return value must be pfree'd
 */
static char *
getModulesSql(Oid nspOid)
{
	StringInfo		sql = makeStringInfo();
	char		   *sql_format = "SELECT modseq, modsrc "
								 "FROM %s "
								 "ORDER BY modseq";

	appendStringInfo(sql, sql_format,
					 quote_qualified_identifier(get_namespace_name(nspOid),
												"plr_modules"));

    return sql->data;
}

#ifdef DEBUGPROTECT
SEXP
pg_protect(SEXP s, char *fn, int ln)
{
	elog(NOTICE, "\tPROTECT\t1\t%s\t%d", fn, ln);
	return protect(s);
}

void
pg_unprotect(int n, char *fn, int ln)
{
	elog(NOTICE, "\tUNPROTECT\t%d\t%s\t%d", n, fn, ln);
	unprotect(n);
}
#endif /* DEBUGPROTECT */

#ifdef HAVE_WINDOW_FUNCTIONS
/*
 * WinGetFrameData
 *		Evaluate a window function's argument expression on a specified
 *		window frame, returning an array of Datums for the frame
 *
 * argno: argument number to evaluate (counted from 0)
 * isnull: output argument, receives isnull status of result
 */
static void
WinGetFrameData(WindowObject winobj, int argno, Datum *dvalues, bool *isnulls, int *numels, bool *has_nulls)
{
	int64		i = 0;

	*has_nulls = false;
	for(;;)
	{
		Datum	lcl_dvalue;
		bool	lcl_isnull;
		bool	isout;
		bool	set_mark;

		if (i > 0)
			set_mark = false;
		else
			set_mark = true;

		lcl_dvalue = WinGetFuncArgInFrame(winobj, argno, i, WINDOW_SEEK_HEAD,
										  set_mark, &lcl_isnull, &isout);

		if (!isout)
		{
			dvalues[i] = lcl_dvalue;
			isnulls[i] = lcl_isnull;
			if (lcl_isnull)
				*has_nulls = true;
		}
		else
		{
			*numels = i;
			break;
		}

		i++;
	};
}
#endif

/*
 * swiped out of plpgsql pl_comp.c
 *
 * This is the same as the standard resolve_polymorphic_argtypes() function,
 * but with a special case for validation: assume that polymorphic arguments
 * are integer, integer-array or integer-range.  Also, we go ahead and report
 * the error if we can't resolve the types.
 */

// static void
// plr_resolve_polymorphic_argtypes(int numargs,
// 								 Oid *argtypes, char *argmodes,
// 								 Node *call_expr, bool forValidator,
// 								 const char *proname)
// {
// 	int			i;
// 
// 	if (!forValidator)
// 	{
// 		/* normal case, pass to standard routine */
// 		if (!resolve_polymorphic_argtypes(numargs, argtypes, argmodes,
// 										  call_expr))
// 			ereport(ERROR,
// 					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
// 					 errmsg("could not determine actual argument "
// 							"type for polymorphic function \"%s\"",
// 							proname)));
// 	}
// 	else
// 	{
// 		/* special validation case */
// 		for (i = 0; i < numargs; i++)
// 		{
// 			switch (argtypes[i])
// 			{
// 				case ANYELEMENTOID:
// 				case ANYNONARRAYOID:
// 				case ANYENUMOID:		/* XXX dubious */
// 					argtypes[i] = INT4OID;
// 					break;
// 				case ANYARRAYOID:
// 					argtypes[i] = INT4ARRAYOID;
// 					break;
// #ifdef ANYRANGEOID
// 				case ANYRANGEOID:
// 					argtypes[i] = INT4RANGEOID;
// 					break;
// #endif
// 				default:
// 					break;
// 			}
// 		}
// 	}
// }
