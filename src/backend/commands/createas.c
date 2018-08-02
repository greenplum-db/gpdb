/*-------------------------------------------------------------------------
 *
 * createas.c
 *	  Execution of CREATE TABLE ... AS, a/k/a SELECT INTO
 *
 * We implement this by diverting the query's normal output to a
 * specialized DestReceiver type.
 *
 * Formerly, this command was implemented as a variant of SELECT, which led
 * to assorted legacy behaviors that we still try to preserve, notably that
 * we must return a tuples-processed count in the completionTag.
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/createas.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/toasting.h"
#include "commands/createas.h"
#include "commands/prepare.h"
#include "commands/tablecmds.h"
#include "parser/parse_clause.h"
#include "postmaster/autostats.h"
#include "rewrite/rewriteHandler.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

#include "access/appendonlywriter.h"
#include "catalog/aoseg.h"
#include "catalog/aovisimap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_attribute_encoding.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"

typedef struct
{
	DestReceiver pub;			/* publicly-known function pointers */
	IntoClause *into;			/* target relation specification */
	/* These fields are filled by intorel_startup: */
	Relation	rel;			/* relation to write to */
	CommandId	output_cid;		/* cmin to insert in output tuples */
	int			hi_options;		/* heap_insert performance options */
	BulkInsertState bistate;	/* bulk insert state */

	struct AppendOnlyInsertDescData *ao_insertDesc; /* descriptor to AO tables */
	struct AOCSInsertDescData *aocs_insertDes;      /* descriptor for aocs */
} DR_intorel;

static void intorel_startup_dummy(DestReceiver *self, int operation, TupleDesc typeinfo);
static void intorel_receive(TupleTableSlot *slot, DestReceiver *self);
static void intorel_shutdown(DestReceiver *self);
static void intorel_destroy(DestReceiver *self);

/*
 * ExecCreateTableAs -- execute a CREATE TABLE AS command
 */
void
ExecCreateTableAs(CreateTableAsStmt *stmt, const char *queryString,
				  ParamListInfo params, char *completionTag)
{
	Query	   *query = (Query *) stmt->query;
	IntoClause *into = stmt->into;
	DestReceiver *dest;
	List	   *rewritten;
	PlannedStmt *plan;
	QueryDesc  *queryDesc;
	ScanDirection dir;
	Oid         relationOid = InvalidOid;   /* relation that is modified */
	AutoStatsCmdType cmdType = AUTOSTATS_CMDTYPE_SENTINEL;  /* command type */

	Assert(Gp_role != GP_ROLE_EXECUTE);

	/*
	 * Create the tuple receiver object and insert info it will need
	 */
	dest = CreateIntoRelDestReceiver(into);

	/*
	 * The contained Query could be a SELECT, or an EXECUTE utility command.
	 * If the latter, we just pass it off to ExecuteQuery.
	 */
	Assert(IsA(query, Query));
	if (query->commandType == CMD_UTILITY &&
		IsA(query->utilityStmt, ExecuteStmt))
	{
		ExecuteStmt *estmt = (ExecuteStmt *) query->utilityStmt;

		ExecuteQuery(estmt, into, queryString, params, dest, completionTag);

		return;
	}
	Assert(query->commandType == CMD_SELECT);

	/*
	 * Parse analysis was done already, but we still have to run the rule
	 * rewriter.  We do not do AcquireRewriteLocks: we assume the query either
	 * came straight from the parser, or suitable locks were acquired by
	 * plancache.c.
	 *
	 * Because the rewriter and planner tend to scribble on the input, we make
	 * a preliminary copy of the source querytree.	This prevents problems in
	 * the case that CTAS is in a portal or plpgsql function and is executed
	 * repeatedly.	(See also the same hack in EXPLAIN and PREPARE.)
	 */
	rewritten = QueryRewrite((Query *) copyObject(stmt->query));

	/* SELECT should never rewrite to more or less than one SELECT query */
	if (list_length(rewritten) != 1)
		elog(ERROR, "unexpected rewrite result for CREATE TABLE AS SELECT");
	query = (Query *) linitial(rewritten);
	Assert(query->commandType == CMD_SELECT);

	/* plan the query */
	plan = pg_plan_query(query, 0, params);

	/*GPDB: Save the target information in PlannedStmt */

	/*
	 * GPDB_92_MERGE_FIXME: it really should be an optimizer's responsibility
	 * to correctly set the into-clause and into-policy of the PlannedStmt.
	 */
	plan->intoClause = copyObject(stmt->into);

	/*
	 * Use a snapshot with an updated command ID to ensure this query sees
	 * results of any previously executed queries.	(This could only matter if
	 * the planner executed an allegedly-stable function that changed the
	 * database contents, but let's do it anyway to be parallel to the EXPLAIN
	 * code path.)
	 */
	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	/* Create a QueryDesc, redirecting output to our tuple receiver */
	queryDesc = CreateQueryDesc(plan, queryString,
								GetActiveSnapshot(), InvalidSnapshot,
								dest, params, 0);

	queryDesc->plannedstmt->query_mem = ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

	/* call ExecutorStart to prepare the plan for execution */
	ExecutorStart(queryDesc, GetIntoRelEFlags(into));

	if (Gp_role == GP_ROLE_DISPATCH)
		autostats_get_cmdtype(queryDesc, &cmdType, &relationOid);

	/*
	 * Normally, we run the plan to completion; but if skipData is specified,
	 * just do tuple receiver startup and shutdown.
	 */
	if (into->skipData)
		dir = NoMovementScanDirection;
	else
		dir = ForwardScanDirection;

	/* run the plan */
	ExecutorRun(queryDesc, dir, 0L);

	dest->rDestroy(dest);

	/* and clean up */
	ExecutorFinish(queryDesc);
	ExecutorEnd(queryDesc);

	if (into->distributedBy &&
		((DistributedBy *)(into->distributedBy))->ptype == POLICYTYPE_REPLICATED)
		queryDesc->es_processed /= getgpsegmentCount();

	/* MPP-14001: Running auto_stats */
	if (Gp_role == GP_ROLE_DISPATCH)
		auto_stats(cmdType, relationOid, queryDesc->es_processed, false /* inFunction */);

	/* save the rowcount if we're given a completionTag to fill */
	if (completionTag)
		snprintf(completionTag, COMPLETION_TAG_BUFSIZE,
				 "SELECT " UINT64_FORMAT, queryDesc->es_processed);

	FreeQueryDesc(queryDesc);

	PopActiveSnapshot();
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
	/*
	 * We need to tell the executor whether it has to produce OIDs or not,
	 * because it doesn't have enough information to do so itself (since we
	 * can't build the target relation until after ExecutorStart).
	 */
	if (interpretOidsOption(intoClause->options))
		return EXEC_FLAG_WITH_OIDS;
	else
		return EXEC_FLAG_WITHOUT_OIDS;
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

	self->ao_insertDesc = NULL;
	self->aocs_insertDes = NULL;

	return (DestReceiver *) self;
}

/*
 * intorel_startup_dummy --- executor startup
 */
static void
intorel_startup_dummy(DestReceiver *self, int operation, TupleDesc typeinfo)
{
	/* no-op */

	/* See intorel_initplan() for explanation */
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
	CreateStmt *create;
	Oid			intoRelationId;
	Relation	intoRelationDesc;
	RangeTblEntry *rte;
	Datum		toast_options;
	ListCell   *lc;
	int			attnum;
	static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
	StdRdOptions *stdRdOptions;
	Datum       reloptions;
	int         relstorage;
	TupleDesc   typeinfo = queryDesc->tupDesc;

	/* If EXPLAIN/QE, skip creating the "into" relation. */
	if ((eflags & EXEC_FLAG_EXPLAIN_ONLY) ||
		(Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer))
		return;

	/*
	 * Create the target relation by faking up a CREATE TABLE parsetree and
	 * passing it to DefineRelation.
	 */
	create = makeNode(CreateStmt);
	create->relation = into->rel;
	create->tableElts = NIL;	/* will fill below */
	create->inhRelations = NIL;
	create->ofTypename = NULL;
	create->constraints = NIL;
	create->options = into->options;
	create->oncommit = into->onCommit;
	/*
	 * Select tablespace to use.  If not specified, use default tablespace
	 * (which may in turn default to database's default).
	 *
	 * In PostgreSQL, we resolve default tablespace here. In GPDB, that's
	 * done earlier, because we need to dispatch the final tablespace name,
	 * after resolving any defaults, to the segments. (Otherwise, we would
	 * rely on the assumption that default_tablespace GUC is kept in sync
	 * in all segment connections. That actually seems to be the case, as of
	 * this writing, but better to not rely on it.) So usually, we already
	 * have the fully-resolved tablespace name stashed in queryDesc->ddesc->
	 * intoTableSpaceName. In the dispatcher, we filled it in earlier, and
	 * in executor nodes, we received it from the dispatcher along with the
	 * query. In utility mode, however, queryDesc->ddesc is not set at all,
	 * and we follow the PostgreSQL codepath, resolving the defaults here.
	 */

	if (queryDesc->ddesc)
		create->tablespacename = queryDesc->ddesc->intoTableSpaceName;
	else
		create->tablespacename = into->tableSpaceName;
	create->if_not_exists = false;

	/*
	 * Build column definitions using "pre-cooked" type and collation info. If
	 * a column name list was specified in CREATE TABLE AS, override the
	 * column names derived from the query.  (Too few column names are OK, too
	 * many are not.)
	 */
	lc = list_head(into->colNames);
	for (attnum = 0; attnum < typeinfo->natts; attnum++)
	{
		Form_pg_attribute attribute = typeinfo->attrs[attnum];
		ColumnDef  *col = makeNode(ColumnDef);
		TypeName   *coltype = makeNode(TypeName);

		if (lc)
		{
			col->colname = strVal(lfirst(lc));
			lc = lnext(lc);
		}
		else
			col->colname = NameStr(attribute->attname);
		col->typeName = coltype;
		col->inhcount = 0;
		col->is_local = true;
		col->is_not_null = false;
		col->is_from_type = false;
		col->storage = 0;
		col->raw_default = NULL;
		col->cooked_default = NULL;
		col->collClause = NULL;
		col->collOid = attribute->attcollation;
		col->constraints = NIL;
		col->fdwoptions = NIL;

		coltype->names = NIL;
		coltype->typeOid = attribute->atttypid;
		coltype->setof = false;
		coltype->pct_type = false;
		coltype->typmods = NIL;
		coltype->typemod = attribute->atttypmod;
		coltype->arrayBounds = NIL;
		coltype->location = -1;

		/*
		 * It's possible that the column is of a collatable type but the
		 * collation could not be resolved, so double-check.  (We must check
		 * this here because DefineRelation would adopt the type's default
		 * collation rather than complaining.)
		 */
		if (!OidIsValid(col->collOid) &&
			type_is_collatable(coltype->typeOid))
			ereport(ERROR,
					(errcode(ERRCODE_INDETERMINATE_COLLATION),
					 errmsg("no collation was derived for column \"%s\" with collatable type %s",
							col->colname, format_type_be(coltype->typeOid)),
					 errhint("Use the COLLATE clause to set the collation explicitly.")));

		create->tableElts = lappend(create->tableElts, col);
	}

	if (lc != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("CREATE TABLE AS specifies too many column names")));

	/* Parse and validate any reloptions */
	reloptions = transformRelOptions((Datum) 0,
									 into->options,
									 NULL,
									 validnsps,
									 true,
									 false);

	stdRdOptions = (StdRdOptions*) heap_reloptions(RELKIND_RELATION,
												   reloptions,
												   queryDesc->ddesc ? queryDesc->ddesc->useChangedAOOpts : true);
	if(stdRdOptions->appendonly)
		relstorage = stdRdOptions->columnstore ? RELSTORAGE_AOCOLS : RELSTORAGE_AOROWS;
	else
		relstorage = RELSTORAGE_HEAP;

	create->distributedBy = into->distributedBy; /* Seems to be not needed? */
	create->partitionBy = NULL; /* CTAS does not not support partition. */

    create->policy = queryDesc->plannedstmt->intoPolicy;
	create->postCreate = NULL;
	create->deferredStmts = NULL;
	create->is_part_child = false;
	create->is_add_part = false;
	create->is_split_part = false;
	create->buildAoBlkdir = false;
	create->attr_encodings = NULL; /* Handle by AddDefaultRelationAttributeOptions() */

	/* Save them in CreateStmt for dispatching. */
	create->relKind = RELKIND_RELATION;
	create->relStorage = relstorage;
	create->ownerid = GetUserId();

	/*
	 * Actually create the target table.
	 * Don't dispatch it yet, as we haven't created the toast and other
	 * auxiliary tables yet.
	 */
	intoRelationId = DefineRelation(create,
									RELKIND_RELATION,
									InvalidOid,
									relstorage,
									false,
									queryDesc->ddesc ? queryDesc->ddesc->useChangedAOOpts : true);

	/*
	 * If necessary, create a TOAST table for the target table.  Note that
	 * AlterTableCreateToastTable ends with CommandCounterIncrement(), so that
	 * the TOAST table will be visible for insertion.
	 */
	CommandCounterIncrement();

	/* parse and validate reloptions for the toast table */
	toast_options = transformRelOptions((Datum) 0,
										create->options,
										"toast",
										validnsps,
										true, false);

	(void) heap_reloptions(RELKIND_TOASTVALUE, toast_options, true);

	AlterTableCreateToastTable(intoRelationId, toast_options, false, true);
	AlterTableCreateAoSegTable(intoRelationId, false);
	/* don't create AO block directory here, it'll be created when needed. */
	AlterTableCreateAoVisimapTable(intoRelationId, false);

	/*
	 * Finally we can open the target table
	 */
	intoRelationDesc = heap_open(intoRelationId, AccessExclusiveLock);

	/*
	 * Add column encoding entries based on the WITH clause.
	 *
	 * NOTE:  we could also do this expansion during parse analysis, by
	 * expanding the IntoClause options field into some attr_encodings field
	 * (cf. CreateStmt and transformCreateStmt()). As it stands, there's no real
	 * benefit for doing that from a code complexity POV. In fact, it would mean
	 * more code. If, however, we supported column encoding syntax during CTAS,
	 * it would be a good time to relocate this code.
	 */
	AddDefaultRelationAttributeOptions(intoRelationDesc,
									   into->options);

	/*
	 * Check INSERT permission on the constructed table.
	 *
	 * XXX: It would arguably make sense to skip this check if into->skipData
	 * is true.
	 */
	rte = makeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;
	rte->relid = intoRelationId;
	rte->relkind = RELKIND_RELATION;
	rte->requiredPerms = ACL_INSERT;

	for (attnum = 1; attnum <= intoRelationDesc->rd_att->natts; attnum++)
		rte->modifiedCols = bms_add_member(rte->modifiedCols,
								attnum - FirstLowInvalidHeapAttributeNumber);

	ExecCheckRTPerms(list_make1(rte), true);

	/*
	 * Fill private fields of myState for use by later routines
	 */

	if (queryDesc->dest->mydest != DestIntoRel)
		queryDesc->dest = CreateIntoRelDestReceiver(into);
	myState = (DR_intorel *) queryDesc->dest;
	myState->rel = intoRelationDesc;
	myState->output_cid = GetCurrentCommandId(true);

	/*
	 * We can skip WAL-logging the insertions, unless PITR or streaming
	 * replication is in use. We can skip the FSM in any case.
	 */
	myState->hi_options = HEAP_INSERT_SKIP_FSM |
		(XLogIsNeeded() ? 0 : HEAP_INSERT_SKIP_WAL);
	myState->bistate = GetBulkInsertState();

	/* Not using WAL requires smgr_targblock be initially invalid */
	Assert(RelationGetTargetBlock(intoRelationDesc) == InvalidBlockNumber);
}

/*
 * intorel_receive --- receive one tuple
 */
static void
intorel_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_intorel *myState = (DR_intorel *) self;
	Relation    into_rel = myState->rel;

	if (RelationIsAoRows(into_rel))
	{
		AOTupleId	aoTupleId;
		MemTuple	tuple;

		tuple = ExecCopySlotMemTuple(slot);
		if (myState->ao_insertDesc == NULL)
			myState->ao_insertDesc = appendonly_insert_init(into_rel, RESERVED_SEGNO, false);

		appendonly_insert(myState->ao_insertDesc, tuple, InvalidOid, &aoTupleId);
		pfree(tuple);
	}
	else if (RelationIsAoCols(into_rel))
	{
		if(myState->aocs_insertDes == NULL)
			myState->aocs_insertDes = aocs_insert_init(into_rel, RESERVED_SEGNO, false);

		aocs_insert(myState->aocs_insertDes, slot);
	}
	else
	{
		HeapTuple	tuple;

		/*
		 * get the heap tuple out of the tuple table slot, making sure we have a
		 * writable copy
		 */
		tuple = ExecMaterializeSlot(slot);

		/*
		 * force assignment of new OID (see comments in ExecInsert)
		 */
		if (myState->rel->rd_rel->relhasoids)
			HeapTupleSetOid(tuple, InvalidOid);

		heap_insert(myState->rel,
					tuple,
					myState->output_cid,
					myState->hi_options,
					myState->bistate,
					GetCurrentTransactionId());

		/* We know this is a newly created relation, so there are no indexes */
	}
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

	/* If we skipped using WAL, must heap_sync before commit */
	if (myState->hi_options & HEAP_INSERT_SKIP_WAL)
		heap_sync(myState->rel);

	if (RelationIsAoRows(into_rel) && myState->ao_insertDesc)
		appendonly_insert_finish(myState->ao_insertDesc);
	else if (RelationIsAoCols(into_rel) && myState->aocs_insertDes)
        aocs_insert_finish(myState->aocs_insertDes);

	/* close rel, but keep lock until commit */
	heap_close(into_rel, NoLock);
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
