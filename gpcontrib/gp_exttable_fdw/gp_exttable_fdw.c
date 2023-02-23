/*-------------------------------------------------------------------------
 *
 * gp_exttable_fdw.c
 *	  Shim layer between legacy GPDB external table API and the Foreign
 *	  Data Wrapper API
 *
 * This implements the FDW routines expected by PostgreSQL backend code,
 * to plan and execute queries on external tables. The FDW routines
 * call into the corresponding legacy external table API functions to do
 * the real work, IterateForeignScan for example just calls
 * external_getnext().
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_exttable_fdw/gp_exttable_fdw.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "extaccess.h"
#include "fmgr.h"
#include "nodes/plannodes.h"

#include "access/external.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "access/table.h"
#include "cdb/cdbvars.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "commands/defrem.h"
#include "commands/copy.h"
#include "cdb/cdbsreh.h"
#include "foreign/fdwapi.h"
#include "funcapi.h"
#include "nodes/execnodes.h"
#include "nodes/makefuncs.h"
#include "nodes/pathnodes.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/partcache.h"
#include "utils/syscache.h"
#include "utils/uri.h"

PG_MODULE_MAGIC;

#define GP_EXTTABLE_ATTRNUM 12

extern Datum gp_exttable_fdw_handler(PG_FUNCTION_ARGS);
extern Datum gp_exttable_permission_check(PG_FUNCTION_ARGS);

extern Datum pg_exttable(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_exttable_fdw_handler);
PG_FUNCTION_INFO_V1(gp_exttable_permission_check);
PG_FUNCTION_INFO_V1(pg_exttable);

/*
 * PGExtTableEntry is used in pg_exttable(). It reflects each external
 * table entry in the foreign table catalog.
 */
typedef struct PGExtTableEntry
{
	Oid			reloid;
	Oid			serveroid;
	List	   *ftoptions;
} PGExtTableEntry;

/*
 * PGExtTableEntriesContext is used in pg_exttable() as user_fctx.
 */
typedef struct PGExtTableEntriesContext
{
	int			entryIdx;
	List	   *ftentries;
} PGExtTableEntriesContext;

typedef struct
{
	FileScanDescData *ess_ScanDesc;

	ExternalSelectDesc externalSelectDesc;

} exttable_fdw_state;

static void cost_externalscan(ForeignPath *path, PlannerInfo *root,
							  RelOptInfo *baserel, ParamPathInfo *param_info);

/*
 * strListToArray - String Value to Text array datum
 */
static Datum
strListToArray(List *stringlist)
{
	ArrayBuildState *astate = NULL;
	ListCell   *cell;

	foreach(cell, stringlist)
	{
		Value	   *val = lfirst(cell);
		astate = accumArrayResult(astate, CStringGetTextDatum(strVal(val)),
								  false, TEXTOID,
								  CurrentMemoryContext);
	}

	if (astate)
		return makeArrayResult(astate, CurrentMemoryContext);

	return PointerGetDatum(NULL);
}

/*
 * formatOptionsToTextDatum
 * Convert format options to text datum. The text datum format is same with
 * the original pg_exttable catalog's fmtopt field.
 */
static Datum
formatOptionsToTextDatum(List *options, char formattype)
{
	ListCell   *option;
	Datum		result;
	StringInfoData cfbuf;

	initStringInfo(&cfbuf);
	bool isfirst = true;
	if (fmttype_is_text(formattype) || fmttype_is_csv(formattype))
	{
		/*
		 * Note: the order of the options should be same with the original
		 * pg_exttable catalog's fmtopt field.
		 */
		foreach(option, options)
		{
			DefElem    *defel = (DefElem *) lfirst(option);
			char	   *key = defel->defname;
			char	   *val = (char *) defGetString(defel);

			if (strcmp(defel->defname, "format") == 0)
				continue;

			if (isfirst)
				isfirst = false;
			else
				appendStringInfo(&cfbuf, " ");

			if (strcmp(defel->defname, "header") == 0)
				appendStringInfo(&cfbuf, "header");
			else if (strcmp(defel->defname, "fill_missing_fields") == 0)
				appendStringInfo(&cfbuf, "fill missing fields");
			else if (strcmp(defel->defname, "force_not_null") == 0)
				appendStringInfo(&cfbuf, "force not null %s", val);
			else if (strcmp(defel->defname, "force_quote") == 0)
				appendStringInfo(&cfbuf, "force quote %s", val);
			else
				appendStringInfo(&cfbuf, "%s '%s'", key, val);
		}
	}
	else
	{
		foreach(option, options)
		{
			DefElem    *defel = (DefElem *) lfirst(option);
			char	   *key = defel->defname;
			char	   *val = (char *) defGetString(defel);

			if (isfirst)
				isfirst = false;
			else
				appendStringInfo(&cfbuf, " ");

			appendStringInfo(&cfbuf, "%s '%s'", key, val);
		}
	}
	result = CStringGetTextDatum(cfbuf.data);
	pfree(cfbuf.data);

	return result;
}

/*
 * Use the pg_exttable UDF to extract pg_exttable catalog info for
 * extension compatibility.
 *
 * pg_exttable catalog was removed because we use the FDW to implement
 * external table now. But other extensions may still rely on the pg_exttable
 * catalog. So we create a view base on this UDF to extract pg_exttable catalog
 * info.
 */
Datum pg_exttable(PG_FUNCTION_ARGS)
{
	FuncCallContext			   *funcctx;
	PGExtTableEntriesContext   *context;
	Datum						values[GP_EXTTABLE_ATTRNUM];
	bool						nulls[GP_EXTTABLE_ATTRNUM] = {false};

	/*
	 * First call setup
	 */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;
		Relation		pg_foreign_table_rel;
		ScanKeyData		ftkey;
		SysScanDesc		ftscan;
		HeapTuple		fttuple;
		Form_pg_foreign_table fttableform;
		List		   *ftentries = NIL;
		Oid				extserver;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Build tuple descriptor */
		TupleDesc	tupdesc = CreateTemplateTupleDesc(GP_EXTTABLE_ATTRNUM);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "reloid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "urilocation", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "execlocation", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "fmttype", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "fmtopts", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "options", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "command", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "rejectlimit", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "rejectlimittype", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "logerrors", BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "encoding", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 12, "writable", BOOLOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		extserver = get_foreign_server_oid(GP_EXTTABLE_SERVER_NAME, false);

		/* Retrieve external table in foreign table catalog */
		pg_foreign_table_rel = table_open(ForeignTableRelationId, AccessShareLock);

		ScanKeyInit(&ftkey,
					Anum_pg_foreign_table_ftserver,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(extserver));

		ftscan = systable_beginscan(pg_foreign_table_rel, InvalidOid,
									false, NULL, 1, &ftkey);

		while (HeapTupleIsValid(fttuple = systable_getnext(ftscan)))
		{
			Datum	ftoptions;
			bool	isNull;
			PGExtTableEntry *entry= (PGExtTableEntry *) palloc0(sizeof(PGExtTableEntry));
			fttableform = (Form_pg_foreign_table) GETSTRUCT(fttuple);
			entry->reloid = fttableform->ftrelid;
			entry->serveroid = fttableform->ftserver;
			/* get the foreign table options */
			ftoptions = heap_getattr(fttuple,
									 Anum_pg_foreign_table_ftoptions,
									 RelationGetDescr(pg_foreign_table_rel),
									 &isNull);
			if (!isNull)
				entry->ftoptions = untransformRelOptions(ftoptions);
			ftentries = lappend(ftentries, entry);
		}
		systable_endscan(ftscan);
		table_close(pg_foreign_table_rel, AccessShareLock);

		context = (PGExtTableEntriesContext *)palloc0(sizeof(PGExtTableEntriesContext));
		context->entryIdx = 0;
		context->ftentries = ftentries;
		funcctx->user_fctx = (void *) context;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (PGExtTableEntriesContext *) funcctx->user_fctx;
	while (context->entryIdx < list_length(context->ftentries))
	{
		PGExtTableEntry	   *entry;
		ExtTableEntry	   *extentry;
		Datum				datum;
		HeapTuple			tuple;
		Datum				result;

		entry = (PGExtTableEntry *)list_nth(context->ftentries, context->entryIdx);
		context->entryIdx++;
		extentry = GetExtFromForeignTableOptions(entry->ftoptions, entry->reloid);

		memset(values, 0, sizeof(values));
		memset(nulls, 0, sizeof(nulls));

		/* reloid */
		values[0] = ObjectIdGetDatum(entry->reloid);

		/* urilocations */
		datum = strListToArray(extentry->urilocations);
		if (DatumGetPointer(datum) != NULL)
			values[1] = datum;
		else
			nulls[1] = true;

		/* execlocations */
		datum = strListToArray(extentry->execlocations);
		if (DatumGetPointer(datum) != NULL)
			values[2] = datum;
		else
			nulls[2] = true;

		/* fmtcode */
		values[3] = CharGetDatum(extentry->fmtcode);

		/* fmtopts */
		if (extentry->options)
			values[4] = formatOptionsToTextDatum(extentry->options, extentry->fmtcode);
		else
			nulls[4] = true;

		/*
		 * options. Since our document not contains the OPTION caluse, so we
		 * assume no external table options in used for now.  Except
		 * gpextprotocol.c.
		 */
		nulls[5] = true;

		/* command */
		if (extentry->command)
			values[6] = CStringGetTextDatum(extentry->command);
		else
			nulls[6] = true;

		/* rejectlimit */
		values[7] = Int32GetDatum(extentry->rejectlimit);
		if (values[7] == -1)
			nulls[7] = true;

		/* rejectlimittype */
		values[8] = CharGetDatum(extentry->rejectlimittype);
		if (values[8] == -1)
			nulls[8] = true;

		/* logerrors */
		if IS_LOG_TO_FILE(extentry->logerrors)
		{
			values[9] = BoolGetDatum(true);
		}
		else
		{
			values[9] = BoolGetDatum(false);
		}
		
		/* encoding */
		values[10] = Int32GetDatum(extentry->encoding);

		/* iswritable */
		values[11] = BoolGetDatum(extentry->iswritable);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

/* FDW validator for external tables */
Datum
gp_exttable_permission_check(PG_FUNCTION_ARGS)
{
	List	   *options_list = untransformRelOptions(PG_GETARG_DATUM(0));

	if (!superuser() && Gp_role == GP_ROLE_DISPATCH)
	{
		/*----------
		 * check permissions to create this external table.
		 *
		 * - Always allow if superuser.
		 * - Never allow EXECUTE or 'file' exttables if not superuser.
		 * - Allow http, gpfdist or gpfdists tables if pg_auth has the right
		 *	 permissions for this role and for this type of table
		 *----------
		 */
		ListCell *lc;
		bool iswritable = false;
		foreach(lc, options_list)
		{
			DefElem    *def = (DefElem *) lfirst(lc);

			if (pg_strcasecmp(def->defname, "command") == 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("must be superuser to create an EXECUTE external web table")));
			}
			else if (pg_strcasecmp(def->defname, "is_writable") == 0)
			{
				iswritable = defGetBoolean(def);
			}
		}

		foreach(lc, options_list)
		{
			DefElem    *def = (DefElem *) lfirst(lc);

			if (pg_strcasecmp(def->defname, "location_uris") == 0)
			{
				List *location_list = TokenizeLocationUris(defGetString(def));
				ListCell   *first_uri = list_head(location_list);
				Value	   *v = lfirst(first_uri);
				char	   *uri_str = pstrdup(v->val.str);
				Uri		   *uri = ParseExternalTableUri(uri_str);

				/* Assert(exttypeDesc->exttabletype == EXTTBL_TYPE_LOCATION); */

				if (uri->protocol == URI_FILE)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("must be superuser to create an external table with a file protocol")));
				}
				else
				{
					/*
					 * Check if this role has the proper 'gpfdist', 'gpfdists' or
					 * 'http' permissions in pg_auth for creating this table.
					 */

					bool		isnull;
					HeapTuple	tuple;

					tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(GetUserId()));
					if (!HeapTupleIsValid(tuple))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("role \"%s\" does not exist (in DefineExternalRelation)",
										GetUserNameFromId(GetUserId(), false))));

					if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && iswritable)
					{
						Datum	 	d_wextgpfd;
						bool		createwextgpfd;

						d_wextgpfd = SysCacheGetAttr(AUTHOID, tuple,
													 Anum_pg_authid_rolcreatewextgpfd,
													 &isnull);
						createwextgpfd = (isnull ? false : DatumGetBool(d_wextgpfd));

						if (!createwextgpfd)
							ereport(ERROR,
									(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
									 errmsg("permission denied: no privilege to create a writable gpfdist(s) external table")));
					}
					else if ((uri->protocol == URI_GPFDIST || uri->protocol == URI_GPFDISTS) && !iswritable)
					{
						Datum		d_rextgpfd;
						bool		createrextgpfd;

						d_rextgpfd = SysCacheGetAttr(AUTHOID, tuple,
													 Anum_pg_authid_rolcreaterextgpfd,
													 &isnull);
						createrextgpfd = (isnull ? false : DatumGetBool(d_rextgpfd));

						if (!createrextgpfd)
							ereport(ERROR,
									(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
									 errmsg("permission denied: no privilege to create a readable gpfdist(s) external table")));
					}
					else if (uri->protocol == URI_HTTP && !iswritable)
					{
						Datum		d_exthttp;
						bool		createrexthttp;

						d_exthttp = SysCacheGetAttr(AUTHOID, tuple,
													Anum_pg_authid_rolcreaterexthttp,
													&isnull);
						createrexthttp = (isnull ? false : DatumGetBool(d_exthttp));

						if (!createrexthttp)
							ereport(ERROR,
									(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
									 errmsg("permission denied: no privilege to create an http external table")));
					}
					else if (uri->protocol == URI_CUSTOM)
					{
						Oid			ownerId = GetUserId();
						char	   *protname = uri->customprotocol;
						Oid			ptcId = get_extprotocol_oid(protname, false);
						AclResult	aclresult;

						/* Check we have the right permissions on this protocol */
						if (!pg_extprotocol_ownercheck(ptcId, ownerId))
						{
							AclMode		mode = (iswritable ? ACL_INSERT : ACL_SELECT);

							aclresult = pg_extprotocol_aclcheck(ptcId, ownerId, mode);

							if (aclresult != ACLCHECK_OK)
								aclcheck_error(aclresult, OBJECT_EXTPROTOCOL, protname);
						}
					}
					else
						ereport(ERROR,
								(errcode(ERRCODE_INTERNAL_ERROR),
								 errmsg("internal error in DefineExternalRelation"),
								 errdetail("Protocol is %d, writable is %d.",
										   uri->protocol, iswritable)));

					ReleaseSysCache(tuple);
				}
				FreeExternalTableUri(uri);
				pfree(uri_str);
				break;
			}
		}
	}

	PG_RETURN_VOID();
}

static void
exttable_GetForeignRelSize(PlannerInfo *root,
						   RelOptInfo *baserel,
						   Oid foreigntableid)
{
	set_baserel_size_estimates(root, baserel);
}

static void
exttable_GetForeignPaths(PlannerInfo *root,
						 RelOptInfo *baserel,
						 Oid foreigntableid)
{
	ForeignPath   *pathnode;
	ExternalScanInfo *externalscan_info;
	ExtTableEntry *extEntry;

	extEntry = GetExtTableEntry(foreigntableid);

	/* it should be an external rel... */
	Assert(baserel->rtekind == RTE_RELATION);
	Assert(extEntry->execlocations != NIL);

	externalscan_info = MakeExternalScanInfo(extEntry);

	pathnode = create_foreignscan_path(root,
									   baserel,
									   NULL, /* default pathtarget */
									   0, /* rows, filled in later */
									   0, /* startup_cost, later */
									   0, /* total_cost, later */
									   NIL, /* external scan has unordered result */
									   NULL,		/* no outer rel either */
									   NULL,		/* no extra plan */
									   list_make1(externalscan_info));
	pathnode->path.locus = cdbpathlocus_from_baserel(root, baserel);
	pathnode->path.motionHazard = false;

	/*
	 * Mark external tables as non-rescannable. While rescan is possible,
	 * it can lead to surprising results if the external table produces
	 * different results when invoked twice.
	 */
	pathnode->path.rescannable = false;
	pathnode->path.sameslice_relids = baserel->relids;

	cost_externalscan(pathnode, root, baserel, pathnode->path.param_info);

	add_path(baserel, (Path *) pathnode);
	set_cheapest(baserel);
}

/*
 * create_externalscan_plan
 *	 Returns an externalscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 *
 *	 The external plan also includes the data format specification and file
 *	 location specification. Here is where we do the mapping of external file
 *	 to segment database and add it to the plan (or bail out of the mapping
 *	 rules are broken)
 *
 *	 Mapping rules
 *	 -------------
 *	 - 'file' protocol: each location (URI of local file) gets mapped to one
 *						and one only primary segdb.
 *	 - 'http' protocol: each location (URI of http server) gets mapped to one
 *						and one only primary segdb.
 *	 - 'gpfdist' and 'gpfdists' protocols: all locations (URI of gpfdist(s) client) are mapped
 *						to all primary segdbs. If there are less URIs than
 *						segdbs (usually the case) the URIs are duplicated
 *						so that there will be one for each segdb. However, if
 *						the GUC variable gp_external_max_segs is set to a num
 *						less than (total segdbs/total URIs) then we make sure
 *						that no URI gets mapped to more than this GUC number by
 *						skipping some segdbs randomly.
 *	 - 'exec' protocol: all segdbs get mapped to execute the command (this is
 *						soon to be changed though).
 */
static ForeignScan *
exttable_GetForeignPlan(PlannerInfo *root,
						RelOptInfo *baserel,
						Oid foreigntableid,
						ForeignPath *best_path,
						List *tlist,
						List *scan_clauses,
						Plan *outer_plan)
{
	Index		scan_relid = best_path->path.parent->relid;
	ForeignScan *scan_plan;

	Assert(scan_relid > 0);

	/* Reduce RestrictInfo list to bare expressions; ignore pseudoconstants */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	scan_plan = make_foreignscan(tlist,
								 scan_clauses,
								 scan_relid,
								 NIL, /* fdw_exprs */
								 best_path->fdw_private,
								 NIL, /* fdw_scan_tlist */
								 NIL, /* fdw_recheck_quals */
								 NULL /* outer_plan */);

	return scan_plan;
}

static void
exttable_BeginForeignScan(ForeignScanState *node,
						  int eflags)
{
	ForeignScan *scan;
	Relation	currentRelation;
	FileScanDesc currentScanDesc;
	ExternalSelectDesc externalSelectDesc;
	ExternalScanInfo *externalscan_info;
	exttable_fdw_state *fdw_state;

	scan = (ForeignScan *) node->ss.ps.plan;
	externalscan_info = (ExternalScanInfo *) linitial(scan->fdw_private);
	Assert(IsA(externalscan_info, ExternalScanInfo));

	currentRelation = node->ss.ss_currentRelation;
	if (!currentRelation)
		elog(ERROR, "external table scan without a current relation");

	currentScanDesc = external_beginscan(currentRelation,
										 externalscan_info->scancounter,
										 externalscan_info->uriList,
										 externalscan_info->fmtType,
										 externalscan_info->isCoordinatorOnly,
										 externalscan_info->rejLimit,
										 externalscan_info->rejLimitInRows,
										 externalscan_info->logErrors,
										 externalscan_info->encoding,
										 externalscan_info->extOptions);
	externalSelectDesc = external_getnext_init(&node->ss.ps);
	if (gp_external_enable_filter_pushdown)
		externalSelectDesc->filter_quals = node->ss.ps.plan->qual;

	fdw_state = palloc(sizeof(exttable_fdw_state));
	fdw_state->ess_ScanDesc = currentScanDesc;
	fdw_state->externalSelectDesc = externalSelectDesc;

	node->fdw_state = fdw_state;
}


static bool
ExternalConstraintCheck(TupleTableSlot *slot, FileScanDesc scandesc, EState *estate)
{
	Relation		rel = scandesc->fs_rd;
	TupleConstr		*constr = rel->rd_att->constr;
	ConstrCheck		*check = constr->check;
	uint16			ncheck = constr->num_check;
	ExprContext		*econtext = NULL;
	MemoryContext	oldContext = NULL;

	/* No constraints */
	if (ncheck == 0)
	{
		return true;
	}

	/*
	 * Build expression nodetrees for rel's constraint expressions.
	 * Keep them in the per-query memory context so they'll survive throughout the query.
	 */
	if (scandesc->fs_constraintExprs == NULL)
	{
		oldContext = MemoryContextSwitchTo(estate->es_query_cxt);
		scandesc->fs_constraintExprs =
			(ExprState **) palloc(ncheck * sizeof(ExprState *));
		for (int i = 0; i < ncheck; i++)
		{
			/* ExecQual wants implicit-AND form */
			List	   *qual = make_ands_implicit(stringToNode(check[i].ccbin));

			scandesc->fs_constraintExprs[i] =
				ExecPrepareExpr((Expr *) qual, estate);
		}
		MemoryContextSwitchTo(oldContext);
	}

	/*
	 * We will use the EState's per-tuple context for evaluating constraint
	 * expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/* And evaluate the constraints */
	for (int i = 0; i < ncheck; i++)
	{
		ExprState *qual = scandesc->fs_constraintExprs[i];

		if (!ExecCheck(qual, econtext))
			return false;
	}

	return true;
}

/*
 * Check whether a row matches the partition constraint.
 */
static bool
ExternalPartitionCheck(TupleTableSlot *slot, FileScanDesc scandesc, EState *estate)
{
	Relation	rel = scandesc->fs_rd;
	ExprContext *econtext;

	/*
	 * Build expression nodetrees for rel's constraint expressions.
	 * Keep them in the per-query memory context so they'll survive throughout the query.
	 */
	if (scandesc->fs_partitionCheckExpr == NULL)
	{
		List	   *partition_check;
		MemoryContext	oldContext;

		oldContext = MemoryContextSwitchTo(estate->es_query_cxt);

		partition_check = RelationGetPartitionQual(rel);

		scandesc->fs_partitionCheckExpr = ExecPrepareCheck(partition_check, estate);

		MemoryContextSwitchTo(oldContext);
	}

	/*
	 * We will use the EState's per-tuple context for evaluating constraint
	 * expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	return ExecCheck(scandesc->fs_partitionCheckExpr, econtext);
}

static TupleTableSlot *
exttable_IterateForeignScan(ForeignScanState *node)
{
	EState	   *estate = node->ss.ps.state;
	exttable_fdw_state *fdw_state = (exttable_fdw_state *) node->fdw_state;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	HeapTuple	tuple;
	MemoryContext oldcxt;

	/*
	 * XXX: ForeignNext() calls us in a short-lived memory context, which
	 * seems like a good idea. However external_getnext() allocates some stuff
	 * that needs to live longer. At least on the first call. The old
	 * ExternalScan plan node used to run in a long-lived context, which seems
	 * a bit dangerous to me, but I guess that external_getnext() and all the
	 * external protocols are careful not to leak memory.
	 */
	oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);

	for (;;)
	{
		tuple = external_getnext(fdw_state->ess_ScanDesc,
								 ForwardScanDirection, /* FIXME: foreign scans don't support backward scans, I think? */
								 fdw_state->externalSelectDesc);
		if (!tuple)
		{
			ExecClearTuple(slot);
			break;
		}

		ExecStoreHeapTuple(tuple, slot, true);

		/*
		 * If this is a partition in a partitioned table, check each row against
		 * the partition qual, and skip rows that don't belong in this partition.
		 * Foreign tables are not required to enforce that, but that has been
		 * the historical behavior for external tables.
		 */
		if (fdw_state->ess_ScanDesc->fs_isPartition &&
			!ExternalPartitionCheck(slot, fdw_state->ess_ScanDesc, estate))
			continue;

		/*
		 * Similarly, check CHECK constraints and skip rows that don't satisfy
		 * them. Foreign tables are not required required to enforce CHECK
		 * constraints either, they are merely hints to the optimizer, but it
		 * is allowed. (In GPDB 6 and below, partition quals were stored in the
		 * catalogs as CHECK constraints, so this was needed to check the
		 * partition quals.)
		 */
		if (fdw_state->ess_ScanDesc->fs_hasConstraints &&
			!ExternalConstraintCheck(slot, fdw_state->ess_ScanDesc, estate))
			continue;

		break;
	}
	MemoryContextSwitchTo(oldcxt);

	return slot;
}

static void
exttable_ReScanForeignScan(ForeignScanState *node)
{
	exttable_fdw_state *fdw_state = (exttable_fdw_state *) node->fdw_state;

	external_rescan(fdw_state->ess_ScanDesc);
}

static void
exttable_EndForeignScan(ForeignScanState *node)
{
	exttable_fdw_state *fdw_state = (exttable_fdw_state *) node->fdw_state;

	if (node->ss.ps.squelched)
		external_stopscan(fdw_state->ess_ScanDesc);

	/*
	 * report Sreh results if external web table execute on coordinator with reject limit.
	 * if external web table execute on segment, these messages are printed
	 * in cdbdisp_sumRejectedRows()
	*/
	if (Gp_role == GP_ROLE_DISPATCH) {
		CopyState cstate = fdw_state->ess_ScanDesc->fs_pstate;
		if (cstate && cstate->cdbsreh)
		{
			CdbSreh	 *cdbsreh = cstate->cdbsreh;
			uint64	total_rejected_from_qd = cdbsreh->rejectcount;
			if (total_rejected_from_qd > 0)
				ReportSrehResults(cdbsreh, total_rejected_from_qd);
		}
	}

	external_endscan(fdw_state->ess_ScanDesc);
}


/* ModifyTable support */

static int
exttable_IsForeignRelUpdatable(Relation rel)
{
	ExtTableEntry *extentry;
	bool		iswritable;

	extentry = GetExtTableEntry(RelationGetRelid(rel));

	iswritable = extentry->iswritable;

	pfree(extentry);

	return iswritable ? (1 << CMD_INSERT) : 0;
}

static void
exttable_BeginForeignModify(ModifyTableState *mtstate,
							ResultRelInfo *rinfo,
							List *fdw_private,
							int subplan_index,
							int eflags)
{
	/*
	 * Do nothing in EXPLAIN (no ANALYZE) case.  resultRelInfo->ri_FdwState
	 * stays NULL.
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	/*
	 * This would be the natural place to call external_insert_init(), but we
	 * delay that until the first actual insert. That's because we don't want
	 * to open the external resource if we don't end up actually inserting any
	 * rows in this segment. In particular, we don't want to initialize the
	 * external resource in the QD node, when all the actual insertions happen
	 * in the segments.
	 */
}

static TupleTableSlot *
exttable_ExecForeignInsert(EState *estate,
						   ResultRelInfo *rinfo,
						   TupleTableSlot *slot,
						   TupleTableSlot *planSlot)
{
	ExternalInsertDescData *extInsertDesc;

	/* Open the external resouce on first call. */
	extInsertDesc = (ExternalInsertDescData *) rinfo->ri_FdwState;
	if (!extInsertDesc)
	{
		extInsertDesc = external_insert_init(rinfo->ri_RelationDesc);

		rinfo->ri_FdwState = extInsertDesc;
	}

	(void) external_insert(extInsertDesc, slot);

	return slot;
}

static void
exttable_EndForeignModify(EState *estate, ResultRelInfo *rinfo)
{
	ExternalInsertDescData *extInsertDesc = (ExternalInsertDescData *) rinfo->ri_FdwState;

	if (extInsertDesc == NULL)
		return;

	external_insert_finish(extInsertDesc);
}

static void exttable_BeginForeignInsert(ModifyTableState *mtstate,
										ResultRelInfo *resultRelInfo)
{
	/*
	 * This would be the natural place to call external_insert_init(), but we
	 * delay that until the first actual insert. That's because we don't want
	 * to open the external resource if we don't end up actually inserting any
	 * rows in this segment. In particular, we don't want to initialize the
	 * external resource in the QD node, when all the actual insertions happen
	 * in the segments.
	 */
}

static void exttable_EndForeignInsert(EState *estate,
									  ResultRelInfo *resultRelInfo)
{
	ExternalInsertDescData *extInsertDesc = (ExternalInsertDescData *) resultRelInfo->ri_FdwState;

	if (extInsertDesc == NULL)
		return;

	external_insert_finish(extInsertDesc);
}

Datum
gp_exttable_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *routine = makeNode(FdwRoutine);

	routine->GetForeignRelSize = exttable_GetForeignRelSize;
	routine->GetForeignPaths = exttable_GetForeignPaths;
	routine->GetForeignPlan = exttable_GetForeignPlan;
	routine->BeginForeignScan = exttable_BeginForeignScan;
	routine->IterateForeignScan = exttable_IterateForeignScan;
	routine->ReScanForeignScan = exttable_ReScanForeignScan;
	routine->EndForeignScan = exttable_EndForeignScan;

	routine->IsForeignRelUpdatable = exttable_IsForeignRelUpdatable;
	routine->BeginForeignModify = exttable_BeginForeignModify;
	routine->ExecForeignInsert = exttable_ExecForeignInsert;
	routine->EndForeignModify = exttable_EndForeignModify;
	routine->BeginForeignInsert = exttable_BeginForeignInsert;
	routine->EndForeignInsert = exttable_EndForeignInsert;

	PG_RETURN_POINTER(routine);
};


/*
 * cost_externalscan
 *	  Determines and returns the cost of scanning an external relation.
 *
 *	  Right now this is not very meaningful at all but we'll probably
 *	  want to make some good estimates in the future.
 */
static void
cost_externalscan(ForeignPath *path, PlannerInfo *root,
				  RelOptInfo *baserel, ParamPathInfo *param_info)
{
	Cost		startup_cost = 0;
	Cost		run_cost = 0;
	Cost		cpu_per_tuple;

	/* Should only be applied to external relations */
	Assert(baserel->relid > 0);
	Assert(baserel->rtekind == RTE_RELATION);

	/* Mark the path with the correct row estimate */
	if (param_info)
		path->path.rows = param_info->ppi_rows;
	else
		path->path.rows = baserel->rows;

	/*
	 * disk costs
	 */
	run_cost += seq_page_cost * baserel->pages;

	/* CPU costs */
	startup_cost += baserel->baserestrictcost.startup;
	cpu_per_tuple = cpu_tuple_cost + baserel->baserestrictcost.per_tuple;
	run_cost += cpu_per_tuple * baserel->tuples;

	path->path.startup_cost = startup_cost;
	path->path.total_cost = startup_cost + run_cost;
}
