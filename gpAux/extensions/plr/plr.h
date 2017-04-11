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
 * plr.h
 */
#ifndef PLR_H
#define PLR_H

#define PLR_VERSION		"08.03.00.15"

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#if PG_VERSION_NUM >= 80400
#include "windowapi.h"
#endif
#include "access/heapam.h"
#if PG_VERSION_NUM >= 90300
#include "access/htup_details.h"
#else
#include "access/htup.h"
#endif
#include "catalog/catversion.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_type.h"
#include "storage/ipc.h"
#include "tcop/tcopprot.h"
#include "utils/array.h"
#include "utils/builtins.h"
#if PG_VERSION_NUM >= 80500
#include "utils/bytea.h"
#endif
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/stat.h>

/*
 * The R headers define various symbols that are also defined by the
 * Postgres headers, so undef them first to avoid conflicts.
 */
#ifdef ERROR
#undef ERROR
#endif

#ifdef WARNING
#undef WARNING
#endif

#include "R.h"
#include "Rversion.h"

/*
 * R version is calculated thus:
 *   Maj * 65536 + Minor * 256 + Build * 1
 * So:
 * version 1.8.0 results in:
 *   (1 * 65536) + (8 * 256) + (0 * 1) == 67584
 * version 1.9.0 results in:
 *   (1 * 65536) + (9 * 256) + (0 * 1) == 67840
 */
#if (R_VERSION >= 132096) /* R_VERSION >= 2.4.0 */
#include "Rembedded.h"
#endif
#if !defined(WIN32) && !defined(WIN64)
#include "Rinterface.h"
#else
extern int R_SignalHandlers;
#endif
#include "Rinternals.h"
#include "Rdefines.h"
#if (R_VERSION < 133120) /* R_VERSION < 2.8.0 */
#include "Rdevices.h"
#endif

/* Restore the Postgres headers */

#ifdef ERROR
#undef ERROR
#endif

#ifdef WARNING
#undef WARNING
#endif

#define WARNING		19
#define ERROR		20

/* starting in R-2.7.0 this defn was removed from Rdevices.h */
#ifndef KillAllDevices
#define KillAllDevices					Rf_KillAllDevices
#endif

/* for some reason this is not in any R header files, it is locally defined */
#define INTEGER_ELT(x,__i__)    INTEGER(x)[__i__]

#ifndef R_HOME_DEFAULT
#define R_HOME_DEFAULT ""
#endif

/* working with postgres 7.3 compatible sources */
#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 80200
#error "This version of PL/R only builds with PostgreSQL 8.2 or later"
#elif PG_VERSION_NUM < 80300
#define PG_VERSION_82_COMPAT
#elif PG_VERSION_NUM < 80400
#define PG_VERSION_83_COMPAT
#else
#define PG_VERSION_84_COMPAT
#endif

#ifdef PG_VERSION_84_COMPAT
#define HAVE_WINDOW_FUNCTIONS
#endif

#ifdef DEBUGPROTECT
#undef PROTECT
extern SEXP pg_protect(SEXP s, char *fn, int ln);
#define PROTECT(s)		pg_protect(s, __FILE__, __LINE__)

#undef UNPROTECT
extern void pg_unprotect(int n, char *fn, int ln);
#define UNPROTECT(n)	pg_unprotect(n, __FILE__, __LINE__)
#endif /* DEBUGPROTECT */

#define xpfree(var_) \
	do { \
		if (var_ != NULL) \
		{ \
			pfree(var_); \
			var_ = NULL; \
		} \
	} while (0)

#define freeStringInfo(mystr_) \
	do { \
		xpfree((mystr_)->data); \
		xpfree(mystr_); \
	} while (0)

#define NEXT_STR_ELEMENT	" %s"

#if (R_VERSION < 67840) /* R_VERSION < 1.9.0 */
#define SET_COLUMN_NAMES \
	do { \
		int i; \
		char *names_buf; \
		names_buf = SPI_fname(tupdesc, j + 1); \
		for (i = 0; i < strlen(names_buf); i++) { \
			if (names_buf[i] == '_') \
				names_buf[i] = '.'; \
		} \
		SET_STRING_ELT(names, df_colnum, mkChar(names_buf)); \
		pfree(names_buf); \
	} while (0)
#else /* R_VERSION >= 1.9.0 */
#define SET_COLUMN_NAMES \
	do { \
		char *names_buf; \
		names_buf = SPI_fname(tupdesc, j + 1); \
		SET_STRING_ELT(names, df_colnum, mkChar(names_buf)); \
		pfree(names_buf); \
	} while (0)
#endif

#if (R_VERSION < 67584) /* R_VERSION < 1.8.0 */
/*
 * See the non-exported header file ${R_HOME}/src/include/Parse.h
 */
extern SEXP R_ParseVector(SEXP, int, int *);
#define PARSE_NULL			0
#define PARSE_OK			1
#define PARSE_INCOMPLETE	2
#define PARSE_ERROR			3
#define PARSE_EOF			4

#define R_PARSEVECTOR(a_, b_, c_)		R_ParseVector(a_, b_, c_)

/*
 * See the non-exported header file ${R_HOME}/src/include/Defn.h
 */
extern void R_PreserveObject(SEXP);
extern void R_ReleaseObject(SEXP);

/* in main.c */
extern void R_dot_Last(void);

/* in memory.c */
extern void R_RunExitFinalizers(void);

#else /* R_VERSION >= 1.8.0 */

#include "R_ext/Parse.h"

#if (R_VERSION >= 132352) /* R_VERSION >= 2.5.0 */
#define R_PARSEVECTOR(a_, b_, c_)		R_ParseVector(a_, b_, (ParseStatus *) c_, R_NilValue)
#else /* R_VERSION < 2.5.0 */
#define R_PARSEVECTOR(a_, b_, c_)		R_ParseVector(a_, b_, (ParseStatus *) c_)
#endif /* R_VERSION >= 2.5.0 */
#endif /* R_VERSION >= 1.8.0 */

/* convert C string to text pointer */
#define PG_TEXT_GET_STR(textp_) \
	DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp_)))
#define PG_STR_GET_TEXT(str_) \
	DatumGetTextP(DirectFunctionCall1(textin, CStringGetDatum(str_)))
#define PG_REPLACE_STR(str_, substr_, replacestr_) \
	PG_TEXT_GET_STR(DirectFunctionCall3(replace_text, \
										PG_STR_GET_TEXT(str_), \
										PG_STR_GET_TEXT(substr_), \
										PG_STR_GET_TEXT(replacestr_)))

/* initial number of hash table entries for compiled functions */
#define FUNCS_PER_USER		64

#define ERRORCONTEXTCALLBACK \
	ErrorContextCallback	plerrcontext

#define PUSH_PLERRCONTEXT(_error_callback_, _plr_error_funcname_) \
	do { \
		plerrcontext.callback = _error_callback_; \
		plerrcontext.arg = (void *) pstrdup(_plr_error_funcname_); \
		plerrcontext.previous = error_context_stack; \
		error_context_stack = &plerrcontext; \
	} while (0)

#define POP_PLERRCONTEXT \
	do { \
		pfree(plerrcontext.arg); \
		error_context_stack = plerrcontext.previous; \
	} while (0)

#define SAVE_PLERRCONTEXT \
	ErrorContextCallback *ecs_save; \
	do { \
		ecs_save = error_context_stack; \
		error_context_stack = NULL; \
	} while (0)

#define RESTORE_PLERRCONTEXT \
	do { \
		error_context_stack = ecs_save; \
	} while (0)

#ifndef TEXTARRAYOID
#define TEXTARRAYOID	1009
#endif

#define TRIGGER_NARGS	9

#define TUPLESTORE_BEGIN_HEAP	tuplestore_begin_heap(true, false, work_mem)

#define INIT_AUX_FMGR_ATTS \
	do { \
		finfo->fn_mcxt = plr_caller_context; \
		finfo->fn_expr = (Node *) NULL; \
	} while (0)

#define PROARGTYPES(i) \
		procStruct->proargtypes.values[i]
#define FUNCARGTYPES(_tup_) \
		((Form_pg_proc) GETSTRUCT(_tup_))->proargtypes.values

#define  PLR_CLEANUP \
	plr_cleanup(int code, Datum arg)
#define TRIGGERTUPLEVARS \
	HeapTuple		tup; \
	HeapTupleHeader	dnewtup; \
	HeapTupleHeader	dtrigtup
#define SET_INSERT_ARGS_567 \
	do { \
		arg[5] = DirectFunctionCall1(textin, CStringGetDatum("INSERT")); \
		tup = trigdata->tg_trigtuple; \
		dtrigtup = (HeapTupleHeader) palloc(tup->t_len); \
		memcpy((char *) dtrigtup, (char *) tup->t_data, tup->t_len); \
		HeapTupleHeaderSetDatumLength(dtrigtup, tup->t_len); \
		HeapTupleHeaderSetTypeId(dtrigtup, tupdesc->tdtypeid); \
		HeapTupleHeaderSetTypMod(dtrigtup, tupdesc->tdtypmod); \
		arg[6] = PointerGetDatum(dtrigtup); \
		argnull[6] = false; \
		arg[7] = (Datum) 0; \
		argnull[7] = true; \
	} while (0)
#define SET_DELETE_ARGS_567 \
	do { \
		arg[5] = DirectFunctionCall1(textin, CStringGetDatum("DELETE")); \
		arg[6] = (Datum) 0; \
		argnull[6] = true; \
		tup = trigdata->tg_trigtuple; \
		dtrigtup = (HeapTupleHeader) palloc(tup->t_len); \
		memcpy((char *) dtrigtup, (char *) tup->t_data, tup->t_len); \
		HeapTupleHeaderSetDatumLength(dtrigtup, tup->t_len); \
		HeapTupleHeaderSetTypeId(dtrigtup, tupdesc->tdtypeid); \
		HeapTupleHeaderSetTypMod(dtrigtup, tupdesc->tdtypmod); \
		arg[7] = PointerGetDatum(dtrigtup); \
		argnull[7] = false; \
	} while (0)
#define SET_UPDATE_ARGS_567 \
	do { \
		arg[5] = DirectFunctionCall1(textin, CStringGetDatum("UPDATE")); \
		tup = trigdata->tg_newtuple; \
		dnewtup = (HeapTupleHeader) palloc(tup->t_len); \
		memcpy((char *) dnewtup, (char *) tup->t_data, tup->t_len); \
		HeapTupleHeaderSetDatumLength(dnewtup, tup->t_len); \
		HeapTupleHeaderSetTypeId(dnewtup, tupdesc->tdtypeid); \
		HeapTupleHeaderSetTypMod(dnewtup, tupdesc->tdtypmod); \
		arg[6] = PointerGetDatum(dnewtup); \
		argnull[6] = false; \
		tup = trigdata->tg_trigtuple; \
		dtrigtup = (HeapTupleHeader) palloc(tup->t_len); \
		memcpy((char *) dtrigtup, (char *) tup->t_data, tup->t_len); \
		HeapTupleHeaderSetDatumLength(dtrigtup, tup->t_len); \
		HeapTupleHeaderSetTypeId(dtrigtup, tupdesc->tdtypeid); \
		HeapTupleHeaderSetTypMod(dtrigtup, tupdesc->tdtypmod); \
		arg[7] = PointerGetDatum(dtrigtup); \
		argnull[7] = false; \
	} while (0)
#define CONVERT_TUPLE_TO_DATAFRAME \
	do { \
		Oid			tupType; \
		int32		tupTypmod; \
		TupleDesc	tupdesc; \
		HeapTuple	tuple = palloc(sizeof(HeapTupleData)); \
		HeapTupleHeader	tuple_hdr = DatumGetHeapTupleHeader(arg[i]); \
		tupType = HeapTupleHeaderGetTypeId(tuple_hdr); \
		tupTypmod = HeapTupleHeaderGetTypMod(tuple_hdr); \
		tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod); \
		tuple->t_len = HeapTupleHeaderGetDatumLength(tuple_hdr); \
		ItemPointerSetInvalid(&(tuple->t_self)); \
		/* tuple->t_tableOid = InvalidOid; */\
		tuple->t_data = tuple_hdr; \
		PROTECT(el = pg_tuple_get_r_frame(1, &tuple, tupdesc)); \
		ReleaseTupleDesc(tupdesc); \
		pfree(tuple); \
	} while (0)
#define GET_ARG_NAMES \
		char  **argnames; \
		argnames = fetchArgNames(procTup, procStruct->pronargs)
#define SET_ARG_NAME \
	do { \
		if (argnames && argnames[i] && argnames[i][0]) \
		{ \
			appendStringInfo(proc_internal_args, "%s", argnames[i]); \
			pfree(argnames[i]); \
		} \
		else \
			appendStringInfo(proc_internal_args, "arg%d", i + 1); \
	} while (0)
#define SET_FRAME_ARG_NAME \
	do { \
		appendStringInfo(proc_internal_args, "farg%d", i + 1); \
	} while (0)
#define SET_FRAME_XARG_NAMES \
	do { \
		appendStringInfo(proc_internal_args, ",fnumrows,prownum"); \
	} while (0)
#define FREE_ARG_NAMES \
	do { \
		if (argnames) \
			pfree(argnames); \
	} while (0)
#define PREPARE_PG_TRY \
	ERRORCONTEXTCALLBACK
#define SWITCHTO_PLR_SPI_CONTEXT(the_caller_context) \
	the_caller_context = MemoryContextSwitchTo(plr_SPI_context)
#define CLEANUP_PLR_SPI_CONTEXT(the_caller_context) \
	MemoryContextSwitchTo(the_caller_context)
#define PLR_PG_CATCH() \
		PG_CATCH(); \
		{ \
			MemoryContext temp_context; \
			ErrorData  *edata; \
			SWITCHTO_PLR_SPI_CONTEXT(temp_context); \
			edata = CopyErrorData(); \
			MemoryContextSwitchTo(temp_context); \
			error("error in SQL statement : %s", edata->message); \
		}
#define PLR_PG_END_TRY() \
	PG_END_TRY()

/*
 * structs
 */

typedef struct plr_func_hashkey
{								/* Hash lookup key for functions */
	Oid		funcOid;

	/*
	 * For a trigger function, the OID of the relation triggered on is part
	 * of the hashkey --- we want to compile the trigger separately for each
	 * relation it is used with, in case the rowtype is different.  Zero if
	 * not called as a trigger.
	 */
	Oid			trigrelOid;

	/*
	 * We include actual argument types in the hash key to support
	 * polymorphic PLpgSQL functions.  Be careful that extra positions
	 * are zeroed!
	 */
	Oid		argtypes[FUNC_MAX_ARGS];
} plr_func_hashkey;


/* The information we cache about loaded procedures */
typedef struct plr_function
{
	char			   *proname;
	TransactionId		fn_xmin;
	ItemPointerData		fn_tid;
	plr_func_hashkey   *fn_hashkey; /* back-link to hashtable key */
	bool				lanpltrusted;
	Oid					result_typid;
	bool				result_istuple;
	FmgrInfo			result_in_func;
	Oid					result_elem;
	FmgrInfo			result_elem_in_func;
	int					result_elem_typlen;
	bool				result_elem_typbyval;
	char				result_elem_typalign;
	int					result_natts;
	Oid				   *result_fld_elem_typid;
	FmgrInfo		   *result_fld_elem_in_func;
	int				   *result_fld_elem_typlen;
	bool			   *result_fld_elem_typbyval;
	char			   *result_fld_elem_typalign;
	int					nargs;
	Oid					arg_typid[FUNC_MAX_ARGS];
	bool				arg_typbyval[FUNC_MAX_ARGS];
	FmgrInfo			arg_out_func[FUNC_MAX_ARGS];
	Oid					arg_elem[FUNC_MAX_ARGS];
	FmgrInfo			arg_elem_out_func[FUNC_MAX_ARGS];
	int					arg_elem_typlen[FUNC_MAX_ARGS];
	bool				arg_elem_typbyval[FUNC_MAX_ARGS];
	char				arg_elem_typalign[FUNC_MAX_ARGS];
	int					arg_is_rel[FUNC_MAX_ARGS];
	SEXP				fun;	/* compiled R function */
#ifdef HAVE_WINDOW_FUNCTIONS
	bool				iswindow;
#endif
}	plr_function;

/* compiled function hash table */
typedef struct plr_hashent
{
	plr_func_hashkey key;
	plr_function   *function;
} plr_HashEnt;

/*
 * external declarations
 */

/* libR interpreter initialization */
extern int Rf_initEmbeddedR(int argc, char **argv);

/* PL/R language handler */
extern Datum plr_call_handler(PG_FUNCTION_ARGS);
extern void PLR_CLEANUP;
extern void plr_init(void);
extern void plr_load_modules(void);
extern void load_r_cmd(const char *cmd);
extern SEXP call_r_func(SEXP fun, SEXP rargs);

/* argument and return value conversion functions */
extern SEXP pg_scalar_get_r(Datum dvalue, Oid arg_typid, FmgrInfo arg_out_func);
extern SEXP pg_array_get_r(Datum dvalue, FmgrInfo out_func, int typlen, bool typbyval, char typalign);
extern SEXP pg_datum_array_get_r(Datum *elem_values, bool *elem_nulls, int numels, bool has_nulls,
								 Oid element_type, FmgrInfo out_func, bool typbyval);
extern SEXP pg_tuple_get_r_frame(int ntuples, HeapTuple *tuples, TupleDesc tupdesc);
extern Datum r_get_pg(SEXP rval, plr_function *function, FunctionCallInfo fcinfo);
extern Datum get_datum(SEXP rval, Oid typid, Oid typelem, FmgrInfo in_func, bool *isnull);
extern Datum get_scalar_datum(SEXP rval, Oid result_typ, FmgrInfo result_in_func, bool *isnull);

/* Postgres support functions installed into the R interpreter */
extern void throw_pg_notice(const char **msg);
extern SEXP plr_quote_literal(SEXP rawstr);
extern SEXP plr_quote_ident(SEXP rawstr);
extern SEXP plr_SPI_exec(SEXP rsql);
extern SEXP plr_SPI_prepare(SEXP rsql, SEXP rargtypes);
extern SEXP plr_SPI_execp(SEXP rsaved_plan, SEXP rargvalues);
extern SEXP plr_SPI_cursor_open(SEXP cursor_name_arg,SEXP rsaved_plan, SEXP rargvalues);
extern SEXP plr_SPI_cursor_fetch(SEXP cursor_in,SEXP forward_in, SEXP rows_in);
extern void plr_SPI_cursor_close(SEXP cursor_in);
extern void plr_SPI_cursor_move(SEXP cursor_in, SEXP forward_in, SEXP rows_in);
extern SEXP plr_SPI_lastoid(void);
extern void throw_r_error(const char **msg);

/* Postgres callable functions useful in conjunction with PL/R */
extern Datum plr_version(PG_FUNCTION_ARGS);
extern Datum reload_plr_modules(PG_FUNCTION_ARGS);
extern Datum install_rcmd(PG_FUNCTION_ARGS);
extern Datum plr_array_push(PG_FUNCTION_ARGS);
extern Datum plr_array(PG_FUNCTION_ARGS);
extern Datum plr_array_accum(PG_FUNCTION_ARGS);
extern Datum plr_environ(PG_FUNCTION_ARGS);
extern Datum plr_set_rhome(PG_FUNCTION_ARGS);
extern Datum plr_unset_rhome(PG_FUNCTION_ARGS);
extern Datum plr_set_display(PG_FUNCTION_ARGS);
extern Datum plr_get_raw(PG_FUNCTION_ARGS);

/* Postgres backend support functions */
extern void compute_function_hashkey(FunctionCallInfo fcinfo,
									 Form_pg_proc procStruct,
									 plr_func_hashkey *hashkey);
extern void plr_HashTableInit(void);
extern plr_function *plr_HashTableLookup(plr_func_hashkey *func_key);
extern void plr_HashTableInsert(plr_function *function,
								plr_func_hashkey *func_key);
extern void plr_HashTableDelete(plr_function *function);
extern char *get_load_self_ref_cmd(Oid funcid);
extern void perm_fmgr_info(Oid functionId, FmgrInfo *finfo);

#endif   /* PLR_H */
