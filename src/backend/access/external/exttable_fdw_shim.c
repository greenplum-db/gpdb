/*-------------------------------------------------------------------------
 *
 * exttable_fdw_shim.c
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
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/external/exttable_fdw_shim.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/exttable_fdw_shim.h"
#include "access/fileam.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "commands/defrem.h"
#include "foreign/fdwapi.h"
#include "funcapi.h"
#include "nodes/execnodes.h"
#include "nodes/relation.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/var.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/syscache.h"
#include "utils/uri.h"

#define GP_EXTTABLE_ATTRNUM 12

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

static ExternalScanInfo *make_externalscan_info(ExtTableEntry *extEntry);
static List *create_external_scan_uri_list(ExtTableEntry *ext, bool *ismasteronly);
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
	if (fmttype_is_text(formattype) || fmttype_is_csv(formattype))
	{
		bool isfirst = true;

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

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Build tuple descriptor */
		TupleDesc	tupdesc =
		CreateTemplateTupleDesc(GP_EXTTABLE_ATTRNUM, false);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "reloid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "urilocation", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "execlocation", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "fmttype", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "fmtopts", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "options", TEXTARRAYOID, -1, 1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "command", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "rejectlimit", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "rejectlimittype", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "logerrors", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "encoding", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 12, "writable", BOOLOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* Retrieve external table in foreign table catalog */
		pg_foreign_table_rel = heap_open(ForeignTableRelationId, AccessShareLock);

		ScanKeyInit(&ftkey,
					Anum_pg_foreign_table_ftserver,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(PG_EXTTABLE_SERVER_OID));

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
		heap_close(pg_foreign_table_rel, AccessShareLock);

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
		values[9] = CharGetDatum(extentry->logerrors);

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
void
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
				List *location_list = tokenizeLocationUris(defGetString(def));
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
								aclcheck_error(aclresult, ACL_KIND_EXTPROTOCOL, protname);
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

	return;
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

	externalscan_info = make_externalscan_info(extEntry);

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

static ExternalScanInfo *
make_externalscan_info(ExtTableEntry *extEntry)
{
	ExternalScanInfo *node = makeNode(ExternalScanInfo);
	List	   *urilist;
	bool		ismasteronly = false;
	bool		islimitinrows = false;
	int			rejectlimit = -1;
	char		logerrors = LOG_ERRORS_DISABLE;
	static uint32 scancounter = 0;

	if (extEntry->rejectlimit != -1)
	{
		/*
		 * single row error handling is requested, make sure reject limit and
		 * reject type are valid.
		 *
		 * NOTE: this should never happen unless somebody modified the catalog
		 * manually. We are just being pedantic here.
		 */
		VerifyRejectLimit(extEntry->rejectlimittype, extEntry->rejectlimit);
	}

	/* assign Uris to segments. */
	urilist = create_external_scan_uri_list(extEntry, &ismasteronly);

	/* single row error handling */
	if (extEntry->rejectlimit != -1)
	{
		islimitinrows = (extEntry->rejectlimittype == 'r' ? true : false);
		rejectlimit = extEntry->rejectlimit;
		logerrors = extEntry->logerrors;
	}

	node->uriList = urilist;
	node->fmtType = extEntry->fmtcode;
	node->isMasterOnly = ismasteronly;
	node->rejLimit = rejectlimit;
	node->rejLimitInRows = islimitinrows;
	node->logErrors = logerrors;
	node->encoding = extEntry->encoding;
	node->scancounter = scancounter++;
	node->extOptions = extEntry->options;

	return node;
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

/*
 * entry point from ORCA, to create a ForeignScan plan for an external table.
 *
 * Note: the caller is responsible for filling the cost information.
 */
ForeignScan *
create_foreignscan_for_external_table(Oid relid, Index scanrelid,
									  List *qual, List *targetlist)
{
	ExtTableEntry *extEntry;
	ForeignScan *fscan;
	ExternalScanInfo *externalscan_info;

	extEntry = GetExtTableEntry(relid);

	externalscan_info = make_externalscan_info(extEntry);

	fscan = makeNode(ForeignScan);
	fscan->scan.scanrelid = scanrelid;
	fscan->scan.plan.qual = qual;
	fscan->scan.plan.targetlist = targetlist;

	/* cost will be filled in by create_foreignscan_plan */
	fscan->operation = CMD_SELECT;
	/* fs_server will be filled in by create_foreignscan_plan */
	fscan->fs_server = PG_EXTTABLE_SERVER_OID;
	fscan->fdw_exprs = NIL;
	fscan->fdw_private = list_make1(externalscan_info);
	fscan->fdw_scan_tlist = NIL;
	fscan->fdw_recheck_quals = NIL;

	fscan->fs_relids = bms_make_singleton(scanrelid);

	/*
	 * Like in create_foreign_plan(), if rel is a base relation, detect
	 * whether any system columns are requested from the rel.
	 */
	fscan->fsSystemCol = false;
	if (scanrelid > 0)
	{
		Bitmapset  *attrs_used = NULL;
		int			i;

		/*
		 * First, examine all the attributes needed for joins or final output.
		 * Note: we must look at rel's targetlist, not the attr_needed data,
		 * because attr_needed isn't computed for inheritance child rels.
		 */
		pull_varattnos((Node *) targetlist, scanrelid, &attrs_used);

		/* Now, are any system columns requested from rel? */
		for (i = FirstLowInvalidHeapAttributeNumber + 1; i < 0; i++)
		{
			if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber, attrs_used))
			{
				fscan->fsSystemCol = true;
				break;
			}
		}

		bms_free(attrs_used);
	}

	return fscan;
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
										 externalscan_info->isMasterOnly,
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
	List	*qual = NULL;
	int		i = 0;

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
			(List **) palloc(ncheck * sizeof(List *));
		for (i = 0; i < ncheck; i++)
		{
			/* ExecQual wants implicit-AND form */
			qual = make_ands_implicit(stringToNode(check[i].ccbin));
			scandesc->fs_constraintExprs[i] = (List *)
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
	for (i = 0; i < ncheck; i++)
	{
		qual = scandesc->fs_constraintExprs[i];

		if (!ExecQual(qual, econtext, true))
			return false;
	}

	return true;
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

		ExecStoreHeapTuple(tuple, slot, InvalidBuffer, true);

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

	if (node->is_squelched)
		external_stopscan(fdw_state->ess_ScanDesc);

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
	HeapTuple	tuple;

	/* Open the external resouce on first call. */
	extInsertDesc = (ExternalInsertDescData *) rinfo->ri_FdwState;
	if (!extInsertDesc)
	{
		extInsertDesc = external_insert_init(rinfo->ri_RelationDesc);

		rinfo->ri_FdwState = extInsertDesc;
	}

	/*
	 * get the heap tuple out of the tuple table slot, making sure we have a
	 * writable copy. (external_insert() can scribble on the tuple)
	 */
	tuple = ExecMaterializeSlot(slot);

	(void) external_insert(extInsertDesc, tuple);

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
exttable_fdw_handler(PG_FUNCTION_ARGS)
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



static List *
create_external_scan_uri_list(ExtTableEntry *ext, bool *ismasteronly)
{
	ListCell   *c;
	List	   *modifiedloclist = NIL;
	int			i;
	CdbComponentDatabases *db_info;
	int			total_primaries;
	char	  **segdb_file_map;

	/* various processing flags */
	bool		using_execute = false;	/* true if EXECUTE is used */
	bool		using_location; /* true if LOCATION is used */
	bool		found_candidate = false;
	bool		found_match = false;
	bool		done = false;
	List	   *filenames;

	/* gpfdist(s) or EXECUTE specific variables */
	int			total_to_skip = 0;
	int			max_participants_allowed = 0;
	int			num_segs_participating = 0;
	bool	   *skip_map = NULL;
	bool		should_skip_randomly = false;

	Uri		   *uri;
	char	   *on_clause;

	*ismasteronly = false;

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command)
	{
		using_execute = true;
		using_location = false;
	}
	else
	{
		using_execute = false;
		using_location = true;
	}

	/* is this an EXECUTE table or a LOCATION (URI) table */
	if (ext->command && !gp_external_enable_exec)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_CONFIGURED),	/* any better errcode? */
				 errmsg("using external tables with OS level commands (EXECUTE clause) is disabled"),
				 errhint("To enable set gp_external_enable_exec=on.")));
	}

	/* various validations */
	if (ext->iswritable)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot read from a WRITABLE external table"),
				 errhint("Create the table as READABLE instead.")));

	/*
	 * take a peek at the first URI so we know which protocol we'll deal with
	 */
	if (!using_execute)
	{
		char	   *first_uri_str;

		first_uri_str = strVal(linitial(ext->urilocations));
		uri = ParseExternalTableUri(first_uri_str);
	}
	else
		uri = NULL;

	/* get the ON clause information, and restrict 'ON MASTER' to custom
	 * protocols only */
	on_clause = (char *) strVal(linitial(ext->execlocations));
	if ((strcmp(on_clause, "MASTER_ONLY") == 0)
		&& using_location && (uri->protocol != URI_CUSTOM)) {
		ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				errmsg("\'ON MASTER\' is not supported by this protocol yet")));
	}

	/* get the total valid primary segdb count */
	db_info = cdbcomponent_getCdbComponents();
	total_primaries = 0;
	for (i = 0; i < db_info->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];

		if (SEGMENT_IS_ACTIVE_PRIMARY(p))
			total_primaries++;
	}

	/*
	 * initialize a file-to-segdb mapping. segdb_file_map string array indexes
	 * segindex and the entries are the external file path is assigned to this
	 * segment database. For example if segdb_file_map[2] has "/tmp/emp.1" then
	 * this file is assigned to primary segdb 2. if an entry has NULL then
	 * that segdb isn't assigned any file.
	 */
	segdb_file_map = (char **) palloc0(total_primaries * sizeof(char *));

	/*
	 * Now we do the actual assignment of work to the segment databases (where
	 * work is either a URI to open or a command to execute). Due to the big
	 * differences between the different protocols we handle each one
	 * separately. Unfortunately this means some code duplication, but keeping
	 * this separation makes the code much more understandable and (even) more
	 * maintainable.
	 *
	 * Outline of the following code blocks (from simplest to most complex):
	 * (only one of these will get executed for a statement)
	 *
	 * 1) segment mapping for tables with LOCATION http:// or file:// .
	 *
	 * These two protocols are very similar in that they enforce a
	 * 1-URI:1-segdb relationship. The only difference between them is that
	 * file:// URI must be assigned to a segdb on a host that is local to that
	 * URI.
	 *
	 * 2) segment mapping for tables with LOCATION gpfdist(s):// or custom
	 * protocol
	 *
	 * This protocol is more complicated - in here we usually duplicate the
	 * user supplied gpfdist(s):// URIs until there is one available to every
	 * segdb. However, in some cases (as determined by gp_external_max_segs
	 * GUC) we don't want to use *all* segdbs but instead figure out how many
	 * and pick them randomly (this is mainly for better performance and
	 * resource mgmt).
	 *
	 * 3) segment mapping for tables with EXECUTE 'cmd' ON.
	 *
	 * In here we don't have URI's. We have a single command string and a
	 * specification of the segdb granularity it should get executed on (the
	 * ON clause). Depending on the ON clause specification we could go many
	 * different ways, for example: assign the command to all segdb, or one
	 * command per host, or assign to 5 random segments, etc...
	 */

	/* (1) */
	if (using_location && (uri->protocol == URI_FILE || uri->protocol == URI_HTTP))
	{
		/*
		 * extract file path and name from URI strings and assign them a
		 * primary segdb
		 */
		foreach(c, ext->urilocations)
		{
			const char *uri_str = (char *) strVal(lfirst(c));

			uri = ParseExternalTableUri(uri_str);

			found_candidate = false;
			found_match = false;

			/*
			 * look through our segment database list and try to find a
			 * database that can handle this uri.
			 */
			for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int segind = p->config->segindex;

				/*
				 * Assign mapping of external file to this segdb only if:
				 * 1) This segdb is a valid primary.
				 * 2) An external file wasn't already assigned to it.
				 * 3) If 'file' protocol, host of segdb and file must be
				 *    the same.
				 *
				 * This logic also guarantees that file that appears first in
				 * the external location list for the same host gets assigned
				 * the segdb with the lowest index for this host.
				 */
				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					if (uri->protocol == URI_FILE)
					{
						if (pg_strcasecmp(uri->hostname, p->config->hostname) != 0 && pg_strcasecmp(uri->hostname, p->config->address) != 0)
							continue;
					}

					/* a valid primary segdb exist on this host */
					found_candidate = true;

					if (segdb_file_map[segind] == NULL)
					{
						/* segdb not taken yet. assign this URI to this segdb */
						segdb_file_map[segind] = pstrdup(uri_str);
						found_match = true;
					}

					/*
					 * too bad. this segdb already has an external source
					 * assigned
					 */
				}
			}

			/*
			 * We failed to find a segdb for this URI.
			 */
			if (!found_match)
			{
				if (uri->protocol == URI_FILE)
				{
					if (found_candidate)
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("could not assign a segment database for \"%s\"",
										uri_str),
								 errdetail("There are more external files than primary segment databases on host \"%s\"",
										   uri->hostname)));
					}
					else
					{
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("could not assign a segment database for \"%s\"",
										uri_str),
								 errdetail("There isn't a valid primary segment database on host \"%s\"",
										   uri->hostname)));
					}
				}
				else	/* HTTP */
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("could not assign a segment database for \"%s\"",
									uri_str),
							 errdetail("There are more URIs than total primary segment databases")));
				}
			}
		}


	}
	/* (2) */
	else if (using_location && (uri->protocol == URI_GPFDIST ||
							   uri->protocol == URI_GPFDISTS ||
							   uri->protocol == URI_CUSTOM))
	{
		if ((strcmp(on_clause, "MASTER_ONLY") == 0) && (uri->protocol == URI_CUSTOM))
		{
			const char *uri_str = strVal(linitial(ext->urilocations));
			segdb_file_map[0] = pstrdup(uri_str);
			*ismasteronly = true;
		}
		else
		{
			/*
			 * Re-write the location list for GPFDIST or GPFDISTS before mapping to segments.
			 *
			 * If we happen to be dealing with URI's with the 'gpfdist' (or 'gpfdists') protocol
			 * we do an extra step here.
			 *
			 * (*) We modify the urilocationlist so that every
			 * primary segdb will get a URI (therefore we duplicate the existing
			 * URI's until the list is of size = total_primaries).
			 * Example: 2 URIs, 7 total segdbs.
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2->URI1
			 *
			 * (**) We also make sure that we don't allocate more segdbs than
			 * (# of URIs x gp_external_max_segs).
			 * Example: 2 URIs, 7 total segdbs, gp_external_max_segs = 3
			 * Original LocationList: URI1->URI2
			 * Modified LocationList: URI1->URI2->URI1->URI2->URI1->URI2 (6 total).
			 *
			 * (***) In that case that we need to allocate only a subset of primary
			 * segdbs and not all we then also create a random map of segments to skip.
			 * Using the previous example a we create a map of 7 entries and need to
			 * randomly select 1 segdb to skip (7 - 6 = 1). so it may look like this:
			 * [F F T F F F F] - in which case we know to skip the 3rd segment only.
			 */

			/* total num of segs that will participate in the external operation */
			num_segs_participating = total_primaries;

			/* max num of segs that are allowed to participate in the operation */
			if ((uri->protocol == URI_GPFDIST) || (uri->protocol == URI_GPFDISTS))
			{
				max_participants_allowed = list_length(ext->urilocations) *
					gp_external_max_segs;
			}
			else
			{
				/*
				 * for custom protocol, set max_participants_allowed to
				 * num_segs_participating so that assignment to segments will use
				 * all available segments
				 */
				max_participants_allowed = num_segs_participating;
			}

			elog(DEBUG5,
				 "num_segs_participating = %d. max_participants_allowed = %d. number of URIs = %d",
				 num_segs_participating, max_participants_allowed, list_length(ext->urilocations));

			/* see (**) above */
			if (num_segs_participating > max_participants_allowed)
			{
				total_to_skip = num_segs_participating - max_participants_allowed;
				num_segs_participating = max_participants_allowed;
				should_skip_randomly = true;

				elog(NOTICE, "External scan %s will utilize %d out "
					 "of %d segment databases",
					 (uri->protocol == URI_GPFDIST ? "from gpfdist(s) server" : "using custom protocol"),
					 num_segs_participating,
					 total_primaries);
			}

			if (list_length(ext->urilocations) > num_segs_participating)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("there are more external files (URLs) than primary segments that can read them"),
						 errdetail("Found %d URLs and %d primary segments.",
								   list_length(ext->urilocations),
								   num_segs_participating)));

			/*
			 * restart location list and fill in new list until number of
			 * locations equals the number of segments participating in this
			 * action (see (*) above for more details).
			 */
			while (!done)
			{
				foreach(c, ext->urilocations)
				{
					char	   *uri_str = (char *) strVal(lfirst(c));

					/* append to a list of Value nodes, size nelems */
					modifiedloclist = lappend(modifiedloclist, makeString(pstrdup(uri_str)));

					if (list_length(modifiedloclist) == num_segs_participating)
					{
						done = true;
						break;
					}

					if (list_length(modifiedloclist) > num_segs_participating)
					{
						elog(ERROR, "External scan location list failed building distribution.");
					}
				}
			}

			/* See (***) above for details */
			if (should_skip_randomly)
				skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			/*
			 * assign each URI from the new location list a primary segdb
			 */
			foreach(c, modifiedloclist)
			{
				const char *uri_str = strVal(lfirst(c));

				uri = ParseExternalTableUri(uri_str);

				found_candidate = false;
				found_match = false;

				/*
				 * look through our segment database list and try to find a
				 * database that can handle this uri.
				 */
				for (i = 0; i < db_info->total_segment_dbs && !found_match; i++)
				{
					CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
					int			segind = p->config->segindex;

					/*
					 * Assign mapping of external file to this segdb only if:
					 * 1) This segdb is a valid primary.
					 * 2) An external file wasn't already assigned to it.
					 */
					if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					{
						/*
						 * skip this segdb if skip_map for this seg index tells us
						 * to skip it (set to 'true').
						 */
						if (should_skip_randomly)
						{
							Assert(segind < total_primaries);

							if (skip_map[segind])
								continue;	/* skip it */
						}

						/* a valid primary segdb exist on this host */
						found_candidate = true;

						if (segdb_file_map[segind] == NULL)
						{
							/* segdb not taken yet. assign this URI to this segdb */
							segdb_file_map[segind] = pstrdup(uri_str);
							found_match = true;
						}

						/*
						 * too bad. this segdb already has an external source
						 * assigned
						 */
					}
				}

				/* We failed to find a segdb for this gpfdist(s) URI */
				if (!found_match)
				{
					/* should never happen */
					elog(LOG,
						 "external tables gpfdist(s) allocation error. "
						 "total_primaries: %d, num_segs_participating %d "
						 "max_participants_allowed %d, total_to_skip %d",
						 total_primaries, num_segs_participating,
						 max_participants_allowed, total_to_skip);

					elog(ERROR,
						 "internal error in createplan for external tables when trying to assign segments for gpfdist(s)");
				}
			}
		}
	}
	/* (3) */
	else if (using_execute)
	{
		const char *command = ext->command;
		const char *prefix = "execute:";
		char	   *prefixed_command;

		/* build the command string for the executor - 'execute:command' */
		StringInfo	buf = makeStringInfo();

		appendStringInfo(buf, "%s%s", prefix, command);
		prefixed_command = pstrdup(buf->data);

		pfree(buf->data);
		pfree(buf);
		buf = NULL;

		/*
		 * Now we handle each one of the ON locations separately:
		 *
		 * 1) all segs
		 * 2) one per host
		 * 3) all segs on host <foo>
		 * 4) seg <n> only
		 * 5) <n> random segs
		 * 6) master only
		 */
		if (strcmp(on_clause, "ALL_SEGMENTS") == 0)
		{
			/* all segments get a copy of the command to execute */

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
					segdb_file_map[segind] = pstrdup(prefixed_command);
			}

		}
		else if (strcmp(on_clause, "PER_HOST") == 0)
		{
			/* 1 seg per host */

			List	   *visited_hosts = NIL;
			ListCell   *lc;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					bool		host_taken = false;

					foreach(lc, visited_hosts)
					{
						const char *hostname = strVal(lfirst(lc));

						if (pg_strcasecmp(hostname, p->config->hostname) == 0)
						{
							host_taken = true;
							break;
						}
					}

					/*
					 * if not assigned to a seg on this host before - do it
					 * now and add this hostname to the list so that we don't
					 * use segs on this host again.
					 */
					if (!host_taken)
					{
						segdb_file_map[segind] = pstrdup(prefixed_command);
						visited_hosts = lappend(visited_hosts,
										   makeString(pstrdup(p->config->hostname)));
					}
				}
			}
		}
		else if (strncmp(on_clause, "HOST:", strlen("HOST:")) == 0)
		{
			/* all segs on the specified host get copy of the command */
			char	   *hostname = on_clause + strlen("HOST:");
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) &&
					pg_strcasecmp(hostname, p->config->hostname) == 0)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("could not assign a segment database for command \"%s\")",
								command),
						 errdetail("No valid primary segment was found in the requested host name \"%s\".",
								hostname)));
		}
		else if (strncmp(on_clause, "SEGMENT_ID:", strlen("SEGMENT_ID:")) == 0)
		{
			/* 1 seg with specified id gets a copy of the command */
			int			target_segid = atoi(on_clause + strlen("SEGMENT_ID:"));
			bool		match_found = false;

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p) && segind == target_segid)
				{
					segdb_file_map[segind] = pstrdup(prefixed_command);
					match_found = true;
				}
			}

			if (!match_found)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("could not assign a segment database for command \"%s\"",
								command),
						 errdetail("The requested segment id %d is not a valid primary segment or doesn't exist in the database",
								   target_segid)));
		}
		else if (strncmp(on_clause, "TOTAL_SEGS:", strlen("TOTAL_SEGS:")) == 0)
		{
			/* total n segments selected randomly */

			int			num_segs_to_use = atoi(on_clause + strlen("TOTAL_SEGS:"));

			if (num_segs_to_use > total_primaries)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("table defined with EXECUTE ON %d but there are only %d valid primary segments in the database",
								num_segs_to_use, total_primaries)));

			total_to_skip = total_primaries - num_segs_to_use;
			skip_map = makeRandomSegMap(total_primaries, total_to_skip);

			for (i = 0; i < db_info->total_segment_dbs; i++)
			{
				CdbComponentDatabaseInfo *p = &db_info->segment_db_info[i];
				int			segind = p->config->segindex;

				if (SEGMENT_IS_ACTIVE_PRIMARY(p))
				{
					Assert(segind < total_primaries);
					if (skip_map[segind])
						continue;		/* skip it */

					segdb_file_map[segind] = pstrdup(prefixed_command);
				}
			}
		}
		else if (strcmp(on_clause, "MASTER_ONLY") == 0)
		{
			/*
			 * store the command in first array entry and indicate that it is
			 * meant for the master segment (not seg o).
			 */
			segdb_file_map[0] = pstrdup(prefixed_command);
			*ismasteronly = true;
		}
		else
		{
			elog(ERROR, "Internal error in createplan for external tables: got invalid ON clause code %s",
				 on_clause);
		}
	}
	else
	{
		/* should never get here */
		elog(ERROR, "Internal error in createplan for external tables");
	}

	/*
	 * convert array map to a list so it can be serialized as part of the plan
	 */
	filenames = NIL;
	for (i = 0; i < total_primaries; i++)
	{
		if (segdb_file_map[i] != NULL)
			filenames = lappend(filenames, makeString(segdb_file_map[i]));
		else
		{
			/* no file for this segdb. add a null entry */
			Value	   *n = makeNode(Value);

			n->type = T_Null;
			filenames = lappend(filenames, n);
		}
	}

	return filenames;
}


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
