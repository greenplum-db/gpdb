/*-------------------------------------------------------------------------
 *
 * extaccess.c
 *	  external table access method routines
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
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_exttable_fdw/extaccess.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "extaccess.h"

#include "access/external.h"
#include "access/formatter.h"
#include "access/heapam.h"
#include "access/url.h"
#include "access/valid.h"
#include "catalog/pg_proc.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "nodes/makefuncs.h"
#include "pgstat.h"
#include "parser/parse_func.h"
#include "utils/relcache.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static HeapTuple externalgettup(FileScanDesc scan, ScanDirection dir);
static void InitParseState(CopyState pstate, Relation relation,
			   bool writable,
			   char fmtType,
			   char *uri, int rejectlimit,
			   bool islimitinrows, char logerrors);

static void FunctionCallPrepareFormatter(FunctionCallInfoBaseData *fcinfo,
							 int nArgs,
							 CopyState pstate,
							 FmgrInfo *formatter_func,
							 List *formatter_params,
							 FormatterData *formatter,
							 Relation rel,
							 TupleDesc tupDesc,
							 FmgrInfo *convFuncs,
							 Oid *typioparams);

static void open_external_readable_source(FileScanDesc scan, ExternalSelectDesc desc);
static void open_external_writable_source(ExternalInsertDesc extInsertDesc);
static int	external_getdata_callback(void *outbuf, int minread, int maxread, void *extra);
static int	external_getdata(URL_FILE *extfile, CopyState pstate, void *outbuf, int maxread);
static void external_senddata(URL_FILE *extfile, CopyState pstate);
static void external_scan_error_callback(void *arg);
static Oid lookupCustomFormatter(List **options, bool iswritable);
static void justifyDatabuf(StringInfo buf);


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
external_beginscan(Relation relation, uint32 scancounter,
				   List *uriList, char fmtType, bool isCoordinatorOnly,
				   int rejLimit, bool rejLimitInRows, char logErrors, int encoding,
				   List *extOptions)
{
	FileScanDesc scan;
	TupleDesc	tupDesc = NULL;
	int			attnum;
	int			segindex = GpIdentity.segindex;
	char	   *uri = NULL;
	List	   *custom_formatter_params = NIL;

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
	scan = (FileScanDesc) palloc0(sizeof(FileScanDescData));

	scan->fs_ctup.t_data = NULL;
	ItemPointerSetInvalid(&scan->fs_ctup.t_self);
	scan->fs_rd = relation;
	scan->fs_scancounter = scancounter;
	scan->fs_noop = false;
	scan->fs_file = NULL;
	scan->fs_formatter = NULL;
	scan->fs_constraintExprs = NULL;
	if (relation->rd_att->constr != NULL && relation->rd_att->constr->num_check > 0)
		scan->fs_hasConstraints = true;
	else
		scan->fs_hasConstraints = false;

	scan->fs_isPartition = relation->rd_rel->relispartition;


	/*
	 * get the external URI assigned to us.
	 *
	 * The URI assigned for this segment is normally in the uriList list at
	 * the index of this segment id. However, if we are executing on COORDINATOR
	 * ONLY the (one and only) entry which is destined for the coordinator will be
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
		if (isCoordinatorOnly && idx == -1)
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
	else if (Gp_role == GP_ROLE_DISPATCH && isCoordinatorOnly)
	{
		/* this is a ON COORDINATOR table. Only get uri if we are the coordinator */
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
	//scan->attr = tupDesc->attrs;
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
		Form_pg_attribute attr = TupleDescAttr(scan->fs_tupDesc, attnum - 1);

		if (attr->attisdropped)
			continue;

		getTypeInputInfo(attr->atttypid,
						 &scan->in_func_oid, &scan->typioparams[attnum - 1]);
		fmgr_info(scan->in_func_oid, &scan->in_functions[attnum - 1]);
	}

	custom_formatter_params = list_copy(extOptions);

	/*
	 * pass external table's encoding to copy's options
	 *
	 * don't append to extentry->options directly, we only store the encoding in
	 * extentry->encoding (and ftoptions)
	 */
	if (fmttype_is_custom(fmtType))
	{
		extOptions = NIL;
	}
	extOptions = appendCopyEncodingOption(list_copy(extOptions), encoding);

	/*
	 * Allocate and init our structure that keeps track of data parsing state
	 */
	scan->fs_pstate = BeginCopyFrom(NULL,
									relation, NULL, false,
									external_getdata_callback,
									(void *) scan,
									NIL,
									extOptions);

	if (scan->fs_pstate->header_line && Gp_role == GP_ROLE_DISPATCH)
	{
		ereport(NOTICE,
				(errmsg("HEADER means that each one of the data files has a header row")));
	}

	/* Initialize all the parsing and state variables */
	InitParseState(scan->fs_pstate, relation, false, fmtType,
				   scan->fs_uri, rejLimit, rejLimitInRows, logErrors);

	if (fmttype_is_custom(fmtType))
	{
		/*
		 * Custom format: get formatter name and find it in the catalog
		 */
		Oid			procOid;

		/* parseFormatString should have seen a formatter name */
		procOid = lookupCustomFormatter(&custom_formatter_params, false);

		/* we found our function. set it up for calling */
		scan->fs_custom_formatter_func = palloc(sizeof(FmgrInfo));
		fmgr_info(procOid, scan->fs_custom_formatter_func);
		scan->fs_custom_formatter_params = custom_formatter_params;

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

	if (!scan->fs_pstate)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("The file parse state of external scan is invalid")));

	/* reset some parse state variables */
	scan->fs_pstate->reached_eof = false;
	scan->fs_pstate->cur_lineno = 0;
	scan->fs_pstate->cur_attname = NULL;
	scan->fs_pstate->raw_buf_len = 0;
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

/*	----------------
 *		external_getnext_init - prepare ExternalSelectDesc struct before external_getnext
 *	----------------
 */
ExternalSelectDesc
external_getnext_init(PlanState *state)
{
	ExternalSelectDesc
		desc = (ExternalSelectDesc) palloc0(sizeof(ExternalSelectDescData));
	if (state != NULL)
		desc->projInfo = state->ps_ProjInfo;
	return desc;
}

/* ----------------------------------------------------------------
*		external_getnext
*
*		Parse a data file and return its rows in heap tuple form
* ----------------------------------------------------------------
*/
HeapTuple
external_getnext(FileScanDesc scan, ScanDirection direction, ExternalSelectDesc desc)
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
		open_external_readable_source(scan, desc);

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
	List	   *copyFmtOpts;
	List	   *custom_formatter_params = NIL;
	char       *on_clause;

	/*
	 * Get the ExtTableEntry information for this table
	 */
	extentry = GetExtTableEntry(RelationGetRelid(rel));
	on_clause = (char *) strVal(linitial(extentry->execlocations));

	/*
	 * allocate and initialize the insert descriptor
	 */
	extInsertDesc = (ExternalInsertDesc) palloc0(sizeof(ExternalInsertDescData));
	extInsertDesc->ext_rel = rel;
	if (strcmp(on_clause, "COORDINATOR_ONLY") == 0)
		extInsertDesc->ext_noop = false;
	else
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
		int			num_segs = getgpsegmentCount();
		int			num_urls = list_length(extentry->urilocations);
		int			my_url = segindex % num_urls;

		if (num_urls > num_segs)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("external table has more URLs than available primary segments that can write into them")));

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

	/*
	 * Writing to an external table is like COPY TO: we get tuples from the executor,
	 * we format them into the format requested, and write the output to an external
	 * sink.
	 */

	custom_formatter_params = list_copy(extentry->options);

	/*
	 * pass external table's encoding to copy's options
	 *
	 * don't append to extentry->options directly, we only store the encoding in
	 * extentry->encoding (and ftoptions)
	 */
	if (fmttype_is_custom(extentry->fmtcode))
	{
		copyFmtOpts = NIL;
	}
	copyFmtOpts = appendCopyEncodingOption(list_copy(extentry->options), extentry->encoding);

	extInsertDesc->ext_pstate = BeginCopyToForeignTable(rel, copyFmtOpts);
	InitParseState(extInsertDesc->ext_pstate,
				   rel,
				   true,
				   extentry->fmtcode,
				   extInsertDesc->ext_uri,
				   extentry->rejectlimit,
				   (extentry->rejectlimittype == 'r'),
				   extentry->logerrors);

	if (fmttype_is_custom(extentry->fmtcode))
	{
		/*
		 * Custom format: get formatter name and find it in the catalog
		 */
		Oid			procOid;

		procOid = lookupCustomFormatter(&custom_formatter_params, true);

		/* we found our function. set it up for calling  */
		extInsertDesc->ext_custom_formatter_func = palloc(sizeof(FmgrInfo));
		fmgr_info(procOid, extInsertDesc->ext_custom_formatter_func);
		extInsertDesc->ext_custom_formatter_params = custom_formatter_params;

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
void
external_insert(ExternalInsertDesc extInsertDesc, TupleTableSlot *slot)
{
	TupleDesc	tupDesc = extInsertDesc->ext_tupDesc;
	CopyStateData *pstate = extInsertDesc->ext_pstate;
	bool		customFormat = (extInsertDesc->ext_custom_formatter_func != NULL);

	if (extInsertDesc->ext_noop)
		return;

	/* Open our output file or output stream if not yet open */
	if (!extInsertDesc->ext_file && !extInsertDesc->ext_noop)
		open_external_writable_source(extInsertDesc);

	/*
	 * deconstruct the tuple and format it into text
	 */
	if (!customFormat)
	{
		/* TEXT or CSV */
		CopyOneRowTo(pstate, slot);
		CopySendEndOfRow(pstate);
	}
	else
	{
		/* custom format. convert tuple using user formatter */
		Datum		d;
		bytea	   *b;
		LOCAL_FCINFO(fcinfo, 1);
		HeapTuple instup;

		/*
		 * There is some redundancy between FormatterData and
		 * ExternalInsertDesc we may be able to consolidate data structures a
		 * little.
		 */
		FormatterData *formatter = extInsertDesc->ext_formatter_data;

		/* must have been created during insert_init */
		Assert(formatter);

		/* per call formatter prep */
		FunctionCallPrepareFormatter(fcinfo,
									 1,
									 pstate,
									 extInsertDesc->ext_custom_formatter_func,
									 extInsertDesc->ext_custom_formatter_params,
									 formatter,
									 extInsertDesc->ext_rel,
									 extInsertDesc->ext_tupDesc,
									 pstate->out_functions,
									 NULL);

		/* Mark the correct record type in the passed tuple */

		/*
		 * get the heap tuple out of the tuple table slot, making sure we have a
		 * writable copy. (the function can scribble on the tuple)
		 */
		instup = ExecCopySlotHeapTuple(slot);

		HeapTupleHeaderSetDatumLength(instup->t_data, instup->t_len);
		HeapTupleHeaderSetTypeId(instup->t_data, tupDesc->tdtypeid);
		HeapTupleHeaderSetTypMod(instup->t_data, tupDesc->tdtypmod);

		fcinfo->args[0].value = HeapTupleGetDatum(instup);
		fcinfo->args[0].isnull = false;

		d = FunctionCallInvoke(fcinfo);
		MemoryContextReset(formatter->fmt_perrow_ctx);

		/* We do not expect a null result */
		if (fcinfo->isnull)
			elog(ERROR, "function %u returned NULL", fcinfo->flinfo->fn_oid);

		b = DatumGetByteaP(d);

		CopyOneCustomRowTo(pstate, b);

		heap_freetuple(instup);
	}

	/* Write the data into the external source */
	external_senddata(extInsertDesc->ext_file, pstate);

	/* Reset our buffer to start clean next round */
	pstate->fe_msgbuf->len = 0;
	pstate->fe_msgbuf->data[0] = '\0';
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
	pstate->cdbsreh->rawdata->cursor = 0; \
	pstate->cdbsreh->rawdata->data = pstate->line_buf.data; \
	pstate->cdbsreh->rawdata->len = pstate->line_buf.len; \
	pstate->cdbsreh->is_server_enc = pstate->line_buf_converted; \
	pstate->cdbsreh->linenumber = pstate->cur_lineno; \
	pstate->cdbsreh->processed++; \
\
	/* set the error message. Use original msg and add column name if available */ \
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

static HeapTuple
externalgettup_defined(FileScanDesc scan)
{
	HeapTuple	tuple = NULL;
	CopyState	pstate = scan->fs_pstate;
	MemoryContext oldcontext;

	MemoryContextReset(pstate->rowcontext);
	oldcontext = MemoryContextSwitchTo(pstate->rowcontext);

	/* Get a line */
	if (!NextCopyFrom(pstate,
					  NULL,
					  scan->values,
					  scan->nulls))
	{
		MemoryContextSwitchTo(oldcontext);
		return NULL;
	}

	MemoryContextSwitchTo(oldcontext);

	/* convert to heap tuple */

	/*
	 * XXX This is bad code.  Planner should be able to decide
	 * whether we need heaptuple or memtuple upstream, so make the
	 * right decision here.
	 */
	tuple = heap_form_tuple(scan->fs_tupDesc, scan->values, scan->nulls);

	return tuple;
}

static HeapTuple
externalgettup_custom(FileScanDesc scan)
{
	HeapTuple	tuple;
	CopyState	pstate = scan->fs_pstate;
	FormatterData *formatter = scan->fs_formatter;
	MemoryContext oldctxt = CurrentMemoryContext;
	bool need_more_data = false;
	bool infinite_loop_detect = false;

	Assert(formatter);

	/* while didn't finish processing the entire file */
	while (formatter->fmt_databuf.len != 0 || !pstate->reached_eof)
	{
		if (infinite_loop_detect) {
			break;
		}
		/* need to fill our buffer with data? */
		if (formatter->fmt_databuf.len == 0 || need_more_data)
		{
			int			bytesread = external_getdata(scan->fs_file, pstate, pstate->raw_buf, RAW_BUF_SIZE);

			if (bytesread > 0)
			{
				appendBinaryStringInfo(&formatter->fmt_databuf, pstate->raw_buf, bytesread);
				need_more_data = false;
			}

			/* HEADER not yet supported ... */
			if (pstate->header_line)
				elog(ERROR, "header line in custom format is not yet supported");
		}

		/* while there is still data in our buffer */
		while (formatter->fmt_databuf.len > 0)
		{
			bool		error_caught = false;

			/*
			 * Invoke the custom formatter function.
			 */
			PG_TRY();
			{
				LOCAL_FCINFO(fcinfo, 0);

				/* per call formatter prep */
				FunctionCallPrepareFormatter(fcinfo,
						0,
						pstate,
						scan->fs_custom_formatter_func,
						scan->fs_custom_formatter_params,
						formatter,
						scan->fs_rd,
						scan->fs_tupDesc,
						scan->in_functions,
						scan->typioparams);
				(void) FunctionCallInvoke(fcinfo);

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
				FlushErrorState();

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

						if (pstate->cdbsreh)
							pstate->cdbsreh->processed++;

						MemoryContextReset(formatter->fmt_perrow_ctx);

						return tuple;

					case FMT_NEED_MORE_DATA:

						/*
						 * Callee consumed all data in the buffer. Prepare
						 * to read more data into it.
						 */
						if (pstate->reached_eof) {
							infinite_loop_detect = true;
						}
						need_more_data = true;
						justifyDatabuf(&formatter->fmt_databuf);
						break;

					default:
						elog(ERROR, "unsupported formatter notification (%d)",
								formatter->fmt_notification);
						break;
				}
				/* 
				 * We can only get here when (formatter->fmt_notification == FMT_NEED_MORE_DATA).
				 * We need to get more data from external source(call external_getdata()) 
				 */
				break;
			}
			else
			{
				ErrorIfRejectLimitReached(pstate->cdbsreh);
			}
		}
	}
	if (formatter->fmt_databuf.len > 0)
	{
		/*
		 * The formatter needs more data, but we have reached
		 * EOF. This is an error.
		 */
		ereport(WARNING,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("unexpected end of file")));
	}

	/*
	 * if we got here we finished reading all the data.
	 */

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
               ScanDirection dir pg_attribute_unused())
{
	bool		custom = (scan->fs_custom_formatter_func != NULL);
	HeapTuple	tup = NULL;
	ErrorContextCallback externalscan_error_context;

	Assert(ScanDirectionIsForward(dir));

	externalscan_error_context.callback = external_scan_error_callback;
	externalscan_error_context.arg = (void *) scan;
	externalscan_error_context.previous = error_context_stack;

	error_context_stack = &externalscan_error_context;

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
lookupCustomFormatter(List **options, bool iswritable)
{
	ListCell   *cell;
	ListCell   *prev = NULL;
	char	   *formatter_name;
	List	   *funcname = NIL;
	Oid			procOid = InvalidOid;
	Oid			argList[1];
	Oid			returnOid;

	/*
	 * The formatter is defined as a "formatter=<name>" tuple in the options
	 * array. Extract into a separate list in order to scan the catalog for
	 * the function definition.
	 */
	foreach(cell, *options)
	{
		DefElem *defel = (DefElem *) lfirst(cell);

		if (strcmp(defel->defname, "formatter") == 0)
		{
			formatter_name = defGetString(defel);
			funcname = list_make1(makeString(formatter_name));
			*options = list_delete_cell(*options, cell, prev);
			break;
		}
		prev = cell;
	}
	if (list_length(funcname) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("formatter function not found in table options")));

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
 * Initialize the data parsing state.
 *
 * This includes format descriptions (delimiter, quote...), format type
 * (text, csv), etc...
 */
static void
InitParseState(CopyState pstate, Relation relation,
			   bool iswritable,
			   char fmtType,
			   char *uri, int rejectlimit,
			   bool islimitinrows, char logerrors)
{
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
		if (IS_LOG_TO_FILE(logerrors))
		{
			/* errors into file */
			pstate->errMode = SREH_LOG;
		}
		else
		{
			/* no error log */
			pstate->errMode = SREH_IGNORE;
		}

		/* Single row error handling */
		pstate->cdbsreh = makeCdbSreh(rejectlimit,
									  islimitinrows,
									  uri,
									  (char *) pstate->cur_relname,
									  logerrors);

		pstate->cdbsreh->relid = RelationGetRelid(relation);
	}

	/* Initialize 'out_functions', like CopyTo() would. */
	CopyState cstate = pstate;
	TupleDesc tupDesc = RelationGetDescr(cstate->rel);
	int num_phys_attrs = tupDesc->natts;
	cstate->out_functions = (FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));
	ListCell *cur;
	foreach(cur, cstate->attnumlist)
	{
		int			attnum = lfirst_int(cur);
		Form_pg_attribute attr = TupleDescAttr(tupDesc, attnum - 1);
		Oid			out_func_oid;
		bool		isvarlena;

		if (cstate->binary)
			getTypeBinaryOutputInfo(attr->atttypid,
									&out_func_oid,
									&isvarlena);
		else
			getTypeOutputInfo(attr->atttypid,
							  &out_func_oid,
							  &isvarlena);
		fmgr_info(out_func_oid, &cstate->out_functions[attnum - 1]);
	}

	/* and 'fe_mgbuf' */
	cstate->fe_msgbuf = makeStringInfo();

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
FunctionCallPrepareFormatter(FunctionCallInfoBaseData *fcinfo,
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
	formatter->fmt_saw_eof = pstate->reached_eof;
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
 * open the external source for scanning (RET only)
 *
 * an external source is one of the following:
 * 1) a local file (requested by 'file')
 * 2) a remote http server
 * 3) a remote gpfdist server
 * 4) a command to execute
 */
static void
open_external_readable_source(FileScanDesc scan, ExternalSelectDesc desc)
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
							  scan->fs_custom_formatter_params);

	/* actually open the external source */
	scan->fs_file = url_fopen(scan->fs_uri,
							  false /* for read */ ,
							  &extvar,
							  scan->fs_pstate,
							  desc);
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
						 extInsertDesc->ext_custom_formatter_params);

	/* actually open the external source */
	extInsertDesc->ext_file = url_fopen(extInsertDesc->ext_uri,
										true /* forwrite */ ,
										&extvar,
										extInsertDesc->ext_pstate,
										NULL);
}

/*
 * Fetch more data from the source, and feed it to the COPY FROM machinery
 * for parsing.
 */
static int
external_getdata_callback(void *outbuf, int minread, int maxread, void *extra)
{
	FileScanDesc scan = (FileScanDesc) extra;

	return external_getdata(scan->fs_file, scan->fs_pstate, outbuf, maxread);
}

/*
 * get a chunk of data from the external data file.
 */
static int
external_getdata(URL_FILE *extfile, CopyState pstate, void *outbuf, int maxread)
{
	int			bytesread;

	/*
	 * CK: this code is very delicate. The caller expects this: - if url_fread
	 * returns something, and the EOF is reached, it this call must return
	 * with both the content and the reached_eof flag set. - failing to do so will
	 * result in skipping the last line. But for custom protocol, it is not possible
	 * to reach EOF when the bytesread > 0, so we need to give them a second
	 * chance to reach EOF when the bytesread = 0, so the formatter is responsible
	 * for dealing with eof flag properly, otherwise infinite loop may be created.
	 */
	bytesread = url_fread((void *) outbuf, maxread, extfile, pstate);

	if (url_feof(extfile, bytesread))
	{
		pstate->reached_eof = true;
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
		if (*ebuf && strlen(ebuf) > 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write to external resource: %s",
							ebuf)));
		else
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write to external resource: %m")));
	}
}

static char *
linenumber_atoi(char *buffer, size_t bufsz, int64 linenumber)
{
	if (linenumber < 0)
		snprintf(buffer, bufsz, "%s", "N/A");
	else
		snprintf(buffer, bufsz, INT64_FORMAT, linenumber);

	return buffer;
}

/*
 * error context callback for external table scan
 */
static void
external_scan_error_callback(void *arg)
{
	FileScanDesc scan = (FileScanDesc) arg;
	CopyState	cstate = scan->fs_pstate;
	char		buffer[20];

	/*
	 * early exit for custom format error. We don't have metadata to report
	 * on. TODO: this actually will override any errcontext that the user
	 * wants to set. maybe another approach is needed here.
	 */
	if (scan->fs_custom_formatter_func)
	{
		errcontext("External table %s", cstate->cur_relname);
		return;
	}

	if (cstate->cur_attname)
	{
		/* error is relevant to a particular column */

		errcontext("External table %s, line %s of %s, column %s",
				   cstate->cur_relname,
				   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
				   scan->fs_uri,
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
					   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
					   scan->fs_uri, line_buf);
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
						   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
						   scan->fs_uri);
			else
				errcontext("External table %s, file %s",
						   cstate->cur_relname, scan->fs_uri);
		}
	}
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

List *
appendCopyEncodingOption(List *copyFmtOpts, int encoding)
{
	return lappend(copyFmtOpts,
				   makeDefElem("encoding",
							   (Node *)makeString((char *)pg_encoding_to_char(encoding)),
							   -1));
}
