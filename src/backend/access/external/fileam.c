/*-------------------------------------------------------------------------
 *
 * fileam.c
 *	  file access method routines
 *
 * This access layer mimics the heap access API with respect to how it
 * communicates with its respective scan node (external scan node) but
 * instead of accessing the heap pages, it actually "scans" data by
 * reading it from a local flat file or a remote data source.
 *
 * The actual data access, whether local or remote, is done with the
 * curl c library ('libcurl') which uses a 'c-file like' API but behind
 * the scenes actually does all the work of parsing the URI and communicating
 * with the target. In this case if the URI uses the file protocol (file://)
 * curl will try to open the specified file locally. If the URI uses the
 * http protocol (http://) then curl will reach out to that address and
 * get the data from there.
 *
 * As data is being read it gets parsed with the COPY command parsing rules,
 * as if it is data meant for COPY. Therefore, currently, with the lack of
 * single row error handling the first error will raise an error and the
 * query will terminate.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/external/fileam.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fstream/gfile.h>

#include "funcapi.h"
#include "access/fileam.h"
#include "access/formatter.h"
#include "access/heapam.h"
#include "access/valid.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_proc.h"
#include "commands/copy.h"
#include "commands/dbcommands.h"
#include "libpq/libpq-be.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pgstat.h"
#include "parser/parse_func.h"
#include "postmaster/postmaster.h"		/* postmaster port */
#include "utils/relcache.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/uri.h"
#include "utils/builtins.h"

#include "cdb/cdbsreh.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"

static HeapTuple externalgettup(FileScanDesc scan, ScanDirection dir);
static void InitParseState(CopyState pstate, Relation relation,
			   Datum *values, bool *nulls, bool writable,
			   List *fmtOpts, char fmtType,
			   char *uri, int rejectlimit,
			   bool islimitinrows, Oid fmterrtbl, int encoding);

static void FunctionCallPrepareFormatter(FunctionCallInfoData *fcinfo,
							 int nArgs,
							 CopyState pstate,
							 FormatterData *formatter,
							 Relation rel,
							 TupleDesc tupDesc,
							 FmgrInfo *convFuncs,
							 Oid *typioparams);

static void open_external_readable_source(FileScanDesc scan);
static void open_external_writable_source(ExternalInsertDesc extInsertDesc);
static int	external_getdata(URL_FILE *extfile, CopyState pstate, int maxread);
static void external_senddata(URL_FILE *extfile, CopyState pstate);
static void external_scan_error_callback(void *arg);
static void readHeaderLine(CopyState pstate);
static void parseFormatString(CopyState pstate, char *fmtstr, bool iscustom);
static void justifyDatabuf(StringInfo buf);

static void base16_encode(char *raw, int len, char *encoded);
static char *get_eol_delimiter(List *params);
static void external_set_env_vars_ext(extvar_t *extvar, char *uri, bool csv, char *escape,
				 char *quote, int eol_type, bool header, uint32 scancounter, List *params);

/* ----------------------------------------------------------------
*				   external_ interface functions
* ----------------------------------------------------------------
*/

#ifdef FILEDEBUGALL
#define FILEDEBUG_1 \
elog(DEBUG2, "external_getnext([%s],dir=%d) called", \
	 RelationGetRelationName(scan->fs_rd), (int) direction)
#define FILEDEBUG_2 \
elog(DEBUG2, "external_getnext returning EOS")
#define FILEDEBUG_3 \
elog(DEBUG2, "external_getnext returning tuple")
#else
#define FILEDEBUG_1
#define FILEDEBUG_2
#define FILEDEBUG_3
#endif   /* !defined(FILEDEBUGALL) */


/* ----------------
*		external_beginscan	- begin file scan
* ----------------
*/
FileScanDesc
external_beginscan(Relation relation, Index scanrelid, uint32 scancounter,
			   List *uriList, List *fmtOpts, char fmtType, bool isMasterOnly,
			  int rejLimit, bool rejLimitInRows, Oid fmterrtbl, int encoding)
{
	FileScanDesc scan;
	TupleDesc	tupDesc = NULL;
	int			attnum;
	int			segindex = GpIdentity.segindex;
	char	   *uri = NULL;

	/*
	 * increment relation ref count while scanning relation
	 *
	 * This is just to make really sure the relcache entry won't go away while
	 * the scan has a pointer to it.  Caller should be holding the rel open
	 * anyway, so this is redundant in all normal scenarios...
	 */
	RelationIncrementReferenceCount(relation);

	/*
	 * allocate and initialize scan descriptor
	 */
	scan = (FileScanDesc) palloc(sizeof(FileScanDescData));

	scan->fs_inited = false;
	scan->fs_ctup.t_data = NULL;
	ItemPointerSetInvalid(&scan->fs_ctup.t_self);
	scan->fs_cbuf = InvalidBuffer;
	scan->fs_rd = relation;
	scan->fs_scanrelid = scanrelid;
	scan->fs_scancounter = scancounter;
	scan->fs_noop = false;
	scan->fs_file = NULL;
	scan->fs_formatter = NULL;
	scan->fs_constraintExprs = NULL;
	if (relation->rd_att->constr != NULL && relation->rd_att->constr->num_check > 0)
	{
		scan->fs_hasConstraints = true;
	}
	else
	{
		scan->fs_hasConstraints = false;
	}


	/*
	 * get the external URI assigned to us.
	 *
	 * The URI assigned for this segment is normally in the uriList list at
	 * the index of this segment id. However, if we are executing on MASTER
	 * ONLY the (one and only) entry which is destined for the master will be
	 * at the first entry of the uriList list.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		/* this is the normal path for most ext tables */
		Value	   *v;
		int			idx = segindex;

		/*
		 * Segindex may be -1, for the following case. A slice is executed on
		 * entry db, (for example, gp_segment_configuration), then external table is
		 * executed on another slice. Entry db slice will still call
		 * ExecInitExternalScan (probably we should fix this?), then segindex
		 * = -1 will bomb out here.
		 */
		if (isMasterOnly && idx == -1)
			idx = 0;

		if (idx >= 0)
		{
			v = (Value *) list_nth(uriList, idx);

			if (v->type == T_Null)
				uri = NULL;
			else
				uri = (char *) strVal(v);
		}
	}
	else if (Gp_role == GP_ROLE_DISPATCH && isMasterOnly)
	{
		/* this is a ON MASTER table. Only get uri if we are the master */
		if (segindex == -1)
		{
			Value	   *v = list_nth(uriList, 0);

			if (v->type == T_Null)
				uri = NULL;
			else
				uri = (char *) strVal(v);
		}
	}

	/*
	 * if a uri is assigned to us - get a reference to it. Some executors
	 * don't have a uri to scan (if # of uri's < # of primary segdbs). in
	 * which case uri will be NULL. If that's the case for this segdb set to
	 * no-op.
	 */
	if (uri)
	{
		/* set external source (uri) */
		scan->fs_uri = uri;

		/*
		 * NOTE: we delay actually opening the data source until
		 * external_getnext()
		 */
	}
	else
	{
		/* segdb has no work to do. set to no-op */
		scan->fs_noop = true;
		scan->fs_uri = NULL;
	}

	tupDesc = RelationGetDescr(relation);
	scan->fs_tupDesc = tupDesc;
	scan->attr = tupDesc->attrs;
	scan->num_phys_attrs = tupDesc->natts;

	scan->values = (Datum *) palloc(scan->num_phys_attrs * sizeof(Datum));
	scan->nulls = (bool *) palloc(scan->num_phys_attrs * sizeof(bool));

	/*
	 * Pick up the required catalog information for each attribute in the
	 * relation, including the input function and the element type (to pass to
	 * the input function).
	 */
	scan->in_functions = (FmgrInfo *) palloc(scan->num_phys_attrs * sizeof(FmgrInfo));
	scan->typioparams = (Oid *) palloc(scan->num_phys_attrs * sizeof(Oid));

	for (attnum = 1; attnum <= scan->num_phys_attrs; attnum++)
	{
		/* We don't need info for dropped attributes */
		if (scan->attr[attnum - 1]->attisdropped)
			continue;

		getTypeInputInfo(scan->attr[attnum - 1]->atttypid,
						 &scan->in_func_oid, &scan->typioparams[attnum - 1]);
		fmgr_info(scan->in_func_oid, &scan->in_functions[attnum - 1]);
	}

	/*
	 * Allocate and init our structure that keeps track of data parsing state
	 */
	scan->fs_pstate = (CopyStateData *) palloc0(sizeof(CopyStateData));

	/* Initialize all the parsing and state variables */
	InitParseState(scan->fs_pstate, relation, NULL, NULL, false, fmtOpts, fmtType,
				scan->fs_uri, rejLimit, rejLimitInRows, fmterrtbl, encoding);

	if (fmttype_is_custom(fmtType))
	{
		scan->fs_formatter = (FormatterData *) palloc0(sizeof(FormatterData));
		initStringInfo(&scan->fs_formatter->fmt_databuf);
		scan->fs_formatter->fmt_perrow_ctx = scan->fs_pstate->rowcontext;
	}

	/* pgstat_initstats(relation); */

	return scan;
}


/* ----------------
*		external_rescan  - (re)start a scan of an external file
* ----------------
*/
void
external_rescan(FileScanDesc scan)
{
	/* Close previous scan if it was already open */
	external_stopscan(scan);

	/* The first call to external_getnext will re-open the scan */

	/* reset some parse state variables */
	scan->fs_pstate->fe_eof = false;
	scan->fs_pstate->cur_lineno = 0;
	scan->fs_pstate->cur_attname = NULL;
	scan->fs_pstate->raw_buf_done = true;		/* true so we will read data
												 * in first run */
	scan->fs_pstate->line_done = true;
	scan->fs_pstate->bytesread = 0;
}

/* ----------------
*		external_endscan - end a scan
* ----------------
*/
void
external_endscan(FileScanDesc scan)
{
	char	   *relname = pstrdup(RelationGetRelationName(scan->fs_rd));

	if (scan->fs_pstate != NULL)
	{
		/*
		 * decrement relation reference count and free scan descriptor storage
		 */
		RelationDecrementReferenceCount(scan->fs_rd);
	}

	if (scan->values)
	{
		pfree(scan->values);
		scan->values = NULL;
	}
	if (scan->nulls)
	{
		pfree(scan->nulls);
		scan->nulls = NULL;
	}
	if (scan->in_functions)
	{
		pfree(scan->in_functions);
		scan->in_functions = NULL;
	}
	if (scan->typioparams)
	{
		pfree(scan->typioparams);
		scan->typioparams = NULL;
	}

	if (scan->fs_pstate != NULL && scan->fs_pstate->rowcontext != NULL)
	{
		/*
		 * delete the row context
		 */
		MemoryContextDelete(scan->fs_pstate->rowcontext);
		scan->fs_pstate->rowcontext = NULL;
	}

	/*----
	 * if SREH was active:
	 * 1) QEs: send a libpq message to QD with num of rows rejected in this segment
	 * 2) Free SREH resources
	 *----
	 */
	if (scan->fs_pstate != NULL && scan->fs_pstate->errMode != ALL_OR_NOTHING)
	{
		if (Gp_role == GP_ROLE_EXECUTE)
			SendNumRows(scan->fs_pstate->cdbsreh->rejectcount, 0);

		destroyCdbSreh(scan->fs_pstate->cdbsreh);
	}

	if (scan->fs_formatter)
	{
		/*
		 * TODO: check if this space is automatically freed. if not, then see
		 * what about freeing the user context
		 */
		if (scan->fs_formatter->fmt_databuf.data)
			pfree(scan->fs_formatter->fmt_databuf.data);
		pfree(scan->fs_formatter);
		scan->fs_formatter = NULL;
	}

	/*
	 * free parse state memory
	 */
	if (scan->fs_pstate != NULL)
	{
		if (scan->fs_pstate->attribute_buf.data)
			pfree(scan->fs_pstate->attribute_buf.data);
		if (scan->fs_pstate->line_buf.data)
			pfree(scan->fs_pstate->line_buf.data);
		if (scan->fs_pstate->attr_offsets)
			pfree(scan->fs_pstate->attr_offsets);
		if (scan->fs_pstate->force_quote_flags)
			pfree(scan->fs_pstate->force_quote_flags);
		if (scan->fs_pstate->force_notnull_flags)
			pfree(scan->fs_pstate->force_notnull_flags);

		pfree(scan->fs_pstate);
		scan->fs_pstate = NULL;
	}

	/*
	 * Close the external file
	 */
	if (!scan->fs_noop && scan->fs_file)
	{
		/*
		 * QueryFinishPending == true means QD have got
		 * enough tuples and query can return correctly,
		 * so slient errors when closing external file.
		 */
		url_fclose(scan->fs_file, !QueryFinishPending, relname);
		scan->fs_file = NULL;
	}

	pfree(relname);
}


/* ----------------
*		external_stopscan - closes an external resource without dismantling the scan context
* ----------------
*/
void
external_stopscan(FileScanDesc scan)
{
	/*
	 * Close the external file
	 */
	if (!scan->fs_noop && scan->fs_file)
	{
		url_fclose(scan->fs_file, false, RelationGetRelationName(scan->fs_rd));
		scan->fs_file = NULL;
	}
}


/* ----------------------------------------------------------------
*		external_getnext
*
*		Parse a data file and return its rows in heap tuple form
* ----------------------------------------------------------------
*/
HeapTuple
external_getnext(FileScanDesc scan, ScanDirection direction)
{
	HeapTuple	tuple;

	if (scan->fs_noop)
		return NULL;

	/*
	 * open the external source (local file or http).
	 *
	 * NOTE: external_beginscan() seems like the natural place for this call.
	 * However, in queries with more than one gang each gang will initialized
	 * all the nodes of the plan (but actually executed only the nodes in it's
	 * local slice) This means that external_beginscan() (and
	 * external_endscan() too) will get called more than needed and we'll end
	 * up opening too many http connections when they are not expected (see
	 * MPP-1261). Therefore we instead do it here on the first time around
	 * only.
	 */
	if (!scan->fs_file)
		open_external_readable_source(scan);

	/* Note: no locking manipulations needed */
	FILEDEBUG_1;

	tuple = externalgettup(scan, direction);


	if (tuple == NULL)
	{
		FILEDEBUG_2;			/* external_getnext returning EOS */

		return NULL;
	}

	/*
	 * if we get here it means we have a new current scan tuple
	 */
	FILEDEBUG_3;				/* external_getnext returning tuple */

	pgstat_count_heap_getnext(scan->fs_rd);

	return tuple;
}

/*
 * external_insert_init
 *
 * before using external_insert() to insert tuples we need to call
 * this function to initialize our various structures and state..
 */
ExternalInsertDesc
external_insert_init(Relation rel)
{
	ExternalInsertDesc extInsertDesc;
	ExtTableEntry *extentry;
	List	   *fmtopts = NIL;

	/*
	 * Get the pg_exttable information for this table
	 */
	extentry = GetExtTableEntry(RelationGetRelid(rel));

	/*
	 * allocate and initialize the insert descriptor
	 */
	extInsertDesc = (ExternalInsertDesc) palloc0(sizeof(ExternalInsertDescData));
	extInsertDesc->ext_rel = rel;
	extInsertDesc->ext_noop = (Gp_role == GP_ROLE_DISPATCH);
	extInsertDesc->ext_formatter_data = NULL;

	if (extentry->command)
	{
		/*
		 * EXECUTE
		 *
		 * build the command string, 'execute:<command>'
		 */
		extInsertDesc->ext_uri = psprintf("execute:%s", extentry->command);
	}
	else
	{
		/* LOCATION - gpfdist or custom */

		Value	   *v;
		char	   *uri_str;
		int			segindex = GpIdentity.segindex;
		int			num_segs = GpIdentity.numsegments;
		int			num_urls = list_length(extentry->urilocations);
		int			my_url = segindex % num_urls;

		if (num_urls > num_segs)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("External table has more URLs than available primary "
					   "segments that can write into them")));

		/* get a url to use. we use seg number modulo total num of urls */
		v = list_nth(extentry->urilocations, my_url);
		uri_str = pstrdup(v->val.str);
		extInsertDesc->ext_uri = uri_str;

#if 0
		elog(NOTICE, "seg %d got url number %d: %s", segindex, my_url, uri_str);
#endif
	}

	/*
	 * Allocate and init our structure that keeps track of data parsing state
	 */
	extInsertDesc->ext_pstate = (CopyStateData *) palloc0(sizeof(CopyStateData));
	extInsertDesc->ext_tupDesc = RelationGetDescr(rel);
	extInsertDesc->ext_values = (Datum *) palloc(extInsertDesc->ext_tupDesc->natts * sizeof(Datum));
	extInsertDesc->ext_nulls = (bool *) palloc(extInsertDesc->ext_tupDesc->natts * sizeof(bool));

	fmtopts = lappend(fmtopts, makeString(pstrdup(extentry->fmtopts)));

	InitParseState(extInsertDesc->ext_pstate,
				   rel,
				   extInsertDesc->ext_values,
				   extInsertDesc->ext_nulls,
				   true,
				   fmtopts,
				   extentry->fmtcode,
				   extInsertDesc->ext_uri,
				   extentry->rejectlimit,
				   (extentry->rejectlimittype == 'r'),
				   extentry->fmterrtbl,
				   extentry->encoding);

	if (fmttype_is_custom(extentry->fmtcode))
	{
		extInsertDesc->ext_formatter_data = (FormatterData *) palloc0(sizeof(FormatterData));
		extInsertDesc->ext_formatter_data->fmt_perrow_ctx = extInsertDesc->ext_pstate->rowcontext;
	}

	return extInsertDesc;
}


/*
 * external_insert - format the tuple into text and write to the external source
 *
 * Note the following major differences from heap_insert
 *
 * - wal is always bypassed here.
 * - transaction information is of no interest.
 * - tuples are sent always to the destination (local file or remote target).
 *
 * Like heap_insert(), this function can modify the input tuple!
 */
Oid
external_insert(ExternalInsertDesc extInsertDesc, HeapTuple instup)
{
	TupleDesc	tupDesc = extInsertDesc->ext_tupDesc;
	Datum	   *values = extInsertDesc->ext_values;
	bool	   *nulls = extInsertDesc->ext_nulls;
	CopyStateData *pstate = extInsertDesc->ext_pstate;
	bool		customFormat = extInsertDesc->ext_pstate->custom;


	if (extInsertDesc->ext_noop)
		return InvalidOid;


	/* Open our output file or output stream if not yet open */
	if (!extInsertDesc->ext_file && !extInsertDesc->ext_noop)
		open_external_writable_source(extInsertDesc);

	/*
	 * deconstruct the tuple and format it into text
	 */
	if (!customFormat)
	{
		/* TEXT or CSV */
		heap_deform_tuple(instup, tupDesc, values, nulls);
		CopyOneRowTo(pstate, HeapTupleGetOid(instup), values, nulls);
		CopySendEndOfRow(pstate);
	}
	else
	{
		/* custom format. convert tuple using user formatter */
		Datum		d;
		bytea	   *b;
		FunctionCallInfoData fcinfo;

		/*
		 * There is some redundancy between FormatterData and
		 * ExternalInsertDesc we may be able to consolidate data structures a
		 * little.
		 */
		FormatterData *formatter = extInsertDesc->ext_formatter_data;

		/* must have been created during insert_init */
		Assert(formatter);

		/* per call formatter prep */
		FunctionCallPrepareFormatter(&fcinfo,
									 1,
									 pstate,
									 formatter,
									 extInsertDesc->ext_rel,
									 extInsertDesc->ext_tupDesc,
									 pstate->out_functions,
									 NULL);

		/* Mark the correct record type in the passed tuple */
		HeapTupleHeaderSetDatumLength(instup->t_data, instup->t_len);
		HeapTupleHeaderSetTypeId(instup->t_data, tupDesc->tdtypeid);
		HeapTupleHeaderSetTypMod(instup->t_data, tupDesc->tdtypmod);

		fcinfo.arg[0] = HeapTupleGetDatum(instup);
		fcinfo.argnull[0] = false;

		d = FunctionCallInvoke(&fcinfo);
		MemoryContextReset(formatter->fmt_perrow_ctx);

		/* We do not expect a null result */
		if (fcinfo.isnull)
			elog(ERROR, "function %u returned NULL", fcinfo.flinfo->fn_oid);

		b = DatumGetByteaP(d);

		CopyOneCustomRowTo(pstate, b);
	}

	/* Write the data into the external source */
	external_senddata(extInsertDesc->ext_file, pstate);

	/* Reset our buffer to start clean next round */
	pstate->fe_msgbuf->len = 0;
	pstate->fe_msgbuf->data[0] = '\0';
	pstate->processed++;

	return HeapTupleGetOid(instup);
}

/*
 * external_insert_finish
 *
 * when done inserting all the data via external_insert() we need to call
 * this function to flush all remaining data in the buffer into the file.
 */
void
external_insert_finish(ExternalInsertDesc extInsertDesc)
{
	/*
	 * Close the external source
	 */
	if (extInsertDesc->ext_file)
	{
		char	   *relname = RelationGetRelationName(extInsertDesc->ext_rel);

		url_fflush(extInsertDesc->ext_file, extInsertDesc->ext_pstate);
		url_fclose(extInsertDesc->ext_file, true, relname);
	}

	if (extInsertDesc->ext_formatter_data)
		pfree(extInsertDesc->ext_formatter_data);

	pfree(extInsertDesc);
}

/* ==========================================================================
 * The follwing macros aid in major refactoring of data processing code (in
 * externalgettup() ). We use macros because in some cases the code must be in
 * line in order to work (for example elog_dismiss() in PG_CATCH) while in
 * other cases we'd like to inline the code for performance reasons.
 *
 * NOTE that an almost identical set of macros exists in copy.c for the COPY
 * command. If you make changes here you may want to consider taking a look.
 * ==========================================================================
 */

#define EXT_RESET_LINEBUF \
pstate->line_buf.len = 0; \
pstate->line_buf.data[0] = '\0'; \
pstate->line_buf.cursor = 0;

/*
 * A data error happened. This code block will always be inside a PG_CATCH()
 * block right when a higher stack level produced an error. We handle the error
 * by checking which error mode is set (SREH or all-or-nothing) and do the right
 * thing accordingly. Note that we MUST have this code in a macro (as opposed
 * to a function) as elog_dismiss() has to be inlined with PG_CATCH in order to
 * access local error state variables.
 */
#define FILEAM_HANDLE_ERROR \
if (pstate->errMode == ALL_OR_NOTHING) \
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
	oldcontext = MemoryContextSwitchTo(pstate->cdbsreh->badrowcontext);\
	edata = CopyErrorData();\
	MemoryContextSwitchTo(oldcontext);\
\
	if (!elog_dismiss(DEBUG5)) \
		PG_RE_THROW(); /* <-- hope to never get here! */ \
\
	truncateEol(&pstate->line_buf, pstate->eol_type); \
	pstate->cdbsreh->rawdata = pstate->line_buf.data; \
	pstate->cdbsreh->is_server_enc = pstate->line_buf_converted; \
	pstate->cdbsreh->linenumber = pstate->cur_lineno; \
	pstate->cdbsreh->processed = ++pstate->processed; \
	pstate->cdbsreh->consec_csv_err = pstate->num_consec_csv_err; \
\
	/* set the error message. Use original msg and add column name if availble */ \
	if (pstate->cur_attname)\
	{\
		pstate->cdbsreh->errmsg = psprintf("%s, column %s", \
				edata->message, \
				pstate->cur_attname); \
	}\
	else\
	{\
		pstate->cdbsreh->errmsg = pstrdup(edata->message); \
	}\
\
	HandleSingleRowError(pstate->cdbsreh); \
	FreeErrorData(edata);\
	if (!IsRejectLimitReached(pstate->cdbsreh)) \
		pfree(pstate->cdbsreh->errmsg); \
}

/*
 * parse_next_line
 *
 * Given a buffer of data, extract the next data line from it and parse it
 * to attributes according to the data format specifications.
 *
 * Returns:
 *	LINE_OK			- line parsed successfully.
 *	LINE_ERROR		- line was mal-formatted. error caught and handled.
 *	NEED_MORE_DATA	- line not parsed all the way through. need more data.
 *	END_MARKER		- saw line end marker. skip attr parsing, we're done.
 */
static DataLineStatus
parse_next_line(FileScanDesc scan)
{
	CopyState	pstate = scan->fs_pstate;
	MemoryContext oldctxt = CurrentMemoryContext;
	MemoryContext err_ctxt = oldctxt;

	DataLineStatus ret_mode = LINE_OK;

	ListCell   *cur;

	/* Initialize all values for row to NULL */
	MemSet(scan->values, 0, scan->num_phys_attrs * sizeof(Datum));
	MemSet(scan->nulls, true, scan->num_phys_attrs * sizeof(bool));
	MemSet(pstate->attr_offsets, 0, scan->num_phys_attrs * sizeof(int));

	PG_TRY();
	{
		/* Get a line */
		pstate->line_done = pstate->csv_mode ?
			CopyReadLineCSV(pstate, pstate->bytesread) :
			CopyReadLineText(pstate, pstate->bytesread);

		/* Did not get a complete and valid data line? */
		if (!pstate->line_done)
		{
			if (pstate->line_buf.len == 0 && pstate->raw_buf_done)
			{
				ret_mode = NEED_MORE_DATA;
			}

			/*
			 * If eof is not yet reached, we skip att parsing and read more
			 * data. But if eof _was_ reached it means that this data line is
			 * defective and let the attribute parser find the error (we leave
			 * ret_mode intact).
			 */
			if (!pstate->fe_eof)
				ret_mode = NEED_MORE_DATA;

			/* found end marker "\." set ret_mode to skip attribute parsing */
			if (pstate->end_marker)
				ret_mode = END_MARKER;
		}

		if (ret_mode == LINE_OK)
		{
			if (pstate->csv_mode)
				CopyReadAttributesCSV(pstate, scan->nulls, pstate->attr_offsets,
									  scan->num_phys_attrs, scan->attr);
			else
				CopyReadAttributesText(pstate, scan->nulls, pstate->attr_offsets,
									   scan->num_phys_attrs, scan->attr);

			err_ctxt = pstate->rowcontext;
			MemoryContextSwitchTo(err_ctxt);

			foreach(cur, pstate->attnumlist)
			{
				int			attnum = lfirst_int(cur);
				int			m = attnum - 1;
				char	   *string;
				bool		isnull;

				string = pstate->attribute_buf.data + pstate->attr_offsets[m];

				if (!scan->nulls[m])
					isnull = false;
				else
					isnull = true;

				/* check FORCE NOT NULL for this column */
				if (pstate->csv_mode && isnull && pstate->force_notnull_flags[m])
				{
					string = pstate->null_print;		/* set to NULL string */
					isnull = false;
				}

				if (!isnull)
				{
					pstate->cur_attname = NameStr(scan->attr[m]->attname);

					scan->values[m] = InputFunctionCall(&scan->in_functions[m],
														string,
														scan->typioparams[m],
												   scan->attr[m]->atttypmod);
					scan->nulls[m] = false;
					pstate->cur_attname = NULL;
				}
			}
			EXT_RESET_LINEBUF;
		}
	}
	PG_CATCH();
	{
		ret_mode = LINE_ERROR;
		MemoryContextSwitchTo(err_ctxt);
		FILEAM_HANDLE_ERROR;
	}
	PG_END_TRY();

	MemoryContextSwitchTo(oldctxt);

	if (ret_mode == LINE_ERROR)
	{
		ErrorIfRejectLimitReached(pstate->cdbsreh, NULL);
		EXT_RESET_LINEBUF;
	}

	return ret_mode;
}

static HeapTuple
externalgettup_defined(FileScanDesc scan)
{
	HeapTuple	tuple = NULL;
	CopyState	pstate = scan->fs_pstate;
	bool		needData = false;

	/* If we either got things to read or stuff to process */
	while (!pstate->fe_eof || !pstate->raw_buf_done)
	{
		/* need to fill our buffer with data? */
		if (pstate->raw_buf_done)
		{
			pstate->bytesread = external_getdata(scan->fs_file, pstate, RAW_BUF_SIZE);
			pstate->begloc = pstate->raw_buf;
			pstate->raw_buf_done = (pstate->bytesread == 0);
			pstate->raw_buf_index = 0;

			/* on first time around just throw the header line away */
			if (pstate->header_line && pstate->bytesread > 0)
			{
				PG_TRY();
				{
					readHeaderLine(pstate);
				}
				PG_CATCH();
				{
					/*
					 * got here? encoding conversion error occured on the
					 * header line (first row).
					 */
					if (pstate->errMode == ALL_OR_NOTHING)
					{
						PG_RE_THROW();
					}
					else
					{
						/* SREH - release error state */
						if (!elog_dismiss(DEBUG5))
							PG_RE_THROW();		/* hope to never get here! */

						/*
						 * note: we don't bother doing anything special here.
						 * we are never interested in logging a header line
						 * error. just continue the workflow.
						 */
					}
				}
				PG_END_TRY();

				EXT_RESET_LINEBUF;
				pstate->header_line = false;
			}
		}

		/* while there is still data in our buffer */
		while (!pstate->raw_buf_done || needData)
		{
			DataLineStatus ret_mode = parse_next_line(scan);

			if (ret_mode == LINE_OK)
			{
				/* convert to heap tuple */

				/*
				 * XXX This is bad code.  Planner should be able to decide
				 * whether we need heaptuple or memtuple upstream, so make the
				 * right decision here.
				 */
				tuple = heap_form_tuple(scan->fs_tupDesc, scan->values, scan->nulls);
				pstate->processed++;
				MemoryContextReset(pstate->rowcontext);
				return tuple;
			}
			else if (ret_mode == LINE_ERROR && !pstate->raw_buf_done)
			{
				/* error was handled in parse_next_line. move to the next */
				continue;
			}
			else if (ret_mode == END_MARKER)
			{
				scan->fs_inited = false;
				return NULL;
			}
			else
			{
				/* try to get more data if possible */
				Assert((ret_mode == NEED_MORE_DATA) ||
					   (ret_mode == LINE_ERROR && pstate->raw_buf_done));
				needData = true;
				break;
			}
		}
	}

	/*
	 * if we got here we finished reading all the data.
	 */
	scan->fs_inited = false;

	return NULL;


}

static HeapTuple
externalgettup_custom(FileScanDesc scan)
{
	HeapTuple	tuple;
	CopyState	pstate = scan->fs_pstate;
	FormatterData *formatter = scan->fs_formatter;
	bool		no_more_data = false;
	MemoryContext oldctxt = CurrentMemoryContext;

	Assert(formatter);

	/* while didn't finish processing the entire file */
	while (!no_more_data)
	{
		/* need to fill our buffer with data? */
		if (pstate->raw_buf_done)
		{
			int			bytesread = external_getdata(scan->fs_file, pstate, RAW_BUF_SIZE);

			if (bytesread > 0)
				appendBinaryStringInfo(&formatter->fmt_databuf, pstate->raw_buf, bytesread);
			pstate->raw_buf_done = false;

			/* HEADER not yet supported ... */
			if (pstate->header_line)
				elog(ERROR, "header line in custom format is not yet supported");
		}

		if (formatter->fmt_databuf.len > 0 || !pstate->fe_eof)
		{
			/* while there is still data in our buffer */
			while (!pstate->raw_buf_done)
			{
				bool		error_caught = false;

				/*
				 * Invoke the custom formatter function.
				 */
				PG_TRY();
				{
					FunctionCallInfoData fcinfo;

					/* per call formatter prep */
					FunctionCallPrepareFormatter(&fcinfo,
												 0,
												 pstate,
												 formatter,
												 scan->fs_rd,
												 scan->fs_tupDesc,
												 scan->in_functions,
												 scan->typioparams);
					(void) FunctionCallInvoke(&fcinfo);

				}
				PG_CATCH();
				{
					error_caught = true;

					MemoryContextSwitchTo(formatter->fmt_perrow_ctx);

					/*
					 * Save any bad row information that was set by the user
					 * in the formatter UDF (if any). Then handle the error in
					 * FILEAM_HANDLE_ERROR.
					 */
					pstate->cur_lineno = formatter->fmt_badrow_num;
					pstate->cur_byteno = formatter->fmt_bytesread;
					resetStringInfo(&pstate->line_buf);

					if (formatter->fmt_badrow_len > 0)
					{
						if (formatter->fmt_badrow_data)
							appendBinaryStringInfo(&pstate->line_buf,
												   formatter->fmt_badrow_data,
												   formatter->fmt_badrow_len);

						formatter->fmt_databuf.cursor += formatter->fmt_badrow_len;
						if (formatter->fmt_databuf.cursor > formatter->fmt_databuf.len ||
							formatter->fmt_databuf.cursor < 0)
						{
							formatter->fmt_databuf.cursor = formatter->fmt_databuf.len;
						}
					}

					FILEAM_HANDLE_ERROR;

					MemoryContextSwitchTo(oldctxt);
				}
				PG_END_TRY();

				/*
				 * Examine the function results. If an error was caught we
				 * already handled it, so after checking the reject limit,
				 * loop again and call the UDF for the next tuple.
				 */
				if (!error_caught)
				{
					switch (formatter->fmt_notification)
					{
						case FMT_NONE:

							/* got a tuple back */

							tuple = formatter->fmt_tuple;
							pstate->processed++;
							MemoryContextReset(formatter->fmt_perrow_ctx);

							return tuple;

						case FMT_NEED_MORE_DATA:

							/*
							 * Callee consumed all data in the buffer. Prepare
							 * to read more data into it.
							 */
							pstate->raw_buf_done = true;
							justifyDatabuf(&formatter->fmt_databuf);

							if (pstate->fe_eof && formatter->fmt_databuf.len > 0)
							{
								/*
								 * The formatter needs more data, but we have reached
								 * EOF. This is an error.
								 */
								ereport(ERROR,
										(ERRCODE_DATA_EXCEPTION,
										 errmsg("unexpected end of file")));
							}

							continue;

						default:
							elog(ERROR, "unsupported formatter notification (%d)",
								 formatter->fmt_notification);
							break;
					}
				}
				else
				{
					ErrorIfRejectLimitReached(pstate->cdbsreh, NULL);
				}

			}
		}
		else
		{
			no_more_data = true;
		}
	}

	/*
	 * if we got here we finished reading all the data.
	 */
	Assert(no_more_data);
	scan->fs_inited = false;

	return NULL;
}

/* ----------------
*		externalgettup	form another tuple from the data file.
*		This is the workhorse - make sure it's fast!
*
*		Initialize the scan if not already done.
*		Verify that we are scanning forward only.
*
* ----------------
*/
static HeapTuple
externalgettup(FileScanDesc scan,
			   ScanDirection dir __attribute__((unused)))
{
	CopyState	pstate = scan->fs_pstate;
	bool		custom = pstate->custom;
	HeapTuple	tup = NULL;
	ErrorContextCallback externalscan_error_context;

	Assert(ScanDirectionIsForward(dir));

	externalscan_error_context.callback = external_scan_error_callback;
	externalscan_error_context.arg = (void *) scan->fs_pstate;
	externalscan_error_context.previous = error_context_stack;

	error_context_stack = &externalscan_error_context;

	if (!scan->fs_inited)
	{
		/* more init stuff here... */
		scan->fs_inited = true;
	}
	else
	{
		/* continue from previously returned tuple */
		/* (set current state...) */
	}

	if (!custom)
		tup = externalgettup_defined(scan); /* text/csv */
	else
		tup = externalgettup_custom(scan);  /* custom */

	/* Restore the previous error callback */
	error_context_stack = externalscan_error_context.previous;

	return tup;
}

/*
 * setCustomFormatter
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
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION),
					errmsg("formatter function %s of type %s was not found.",
						   formatter_name,
						   (iswritable ? "writable" : "readable")),
						errhint("Create it with CREATE FUNCTION.")));

	/* check return type matches */
	if (get_func_rettype(procOid) != returnOid)
		ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						errmsg("formatter function %s of type %s has an incorrect return type",
							   formatter_name,
							   (iswritable ? "writable" : "readable"))));

	/* check allowed volatility */
	if (func_volatile(procOid) != PROVOLATILE_STABLE)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("formatter function %s is not declared STABLE.",
						formatter_name)));

	return procOid;
}


/*
 * Initialize the data parsing state.
 *
 * This includes format descriptions (delimiter, quote...), format type
 * (text, csv), encoding converstion information, etc...
 */
static void
InitParseState(CopyState pstate, Relation relation,
			   Datum *values, bool *nulls, bool iswritable,
			   List *fmtOpts, char fmtType,
			   char *uri, int rejectlimit,
			   bool islimitinrows, Oid fmterrtbl, int encoding)
{
	TupleDesc	tupDesc = NULL;
	char	   *format_str = NULL;
	bool		format_is_custom = fmttype_is_custom(fmtType);

	pstate->fe_eof = false;
	pstate->eol_type = EOL_UNKNOWN;
	pstate->eol_str = NULL;
	pstate->cur_relname = RelationGetRelationName(relation);
	pstate->cur_lineno = 0;
	pstate->err_loc_type = ROWNUM_ORIGINAL;
	pstate->cur_attname = NULL;
	pstate->raw_buf_done = true;	/* true so we will read data in first run */
	pstate->line_done = true;
	pstate->bytesread = 0;
	pstate->custom = false;
	pstate->header_line = false;
	pstate->fill_missing = false;
	pstate->line_buf_converted = false;
	pstate->raw_buf_index = 0;
	pstate->processed = 0;
	pstate->filename = uri;
	pstate->copy_dest = COPY_EXTERNAL_SOURCE;
	pstate->missing_bytes = 0;
	pstate->csv_mode = fmttype_is_csv(fmtType);
	pstate->custom = fmttype_is_custom(fmtType);
	pstate->custom_formatter_func = NULL;
	pstate->custom_formatter_name = NULL;
	pstate->rel = relation;

	/*
	 * Error handling setup
	 */
	if (rejectlimit == -1)
	{
		/* Default error handling - "all-or-nothing" */
		pstate->cdbsreh = NULL; /* no SREH */
		pstate->errMode = ALL_OR_NOTHING;
	}
	else
	{
		/* select the SREH mode */
		if (fmterrtbl == InvalidOid)
		{
			/* no error log */
			pstate->errMode = SREH_IGNORE;
		}
		else if (fmterrtbl == RelationGetRelid(relation))
		{
			/* errors into file */
			pstate->errMode = SREH_LOG;
		}

		/* Single row error handling */
		pstate->cdbsreh = makeCdbSreh(rejectlimit,
									  islimitinrows,
									  pstate->filename,
									  (char *) pstate->cur_relname,
									  true);

		pstate->cdbsreh->relid = RelationGetRelid(relation);

		pstate->num_consec_csv_err = 0;
	}


	/*
	 * Set up encoding conversion info.  Even if the client and server
	 * encodings are the same, we must apply pg_client_to_server() to validate
	 * data in multibyte encodings.
	 *
	 * Each external table specifies the encoding of its external data. We
	 * will therefore set a client encoding and client-to-server conversion
	 * procedure in here (server-to-client in WET) and these will be used in
	 * the data conversion routines (in copy.c CopyReadLineXXX(), etc).
	 */
	Insist(PG_VALID_ENCODING(encoding));
	pstate->client_encoding = encoding;
	setEncodingConversionProc(pstate, encoding, iswritable);
	pstate->need_transcoding = (pstate->client_encoding != GetDatabaseEncoding() ||
								pg_database_encoding_max_length() > 1);
	pstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(pstate->client_encoding);

	/*
	 * Now parse the data FORMAT options.
	 */
	format_str = pstrdup((char *) strVal(linitial(fmtOpts)));
	parseFormatString(pstate, format_str, format_is_custom);

	/*
	 * Custom format: get formatter name and find it in the catalog
	 */
	if (format_is_custom)
	{
		Oid			procOid;

		/* parseFormatString should have seen a formatter name */
		if (!pstate->custom_formatter_name)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("No formatter function defined for this external table.")));

		procOid = lookupCustomFormatter(pstate->custom_formatter_name, iswritable);

		/* we found our function. set it in pstate */
		pstate->custom_formatter_func = palloc(sizeof(FmgrInfo));
		fmgr_info(procOid, pstate->custom_formatter_func);
	}

	tupDesc = RelationGetDescr(relation);
	pstate->attr_offsets = (int *) palloc(tupDesc->natts * sizeof(int));

	/* Generate or convert list of attributes to process */
	pstate->attnumlist = CopyGetAttnums(tupDesc, relation, NIL);

	/* Convert FORCE NOT NULL name list to per-column flags, check validity */
	pstate->force_notnull_flags = (bool *) palloc0(tupDesc->natts * sizeof(bool));
	if (pstate->force_notnull)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, relation, pstate->force_notnull);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			pstate->force_notnull_flags[attnum - 1] = true;
		}
	}

	/* Convert FORCE QUOTE name list to per-column flags, check validity */
	pstate->force_quote_flags = (bool *) palloc0(tupDesc->natts * sizeof(bool));
	if (pstate->force_quote)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, relation, pstate->force_quote);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			pstate->force_quote_flags[attnum - 1] = true;
		}
	}

	/* finally take care of state that is WET or RET specific */
	if (!iswritable)
	{
		/* RET */
		initStringInfo(&pstate->attribute_buf);
		initStringInfo(&pstate->line_buf);

		/* Set up data buffer to hold a chunk of data */
		MemSet(pstate->raw_buf, ' ', RAW_BUF_SIZE * sizeof(char));
		pstate->raw_buf[RAW_BUF_SIZE] = '\0';

	}
	else
	{
		/* WET */

		Form_pg_attribute *attr = tupDesc->attrs;
		ListCell   *cur;

		pstate->null_print_client = pstate->null_print; /* default */

		/* We use fe_msgbuf as a per-row buffer */
		pstate->fe_msgbuf = makeStringInfo();

		pstate->out_functions =
			(FmgrInfo *) palloc(tupDesc->natts * sizeof(FmgrInfo));

		/* Get info about the columns we need to process. */
		foreach(cur, pstate->attnumlist)
		{
			int			attnum = lfirst_int(cur);
			Oid			out_func_oid;
			bool		isvarlena;

			getTypeOutputInfo(attr[attnum - 1]->atttypid,
							  &out_func_oid,
							  &isvarlena);
			fmgr_info(out_func_oid, &pstate->out_functions[attnum - 1]);
		}

		/*
		 * we need to convert null_print to client encoding, because it will
		 * be sent directly with CopySendString.
		 */
		if (pstate->need_transcoding)
			pstate->null_print_client = pg_server_to_client(pstate->null_print,
													 pstate->null_print_len);
	}


	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype input or output routines, and should be faster than retail
	 * pfree's anyway.
	 */
	pstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "ExtTableMemCxt",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);
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
	formatter->fmt_args = pstate->custom_formatter_params;
	formatter->fmt_conv_funcs = convFuncs;
	formatter->fmt_saw_eof = pstate->fe_eof;
	formatter->fmt_typioparams = typioparams;
	formatter->fmt_perrow_ctx = pstate->rowcontext;
	formatter->fmt_needs_transcoding = pstate->need_transcoding;
	formatter->fmt_conversion_proc = pstate->enc_conversion_proc;
	formatter->fmt_external_encoding = pstate->client_encoding;

	InitFunctionCallInfoData( /* FunctionCallInfoData */ *fcinfo,
							  /* FmgrInfo */ pstate->custom_formatter_func,
							  /* nArgs */ nArgs,
							  /* Call Context */ (Node *) formatter,
							  /* ResultSetInfo */ NULL);
}


/*
 * open the external source for scanning (RET only)
 *
 * an external source is one of the following:
 * 1) a local file (requested by 'file')
 * 2) a remote http server
 * 3) a remote gpfdist server
 * 4) a command to execute
 */
static void
open_external_readable_source(FileScanDesc scan)
{
	extvar_t	extvar;

	/* set up extvar */
	memset(&extvar, 0, sizeof(extvar));
	external_set_env_vars_ext(&extvar,
							  scan->fs_uri,
							  scan->fs_pstate->csv_mode,
							  scan->fs_pstate->escape,
							  scan->fs_pstate->quote,
							  scan->fs_pstate->eol_type,
							  scan->fs_pstate->header_line,
							  scan->fs_scancounter,
							  scan->fs_pstate->custom_formatter_params);

	/* actually open the external source */
	scan->fs_file = url_fopen(scan->fs_uri,
							  false /* for read */ ,
							  &extvar,
							  scan->fs_pstate);
}

/*
 * open the external source for writing (WET only)
 *
 * an external source is one of the following:
 * 1) a local file (requested by 'tablespace' protocol)
 * 2) a command to execute
 */
static void
open_external_writable_source(ExternalInsertDesc extInsertDesc)
{
	extvar_t	extvar;

	/* set up extvar */
	memset(&extvar, 0, sizeof(extvar));
	external_set_env_vars_ext(&extvar,
							  extInsertDesc->ext_uri,
							  extInsertDesc->ext_pstate->csv_mode,
							  extInsertDesc->ext_pstate->escape,
							  extInsertDesc->ext_pstate->quote,
							  extInsertDesc->ext_pstate->eol_type,
							  extInsertDesc->ext_pstate->header_line,
							  0,
						 extInsertDesc->ext_pstate->custom_formatter_params);

	/* actually open the external source */
	extInsertDesc->ext_file = url_fopen(extInsertDesc->ext_uri,
										true /* forwrite */ ,
										&extvar,
										extInsertDesc->ext_pstate);
}

/*
 * get a chunk of data from the external data file.
 */
static int
external_getdata(URL_FILE *extfile, CopyState pstate, int maxread)
{
	int			bytesread = 0;

	/*
	 * CK: this code is very delicate. The caller expects this: - if url_fread
	 * returns something, and the EOF is reached, it this call must return
	 * with both the content and the fe_eof flag set. - failing to do so will
	 * result in skipping the last line.
	 */


	bytesread = url_fread((void *) pstate->raw_buf, maxread, extfile, pstate);

	if (url_feof(extfile, bytesread))
	{
		pstate->fe_eof = true;
	}

	if (bytesread <= 0)
	{
		if (url_ferror(extfile, bytesread, NULL, 0))
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read from external file: %m")));

	}

	return bytesread;
}

/*
 * send a chunk of data from the external data file.
 */
static void
external_senddata(URL_FILE *extfile, CopyState pstate)
{
	StringInfo	fe_msgbuf = pstate->fe_msgbuf;
	static char ebuf[512] = {0};
	size_t		nwrote = 0;
	int			ebuflen = 512;

	nwrote = url_fwrite((void *) fe_msgbuf->data, fe_msgbuf->len, extfile, pstate);

	if (url_ferror(extfile, nwrote, ebuf, ebuflen))
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 strlen(ebuf) > 0 ? errmsg("could not write to external resource:\n%s", ebuf) :
				 errmsg("could not write to external resource: %m")));
	}
}

/*
 * error context callback for external table scan
 */
static void
external_scan_error_callback(void *arg)
{
	CopyState	cstate = (CopyState) arg;
	char		buffer[20];

	/*
	 * early exit for custom format error. We don't have metadata to report
	 * on. TODO: this actually will override any errcontext that the user
	 * wants to set. maybe another approach is needed here.
	 */
	if (cstate->custom)
	{
		errcontext("External table %s", cstate->cur_relname);
		return;
	}

	if (cstate->cur_attname)
	{
		/* error is relevant to a particular column */

		errcontext("External table %s, line %s of %s, column %s",
				   cstate->cur_relname,
				   linenumber_atoi(buffer, cstate->cur_lineno),
				   cstate->filename,
				   cstate->cur_attname);
	}
	else
	{
		/* error is relevant to a particular line */
		if (cstate->line_buf_converted || !cstate->need_transcoding)
		{
			char	   *line_buf;

			line_buf = limit_printout_length(cstate->line_buf.data);
			truncateEolStr(line_buf, cstate->eol_type);

			errcontext("External table %s, line %s of %s: \"%s\"",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, cstate->cur_lineno),
					   cstate->filename, line_buf);
			pfree(line_buf);
		}
		else
		{
			/*
			 * Here, the line buffer is still in a foreign encoding, and
			 * indeed it's quite likely that the error is precisely a failure
			 * to do encoding conversion (ie, bad data).  We dare not try to
			 * convert it, and at present there's no way to regurgitate it
			 * without conversion.  So we have to punt and just report the
			 * line number.
			 *
			 * since the gpfdist protocol does not transfer line numbers
			 * correclty in certain places - if line number is 0 we just do
			 * not print it.
			 */
			if (cstate->cur_lineno > 0)
				errcontext("External table %s, line %s of file %s",
						   cstate->cur_relname,
						   linenumber_atoi(buffer, cstate->cur_lineno),
						   cstate->filename);
			else
				errcontext("External table %s, file %s",
						   cstate->cur_relname, cstate->filename);
		}
	}
}

/*
 * Read the data file header line and ignore it.
 *
 * This function should be called only once for each data file
 * and only if HEADER is specified in the SQL command.
 */
static void
readHeaderLine(CopyState pstate)
{
	pstate->line_done = pstate->csv_mode ?
		CopyReadLineCSV(pstate, pstate->bytesread) :
		CopyReadLineText(pstate, pstate->bytesread);
}

void
gfile_printf_then_putc_newline(const char *format,...)
{
	char	   *a;
	va_list		va;
	int			i;

	va_start(va, format);
	i = vsnprintf(0, 0, format, va);
	va_end(va);

	if (i < 0)
		elog(NOTICE, "gfile_printf_then_putc_newline vsnprintf failed.");
	else if (!(a = palloc(i + 1)))
		elog(NOTICE, "gfile_printf_then_putc_newline palloc failed.");
	else
	{
		va_start(va, format);
		vsnprintf(a, i + 1, format, va);
		va_end(va);
		elog(NOTICE, "%s", a);
		pfree(a);
	}
}

void *
gfile_malloc(size_t size)
{
	return palloc(size);
}

void
gfile_free(void *a)
{
	pfree(a);
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
		buf->data[buf->len] = '\0';		/* be consistent with
										 * appendBinaryStringInfo() */
	}

	buf->cursor = 0;
}

char *
linenumber_atoi(char buffer[20], int64 linenumber)
{
	if (linenumber < 0)
		return "N/A";

	snprintf(buffer, 20, INT64_FORMAT, linenumber);

	return buffer;
}


/*
 * strip_quotes
 *
 * (copied from bin/psql/stringutils.c - TODO: place to share FE and BE code?).
 *
 * Remove quotes from the string at *source.  Leading and trailing occurrences
 * of 'quote' are removed; embedded double occurrences of 'quote' are reduced
 * to single occurrences; if 'escape' is not 0 then 'escape' removes special
 * significance of next character.
 *
 * Note that the source string is overwritten in-place.
 */
static void
strip_quotes(char *source, char quote, char escape, int encoding)
{
	char	   *src;
	char	   *dst;

	Assert(source);
	Assert(quote);

	src = dst = source;

	if (*src && *src == quote)
		src++;					/* skip leading quote */

	while (*src)
	{
		char		c = *src;
		int			i;

		if (c == quote && src[1] == '\0')
			break;				/* skip trailing quote */
		else if (c == quote && src[1] == quote)
			src++;				/* process doubled quote */
		else if (c == escape && src[1] != '\0')
			src++;				/* process escaped character */

		i = pg_encoding_mblen(encoding, src);
		while (i--)
			*dst++ = *src++;
	}

	*dst = '\0';
}

/*
 * strtokx2
 *
 * strtokx2 is a replica of psql's strtokx (bin/psql/stringutils.c), fitted
 * to be used in the backend for the same purpose - parsing an sql string of
 * literals. Information follows (right now identical to strtokx, except for
 * a small hack - see below comment about MPP-6698):
 *
 * Replacement for strtok() (a.k.a. poor man's flex)
 *
 * Splits a string into tokens, returning one token per call, then NULL
 * when no more tokens exist in the given string.
 *
 * The calling convention is similar to that of strtok, but with more
 * frammishes.
 *
 * s -			string to parse, if NULL continue parsing the last string
 * whitespace - set of whitespace characters that separate tokens
 * delim -		set of non-whitespace separator characters (or NULL)
 * quote -		set of characters that can quote a token (NULL if none)
 * escape -		character that can quote quotes (0 if none)
 * e_strings -	if TRUE, treat E'...' syntax as a valid token
 * del_quotes - if TRUE, strip quotes from the returned token, else return
 *				it exactly as found in the string
 * encoding -	the active character-set encoding
 *
 * Characters in 'delim', if any, will be returned as single-character
 * tokens unless part of a quoted token.
 *
 * Double occurrences of the quoting character are always taken to represent
 * a single quote character in the data.  If escape isn't 0, then escape
 * followed by anything (except \0) is a data character too.
 *
 * The combination of e_strings and del_quotes both TRUE is not currently
 * handled.  This could be fixed but it's not needed anywhere at the moment.
 *
 * Note that the string s is _not_ overwritten in this implementation.
 *
 * NB: it's okay to vary delim, quote, and escape from one call to the
 * next on a single source string, but changing whitespace is a bad idea
 * since you might lose data.
 */
static char *
strtokx2(const char *s,
		 const char *whitespace,
		 const char *delim,
		 const char *quote,
		 char escape,
		 bool e_strings,
		 bool del_quotes,
		 int encoding)
{
	static char *storage = NULL;/* store the local copy of the users string
								 * here */
	static char *string = NULL; /* pointer into storage where to continue on
								 * next call */

	/* variously abused variables: */
	unsigned int offset;
	char	   *start;
	char	   *p;

	if (s)
	{
		/*
		 * We may need extra space to insert delimiter nulls for adjacent
		 * tokens.  2X the space is a gross overestimate, but it's unlikely
		 * that this code will be used on huge strings anyway.
		 */
		storage = palloc(2 * strlen(s) + 1);
		strcpy(storage, s);
		string = storage;
	}

	if (!storage)
		return NULL;

	/* skip leading whitespace */
	offset = strspn(string, whitespace);
	start = &string[offset];

	/* end of string reached? */
	if (*start == '\0')
	{
		/* technically we don't need to free here, but we're nice */
		pfree(storage);
		storage = NULL;
		string = NULL;
		return NULL;
	}

	/* test if delimiter character */
	if (delim && strchr(delim, *start))
	{
		/*
		 * If not at end of string, we need to insert a null to terminate the
		 * returned token.  We can just overwrite the next character if it
		 * happens to be in the whitespace set ... otherwise move over the
		 * rest of the string to make room.  (This is why we allocated extra
		 * space above).
		 */
		p = start + 1;
		if (*p != '\0')
		{
			if (!strchr(whitespace, *p))
				memmove(p + 1, p, strlen(p) + 1);
			*p = '\0';
			string = p + 1;
		}
		else
		{
			/* at end of string, so no extra work */
			string = p;
		}

		return start;
	}

	/* check for E string */
	p = start;
	if (e_strings &&
		(*p == 'E' || *p == 'e') &&
		p[1] == '\'')
	{
		quote = "'";
		escape = '\\';			/* if std strings before, not any more */
		p++;
	}

	/* test if quoting character */
	if (quote && strchr(quote, *p))
	{
		/* okay, we have a quoted token, now scan for the closer */
		char		thisquote = *p++;

		/*
		 * MPP-6698 START
		 *
		 * unfortunately, it is possible for an external table format string
		 * to be represented in the catalog in a way which is problematic to
		 * parse: when using a single quote as a QUOTE or ESCAPE character the
		 * format string will show [quote ''']. since we do not want to change
		 * how this is stored at this point (as it will affect previous
		 * versions of the software already in production) the following code
		 * block will detect this scenario where 3 quote characters follow
		 * each other, with no fourth one. in that case, we will skip the
		 * second one (the first is skipped just above) and the last trailing
		 * quote will be skipped below. the result will be the actual token
		 * (''') and after stripping it due to del_quotes we'll end up with
		 * ('). very ugly, but will do the job...
		 */
		char		qt = quote[0];

		if (strlen(p) >= 3 && p[0] == qt && p[1] == qt && p[2] != qt)
			p++;
		/* MPP-6698 END */

		for (; *p; p += pg_encoding_mblen(encoding, p))
		{
			if (*p == escape && p[1] != '\0')
				p++;			/* process escaped anything */
			else if (*p == thisquote && p[1] == thisquote)
				p++;			/* process doubled quote */
			else if (*p == thisquote)
			{
				p++;			/* skip trailing quote */
				break;
			}
		}

		/*
		 * If not at end of string, we need to insert a null to terminate the
		 * returned token.  See notes above.
		 */
		if (*p != '\0')
		{
			if (!strchr(whitespace, *p))
				memmove(p + 1, p, strlen(p) + 1);
			*p = '\0';
			string = p + 1;
		}
		else
		{
			/* at end of string, so no extra work */
			string = p;
		}

		/* Clean up the token if caller wants that */
		if (del_quotes)
			strip_quotes(start, thisquote, escape, encoding);

		return start;
	}

	/*
	 * Otherwise no quoting character.  Scan till next whitespace, delimiter
	 * or quote.  NB: at this point, *start is known not to be '\0',
	 * whitespace, delim, or quote, so we will consume at least one character.
	 */
	offset = strcspn(start, whitespace);

	if (delim)
	{
		unsigned int offset2 = strcspn(start, delim);

		if (offset > offset2)
			offset = offset2;
	}

	if (quote)
	{
		unsigned int offset2 = strcspn(start, quote);

		if (offset > offset2)
			offset = offset2;
	}

	p = start + offset;

	/*
	 * If not at end of string, we need to insert a null to terminate the
	 * returned token.  See notes above.
	 */
	if (*p != '\0')
	{
		if (!strchr(whitespace, *p))
			memmove(p + 1, p, strlen(p) + 1);
		*p = '\0';
		string = p + 1;
	}
	else
	{
		/* at end of string, so no extra work */
		string = p;
	}

	return start;
}

/*
 * parseFormatString
 *
 * Given a data format string (e.g: "delimiter '|' null ''"), parse it to its
 * individual elements and store the parsed values into pstate. this routine
 * will parse the format string for both 'text' and 'csv' data formats. the
 * logic here is largely borrowed from psql's parsing of '\copy' and adapted
 * for use in the backend, for the supported external table options only.
 */
static void
parseFormatString(CopyState pstate, char *fmtstr, bool iscustom)
{
	char	   *token;
	const char *whitespace = " \t\n\r";
	char		nonstd_backslash = 0;
	int			encoding = GetDatabaseEncoding();

	token = strtokx2(fmtstr, whitespace, NULL, NULL,
					 0, false, true, encoding);

	if (!iscustom)
	{
		if (token)
		{

			while (token)
			{
				bool		fetch_next;

				fetch_next = true;

				if (pg_strcasecmp(token, "header") == 0)
					pstate->header_line = true;
				else if (pg_strcasecmp(token, "delimiter") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
					{
						pstate->delim = pstrdup(token);

						if (pg_strcasecmp(pstate->delim, "off") == 0)
							pstate->delimiter_off = true;
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "null") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
					{
						pstate->null_print = pstrdup(token);
						pstate->null_print_len = strlen(token);
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "quote") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
						pstate->quote = pstrdup(token);
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "escape") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
					{
						pstate->escape = pstrdup(token);

						if (pg_strcasecmp(pstate->escape, "off") == 0)
							pstate->escape_off = true;
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "force") == 0)
				{
					token = strtokx2(NULL, whitespace, ",", "\"",
									 0, false, false, encoding);
					if (pg_strcasecmp(token, "not") == 0)
					{
						token = strtokx2(NULL, whitespace, ",", "\"",
										 0, false, false, encoding);
						if (pg_strcasecmp(token, "null") != 0)
							goto error;
						/* handle column list */
						fetch_next = false;
						for (;;)
						{
							Value	   *val;

							token = strtokx2(NULL, whitespace, ",", "\"",
											 0, false, false, encoding);
							if (!token || strchr(",", token[0]))
								goto error;

							val = makeString((char *) pstrdup(token));
							pstate->force_notnull = lappend(pstate->force_notnull, val);

							/* consume the comma if any */
							token = strtokx2(NULL, whitespace, ",", "\"",
											 0, false, false, encoding);
							if (!token || token[0] != ',')
								break;
						}
					}
					else if (pg_strcasecmp(token, "quote") == 0)
					{
						fetch_next = false;
						for (;;)
						{
							Value	   *val;

							token = strtokx2(NULL, whitespace, ",", "\"",
											 0, false, false, encoding);
							if (!token || strchr(",", token[0]))
								goto error;

							val = makeString((char *) pstrdup(token));
							pstate->force_quote = lappend(pstate->force_quote, val);

							/* consume the comma if any */
							token = strtokx2(NULL, whitespace, ",", "\"",
											 0, false, false, encoding);
							if (!token || token[0] != ',')
								break;
						}
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "fill") == 0)
				{
					token = strtokx2(NULL, whitespace, ",", "\"",
									 0, false, false, encoding);
					if (pg_strcasecmp(token, "missing") == 0)
					{
						token = strtokx2(NULL, whitespace, ",", "\"",
										 0, false, false, encoding);
						if (pg_strcasecmp(token, "fields") == 0)
						{
							pstate->fill_missing = true;
						}
						else
							goto error;
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "newline") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
					{
						/*
						 * if NEWLINE was specified in exttab def, set
						 * eol_type now
						 */
						pstate->eol_str = pstrdup(token);
						CopyEolStrToType(pstate);
					}
					else
						goto error;
				}
				else if (pg_strcasecmp(token, "formatter") == 0)
				{
					token = strtokx2(NULL, whitespace, NULL, "'",
									 nonstd_backslash, true, true, encoding);
					if (token)
						pstate->custom_formatter_name = pstrdup(token);
					else
						goto error;
				}
				else
					goto error;

				if (fetch_next)
					token = strtokx2(NULL, whitespace, NULL, NULL,
									 0, false, false, encoding);
			}
		}

		/* set defaults */

		if (pstate->csv_mode)
		{
			if (!pstate->quote)
				pstate->quote = "\"";
		}
	}
	else
	{
		/* parse user custom options. take it as is. no validation needed */

		List	   *l = NIL;
		bool		formatter_found = false;

		if (token)
		{
			char	   *key = token;
			char	   *val = NULL;
			StringInfoData key_modified;

			initStringInfo(&key_modified);

			while (key)
			{
				/* MPP-14467 - replace meta chars back to original */
				resetStringInfo(&key_modified);
				appendStringInfoString(&key_modified, key);
				replaceStringInfoString(&key_modified, "<gpx20>", " ");

				val = strtokx2(NULL, whitespace, NULL, "'",
							   nonstd_backslash, true, true, encoding);
				if (val)
				{

					if (pg_strcasecmp(key, "formatter") == 0)
					{
						pstate->custom_formatter_name = pstrdup(val);
						formatter_found = true;
					}
					else
						l = lappend(l, makeDefElem(pstrdup(key_modified.data),
										 (Node *) makeString(pstrdup(val))));
				}
				else
					goto error;

				key = strtokx2(NULL, whitespace, NULL, NULL,
							   0, false, false, encoding);
			}

		}

		if (!formatter_found)
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("external table internal parse error: "
								   "no formatter function name found")));

		pstate->custom_formatter_params = l;
	}

#if 0
	elog(NOTICE, "delim %s null %s escape %s quote %s %s %s", pstate->delim,
	  pstate->null_print, pstate->escape, pstate->quote, (pstate->header_line
	  ? "header" : ""), (pstate->fill_missing ? "fill missing fields" : ""));
#endif

	return;

error:
	if (token)
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
					  errmsg("external table internal parse error at \"%s\"",
							 token)));
	else
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
					  errmsg("external table internal parse error at end of "
							 "line")));

}

static char *
get_eol_delimiter(List *params)
{
	ListCell   *lc = params->head;

	while (lc)
	{
		if (pg_strcasecmp(((DefElem *) lc->data.ptr_value)->defname, "line_delim") == 0)
			return pstrdup(((Value *) ((DefElem *) lc->data.ptr_value)->arg)->val.str);
		lc = lc->next;
	}
	return pstrdup("");
}

static void
base16_encode(char *raw, int len, char *encoded)
{
	const char *raw_bytes = raw;
	char	   *encoded_bytes = encoded;
	int			remaining = len;

	for (; remaining--; encoded_bytes += 2)
	{
		sprintf(encoded_bytes, "%02x", *(raw_bytes++));
	}
}

void
external_set_env_vars(extvar_t *extvar, char *uri, bool csv, char *escape, char *quote, bool header, uint32 scancounter)
{
	external_set_env_vars_ext(extvar, uri, csv, escape, quote, EOL_UNKNOWN, header, scancounter, NULL);
}

static void
external_set_env_vars_ext(extvar_t *extvar, char *uri, bool csv, char *escape, char *quote, int eol_type, bool header,
						  uint32 scancounter, List *params)
{
	time_t		now = time(0);
	struct tm  *tm = localtime(&now);
	char	   *result = (char *) palloc(7);	/* sign, 5 digits, '\0' */

	char	   *encoded_delim;
	int			line_delim_len;

	sprintf(extvar->GP_CSVOPT,
			"m%dx%dq%dn%dh%d",
			csv ? 1 : 0,
			escape ? 255 & *escape : 0,
			quote ? 255 & *quote : 0,
			eol_type,
			header ? 1 : 0);

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		pg_ltoa(qdPostmasterPort, result);
		extvar->GP_MASTER_PORT = result;
		extvar->GP_MASTER_HOST = qdHostname;
	}
	else
	{
		CdbComponentDatabases *cdb_component_dbs = getCdbComponentDatabases();
		CdbComponentDatabaseInfo *qdinfo = &cdb_component_dbs->entry_db_info[0];

		pg_ltoa(qdinfo->port, result);
		extvar->GP_MASTER_PORT = result;

		if (qdinfo->hostip != NULL)
			extvar->GP_MASTER_HOST = pstrdup(qdinfo->hostip);
		else
			extvar->GP_MASTER_HOST = pstrdup(qdinfo->hostname);

		freeCdbComponentDatabases(cdb_component_dbs);
	}

	if (MyProcPort)
		extvar->GP_USER = MyProcPort->user_name;
	else
		extvar->GP_USER = "";

	extvar->GP_DATABASE = get_database_name(MyDatabaseId);
	extvar->GP_SEG_PG_CONF = ConfigFileName;	/* location of the segments
												 * pg_conf file  */
	extvar->GP_SEG_DATADIR = data_directory;	/* location of the segments
												 * datadirectory */
	sprintf(extvar->GP_DATE, "%04d%02d%02d",
			1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday);
	sprintf(extvar->GP_TIME, "%02d%02d%02d",
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	if (!getDistributedTransactionIdentifier(extvar->GP_XID))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("cannot get distributed transaction identifier while %s", uri)));

	sprintf(extvar->GP_CID, "%x", QEDtxContextInfo.curcid);
	sprintf(extvar->GP_SN, "%x", scancounter);
	sprintf(extvar->GP_SEGMENT_ID, "%d", Gp_segment);
	sprintf(extvar->GP_SEG_PORT, "%d", PostPortNumber);
	sprintf(extvar->GP_SESSION_ID, "%d", gp_session_id);
	sprintf(extvar->GP_SEGMENT_COUNT, "%d", GpIdentity.numsegments);

	/*
	 * Hadoop Connector env var
	 *
	 * Those has to be set into the env because the gphdfs env setup script
	 * (hadoop_env.sh) relies on those to set the classpath to the connector
	 * jar as well as the Hadoop jar.
	 *
	 * Setting these var here (instead of inside gphdfs protocol) allows
	 * ordinary "execute" external table to run hadoop connector jar for other
	 * purposes.
	 */
	extvar->GP_HADOOP_CONN_JARDIR = gp_hadoop_connector_jardir;
	extvar->GP_HADOOP_CONN_VERSION = gp_hadoop_connector_version;
	extvar->GP_HADOOP_HOME = gp_hadoop_home;

	if (NULL != params)
	{
		char	   *line_delim_str = get_eol_delimiter(params);

		line_delim_len = (int) strlen(line_delim_str);
		if (line_delim_len > 0)
		{
			encoded_delim = (char *) (palloc(line_delim_len * 2 + 1));
			base16_encode(line_delim_str, line_delim_len, encoded_delim);
		}
		else
		{
			line_delim_len = -1;
			encoded_delim = "";
		}
	}
	else
	{
		switch(eol_type)
		{
			case EOL_CR:
				encoded_delim = "0D";
				line_delim_len = 1;
				break;
			case EOL_LF:
				encoded_delim = "0A";
				line_delim_len = 1;
				break;
			case EOL_CRLF:
				encoded_delim = "0D0A";
				line_delim_len = 2;
				break;
			default:
				encoded_delim = "";
				line_delim_len = -1;
				break;
		}
	}
	extvar->GP_LINE_DELIM_STR = pstrdup(encoded_delim);
	sprintf(extvar->GP_LINE_DELIM_LENGTH, "%d", line_delim_len);
}
