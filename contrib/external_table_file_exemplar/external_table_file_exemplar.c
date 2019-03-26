/*-------------------------------------------------------------------------
 *
 * external_table_file_exemplar.c
 *		  foreign-data wrapper for external table flat files.
 *
 * Copyright (c)
 *
 * IDENTIFICATION
 *		  contrib/external_table_file_exemplar/external_table_file_exemplar.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "catalog/pg_foreign_table.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "commands/vacuum.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/var.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/* LookupFuncName */
#include "parser/parse_func.h"

/* get_func_rettype */
#include "utils/lsyscache.h"

/* PROVOLATILE_STABLE */
#include "catalog/pg_proc.h"

/* FormatterData */
#include "access/relscan.h"

/* url family of functions */
#include "access/url.h"

/* Single Row Error Handling */
#include "cdb/cdbsreh.h"

PG_MODULE_MAGIC;

/*
 * Describes the valid options for objects that use this wrapper.
 */
struct FileFdwOption
{
	const char *optname;
	Oid			optcontext;		/* Oid of catalog in which option may appear */
};

/*
 * Valid options for file_fdw.
 * These options are based on the options for the COPY FROM command.
 * But note that force_not_null and force_null are handled as boolean options
 * attached to a column, not as table options.
 *
 * Note: If you are adding new option for user mapping, you need to modify
 * fileGetOptions(), which currently doesn't bother to look at user mappings.
 */
static const struct FileFdwOption valid_options[] = {
	/* File options */
	{"filename", ForeignTableRelationId},

	/* Format options */
	/* oids option is not supported */
	{"format", ForeignTableRelationId},

	/* Custom format options */
	{"formatter", ForeignTableRelationId},

	/* Error handling */
	{"reject_limit", ForeignTableRelationId},
	{"reject_limit_kind", ForeignTableRelationId},

	/*
	 * based on the non custom options
	 */
	{"header", ForeignTableRelationId},
	{"delimiter", ForeignTableRelationId},
	{"quote", ForeignTableRelationId},
	{"escape", ForeignTableRelationId},
	{"null", ForeignTableRelationId},
	{"encoding", ForeignTableRelationId},
	{"force_not_null", AttributeRelationId},
	{"force_null", AttributeRelationId},

	/*
	 * force_quote is not supported yet until we can write to the files
	 */

	/* Sentinel */
	{NULL, InvalidOid}
};

/*
 * FDW-specific information for RelOptInfo.fdw_private.
 */
typedef struct FileFdwPlanState
{
	char	   *filename;		/* file to read */
	List	   *options;		/* merged options */
	BlockNumber pages;			/* estimate of file's physical size */
	double		ntuples;		/* estimate of number of rows in file */
} FileFdwPlanState;

/*
 * FDW-specific information for ForeignScanState.fdw_state.
 */
typedef struct FileFdwExecutionState
{
	char	   *filename;		/* file to read */
	List	   *options;		/* merged COPY options, excluding filename */
	CopyState	cstate;			/* state of reading file */

	bool		iscustom;		/* is custom formatter */

	/*
	 * custom data formatter and gpfdist file specific info
	 */
	struct custom
	{
		FmgrInfo   *fs_custom_formatter_func;	/* function to convert to
												 * custom format */
		List	   *fs_custom_formatter_params; /* list of defelems that hold
												 * user's format parameters */
		FormatterData *fs_formatter;

		Relation	fs_rd;		/* target relation descriptor */
		FmgrInfo   *in_functions;
		Oid		   *typioparams;

		TupleDesc	fs_tupDesc;

		bool		raw_buf_done;
		URL_FILE   *url_file;
		/* Single Row Error Handling */
		int			reject_limit;
		bool		is_reject_limit_rows;
	}			custom;
} FileFdwExecutionState;

/*
 * SQL functions
 */
PG_FUNCTION_INFO_V1(external_table_file_handler);
PG_FUNCTION_INFO_V1(external_table_file_validator);

/*
 * FDW callback routines
 */
static void fileGetForeignRelSize(PlannerInfo *root,
					  RelOptInfo *baserel,
					  Oid foreigntableid);
static void fileGetForeignPaths(PlannerInfo *root,
					RelOptInfo *baserel,
					Oid foreigntableid);
static ForeignScan *fileGetForeignPlan(PlannerInfo *root,
				   RelOptInfo *baserel,
				   Oid foreigntableid,
				   ForeignPath *best_path,
				   List *tlist,
				   List *scan_clauses);
static void fileExplainForeignScan(ForeignScanState *node, ExplainState *es);
static void fileBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *fileIterateForeignScan(ForeignScanState *node);
static void fileReScanForeignScan(ForeignScanState *node);
static void fileEndForeignScan(ForeignScanState *node);
static bool fileAnalyzeForeignTable(Relation relation,
						AcquireSampleRowsFunc *func,
						BlockNumber *totalpages);

/*
 * Helper functions
 */
static bool is_valid_option(const char *option, Oid context);
static void fileGetOptions(Oid foreigntableid,
			   char **filename, List **other_options);
static List *get_file_fdw_attribute_options(Oid relid);
static bool check_selective_binary_conversion(RelOptInfo *baserel,
								  Oid foreigntableid,
								  List **columns);
static void estimate_size(PlannerInfo *root, RelOptInfo *baserel,
			  FileFdwPlanState *fdw_private);
static void estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
			   FileFdwPlanState *fdw_private,
			   Cost *startup_cost, Cost *total_cost);
static int file_acquire_sample_rows(Relation onerel, int elevel,
						 HeapTuple *rows, int targrows,
						 double *totalrows, double *totaldeadrows);
static void parseCustomFormatString(const char *fmtstr,
						char **formatter_name,
						List **formatter_params);

static int	dummy(void *outbuf, int datasize, void *cb_args);

static Oid lookupCustomFormatter(char *formatter_name,
					  bool iswritable);

static HeapTuple dirrect_call(FileFdwExecutionState *festate);

static void FunctionCallPrepareFormatter(FunctionCallInfoData *fcinfo,
							 int nArgs,
							 CopyState pstate,
							 FmgrInfo *formatter_func,
							 List *formatter_params,
							 FormatterData *formatter,
							 Relation rel,
							 TupleDesc tupDesc,
							 FmgrInfo *convFuncs,
							 Oid *typioparams);

static void justifyDatabuf(StringInfo buf);

/*
 * A data error happened. This code block will always be inside a PG_CATCH()
 * block right when a higher stack level produced an error. We handle the error
 * by checking which error mode is set (SREH or all-or-nothing) and do the right
 * thing accordingly. Note that we MUST have this code in a macro (as opposed
 * to a function) as elog_dismiss() has to be inlined with PG_CATCH in order to
 * access local error state variables.
 */
#define HANDLE_ERROR \
if (festate->cstate->errMode == ALL_OR_NOTHING) \
{ \
	/* re-throw error and abort */ \
	PG_RE_THROW(); \
} \
else \
{ \
	/* SREH - release error state */ \
\
	ErrorData	*edata; \
	MemoryContext oldcontext;\
\
	/* SREH must only handle data errors. all other errors must not be caught */\
	if(ERRCODE_TO_CATEGORY(elog_geterrcode()) != ERRCODE_DATA_EXCEPTION)\
	{\
		PG_RE_THROW(); \
	}\
\
	/* save a copy of the error info */ \
	oldcontext = MemoryContextSwitchTo(festate->cstate->cdbsreh->badrowcontext);\
	edata = CopyErrorData();\
	MemoryContextSwitchTo(oldcontext);\
\
	if (!elog_dismiss(DEBUG5)) \
		PG_RE_THROW(); /* <-- hope to never get here! */ \
\
	truncateEol(&festate->cstate->line_buf, festate->cstate->eol_type); \
	festate->cstate->cdbsreh->rawdata = festate->cstate->line_buf.data; \
	festate->cstate->cdbsreh->is_server_enc = festate->cstate->line_buf_converted; \
	festate->cstate->cdbsreh->linenumber = festate->cstate->cur_lineno; \
	festate->cstate->cdbsreh->processed++; \
\
	/* set the error message. Use original msg and add column name if available */ \
	if (festate->cstate->cur_attname)\
	{\
		festate->cstate->cdbsreh->errmsg = psprintf("%s, column %s", \
				edata->message, \
				festate->cstate->cur_attname); \
	}\
	else\
	{\
		festate->cstate->cdbsreh->errmsg = pstrdup(edata->message); \
	}\
\
	HandleSingleRowError(festate->cstate->cdbsreh); \
	FreeErrorData(edata);\
	if (!IsRejectLimitReached(festate->cstate->cdbsreh)) \
		pfree(festate->cstate->cdbsreh->errmsg); \
}

/*
 * Foreign-data wrapper handler function: return a struct with pointers
 * to my callback routines.
 */
Datum
external_table_file_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);

	fdwroutine->GetForeignRelSize = fileGetForeignRelSize;
	fdwroutine->GetForeignPaths = fileGetForeignPaths;
	fdwroutine->GetForeignPlan = fileGetForeignPlan;
	fdwroutine->ExplainForeignScan = fileExplainForeignScan;
	fdwroutine->BeginForeignScan = fileBeginForeignScan;
	fdwroutine->IterateForeignScan = fileIterateForeignScan;
	fdwroutine->ReScanForeignScan = fileReScanForeignScan;
	fdwroutine->EndForeignScan = fileEndForeignScan;
	fdwroutine->AnalyzeForeignTable = fileAnalyzeForeignTable;

	/*
	 * insert / update functions are not defined yet check
	 * src/include/foreign/fdwapi.h for definitions
	 */

	PG_RETURN_POINTER(fdwroutine);
}

/*
 * Validate the generic options given to a FOREIGN DATA WRAPPER, SERVER,
 * USER MAPPING or FOREIGN TABLE that uses file_fdw.
 *
 * Raise an ERROR if the option or its value is considered invalid.
 *
 */
Datum
external_table_file_validator(PG_FUNCTION_ARGS)
{
	List	   *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);
	char	   *filename = NULL;
	char	   *format = NULL;
	char	   *formatter = NULL;
	DefElem    *force_not_null = NULL;
	DefElem    *force_null = NULL;
	List	   *other_options = NIL;
	ListCell   *cell;

	/*
	 * Only superusers are allowed to set options of a file_fdw foreign table.
	 * This is because the filename is one of those options, and we don't want
	 * non-superusers to be able to determine which file gets read.
	 *
	 * Putting this sort of permissions check in a validator is a bit of a
	 * crock, but there doesn't seem to be any other place that can enforce
	 * the check more cleanly.
	 *
	 * Note that the valid_options[] array disallows setting filename at any
	 * options level other than foreign table --- otherwise there'd still be a
	 * security hole.
	 */
	Assert(isTransactionState());
	if (catalog == ForeignTableRelationId && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("only superuser can change options of a file_fdw foreign table")));

	/*
	 * Check that only options supported by the external_table_file_exemplar
	 * and allowed for the current object type, are given.
	 */
	foreach(cell, options_list)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		if (!is_valid_option(def->defname, catalog))
		{
			const struct FileFdwOption *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = valid_options; opt->optname; opt++)
			{
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
									 opt->optname);
			}

			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					 errmsg("invalid option \"%s\"", def->defname),
					 buf.len > 0
					 ? errhint("Valid options in this context are: %s",
							   buf.data)
					 : errhint("There are no valid options in this context.")));
		}

		/*
		 * Separate out filename and column-specific options, since
		 * ProcessCopyOptions won't accept them.
		 */
		if (strcmp(def->defname, "filename") == 0)
		{
			if (filename)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			filename = defGetString(def);
		}

		/*
		 * force_not_null is a boolean option; after validation we can discard
		 * it - it will be retrieved later in get_file_fdw_attribute_options()
		 */
		else if (strcmp(def->defname, "force_not_null") == 0)
		{
			if (force_not_null)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("Option \"force_not_null\" supplied more than once for a column.")));
			force_not_null = def;
			/* Don't care what the value is, as long as it's a legal boolean */
			(void) defGetBoolean(def);
		}
		/* See comments for force_not_null above */
		else if (strcmp(def->defname, "force_null") == 0)
		{
			if (force_null)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("Option \"force_null\" supplied more than once for a column.")));
			force_null = def;
			(void) defGetBoolean(def);
		}

		/*
		 * Get the format options, this is basic validation for now
		 */
		else if (strcmp(def->defname, "format") == 0)
		{
			if (format)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("Option \"format\" supplied more than once for a relation.")));

			/*
			 * add format to list in case it needs to be parsed by the core
			 * COPY's validator
			 */
			other_options = lappend(other_options, def);
			format = defGetString(def);
		}
		else if (strcmp(def->defname, "formatter") == 0)
		{
			if (formatter)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 errhint("Option \"formatter\" supplied more than once for a relation.")));
			formatter = defGetString(def);
		}
		else
			other_options = lappend(other_options, def);
	}

	/*
	 * Only allow custom formatter when format is custom. Otherwise use the
	 * core COPY code's validation logic for more checks.
	 */
	if (format)
	{
		if (strcmp(format, "custom") == 0 && !formatter)
			ereport(ERROR,
					(errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
					 errmsg("formatter is required for custom format foreign tables")));
		else if (strcmp(format, "custom") != 0 && formatter)
			ereport(ERROR,
					(errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
					 errmsg("formatter is only used on custom format foreign tables")));
	}

	if (!format || strcmp(format, "custom") != 0)
		ProcessCopyOptions(NULL, true, other_options, 0, true);

	/*
	 * Filename option is required for external table file foreign tables.
	 */
	if (catalog == ForeignTableRelationId && filename == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
				 errmsg("filename is required for external table file foreign tables")));

	PG_RETURN_VOID();
}

/*
 * justifyDatabuf
 *
 * shift all data remaining in the buffer (anything from cursor to
 * end of buffer) to the beginning of the buffer, and readjust the
 * cursor and length to the new end of buffer position.
 *
 * 3 possible cases:
 *	 1 - cursor at beginning of buffer (whole buffer is a partial row) - nothing to do.
 *	 2 - cursor at end of buffer (last row ended in the last byte of the buffer)
 *	 3 - cursor at middle of buffer (remaining bytes are a partial row)
 */
static void
justifyDatabuf(StringInfo buf)
{
	/* 1 */
	if (buf->cursor == 0)
	{
		/* nothing to do */
	}
	/* 2 */
	else if (buf->cursor == buf->len)
	{
		Assert(buf->data[buf->cursor] == '\0');
		resetStringInfo(buf);
	}
	/* 3 */
	else
	{
		char	   *position = buf->data + buf->cursor;
		int			remaining = buf->len - buf->cursor;

		/* slide data back (data may overlap so use memmove not memcpy) */
		memmove(buf->data, position, remaining);

		buf->len = remaining;

		/* be consistent with appendBinaryStringInfo() */
		buf->data[buf->len] = '\0';
	}

	buf->cursor = 0;
}


/*
 * fileGetForeignRelSize
 *		Obtain relation size estimates for a foreign table
 */
static void
fileGetForeignRelSize(PlannerInfo *root,
					  RelOptInfo *baserel,
					  Oid foreigntableid)
{
	FileFdwPlanState *fdw_private;

	/*
	 * Fetch options.  We only need filename at this point, but we might as
	 * well get everything and not need to re-fetch it later in planning.
	 */
	fdw_private = (FileFdwPlanState *) palloc(sizeof(FileFdwPlanState));
	fileGetOptions(foreigntableid, &fdw_private->filename, &fdw_private->options);
	baserel->fdw_private = (void *) fdw_private;

	/* Estimate relation size */
	estimate_size(root, baserel, fdw_private);
}

/*
 * Dummy function to pass in BeginCopyFrom
 * in the case of custom format. We want Copy to
 * set up our copy state, but not to try to stat
 * the file
 */
static int
dummy(void *outbuf, int datasize, void *cb_args)
{
	return 0;
}

/*
 * Prepare the formatter data to be used inside the formatting UDF.
 * This function should be called every time before invoking the
 * UDF, for both insert and scan operations. Even though there's some
 * redundancy here, it is needed in order to reset some per-call state
 * that should be examined by the caller upon return from the UDF.
 *
 * Also, set up the function call context.
 */
static void
FunctionCallPrepareFormatter(FunctionCallInfoData *fcinfo,
							 int nArgs,
							 CopyState pstate,
							 FmgrInfo *formatter_func,
							 List *formatter_params,
							 FormatterData *formatter,
							 Relation rel,
							 TupleDesc tupDesc,
							 FmgrInfo *convFuncs,
							 Oid *typioparams)
{
	formatter->type = T_FormatterData;
	formatter->fmt_relation = rel;
	formatter->fmt_tupDesc = tupDesc;
	formatter->fmt_notification = FMT_NONE;
	formatter->fmt_badrow_len = 0;
	formatter->fmt_badrow_num = 0;
	formatter->fmt_args = formatter_params;
	formatter->fmt_conv_funcs = convFuncs;
	formatter->fmt_saw_eof = pstate->fe_eof;
	formatter->fmt_typioparams = typioparams;
	formatter->fmt_perrow_ctx = pstate->rowcontext;
	formatter->fmt_needs_transcoding = pstate->need_transcoding;
	formatter->fmt_conversion_proc = pstate->enc_conversion_proc;
	formatter->fmt_external_encoding = pstate->file_encoding;

	InitFunctionCallInfoData( /* FunctionCallInfoData */ *fcinfo,
							  /* FmgrInfo */ formatter_func,
							  /* nArgs */ nArgs,
							  /* collation */ InvalidOid,
							  /* Call Context */ (Node *) formatter,
							  /* ResultSetInfo */ NULL);
}

/*
 * Read from custom file format
 */
static HeapTuple
dirrect_call(FileFdwExecutionState *festate)
{
	FormatterData *formatter;
	MemoryContext oldcontext;
	int			nbytes;

	formatter = festate->custom.fs_formatter;
	oldcontext = CurrentMemoryContext;
	while (!festate->custom.raw_buf_done || !festate->cstate->fe_eof)
	{
		/* need to fill our buffer with data? */
		if (festate->custom.raw_buf_done)
		{
			nbytes = url_fread(festate->cstate->raw_buf, RAW_BUF_SIZE, festate->custom.url_file, festate->cstate);
			if (url_feof(festate->custom.url_file, nbytes))
				festate->cstate->fe_eof = true;

			if (nbytes <= 0 && url_ferror(festate->custom.url_file, nbytes, NULL, 0))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read from external file: %m")));

			appendBinaryStringInfo(&formatter->fmt_databuf, festate->cstate->raw_buf, nbytes);
			festate->custom.raw_buf_done = false;
		}

		while (!festate->custom.raw_buf_done)
		{
			bool has_error;
			has_error = false;
			PG_TRY();
			{
				/* Now call the custom formatter */
				FunctionCallInfoData fcinfo;

				/* per call formatter prep */
				FunctionCallPrepareFormatter(&fcinfo,
								0,
								festate->cstate,
								festate->custom.fs_custom_formatter_func,
								festate->custom.fs_custom_formatter_params,
								formatter,
								festate->custom.fs_rd,
								festate->custom.fs_tupDesc,
								festate->custom.in_functions,
								festate->custom.typioparams);
				(void) FunctionCallInvoke(&fcinfo);
			}
			PG_CATCH();
			{
				has_error = true;
				MemoryContextSwitchTo(formatter->fmt_perrow_ctx);

				/*
				 * Save any bad row information that was set by the user
				 * in the formatter UDF (if any). Then handle the error.
				 */
				festate->cstate->cur_lineno = formatter->fmt_badrow_num;
				resetStringInfo(&festate->cstate->line_buf);

				if (formatter->fmt_badrow_len > 0)
				{
					if (formatter->fmt_badrow_data)
						appendBinaryStringInfo(&festate->cstate->line_buf, formatter->fmt_badrow_data, formatter->fmt_badrow_len);

					formatter->fmt_databuf.cursor += formatter->fmt_badrow_len;
					if (formatter->fmt_databuf.cursor > formatter->fmt_databuf.len ||
							formatter->fmt_databuf.cursor < 0)
						formatter->fmt_databuf.cursor = formatter->fmt_databuf.len;
				}

				HANDLE_ERROR;
				FlushErrorState();

				MemoryContextSwitchTo(oldcontext);
			}
			PG_END_TRY();

			HeapTuple	tuple;

			if (has_error)
				ErrorIfRejectLimitReached(festate->cstate->cdbsreh);
			else
			{
				switch (formatter->fmt_notification)
				{
					case FMT_NONE:
							/* got a tuple back */
							tuple = formatter->fmt_tuple;
							MemoryContextReset(formatter->fmt_perrow_ctx);

							if (festate->cstate->cdbsreh)
									festate->cstate->cdbsreh->processed++;

							MemoryContextReset(formatter->fmt_perrow_ctx);
							return tuple;
					case FMT_NEED_MORE_DATA:

							/*
							 * Callee consumed all data in the buffer. Prepare to read
							 * more data into it.
							 */
							festate->custom.raw_buf_done = true;
							justifyDatabuf(&formatter->fmt_databuf);
							continue;
					default:
							elog(ERROR, "unsupported formatter notification (%d)",
											formatter->fmt_notification);
							break;
				}
			}
		}
	}

	return NULL;
}

/*
 * setCustomFormatter
 *
 * Exact copy from external/fileam.c
 *
 * Given a formatter name and a function signature (pre-determined
 * by whether it is readable or writable) find such a function in
 * the catalog and store it to be used later.
 *
 * WET function: 1 record arg, return bytea.
 * RET function: 0 args, returns record.
 */
static Oid
lookupCustomFormatter(char *formatter_name, bool iswritable)
{
	List	   *funcname = NIL;
	Oid			procOid = InvalidOid;
	Oid			argList[1];
	Oid			returnOid;

	funcname = lappend(funcname, makeString(formatter_name));

	if (iswritable)
	{
		argList[0] = RECORDOID;
		returnOid = BYTEAOID;
		procOid = LookupFuncName(funcname, 1, argList, true);
	}
	else
	{
		returnOid = RECORDOID;
		procOid = LookupFuncName(funcname, 0, argList, true);
	}

	if (!OidIsValid(procOid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("formatter function \"%s\" of type %s was not found",
						formatter_name,
						(iswritable ? "writable" : "readable")),
				 errhint("Create it with CREATE FUNCTION.")));

	/* check return type matches */
	if (get_func_rettype(procOid) != returnOid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("formatter function \"%s\" of type %s has an incorrect return type",
						formatter_name,
						(iswritable ? "writable" : "readable"))));

	/* check allowed volatility */
	if (func_volatile(procOid) != PROVOLATILE_STABLE)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("formatter function %s is not declared STABLE",
						formatter_name)));

	return procOid;
}

/*
 * Parse the custom format string
 *
 * It is expected to be of the form:
 * 		name [key value]...[key value]
 *
 * Database encoding is ignored at the moment
 */
static void
parseCustomFormatString(const char *fmtstr, char **formatter_name, List **formatter_params)
{
	char	   *to_free;
	char	   *start;
	char	   *token;
	char	   *next;
	char	   *key;
	const char *whitespace = " \t\n\r";
	List	   *l = NIL;
	bool		is_key;

	to_free = start = pstrdup(fmtstr);
	/* Formatter name is first, then key value pairs */
	token = strtok_r(start, whitespace, &next);
	/* First value is always name */
	Assert(token);
	*formatter_name = pstrdup(token);

	key = NULL;
	for (token = strtok_r(NULL, whitespace, &next), is_key = true;
		 token != NULL;
		 token = strtok_r(NULL, whitespace, &next), is_key = !is_key)
	{
		if (is_key)
			key = pstrdup(token);
		else
		{
			l = lappend(l, makeDefElem(pstrdup(key), (Node *) makeString(pstrdup(token))));
			pfree(key);
			key = NULL;
		}
	}

	/*
	 * This is formatter specific, it is kept for the current exemplar only
	 */
	if (key)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("formatter internal parse error at \"%s\"",
						key)));

	*formatter_params = l;
	pfree(to_free);

	return;
}

/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
is_valid_option(const char *option, Oid context)
{
	const struct FileFdwOption *opt;

	for (opt = valid_options; opt->optname; opt++)
	{
		if (context == opt->optcontext &&
			strcmp(opt->optname, option) == 0)
			return true;
	}
	return false;
}

/*
 * Fetch the options for a file_fdw foreign table.
 *
 * We have to separate out "filename" from the other options because
 * it must not appear in the options list passed to the core COPY code.
 */
static void
fileGetOptions(Oid foreigntableid,
			   char **filename, List **other_options)
{
	ForeignTable *table;
	ForeignServer *server;
	ForeignDataWrapper *wrapper;
	List	   *options;
	ListCell   *lc,
			   *prev;

	/*
	 * Extract options from FDW objects.  We ignore user mappings because
	 * file_fdw doesn't have any options that can be specified there.
	 */
	table = GetForeignTable(foreigntableid);
	server = GetForeignServer(table->serverid);
	wrapper = GetForeignDataWrapper(server->fdwid);

	options = NIL;
	options = list_concat(options, wrapper->options);
	options = list_concat(options, server->options);
	options = list_concat(options, table->options);
	options = list_concat(options, get_file_fdw_attribute_options(foreigntableid));

	/*
	 * Separate out the filename.
	 */
	*filename = NULL;
	prev = NULL;
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "filename") == 0)
		{
			*filename = defGetString(def);
			options = list_delete_cell(options, lc, prev);
			break;
		}
		prev = lc;
	}

	Assert(filename);

	if (table->exec_location == FTEXECLOCATION_ALL_SEGMENTS)
	{
		/*
		 * pass the on_segment option to COPY, which will replace the required
		 * placeholder "<SEGID>" in filename
		 */
		options = list_append_unique(options, makeDefElem("on_segment", (Node *) makeInteger(TRUE)));
	}
	else if (table->exec_location == FTEXECLOCATION_ANY)
	{
		/* TODO: validate if external tables allow ANY */
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("file_fdw does not support mpp_execute option 'any'")));
	}

	*other_options = options;
}

/*
 * Retrieve per-column generic options from pg_attribute and construct a list
 * of DefElems representing them.
 *
 * At the moment we only have "force_not_null", and "force_null",
 * which should each be combined into a single DefElem listing all such
 * columns, since that's what COPY expects.
 */
static List *
get_file_fdw_attribute_options(Oid relid)
{
	Relation	rel;
	TupleDesc	tupleDesc;
	AttrNumber	natts;
	AttrNumber	attnum;
	List	   *fnncolumns = NIL;
	List	   *fncolumns = NIL;

	List	   *options = NIL;

	rel = heap_open(relid, AccessShareLock);
	tupleDesc = RelationGetDescr(rel);
	natts = tupleDesc->natts;

	/* Retrieve FDW options for all user-defined attributes. */
	for (attnum = 1; attnum <= natts; attnum++)
	{
		Form_pg_attribute attr = tupleDesc->attrs[attnum - 1];
		List	   *options;
		ListCell   *lc;

		/* Skip dropped attributes. */
		if (attr->attisdropped)
			continue;

		options = GetForeignColumnOptions(relid, attnum);
		foreach(lc, options)
		{
			DefElem    *def = (DefElem *) lfirst(lc);

			if (strcmp(def->defname, "force_not_null") == 0)
			{
				if (defGetBoolean(def))
				{
					char	   *attname = pstrdup(NameStr(attr->attname));

					fnncolumns = lappend(fnncolumns, makeString(attname));
				}
			}
			else if (strcmp(def->defname, "force_null") == 0)
			{
				if (defGetBoolean(def))
				{
					char	   *attname = pstrdup(NameStr(attr->attname));

					fncolumns = lappend(fncolumns, makeString(attname));
				}
			}
			/* maybe in future handle other options here */
		}
	}

	heap_close(rel, AccessShareLock);

	/*
	 * Return DefElem only when some column(s) have force_not_null /
	 * force_null options set
	 */
	if (fnncolumns != NIL)
		options = lappend(options, makeDefElem("force_not_null", (Node *) fnncolumns));

	if (fncolumns != NIL)
		options = lappend(options, makeDefElem("force_null", (Node *) fncolumns));

	return options;
}

/*
 * fileGetForeignPaths
 *		Create possible access paths for a scan on the foreign table
 *
 *		Currently we don't support any push-down feature, so there is only one
 *		possible access path, which simply returns all records in the order in
 *		the data file.
 */
static void
fileGetForeignPaths(PlannerInfo *root,
					RelOptInfo *baserel,
					Oid foreigntableid)
{
	FileFdwPlanState *fdw_private = (FileFdwPlanState *) baserel->fdw_private;
	Cost		startup_cost;
	Cost		total_cost;
	List	   *columns;
	List	   *coptions = NIL;

	/* Decide whether to selectively perform binary conversion */
	if (check_selective_binary_conversion(baserel,
										  foreigntableid,
										  &columns))
		coptions = list_make1(makeDefElem("convert_selectively",
										  (Node *) columns));

	/* Estimate costs */
	estimate_costs(root, baserel, fdw_private,
				   &startup_cost, &total_cost);

	/*
	 * Create a ForeignPath node and add it as only possible path.  We use the
	 * fdw_private list of the path to carry the convert_selectively option;
	 * it will be propagated into the fdw_private list of the Plan node.
	 */
	add_path(baserel, (Path *)
			 create_foreignscan_path(root, baserel,
									 baserel->rows,
									 startup_cost,
									 total_cost,
									 NIL,	/* no pathkeys */
									 NULL,	/* no outer rel either */
									 coptions));

	/*
	 * If data file was sorted, and we knew it somehow, we could insert
	 * appropriate pathkeys into the ForeignPath node to tell the planner
	 * that.
	 */
}

/*
 * fileGetForeignPlan
 *		Create a ForeignScan plan node for scanning the foreign table
 */
static ForeignScan *
fileGetForeignPlan(PlannerInfo *root,
				   RelOptInfo *baserel,
				   Oid foreigntableid,
				   ForeignPath *best_path,
				   List *tlist,
				   List *scan_clauses)
{
	Index		scan_relid = baserel->relid;

	/*
	 * We have no native ability to evaluate restriction clauses, so we just
	 * put all the scan_clauses into the plan node's qual list for the
	 * executor to check.  So all we have to do here is strip RestrictInfo
	 * nodes from the clauses and ignore pseudoconstants (which will be
	 * handled elsewhere).
	 */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/* Create the ForeignScan node */
	return make_foreignscan(tlist,
							scan_clauses,
							scan_relid,
							NIL,	/* no expressions to evaluate */
							best_path->fdw_private);
}

/*
 * fileExplainForeignScan
 *		Produce extra output for EXPLAIN
 */
static void
fileExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
	char	   *filename;
	List	   *options;

	/* Fetch options --- we only need filename at this point */
	fileGetOptions(RelationGetRelid(node->ss.ss_currentRelation),
				   &filename, &options);

	ExplainPropertyText("Foreign File", filename, es);

	/* Suppress file size if we're not showing cost details */
	if (es->costs)
	{
		struct stat stat_buf;

		if (stat(filename, &stat_buf) == 0)
			ExplainPropertyLong("Foreign File Size", (long) stat_buf.st_size,
								es);
	}
}

static List *
setupCustomState(FileFdwExecutionState *festate, Relation relation, List *options)
{
	bool		iscustom;
	char	   *custom_formatter_name;
	List	   *custom_formatter_params;
	char	   *formatter_str;
	ListCell   *lc;

	iscustom = false;
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "format") == 0)
		{
			char	   *format = defGetString(def);

			if (strcmp(format, "custom") == 0)
				iscustom = true;
			break;
		}
	}

	if (!iscustom)
		return options;

	festate->iscustom = true;

	DefElem    *formatter = NULL;

	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "formatter") == 0)
		{
			formatter = def;
			break;
		}
	}

	/* Validation has caught this */
	Assert(formatter);
	formatter_str = defGetString(formatter);
	parseCustomFormatString(formatter_str, &custom_formatter_name, &custom_formatter_params);

	/*
	 * Custom format: get formatter name and find it in the catalog
	 */
	Oid			procOid;

	/* parseFormatString should have seen a formatter name */
	procOid = lookupCustomFormatter(custom_formatter_name, false);

	/* we found our function. set it up for calling */
	festate->custom.fs_custom_formatter_func = palloc(sizeof(FmgrInfo));
	fmgr_info(procOid, festate->custom.fs_custom_formatter_func);
	festate->custom.fs_custom_formatter_params = custom_formatter_params;

	festate->custom.fs_formatter = (FormatterData *) palloc0(sizeof(FormatterData));
	initStringInfo(&festate->custom.fs_formatter->fmt_databuf);

	festate->custom.fs_rd = relation;
	festate->custom.fs_tupDesc = RelationGetDescr(relation);
	TupleDesc	tupDesc = RelationGetDescr(relation);
	Form_pg_attribute *attr = tupDesc->attrs;

	festate->custom.in_functions = (FmgrInfo *) palloc(tupDesc->natts * sizeof(FmgrInfo));
	festate->custom.typioparams = (Oid *) palloc(tupDesc->natts * sizeof(Oid));

	for (int attnum = 0; attnum < tupDesc->natts; attnum++)
	{
		Oid			in_func_oid;

		/* We don't need info for dropped attributes */
		if (attr[attnum]->attisdropped)
			continue;

		getTypeInputInfo(attr[attnum]->atttypid, &in_func_oid, &festate->custom.typioparams[attnum]);
		fmgr_info(in_func_oid, &festate->custom.in_functions[attnum]);
	}

	/*
	 * Finally set the options to Nil since we don't want copy to fiddle with
	 * those
	 */
	return NIL;
}

/*
 * fileBeginForeignScan
 *	Initiate access to the file, create CopyState
 *	and set up state depending on kind of file
 */
static void
fileBeginForeignScan(ForeignScanState *node, int eflags)
{
	ForeignScan *plan = (ForeignScan *) node->ss.ps.plan;
	FileFdwExecutionState *festate;
	CopyState	cstate;
	char	   *filename;
	List	   *options;
	List	   *coptions;

	/*
	 * Do nothing in EXPLAIN (no ANALYZE) case.  node->fdw_state stays NULL.
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	/* Fetch options of foreign table */
	fileGetOptions(RelationGetRelid(node->ss.ss_currentRelation),
				   &filename, &options);

	/* Add any options from the plan (currently only convert_selectively) */
	options = list_concat(options, plan->fdw_private);

	/*
	 * Save state in node->fdw_state.  We must save enough information to call
	 * BeginCopyFrom() again, or the custom access methods
	 */
	festate = (FileFdwExecutionState *) palloc(sizeof(FileFdwExecutionState));
	coptions = setupCustomState(festate, node->ss.ss_currentRelation, options);

	/*
	 * Create CopyState from FDW options.  We always acquire all columns, so
	 * as to match the expected ScanTupleSlot signature.
	 */
	cstate = BeginCopyFrom(node->ss.ss_currentRelation,
						   filename,
						   false,	/* is_program */
						   festate->iscustom ? dummy : NULL,	/* data_source_cb */
						   NULL,	/* data_source_cb_extra */
						   NIL, /* attnamelist */
						   coptions,
						   NIL);	/* ao_segnos */

	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype input or output routines, and should be faster than retail
	 * pfree's anyway.
	 */
	cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "ExtTableExemplarContext",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	festate->filename = filename;
	festate->options = options;
	festate->cstate = cstate;
	node->fdw_state = (void *) festate;

	/* Set up per row error limits if defined */
	festate->custom.reject_limit = -1;

	ListCell   *lc;
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);
		if (strcmp(def->defname, "reject_limit") == 0)
		{
			char *val = defGetString(def);
			festate->custom.reject_limit = atoi(val);
		}
		else if (strcmp(def->defname, "reject_limit_kind") == 0)
		{
			char *val = defGetString(def);
			festate->custom.is_reject_limit_rows = !!(strcmp(val, "rows") == 0);
		}
	}

	if (festate->custom.reject_limit == -1)
	{
		/* Default error handling - "all-or-nothing" */
		festate->cstate->cdbsreh = NULL; /* no SREH */
		festate->cstate->errMode = ALL_OR_NOTHING;
	}
	else
	{
		/* XXX: no error log for now*/
		festate->cstate->errMode = SREH_IGNORE;
		festate->cstate->cdbsreh = makeCdbSreh(festate->custom.reject_limit,
									  festate->custom.is_reject_limit_rows,
									  festate->filename,
									  (char *)festate->cstate->cur_relname,
									  false /* logerrors */);

		festate->cstate->cdbsreh->relid = RelationGetRelid(festate->custom.fs_rd);
	}

	/*
	 * Finally if this is a custom file, open it. This call has to be last,
	 * since CopyState is used for parsing and this is set by BeginCopyFrom
	 */
	if (festate->iscustom)
		festate->custom.url_file = url_fopen(festate->filename, false, NULL, festate->cstate, NULL);
}

/*
 * fileIterateForeignScan
 *		Read next record from the data file and store it into the
 *		ScanTupleSlot as a virtual tuple
 */
static TupleTableSlot *
fileIterateForeignScan(ForeignScanState *node)
{
	FileFdwExecutionState *festate = (FileFdwExecutionState *) node->fdw_state;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	bool		found;
	ErrorContextCallback errcallback;

	/* Set up callback to identify error line number. */
	errcallback.callback = CopyFromErrorCallback;
	errcallback.arg = (void *) festate->cstate;
	errcallback.previous = error_context_stack;
	error_context_stack = &errcallback;

	/*
	 * The protocol for loading a virtual tuple into a slot is first
	 * ExecClearTuple, then fill the values/isnull arrays, then
	 * ExecStoreVirtualTuple.  If we don't find another row in the file, we
	 * just skip the last step, leaving the slot empty as required.
	 *
	 * We can pass ExprContext = NULL because we read all columns from the
	 * file, so no need to evaluate default expressions.
	 *
	 * We can also pass tupleOid = NULL because we don't allow oids for
	 * foreign tables.
	 */
	ExecClearTuple(slot);
	HeapTuple	tuple;

	if (!festate->iscustom)
		found = NextCopyFrom(festate->cstate, NULL,
							 slot_get_values(slot), slot_get_isnull(slot),
							 NULL);
	else
	{
		tuple = dirrect_call(festate);
		found = tuple != NULL;
	}

	if (found)
	{
		if (!festate->iscustom)
			ExecStoreVirtualTuple(slot);
		else
			ExecStoreHeapTuple(tuple, slot, InvalidBuffer, false);
	}

	/* Remove error callback. */
	error_context_stack = errcallback.previous;

	return slot;
}

/*
 * fileReScanForeignScan
 */
static void
fileReScanForeignScan(ForeignScanState *node)
{
	FileFdwExecutionState *festate = (FileFdwExecutionState *) node->fdw_state;

	EndCopyFrom(festate->cstate);

	festate->cstate = BeginCopyFrom(node->ss.ss_currentRelation,
									festate->filename,
									false,	/* is_program */
									NULL,	/* data_source_cb */
									NULL,	/* data_source_cb_extra */
									NIL,	/* attnamelist */
									festate->options,
									NIL);	/* ao_segnos */
}

/*
 * fileEndForeignScan
 *		Finish scanning foreign table and dispose objects used for this scan
 */
static void
fileEndForeignScan(ForeignScanState *node)
{
	FileFdwExecutionState *festate = (FileFdwExecutionState *) node->fdw_state;

	/* if festate is NULL, we are in EXPLAIN; nothing to do */
	if (!festate)
		return;

	if (festate->iscustom)
	{
		if (festate->custom.url_file)
		{
			url_fclose(festate->custom.url_file, false, RelationGetRelationName(festate->custom.fs_rd));
			festate->custom.url_file = NULL;
		}

		if (festate->custom.fs_custom_formatter_func)
			pfree(festate->custom.fs_custom_formatter_func);
		if (festate->custom.fs_formatter)
			pfree(festate->custom.fs_formatter);
		if (festate->custom.in_functions)
			pfree(festate->custom.in_functions);
		if (festate->custom.typioparams)
			pfree(festate->custom.typioparams);
	}

	EndCopyFrom(festate->cstate);
}

/*
 * fileAnalyzeForeignTable
 *		Test whether analyzing this foreign table is supported
 */
static bool
fileAnalyzeForeignTable(Relation relation,
						AcquireSampleRowsFunc *func,
						BlockNumber *totalpages)
{
	char	   *filename;
	List	   *options;
	ListCell   *lc;
	struct stat stat_buf;

	/* Fetch options of foreign table */
	fileGetOptions(RelationGetRelid(relation), &filename, &options);

	/* Skip analyzing a custom format table */
	foreach(lc, options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "format") == 0)
		{
			char	   *format = defGetString(def);

			if (strcmp(format, "custom") == 0)
				return false;
		}
	}

	/*
	 * Get size of the file.  (XXX if we fail here, would it be better to just
	 * return false to skip analyzing the table?)
	 */
	if (stat(filename, &stat_buf) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m",
						filename)));

	/*
	 * Convert size to pages.  Must return at least 1 so that we can tell
	 * later on that pg_class.relpages is not default.
	 */
	*totalpages = (stat_buf.st_size + (BLCKSZ - 1)) / BLCKSZ;
	if (*totalpages < 1)
		*totalpages = 1;

	*func = file_acquire_sample_rows;

	return true;
}

/*
 * check_selective_binary_conversion
 *
 * Check to see if it's useful to convert only a subset of the file's columns
 * to binary.  If so, construct a list of the column names to be converted,
 * return that at *columns, and return TRUE.  (Note that it's possible to
 * determine that no columns need be converted, for instance with a COUNT(*)
 * query.  So we can't use returning a NIL list to indicate failure.)
 */
static bool
check_selective_binary_conversion(RelOptInfo *baserel,
								  Oid foreigntableid,
								  List **columns)
{
	ForeignTable *table;
	ListCell   *lc;
	Relation	rel;
	TupleDesc	tupleDesc;
	AttrNumber	attnum;
	Bitmapset  *attrs_used = NULL;
	bool		has_wholerow = false;
	int			numattrs;
	int			i;

	*columns = NIL;				/* default result */

	/*
	 * Check format of the file.  If binary or custom format, this is
	 * irrelevant.
	 */
	table = GetForeignTable(foreigntableid);
	foreach(lc, table->options)
	{
		DefElem    *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "format") == 0)
		{
			char	   *format = defGetString(def);

			if (strcmp(format, "binary") == 0)
				return false;
			break;
		}
	}

	/* Collect all the attributes needed for joins or final output. */
	pull_varattnos((Node *) baserel->reltargetlist, baserel->relid,
				   &attrs_used);

	/* Add all the attributes used by restriction clauses. */
	foreach(lc, baserel->baserestrictinfo)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		pull_varattnos((Node *) rinfo->clause, baserel->relid,
					   &attrs_used);
	}

	/* Convert attribute numbers to column names. */
	rel = heap_open(foreigntableid, AccessShareLock);
	tupleDesc = RelationGetDescr(rel);

	while ((attnum = bms_first_member(attrs_used)) >= 0)
	{
		/* Adjust for system attributes. */
		attnum += FirstLowInvalidHeapAttributeNumber;

		if (attnum == 0)
		{
			has_wholerow = true;
			break;
		}

		/* Ignore system attributes. */
		if (attnum < 0)
			continue;

		/* Get user attributes. */
		if (attnum > 0)
		{
			Form_pg_attribute attr = tupleDesc->attrs[attnum - 1];
			char	   *attname = NameStr(attr->attname);

			/* Skip dropped attributes (probably shouldn't see any here). */
			if (attr->attisdropped)
				continue;
			*columns = lappend(*columns, makeString(pstrdup(attname)));
		}
	}

	/* Count non-dropped user attributes while we have the tupdesc. */
	numattrs = 0;
	for (i = 0; i < tupleDesc->natts; i++)
	{
		Form_pg_attribute attr = tupleDesc->attrs[i];

		if (attr->attisdropped)
			continue;
		numattrs++;
	}

	heap_close(rel, AccessShareLock);

	/* If there's a whole-row reference, fail: we need all the columns. */
	if (has_wholerow)
	{
		*columns = NIL;
		return false;
	}

	/* If all the user attributes are needed, fail. */
	if (numattrs == list_length(*columns))
	{
		*columns = NIL;
		return false;
	}

	return true;
}

/*
 * Estimate size of a foreign table.
 *
 * The main result is returned in baserel->rows.  We also set
 * fdw_private->pages and fdw_private->ntuples for later use in the cost
 * calculation.
 */
static void
estimate_size(PlannerInfo *root, RelOptInfo *baserel,
			  FileFdwPlanState *fdw_private)
{
	struct stat stat_buf;
	BlockNumber pages;
	double		ntuples;
	double		nrows;

	/*
	 * Get size of the file.  It might not be there at plan time, though, in
	 * which case we have to use a default estimate.
	 */
	if (stat(fdw_private->filename, &stat_buf) < 0)
		stat_buf.st_size = 10 * BLCKSZ;

	/*
	 * Convert size to pages for use in I/O cost estimate later.
	 */
	pages = (stat_buf.st_size + (BLCKSZ - 1)) / BLCKSZ;
	if (pages < 1)
		pages = 1;
	fdw_private->pages = pages;

	/*
	 * Estimate the number of tuples in the file.
	 */
	if (baserel->pages > 0)
	{
		/*
		 * We have # of pages and # of tuples from pg_class (that is, from a
		 * previous ANALYZE), so compute a tuples-per-page estimate and scale
		 * that by the current file size.
		 */
		double		density;

		density = baserel->tuples / (double) baserel->pages;
		ntuples = clamp_row_est(density * (double) pages);
	}
	else
	{
		/*
		 * Otherwise we have to fake it.  We back into this estimate using the
		 * planner's idea of the relation width; which is bogus if not all
		 * columns are being read, not to mention that the text representation
		 * of a row probably isn't the same size as its internal
		 * representation.  Possibly we could do something better, but the
		 * real answer to anyone who complains is "ANALYZE" ...
		 */
		int			tuple_width;

		tuple_width = MAXALIGN(baserel->width) +
			MAXALIGN(sizeof(HeapTupleHeaderData));
		ntuples = clamp_row_est((double) stat_buf.st_size /
								(double) tuple_width);
	}
	fdw_private->ntuples = ntuples;

	/*
	 * Now estimate the number of rows returned by the scan after applying the
	 * baserestrictinfo quals.
	 */
	nrows = ntuples *
		clauselist_selectivity(root,
							   baserel->baserestrictinfo,
							   0,
							   JOIN_INNER,
							   NULL,
							   false);

	nrows = clamp_row_est(nrows);

	/* Save the output-rows estimate for the planner */
	baserel->rows = nrows;
}

/*
 * Estimate costs of scanning a foreign table.
 *
 * Results are returned in *startup_cost and *total_cost.
 */
static void
estimate_costs(PlannerInfo *root, RelOptInfo *baserel,
			   FileFdwPlanState *fdw_private,
			   Cost *startup_cost, Cost *total_cost)
{
	BlockNumber pages = fdw_private->pages;
	double		ntuples = fdw_private->ntuples;
	Cost		run_cost = 0;
	Cost		cpu_per_tuple;

	/*
	 * We estimate costs almost the same way as cost_seqscan(), thus assuming
	 * that I/O costs are equivalent to a regular table file of the same size.
	 * However, we take per-tuple CPU costs as 10x of a seqscan, to account
	 * for the cost of parsing records.
	 */
	run_cost += seq_page_cost * pages;

	*startup_cost = baserel->baserestrictcost.startup;
	cpu_per_tuple = cpu_tuple_cost * 10 + baserel->baserestrictcost.per_tuple;
	run_cost += cpu_per_tuple * ntuples;
	*total_cost = *startup_cost + run_cost;
}

/*
 * file_acquire_sample_rows -- acquire a random sample of rows from the table
 *
 * Selected rows are returned in the caller-allocated array rows[],
 * which must have at least targrows entries.
 * The actual number of rows selected is returned as the function result.
 * We also count the total number of rows in the file and return it into
 * *totalrows.  Note that *totaldeadrows is always set to 0.
 *
 * Note that the returned list of rows is not always in order by physical
 * position in the file.  Therefore, correlation estimates derived later
 * may be meaningless, but it's OK because we don't use the estimates
 * currently (the planner only pays attention to correlation for indexscans).
 */
static int
file_acquire_sample_rows(Relation onerel, int elevel,
						 HeapTuple *rows, int targrows,
						 double *totalrows, double *totaldeadrows)
{
	int			numrows = 0;
	double		rowstoskip = -1;	/* -1 means not set yet */
	double		rstate;
	TupleDesc	tupDesc;
	Datum	   *values;
	bool	   *nulls;
	bool		found;
	char	   *filename;
	List	   *options;
	CopyState	cstate;
	ErrorContextCallback errcallback;
	MemoryContext oldcontext = CurrentMemoryContext;
	MemoryContext tupcontext;

	Assert(onerel);
	Assert(targrows > 0);

	tupDesc = RelationGetDescr(onerel);
	values = (Datum *) palloc(tupDesc->natts * sizeof(Datum));
	nulls = (bool *) palloc(tupDesc->natts * sizeof(bool));

	/* Fetch options of foreign table */
	fileGetOptions(RelationGetRelid(onerel), &filename, &options);

	/*
	 * Create CopyState from FDW options.
	 */
	cstate = BeginCopyFrom(onerel, filename, false, NULL, NULL, NIL, options, NIL);

	/*
	 * Use per-tuple memory context to prevent leak of memory used to read
	 * rows from the file with Copy routines.
	 */
	tupcontext = AllocSetContextCreate(CurrentMemoryContext,
									   "file_fdw temporary context",
									   ALLOCSET_DEFAULT_MINSIZE,
									   ALLOCSET_DEFAULT_INITSIZE,
									   ALLOCSET_DEFAULT_MAXSIZE);

	/* Prepare for sampling rows */
	rstate = anl_init_selection_state(targrows);

	/* Set up callback to identify error line number. */
	errcallback.callback = CopyFromErrorCallback;
	errcallback.arg = (void *) cstate;
	errcallback.previous = error_context_stack;
	error_context_stack = &errcallback;

	*totalrows = 0;
	*totaldeadrows = 0;
	for (;;)
	{
		/* Check for user-requested abort or sleep */
		vacuum_delay_point();

		/* Fetch next row */
		MemoryContextReset(tupcontext);
		MemoryContextSwitchTo(tupcontext);

		found = NextCopyFrom(cstate, NULL, values, nulls, NULL);

		MemoryContextSwitchTo(oldcontext);

		if (!found)
			break;

		/*
		 * The first targrows sample rows are simply copied into the
		 * reservoir.  Then we start replacing tuples in the sample until we
		 * reach the end of the relation. This algorithm is from Jeff Vitter's
		 * paper (see more info in commands/analyze.c).
		 */
		if (numrows < targrows)
		{
			rows[numrows++] = heap_form_tuple(tupDesc, values, nulls);
		}
		else
		{
			/*
			 * t in Vitter's paper is the number of records already processed.
			 * If we need to compute a new S value, we must use the
			 * not-yet-incremented value of totalrows as t.
			 */
			if (rowstoskip < 0)
				rowstoskip = anl_get_next_S(*totalrows, targrows, &rstate);

			if (rowstoskip <= 0)
			{
				/*
				 * Found a suitable tuple, so save it, replacing one old tuple
				 * at random
				 */
				int			k = (int) (targrows * anl_random_fract());

				Assert(k >= 0 && k < targrows);
				heap_freetuple(rows[k]);
				rows[k] = heap_form_tuple(tupDesc, values, nulls);
			}

			rowstoskip -= 1;
		}

		*totalrows += 1;
	}

	/* Remove error callback. */
	error_context_stack = errcallback.previous;

	/* Clean up. */
	MemoryContextDelete(tupcontext);

	EndCopyFrom(cstate);

	pfree(values);
	pfree(nulls);

	/*
	 * Emit some interesting relation info
	 */
	ereport(elevel,
			(errmsg("\"%s\": file contains %.0f rows; "
					"%d rows in sample",
					RelationGetRelationName(onerel),
					*totalrows, numrows)));

	return numrows;
}
