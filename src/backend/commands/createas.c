/*-------------------------------------------------------------------------
 *
 * createas.c
 *	  Execution of CREATE TABLE ... AS, a/k/a SELECT INTO.
 *	  Since CREATE MATERIALIZED VIEW shares syntax and most behaviors,
 *	  we implement that here, too.
 *
 * We implement this by diverting the query's normal output to a
 * specialized DestReceiver type.
 *
 * Formerly, CTAS was implemented as a variant of SELECT, which led
 * to assorted legacy behaviors that we still try to preserve, notably that
 * we must return a tuples-processed count in the completionTag.  (We no
 * longer do that for CTAS ... WITH NO DATA, however.)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/createas.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/namespace.h"
#include "catalog/toasting.h"
#include "commands/createas.h"
#include "commands/matview.h"
#include "commands/prepare.h"
#include "commands/tablecmds.h"
#include "commands/view.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_clause.h"
#include "postmaster/autostats.h"
#include "rewrite/rewriteHandler.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"

#include "catalog/oid_dispatch.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "utils/metrics_utils.h"

typedef struct
{
	DestReceiver pub;			/* publicly-known function pointers */
	IntoClause *into;			/* target relation specification */
	/* These fields are filled by intorel_startup: */
	Relation	rel;			/* relation to write to */
	ObjectAddress reladdr;		/* address of rel, for ExecCreateTableAs */
	CommandId	output_cid;		/* cmin to insert in output tuples */
	int			ti_options;		/* table_tuple_insert performance options */
	BulkInsertState bistate;	/* bulk insert state */
} DR_intorel;

static void intorel_startup_dummy(DestReceiver *self, int operation, TupleDesc typeinfo);
/* utility functions for CTAS definition creation */
static ObjectAddress create_ctas_internal(List *attrList, IntoClause *into,
										  QueryDesc *queryDesc, bool dispatch);
static ObjectAddress create_ctas_nodata(List *tlist, IntoClause *into, QueryDesc *queryDesc);

/* DestReceiver routines for collecting data */
static bool intorel_receive(TupleTableSlot *slot, DestReceiver *self);
static void intorel_shutdown(DestReceiver *self);
static void intorel_destroy(DestReceiver *self);


/*
 * create_ctas_internal
 *
 * Internal utility used for the creation of the definition of a relation
 * created via CREATE TABLE AS or a materialized view.  Caller needs to
 * provide a list of attributes (ColumnDef nodes).
 */
static ObjectAddress
create_ctas_internal(List *attrList, IntoClause *into, QueryDesc *queryDesc, bool dispatch)
{
	CreateStmt *create = makeNode(CreateStmt);
	bool		is_matview;
	char		relkind;
	Datum		toast_options;
	static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
	ObjectAddress intoRelationAddr;

	/* Sync OIDs for into relation, if any */
	cdb_sync_oid_to_segments();

	/* This code supports both CREATE TABLE AS and CREATE MATERIALIZED VIEW */
	is_matview = (into->viewQuery != NULL);
	relkind = is_matview ? RELKIND_MATVIEW : RELKIND_RELATION;

	if (Gp_role == GP_ROLE_EXECUTE)
	{
		create = queryDesc->ddesc->intoCreateStmt;
	}
	/* funny indentation to avoid re-indenting a lot of upstream code */
	else
  {
	/*
	 * Create the target relation by faking up a CREATE TABLE parsetree and
	 * passing it to DefineRelation.
	 */
	create->relation = into->rel;
	create->tableElts = attrList;
	create->inhRelations = NIL;
	create->ofTypename = NULL;
	create->constraints = NIL;
	create->options = into->options;
	create->oncommit = into->onCommit;
	create->tablespacename = into->tableSpaceName;
	create->if_not_exists = false;
	create->gp_style_alter_part = false;

	create->distributedBy = NULL; /* We will pass a pre-made intoPolicy instead */
	create->partitionBy = NULL; /* CTAS does not not support partition. */

	create->buildAoBlkdir = false;
	create->attr_encodings = NULL; /* filled in by DefineRelation */

	/* Save them in CreateStmt for dispatching. */
	create->relKind = relkind;
	create->ownerid = GetUserId();
	create->accessMethod = into->accessMethod;
	create->isCtas = true;

	create->intoQuery = into->viewQuery;
	create->intoPolicy = queryDesc->plannedstmt->intoPolicy;
  }
	/* end of funny indentation */

	/*
	 * Create the relation.  (This will error out if there's an existing view,
	 * so we don't need more code to complain if "replace" is false.)
	 *
	 * Don't dispatch it yet, as we haven't created the toast and other
	 * auxiliary tables yet.
	 *
	 * Pass the policy that was computed by the planner.
	 */
    intoRelationAddr = DefineRelation(create,
                                      relkind,
                                      InvalidOid,
                                      NULL,
                                      NULL,
                                      false,
                                      queryDesc->ddesc ? queryDesc->ddesc->useChangedAOOpts : true,
                                      queryDesc->plannedstmt->intoPolicy);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		queryDesc->ddesc->intoCreateStmt = create;
	}

	/*
	 * If necessary, create a TOAST table for the target table.  Note that
	 * NewRelationCreateToastTable ends with CommandCounterIncrement(), so
	 * that the TOAST table will be visible for insertion.
	 */
	CommandCounterIncrement();

	/* parse and validate reloptions for the toast table */
	toast_options = transformRelOptions((Datum) 0,
										create->options,
										"toast",
										validnsps,
										true, false);

	(void) heap_reloptions(RELKIND_TOASTVALUE, toast_options, true);

	NewRelationCreateToastTable(intoRelationAddr.objectId, toast_options);

	/* Create the "view" part of a materialized view. */
	if (is_matview)
	{
		/* StoreViewQuery scribbles on tree, so make a copy */
		Query	   *query = (Query *) copyObject(into->viewQuery);

		StoreViewQuery(intoRelationAddr.objectId, query, false);
		CommandCounterIncrement();
	}

	if (Gp_role == GP_ROLE_DISPATCH && dispatch)
		CdbDispatchUtilityStatement((Node *) create,
									DF_CANCEL_ON_ERROR |
									DF_NEED_TWO_PHASE |
									DF_WITH_SNAPSHOT,
									GetAssignedOidsForDispatch(),
									NULL);

	return intoRelationAddr;
}


/*
 * create_ctas_nodata
 *
 * Create CTAS or materialized view when WITH NO DATA is used, starting from
 * the targetlist of the SELECT or view definition.
 */
static ObjectAddress
create_ctas_nodata(List *tlist, IntoClause *into, QueryDesc *queryDesc)
{
	List	   *attrList;
	ListCell   *t,
			   *lc;
	ObjectAddress intoRelationAddr;

	/*
	 * Build list of ColumnDefs from non-junk elements of the tlist.  If a
	 * column name list was specified in CREATE TABLE AS, override the column
	 * names in the query.  (Too few column names are OK, too many are not.)
	 */
	attrList = NIL;
	lc = list_head(into->colNames);
	foreach(t, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(t);

		if (!tle->resjunk)
		{
			ColumnDef  *col;
			char	   *colname;

			if (lc)
			{
				colname = strVal(lfirst(lc));
				lc = lnext(lc);
			}
			else
				colname = tle->resname;

			col = makeColumnDef(colname,
								exprType((Node *) tle->expr),
								exprTypmod((Node *) tle->expr),
								exprCollation((Node *) tle->expr));

			/*
			 * It's possible that the column is of a collatable type but the
			 * collation could not be resolved, so double-check.  (We must
			 * check this here because DefineRelation would adopt the type's
			 * default collation rather than complaining.)
			 */
			if (!OidIsValid(col->collOid) &&
				type_is_collatable(col->typeName->typeOid))
				ereport(ERROR,
						(errcode(ERRCODE_INDETERMINATE_COLLATION),
						 errmsg("no collation was derived for column \"%s\" with collatable type %s",
								col->colname,
								format_type_be(col->typeName->typeOid)),
						 errhint("Use the COLLATE clause to set the collation explicitly.")));

			attrList = lappend(attrList, col);
		}
	}

	if (lc != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("too many column names were specified")));

	/* Create the relation definition using the ColumnDef list */
	intoRelationAddr = create_ctas_internal(attrList, into, queryDesc, true);

	return intoRelationAddr;
}


/*
 * ExecCreateTableAs -- execute a CREATE TABLE AS command
 */
ObjectAddress
ExecCreateTableAs(CreateTableAsStmt *stmt, const char *queryString,
				  ParamListInfo params, QueryEnvironment *queryEnv,
				  char *completionTag)
{
	Query	   *query = castNode(Query, stmt->query);
	IntoClause *into = stmt->into;
	bool		is_matview = (into->viewQuery != NULL);
	DestReceiver *dest;
	Oid			save_userid = InvalidOid;
	int			save_sec_context = 0;
	int			save_nestlevel = 0;
	ObjectAddress address;
	List	   *rewritten;
	PlannedStmt *plan;
	QueryDesc  *queryDesc;
	Oid         relationOid = InvalidOid;   /* relation that is modified */
	AutoStatsCmdType cmdType = AUTOSTATS_CMDTYPE_SENTINEL;  /* command type */

	Assert(Gp_role != GP_ROLE_EXECUTE);

	if (stmt->if_not_exists)
	{
		Oid			nspid;
		Oid			oldrelid;

		nspid = RangeVarGetCreationNamespace(into->rel);

		oldrelid = get_relname_relid(into->rel->relname, nspid);
		if (OidIsValid(oldrelid))
		{
			/*
			 * The relation exists and IF NOT EXISTS has been specified.
			 *
			 * If we are in an extension script, insist that the pre-existing
			 * object be a member of the extension, to avoid security risks.
			 */
			ObjectAddressSet(address, RelationRelationId, oldrelid);
			checkMembershipInCurrentExtension(&address);

			/* OK to skip */
			ereport(NOTICE,
					(errcode(ERRCODE_DUPLICATE_TABLE),
					 errmsg("relation \"%s\" already exists, skipping",
							into->rel->relname)));
			return InvalidObjectAddress;
		}
	}
	/*
	 * Create the tuple receiver object and insert info it will need
	 */
	dest = CreateIntoRelDestReceiver(into);

	/*
	 * The contained Query could be a SELECT, or an EXECUTE utility command.
	 * If the latter, we just pass it off to ExecuteQuery.
	 */
	if (query->commandType == CMD_UTILITY &&
		IsA(query->utilityStmt, ExecuteStmt))
	{
		ExecuteStmt *estmt = castNode(ExecuteStmt, query->utilityStmt);

		Assert(!is_matview);	/* excluded by syntax */
		ExecuteQuery(estmt, into, queryString, params, dest, completionTag);

		/* get object address that intorel_startup saved for us */
		address = ((DR_intorel *) dest)->reladdr;

		return address;
	}
	Assert(query->commandType == CMD_SELECT);

	/*
	 * For materialized views, lock down security-restricted operations and
	 * arrange to make GUC variable changes local to this command.  This is
	 * not necessary for security, but this keeps the behavior similar to
	 * REFRESH MATERIALIZED VIEW.  Otherwise, one could create a materialized
	 * view not possible to refresh.
	 */
	if (is_matview)
	{
		GetUserIdAndSecContext(&save_userid, &save_sec_context);
		SetUserIdAndSecContext(save_userid,
							   save_sec_context | SECURITY_RESTRICTED_OPERATION);
		save_nestlevel = NewGUCNestLevel();
	}

	{
		/*
		 * Parse analysis was done already, but we still have to run the rule
		 * rewriter.  We do not do AcquireRewriteLocks: we assume the query
		 * either came straight from the parser, or suitable locks were
		 * acquired by plancache.c.
		 *
		 * Because the rewriter and planner tend to scribble on the input, we
		 * make a preliminary copy of the source querytree.  This prevents
		 * problems in the case that CTAS is in a portal or plpgsql function
		 * and is executed repeatedly.  (See also the same hack in EXPLAIN and
		 * PREPARE.)
		 */
		rewritten = QueryRewrite(copyObject(query));

		/* SELECT should never rewrite to more or less than one SELECT query */
		if (list_length(rewritten) != 1)
			elog(ERROR, "unexpected rewrite result for %s",
				 is_matview ? "CREATE MATERIALIZED VIEW" :
				 "CREATE TABLE AS SELECT");
		query = linitial_node(Query, rewritten);
		Assert(query->commandType == CMD_SELECT);

		/* plan the query */
		plan = pg_plan_query(query, CURSOR_OPT_PARALLEL_OK, params);

		/*GPDB: Save the target information in PlannedStmt */
		/*
		 * GPDB_92_MERGE_FIXME: it really should be an optimizer's responsibility
		 * to correctly set the into-clause and into-policy of the PlannedStmt.
		 */
		plan->intoClause = copyObject(stmt->into);

		/*
		 * Use a snapshot with an updated command ID to ensure this query sees
		 * results of any previously executed queries.  (This could only
		 * matter if the planner executed an allegedly-stable function that
		 * changed the database contents, but let's do it anyway to be
		 * parallel to the EXPLAIN code path.)
		 */
		PushCopiedSnapshot(GetActiveSnapshot());
		UpdateActiveSnapshotCommandId();

		/* Create a QueryDesc, redirecting output to our tuple receiver */
		queryDesc = CreateQueryDesc(plan, queryString,
									GetActiveSnapshot(), InvalidSnapshot,
									dest, params, queryEnv, 0);
	}

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_QUERY_SUBMIT, queryDesc);

	if (into->skipData)
	{
		/*
		 * If WITH NO DATA was specified, do not go through the rewriter,
		 * planner and executor.  Just define the relation using a code path
		 * similar to CREATE VIEW.  This avoids dump/restore problems stemming
		 * from running the planner before all dependencies are set up.
		 */
		queryDesc->ddesc = makeNode(QueryDispatchDesc);
		address = create_ctas_nodata(query->targetList, into, queryDesc);
	}
	else
	{
		queryDesc->plannedstmt->query_mem = ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

		/* call ExecutorStart to prepare the plan for execution */
		ExecutorStart(queryDesc, GetIntoRelEFlags(into));

		if (Gp_role == GP_ROLE_DISPATCH)
			autostats_get_cmdtype(queryDesc, &cmdType, &relationOid);

		/* run the plan to completion */
		ExecutorRun(queryDesc, ForwardScanDirection, 0L, true);

		/* and clean up */
		ExecutorFinish(queryDesc);
		ExecutorEnd(queryDesc);

		/*
		 * In GPDB, computing the row count needs to happen after ExecutorEnd()
		 * because that is where it gets the row count from dispatch. There's
		 * also some special processing if the relation was a replicated table.
		 * In upstream Postgres, the rowcount is saved before ExecutorFinish().
		 */
		if (into->distributedBy &&
			((DistributedBy *)(into->distributedBy))->ptype == POLICYTYPE_REPLICATED)
			queryDesc->es_processed /= ((DistributedBy *)(into->distributedBy))->numsegments;

		/* get object address that intorel_startup saved for us */
		address = ((DR_intorel *) dest)->reladdr;

		/* save the rowcount if we're given a completionTag to fill */
		if (completionTag)
			snprintf(completionTag, COMPLETION_TAG_BUFSIZE,
					 "SELECT " UINT64_FORMAT,
					 queryDesc->es_processed);

		/* MPP-14001: Running auto_stats */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			bool inFunction = already_under_executor_run() || utility_nested();
			auto_stats(cmdType, relationOid, queryDesc->es_processed, inFunction);
		}
	}

	{
		dest->rDestroy(dest);

		FreeQueryDesc(queryDesc);

		PopActiveSnapshot();
	}

	if (is_matview)
	{
		/* Roll back any GUC changes */
		AtEOXact_GUC(false, save_nestlevel);

		/* Restore userid and security context */
		SetUserIdAndSecContext(save_userid, save_sec_context);
	}

	return address;
}

/*
 * GetIntoRelEFlags --- compute executor flags needed for CREATE TABLE AS
 *
 * This is exported because EXPLAIN and PREPARE need it too.  (Note: those
 * callers still need to deal explicitly with the skipData flag; since they
 * use different methods for suppressing execution, it doesn't seem worth
 * trying to encapsulate that part.)
 */
int
GetIntoRelEFlags(IntoClause *intoClause)
{
	int			flags = 0;

	if (intoClause->skipData)
		flags |= EXEC_FLAG_WITH_NO_DATA;

	return flags;
}

/*
 * CreateIntoRelDestReceiver -- create a suitable DestReceiver object
 *
 * intoClause will be NULL if called from CreateDestReceiver(), in which
 * case it has to be provided later.  However, it is convenient to allow
 * self->into to be filled in immediately for other callers.
 */
DestReceiver *
CreateIntoRelDestReceiver(IntoClause *intoClause)
{
	DR_intorel *self = (DR_intorel *) palloc0(sizeof(DR_intorel));

	self->pub.receiveSlot = intorel_receive;
	self->pub.rStartup = intorel_startup_dummy;
	self->pub.rShutdown = intorel_shutdown;
	self->pub.rDestroy = intorel_destroy;
	self->pub.mydest = DestIntoRel;
	self->into = intoClause;

	return (DestReceiver *) self;
}

/*
 * intorel_startup_dummy --- executor startup
 */
static void
intorel_startup_dummy(DestReceiver *self, int operation, TupleDesc typeinfo)
{
	Relation rel = ((DR_intorel *)self)->rel;
	/* See intorel_initplan() for explanation */
	if (rel->rd_tableam)
		table_dml_init(rel);
}

/*
 * intorel_initplan --- Based on PG intorel_startup().
 * Parameters are different. We need to run the code earlier before the
 * executor runs since we want the relation to be created earlier else current
 * MPP framework will fail. This could be called in InitPlan() as before, but
 * we could call it just before ExecutorRun() in ExecCreateTableAs(). In the
 * future if the requirment is general we could add an interface into
 * DestReceiver but so far that is not needed (Based on PG 11 code.)
 */
void
intorel_initplan(struct QueryDesc *queryDesc, int eflags)
{
	DR_intorel *myState;
	/* Get 'into' from the dispatched plan */
	IntoClause *into = queryDesc->plannedstmt->intoClause;
	bool		is_matview;
	char		relkind;
	List	   *attrList;
	ObjectAddress intoRelationAddr;
	Relation	intoRelationDesc;
	RangeTblEntry *rte;
	ListCell   *lc;
	int			attnum;
	TupleDesc   typeinfo = queryDesc->tupDesc;

	/* If EXPLAIN/QE, skip creating the "into" relation. */
	if ((eflags & EXEC_FLAG_EXPLAIN_ONLY) ||
		(Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer))
		return;

	/* This code supports both CREATE TABLE AS and CREATE MATERIALIZED VIEW */
	is_matview = (into->viewQuery != NULL);
	relkind = is_matview ? RELKIND_MATVIEW : RELKIND_RELATION;

	/*
	 * Build column definitions using "pre-cooked" type and collation info. If
	 * a column name list was specified in CREATE TABLE AS, override the
	 * column names derived from the query.  (Too few column names are OK, too
	 * many are not.)
	 */
	attrList = NIL;
	lc = list_head(into->colNames);
	for (attnum = 0; attnum < typeinfo->natts; attnum++)
	{
		Form_pg_attribute attribute = TupleDescAttr(typeinfo, attnum);
		ColumnDef  *col;
		char	   *colname;

		if (lc)
		{
			colname = strVal(lfirst(lc));
			lc = lnext(lc);
		}
		else
			colname = NameStr(attribute->attname);

		col = makeColumnDef(colname,
							attribute->atttypid,
							attribute->atttypmod,
							attribute->attcollation);

		/*
		 * It's possible that the column is of a collatable type but the
		 * collation could not be resolved, so double-check.  (We must check
		 * this here because DefineRelation would adopt the type's default
		 * collation rather than complaining.)
		 */
		if (!OidIsValid(col->collOid) &&
			type_is_collatable(col->typeName->typeOid))
			ereport(ERROR,
					(errcode(ERRCODE_INDETERMINATE_COLLATION),
					 errmsg("no collation was derived for column \"%s\" with collatable type %s",
							col->colname,
							format_type_be(col->typeName->typeOid)),
					 errhint("Use the COLLATE clause to set the collation explicitly.")));

		attrList = lappend(attrList, col);
	}

	if (lc != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("too many column names were specified")));

	/*
	 * Actually create the target table
	 * We also get here with CREATE TABLE AS EXECUTE ... WITH NO DATA. In that
	 * case, dispatch the creation of the table immediately. Normally, the table
	 * is created in the initialization of the plan in QEs, but with NO DATA, we
	 * don't need to dispatch the plan during ExecutorStart().
	 */
	intoRelationAddr = create_ctas_internal(attrList, into, queryDesc,
											into->skipData ? true : false);

	/*
	 * Finally we can open the target table
	 */
	intoRelationDesc = table_open(intoRelationAddr.objectId, AccessExclusiveLock);

	/*
	 * Check INSERT permission on the constructed table.
	 *
	 * XXX: It would arguably make sense to skip this check if into->skipData
	 * is true.
	 */
	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;
	rte->relid = intoRelationAddr.objectId;
	rte->relkind = relkind;
	rte->rellockmode = RowExclusiveLock;
	rte->requiredPerms = ACL_INSERT;

	for (attnum = 1; attnum <= intoRelationDesc->rd_att->natts; attnum++)
		rte->insertedCols = bms_add_member(rte->insertedCols,
										   attnum - FirstLowInvalidHeapAttributeNumber);

	ExecCheckRTPerms(list_make1(rte), true);

	/*
	 * Make sure the constructed table does not have RLS enabled.
	 *
	 * check_enable_rls() will ereport(ERROR) itself if the user has requested
	 * something invalid, and otherwise will return RLS_ENABLED if RLS should
	 * be enabled here.  We don't actually support that currently, so throw
	 * our own ereport(ERROR) if that happens.
	 */
	if (check_enable_rls(intoRelationAddr.objectId, InvalidOid, false) == RLS_ENABLED)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 (errmsg("policies not yet implemented for this command"))));

	/*
	 * Tentatively mark the target as populated, if it's a matview and we're
	 * going to fill it; otherwise, no change needed.
	 */
	if (is_matview && !into->skipData)
		SetMatViewPopulatedState(intoRelationDesc, true);

	/*
	 * Fill private fields of myState for use by later routines
	 */

	if (queryDesc->dest->mydest != DestIntoRel)
		queryDesc->dest = CreateIntoRelDestReceiver(into);
	myState = (DR_intorel *) queryDesc->dest;
	myState->rel = intoRelationDesc;
	myState->reladdr = intoRelationAddr;
	myState->output_cid = GetCurrentCommandId(true);

	/*
	 * We can skip WAL-logging the insertions, unless PITR or streaming
	 * replication is in use. We can skip the FSM in any case.
	 */
	myState->ti_options = TABLE_INSERT_SKIP_FSM |
		(XLogIsNeeded() ? 0 : TABLE_INSERT_SKIP_WAL);
	myState->bistate = GetBulkInsertState();

	/* Not using WAL requires smgr_targblock be initially invalid */
	Assert(RelationGetTargetBlock(intoRelationDesc) == InvalidBlockNumber);
}

/*
 * intorel_receive --- receive one tuple
 */
static bool
intorel_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_intorel *myState = (DR_intorel *) self;

	/*
	 * Note that the input slot might not be of the type of the target
	 * relation. That's supported by table_tuple_insert(), but slightly less
	 * efficient than inserting with the right slot - but the alternative
	 * would be to copy into a slot of the right type, which would not be
	 * cheap either. This also doesn't allow accessing per-AM data (say a
	 * tuple's xmin), but since we don't do that here...
	 */

	table_tuple_insert(myState->rel,
					   slot,
					   myState->output_cid,
					   myState->ti_options,
					   myState->bistate);

	/* We know this is a newly created relation, so there are no indexes */

	return true;
}

/*
 * intorel_shutdown --- executor end
 */
static void
intorel_shutdown(DestReceiver *self)
{
	DR_intorel *myState = (DR_intorel *) self;
	Relation	into_rel = myState->rel;

	if (into_rel == NULL)
		return;

	FreeBulkInsertState(myState->bistate);

	table_finish_bulk_insert(myState->rel, myState->ti_options);

	if (myState->rel->rd_tableam)
		table_dml_finish(myState->rel);

	/* close rel, but keep lock until commit */
	table_close(myState->rel, NoLock);
	myState->rel = NULL;
}

/*
 * intorel_destroy --- release DestReceiver object
 */
static void
intorel_destroy(DestReceiver *self)
{
	pfree(self);
}

/*
 * Get the OID of the relation created for SELECT INTO or CREATE TABLE AS.
 *
 * To be called between ExecutorStart and ExecutorEnd.
 */
Oid
GetIntoRelOid(QueryDesc *queryDesc)
{
	DR_intorel *myState = (DR_intorel *) queryDesc->dest;
	Relation    into_rel = myState->rel;

	if (myState && myState->pub.mydest == DestIntoRel && into_rel)
		return RelationGetRelid(into_rel);
	else
		return InvalidOid;
}
