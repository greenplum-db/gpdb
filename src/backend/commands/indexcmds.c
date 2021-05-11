/*-------------------------------------------------------------------------
 *
 * indexcmds.c
 *	  POSTGRES define and remove index code.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/indexcmds.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/tupconvert.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "partitioning/partdesc.h"
#include "pgstat.h"
#include "rewrite/rewriteManip.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/pg_rusage.h"
#include "utils/regproc.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

#include "access/xact.h"
#include "catalog/aoblkdir.h"
#include "catalog/pg_constraint.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_appendonly.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdbvars.h"
#include "libpq-fe.h"
#include "utils/faultinjector.h"

/* non-export function prototypes */
static void CheckPredicate(Expr *predicate);
static void ComputeIndexAttrs(IndexInfo *indexInfo,
				  Oid *typeOidP,
				  Oid *collationOidP,
				  Oid *classOidP,
				  int16 *colOptionP,
				  List *attList,
				  List *exclusionOpNames,
				  Oid relId,
				  const char *accessMethodName, Oid accessMethodId,
				  bool amcanorder,
				  bool isconstraint);
static char *ChooseIndexNameAddition(List *colnames);
static void RangeVarCallbackForReindexIndex(const RangeVar *relation,
											Oid relId, Oid oldRelId, void *arg);
static bool ReindexRelationConcurrently(Oid relationOid, int options);
static void ReindexPartitionedIndex(Relation parentIdx);
static void update_relispartition(Oid relationId, bool newval);

static void ReindexRelationList(List *relids, int options, bool concurrent, bool multiple);

/*
 * callback argument type for RangeVarCallbackForReindexIndex()
 */
struct ReindexIndexCallbackState
{
	bool		concurrent;		/* flag from statement */
	Oid			locked_table_oid;	/* tracks previously locked table */
};


/*
 * Helper function, to check indcheckxmin for an index on all segments, and
 * set it on the master if it was set on any segment.
 *
 * If CREATE INDEX creates a "broken" HOT chain, the new index must not be
 * used by new queries, with an old snapshot, that would need to see the old
 * values. See src/backend/access/heap/README.HOT. This is enforced by
 * setting indcheckxmin in the pg_index row. In GPDB, we use the pg_index
 * row in the master for planning, but all the data is stored in the
 * segments, so indcheckxmin must be set in the master, if it's set in any
 * of the segments.
 */
static void
cdb_sync_indcheckxmin_with_segments(Oid indexRelationId)
{
	CdbPgResults cdb_pgresults = {NULL, 0};
	int			i;
	char		cmd[100];
	bool		indcheckxmin_set_in_any_segment;

	Assert(Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode());

	/*
	 * Query all the segments, for their indcheckxmin value for this index.
	 */
	snprintf(cmd, sizeof(cmd),
			 "select indcheckxmin from pg_catalog.pg_index where indexrelid = '%u'",
			 indexRelationId);

	CdbDispatchCommand(cmd, DF_WITH_SNAPSHOT, &cdb_pgresults);

	indcheckxmin_set_in_any_segment = false;
	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		char	   *val;

		if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			elog(ERROR, "could not fetch indcheckxmin from segment");
		}

		if (PQntuples(cdb_pgresults.pg_results[i]) != 1 ||
			PQnfields(cdb_pgresults.pg_results[i]) != 1 ||
			PQgetisnull(cdb_pgresults.pg_results[i], 0, 0))
			elog(ERROR, "unexpected shape of result set for indcheckxmin query");

		val = PQgetvalue(cdb_pgresults.pg_results[i], 0, 0);
		if (val[0] == 't')
		{
			indcheckxmin_set_in_any_segment = true;
			break;
		}
		else if (val[0] != 'f')
			elog(ERROR, "invalid boolean value received from segment: %s", val);
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	/*
	 * If indcheckxmin was set on any segment, also set it in the master.
	 */
	if (indcheckxmin_set_in_any_segment)
	{
		Relation	pg_index;
		HeapTuple	indexTuple;
		Form_pg_index indexForm;

		pg_index = heap_open(IndexRelationId, RowExclusiveLock);

		indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(indexRelationId));
		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexRelationId);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		if (!indexForm->indcheckxmin)
		{
			indexForm->indcheckxmin = true;
			CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);
		}

		heap_freetuple(indexTuple);
		heap_close(pg_index, RowExclusiveLock);
	}
}

/*
 * CheckIndexCompatible
 *		Determine whether an existing index definition is compatible with a
 *		prospective index definition, such that the existing index storage
 *		could become the storage of the new index, avoiding a rebuild.
 *
 * 'heapRelation': the relation the index would apply to.
 * 'accessMethodName': name of the AM to use.
 * 'attributeList': a list of IndexElem specifying columns and expressions
 *		to index on.
 * 'exclusionOpNames': list of names of exclusion-constraint operators,
 *		or NIL if not an exclusion constraint.
 *
 * This is tailored to the needs of ALTER TABLE ALTER TYPE, which recreates
 * any indexes that depended on a changing column from their pg_get_indexdef
 * or pg_get_constraintdef definitions.  We omit some of the sanity checks of
 * DefineIndex.  We assume that the old and new indexes have the same number
 * of columns and that if one has an expression column or predicate, both do.
 * Errors arising from the attribute list still apply.
 *
 * Most column type changes that can skip a table rewrite do not invalidate
 * indexes.  We acknowledge this when all operator classes, collations and
 * exclusion operators match.  Though we could further permit intra-opfamily
 * changes for btree and hash indexes, that adds subtle complexity with no
 * concrete benefit for core types. Note, that INCLUDE columns aren't
 * checked by this function, for them it's enough that table rewrite is
 * skipped.
 *
 * When a comparison or exclusion operator has a polymorphic input type, the
 * actual input types must also match.  This defends against the possibility
 * that operators could vary behavior in response to get_fn_expr_argtype().
 * At present, this hazard is theoretical: check_exclusion_constraint() and
 * all core index access methods decline to set fn_expr for such calls.
 *
 * We do not yet implement a test to verify compatibility of expression
 * columns or predicates, so assume any such index is incompatible.
 */
bool
CheckIndexCompatible(Oid oldId,
					 const char *accessMethodName,
					 List *attributeList,
					 List *exclusionOpNames)
{
	bool		isconstraint;
	Oid		   *typeObjectId;
	Oid		   *collationObjectId;
	Oid		   *classObjectId;
	Oid			accessMethodId;
	Oid			relationId;
	HeapTuple	tuple;
	Form_pg_index indexForm;
	Form_pg_am	accessMethodForm;
	IndexAmRoutine *amRoutine;
	bool		amcanorder;
	int16	   *coloptions;
	IndexInfo  *indexInfo;
	int			numberOfAttributes;
	int			old_natts;
	bool		isnull;
	bool		ret = true;
	oidvector  *old_indclass;
	oidvector  *old_indcollation;
	Relation	irel;
	int			i;
	Datum		d;

	/* Caller should already have the relation locked in some way. */
	relationId = IndexGetRelation(oldId, false);

	/*
	 * We can pretend isconstraint = false unconditionally.  It only serves to
	 * decide the text of an error message that should never happen for us.
	 */
	isconstraint = false;

	numberOfAttributes = list_length(attributeList);
	Assert(numberOfAttributes > 0);
	Assert(numberOfAttributes <= INDEX_MAX_KEYS);

	/* look up the access method */
	tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("access method \"%s\" does not exist",
						accessMethodName)));
	accessMethodForm = (Form_pg_am) GETSTRUCT(tuple);
	accessMethodId = accessMethodForm->oid;
	amRoutine = GetIndexAmRoutine(accessMethodForm->amhandler);
	ReleaseSysCache(tuple);

	amcanorder = amRoutine->amcanorder;

	/*
	 * Compute the operator classes, collations, and exclusion operators for
	 * the new index, so we can test whether it's compatible with the existing
	 * one.  Note that ComputeIndexAttrs might fail here, but that's OK:
	 * DefineIndex would have called this function with the same arguments
	 * later on, and it would have failed then anyway.  Our attributeList
	 * contains only key attributes, thus we're filling ii_NumIndexAttrs and
	 * ii_NumIndexKeyAttrs with same value.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = numberOfAttributes;
	indexInfo->ii_NumIndexKeyAttrs = numberOfAttributes;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_ExclusionOps = NULL;
	indexInfo->ii_ExclusionProcs = NULL;
	indexInfo->ii_ExclusionStrats = NULL;
	indexInfo->ii_Am = accessMethodId;
	indexInfo->ii_AmCache = NULL;
	indexInfo->ii_Context = CurrentMemoryContext;
	typeObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	collationObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	classObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	coloptions = (int16 *) palloc(numberOfAttributes * sizeof(int16));
	ComputeIndexAttrs(indexInfo,
					  typeObjectId, collationObjectId, classObjectId,
					  coloptions, attributeList,
					  exclusionOpNames, relationId,
					  accessMethodName, accessMethodId,
					  amcanorder, isconstraint);


	/* Get the soon-obsolete pg_index tuple. */
	tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(oldId));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for index %u", oldId);
	indexForm = (Form_pg_index) GETSTRUCT(tuple);

	/*
	 * We don't assess expressions or predicates; assume incompatibility.
	 * Also, if the index is invalid for any reason, treat it as incompatible.
	 */
	if (!(heap_attisnull(tuple, Anum_pg_index_indpred, NULL) &&
		  heap_attisnull(tuple, Anum_pg_index_indexprs, NULL) &&
		  indexForm->indisvalid))
	{
		ReleaseSysCache(tuple);
		return false;
	}

	/* Any change in operator class or collation breaks compatibility. */
	old_natts = indexForm->indnkeyatts;
	Assert(old_natts == numberOfAttributes);

	d = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indcollation, &isnull);
	Assert(!isnull);
	old_indcollation = (oidvector *) DatumGetPointer(d);

	d = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indclass, &isnull);
	Assert(!isnull);
	old_indclass = (oidvector *) DatumGetPointer(d);

	ret = (memcmp(old_indclass->values, classObjectId,
				  old_natts * sizeof(Oid)) == 0 &&
		   memcmp(old_indcollation->values, collationObjectId,
				  old_natts * sizeof(Oid)) == 0);

	ReleaseSysCache(tuple);

	if (!ret)
		return false;

	/* For polymorphic opcintype, column type changes break compatibility. */
	irel = index_open(oldId, AccessShareLock);	/* caller probably has a lock */
	for (i = 0; i < old_natts; i++)
	{
		if (IsPolymorphicType(get_opclass_input_type(classObjectId[i])) &&
			TupleDescAttr(irel->rd_att, i)->atttypid != typeObjectId[i])
		{
			ret = false;
			break;
		}
	}

	/* Any change in exclusion operator selections breaks compatibility. */
	if (ret && indexInfo->ii_ExclusionOps != NULL)
	{
		Oid		   *old_operators,
				   *old_procs;
		uint16	   *old_strats;

		RelationGetExclusionInfo(irel, &old_operators, &old_procs, &old_strats);
		ret = memcmp(old_operators, indexInfo->ii_ExclusionOps,
					 old_natts * sizeof(Oid)) == 0;

		/* Require an exact input type match for polymorphic operators. */
		if (ret)
		{
			for (i = 0; i < old_natts && ret; i++)
			{
				Oid			left,
							right;

				op_input_types(indexInfo->ii_ExclusionOps[i], &left, &right);
				if ((IsPolymorphicType(left) || IsPolymorphicType(right)) &&
					TupleDescAttr(irel->rd_att, i)->atttypid != typeObjectId[i])
				{
					ret = false;
					break;
				}
			}
		}
	}

	index_close(irel, NoLock);
	return ret;
}


/*
 * WaitForOlderSnapshots
 *
 * Wait for transactions that might have an older snapshot than the given xmin
 * limit, because it might not contain tuples deleted just before it has
 * been taken. Obtain a list of VXIDs of such transactions, and wait for them
 * individually. This is used when building an index concurrently.
 *
 * We can exclude any running transactions that have xmin > the xmin given;
 * their oldest snapshot must be newer than our xmin limit.
 * We can also exclude any transactions that have xmin = zero, since they
 * evidently have no live snapshot at all (and any one they might be in
 * process of taking is certainly newer than ours).  Transactions in other
 * DBs can be ignored too, since they'll never even be able to see the
 * index being worked on.
 *
 * We can also exclude autovacuum processes and processes running manual
 * lazy VACUUMs, because they won't be fazed by missing index entries
 * either.  (Manual ANALYZEs, however, can't be excluded because they
 * might be within transactions that are going to do arbitrary operations
 * later.)
 *
 * Also, GetCurrentVirtualXIDs never reports our own vxid, so we need not
 * check for that.
 *
 * If a process goes idle-in-transaction with xmin zero, we do not need to
 * wait for it anymore, per the above argument.  We do not have the
 * infrastructure right now to stop waiting if that happens, but we can at
 * least avoid the folly of waiting when it is idle at the time we would
 * begin to wait.  We do this by repeatedly rechecking the output of
 * GetCurrentVirtualXIDs.  If, during any iteration, a particular vxid
 * doesn't show up in the output, we know we can forget about it.
 */
static void
WaitForOlderSnapshots(TransactionId limitXmin, bool progress)
{
	int			n_old_snapshots;
	int			i;
	VirtualTransactionId *old_snapshots;

	old_snapshots = GetCurrentVirtualXIDs(limitXmin, true, false,
										  PROC_IS_AUTOVACUUM | PROC_IN_VACUUM,
										  &n_old_snapshots);
	if (progress)
		pgstat_progress_update_param(PROGRESS_WAITFOR_TOTAL, n_old_snapshots);

	for (i = 0; i < n_old_snapshots; i++)
	{
		if (!VirtualTransactionIdIsValid(old_snapshots[i]))
			continue;			/* found uninteresting in previous cycle */

		if (i > 0)
		{
			/* see if anything's changed ... */
			VirtualTransactionId *newer_snapshots;
			int			n_newer_snapshots;
			int			j;
			int			k;

			newer_snapshots = GetCurrentVirtualXIDs(limitXmin,
													true, false,
													PROC_IS_AUTOVACUUM | PROC_IN_VACUUM,
													&n_newer_snapshots);
			for (j = i; j < n_old_snapshots; j++)
			{
				if (!VirtualTransactionIdIsValid(old_snapshots[j]))
					continue;	/* found uninteresting in previous cycle */
				for (k = 0; k < n_newer_snapshots; k++)
				{
					if (VirtualTransactionIdEquals(old_snapshots[j],
												   newer_snapshots[k]))
						break;
				}
				if (k >= n_newer_snapshots) /* not there anymore */
					SetInvalidVirtualTransactionId(old_snapshots[j]);
			}
			pfree(newer_snapshots);
		}

		if (VirtualTransactionIdIsValid(old_snapshots[i]))
		{
			if (progress)
			{
				PGPROC	   *holder = BackendIdGetProc(old_snapshots[i].backendId);

				pgstat_progress_update_param(PROGRESS_WAITFOR_CURRENT_PID,
											 holder->pid);
			}
			VirtualXactLock(old_snapshots[i], true);
		}

		if (progress)
			pgstat_progress_update_param(PROGRESS_WAITFOR_DONE, i + 1);
	}
}


/*
 * DefineIndex
 *		Creates a new index.
 *
 * 'relationId': the OID of the heap relation on which the index is to be
 *		created
 * 'stmt': IndexStmt describing the properties of the new index.
 * 'indexRelationId': normally InvalidOid, but during bootstrap can be
 *		nonzero to specify a preselected OID for the index.
 * 'parentIndexId': the OID of the parent index; InvalidOid if not the child
 *		of a partitioned index.
 * 'parentConstraintId': the OID of the parent constraint; InvalidOid if not
 *		the child of a constraint (only used when recursing)
 * 'is_alter_table': this is due to an ALTER rather than a CREATE operation.
 * 'check_rights': check for CREATE rights in namespace and tablespace.  (This
 *		should be true except when ALTER is deleting/recreating an index.)
 * 'check_not_in_use': check for table not already in use in current session.
 *		This should be true unless caller is holding the table open, in which
 *		case the caller had better have checked it earlier.
 * 'skip_build': make the catalog entries but don't create the index files
 * 'quiet': suppress the NOTICE chatter ordinarily provided for constraints.
 *
 * GPDB:
 * 'is_new_table': is the parent relation new, guaranteed to still be empty?
 *
 * Returns the object address of the created index.
 */
ObjectAddress
DefineIndex(Oid relationId,
			IndexStmt *stmt,
			Oid indexRelationId,
			Oid parentIndexId,
			Oid parentConstraintId,
			bool is_alter_table,
			bool check_rights,
			bool check_not_in_use,
			bool skip_build,
			bool quiet,
			bool is_new_table)
{
	char	   *indexRelationName;
	char	   *accessMethodName;
	Oid		   *typeObjectId;
	Oid		   *collationObjectId;
	Oid		   *classObjectId;
	Oid			accessMethodId;
	Oid			namespaceId;
	Oid			tablespaceId;
	Oid			createdConstraintId = InvalidOid;
	List	   *indexColNames;
	List	   *allIndexParams;
	Relation	rel;
	HeapTuple	tuple;
	Form_pg_am	accessMethodForm;
	IndexAmRoutine *amRoutine;
	bool		amcanorder;
	amoptions_function amoptions;
	bool		partitioned;
	Datum		reloptions;
	int16	   *coloptions;
	IndexInfo  *indexInfo;
	bits16		flags;
	bits16		constr_flags;
	int			numberOfAttributes;
	int			numberOfKeyAttributes;
	TransactionId limitXmin;
	ObjectAddress address;
	LockRelId	heaprelid;
	LOCKTAG		heaplocktag;
	LOCKMODE	lockmode;
	Snapshot	snapshot;
	int			save_nestlevel = -1;
	bool		shouldDispatch;
	int			i;

	if (Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode())
		shouldDispatch = true;
	else
		shouldDispatch = false;

	if (parentIndexId)
	{
		/*
		 * If we're recursing for partitions, don't dispatch this command
		 * separately. We will dispatch the parent command.
		 */
		shouldDispatch = false;
	}

	/*
	 * Also don't dispatch this if it's part of an ALTER TABLE. We will dispatch
	 * the whole ALTER TABLE command later.
	 */
	if (is_alter_table)
		shouldDispatch = false;

	/*
	 * Some callers need us to run with an empty default_tablespace; this is a
	 * necessary hack to be able to reproduce catalog state accurately when
	 * recreating indexes after table-rewriting ALTER TABLE.
	 */
	if (stmt->reset_default_tblspc)
	{
		save_nestlevel = NewGUCNestLevel();
		(void) set_config_option("default_tablespace", "",
								 PGC_USERSET, PGC_S_SESSION,
								 GUC_ACTION_SAVE, true, 0, false);
	}

	/*
	 * Start progress report.  If we're building a partition, this was already
	 * done.
	 */
	if (!OidIsValid(parentIndexId))
	{
		pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX,
									  relationId);
		pgstat_progress_update_param(PROGRESS_CREATEIDX_COMMAND,
									 stmt->concurrent ?
									 PROGRESS_CREATEIDX_COMMAND_CREATE_CONCURRENTLY :
									 PROGRESS_CREATEIDX_COMMAND_CREATE);
	}

	/*
	 * No index OID to report yet
	 */
	pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID,
								 InvalidOid);

	/*
	 * count key attributes in index
	 */
	numberOfKeyAttributes = list_length(stmt->indexParams);

	/*
	 * Calculate the new list of index columns including both key columns and
	 * INCLUDE columns.  Later we can determine which of these are key
	 * columns, and which are just part of the INCLUDE list by checking the
	 * list position.  A list item in a position less than ii_NumIndexKeyAttrs
	 * is part of the key columns, and anything equal to and over is part of
	 * the INCLUDE columns.
	 */
	allIndexParams = list_concat(list_copy(stmt->indexParams),
								 list_copy(stmt->indexIncludingParams));
	numberOfAttributes = list_length(allIndexParams);

	if (numberOfAttributes <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("must specify at least one column")));
	if (numberOfAttributes > INDEX_MAX_KEYS)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("cannot use more than %d columns in an index",
						INDEX_MAX_KEYS)));

	/*
	 * Only SELECT ... FOR UPDATE/SHARE are allowed while doing a standard
	 * index build; but for concurrent builds we allow INSERT/UPDATE/DELETE
	 * (but not VACUUM).
	 *
	 * NB: Caller is responsible for making sure that relationId refers to the
	 * relation on which the index should be built; except in bootstrap mode,
	 * this will typically require the caller to have already locked the
	 * relation.  To avoid lock upgrade hazards, that lock should be at least
	 * as strong as the one we take here.
	 *
	 * NB: If the lock strength here ever changes, code that is run by
	 * parallel workers under the control of certain particular ambuild
	 * functions will need to be updated, too.
	 */
	lockmode = stmt->concurrent ? ShareUpdateExclusiveLock : ShareLock;

	/*
	 * Appendoptimized tables need block directory relation for index
	 * access. Creating and maintaining block directory is expensive,
	 * because it needs to be kept up to date whenever new data is inserted
	 * in the table. We delay the block directory creation until it is
	 * really needed - the first index creation. Once created, all indexes
	 * share the same block directory. We need stronger lock
	 * (ShareRowExclusiveLock) that blocks index creation from another
	 * transaction (not to be confused with create index concurrently) as
	 * well as concurrent insert for appendoptimized tables, if the block
	 * directory needs to be created. If the block directory already exists,
	 * we can use the same lock as heap tables.
	 */
	rel = table_open(relationId, NoLock);
	if (RelationIsAppendOptimized(rel))
	{
		Oid blkdirrelid = InvalidOid;
		GetAppendOnlyEntryAuxOids(relationId, NULL, NULL, &blkdirrelid, NULL, NULL, NULL);

		if (!OidIsValid(blkdirrelid))
			lockmode = ShareRowExclusiveLock; /* Relation is AO, and has no block directory */
	}
	table_close(rel, NoLock);

	rel = table_open(relationId, lockmode);

	namespaceId = RelationGetNamespace(rel);

	/* Ensure that it makes sense to index this kind of relation */
	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_MATVIEW:
		case RELKIND_PARTITIONED_TABLE:
			/* OK */
			break;
		case RELKIND_FOREIGN_TABLE:

			/*
			 * Custom error message for FOREIGN TABLE since the term is close
			 * to a regular table and can confuse the user.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot create index on foreign table \"%s\"",
							RelationGetRelationName(rel))));
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a table or materialized view",
							RelationGetRelationName(rel))));
			break;
	}

	/*
	 * Establish behavior for partitioned tables, and verify sanity of
	 * parameters.
	 *
	 * We do not build an actual index in this case; we only create a few
	 * catalog entries.  The actual indexes are built by recursing for each
	 * partition.
	 */
	partitioned = rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE;
	if (partitioned)
	{
		if (stmt->concurrent)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot create index on partitioned table \"%s\" concurrently",
							RelationGetRelationName(rel))));
		if (stmt->excludeOpNames)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot create exclusion constraints on partitioned table \"%s\"",
							RelationGetRelationName(rel))));
	}

	/*
	 * Don't try to CREATE INDEX on temp tables of other backends.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot create indexes on temporary tables of other sessions")));

	/*
	 * Unless our caller vouches for having checked this already, insist that
	 * the table not be in use by our own session, either.  Otherwise we might
	 * fail to make entries in the new index (for instance, if an INSERT or
	 * UPDATE is in progress and has already made its list of target indexes).
	 */
	if (check_not_in_use)
		CheckTableNotInUse(rel, "CREATE INDEX");

	/*
	 * Verify we (still) have CREATE rights in the rel's namespace.
	 * (Presumably we did when the rel was created, but maybe not anymore.)
	 * Skip check if caller doesn't want it.  Also skip check if
	 * bootstrapping, since permissions machinery may not be working yet.
	 */
	if (check_rights && !IsBootstrapProcessingMode())
	{
		AclResult	aclresult;

		aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(),
										  ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, OBJECT_SCHEMA,
						   get_namespace_name(namespaceId));
	}

	/*
	 * Select tablespace to use.  If not specified, use default tablespace
	 * (which may in turn default to database's default).
	 *
	 * Note: This code duplicates code in tablecmds.c
	 *
	 * MPP-8238 : inconsistent tablespaces between segments and master. In the
	 * QD, store the resolved tablespace name in the command, so that it's
	 * dispatched. In QE, skip the check for 'partitioned': because we got
	 * the value from the QD, it should be ok.
	 */
	if (stmt->tableSpace)
	{
		tablespaceId = get_tablespace_oid(stmt->tableSpace, false);
		if (partitioned && tablespaceId == MyDatabaseTableSpace &&
			Gp_role != GP_ROLE_EXECUTE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot specify default tablespace for partitioned relations")));
	}
	else
	{
		tablespaceId = GetDefaultTablespace(rel->rd_rel->relpersistence,
											partitioned);
		/* note InvalidOid is OK in this case */

		/* Need the real tablespace id for dispatch */
		if (!OidIsValid(tablespaceId)) 
			tablespaceId = MyDatabaseTableSpace;
	}

	/* Check tablespace permissions */
	if (check_rights &&
		OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(),
										   ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, OBJECT_TABLESPACE,
						   get_tablespace_name(tablespaceId));
	}

	/*
	 * Force shared indexes into the pg_global tablespace.  This is a bit of a
	 * hack but seems simpler than marking them in the BKI commands.  On the
	 * other hand, if it's not shared, don't allow it to be placed there.
	 */
	if (rel->rd_rel->relisshared)
		tablespaceId = GLOBALTABLESPACE_OID;
	else if (tablespaceId == GLOBALTABLESPACE_OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("only shared relations can be placed in pg_global tablespace")));

	/*
	 * Choose the index column names.
	 */
	indexColNames = ChooseIndexColumnNames(allIndexParams);

	/*
	 * Select name for index if caller didn't specify
	 *
	 * In GPDB, we need to coordinate the index name between the QD and the
	 * QEs. In the QD, after creating the child index, we stash the chosen
	 * index name in the "oid assignments" list that's normally used to sync
	 * OIDs between QD and QEs. Here, in the QE, we fetch the stashed name
	 * from the list.
	 */
	indexRelationName = stmt->idxname;
	if (indexRelationName == NULL)
	{
		if (OidIsValid(parentIndexId) && Gp_role == GP_ROLE_EXECUTE)
			indexRelationName = GetPreassignedIndexNameForChildIndex(parentIndexId,
																	 relationId);
		else
		{
			indexRelationName = ChooseIndexName(RelationGetRelationName(rel),
											namespaceId,
											indexColNames,
											stmt->excludeOpNames,
											stmt->primary,
											stmt->isconstraint);
		}
	}

	/*
	 * look up the access method, verify it can handle the requested features
	 */
	accessMethodName = stmt->accessMethod;
	tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
	if (!HeapTupleIsValid(tuple))
	{
		/*
		 * Hack to provide more-or-less-transparent updating of old RTREE
		 * indexes to GiST: if RTREE is requested and not found, use GIST.
		 */
		if (strcmp(accessMethodName, "rtree") == 0)
		{
			ereport(NOTICE,
					(errmsg("substituting access method \"gist\" for obsolete method \"rtree\"")));
			accessMethodName = "gist";
			tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
		}

		if (!HeapTupleIsValid(tuple))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("access method \"%s\" does not exist",
							accessMethodName)));
	}
	accessMethodForm = (Form_pg_am) GETSTRUCT(tuple);
	accessMethodId = accessMethodForm->oid;
	amRoutine = GetIndexAmRoutine(accessMethodForm->amhandler);

	pgstat_progress_update_param(PROGRESS_CREATEIDX_ACCESS_METHOD_OID,
								 accessMethodId);

	if (stmt->unique && !amRoutine->amcanunique)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("access method \"%s\" does not support unique indexes",
						accessMethodName)));
	if (stmt->indexIncludingParams != NIL && !amRoutine->amcaninclude)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("access method \"%s\" does not support included columns",
						accessMethodName)));
	if (numberOfAttributes > 1 && !amRoutine->amcanmulticol)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("access method \"%s\" does not support multicolumn indexes",
						accessMethodName)));
	if (stmt->excludeOpNames && amRoutine->amgettuple == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("access method \"%s\" does not support exclusion constraints",
						accessMethodName)));

    if  (stmt->unique && RelationIsAppendOptimized(rel))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("append-only tables do not support unique indexes")));

	amcanorder = amRoutine->amcanorder;
	amoptions = amRoutine->amoptions;

	pfree(amRoutine);
	ReleaseSysCache(tuple);

	/*
	 * Validate predicate, if given
	 */
	if (stmt->whereClause)
		CheckPredicate((Expr *) stmt->whereClause);

	/*
	 * Parse AM-specific options, convert to text array form, validate.
	 */
	reloptions = transformRelOptions((Datum) 0, stmt->options,
									 NULL, NULL, false, false);

	(void) index_reloptions(amoptions, reloptions, true);

	/*
	 * Prepare arguments for index_create, primarily an IndexInfo structure.
	 * Note that ii_Predicate must be in implicit-AND format.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = numberOfAttributes;
	indexInfo->ii_NumIndexKeyAttrs = numberOfKeyAttributes;
	indexInfo->ii_Expressions = NIL;	/* for now */
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = make_ands_implicit((Expr *) stmt->whereClause);
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_ExclusionOps = NULL;
	indexInfo->ii_ExclusionProcs = NULL;
	indexInfo->ii_ExclusionStrats = NULL;
	indexInfo->ii_Unique = stmt->unique;
	/* In a concurrent build, mark it not-ready-for-inserts */
	indexInfo->ii_ReadyForInserts = !stmt->concurrent;
	indexInfo->ii_Concurrent = stmt->concurrent;
	indexInfo->ii_BrokenHotChain = false;
	indexInfo->ii_ParallelWorkers = 0;
	indexInfo->ii_Am = accessMethodId;
	indexInfo->ii_AmCache = NULL;
	indexInfo->ii_Context = CurrentMemoryContext;

	typeObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	collationObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	classObjectId = (Oid *) palloc(numberOfAttributes * sizeof(Oid));
	coloptions = (int16 *) palloc(numberOfAttributes * sizeof(int16));
	ComputeIndexAttrs(indexInfo,
					  typeObjectId, collationObjectId, classObjectId,
					  coloptions, allIndexParams,
					  stmt->excludeOpNames, relationId,
					  accessMethodName, accessMethodId,
					  amcanorder, stmt->isconstraint);

	/*
	 * Extra checks when creating a PRIMARY KEY index.
	 */
	if (stmt->primary)
		index_check_primary_key(rel, indexInfo, is_alter_table, stmt);

	/*
	 * Check that the index is compatible with the distribution policy.
	 *
	 * If the index is unique, the index columns must include all the
	 * distribution key columns. Otherwise we cannot enforce the uniqueness,
	 * because rows with duplicate keys might be stored in differenet segments,
	 * and we would miss it. Similarly, an exlusion constraint must include
	 * all all the distribution key columns.
	 *
	 * As a convenience, if it's a newly created table, we try to change the
	 * policy to allow the index to exist, instead of throwing an error. This
	 * allows the typical case of CREATE TABLE, without a DISTRIBUTED BY
	 * clause, followed by CREATE UNIQUE INDEX, to work. This is a bit weird
	 * if the user specified the distribution policy explicitly in the
	 * CREATE TABLE clause, but we have no way of knowing whether it was
	 * specified explicitly or not.
	 */
	if (rel->rd_cdbpolicy && (stmt->primary || stmt->unique || stmt->excludeOpNames))
	{
		index_check_policy_compatible_context ctx;

		/* Don't allow indexes on system attributes. */
		for (int i = 0; i < indexInfo->ii_NumIndexKeyAttrs; i++)
		{
			if (indexInfo->ii_IndexAttrNumbers[i] < 0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("cannot create constraint or unique index on system column")));
		}
		memset(&ctx, 0, sizeof(ctx));
		ctx.for_alter_dist_policy = false;
		ctx.is_constraint = stmt->isconstraint;
		ctx.is_unique = stmt->unique;
		ctx.is_primarykey = stmt->primary;
		ctx.constraint_name = indexRelationName;
		(void) index_check_policy_compatible(rel->rd_cdbpolicy,
											 RelationGetDescr(rel),
											 indexInfo->ii_IndexAttrNumbers,
											 classObjectId,
											 indexInfo->ii_ExclusionOps,
											 indexInfo->ii_NumIndexKeyAttrs,
											 true, /* report_error */
											 &ctx);
	}

	/*
	 * If this table is partitioned and we're creating a unique index or a
	 * primary key, make sure that the indexed columns are part of the
	 * partition key.  Otherwise it would be possible to violate uniqueness by
	 * putting values that ought to be unique in different partitions.
	 *
	 * We could lift this limitation if we had global indexes, but those have
	 * their own problems, so this is a useful feature combination.
	 */
	if (partitioned && (stmt->unique || stmt->primary))
	{
		PartitionKey key = rel->rd_partkey;
		int			i;

		/*
		 * A partitioned table can have unique indexes, as long as all the
		 * columns in the partition key appear in the unique key.  A
		 * partition-local index can enforce global uniqueness iff the PK
		 * value completely determines the partition that a row is in.
		 *
		 * Thus, verify that all the columns in the partition key appear in
		 * the unique key definition.
		 */
		for (i = 0; i < key->partnatts; i++)
		{
			bool		found = false;
			int			j;
			const char *constraint_type;

			if (stmt->primary)
				constraint_type = "PRIMARY KEY";
			else if (stmt->unique)
				constraint_type = "UNIQUE";
			else if (stmt->excludeOpNames != NIL)
				constraint_type = "EXCLUDE";
			else
			{
				elog(ERROR, "unknown constraint type");
				constraint_type = NULL; /* keep compiler quiet */
			}

			/*
			 * It may be possible to support UNIQUE constraints when partition
			 * keys are expressions, but is it worth it?  Give up for now.
			 */
			if (key->partattrs[i] == 0)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("unsupported %s constraint with partition key definition",
								constraint_type),
						 errdetail("%s constraints cannot be used when partition keys include expressions.",
								   constraint_type)));

			for (j = 0; j < indexInfo->ii_NumIndexKeyAttrs; j++)
			{
				if (key->partattrs[i] == indexInfo->ii_IndexAttrNumbers[j])
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				Form_pg_attribute att;

				att = TupleDescAttr(RelationGetDescr(rel), key->partattrs[i] - 1);
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("insufficient columns in %s constraint definition",
								constraint_type),
						 errdetail("%s constraint on table \"%s\" lacks column \"%s\" which is part of the partition key.",
								   constraint_type, RelationGetRelationName(rel),
								   NameStr(att->attname))));
			}
		}
	}

	if (Gp_role == GP_ROLE_EXECUTE && stmt)
		quiet = true;

	/*
	 * We disallow indexes on system columns.  They would not necessarily get
	 * updated correctly, and they don't seem useful anyway.
	 */
	for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
	{
		AttrNumber	attno = indexInfo->ii_IndexAttrNumbers[i];

		if (attno < 0)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("index creation on system columns is not supported")));
	}

	/*
	 * Also check for system columns used in expressions or predicates.
	 */
	if (indexInfo->ii_Expressions || indexInfo->ii_Predicate)
	{
		Bitmapset  *indexattrs = NULL;

		pull_varattnos((Node *) indexInfo->ii_Expressions, 1, &indexattrs);
		pull_varattnos((Node *) indexInfo->ii_Predicate, 1, &indexattrs);

		for (i = FirstLowInvalidHeapAttributeNumber + 1; i < 0; i++)
		{
			if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber,
							  indexattrs))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("index creation on system columns is not supported")));
		}
	}

	/*
	 * Report index creation if appropriate (delay this till after most of the
	 * error checks)
	 */
	if (stmt->isconstraint && !quiet && Gp_role != GP_ROLE_EXECUTE)
	{
		const char *constraint_type;

		if (stmt->primary)
			constraint_type = "PRIMARY KEY";
		else if (stmt->unique)
			constraint_type = "UNIQUE";
		else if (stmt->excludeOpNames != NIL)
			constraint_type = "EXCLUDE";
		else
		{
			elog(ERROR, "unknown constraint type");
			constraint_type = NULL; /* keep compiler quiet */
		}

		ereport(DEBUG1,
				(errmsg("%s %s will create implicit index \"%s\" for table \"%s\"",
						is_alter_table ? "ALTER TABLE / ADD" : "CREATE TABLE /",
						constraint_type,
						indexRelationName, RelationGetRelationName(rel))));
	}

	if (shouldDispatch)
	{
		cdb_sync_oid_to_segments();

		/*
		 * We defer the dispatch of the utility command until after
		 * index_create(), because that call will *wait*
		 * for any other transactions touching this new relation,
		 * which can cause a non-local deadlock if we've already
		 * dispatched
		 */
	}

	/*
	 * A valid stmt->oldNode implies that we already have a built form of the
	 * index.  The caller should also decline any index build.
	 */
	Assert(!OidIsValid(stmt->oldNode) || (skip_build && !stmt->concurrent));

	/*
	 * Create block directory if this is an appendoptimized
	 * relation
	 */
	AlterTableCreateAoBlkdirTable(RelationGetRelid(rel));

	/*
	 * Make the catalog entries for the index, including constraints. This
	 * step also actually builds the index, except if caller requested not to
	 * or in concurrent mode, in which case it'll be done later, or doing a
	 * partitioned index (because those don't have storage).
	 */
	flags = constr_flags = 0;
	if (stmt->isconstraint)
		flags |= INDEX_CREATE_ADD_CONSTRAINT;
	if (skip_build || stmt->concurrent || partitioned)
		flags |= INDEX_CREATE_SKIP_BUILD;
	if (stmt->if_not_exists)
		flags |= INDEX_CREATE_IF_NOT_EXISTS;
	if (stmt->concurrent)
		flags |= INDEX_CREATE_CONCURRENT;
	if (partitioned)
		flags |= INDEX_CREATE_PARTITIONED;
	if (stmt->primary)
		flags |= INDEX_CREATE_IS_PRIMARY;

	/*
	 * If the table is partitioned, and recursion was declined but partitions
	 * exist, mark the index as invalid.
	 */
	if (partitioned && stmt->relation && !stmt->relation->inh)
	{
		PartitionDesc pd = RelationGetPartitionDesc(rel);

		if (pd->nparts != 0)
			flags |= INDEX_CREATE_INVALID;
	}

	if (stmt->deferrable)
		constr_flags |= INDEX_CONSTR_CREATE_DEFERRABLE;
	if (stmt->initdeferred)
		constr_flags |= INDEX_CONSTR_CREATE_INIT_DEFERRED;

	indexRelationId =
		index_create(rel, indexRelationName, indexRelationId, parentIndexId,
					 parentConstraintId,
					 stmt->oldNode, indexInfo, indexColNames,
					 accessMethodId, tablespaceId,
					 collationObjectId, classObjectId,
					 coloptions, reloptions,
					 flags, constr_flags,
					 allowSystemTableMods, !check_rights,
					 &createdConstraintId);

	ObjectAddressSet(address, RelationRelationId, indexRelationId);

	/*
	 * Revert to original default_tablespace.  Must do this before any return
	 * from this function, but after index_create, so this is a good time.
	 */
	if (save_nestlevel >= 0)
		AtEOXact_GUC(true, save_nestlevel);

	if (!OidIsValid(indexRelationId))
	{
		table_close(rel, NoLock);

		/* If this is the top-level index, we're done */
		if (!OidIsValid(parentIndexId))
			pgstat_progress_end_command();

		return address;
	}

	/*
	 * In the QD, remember the chosen index name and stash it with the
	 * chosen OIDs, so that it's dispatched to the QE later.
	 */
	if (OidIsValid(parentIndexId) && Gp_role == GP_ROLE_DISPATCH)
	{
		RememberPreassignedIndexNameForChildIndex(parentIndexId,
												  relationId,
												  indexRelationName);
	}

	/* Add any requested comment */
	if (stmt->idxcomment != NULL)
		CreateComments(indexRelationId, RelationRelationId, 0,
					   stmt->idxcomment);

	if (partitioned)
	{
		/*
		 * Unless caller specified to skip this step (via ONLY), process each
		 * partition to make sure they all contain a corresponding index.
		 *
		 * If we're called internally (no stmt->relation), recurse always.
		 */
		if (!stmt->relation || stmt->relation->inh)
		{
			PartitionDesc partdesc = RelationGetPartitionDesc(rel);
			int			nparts = partdesc->nparts;
			Oid		   *part_oids = palloc(sizeof(Oid) * nparts);
			bool		invalidate_parent = false;
			TupleDesc	parentDesc;
			Oid		   *opfamOids;

			pgstat_progress_update_param(PROGRESS_CREATEIDX_PARTITIONS_TOTAL,
										 nparts);

			memcpy(part_oids, partdesc->oids, sizeof(Oid) * nparts);

			parentDesc = RelationGetDescr(rel);
			opfamOids = palloc(sizeof(Oid) * numberOfKeyAttributes);
			for (i = 0; i < numberOfKeyAttributes; i++)
				opfamOids[i] = get_opclass_family(classObjectId[i]);

			/*
			 * For each partition, scan all existing indexes; if one matches
			 * our index definition and is not already attached to some other
			 * parent index, attach it to the one we just created.
			 *
			 * If none matches, build a new index by calling ourselves
			 * recursively with the same options (except for the index name).
			 */
			for (i = 0; i < nparts; i++)
			{
				Oid			childRelid = part_oids[i];
				Relation	childrel;
				List	   *childidxs;
				ListCell   *cell;
				AttrNumber *attmap;
				bool		found = false;
				int			maplen;

				childrel = table_open(childRelid, lockmode);

				/*
				 * Don't try to create indexes on foreign tables, though. Skip
				 * those if a regular index, or fail if trying to create a
				 * constraint index.
				 */
				if (childrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
				{
					if (stmt->unique || stmt->primary)
						ereport(ERROR,
								(errcode(ERRCODE_WRONG_OBJECT_TYPE),
								 errmsg("cannot create unique index on partitioned table \"%s\"",
										RelationGetRelationName(rel)),
								 errdetail("Table \"%s\" contains partitions that are foreign tables.",
										   RelationGetRelationName(rel))));

					table_close(childrel, lockmode);
					continue;
				}

				childidxs = RelationGetIndexList(childrel);
				attmap =
					convert_tuples_by_name_map(RelationGetDescr(childrel),
											   parentDesc,
											   gettext_noop("could not convert row type"));
				maplen = parentDesc->natts;

				foreach(cell, childidxs)
				{
					Oid			cldidxid = lfirst_oid(cell);
					Relation	cldidx;
					IndexInfo  *cldIdxInfo;

					/* this index is already partition of another one */
					if (has_superclass(cldidxid))
						continue;

					cldidx = index_open(cldidxid, lockmode);
					cldIdxInfo = BuildIndexInfo(cldidx);
					if (CompareIndexInfo(cldIdxInfo, indexInfo,
										 cldidx->rd_indcollation,
										 collationObjectId,
										 cldidx->rd_opfamily,
										 opfamOids,
										 attmap, maplen))
					{
						Oid			cldConstrOid = InvalidOid;

						/*
						 * Found a match.
						 *
						 * If this index is being created in the parent
						 * because of a constraint, then the child needs to
						 * have a constraint also, so look for one.  If there
						 * is no such constraint, this index is no good, so
						 * keep looking.
						 */
						if (createdConstraintId != InvalidOid)
						{
							cldConstrOid =
								get_relation_idx_constraint_oid(childRelid,
																cldidxid);
							if (cldConstrOid == InvalidOid)
							{
								index_close(cldidx, lockmode);
								continue;
							}
						}

						/* Attach index to parent and we're done. */
						IndexSetParentIndex(cldidx, indexRelationId);
						if (createdConstraintId != InvalidOid)
							ConstraintSetParentConstraint(cldConstrOid,
														  createdConstraintId,
														  childRelid);

						if (!cldidx->rd_index->indisvalid)
							invalidate_parent = true;

						found = true;
						/* keep lock till commit */
						index_close(cldidx, NoLock);
						break;
					}

					index_close(cldidx, lockmode);
				}

				list_free(childidxs);
				table_close(childrel, NoLock);

				/*
				 * If no matching index was found, create our own.
				 */
				if (!found)
				{
					IndexStmt  *childStmt = copyObject(stmt);
					bool		found_whole_row;
					ListCell   *lc;

					/*
					 * We can't use the same index name for the child index,
					 * so clear idxname to let the recursive invocation choose
					 * a new name.  Likewise, the existing target relation
					 * field is wrong, and if indexOid or oldNode are set,
					 * they mustn't be applied to the child either.
					 */
					childStmt->idxname = NULL;
					childStmt->relation = NULL;
					childStmt->indexOid = InvalidOid;
					childStmt->oldNode = InvalidOid;

					/*
					 * Adjust any Vars (both in expressions and in the index's
					 * WHERE clause) to match the partition's column numbering
					 * in case it's different from the parent's.
					 */
					foreach(lc, childStmt->indexParams)
					{
						IndexElem  *ielem = lfirst(lc);

						/*
						 * If the index parameter is an expression, we must
						 * translate it to contain child Vars.
						 */
						if (ielem->expr)
						{
							ielem->expr =
								map_variable_attnos((Node *) ielem->expr,
													1, 0, attmap, maplen,
													InvalidOid,
													&found_whole_row);
							if (found_whole_row)
								elog(ERROR, "cannot convert whole-row table reference");
						}
					}
					childStmt->whereClause =
						map_variable_attnos(stmt->whereClause, 1, 0,
											attmap, maplen,
											InvalidOid, &found_whole_row);
					if (found_whole_row)
						elog(ERROR, "cannot convert whole-row table reference");

					DefineIndex(childRelid, childStmt,
								InvalidOid, /* no predefined OID */
								indexRelationId,	/* this is our child */
								createdConstraintId,
								is_alter_table, check_rights, check_not_in_use,
								skip_build, quiet, is_new_table);
				}

				pgstat_progress_update_param(PROGRESS_CREATEIDX_PARTITIONS_DONE,
											 i + 1);
				pfree(attmap);
			}

			/*
			 * The pg_index row we inserted for this index was marked
			 * indisvalid=true.  But if we attached an existing index that is
			 * invalid, this is incorrect, so update our row to invalid too.
			 */
			if (invalidate_parent)
			{
				Relation	pg_index = table_open(IndexRelationId, RowExclusiveLock);
				HeapTuple	tup,
							newtup;

				tup = SearchSysCache1(INDEXRELID,
									  ObjectIdGetDatum(indexRelationId));
				if (!HeapTupleIsValid(tup))
					elog(ERROR, "cache lookup failed for index %u",
						 indexRelationId);
				newtup = heap_copytuple(tup);
				((Form_pg_index) GETSTRUCT(newtup))->indisvalid = false;
				CatalogTupleUpdate(pg_index, &tup->t_self, newtup);
				ReleaseSysCache(tup);
				table_close(pg_index, RowExclusiveLock);
				heap_freetuple(newtup);
			}
		}

		stmt->idxname = indexRelationName;
		if (shouldDispatch)
		{
			/* make sure the QE uses the same index name that we chose */
			stmt->oldNode = InvalidOid;
			Assert(stmt->relation != NULL);

			stmt->tableSpace = get_tablespace_name(tablespaceId);

			CdbDispatchUtilityStatement((Node *) stmt,
										DF_CANCEL_ON_ERROR |
										DF_WITH_SNAPSHOT |
										DF_NEED_TWO_PHASE,
										GetAssignedOidsForDispatch(),
										NULL);
		}

		/*
		 * Indexes on partitioned tables are not themselves built, so we're
		 * done here.
		 */
		table_close(rel, NoLock);
		if (!OidIsValid(parentIndexId))
			pgstat_progress_end_command();
		return address;
	}

	stmt->idxname = indexRelationName;
	if (shouldDispatch)
	{
		/* make sure the QE uses the same index name that we chose */
		stmt->oldNode = InvalidOid;
		Assert(stmt->relation != NULL);
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT |
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);

		/* Set indcheckxmin in the master, if it was set on any segment */
		if (!indexInfo->ii_BrokenHotChain)
			cdb_sync_indcheckxmin_with_segments(indexRelationId);
	}

	if (!stmt->concurrent)
	{
		/* Close the heap and we're done, in the non-concurrent case */
		table_close(rel, NoLock);

		/* If this is the top-level index, we're done. */
		if (!OidIsValid(parentIndexId))
			pgstat_progress_end_command();

		return address;
	}

	/*
	 * FIXME: concurrent index build needs additional work in Greenplum.  The
	 * feature is disabled in Greenplum until this work is done.  In upstream,
	 * concurrent index build is accomplished in three steps.  Each step is
	 * performed in its own transaction.  In GPDB, each step must be performed
	 * in its own distributed transaction.  Today, we only support dispatching
	 * one IndexStmt.  QEs cannot distinguish the steps within a concurrent
	 * index build.  May be, augment IndexStmt with a variable indicating which
	 * step of concurrent index build a QE should perform?
	 */

	/* save lockrelid and locktag for below, then close rel */
	heaprelid = rel->rd_lockInfo.lockRelId;
	SET_LOCKTAG_RELATION(heaplocktag, heaprelid.dbId, heaprelid.relId);
	table_close(rel, NoLock);

	/*
	 * For a concurrent build, it's important to make the catalog entries
	 * visible to other transactions before we start to build the index. That
	 * will prevent them from making incompatible HOT updates.  The new index
	 * will be marked not indisready and not indisvalid, so that no one else
	 * tries to either insert into it or use it for queries.
	 *
	 * We must commit our current transaction so that the index becomes
	 * visible; then start another.  Note that all the data structures we just
	 * built are lost in the commit.  The only data we keep past here are the
	 * relation IDs.
	 *
	 * Before committing, get a session-level lock on the table, to ensure
	 * that neither it nor the index can be dropped before we finish. This
	 * cannot block, even if someone else is waiting for access, because we
	 * already have the same lock within our transaction.
	 *
	 * Note: we don't currently bother with a session lock on the index,
	 * because there are no operations that could change its state while we
	 * hold lock on the parent table.  This might need to change later.
	 */
	LockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);

	PopActiveSnapshot();
	CommitTransactionCommand();

	StartTransactionCommand();

	/*
	 * The index is now visible, so we can report the OID.
	 */
	pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID,
								 indexRelationId);

	/*
	 * Phase 2 of concurrent index build (see comments for validate_index()
	 * for an overview of how this works)
	 *
	 * Now we must wait until no running transaction could have the table open
	 * with the old list of indexes.  Use ShareLock to consider running
	 * transactions that hold locks that permit writing to the table.  Note we
	 * do not need to worry about xacts that open the table for writing after
	 * this point; they will see the new index when they open it.
	 *
	 * Note: the reason we use actual lock acquisition here, rather than just
	 * checking the ProcArray and sleeping, is that deadlock is possible if
	 * one of the transactions in question is blocked trying to acquire an
	 * exclusive lock on our table.  The lock code will detect deadlock and
	 * error out properly.
	 */
	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_1);
	WaitForLockers(heaplocktag, ShareLock, true);

	/*
	 * At this moment we are sure that there are no transactions with the
	 * table open for write that don't have this new index in their list of
	 * indexes.  We have waited out all the existing transactions and any new
	 * transaction will have the new index in its list, but the index is still
	 * marked as "not-ready-for-inserts".  The index is consulted while
	 * deciding HOT-safety though.  This arrangement ensures that no new HOT
	 * chains can be created where the new tuple and the old tuple in the
	 * chain have different index keys.
	 *
	 * We now take a new snapshot, and build the index using all tuples that
	 * are visible in this snapshot.  We can be sure that any HOT updates to
	 * these tuples will be compatible with the index, since any updates made
	 * by transactions that didn't know about the index are now committed or
	 * rolled back.  Thus, each visible tuple is either the end of its
	 * HOT-chain or the extension of the chain is HOT-safe for this index.
	 */

	/* Set ActiveSnapshot since functions in the indexes may need it */
	PushActiveSnapshot(GetTransactionSnapshot());

	/* Perform concurrent build of index */
	index_concurrently_build(relationId, indexRelationId);

	/* we can do away with our snapshot */
	PopActiveSnapshot();

	/*
	 * Commit this transaction to make the indisready update visible.
	 */
	CommitTransactionCommand();
	StartTransactionCommand();

	/*
	 * Phase 3 of concurrent index build
	 *
	 * We once again wait until no transaction can have the table open with
	 * the index marked as read-only for updates.
	 */
	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_2);
	WaitForLockers(heaplocktag, ShareLock, true);

	/*
	 * Now take the "reference snapshot" that will be used by validate_index()
	 * to filter candidate tuples.  Beware!  There might still be snapshots in
	 * use that treat some transaction as in-progress that our reference
	 * snapshot treats as committed.  If such a recently-committed transaction
	 * deleted tuples in the table, we will not include them in the index; yet
	 * those transactions which see the deleting one as still-in-progress will
	 * expect such tuples to be there once we mark the index as valid.
	 *
	 * We solve this by waiting for all endangered transactions to exit before
	 * we mark the index as valid.
	 *
	 * We also set ActiveSnapshot to this snap, since functions in indexes may
	 * need a snapshot.
	 */
	snapshot = RegisterSnapshot(GetTransactionSnapshot());
	PushActiveSnapshot(snapshot);

	/*
	 * Scan the index and the heap, insert any missing index entries.
	 */
	validate_index(relationId, indexRelationId, snapshot);

	/*
	 * Drop the reference snapshot.  We must do this before waiting out other
	 * snapshot holders, else we will deadlock against other processes also
	 * doing CREATE INDEX CONCURRENTLY, which would see our snapshot as one
	 * they must wait for.  But first, save the snapshot's xmin to use as
	 * limitXmin for GetCurrentVirtualXIDs().
	 */
	limitXmin = snapshot->xmin;

	PopActiveSnapshot();
	UnregisterSnapshot(snapshot);

	/*
	 * The snapshot subsystem could still contain registered snapshots that
	 * are holding back our process's advertised xmin; in particular, if
	 * default_transaction_isolation = serializable, there is a transaction
	 * snapshot that is still active.  The CatalogSnapshot is likewise a
	 * hazard.  To ensure no deadlocks, we must commit and start yet another
	 * transaction, and do our wait before any snapshot has been taken in it.
	 */
	CommitTransactionCommand();
	StartTransactionCommand();

	/* We should now definitely not be advertising any xmin. */
	Assert(MyPgXact->xmin == InvalidTransactionId);

	/*
	 * The index is now valid in the sense that it contains all currently
	 * interesting tuples.  But since it might not contain tuples deleted just
	 * before the reference snap was taken, we have to wait out any
	 * transactions that might have older snapshots.
	 */
	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_3);
	WaitForOlderSnapshots(limitXmin, true);

	/*
	 * Index can now be marked valid -- update its pg_index entry
	 */
	index_set_state_flags(indexRelationId, INDEX_CREATE_SET_VALID);

	/*
	 * The pg_index update will cause backends (including this one) to update
	 * relcache entries for the index itself, but we should also send a
	 * relcache inval on the parent table to force replanning of cached plans.
	 * Otherwise existing sessions might fail to use the new index where it
	 * would be useful.  (Note that our earlier commits did not create reasons
	 * to replan; so relcache flush on the index itself was sufficient.)
	 */
	CacheInvalidateRelcacheByRelid(heaprelid.relId);

	/*
	 * Last thing to do is release the session-level lock on the parent table.
	 */
	UnlockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);

	pgstat_progress_end_command();

	return address;
}


/*
 * CheckMutability
 *		Test whether given expression is mutable
 */
static bool
CheckMutability(Expr *expr)
{
	/*
	 * First run the expression through the planner.  This has a couple of
	 * important consequences.  First, function default arguments will get
	 * inserted, which may affect volatility (consider "default now()").
	 * Second, inline-able functions will get inlined, which may allow us to
	 * conclude that the function is really less volatile than it's marked. As
	 * an example, polymorphic functions must be marked with the most volatile
	 * behavior that they have for any input type, but once we inline the
	 * function we may be able to conclude that it's not so volatile for the
	 * particular input type we're dealing with.
	 *
	 * We assume here that expression_planner() won't scribble on its input.
	 */
	expr = expression_planner(expr);

	/* Now we can search for non-immutable functions */
	return contain_mutable_functions((Node *) expr);
}


/*
 * CheckPredicate
 *		Checks that the given partial-index predicate is valid.
 *
 * This used to also constrain the form of the predicate to forms that
 * indxpath.c could do something with.  However, that seems overly
 * restrictive.  One useful application of partial indexes is to apply
 * a UNIQUE constraint across a subset of a table, and in that scenario
 * any evaluable predicate will work.  So accept any predicate here
 * (except ones requiring a plan), and let indxpath.c fend for itself.
 */
static void
CheckPredicate(Expr *predicate)
{
	/*
	 * transformExpr() should have already rejected subqueries, aggregates,
	 * and window functions, based on the EXPR_KIND_ for a predicate.
	 */

	/*
	 * A predicate using mutable functions is probably wrong, for the same
	 * reasons that we don't allow an index expression to use one.
	 */
	if (CheckMutability(predicate))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("functions in index predicate must be marked IMMUTABLE")));
}

/*
 * Compute per-index-column information, including indexed column numbers
 * or index expressions, opclasses, and indoptions. Note, all output vectors
 * should be allocated for all columns, including "including" ones.
 */
static void
ComputeIndexAttrs(IndexInfo *indexInfo,
				  Oid *typeOidP,
				  Oid *collationOidP,
				  Oid *classOidP,
				  int16 *colOptionP,
				  List *attList,	/* list of IndexElem's */
				  List *exclusionOpNames,
				  Oid relId,
				  const char *accessMethodName,
				  Oid accessMethodId,
				  bool amcanorder,
				  bool isconstraint)
{
	ListCell   *nextExclOp;
	ListCell   *lc;
	int			attn;
	int			nkeycols = indexInfo->ii_NumIndexKeyAttrs;

	/* Allocate space for exclusion operator info, if needed */
	if (exclusionOpNames)
	{
		Assert(list_length(exclusionOpNames) == nkeycols);
		indexInfo->ii_ExclusionOps = (Oid *) palloc(sizeof(Oid) * nkeycols);
		indexInfo->ii_ExclusionProcs = (Oid *) palloc(sizeof(Oid) * nkeycols);
		indexInfo->ii_ExclusionStrats = (uint16 *) palloc(sizeof(uint16) * nkeycols);
		nextExclOp = list_head(exclusionOpNames);
	}
	else
		nextExclOp = NULL;

	/*
	 * process attributeList
	 */
	attn = 0;
	foreach(lc, attList)
	{
		IndexElem  *attribute = (IndexElem *) lfirst(lc);
		Oid			atttype;
		Oid			attcollation;

		/*
		 * Process the column-or-expression to be indexed.
		 */
		if (attribute->name != NULL)
		{
			/* Simple index attribute */
			HeapTuple	atttuple;
			Form_pg_attribute attform;

			Assert(attribute->expr == NULL);
			atttuple = SearchSysCacheAttName(relId, attribute->name);
			if (!HeapTupleIsValid(atttuple))
			{
				/* difference in error message spellings is historical */
				if (isconstraint)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" named in key does not exist",
									attribute->name)));
				else
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" does not exist",
									attribute->name)));
			}
			attform = (Form_pg_attribute) GETSTRUCT(atttuple);
			indexInfo->ii_IndexAttrNumbers[attn] = attform->attnum;
			atttype = attform->atttypid;
			attcollation = attform->attcollation;
			ReleaseSysCache(atttuple);
		}
		else
		{
			/* Index expression */
			Node	   *expr = attribute->expr;

			Assert(expr != NULL);

			if (attn >= nkeycols)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("expressions are not supported in included columns")));
			atttype = exprType(expr);
			attcollation = exprCollation(expr);

			/*
			 * transformExpr() should have already rejected subqueries,
			 * aggregates, and window functions, based on the EXPR_KIND_
			 * for an index expression.
			 */

			/*
			 * Strip any top-level COLLATE clause.  This ensures that we treat
			 * "x COLLATE y" and "(x COLLATE y)" alike.
			 */
			while (IsA(expr, CollateExpr))
				expr = (Node *) ((CollateExpr *) expr)->arg;

			if (IsA(expr, Var) &&
				((Var *) expr)->varattno != InvalidAttrNumber)
			{
				/*
				 * User wrote "(column)" or "(column COLLATE something)".
				 * Treat it like simple attribute anyway.
				 */
				indexInfo->ii_IndexAttrNumbers[attn] = ((Var *) expr)->varattno;
			}
			else
			{
				indexInfo->ii_IndexAttrNumbers[attn] = 0;	/* marks expression */
				indexInfo->ii_Expressions = lappend(indexInfo->ii_Expressions,
													expr);

				/*
				 * transformExpr() should have already rejected subqueries,
				 * aggregates, and window functions, based on the EXPR_KIND_
				 * for an index expression.
				 */

				/*
				 * An expression using mutable functions is probably wrong,
				 * since if you aren't going to get the same result for the
				 * same data every time, it's not clear what the index entries
				 * mean at all.
				 */
				if (CheckMutability((Expr *) expr))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
							 errmsg("functions in index expression must be marked IMMUTABLE")));
			}
		}

		typeOidP[attn] = atttype;

		/*
		 * Included columns have no collation, no opclass and no ordering
		 * options.
		 */
		if (attn >= nkeycols)
		{
			if (attribute->collation)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("including column does not support a collation")));
			if (attribute->opclass)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("including column does not support an operator class")));
			if (attribute->ordering != SORTBY_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("including column does not support ASC/DESC options")));
			if (attribute->nulls_ordering != SORTBY_NULLS_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
						 errmsg("including column does not support NULLS FIRST/LAST options")));

			classOidP[attn] = InvalidOid;
			colOptionP[attn] = 0;
			collationOidP[attn] = InvalidOid;
			attn++;

			continue;
		}

		/*
		 * Apply collation override if any
		 */
		if (attribute->collation)
			attcollation = get_collation_oid(attribute->collation, false);

		/*
		 * Check we have a collation iff it's a collatable type.  The only
		 * expected failures here are (1) COLLATE applied to a noncollatable
		 * type, or (2) index expression had an unresolved collation.  But we
		 * might as well code this to be a complete consistency check.
		 */
		if (type_is_collatable(atttype))
		{
			if (!OidIsValid(attcollation))
				ereport(ERROR,
						(errcode(ERRCODE_INDETERMINATE_COLLATION),
						 errmsg("could not determine which collation to use for index expression"),
						 errhint("Use the COLLATE clause to set the collation explicitly.")));
		}
		else
		{
			if (OidIsValid(attcollation))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("collations are not supported by type %s",
								format_type_be(atttype))));
		}

		collationOidP[attn] = attcollation;

		/*
		 * Identify the opclass to use.
		 */
		classOidP[attn] = ResolveOpClass(attribute->opclass,
										 atttype,
										 accessMethodName,
										 accessMethodId);

		/*
		 * Identify the exclusion operator, if any.
		 */
		if (nextExclOp)
		{
			List	   *opname = (List *) lfirst(nextExclOp);
			Oid			opid;
			Oid			opfamily;
			int			strat;

			/*
			 * Find the operator --- it must accept the column datatype
			 * without runtime coercion (but binary compatibility is OK)
			 */
			opid = compatible_oper_opid(opname, atttype, atttype, false);

			/*
			 * Only allow commutative operators to be used in exclusion
			 * constraints. If X conflicts with Y, but Y does not conflict
			 * with X, bad things will happen.
			 */
			if (get_commutator(opid) != opid)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("operator %s is not commutative",
								format_operator(opid)),
						 errdetail("Only commutative operators can be used in exclusion constraints.")));

			/*
			 * Operator must be a member of the right opfamily, too
			 */
			opfamily = get_opclass_family(classOidP[attn]);
			strat = get_op_opfamily_strategy(opid, opfamily);
			if (strat == 0)
			{
				HeapTuple	opftuple;
				Form_pg_opfamily opfform;

				/*
				 * attribute->opclass might not explicitly name the opfamily,
				 * so fetch the name of the selected opfamily for use in the
				 * error message.
				 */
				opftuple = SearchSysCache1(OPFAMILYOID,
										   ObjectIdGetDatum(opfamily));
				if (!HeapTupleIsValid(opftuple))
					elog(ERROR, "cache lookup failed for opfamily %u",
						 opfamily);
				opfform = (Form_pg_opfamily) GETSTRUCT(opftuple);

				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("operator %s is not a member of operator family \"%s\"",
								format_operator(opid),
								NameStr(opfform->opfname)),
						 errdetail("The exclusion operator must be related to the index operator class for the constraint.")));
			}

			indexInfo->ii_ExclusionOps[attn] = opid;
			indexInfo->ii_ExclusionProcs[attn] = get_opcode(opid);
			indexInfo->ii_ExclusionStrats[attn] = strat;
			nextExclOp = lnext(nextExclOp);
		}

		/*
		 * Set up the per-column options (indoption field).  For now, this is
		 * zero for any un-ordered index, while ordered indexes have DESC and
		 * NULLS FIRST/LAST options.
		 */
		colOptionP[attn] = 0;
		if (amcanorder)
		{
			/* default ordering is ASC */
			if (attribute->ordering == SORTBY_DESC)
				colOptionP[attn] |= INDOPTION_DESC;
			/* default null ordering is LAST for ASC, FIRST for DESC */
			if (attribute->nulls_ordering == SORTBY_NULLS_DEFAULT)
			{
				if (attribute->ordering == SORTBY_DESC)
					colOptionP[attn] |= INDOPTION_NULLS_FIRST;
			}
			else if (attribute->nulls_ordering == SORTBY_NULLS_FIRST)
				colOptionP[attn] |= INDOPTION_NULLS_FIRST;
		}
		else
		{
			/* index AM does not support ordering */
			if (attribute->ordering != SORTBY_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("access method \"%s\" does not support ASC/DESC options",
								accessMethodName)));
			if (attribute->nulls_ordering != SORTBY_NULLS_DEFAULT)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("access method \"%s\" does not support NULLS FIRST/LAST options",
								accessMethodName)));
		}

		attn++;
	}
}

/*
 * Resolve possibly-defaulted operator class specification
 *
 * Note: This is used to resolve operator class specification in index and
 * partition key definitions.
 */
Oid
ResolveOpClass(List *opclass, Oid attrType,
			   const char *accessMethodName, Oid accessMethodId)
{
	char	   *schemaname;
	char	   *opcname;
	HeapTuple	tuple;
	Form_pg_opclass opform;
	Oid			opClassId,
				opInputType;

	/*
	 * Release 7.0 removed network_ops, timespan_ops, and datetime_ops, so we
	 * ignore those opclass names so the default *_ops is used.  This can be
	 * removed in some later release.  bjm 2000/02/07
	 *
	 * Release 7.1 removes lztext_ops, so suppress that too for a while.  tgl
	 * 2000/07/30
	 *
	 * Release 7.2 renames timestamp_ops to timestamptz_ops, so suppress that
	 * too for awhile.  I'm starting to think we need a better approach. tgl
	 * 2000/10/01
	 *
	 * Release 8.0 removes bigbox_ops (which was dead code for a long while
	 * anyway).  tgl 2003/11/11
	 */
	if (list_length(opclass) == 1)
	{
		char	   *claname = strVal(linitial(opclass));

		if (strcmp(claname, "network_ops") == 0 ||
			strcmp(claname, "timespan_ops") == 0 ||
			strcmp(claname, "datetime_ops") == 0 ||
			strcmp(claname, "lztext_ops") == 0 ||
			strcmp(claname, "timestamp_ops") == 0 ||
			strcmp(claname, "bigbox_ops") == 0)
			opclass = NIL;
	}

	if (opclass == NIL)
	{
		/* no operator class specified, so find the default */
		opClassId = GetDefaultOpClass(attrType, accessMethodId);
		if (!OidIsValid(opClassId))
		{
			/*
			 * In GPDB, this function is also used for DISTRIBUTED BY. That's why
			 * we've removed "for index" from the error message.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("data type %s has no default operator class for access method \"%s\"",
							format_type_be(attrType), accessMethodName),
					 errhint("You must specify an operator class or define a default operator class for the data type.")));
		}
		return opClassId;
	}

	/*
	 * Specific opclass name given, so look up the opclass.
	 */

	/* deconstruct the name list */
	DeconstructQualifiedName(opclass, &schemaname, &opcname);

	if (schemaname)
	{
		/* Look in specific schema only */
		Oid			namespaceId;

		namespaceId = LookupExplicitNamespace(schemaname, false);
		tuple = SearchSysCache3(CLAAMNAMENSP,
								ObjectIdGetDatum(accessMethodId),
								PointerGetDatum(opcname),
								ObjectIdGetDatum(namespaceId));
	}
	else
	{
		/* Unqualified opclass name, so search the search path */
		opClassId = OpclassnameGetOpcid(accessMethodId, opcname);
		if (!OidIsValid(opClassId))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("operator class \"%s\" does not exist for access method \"%s\"",
							opcname, accessMethodName)));
		tuple = SearchSysCache1(CLAOID, ObjectIdGetDatum(opClassId));
	}

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("operator class \"%s\" does not exist for access method \"%s\"",
						NameListToString(opclass), accessMethodName)));

	/*
	 * Verify that the index operator class accepts this datatype.  Note we
	 * will accept binary compatibility.
	 */
	opform = (Form_pg_opclass) GETSTRUCT(tuple);
	opClassId = opform->oid;
	opInputType = opform->opcintype;

	if (!IsBinaryCoercible(attrType, opInputType))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("operator class \"%s\" does not accept data type %s",
						NameListToString(opclass), format_type_be(attrType))));

	ReleaseSysCache(tuple);

	return opClassId;
}

/*
 * GetDefaultOpClass
 *
 * Given the OIDs of a datatype and an access method, find the default
 * operator class, if any.  Returns InvalidOid if there is none.
 */
Oid
GetDefaultOpClass(Oid type_id, Oid am_id)
{
	Oid			result = InvalidOid;
	int			nexact = 0;
	int			ncompatible = 0;
	int			ncompatiblepreferred = 0;
	Relation	rel;
	ScanKeyData skey[1];
	SysScanDesc scan;
	HeapTuple	tup;
	TYPCATEGORY tcategory;

	/* If it's a domain, look at the base type instead */
	type_id = getBaseType(type_id);

	tcategory = TypeCategory(type_id);

	/*
	 * We scan through all the opclasses available for the access method,
	 * looking for one that is marked default and matches the target type
	 * (either exactly or binary-compatibly, but prefer an exact match).
	 *
	 * We could find more than one binary-compatible match.  If just one is
	 * for a preferred type, use that one; otherwise we fail, forcing the user
	 * to specify which one he wants.  (The preferred-type special case is a
	 * kluge for varchar: it's binary-compatible to both text and bpchar, so
	 * we need a tiebreaker.)  If we find more than one exact match, then
	 * someone put bogus entries in pg_opclass.
	 */
	rel = table_open(OperatorClassRelationId, AccessShareLock);

	ScanKeyInit(&skey[0],
				Anum_pg_opclass_opcmethod,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(am_id));

	scan = systable_beginscan(rel, OpclassAmNameNspIndexId, true,
							  NULL, 1, skey);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		Form_pg_opclass opclass = (Form_pg_opclass) GETSTRUCT(tup);

		/* ignore altogether if not a default opclass */
		if (!opclass->opcdefault)
			continue;
		if (opclass->opcintype == type_id)
		{
			nexact++;
			result = opclass->oid;
		}
		else if (nexact == 0 &&
				 IsBinaryCoercible(type_id, opclass->opcintype))
		{
			if (IsPreferredType(tcategory, opclass->opcintype))
			{
				ncompatiblepreferred++;
				result = opclass->oid;
			}
			else if (ncompatiblepreferred == 0)
			{
				ncompatible++;
				result = opclass->oid;
			}
		}
	}

	systable_endscan(scan);

	table_close(rel, AccessShareLock);

	/* raise error if pg_opclass contains inconsistent data */
	if (nexact > 1)
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("there are multiple default operator classes for data type %s",
						format_type_be(type_id))));

	if (nexact == 1 ||
		ncompatiblepreferred == 1 ||
		(ncompatiblepreferred == 0 && ncompatible == 1))
		return result;

	return InvalidOid;
}

/*
 *	makeObjectName()
 *
 *	Create a name for an implicitly created index, sequence, constraint,
 *	extended statistics, etc.
 *
 *	The parameters are typically: the original table name, the original field
 *	name, and a "type" string (such as "seq" or "pkey").    The field name
 *	and/or type can be NULL if not relevant.
 *
 *	The result is a palloc'd string.
 *
 *	The basic result we want is "name1_name2_label", omitting "_name2" or
 *	"_label" when those parameters are NULL.  However, we must generate
 *	a name with less than NAMEDATALEN characters!  So, we truncate one or
 *	both names if necessary to make a short-enough string.  The label part
 *	is never truncated (so it had better be reasonably short).
 *
 *	The caller is responsible for checking uniqueness of the generated
 *	name and retrying as needed; retrying will be done by altering the
 *	"label" string (which is why we never truncate that part).
 */
char *
makeObjectName(const char *name1, const char *name2, const char *label)
{
	char	   *name;
	int			overhead = 0;	/* chars needed for label and underscores */
	int			availchars;		/* chars available for name(s) */
	int			name1chars;		/* chars allocated to name1 */
	int			name2chars;		/* chars allocated to name2 */
	int			ndx;

	name1chars = strlen(name1);
	if (name2)
	{
		name2chars = strlen(name2);
		overhead++;				/* allow for separating underscore */
	}
	else
		name2chars = 0;
	if (label)
		overhead += strlen(label) + 1;

	availchars = NAMEDATALEN - 1 - overhead;

	/* GPDB_12_MERGE_FIXME: we're hitting this assertion with some SPLIT PARTITION
	 * commands in regression tests.
	 */
	if (availchars <= 0)
		elog(ERROR, "partition name too long");
	Assert(availchars > 0);		/* else caller chose a bad label */

	/*
	 * If we must truncate,  preferentially truncate the longer name. This
	 * logic could be expressed without a loop, but it's simple and obvious as
	 * a loop.
	 */
	while (name1chars + name2chars > availchars)
	{
		if (name1chars > name2chars)
			name1chars--;
		else
			name2chars--;
	}

	name1chars = pg_mbcliplen(name1, name1chars, name1chars);
	if (name2)
		name2chars = pg_mbcliplen(name2, name2chars, name2chars);

	/* Now construct the string using the chosen lengths */
	name = palloc(name1chars + name2chars + overhead + 1);
	memcpy(name, name1, name1chars);
	ndx = name1chars;
	if (name2)
	{
		name[ndx++] = '_';
		memcpy(name + ndx, name2, name2chars);
		ndx += name2chars;
	}
	if (label)
	{
		name[ndx++] = '_';
		strcpy(name + ndx, label);
	}
	else
		name[ndx] = '\0';

	return name;
}

/*
 * Select a nonconflicting name for a new relation.  This is ordinarily
 * used to choose index names (which is why it's here) but it can also
 * be used for sequences, or any autogenerated relation kind.
 *
 * name1, name2, and label are used the same way as for makeObjectName(),
 * except that the label can't be NULL; digits will be appended to the label
 * if needed to create a name that is unique within the specified namespace.
 *
 * If isconstraint is true, we also avoid choosing a name matching any
 * existing constraint in the same namespace.  (This is stricter than what
 * Postgres itself requires, but the SQL standard says that constraint names
 * should be unique within schemas, so we follow that for autogenerated
 * constraint names.)
 *
 * Note: it is theoretically possible to get a collision anyway, if someone
 * else chooses the same name concurrently.  This is fairly unlikely to be
 * a problem in practice, especially if one is holding an exclusive lock on
 * the relation identified by name1.  However, if choosing multiple names
 * within a single command, you'd better create the new object and do
 * CommandCounterIncrement before choosing the next one!
 *
 * Returns a palloc'd string.
 */
char *
ChooseRelationName(const char *name1, const char *name2,
				   const char *label, Oid namespaceid,
				   bool isconstraint)
{
	int			pass = 0;
	char	   *relname = NULL;
	char		modlabel[NAMEDATALEN];

	if (GP_ROLE_EXECUTE == Gp_role)
		elog(ERROR, "relation names cannot be chosen on QE");

	/* try the unmodified label first */
	StrNCpy(modlabel, label, sizeof(modlabel));

	for (;;)
	{
		relname = makeObjectName(name1, name2, modlabel);

		if (!OidIsValid(get_relname_relid(relname, namespaceid)))
		{
			if (!isconstraint ||
				!ConstraintNameExists(relname, namespaceid))
				break;
		}

		/* found a conflict, so try a new name component */
		pfree(relname);
		snprintf(modlabel, sizeof(modlabel), "%s%d", label, ++pass);
	}

	return relname;
}

/*
 * Select the name to be used for an index.
 *
 * The argument list is pretty ad-hoc :-(
 */
char *
ChooseIndexName(const char *tabname, Oid namespaceId,
				List *colnames, List *exclusionOpNames,
				bool primary, bool isconstraint)
{
	char	   *indexname;

	if (primary)
	{
		/* the primary key's name does not depend on the specific column(s) */
		indexname = ChooseRelationName(tabname,
									   NULL,
									   "pkey",
									   namespaceId,
									   true);
	}
	else if (exclusionOpNames != NIL)
	{
		indexname = ChooseRelationName(tabname,
									   ChooseIndexNameAddition(colnames),
									   "excl",
									   namespaceId,
									   true);
	}
	else if (isconstraint)
	{
		indexname = ChooseRelationName(tabname,
									   ChooseIndexNameAddition(colnames),
									   "key",
									   namespaceId,
									   true);
	}
	else
	{
		indexname = ChooseRelationName(tabname,
									   ChooseIndexNameAddition(colnames),
									   "idx",
									   namespaceId,
									   false);
	}

	return indexname;
}

/*
 * Generate "name2" for a new index given the list of column names for it
 * (as produced by ChooseIndexColumnNames).  This will be passed to
 * ChooseRelationName along with the parent table name and a suitable label.
 *
 * We know that less than NAMEDATALEN characters will actually be used,
 * so we can truncate the result once we've generated that many.
 *
 * XXX See also ChooseForeignKeyConstraintNameAddition and
 * ChooseExtendedStatisticNameAddition.
 */
static char *
ChooseIndexNameAddition(List *colnames)
{
	char		buf[NAMEDATALEN * 2];
	int			buflen = 0;
	ListCell   *lc;

	buf[0] = '\0';
	foreach(lc, colnames)
	{
		const char *name = (const char *) lfirst(lc);

		if (buflen > 0)
			buf[buflen++] = '_';	/* insert _ between names */

		/*
		 * At this point we have buflen <= NAMEDATALEN.  name should be less
		 * than NAMEDATALEN already, but use strlcpy for paranoia.
		 */
		strlcpy(buf + buflen, name, NAMEDATALEN);
		buflen += strlen(buf + buflen);
		if (buflen >= NAMEDATALEN)
			break;
	}
	return pstrdup(buf);
}

/*
 * Select the actual names to be used for the columns of an index, given the
 * list of IndexElems for the columns.  This is mostly about ensuring the
 * names are unique so we don't get a conflicting-attribute-names error.
 *
 * Returns a List of plain strings (char *, not String nodes).
 */
List *
ChooseIndexColumnNames(List *indexElems)
{
	List	   *result = NIL;
	ListCell   *lc;

	foreach(lc, indexElems)
	{
		IndexElem  *ielem = (IndexElem *) lfirst(lc);
		const char *origname;
		const char *curname;
		int			i;
		char		buf[NAMEDATALEN];

		/* Get the preliminary name from the IndexElem */
		if (ielem->indexcolname)
			origname = ielem->indexcolname; /* caller-specified name */
		else if (ielem->name)
			origname = ielem->name; /* simple column reference */
		else
			origname = "expr";	/* default name for expression */

		/* If it conflicts with any previous column, tweak it */
		curname = origname;
		for (i = 1;; i++)
		{
			ListCell   *lc2;
			char		nbuf[32];
			int			nlen;

			foreach(lc2, result)
			{
				if (strcmp(curname, (char *) lfirst(lc2)) == 0)
					break;
			}
			if (lc2 == NULL)
				break;			/* found nonconflicting name */

			sprintf(nbuf, "%d", i);

			/* Ensure generated names are shorter than NAMEDATALEN */
			nlen = pg_mbcliplen(origname, strlen(origname),
								NAMEDATALEN - 1 - strlen(nbuf));
			memcpy(buf, origname, nlen);
			strcpy(buf + nlen, nbuf);
			curname = buf;
		}

		/* And attach to the result list */
		result = lappend(result, pstrdup(curname));
	}
	return result;
}

/*
 * ReindexIndex
 *		Recreate a specific index.
 */
void
ReindexIndex(ReindexStmt *stmt)
{
	RangeVar   *indexRelation = stmt->relation;
	int			options = stmt->options;
	bool		concurrent = stmt->concurrent;
	struct ReindexIndexCallbackState state;
	Oid			indOid;
	Relation	irel;
	char		persistence;

	/*
	 * Find and lock index, and check permissions on table; use callback to
	 * obtain lock on table first, to avoid deadlock hazard.  The lock level
	 * used here must match the index lock obtained in reindex_index().
	 */
	state.concurrent = concurrent;
	state.locked_table_oid = InvalidOid;
	indOid = RangeVarGetRelidExtended(indexRelation,
									  concurrent ? ShareUpdateExclusiveLock : AccessExclusiveLock,
									  0,
									  RangeVarCallbackForReindexIndex,
									  &state);

	/*
	 * Obtain the current persistence of the existing index.  We already hold
	 * lock on the index.
	 */
	irel = index_open(indOid, NoLock);

	if (irel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
	{
		ReindexPartitionedIndex(irel);
		return;
	}

	persistence = irel->rd_rel->relpersistence;
	index_close(irel, NoLock);

	if (concurrent)
		ReindexRelationConcurrently(indOid, options);
	else
		reindex_index(indOid, false, persistence, options);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT,
									GetAssignedOidsForDispatch(),
									NULL);
	}
}

/*
 * Check permissions on table before acquiring relation lock; also lock
 * the heap before the RangeVarGetRelidExtended takes the index lock, to avoid
 * deadlocks.
 */
static void
RangeVarCallbackForReindexIndex(const RangeVar *relation,
								Oid relId, Oid oldRelId, void *arg)
{
	char		relkind;
	struct ReindexIndexCallbackState *state = arg;
	LOCKMODE	table_lockmode;

	/*
	 * Lock level here should match table lock in reindex_index() for
	 * non-concurrent case and table locks used by index_concurrently_*() for
	 * concurrent case.
	 */
	table_lockmode = state->concurrent ? ShareUpdateExclusiveLock : ShareLock;

	/*
	 * If we previously locked some other index's heap, and the name we're
	 * looking up no longer refers to that relation, release the now-useless
	 * lock.
	 */
	if (relId != oldRelId && OidIsValid(oldRelId))
	{
		UnlockRelationOid(state->locked_table_oid, table_lockmode);
		state->locked_table_oid = InvalidOid;
	}

	/* If the relation does not exist, there's nothing more to do. */
	if (!OidIsValid(relId))
		return;

	/*
	 * If the relation does exist, check whether it's an index.  But note that
	 * the relation might have been dropped between the time we did the name
	 * lookup and now.  In that case, there's nothing to do.
	 */
	relkind = get_rel_relkind(relId);
	if (!relkind)
		return;
	if (relkind != RELKIND_INDEX &&
		relkind != RELKIND_PARTITIONED_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index", relation->relname)));

	/* Check permissions */
	if (!pg_class_ownercheck(relId, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_INDEX, relation->relname);

	/* Lock heap before index to avoid deadlock. */
	if (relId != oldRelId)
	{
		Oid			table_oid = IndexGetRelation(relId, true);

		/*
		 * If the OID isn't valid, it means the index was concurrently
		 * dropped, which is not a problem for us; just return normally.
		 */
		if (OidIsValid(table_oid))
		{
			LockRelationOid(table_oid, table_lockmode);
			state->locked_table_oid = table_oid;
		}
	}
}

/*
 * ReindexTable
 *		Recreate all indexes of a table (and of its toast table, if any)
 *
 * GPDB: isTopLevel is not exist in upstream, but since we support reindex
 * on partitioned table, this will have non-rollback-able side effects,
 * so we need to make sure reindex on partitioned table must not run inside
 * a transaction block.
 */
Oid
ReindexTable(ReindexStmt *stmt, bool isTopLevel)
{
	RangeVar   *relation = stmt->relation;
	int			options = stmt->options;
	bool		concurrent = stmt->concurrent;
	Oid			heapOid;
	bool		result;

	if (Gp_role == GP_ROLE_EXECUTE)
	{
		reindex_relation(stmt->relid,
						 REINDEX_REL_PROCESS_TOAST |
						 REINDEX_REL_CHECK_CONSTRAINTS,
						 options);
		return stmt->relid;
	}

	/* The lock level used here should match reindex_relation(). */
	heapOid = RangeVarGetRelidExtended(relation,
									   concurrent ? ShareUpdateExclusiveLock : ShareLock,
									   0,
									   RangeVarCallbackOwnsTable, NULL);

	/*
	 * PostgreSQL doesn't allow REINDEX on a partitioned table, but we support
	 * it in Greenplum.
	 */
	if (get_rel_relkind(heapOid) == RELKIND_PARTITIONED_TABLE)
	{
		List	   *prels;

		/*
		 * To prevent non-rollback-able side effects, this case isn't allowed
		 * within a transaction block. There are numerous other subtle
		 * dependencies on this, too, like pl/sql execution.
		 */
		PreventInTransactionBlock(isTopLevel, "REINDEX TABLE(on partitioned table)");

		prels = find_all_inheritors(heapOid,
									concurrent ? ShareUpdateExclusiveLock : ShareLock,
									NULL);

		ReindexRelationList(prels, options | REINDEX_REL_RECURSING_PARTITIONED_TABLE,
							concurrent, false);
		return heapOid;
	}

	if (concurrent)
	{
		result = ReindexRelationConcurrently(heapOid, options);

		if (!result)
			ereport(NOTICE,
					(errmsg("table \"%s\" has no indexes that can be reindexed concurrently",
							relation->relname)));
	}
	else
	{
		result = reindex_relation(heapOid,
								  REINDEX_REL_PROCESS_TOAST |
								  REINDEX_REL_CHECK_CONSTRAINTS,
								  options);
		if (!result)
			ereport(NOTICE,
					(errmsg("table \"%s\" has no indexes to reindex",
							relation->relname)));
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		ReindexStmt	   *qestmt;

		qestmt = makeNode(ReindexStmt);

		qestmt->kind = REINDEX_OBJECT_TABLE;
		qestmt->relation = NULL;
		qestmt->options = options;
		qestmt->concurrent = concurrent;
		qestmt->relid = heapOid;

		PushActiveSnapshot(GetTransactionSnapshot());
		CdbDispatchUtilityStatement((Node *) qestmt,
									DF_CANCEL_ON_ERROR |
									DF_WITH_SNAPSHOT,
									GetAssignedOidsForDispatch(),
									NULL);
		PopActiveSnapshot();
	}

	return heapOid;
}

/*
 * ReindexMultipleTables
 *		Recreate indexes of tables selected by objectName/objectKind.
 *
 * To reduce the probability of deadlocks, each table is reindexed in a
 * separate transaction, so we can release the lock on it right away.
 * That means this must not be called within a user transaction block!
 */
void
ReindexMultipleTables(const char *objectName, ReindexObjectType objectKind,
					  int options, bool concurrent)
{
	Oid			objectOid;
	Relation	relationRelation;
	TableScanDesc scan;
	ScanKeyData scan_keys[1];
	HeapTuple	tuple;
	MemoryContext private_context;
	MemoryContext old;
	List	   *relids = NIL;
	int			num_keys;
	bool		concurrent_warning = false;

	Assert(Gp_role != GP_ROLE_EXECUTE);
	AssertArg(objectName);
	Assert(objectKind == REINDEX_OBJECT_SCHEMA ||
		   objectKind == REINDEX_OBJECT_SYSTEM ||
		   objectKind == REINDEX_OBJECT_DATABASE);

	if (objectKind == REINDEX_OBJECT_SYSTEM && concurrent)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot reindex system catalogs concurrently")));

	SIMPLE_FAULT_INJECTOR("reindex_db");

	/*
	 * Get OID of object to reindex, being the database currently being used
	 * by session for a database or for system catalogs, or the schema defined
	 * by caller. At the same time do permission checks that need different
	 * processing depending on the object type.
	 */
	if (objectKind == REINDEX_OBJECT_SCHEMA)
	{
		objectOid = get_namespace_oid(objectName, false);

		if (!pg_namespace_ownercheck(objectOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SCHEMA,
						   objectName);
	}
	else
	{
		objectOid = MyDatabaseId;

		if (strcmp(objectName, get_database_name(objectOid)) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("can only reindex the currently open database")));
		if (!pg_database_ownercheck(objectOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE,
						   objectName);
	}

	/*
	 * Create a memory context that will survive forced transaction commits we
	 * do below.  Since it is a child of PortalContext, it will go away
	 * eventually even if we suffer an error; there's no need for special
	 * abort cleanup logic.
	 */
	private_context = AllocSetContextCreate(PortalContext,
											"ReindexMultipleTables",
											ALLOCSET_SMALL_SIZES);

	/*
	 * Define the search keys to find the objects to reindex. For a schema, we
	 * select target relations using relnamespace, something not necessary for
	 * a database-wide operation.
	 */
	if (objectKind == REINDEX_OBJECT_SCHEMA)
	{
		num_keys = 1;
		ScanKeyInit(&scan_keys[0],
					Anum_pg_class_relnamespace,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(objectOid));
	}
	else
		num_keys = 0;

	/*
	 * Scan pg_class to build a list of the relations we need to reindex.
	 *
	 * We only consider plain relations and materialized views here (toast
	 * rels will be processed indirectly by reindex_relation).
	 */
	relationRelation = table_open(RelationRelationId, AccessShareLock);
	scan = table_beginscan_catalog(relationRelation, num_keys, scan_keys);
	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class classtuple = (Form_pg_class) GETSTRUCT(tuple);
		Oid			relid = classtuple->oid;

		/*
		 * Only regular tables and matviews can have indexes, so ignore any
		 * other kind of relation.
		 *
		 * It is tempting to also consider partitioned tables here, but that
		 * has the problem that if the children are in the same schema, they
		 * would be processed twice.  Maybe we could have a separate list of
		 * partitioned tables, and expand that afterwards into relids,
		 * ignoring any duplicates.
		 */
		if (classtuple->relkind != RELKIND_RELATION &&
			classtuple->relkind != RELKIND_MATVIEW)
			continue;

		/* Skip temp tables of other backends; we can't reindex them at all */
		if (classtuple->relpersistence == RELPERSISTENCE_TEMP &&
			!isTempNamespace(classtuple->relnamespace))
			continue;

		/* Check user/system classification, and optionally skip */
		if (objectKind == REINDEX_OBJECT_SYSTEM &&
			!IsSystemClass(relid, classtuple))
			continue;

		/*
		 * The table can be reindexed if the user is superuser, the table
		 * owner, or the database/schema owner (but in the latter case, only
		 * if it's not a shared relation).  pg_class_ownercheck includes the
		 * superuser case, and depending on objectKind we already know that
		 * the user has permission to run REINDEX on this database or schema
		 * per the permission checks at the beginning of this routine.
		 */
		if (classtuple->relisshared &&
			!pg_class_ownercheck(relid, GetUserId()))
			continue;

		/*
		 * Skip system tables, since index_create() would reject indexing them
		 * concurrently (and it would likely fail if we tried).
		 */
		if (concurrent &&
			IsCatalogRelationOid(relid))
		{
			if (!concurrent_warning)
				ereport(WARNING,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot reindex system catalogs concurrently, skipping all")));
			concurrent_warning = true;
			continue;
		}

		/* Save the list of relation OIDs in private context */
		old = MemoryContextSwitchTo(private_context);

		/*
		 * We always want to reindex pg_class first if it's selected to be
		 * reindexed.  This ensures that if there is any corruption in
		 * pg_class' indexes, they will be fixed before we process any other
		 * tables.  This is critical because reindexing itself will try to
		 * update pg_class.
		 */
		if (relid == RelationRelationId)
			relids = lcons_oid(relid, relids);
		else
			relids = lappend_oid(relids, relid);

		MemoryContextSwitchTo(old);
	}
	table_endscan(scan);
	table_close(relationRelation, AccessShareLock);

	ReindexRelationList(relids, options, concurrent, true);

	MemoryContextDelete(private_context);

}

/*
 * Perform REINDEX on each relation of the relids list.  The function
 * opens and closes a transaction per relation.  This is designed for
 * QD/utility, and is not useful for QE.
 */
static void
ReindexRelationList(List *relids, int options, bool concurrent, bool multiple)
{
	ListCell   *l;

	/* Now reindex each rel in a separate transaction */
	PopActiveSnapshot();
	CommitTransactionCommand();
	foreach(l, relids)
	{
		Oid			relid = lfirst_oid(l);
		bool		result;

		StartTransactionCommand();
		/* functions in indexes may want a snapshot set */
		PushActiveSnapshot(GetTransactionSnapshot());

		if (concurrent)
		{
			result = ReindexRelationConcurrently(relid, options);
			/* ReindexRelationConcurrently() does the verbose output */
		}
		else
		{
			result = reindex_relation(relid,
									  REINDEX_REL_PROCESS_TOAST |
									  REINDEX_REL_CHECK_CONSTRAINTS,
									  options);

			if (result && (options & REINDEXOPT_VERBOSE))
				ereport(INFO,
						(errmsg("table \"%s.%s\" was reindexed",
								get_namespace_name(get_rel_namespace(relid)),
								get_rel_name(relid))));

			PopActiveSnapshot();
		}

		/* Dispatch a separate REINDEX command for each table. */
		if (result && Gp_role == GP_ROLE_DISPATCH)
		{
			ReindexStmt	   *stmt;

			stmt = makeNode(ReindexStmt);

			stmt->kind = REINDEX_OBJECT_TABLE;
			stmt->relation = NULL;
			stmt->options = options;
			stmt->concurrent = concurrent;
			stmt->relid = relid;

			PushActiveSnapshot(GetTransactionSnapshot());
			CdbDispatchUtilityStatement((Node *) stmt,
										DF_CANCEL_ON_ERROR |
										DF_WITH_SNAPSHOT,
										GetAssignedOidsForDispatch(),
										NULL);
			PopActiveSnapshot();
		}

		CommitTransactionCommand();
	}
	StartTransactionCommand();
}


/*
 * ReindexRelationConcurrently - process REINDEX CONCURRENTLY for given
 * relation OID
 *
 * 'relationOid' can either belong to an index, a table or a materialized
 * view.  For tables and materialized views, all its indexes will be rebuilt,
 * excluding invalid indexes and any indexes used in exclusion constraints,
 * but including its associated toast table indexes.  For indexes, the index
 * itself will be rebuilt.  If 'relationOid' belongs to a partitioned table
 * then we issue a warning to mention these are not yet supported.
 *
 * The locks taken on parent tables and involved indexes are kept until the
 * transaction is committed, at which point a session lock is taken on each
 * relation.  Both of these protect against concurrent schema changes.
 *
 * Returns true if any indexes have been rebuilt (including toast table's
 * indexes, when relevant), otherwise returns false.
 */
static bool
ReindexRelationConcurrently(Oid relationOid, int options)
{
	List	   *heapRelationIds = NIL;
	List	   *indexIds = NIL;
	List	   *newIndexIds = NIL;
	List	   *relationLocks = NIL;
	List	   *lockTags = NIL;
	ListCell   *lc,
			   *lc2;
	MemoryContext private_context;
	MemoryContext oldcontext;
	char		relkind;
	char	   *relationName = NULL;
	char	   *relationNamespace = NULL;
	PGRUsage	ru0;

	/*
	 * Create a memory context that will survive forced transaction commits we
	 * do below.  Since it is a child of PortalContext, it will go away
	 * eventually even if we suffer an error; there's no need for special
	 * abort cleanup logic.
	 */
	private_context = AllocSetContextCreate(PortalContext,
											"ReindexConcurrent",
											ALLOCSET_SMALL_SIZES);

	if (options & REINDEXOPT_VERBOSE)
	{
		/* Save data needed by REINDEX VERBOSE in private context */
		oldcontext = MemoryContextSwitchTo(private_context);

		relationName = get_rel_name(relationOid);
		relationNamespace = get_namespace_name(get_rel_namespace(relationOid));

		pg_rusage_init(&ru0);

		MemoryContextSwitchTo(oldcontext);
	}

	relkind = get_rel_relkind(relationOid);

	/*
	 * Extract the list of indexes that are going to be rebuilt based on the
	 * list of relation Oids given by caller.
	 */
	switch (relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_MATVIEW:
		case RELKIND_TOASTVALUE:
			{
				/*
				 * In the case of a relation, find all its indexes including
				 * toast indexes.
				 */
				Relation	heapRelation;

				/* Save the list of relation OIDs in private context */
				oldcontext = MemoryContextSwitchTo(private_context);

				/* Track this relation for session locks */
				heapRelationIds = lappend_oid(heapRelationIds, relationOid);

				MemoryContextSwitchTo(oldcontext);

				if (IsCatalogRelationOid(relationOid))
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot reindex system catalogs concurrently")));

				/* Open relation to get its indexes */
				heapRelation = table_open(relationOid, ShareUpdateExclusiveLock);

				/* Add all the valid indexes of relation to list */
				foreach(lc, RelationGetIndexList(heapRelation))
				{
					Oid			cellOid = lfirst_oid(lc);
					Relation	indexRelation = index_open(cellOid,
														   ShareUpdateExclusiveLock);

					if (!indexRelation->rd_index->indisvalid)
						ereport(WARNING,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot reindex invalid index \"%s.%s\" concurrently, skipping",
										get_namespace_name(get_rel_namespace(cellOid)),
										get_rel_name(cellOid))));
					else if (indexRelation->rd_index->indisexclusion)
						ereport(WARNING,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot reindex exclusion constraint index \"%s.%s\" concurrently, skipping",
										get_namespace_name(get_rel_namespace(cellOid)),
										get_rel_name(cellOid))));
					else
					{
						/* Save the list of relation OIDs in private context */
						oldcontext = MemoryContextSwitchTo(private_context);

						indexIds = lappend_oid(indexIds, cellOid);

						MemoryContextSwitchTo(oldcontext);
					}

					index_close(indexRelation, NoLock);
				}

				/* Also add the toast indexes */
				if (OidIsValid(heapRelation->rd_rel->reltoastrelid))
				{
					Oid			toastOid = heapRelation->rd_rel->reltoastrelid;
					Relation	toastRelation = table_open(toastOid,
														   ShareUpdateExclusiveLock);

					/* Save the list of relation OIDs in private context */
					oldcontext = MemoryContextSwitchTo(private_context);

					/* Track this relation for session locks */
					heapRelationIds = lappend_oid(heapRelationIds, toastOid);

					MemoryContextSwitchTo(oldcontext);

					foreach(lc2, RelationGetIndexList(toastRelation))
					{
						Oid			cellOid = lfirst_oid(lc2);
						Relation	indexRelation = index_open(cellOid,
															   ShareUpdateExclusiveLock);

						if (!indexRelation->rd_index->indisvalid)
							ereport(WARNING,
									(errcode(ERRCODE_INDEX_CORRUPTED),
									 errmsg("cannot reindex invalid index \"%s.%s\" concurrently, skipping",
											get_namespace_name(get_rel_namespace(cellOid)),
											get_rel_name(cellOid))));
						else
						{
							/*
							 * Save the list of relation OIDs in private
							 * context
							 */
							oldcontext = MemoryContextSwitchTo(private_context);

							indexIds = lappend_oid(indexIds, cellOid);

							MemoryContextSwitchTo(oldcontext);
						}

						index_close(indexRelation, NoLock);
					}

					table_close(toastRelation, NoLock);
				}

				table_close(heapRelation, NoLock);
				break;
			}
		case RELKIND_INDEX:
			{
				Oid			heapId = IndexGetRelation(relationOid, false);

				if (IsCatalogRelationOid(heapId))
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot reindex system catalogs concurrently")));

				/* Save the list of relation OIDs in private context */
				oldcontext = MemoryContextSwitchTo(private_context);

				/* Track the heap relation of this index for session locks */
				heapRelationIds = list_make1_oid(heapId);

				/*
				 * Save the list of relation OIDs in private context.  Note
				 * that invalid indexes are allowed here.
				 */
				indexIds = lappend_oid(indexIds, relationOid);

				MemoryContextSwitchTo(oldcontext);
				break;
			}
		case RELKIND_PARTITIONED_TABLE:
			/* see reindex_relation() */
			ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("REINDEX of partitioned tables is not yet implemented, skipping \"%s\"",
							get_rel_name(relationOid))));
			return false;
		default:
			/* Return error if type of relation is not supported */
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot reindex this type of relation concurrently")));
			break;
	}

	/* Definitely no indexes, so leave */
	if (indexIds == NIL)
	{
		PopActiveSnapshot();
		return false;
	}

	Assert(heapRelationIds != NIL);

	/*-----
	 * Now we have all the indexes we want to process in indexIds.
	 *
	 * The phases now are:
	 *
	 * 1. create new indexes in the catalog
	 * 2. build new indexes
	 * 3. let new indexes catch up with tuples inserted in the meantime
	 * 4. swap index names
	 * 5. mark old indexes as dead
	 * 6. drop old indexes
	 *
	 * We process each phase for all indexes before moving to the next phase,
	 * for efficiency.
	 */

	/*
	 * Phase 1 of REINDEX CONCURRENTLY
	 *
	 * Create a new index with the same properties as the old one, but it is
	 * only registered in catalogs and will be built later.  Then get session
	 * locks on all involved tables.  See analogous code in DefineIndex() for
	 * more detailed comments.
	 */

	foreach(lc, indexIds)
	{
		char	   *concurrentName;
		Oid			indexId = lfirst_oid(lc);
		Oid			newIndexId;
		Relation	indexRel;
		Relation	heapRel;
		Relation	newIndexRel;
		LockRelId  *lockrelid;

		indexRel = index_open(indexId, ShareUpdateExclusiveLock);
		heapRel = table_open(indexRel->rd_index->indrelid,
							 ShareUpdateExclusiveLock);

		pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX,
									  RelationGetRelid(heapRel));
		pgstat_progress_update_param(PROGRESS_CREATEIDX_COMMAND,
									 PROGRESS_CREATEIDX_COMMAND_REINDEX_CONCURRENTLY);
		pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID,
									 indexId);
		pgstat_progress_update_param(PROGRESS_CREATEIDX_ACCESS_METHOD_OID,
									 indexRel->rd_rel->relam);

		/* Choose a temporary relation name for the new index */
		concurrentName = ChooseRelationName(get_rel_name(indexId),
											NULL,
											"ccnew",
											get_rel_namespace(indexRel->rd_index->indrelid),
											false);

		/* Create new index definition based on given index */
		newIndexId = index_concurrently_create_copy(heapRel,
													indexId,
													concurrentName);

		/* Now open the relation of the new index, a lock is also needed on it */
		newIndexRel = index_open(indexId, ShareUpdateExclusiveLock);

		/*
		 * Save the list of OIDs and locks in private context
		 */
		oldcontext = MemoryContextSwitchTo(private_context);

		newIndexIds = lappend_oid(newIndexIds, newIndexId);

		/*
		 * Save lockrelid to protect each relation from drop then close
		 * relations. The lockrelid on parent relation is not taken here to
		 * avoid multiple locks taken on the same relation, instead we rely on
		 * parentRelationIds built earlier.
		 */
		lockrelid = palloc(sizeof(*lockrelid));
		*lockrelid = indexRel->rd_lockInfo.lockRelId;
		relationLocks = lappend(relationLocks, lockrelid);
		lockrelid = palloc(sizeof(*lockrelid));
		*lockrelid = newIndexRel->rd_lockInfo.lockRelId;
		relationLocks = lappend(relationLocks, lockrelid);

		MemoryContextSwitchTo(oldcontext);

		index_close(indexRel, NoLock);
		index_close(newIndexRel, NoLock);
		table_close(heapRel, NoLock);
	}

	/*
	 * Save the heap lock for following visibility checks with other backends
	 * might conflict with this session.
	 */
	foreach(lc, heapRelationIds)
	{
		Relation	heapRelation = table_open(lfirst_oid(lc), ShareUpdateExclusiveLock);
		LockRelId  *lockrelid;
		LOCKTAG    *heaplocktag;

		/* Save the list of locks in private context */
		oldcontext = MemoryContextSwitchTo(private_context);

		/* Add lockrelid of heap relation to the list of locked relations */
		lockrelid = palloc(sizeof(*lockrelid));
		*lockrelid = heapRelation->rd_lockInfo.lockRelId;
		relationLocks = lappend(relationLocks, lockrelid);

		heaplocktag = (LOCKTAG *) palloc(sizeof(LOCKTAG));

		/* Save the LOCKTAG for this parent relation for the wait phase */
		SET_LOCKTAG_RELATION(*heaplocktag, lockrelid->dbId, lockrelid->relId);
		lockTags = lappend(lockTags, heaplocktag);

		MemoryContextSwitchTo(oldcontext);

		/* Close heap relation */
		table_close(heapRelation, NoLock);
	}

	/* Get a session-level lock on each table. */
	foreach(lc, relationLocks)
	{
		LockRelId  *lockrelid = (LockRelId *) lfirst(lc);

		LockRelationIdForSession(lockrelid, ShareUpdateExclusiveLock);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();
	StartTransactionCommand();

	/*
	 * Phase 2 of REINDEX CONCURRENTLY
	 *
	 * Build the new indexes in a separate transaction for each index to avoid
	 * having open transactions for an unnecessary long time.  But before
	 * doing that, wait until no running transactions could have the table of
	 * the index open with the old list of indexes.  See "phase 2" in
	 * DefineIndex() for more details.
	 */

	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_1);
	WaitForLockersMultiple(lockTags, ShareLock, true);
	CommitTransactionCommand();

	forboth(lc, indexIds, lc2, newIndexIds)
	{
		Relation	indexRel;
		Oid			oldIndexId = lfirst_oid(lc);
		Oid			newIndexId = lfirst_oid(lc2);
		Oid			heapId;

		CHECK_FOR_INTERRUPTS();

		/* Start new transaction for this index's concurrent build */
		StartTransactionCommand();

		/* Set ActiveSnapshot since functions in the indexes may need it */
		PushActiveSnapshot(GetTransactionSnapshot());

		/*
		 * Index relation has been closed by previous commit, so reopen it to
		 * get its information.
		 */
		indexRel = index_open(oldIndexId, ShareUpdateExclusiveLock);
		heapId = indexRel->rd_index->indrelid;
		index_close(indexRel, NoLock);

		/* Perform concurrent build of new index */
		index_concurrently_build(heapId, newIndexId);

		PopActiveSnapshot();
		CommitTransactionCommand();
	}
	StartTransactionCommand();

	/*
	 * Phase 3 of REINDEX CONCURRENTLY
	 *
	 * During this phase the old indexes catch up with any new tuples that
	 * were created during the previous phase.  See "phase 3" in DefineIndex()
	 * for more details.
	 */

	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_2);
	WaitForLockersMultiple(lockTags, ShareLock, true);
	CommitTransactionCommand();

	foreach(lc, newIndexIds)
	{
		Oid			newIndexId = lfirst_oid(lc);
		Oid			heapId;
		TransactionId limitXmin;
		Snapshot	snapshot;

		CHECK_FOR_INTERRUPTS();

		StartTransactionCommand();

		heapId = IndexGetRelation(newIndexId, false);

		/*
		 * Take the "reference snapshot" that will be used by validate_index()
		 * to filter candidate tuples.
		 */
		snapshot = RegisterSnapshot(GetTransactionSnapshot());
		PushActiveSnapshot(snapshot);

		validate_index(heapId, newIndexId, snapshot);

		/*
		 * We can now do away with our active snapshot, we still need to save
		 * the xmin limit to wait for older snapshots.
		 */
		limitXmin = snapshot->xmin;

		PopActiveSnapshot();
		UnregisterSnapshot(snapshot);

		/*
		 * To ensure no deadlocks, we must commit and start yet another
		 * transaction, and do our wait before any snapshot has been taken in
		 * it.
		 */
		CommitTransactionCommand();
		StartTransactionCommand();

		/*
		 * The index is now valid in the sense that it contains all currently
		 * interesting tuples.  But since it might not contain tuples deleted
		 * just before the reference snap was taken, we have to wait out any
		 * transactions that might have older snapshots.
		 */
		pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
									 PROGRESS_CREATEIDX_PHASE_WAIT_3);
		WaitForOlderSnapshots(limitXmin, true);

		CommitTransactionCommand();
	}

	/*
	 * Phase 4 of REINDEX CONCURRENTLY
	 *
	 * Now that the new indexes have been validated, swap each new index with
	 * its corresponding old index.
	 *
	 * We mark the new indexes as valid and the old indexes as not valid at
	 * the same time to make sure we only get constraint violations from the
	 * indexes with the correct names.
	 */

	StartTransactionCommand();

	forboth(lc, indexIds, lc2, newIndexIds)
	{
		char	   *oldName;
		Oid			oldIndexId = lfirst_oid(lc);
		Oid			newIndexId = lfirst_oid(lc2);
		Oid			heapId;

		CHECK_FOR_INTERRUPTS();

		heapId = IndexGetRelation(oldIndexId, false);

		/* Choose a relation name for old index */
		oldName = ChooseRelationName(get_rel_name(oldIndexId),
									 NULL,
									 "ccold",
									 get_rel_namespace(heapId),
									 false);

		/*
		 * Swap old index with the new one.  This also marks the new one as
		 * valid and the old one as not valid.
		 */
		index_concurrently_swap(newIndexId, oldIndexId, oldName);

		/*
		 * Invalidate the relcache for the table, so that after this commit
		 * all sessions will refresh any cached plans that might reference the
		 * index.
		 */
		CacheInvalidateRelcacheByRelid(heapId);

		/*
		 * CCI here so that subsequent iterations see the oldName in the
		 * catalog and can choose a nonconflicting name for their oldName.
		 * Otherwise, this could lead to conflicts if a table has two indexes
		 * whose names are equal for the first NAMEDATALEN-minus-a-few
		 * characters.
		 */
		CommandCounterIncrement();
	}

	/* Commit this transaction and make index swaps visible */
	CommitTransactionCommand();
	StartTransactionCommand();

	/*
	 * Phase 5 of REINDEX CONCURRENTLY
	 *
	 * Mark the old indexes as dead.  First we must wait until no running
	 * transaction could be using the index for a query.  See also
	 * index_drop() for more details.
	 */

	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_4);
	WaitForLockersMultiple(lockTags, AccessExclusiveLock, true);

	foreach(lc, indexIds)
	{
		Oid			oldIndexId = lfirst_oid(lc);
		Oid			heapId;

		CHECK_FOR_INTERRUPTS();
		heapId = IndexGetRelation(oldIndexId, false);
		index_concurrently_set_dead(heapId, oldIndexId);
	}

	/* Commit this transaction to make the updates visible. */
	CommitTransactionCommand();
	StartTransactionCommand();

	/*
	 * Phase 6 of REINDEX CONCURRENTLY
	 *
	 * Drop the old indexes.
	 */

	pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE,
								 PROGRESS_CREATEIDX_PHASE_WAIT_4);
	WaitForLockersMultiple(lockTags, AccessExclusiveLock, true);

	PushActiveSnapshot(GetTransactionSnapshot());

	{
		ObjectAddresses *objects = new_object_addresses();

		foreach(lc, indexIds)
		{
			Oid			oldIndexId = lfirst_oid(lc);
			ObjectAddress object;

			object.classId = RelationRelationId;
			object.objectId = oldIndexId;
			object.objectSubId = 0;

			add_exact_object_address(&object, objects);
		}

		/*
		 * Use PERFORM_DELETION_CONCURRENT_LOCK so that index_drop() uses the
		 * right lock level.
		 */
		performMultipleDeletions(objects, DROP_RESTRICT,
								 PERFORM_DELETION_CONCURRENT_LOCK | PERFORM_DELETION_INTERNAL);
	}

	PopActiveSnapshot();
	CommitTransactionCommand();

	/*
	 * Finally, release the session-level lock on the table.
	 */
	foreach(lc, relationLocks)
	{
		LockRelId  *lockrelid = (LockRelId *) lfirst(lc);

		UnlockRelationIdForSession(lockrelid, ShareUpdateExclusiveLock);
	}

	/* Start a new transaction to finish process properly */
	StartTransactionCommand();

	/* Log what we did */
	if (options & REINDEXOPT_VERBOSE)
	{
		if (relkind == RELKIND_INDEX)
			ereport(INFO,
					(errmsg("index \"%s.%s\" was reindexed",
							relationNamespace, relationName),
					 errdetail("%s.",
							   pg_rusage_show(&ru0))));
		else
		{
			foreach(lc, newIndexIds)
			{
				Oid			indOid = lfirst_oid(lc);

				ereport(INFO,
						(errmsg("index \"%s.%s\" was reindexed",
								get_namespace_name(get_rel_namespace(indOid)),
								get_rel_name(indOid))));
				/* Don't show rusage here, since it's not per index. */
			}

			ereport(INFO,
					(errmsg("table \"%s.%s\" was reindexed",
							relationNamespace, relationName),
					 errdetail("%s.",
							   pg_rusage_show(&ru0))));
		}
	}

	MemoryContextDelete(private_context);

	pgstat_progress_end_command();

	return true;
}

/*
 *	ReindexPartitionedIndex
 *		Reindex each child of the given partitioned index.
 *
 * Not yet implemented.
 */
static void
ReindexPartitionedIndex(Relation parentIdx)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("REINDEX is not yet implemented for partitioned indexes")));
}

/*
 * Insert or delete an appropriate pg_inherits tuple to make the given index
 * be a partition of the indicated parent index.
 *
 * This also corrects the pg_depend information for the affected index.
 */
void
IndexSetParentIndex(Relation partitionIdx, Oid parentOid)
{
	Relation	pg_inherits;
	ScanKeyData key[2];
	SysScanDesc scan;
	Oid			partRelid = RelationGetRelid(partitionIdx);
	HeapTuple	tuple;
	bool		fix_dependencies;

	/* Make sure this is an index */
	Assert(partitionIdx->rd_rel->relkind == RELKIND_INDEX ||
		   partitionIdx->rd_rel->relkind == RELKIND_PARTITIONED_INDEX);

	/*
	 * Scan pg_inherits for rows linking our index to some parent.
	 */
	pg_inherits = relation_open(InheritsRelationId, RowExclusiveLock);
	ScanKeyInit(&key[0],
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partRelid));
	ScanKeyInit(&key[1],
				Anum_pg_inherits_inhseqno,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum(1));
	scan = systable_beginscan(pg_inherits, InheritsRelidSeqnoIndexId, true,
							  NULL, 2, key);
	tuple = systable_getnext(scan);

	if (!HeapTupleIsValid(tuple))
	{
		if (parentOid == InvalidOid)
		{
			/*
			 * No pg_inherits row, and no parent wanted: nothing to do in this
			 * case.
			 */
			fix_dependencies = false;
		}
		else
		{
			Datum		values[Natts_pg_inherits];
			bool		isnull[Natts_pg_inherits];

			/*
			 * No pg_inherits row exists, and we want a parent for this index,
			 * so insert it.
			 */
			values[Anum_pg_inherits_inhrelid - 1] = ObjectIdGetDatum(partRelid);
			values[Anum_pg_inherits_inhparent - 1] =
				ObjectIdGetDatum(parentOid);
			values[Anum_pg_inherits_inhseqno - 1] = Int32GetDatum(1);
			memset(isnull, false, sizeof(isnull));

			tuple = heap_form_tuple(RelationGetDescr(pg_inherits),
									values, isnull);
			CatalogTupleInsert(pg_inherits, tuple);

			fix_dependencies = true;
		}
	}
	else
	{
		Form_pg_inherits inhForm = (Form_pg_inherits) GETSTRUCT(tuple);

		if (parentOid == InvalidOid)
		{
			/*
			 * There exists a pg_inherits row, which we want to clear; do so.
			 */
			CatalogTupleDelete(pg_inherits, &tuple->t_self);
			fix_dependencies = true;
		}
		else
		{
			/*
			 * A pg_inherits row exists.  If it's the same we want, then we're
			 * good; if it differs, that amounts to a corrupt catalog and
			 * should not happen.
			 */
			if (inhForm->inhparent != parentOid)
			{
				/* unexpected: we should not get called in this case */
				elog(ERROR, "bogus pg_inherit row: inhrelid %u inhparent %u",
					 inhForm->inhrelid, inhForm->inhparent);
			}

			/* already in the right state */
			fix_dependencies = false;
		}
	}

	/* done with pg_inherits */
	systable_endscan(scan);
	relation_close(pg_inherits, RowExclusiveLock);

	/* set relhassubclass if an index partition has been added to the parent */
	if (OidIsValid(parentOid))
		SetRelationHasSubclass(parentOid, true);

	/* set relispartition correctly on the partition */
	update_relispartition(partRelid, OidIsValid(parentOid));

	if (fix_dependencies)
	{
		/*
		 * Insert/delete pg_depend rows.  If setting a parent, add PARTITION
		 * dependencies on the parent index and the table; if removing a
		 * parent, delete PARTITION dependencies.
		 */
		if (OidIsValid(parentOid))
		{
			ObjectAddress partIdx;
			ObjectAddress parentIdx;
			ObjectAddress partitionTbl;

			ObjectAddressSet(partIdx, RelationRelationId, partRelid);
			ObjectAddressSet(parentIdx, RelationRelationId, parentOid);
			ObjectAddressSet(partitionTbl, RelationRelationId,
							 partitionIdx->rd_index->indrelid);
			recordDependencyOn(&partIdx, &parentIdx,
							   DEPENDENCY_PARTITION_PRI);
			recordDependencyOn(&partIdx, &partitionTbl,
							   DEPENDENCY_PARTITION_SEC);
		}
		else
		{
			deleteDependencyRecordsForClass(RelationRelationId, partRelid,
											RelationRelationId,
											DEPENDENCY_PARTITION_PRI);
			deleteDependencyRecordsForClass(RelationRelationId, partRelid,
											RelationRelationId,
											DEPENDENCY_PARTITION_SEC);
		}

		/* make our updates visible */
		CommandCounterIncrement();
	}
}

/*
 * Subroutine of IndexSetParentIndex to update the relispartition flag of the
 * given index to the given value.
 */
static void
update_relispartition(Oid relationId, bool newval)
{
	HeapTuple	tup;
	Relation	classRel;

	classRel = table_open(RelationRelationId, RowExclusiveLock);
	tup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relationId));
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for relation %u", relationId);
	Assert(((Form_pg_class) GETSTRUCT(tup))->relispartition != newval);
	((Form_pg_class) GETSTRUCT(tup))->relispartition = newval;
	CatalogTupleUpdate(classRel, &tup->t_self, tup);
	heap_freetuple(tup);
	table_close(classRel, RowExclusiveLock);
}
