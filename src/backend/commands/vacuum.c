/*-------------------------------------------------------------------------
 *
 * vacuum.c
 *	  The postgres vacuum cleaner.
 *
 * This file now includes only control and dispatch code for VACUUM and
 * ANALYZE commands.  Regular VACUUM is implemented in vacuumlazy.c,
 * ANALYZE in analyze.c, and VACUUM FULL is a variant of CLUSTER, handled
 * in cluster.c.
 *
 * Also have a look at vacuum_ao.c, which contains VACUUM related code for
 * Append-Optimized tables.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/vacuum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/visibilitymap.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/appendonly_compaction.h"
#include "access/appendonly_visimap.h"
#include "access/aocs_compaction.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_database.h"
#include "catalog/pg_namespace.h"
#include "commands/analyzeutils.h"
#include "commands/cluster.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdispatchresult.h"      /* CdbDispatchResults */
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"             /* pq_beginmessage() etc. */
#include "miscadmin.h"
#include "pgstat.h"
#include "parser/parse_relation.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "access/distributedlog.h"
#include "catalog/heap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_inherits_fn.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "nodes/makefuncs.h"     /* makeRangeVar */
#include "pgstat.h"
#include "utils/faultinjector.h"
#include "utils/lsyscache.h"
#include "utils/pg_rusage.h"


/*
 * GUC parameters
 */
int			vacuum_freeze_min_age;
int			vacuum_freeze_table_age;
int			vacuum_multixact_freeze_min_age;
int			vacuum_multixact_freeze_table_age;


typedef struct VacuumStatsContext
{
	List	   *updated_stats;
} VacuumStatsContext;

/* A few variables that don't seem worth passing around as parameters */
static MemoryContext vac_context = NULL;

static BufferAccessStrategy vac_strategy;

/* non-export function prototypes */
static List *get_rel_oids(Oid relid, const RangeVar *vacrel,
						  int options, List *va_cols, int stmttype);
static void vac_truncate_clog(TransactionId frozenXID,
				  MultiXactId minMulti,
				  TransactionId lastSaneFrozenXid,
				  MultiXactId lastSaneMinMulti);
static bool vacuum_rel(Oid relid, RangeVar *relation, int options, VacuumParams *params,
					   bool recursing);

static void dispatchVacuum(int options, RangeVar *relation,
						   VacuumStatsContext *ctx);

static void
vacuum_combine_stats(VacuumStatsContext *stats_context,
					CdbPgResults* cdb_pgresults);

static void
vac_update_relstats_from_list(List *updated_stats);

/*
 * Primary entry point for manual VACUUM and ANALYZE commands
 *
 * This is mainly a preparation wrapper for the real operations that will
 * happen in vacuum().
 */
void
ExecVacuum(VacuumStmt *vacstmt, bool isTopLevel)
{
	VacuumParams params;

	/* sanity checks on options */
	Assert(vacstmt->options & (VACOPT_VACUUM | VACOPT_ANALYZE));
	Assert((vacstmt->options & VACOPT_VACUUM) ||
		   !(vacstmt->options & (VACOPT_FULL | VACOPT_FREEZE)));
	Assert((vacstmt->options & VACOPT_ANALYZE) || vacstmt->va_cols == NIL);
	Assert(!(vacstmt->options & VACOPT_SKIPTOAST));

	/*
	 * All freeze ages are zero if the FREEZE option is given; otherwise pass
	 * them as -1 which means to use the default values.
	 */
	if (vacstmt->options & VACOPT_FREEZE)
	{
		params.freeze_min_age = 0;
		params.freeze_table_age = 0;
		params.multixact_freeze_min_age = 0;
		params.multixact_freeze_table_age = 0;
	}
	else
	{
		params.freeze_min_age = -1;
		params.freeze_table_age = -1;
		params.multixact_freeze_min_age = -1;
		params.multixact_freeze_table_age = -1;
	}

	/* user-invoked vacuum is never "for wraparound" */
	params.is_wraparound = false;

	/* user-invoked vacuum never uses this parameter */
	params.log_min_duration = -1;

	/* Now go through the common routine */
	vacuum(vacstmt->options, vacstmt->relation, InvalidOid, &params,
		   vacstmt->va_cols, NULL, isTopLevel);
}


/*
 * Primary entry point for VACUUM and ANALYZE commands.
 *
 * options is a bitmask of VacuumOption flags, indicating what to do.
 *
 * relid, if not InvalidOid, indicate the relation to process; otherwise,
 * the RangeVar is used.  (The latter must always be passed, because it's
 * used for error messages.)
 *
 * params contains a set of parameters that can be used to customize the
 * behavior.
 *
 * va_cols is a list of columns to analyze, or NIL to process them all.
 *
 * bstrategy is normally given as NULL, but in autovacuum it can be passed
 * in to use the same buffer strategy object across multiple vacuum() calls.
 *
 * isTopLevel should be passed down from ProcessUtility.
 *
 * It is the caller's responsibility that all parameters are allocated in a
 * memory context that will not disappear at transaction commit.
 */
void
vacuum(int options, RangeVar *relation, Oid relid, VacuumParams *params,
	   List *va_cols, BufferAccessStrategy bstrategy, bool isTopLevel)
{
	const char *stmttype;
	volatile bool in_outer_xact,
				use_own_xacts;
	List	   *vacuum_relations = NIL;
	List	   *analyze_relations = NIL;
	static bool in_vacuum = false;

	if ((options & VACOPT_VACUUM) &&
		(options & VACOPT_ROOTONLY))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("ROOTPARTITION option cannot be used together with VACUUM, try ANALYZE ROOTPARTITION")));

	Assert(params != NULL);

	stmttype = (options & VACOPT_VACUUM) ? "VACUUM" : "ANALYZE";

	/*
	 * We cannot run VACUUM inside a user transaction block; if we were inside
	 * a transaction, then our commit- and start-transaction-command calls
	 * would not have the intended effect!	There are numerous other subtle
	 * dependencies on this, too.
	 *
	 * GPDB: AO vacuum's compaction phase has to run in a distributed
	 * transaction though.
	 */
	if ((options & VACOPT_VACUUM) &&
		(options & VACUUM_AO_PHASE_MASK) == 0)
	{
		PreventTransactionChain(isTopLevel, stmttype);
		in_outer_xact = false;
	}
	else
		in_outer_xact = IsInTransactionChain(isTopLevel);

	/*
	 * Due to static variables vac_context, anl_context and vac_strategy,
	 * vacuum() is not reentrant.  This matters when VACUUM FULL or ANALYZE
	 * calls a hostile index expression that itself calls ANALYZE.
	 */
	if (in_vacuum)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("%s cannot be executed from VACUUM or ANALYZE",
						stmttype)));

	/*
	 * Sanity check DISABLE_PAGE_SKIPPING option.
	 */
	if ((options & VACOPT_FULL) != 0 &&
		(options & VACOPT_DISABLE_PAGE_SKIPPING) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("VACUUM option DISABLE_PAGE_SKIPPING cannot be used with FULL")));

	/*
	 * Send info about dead objects to the statistics collector, unless we are
	 * in autovacuum --- autovacuum.c does this for itself.
	 */
	if ((options & VACOPT_VACUUM) && !IsAutoVacuumWorkerProcess())
		pgstat_vacuum_stat();

	/*
	 * Create special memory context for cross-transaction storage.
	 *
	 * Since it is a child of PortalContext, it will go away eventually even
	 * if we suffer an error; there's no need for special abort cleanup logic.
	 */
	vac_context = AllocSetContextCreate(PortalContext,
										"Vacuum",
										ALLOCSET_DEFAULT_MINSIZE,
										ALLOCSET_DEFAULT_INITSIZE,
										ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * If caller didn't give us a buffer strategy object, make one in the
	 * cross-transaction memory context.
	 */
	if (bstrategy == NULL)
	{
		MemoryContext old_context = MemoryContextSwitchTo(vac_context);

		bstrategy = GetAccessStrategy(BAS_VACUUM);
		MemoryContextSwitchTo(old_context);
	}
	vac_strategy = bstrategy;

	/*
	 * Build list of relations to process, unless caller gave us one. (If we
	 * build one, we put it in vac_context for safekeeping.)
	 */

	/*
	 * Analyze on midlevel partition is not allowed directly so
	 * vacuum_relations and analyze_relations may be different.  In case of
	 * partitioned tables, vacuum_relation will contain all OIDs of the
	 * partitions of a partitioned table. However, analyze_relation will
	 * contain all the OIDs of partition of a partitioned table except midlevel
	 * partition unless GUC optimizer_analyze_midlevel_partition is set to on.
	 */
	if (options & VACOPT_VACUUM)
	{
		vacuum_relations = get_rel_oids(relid, relation,
										options, va_cols, VACOPT_VACUUM);
	}
	if (options & VACOPT_ANALYZE)
		analyze_relations = get_rel_oids(relid, relation,
										 options, va_cols, VACOPT_ANALYZE);

	/*
	 * Decide whether we need to start/commit our own transactions.
	 *
	 * For VACUUM (with or without ANALYZE): always do so, so that we can
	 * release locks as soon as possible.  (We could possibly use the outer
	 * transaction for a one-table VACUUM, but handling TOAST tables would be
	 * problematic.)
	 *
	 * For ANALYZE (no VACUUM): if inside a transaction block, we cannot
	 * start/commit our own transactions.  Also, there's no need to do so if
	 * only processing one relation.  For multiple relations when not within a
	 * transaction block, and also in an autovacuum worker, use own
	 * transactions so we can release locks sooner.
	 */
	if (options & VACOPT_AO_COMPACT_PHASE)
		use_own_xacts = false;
	else if (options & VACOPT_VACUUM)
		use_own_xacts = true;
	else
	{
		Assert(options & VACOPT_ANALYZE);
		if (IsAutoVacuumWorkerProcess())
			use_own_xacts = true;
		else if (in_outer_xact)
			use_own_xacts = false;
		else if (list_length(analyze_relations) > 1)
			use_own_xacts = true;
		else
			use_own_xacts = false;
	}

	/*
	 * vacuum_rel expects to be entered with no transaction active; it will
	 * start and commit its own transaction.  But we are called by an SQL
	 * command, and so we are executing inside a transaction already. We
	 * commit the transaction started in PostgresMain() here, and start
	 * another one before exiting to match the commit waiting for us back in
	 * PostgresMain().
	 */
	if (use_own_xacts)
	{
		Assert(!in_outer_xact);

		/* ActiveSnapshot is not set by autovacuum */
		if (ActiveSnapshotSet())
			PopActiveSnapshot();

		PreserveOidAssignmentsOnCommit();

		/* matches the StartTransaction in PostgresMain() */
		CommitTransactionCommand();
	}

	/* Turn vacuum cost accounting on or off */
	PG_TRY();
	{
		ListCell   *cur;

		in_vacuum = true;
		VacuumCostActive = (VacuumCostDelay > 0);
		VacuumCostBalance = 0;
		VacuumPageHit = 0;
		VacuumPageMiss = 0;
		VacuumPageDirty = 0;

		if (options & VACOPT_VACUUM)
		{
			/*
			 * Loop to process each selected relation which needs to be
			 * vacuumed.
			 */
			foreach(cur, vacuum_relations)
			{
				Oid			relid = lfirst_oid(cur);

				vacuum_rel(relid, relation, options, params, false);
			}
		}

		if (options & VACOPT_ANALYZE)
		{
			/*
			 * If there are no partition tables in the database and ANALYZE
			 * ROOTPARTITION ALL is executed, report a WARNING as no root
			 * partitions are there to be analyzed
			 */
			if ((options & VACOPT_ROOTONLY) && NIL == analyze_relations && !relation)
			{
				ereport(NOTICE,
						(errmsg("there are no partitioned tables in database to ANALYZE ROOTPARTITION")));
			}

			/*
			 * Loop to process each selected relation which needs to be analyzed.
			 */
			foreach(cur, analyze_relations)
			{
				Oid			relid = lfirst_oid(cur);

				/*
				 * If using separate xacts, start one for analyze. Otherwise,
				 * we can use the outer transaction.
				 */
				if (use_own_xacts)
				{
					StartTransactionCommand();
					/* functions in indexes may want a snapshot set */
					PushActiveSnapshot(GetTransactionSnapshot());
				}

				analyze_rel(relid, relation, options, params,
							va_cols, in_outer_xact, vac_strategy, NULL);

				if (use_own_xacts)
				{
					PopActiveSnapshot();
					CommitTransactionCommand();
				}
			}
		}
	}
	PG_CATCH();
	{
		in_vacuum = false;
		VacuumCostActive = false;
		PG_RE_THROW();
	}
	PG_END_TRY();

	in_vacuum = false;
	VacuumCostActive = false;

	/*
	 * Finish up processing.
	 */
	if (use_own_xacts)
	{
		/* here, we are not in a transaction */

		StartTransactionCommand();
		ClearOidAssignmentsOnCommit();
	}

	if ((options & VACOPT_VACUUM) && !IsAutoVacuumWorkerProcess())
	{
		/*
		 * Update pg_database.datfrozenxid, and truncate pg_clog if possible.
		 * (autovacuum.c does this for itself.)
		 */
		vac_update_datfrozenxid();
	}

	/*
	 * Clean up working storage --- note we must do this after
	 * StartTransactionCommand, else we might be trying to delete the active
	 * context!
	 */
	MemoryContextDelete(vac_context);
	vac_context = NULL;
}

bool
vacuumStatement_IsTemporary(Relation onerel)
{
	bool bTemp = false;
	/* MPP-7576: don't track internal namespace tables */
	switch (RelationGetNamespace(onerel))
	{
		case PG_CATALOG_NAMESPACE:
			/* MPP-7773: don't track objects in system namespace
			 * if modifying system tables (eg during upgrade)
			 */
			if (allowSystemTableMods)
				bTemp = true;
			break;

		case PG_TOAST_NAMESPACE:
		case PG_BITMAPINDEX_NAMESPACE:
		case PG_AOSEGMENT_NAMESPACE:
			bTemp = true;
			break;
		default:
			break;
	}

	/* MPP-7572: Don't track metadata if table in any
	 * temporary namespace
	 */
	if (!bTemp)
		bTemp = isAnyTempNamespace(RelationGetNamespace(onerel));
	return bTemp;
}

/*
 * Build a list of Oids for each relation to be processed
 *
 * The list is built in vac_context so that it will survive across our
 * per-relation transactions.
 *
 * 'stmttype' is either VACOPT_VACUUM or VACOPT_ANALYZE, to indicate
 * whether we should fetch the list for VACUUM or ANALYZE. It's
 * passed as a separate argument, so that the caller can build
 * separate lists for a combined "VACUUM ANALYZE".
 */
static List *
get_rel_oids(Oid relid, const RangeVar *vacrel,
			 int options, List *va_cols, int stmttype)
{
	List	   *oid_list = NIL;
	MemoryContext oldcontext;

	Assert(stmttype == VACOPT_VACUUM || stmttype == VACOPT_ANALYZE);

	/* OID supplied by VACUUM's caller? */
	if (OidIsValid(relid))
	{
		oldcontext = MemoryContextSwitchTo(vac_context);
		oid_list = lappend_oid(oid_list, relid);
		MemoryContextSwitchTo(oldcontext);
	}
	else if (vacrel)
	{
		if (stmttype == VACOPT_VACUUM)
		{
			/* Process a specific relation */
			Oid			relid;
			List	   *prels = NIL;

			/*
			 * Since we don't take a lock here, the relation might be gone, or the
			 * RangeVar might no longer refer to the OID we look up here.  In the
			 * former case, VACUUM will do nothing; in the latter case, it will
			 * process the OID we looked up here, rather than the new one. Neither
			 * is ideal, but there's little practical alternative, since we're
			 * going to commit this transaction and begin a new one between now
			 * and then.
			 */
			relid = RangeVarGetRelid(vacrel, NoLock, false);

			if (rel_is_partitioned(relid))
			{
				PartitionNode *pn;

				pn = get_parts(relid, 0, 0, false, true /*includesubparts*/);

				prels = all_partition_relids(pn);
			}
			else if (rel_is_child_partition(relid))
			{
				/* get my children */
				prels = find_all_inheritors(relid, NoLock, NULL);
			}

			/* Make a relation list entry for this relation */
			oldcontext = MemoryContextSwitchTo(vac_context);
			oid_list = lappend_oid(oid_list, relid);
			oid_list = list_concat_unique_oid(oid_list, prels);
			MemoryContextSwitchTo(oldcontext);
		}
		else
		{
			oldcontext = MemoryContextSwitchTo(vac_context);
			/**
			 * ANALYZE one relation (optionally, a list of columns).
			 */
			Oid relationOid = InvalidOid;

			relationOid = RangeVarGetRelid(vacrel, NoLock, false);
			PartStatus ps = rel_part_status(relationOid);

			if (ps != PART_STATUS_ROOT && (options & VACOPT_ROOTONLY))
			{
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- cannot analyze a non-root partition using ANALYZE ROOTPARTITION",
								get_rel_name(relationOid))));
			}
			else if (ps == PART_STATUS_ROOT)
			{
				PartitionNode *pn = get_parts(relationOid, 0 /*level*/ ,
											  0 /*parent*/, false /* inctemplate */, true /*includesubparts*/);
				Assert(pn);
				if (!(options & VACOPT_ROOTONLY))
				{
					oid_list = all_leaf_partition_relids(pn); /* all leaves */

					if (optimizer_analyze_midlevel_partition)
					{
						oid_list = list_concat(oid_list, all_interior_partition_relids(pn)); /* interior partitions */
					}
				}
				if (optimizer_analyze_root_partition || (options & VACOPT_ROOTONLY))
					oid_list = lappend_oid(oid_list, relationOid); /* root partition */
			}
			else if (ps == PART_STATUS_LEAF)
			{
				Oid root_rel_oid = rel_partition_get_master(relationOid);
				oid_list = list_make1_oid(relationOid);

				List *va_root_attnums = NIL;
				if (va_cols != NIL)
				{
					ListCell *lc;
					int i;
					foreach(lc, va_cols)
					{
						char	   *col = strVal(lfirst(lc));

						i = get_attnum(root_rel_oid, col);
						if (i == InvalidAttrNumber)
							ereport(ERROR,
									(errcode(ERRCODE_UNDEFINED_COLUMN),
									 errmsg("column \"%s\" of relation \"%s\" does not exist",
											col, get_rel_name(root_rel_oid))));
						va_root_attnums = lappend_int(va_root_attnums, i);
					}
				}
				else
				{
					Relation onerel = RelationIdGetRelation(root_rel_oid);
					int attr_cnt = onerel->rd_att->natts;
					for (int i = 1; i <= attr_cnt; i++)
					{
						Form_pg_attribute attr = onerel->rd_att->attrs[i-1];
						if (attr->attisdropped)
							continue;
						va_root_attnums = lappend_int(va_root_attnums, i);
					}
					RelationClose(onerel);
				}
				if (optimizer_analyze_root_partition || (options & VACOPT_ROOTONLY))
				{
					int		elevel = ((options & VACOPT_VERBOSE) ? LOG : DEBUG2);

					if (optimizer_analyze_enable_merge_of_leaf_stats &&
						leaf_parts_analyzed(root_rel_oid, relationOid, va_root_attnums, elevel))
						/* remember to merge the partition stats into the root partition */
						oid_list = lappend_oid(oid_list, root_rel_oid);
				}
			}
			else if (ps == PART_STATUS_INTERIOR) /* analyze an interior partition directly */
			{
				/* disable analyzing mid-level partitions directly since the users are encouraged
				 * to work with the root partition only. To gather stats on mid-level partitions
				 * (for Orca's use), the user should run ANALYZE or ANALYZE ROOTPARTITION on the
				 * root level with optimizer_analyze_midlevel_partition GUC set to ON.
				 * Planner uses the stats on leaf partitions, so it's unnecessary to collect stats on
				 * midlevel partitions.
				 */
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- cannot analyze a mid-level partition. "
								"Please run ANALYZE on the root partition table.",
								get_rel_name(relationOid))));
			}
			else
			{
				oid_list = list_make1_oid(relationOid);
			}
			MemoryContextSwitchTo(oldcontext);
		}
	}
	else
	{
		/*
		 * Process all plain relations and materialized views listed in
		 * pg_class
		 */
		Relation	pgclass;
		HeapScanDesc scan;
		HeapTuple	tuple;
		Oid candidateOid;
		List	   *rootParts = NIL;

		pgclass = heap_open(RelationRelationId, AccessShareLock);

		scan = heap_beginscan_catalog(pgclass, 0, NULL);

		while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
		{
			Form_pg_class classForm = (Form_pg_class) GETSTRUCT(tuple);

			/*
			 * Don't include non-vacuum-able relations:
			 *   - External tables
			 *   - Foreign tables
			 *   - etc.
			 */
			if (classForm->relkind != RELKIND_RELATION &&
				classForm->relkind != RELKIND_MATVIEW)
				continue;
			if (classForm->relstorage == RELSTORAGE_FOREIGN  ||
				classForm->relstorage == RELSTORAGE_VIRTUAL)
				continue;

			/* Make a relation list entry for this guy */
			candidateOid = HeapTupleGetOid(tuple);

			/* Skip non root partition tables if ANALYZE ROOTPARTITION ALL is executed */
			if ((options & VACOPT_ROOTONLY) && !rel_is_partitioned(candidateOid))
			{
				continue;
			}

			// skip mid-level partition tables if we have disabled collecting statistics for them
			PartStatus ps = rel_part_status(candidateOid);
			if (!optimizer_analyze_midlevel_partition && ps == PART_STATUS_INTERIOR)
			{
				continue;
			}

			// Likewise, skip root partition, if disabled.
			if (!optimizer_analyze_root_partition && (options & VACOPT_ROOTONLY) == 0 && ps == PART_STATUS_ROOT)
			{
				continue;
			}

			oldcontext = MemoryContextSwitchTo(vac_context);
			if (ps == PART_STATUS_ROOT)
				rootParts = lappend_oid(rootParts, candidateOid);
			else
				oid_list = lappend_oid(oid_list, candidateOid);
			MemoryContextSwitchTo(oldcontext);
		}

		/*
		 * Schedule the root partitions to be analyzed after all the leaves.
		 * A root partition can often be analyzed by combining the HLL
		 * counters from all the leaves, which is much cheaper than scanning
		 * the whole partitioned table, but that only works if the leaves
		 * have already been analyzed.
		 */
		oldcontext = MemoryContextSwitchTo(vac_context);
		oid_list = list_concat(oid_list, rootParts);
		MemoryContextSwitchTo(oldcontext);

		heap_endscan(scan);
		heap_close(pgclass, AccessShareLock);
	}

	return oid_list;
}

/*
 * vacuum_set_xid_limits() -- compute oldest-Xmin and freeze cutoff points
 *
 * The output parameters are:
 * - oldestXmin is the cutoff value used to distinguish whether tuples are
 *	 DEAD or RECENTLY_DEAD (see HeapTupleSatisfiesVacuum).
 * - freezeLimit is the Xid below which all Xids are replaced by
 *	 FrozenTransactionId during vacuum.
 * - xidFullScanLimit (computed from table_freeze_age parameter)
 *	 represents a minimum Xid value; a table whose relfrozenxid is older than
 *	 this will have a full-table vacuum applied to it, to freeze tuples across
 *	 the whole table.  Vacuuming a table younger than this value can use a
 *	 partial scan.
 * - multiXactCutoff is the value below which all MultiXactIds are removed from
 *	 Xmax.
 * - mxactFullScanLimit is a value against which a table's relminmxid value is
 *	 compared to produce a full-table vacuum, as with xidFullScanLimit.
 *
 * xidFullScanLimit and mxactFullScanLimit can be passed as NULL if caller is
 * not interested.
 */
void
vacuum_set_xid_limits(Relation rel,
					  int freeze_min_age,
					  int freeze_table_age,
					  int multixact_freeze_min_age,
					  int multixact_freeze_table_age,
					  TransactionId *oldestXmin,
					  TransactionId *freezeLimit,
					  TransactionId *xidFullScanLimit,
					  MultiXactId *multiXactCutoff,
					  MultiXactId *mxactFullScanLimit)
{
	int			freezemin;
	int			mxid_freezemin;
	int			effective_multixact_freeze_max_age;
	TransactionId limit;
	TransactionId safeLimit;
	MultiXactId mxactLimit;
	MultiXactId safeMxactLimit;

	/*
	 * We can always ignore processes running lazy vacuum.  This is because we
	 * use these values only for deciding which tuples we must keep in the
	 * tables.  Since lazy vacuum doesn't write its XID anywhere, it's safe to
	 * ignore it.  In theory it could be problematic to ignore lazy vacuums in
	 * a full vacuum, but keep in mind that only one vacuum process can be
	 * working on a particular table at any time, and that each vacuum is
	 * always an independent transaction.
	 */
	*oldestXmin =
		TransactionIdLimitedForOldSnapshots(GetOldestXmin(rel, true), rel);

	Assert(TransactionIdIsNormal(*oldestXmin));

	/*
	 * Determine the minimum freeze age to use: as specified by the caller, or
	 * vacuum_freeze_min_age, but in any case not more than half
	 * autovacuum_freeze_max_age, so that autovacuums to prevent XID
	 * wraparound won't occur too frequently.
	 */
	freezemin = freeze_min_age;
	if (freezemin < 0)
		freezemin = vacuum_freeze_min_age;
	freezemin = Min(freezemin, autovacuum_freeze_max_age / 2);
	Assert(freezemin >= 0);

	/*
	 * Compute the cutoff XID, being careful not to generate a "permanent" XID
	 */
	limit = *oldestXmin - freezemin;
	if (!TransactionIdIsNormal(limit))
		limit = FirstNormalTransactionId;

	/*
	 * If oldestXmin is very far back (in practice, more than
	 * autovacuum_freeze_max_age / 2 XIDs old), complain and force a minimum
	 * freeze age of zero.
	 */
	safeLimit = ReadNewTransactionId() - autovacuum_freeze_max_age;
	if (!TransactionIdIsNormal(safeLimit))
		safeLimit = FirstNormalTransactionId;

	if (TransactionIdPrecedes(limit, safeLimit))
	{
		ereport(WARNING,
				(errmsg("oldest xmin is far in the past"),
				 errhint("Close open transactions soon to avoid wraparound problems.")));
		limit = *oldestXmin;
	}

	*freezeLimit = limit;

	/*
	 * Compute the multixact age for which freezing is urgent.  This is
	 * normally autovacuum_multixact_freeze_max_age, but may be less if we are
	 * short of multixact member space.
	 */
	effective_multixact_freeze_max_age = MultiXactMemberFreezeThreshold();

	/*
	 * Determine the minimum multixact freeze age to use: as specified by
	 * caller, or vacuum_multixact_freeze_min_age, but in any case not more
	 * than half effective_multixact_freeze_max_age, so that autovacuums to
	 * prevent MultiXact wraparound won't occur too frequently.
	 */
	mxid_freezemin = multixact_freeze_min_age;
	if (mxid_freezemin < 0)
		mxid_freezemin = vacuum_multixact_freeze_min_age;
	mxid_freezemin = Min(mxid_freezemin,
						 effective_multixact_freeze_max_age / 2);
	Assert(mxid_freezemin >= 0);

	/* compute the cutoff multi, being careful to generate a valid value */
	mxactLimit = GetOldestMultiXactId() - mxid_freezemin;
	if (mxactLimit < FirstMultiXactId)
		mxactLimit = FirstMultiXactId;

	safeMxactLimit =
		ReadNextMultiXactId() - effective_multixact_freeze_max_age;
	if (safeMxactLimit < FirstMultiXactId)
		safeMxactLimit = FirstMultiXactId;

	if (MultiXactIdPrecedes(mxactLimit, safeMxactLimit))
	{
		ereport(WARNING,
				(errmsg("oldest multixact is far in the past"),
				 errhint("Close open transactions with multixacts soon to avoid wraparound problems.")));
		mxactLimit = safeMxactLimit;
	}

	*multiXactCutoff = mxactLimit;

	if (xidFullScanLimit != NULL)
	{
		int			freezetable;

		Assert(mxactFullScanLimit != NULL);

		/*
		 * Determine the table freeze age to use: as specified by the caller,
		 * or vacuum_freeze_table_age, but in any case not more than
		 * autovacuum_freeze_max_age * 0.95, so that if you have e.g nightly
		 * VACUUM schedule, the nightly VACUUM gets a chance to freeze tuples
		 * before anti-wraparound autovacuum is launched.
		 */
		freezetable = freeze_table_age;
		if (freezetable < 0)
			freezetable = vacuum_freeze_table_age;
		freezetable = Min(freezetable, autovacuum_freeze_max_age * 0.95);
		Assert(freezetable >= 0);

		/*
		 * Compute XID limit causing a full-table vacuum, being careful not to
		 * generate a "permanent" XID.
		 */
		limit = ReadNewTransactionId() - freezetable;
		if (!TransactionIdIsNormal(limit))
			limit = FirstNormalTransactionId;

		*xidFullScanLimit = limit;

		/*
		 * Similar to the above, determine the table freeze age to use for
		 * multixacts: as specified by the caller, or
		 * vacuum_multixact_freeze_table_age, but in any case not more than
		 * autovacuum_multixact_freeze_table_age * 0.95, so that if you have
		 * e.g. nightly VACUUM schedule, the nightly VACUUM gets a chance to
		 * freeze multixacts before anti-wraparound autovacuum is launched.
		 */
		freezetable = multixact_freeze_table_age;
		if (freezetable < 0)
			freezetable = vacuum_multixact_freeze_table_age;
		freezetable = Min(freezetable,
						  effective_multixact_freeze_max_age * 0.95);
		Assert(freezetable >= 0);

		/*
		 * Compute MultiXact limit causing a full-table vacuum, being careful
		 * to generate a valid MultiXact value.
		 */
		mxactLimit = ReadNextMultiXactId() - freezetable;
		if (mxactLimit < FirstMultiXactId)
			mxactLimit = FirstMultiXactId;

		*mxactFullScanLimit = mxactLimit;
	}
	else
	{
		Assert(mxactFullScanLimit == NULL);
	}
}

/*
 * vac_estimate_reltuples() -- estimate the new value for pg_class.reltuples
 *
 *		If we scanned the whole relation then we should just use the count of
 *		live tuples seen; but if we did not, we should not blindly extrapolate
 *		from that number, since VACUUM may have scanned a quite nonrandom
 *		subset of the table.  When we have only partial information, we take
 *		the old value of pg_class.reltuples as a measurement of the
 *		tuple density in the unscanned pages.
 *
 *		The is_analyze argument is historical.
 */
double
vac_estimate_reltuples(Relation relation, bool is_analyze,
					   BlockNumber total_pages,
					   BlockNumber scanned_pages,
					   double scanned_tuples)
{
	BlockNumber old_rel_pages = relation->rd_rel->relpages;
	double		old_rel_tuples = relation->rd_rel->reltuples;
	double		old_density;
	double		unscanned_pages;
	double		total_tuples;

	/* If we did scan the whole table, just use the count as-is */
	if (scanned_pages >= total_pages)
		return scanned_tuples;

	/*
	 * If scanned_pages is zero but total_pages isn't, keep the existing value
	 * of reltuples.  (Note: callers should avoid updating the pg_class
	 * statistics in this situation, since no new information has been
	 * provided.)
	 */
	if (scanned_pages == 0)
		return old_rel_tuples;

	/*
	 * If old value of relpages is zero, old density is indeterminate; we
	 * can't do much except scale up scanned_tuples to match total_pages.
	 */
	if (old_rel_pages == 0)
		return floor((scanned_tuples / scanned_pages) * total_pages + 0.5);

	/*
	 * Okay, we've covered the corner cases.  The normal calculation is to
	 * convert the old measurement to a density (tuples per page), then
	 * estimate the number of tuples in the unscanned pages using that figure,
	 * and finally add on the number of tuples in the scanned pages.
	 */
	old_density = old_rel_tuples / old_rel_pages;
	unscanned_pages = (double) total_pages - (double) scanned_pages;
	total_tuples = old_density * unscanned_pages + scanned_tuples;
	return floor(total_tuples + 0.5);
}


/*
 * Update relpages/reltuples of all the relations in the list.
 */
static void
vac_update_relstats_from_list(List *updated_stats)
{
	ListCell *lc;

	/*
	 * This function is only called in the context of the QD, so let's be
	 * explicit about that given the assumptions taken.
	 */
	Assert(Gp_role == GP_ROLE_DISPATCH);

	foreach (lc, updated_stats)
	{
		VPgClassStats *stats = (VPgClassStats *) lfirst(lc);
		Relation	rel;

		rel = relation_open(stats->relid, AccessShareLock);

		if (GpPolicyIsReplicated(rel->rd_cdbpolicy))
		{
			stats->rel_pages = stats->rel_pages / rel->rd_cdbpolicy->numsegments;
			stats->rel_tuples = stats->rel_tuples / rel->rd_cdbpolicy->numsegments;
			stats->relallvisible = stats->relallvisible / rel->rd_cdbpolicy->numsegments;
		}

		/*
		 * Pass 'false' for isvacuum, so that the stats are
		 * actually updated.
		 */
		vac_update_relstats(rel,
							stats->rel_pages, stats->rel_tuples,
							stats->relallvisible,
							rel->rd_rel->relhasindex,
							InvalidTransactionId,
							InvalidMultiXactId,
							false,
							false /* isvacuum */);
		relation_close(rel, AccessShareLock);
	}
}

/*
 *	vac_update_relstats() -- update statistics for one relation
 *
 *		Update the whole-relation statistics that are kept in its pg_class
 *		row.  There are additional stats that will be updated if we are
 *		doing ANALYZE, but we always update these stats.  This routine works
 *		for both index and heap relation entries in pg_class.
 *
 *		We violate transaction semantics here by overwriting the rel's
 *		existing pg_class tuple with the new values.  This is reasonably
 *		safe as long as we're sure that the new values are correct whether or
 *		not this transaction commits.  The reason for doing this is that if
 *		we updated these tuples in the usual way, vacuuming pg_class itself
 *		wouldn't work very well --- by the time we got done with a vacuum
 *		cycle, most of the tuples in pg_class would've been obsoleted.  Of
 *		course, this only works for fixed-size not-null columns, but these are.
 *
 *		Another reason for doing it this way is that when we are in a lazy
 *		VACUUM and have PROC_IN_VACUUM set, we mustn't do any regular updates.
 *		Somebody vacuuming pg_class might think they could delete a tuple
 *		marked with xmin = our xid.
 *
 *		In addition to fundamentally nontransactional statistics such as
 *		relpages and relallvisible, we try to maintain certain lazily-updated
 *		DDL flags such as relhasindex, by clearing them if no longer correct.
 *		It's safe to do this in VACUUM, which can't run in parallel with
 *		CREATE INDEX/RULE/TRIGGER and can't be part of a transaction block.
 *		However, it's *not* safe to do it in an ANALYZE that's within an
 *		outer transaction, because for example the current transaction might
 *		have dropped the last index; then we'd think relhasindex should be
 *		cleared, but if the transaction later rolls back this would be wrong.
 *		So we refrain from updating the DDL flags if we're inside an outer
 *		transaction.  This is OK since postponing the flag maintenance is
 *		always allowable.
 *
 *		This routine is shared by VACUUM and ANALYZE.
 */
void
vac_update_relstats(Relation relation,
					BlockNumber num_pages, double num_tuples,
					BlockNumber num_all_visible_pages,
					bool hasindex, TransactionId frozenxid,
					MultiXactId minmulti,
					bool in_outer_xact,
					bool isvacuum)
{
	Oid			relid = RelationGetRelid(relation);
	Relation	rd;
	HeapTuple	ctup;
	Form_pg_class pgcform;
	bool		dirty;

	Assert(relid != InvalidOid);

	/*
	 * In GPDB, all the data is stored in the segments, and the
	 * relpages/reltuples in the master reflect the sum of the values in
	 * all the segments. In VACUUM, don't overwrite relpages/reltuples with
	 * the values we counted in the QD node itself. We will dispatch the
	 * VACUUM to the segments after processing the QD node, and we will
	 * update relpages/reltuples then.
	 *
	 * Update stats for system tables normally, though (it'd better say
	 * "non-distributed" tables than system relations here, but for now
	 * it's effectively the same.)
	 */
	if (!IsSystemRelation(relation) && isvacuum)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			num_pages = relation->rd_rel->relpages;
			num_tuples = relation->rd_rel->reltuples;
			num_all_visible_pages = relation->rd_rel->relallvisible;
		}
		else if (Gp_role == GP_ROLE_EXECUTE)
		{
			/*
			 * CDB: Build a special message, to send the number of tuples
			 * and the number of pages in pg_class located at QEs through
			 * the dispatcher.
			 */
			StringInfoData buf;
			VPgClassStats stats;

			pq_beginmessage(&buf, 'y');
			pq_sendstring(&buf, "VACUUM");
			stats.relid = RelationGetRelid(relation);
			stats.rel_pages = num_pages;
			stats.rel_tuples = num_tuples;
			stats.relallvisible = num_all_visible_pages;
			pq_sendint(&buf, sizeof(VPgClassStats), sizeof(int));
			pq_sendbytes(&buf, (char *) &stats, sizeof(VPgClassStats));
			pq_endmessage(&buf);
		}
	}

	/*
	 * We need a way to distinguish these 2 cases:
	 * a) ANALYZEd/VACUUMed table is empty
	 * b) Table has never been ANALYZEd/VACUUMed
	 * To do this, in case (a), we set relPages = 1. For case (b), relPages = 0.
	 */
	if (num_pages < 1.0)
	{
		/*
		 * When running in utility mode in the QD node, we get the number of
		 * tuples of an AO table from the pg_aoseg table, but we don't know
		 * the file size, so that's always 0. Ignore the tuple count we got,
		 * and set reltuples to 0 instead, to avoid storing a confusing
		 * combination, and to avoid hitting the Assert below (which we
		 * inherited from upstream).
		 *
		 * It's perhaps not such a great idea to overwrite perfectly good
		 * relpages/reltuples estimates in utility mode, but that's what we
		 * do for heap tables, too, because we don't have even a tuple count
		 * for them. At least this is consistent.
		 */
		if (num_tuples >= 1.0)
		{
			Assert(Gp_role == GP_ROLE_UTILITY);
			Assert(!IsSystemRelation(relation));
			Assert(RelationIsAppendOptimized(relation));
			num_tuples = 0;
		}

		Assert(num_tuples < 1.0);
		num_pages = 1.0;
	}

	/*
	 * update number of tuples and number of pages in pg_class
	 */
	rd = heap_open(RelationRelationId, RowExclusiveLock);

	/* Fetch a copy of the tuple to scribble on */
	ctup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(ctup))
		elog(ERROR, "pg_class entry for relid %u vanished during vacuuming",
			 relid);
	pgcform = (Form_pg_class) GETSTRUCT(ctup);

	/* Apply statistical updates, if any, to copied tuple */

	/* GPDB-specific not allow change relpages and reltuples when vacuum in utility mode on QD
	 * Because there's a chance that we overwrite perfectly good stats with zeros
	 */

	bool ifUpdate = ! (IS_QUERY_DISPATCHER() && Gp_role == GP_ROLE_UTILITY);

	dirty = false;
	if (pgcform->relpages != (int32) num_pages && ifUpdate)
	{
		pgcform->relpages = (int32) num_pages;
		dirty = true;
	}
	if (pgcform->reltuples != (float4) num_tuples && ifUpdate)
	{
		pgcform->reltuples = (float4) num_tuples;
		dirty = true;
	}
	if (pgcform->relallvisible != (int32) num_all_visible_pages)
	{
		pgcform->relallvisible = (int32) num_all_visible_pages;
		dirty = true;
	}

	elog(DEBUG2, "Vacuum oid=%u pages=%d tuples=%f",
		 relid, pgcform->relpages, pgcform->reltuples);

	/* Apply DDL updates, but not inside an outer transaction (see above) */

	if (!in_outer_xact)
	{
		/*
		 * If we didn't find any indexes, reset relhasindex.
		 */
		if (pgcform->relhasindex && !hasindex)
		{
			pgcform->relhasindex = false;
			dirty = true;
		}

		/*
		 * If we have discovered that there are no indexes, then there's no
		 * primary key either.  This could be done more thoroughly...
		 */
		if (pgcform->relhaspkey && !hasindex)
		{
			pgcform->relhaspkey = false;
			dirty = true;
		}

		/* We also clear relhasrules and relhastriggers if needed */
		if (pgcform->relhasrules && relation->rd_rules == NULL)
		{
			pgcform->relhasrules = false;
			dirty = true;
		}
		if (pgcform->relhastriggers && relation->trigdesc == NULL)
		{
			pgcform->relhastriggers = false;
			dirty = true;
		}
	}

	/*
	 * Update relfrozenxid, unless caller passed InvalidTransactionId
	 * indicating it has no new data.
	 *
	 * Ordinarily, we don't let relfrozenxid go backwards: if things are
	 * working correctly, the only way the new frozenxid could be older would
	 * be if a previous VACUUM was done with a tighter freeze_min_age, in
	 * which case we don't want to forget the work it already did.  However,
	 * if the stored relfrozenxid is "in the future", then it must be corrupt
	 * and it seems best to overwrite it with the cutoff we used this time.
	 * This should match vac_update_datfrozenxid() concerning what we consider
	 * to be "in the future".
	 *
	 * GPDB: We check if pgcform->relfrozenxid is valid because AO and CO
	 * tables should have relfrozenxid as InvalidTransactionId.
	 */
	if (TransactionIdIsNormal(frozenxid) &&
		TransactionIdIsValid(pgcform->relfrozenxid) &&
		pgcform->relfrozenxid != frozenxid &&
		(TransactionIdPrecedes(pgcform->relfrozenxid, frozenxid) ||
		 TransactionIdPrecedes(ReadNewTransactionId(),
							   pgcform->relfrozenxid)))
	{
		pgcform->relfrozenxid = frozenxid;
		dirty = true;
	}

	/* Similarly for relminmxid */
	if (MultiXactIdIsValid(minmulti) &&
		pgcform->relminmxid != minmulti &&
		(MultiXactIdPrecedes(pgcform->relminmxid, minmulti) ||
		 MultiXactIdPrecedes(ReadNextMultiXactId(), pgcform->relminmxid)))
	{
		pgcform->relminmxid = minmulti;
		dirty = true;
	}

	/* If anything changed, write out the tuple. */
	if (dirty)
		heap_inplace_update(rd, ctup);

	heap_close(rd, RowExclusiveLock);
}


/*
 *	vac_update_datfrozenxid() -- update pg_database.datfrozenxid for our DB
 *
 *		Update pg_database's datfrozenxid entry for our database to be the
 *		minimum of the pg_class.relfrozenxid values.
 *
 *		Similarly, update our datminmxid to be the minimum of the
 *		pg_class.relminmxid values.
 *
 *		If we are able to advance either pg_database value, also try to
 *		truncate pg_clog and pg_multixact.
 *
 *		We violate transaction semantics here by overwriting the database's
 *		existing pg_database tuple with the new values.  This is reasonably
 *		safe since the new values are correct whether or not this transaction
 *		commits.  As with vac_update_relstats, this avoids leaving dead tuples
 *		behind after a VACUUM.
 */
void
vac_update_datfrozenxid(void)
{
	HeapTuple	tuple;
	Form_pg_database dbform;
	Relation	relation;
	SysScanDesc scan;
	HeapTuple	classTup;
	TransactionId newFrozenXid;
	MultiXactId newMinMulti;
	TransactionId lastSaneFrozenXid;
	MultiXactId lastSaneMinMulti;
	bool		bogus = false;
	bool		dirty = false;

	/*
	 * Initialize the "min" calculation with GetOldestXmin, which is a
	 * reasonable approximation to the minimum relfrozenxid for not-yet-
	 * committed pg_class entries for new tables; see AddNewRelationTuple().
	 * So we cannot produce a wrong minimum by starting with this.
	 *
	 * GPDB: Use GetLocalOldestXmin here, rather than GetOldestXmin. We don't
	 * want to include effects of distributed transactions in this. If a
	 * database's datfrozenxid is past the oldest XID as determined by
	 * distributed transactions, we will nevertheless never encounter such
	 * XIDs on disk.
	 */
	newFrozenXid = GetLocalOldestXmin(NULL, true);

	/*
	 * Similarly, initialize the MultiXact "min" with the value that would be
	 * used on pg_class for new tables.  See AddNewRelationTuple().
	 */
	newMinMulti = GetOldestMultiXactId();

	/*
	 * Identify the latest relfrozenxid and relminmxid values that we could
	 * validly see during the scan.  These are conservative values, but it's
	 * not really worth trying to be more exact.
	 */
	lastSaneFrozenXid = ReadNewTransactionId();
	lastSaneMinMulti = ReadNextMultiXactId();

	/*
	 * We must seqscan pg_class to find the minimum Xid, because there is no
	 * index that can help us here.
	 */
	relation = heap_open(RelationRelationId, AccessShareLock);

	scan = systable_beginscan(relation, InvalidOid, false,
							  NULL, 0, NULL);

	while ((classTup = systable_getnext(scan)) != NULL)
	{
		Form_pg_class classForm = (Form_pg_class) GETSTRUCT(classTup);

#if 0
		/*
		 * Only consider relations able to hold unfrozen XIDs (anything else
		 * should have InvalidTransactionId in relfrozenxid anyway.)
		 */
		if (classForm->relkind != RELKIND_RELATION &&
			classForm->relkind != RELKIND_MATVIEW &&
			classForm->relkind != RELKIND_TOASTVALUE)
			continue;
#endif

		/* GPDB_94_MERGE_FIXME: We have had this check here, instead of the above
		 * check that upstream has. I would be more comfortable if we would list
		 * the relkinds here explicitly, like in upstream..
		 */
		if (!TransactionIdIsValid(classForm->relfrozenxid))
			continue;

		Assert(TransactionIdIsNormal(classForm->relfrozenxid));
		Assert(MultiXactIdIsValid(classForm->relminmxid));
		/*
		 * Don't know partition parent or not here but passing false is perfect
		 * for assertion, as valid relfrozenxid means it shouldn't be parent.
		 */
		Assert(should_have_valid_relfrozenxid(classForm->relkind,
											  classForm->relstorage, false));

		/*
		 * If things are working properly, no relation should have a
		 * relfrozenxid or relminmxid that is "in the future".  However, such
		 * cases have been known to arise due to bugs in pg_upgrade.  If we
		 * see any entries that are "in the future", chicken out and don't do
		 * anything.  This ensures we won't truncate clog before those
		 * relations have been scanned and cleaned up.
		 */
		if (TransactionIdPrecedes(lastSaneFrozenXid, classForm->relfrozenxid) ||
			MultiXactIdPrecedes(lastSaneMinMulti, classForm->relminmxid))
		{
			bogus = true;
			break;
		}

		/*
		 * If things are working properly, no relation should have a
		 * relfrozenxid or relminmxid that is "in the future".  However, such
		 * cases have been known to arise due to bugs in pg_upgrade.  If we
		 * see any entries that are "in the future", chicken out and don't do
		 * anything.  This ensures we won't truncate clog before those
		 * relations have been scanned and cleaned up.
		 */
		if (TransactionIdPrecedes(lastSaneFrozenXid, classForm->relfrozenxid) ||
			MultiXactIdPrecedes(lastSaneMinMulti, classForm->relminmxid))
		{
			bogus = true;
			break;
		}

		if (TransactionIdPrecedes(classForm->relfrozenxid, newFrozenXid))
			newFrozenXid = classForm->relfrozenxid;

		if (MultiXactIdPrecedes(classForm->relminmxid, newMinMulti))
			newMinMulti = classForm->relminmxid;
	}

	/* we're done with pg_class */
	systable_endscan(scan);
	heap_close(relation, AccessShareLock);

	/* chicken out if bogus data found */
	if (bogus)
		return;

	Assert(TransactionIdIsNormal(newFrozenXid));
	Assert(MultiXactIdIsValid(newMinMulti));

	/* Now fetch the pg_database tuple we need to update. */
	relation = heap_open(DatabaseRelationId, RowExclusiveLock);

	/* Fetch a copy of the tuple to scribble on */
	tuple = SearchSysCacheCopy1(DATABASEOID, ObjectIdGetDatum(MyDatabaseId));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "could not find tuple for database %u", MyDatabaseId);
	dbform = (Form_pg_database) GETSTRUCT(tuple);

	/*
	 * As in vac_update_relstats(), we ordinarily don't want to let
	 * datfrozenxid go backward; but if it's "in the future" then it must be
	 * corrupt and it seems best to overwrite it.
	 */
	if (dbform->datfrozenxid != newFrozenXid &&
		(TransactionIdPrecedes(dbform->datfrozenxid, newFrozenXid) ||
		 TransactionIdPrecedes(lastSaneFrozenXid, dbform->datfrozenxid)))
	{
		dbform->datfrozenxid = newFrozenXid;
		dirty = true;
	}
	else
		newFrozenXid = dbform->datfrozenxid;

	/* Ditto for datminmxid */
	if (dbform->datminmxid != newMinMulti &&
		(MultiXactIdPrecedes(dbform->datminmxid, newMinMulti) ||
		 MultiXactIdPrecedes(lastSaneMinMulti, dbform->datminmxid)))
	{
		dbform->datminmxid = newMinMulti;
		dirty = true;
	}
	else
		newMinMulti = dbform->datminmxid;

	if (dirty)
	{
		heap_inplace_update(relation, tuple);
		SIMPLE_FAULT_INJECTOR("vacuum_update_dat_frozen_xid");
	}

	heap_freetuple(tuple);
	heap_close(relation, RowExclusiveLock);

	/*
	 * If we were able to advance datfrozenxid or datminmxid, see if we can
	 * truncate pg_clog and/or pg_multixact.  Also do it if the shared
	 * XID-wrap-limit info is stale, since this action will update that too.
	 */
	if (dirty || ForceTransactionIdLimitUpdate())
		vac_truncate_clog(newFrozenXid, newMinMulti,
						  lastSaneFrozenXid, lastSaneMinMulti);
}


/*
 *	vac_truncate_clog() -- attempt to truncate the commit log
 *
 *		Scan pg_database to determine the system-wide oldest datfrozenxid,
 *		and use it to truncate the transaction commit log (pg_clog).
 *		Also update the XID wrap limit info maintained by varsup.c.
 *		Likewise for datminmxid.
 *
 *		The passed frozenXID and minMulti are the updated values for my own
 *		pg_database entry. They're used to initialize the "min" calculations.
 *		The caller also passes the "last sane" XID and MXID, since it has
 *		those at hand already.
 *
 *		This routine is only invoked when we've managed to change our
 *		DB's datfrozenxid/datminmxid values, or we found that the shared
 *		XID-wrap-limit info is stale.
 */
static void
vac_truncate_clog(TransactionId frozenXID,
				  MultiXactId minMulti,
				  TransactionId lastSaneFrozenXid,
				  MultiXactId lastSaneMinMulti)
{
	TransactionId nextXID = ReadNewTransactionId();
	Relation	relation;
	HeapScanDesc scan;
	HeapTuple	tuple;
	Oid			oldestxid_datoid;
	Oid			minmulti_datoid;
	bool		bogus = false;
	bool		frozenAlreadyWrapped = false;

	/* init oldest datoids to sync with my frozenXID/minMulti values */
	oldestxid_datoid = MyDatabaseId;
	minmulti_datoid = MyDatabaseId;

	/*
	 * Scan pg_database to compute the minimum datfrozenxid/datminmxid
	 *
	 * Since vac_update_datfrozenxid updates datfrozenxid/datminmxid in-place,
	 * the values could change while we look at them.  Fetch each one just
	 * once to ensure sane behavior of the comparison logic.  (Here, as in
	 * many other places, we assume that fetching or updating an XID in shared
	 * storage is atomic.)
	 *
	 * Note: we need not worry about a race condition with new entries being
	 * inserted by CREATE DATABASE.  Any such entry will have a copy of some
	 * existing DB's datfrozenxid, and that source DB cannot be ours because
	 * of the interlock against copying a DB containing an active backend.
	 * Hence the new entry will not reduce the minimum.  Also, if two VACUUMs
	 * concurrently modify the datfrozenxid's of different databases, the
	 * worst possible outcome is that pg_clog is not truncated as aggressively
	 * as it could be.
	 */
	relation = heap_open(DatabaseRelationId, AccessShareLock);

	scan = heap_beginscan_catalog(relation, 0, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		volatile FormData_pg_database *dbform = (Form_pg_database) GETSTRUCT(tuple);
		TransactionId datfrozenxid = dbform->datfrozenxid;
		TransactionId datminmxid = dbform->datminmxid;

		Assert(TransactionIdIsNormal(datfrozenxid));
		Assert(MultiXactIdIsValid(datminmxid));

		/*
		 * If things are working properly, no database should have a
		 * datfrozenxid or datminmxid that is "in the future".  However, such
		 * cases have been known to arise due to bugs in pg_upgrade.  If we
		 * see any entries that are "in the future", chicken out and don't do
		 * anything.  This ensures we won't truncate clog before those
		 * databases have been scanned and cleaned up.  (We will issue the
		 * "already wrapped" warning if appropriate, though.)
		 */
		if (TransactionIdPrecedes(lastSaneFrozenXid, datfrozenxid) ||
			MultiXactIdPrecedes(lastSaneMinMulti, datminmxid))
			bogus = true;

		if (TransactionIdPrecedes(nextXID, datfrozenxid))
			frozenAlreadyWrapped = true;
		else if (TransactionIdPrecedes(datfrozenxid, frozenXID))
		{
			frozenXID = datfrozenxid;
			oldestxid_datoid = HeapTupleGetOid(tuple);
		}

		if (MultiXactIdPrecedes(datminmxid, minMulti))
		{
			minMulti = datminmxid;
			minmulti_datoid = HeapTupleGetOid(tuple);
		}
	}

	heap_endscan(scan);

	heap_close(relation, AccessShareLock);

	/*
	 * Do not truncate CLOG if we seem to have suffered wraparound already;
	 * the computed minimum XID might be bogus.  This case should now be
	 * impossible due to the defenses in GetNewTransactionId, but we keep the
	 * test anyway.
	 */
	if (frozenAlreadyWrapped)
	{
		ereport(WARNING,
				(errmsg("some databases have not been vacuumed in over 2 billion transactions"),
				 errdetail("You might have already suffered transaction-wraparound data loss.")));
		return;
	}

	/* chicken out if data is bogus in any other way */
	if (bogus)
		return;

	/*
	 * Truncate CLOG, multixact and CommitTs to the oldest computed value.
	 */
	TruncateCLOG(frozenXID);
	TruncateCommitTs(frozenXID);
	TruncateMultiXact(minMulti, minmulti_datoid);

	/*
	 * Update the wrap limit for GetNewTransactionId and creation of new
	 * MultiXactIds.  Note: these functions will also signal the postmaster
	 * for an(other) autovac cycle if needed.   XXX should we avoid possibly
	 * signalling twice?
	 */
	SetTransactionIdLimit(frozenXID, oldestxid_datoid);
	SetMultiXactIdLimit(minMulti, minmulti_datoid);
	AdvanceOldestCommitTsXid(frozenXID);
}


/*
 *	vacuum_rel() -- vacuum one heap relation
 *
 *		Doing one heap at a time incurs extra overhead, since we need to
 *		check that the heap exists again just before we vacuum it.  The
 *		reason that we do this is so that vacuuming can be spread across
 *		many small transactions.  Otherwise, two-phase locking would require
 *		us to lock the entire database during one pass of the vacuum cleaner.
 *
 *		At entry and exit, we are not inside a transaction.
 */
static bool
vacuum_rel(Oid relid, RangeVar *relation, int options, VacuumParams *params,
		   bool recursing)
{
	LOCKMODE	lmode;
	Relation	onerel;
	LockRelId	onerelid;
	Oid			toast_relid;
	Oid			aoseg_relid = InvalidOid;
	Oid         aoblkdir_relid = InvalidOid;
	Oid         aovisimap_relid = InvalidOid;
	RangeVar	*toast_rangevar = NULL;
	RangeVar	*aoseg_rangevar = NULL;
	RangeVar	*aoblkdir_rangevar = NULL;
	RangeVar	*aovisimap_rangevar = NULL;
	RangeVar	*this_rangevar = NULL;
	int			ao_vacuum_phase = (options & VACUUM_AO_PHASE_MASK);
	bool		is_appendoptimized;
	Oid			save_userid;
	int			save_sec_context;
	int			save_nestlevel;
	MemoryContext oldcontext;

	Assert(params != NULL);

	/*
	 * For each iteration we start/commit our own transactions,
	 * so that we can release resources such as locks and memories,
	 * and we can also safely perform non-transactional work
	 * along with transactional work.
	 */
	StartTransactionCommand();

	/*
	 * Functions in indexes may want a snapshot set. Also, setting
	 * a snapshot ensures that RecentGlobalXmin is kept truly recent.
	 */
	PushActiveSnapshot(GetTransactionSnapshot());

	if (!(options & VACOPT_FULL))
	{
		/*
		 * PostgreSQL does this:
		 * In lazy vacuum, we can set the PROC_IN_VACUUM flag, which lets
		 * other concurrent VACUUMs know that they can ignore this one while
		 * determining their OldestXmin.  (The reason we don't set it during a
		 * full VACUUM is exactly that we may have to run user-defined
		 * functions for functional indexes, and we want to make sure that if
		 * they use the snapshot set above, any tuples it requires can't get
		 * removed from other tables.  An index function that depends on the
		 * contents of other tables is arguably broken, but we won't break it
		 * here by violating transaction semantics.)
		 *
		 * GPDB doesn't use PROC_IN_VACUUM, as lazy vacuum for bitmap
		 * indexed tables performs reindex causing updates to pg_class
		 * tuples for index entries.
		 *
		 * We also set the VACUUM_FOR_WRAPAROUND flag, which is passed down by
		 * autovacuum; it's used to avoid canceling a vacuum that was invoked
		 * in an emergency.
		 *
		 * Note: this flag remains set until CommitTransaction or
		 * AbortTransaction.  We don't want to clear it until we reset
		 * MyPgXact->xid/xmin, else OldestXmin might appear to go backwards,
		 * which is probably Not Good.
		 */
		LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
#if 0 /* Upstream code not applicable to GPDB */
		MyPgXact->vacuumFlags |= PROC_IN_VACUUM;
#endif
		if (params->is_wraparound)
			MyPgXact->vacuumFlags |= PROC_VACUUM_FOR_WRAPAROUND;
		LWLockRelease(ProcArrayLock);
	}

	/*
	 * Check for user-requested abort.  Note we want this to be inside a
	 * transaction, so xact.c doesn't issue useless WARNING.
	 */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Determine the type of lock we want --- hard exclusive lock for a FULL
	 * vacuum, but just ShareUpdateExclusiveLock for concurrent vacuum. Either
	 * way, we can be sure that no other backend is vacuuming the same table.
	 */
	// FIXME: This fault point was roughly here before. It's kept here to keep
	// the regression tests from hanging, but need to check that the tests
	// still make sense. And "drop phase" isn't a term we use anymore.
	if (ao_vacuum_phase == VACOPT_AO_POST_CLEANUP_PHASE)
	{
		SIMPLE_FAULT_INJECTOR("vacuum_relation_open_relation_during_drop_phase");
	}

	lmode = (options & VACOPT_FULL) ? AccessExclusiveLock : ShareUpdateExclusiveLock;
	// FIXME: what's the right level for AO tables?

	/*
	 * Open the relation and get the appropriate lock on it.
	 *
	 * There's a race condition here: the rel may have gone away since the
	 * last time we saw it.  If so, we don't need to vacuum it.
	 *
	 * If we've been asked not to wait for the relation lock, acquire it first
	 * in non-blocking mode, before calling try_relation_open().
	 */
	if (!(options & VACOPT_NOWAIT))
		onerel = try_relation_open(relid, lmode, false /* nowait */);
	else if (ConditionalLockRelationOid(relid, lmode))
		onerel = try_relation_open(relid, NoLock, false /* nowait */);
	else
	{
		onerel = NULL;
		if (IsAutoVacuumWorkerProcess() && params->log_min_duration >= 0)
			ereport(LOG,
					(errcode(ERRCODE_LOCK_NOT_AVAILABLE),
				   errmsg("skipping vacuum of \"%s\" --- lock not available",
						  relation->relname)));
	}

	if (!onerel)
	{
		PopActiveSnapshot();
		CommitTransactionCommand();
		return false;
	}

	/*
	 * Check permissions.
	 *
	 * We allow the user to vacuum a table if he is superuser, the table
	 * owner, or the database owner (but in the latter case, only if it's not
	 * a shared relation).	pg_class_ownercheck includes the superuser case.
	 *
	 * Note we choose to treat permissions failure as a WARNING and keep
	 * trying to vacuum the rest of the DB --- is this appropriate?
	 */
	if (!(pg_class_ownercheck(RelationGetRelid(onerel), GetUserId()) ||
		  (pg_database_ownercheck(MyDatabaseId, GetUserId()) && !onerel->rd_rel->relisshared)))
	{
		if (Gp_role != GP_ROLE_EXECUTE)
		{
			if (onerel->rd_rel->relisshared)
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- only superuser can vacuum it",
								RelationGetRelationName(onerel))));
			else if (onerel->rd_rel->relnamespace == PG_CATALOG_NAMESPACE)
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- only superuser or database owner can vacuum it",
								RelationGetRelationName(onerel))));
			else
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- only table or database owner can vacuum it",
								RelationGetRelationName(onerel))));
		}
		relation_close(onerel, lmode);
		PopActiveSnapshot();
		CommitTransactionCommand();
		return false;
	}

	/*
	 * Check that it's a vacuumable relation; we used to do this in
	 * get_rel_oids() but seems safer to check after we've locked the
	 * relation.
	 */
	if ((onerel->rd_rel->relkind != RELKIND_RELATION &&
		 onerel->rd_rel->relkind != RELKIND_MATVIEW &&
		 onerel->rd_rel->relkind != RELKIND_TOASTVALUE &&
		 onerel->rd_rel->relkind != RELKIND_AOSEGMENTS &&
		 onerel->rd_rel->relkind != RELKIND_AOBLOCKDIR &&
		 onerel->rd_rel->relkind != RELKIND_AOVISIMAP)
		|| RelationIsForeign(onerel))
	{
		ereport(WARNING,
				(errmsg("skipping \"%s\" --- cannot vacuum non-tables, external tables, foreign tables or special system tables",
						RelationGetRelationName(onerel))));
		relation_close(onerel, lmode);
		PopActiveSnapshot();
		CommitTransactionCommand();
		return false;
	}

#ifdef FAULT_INJECTOR
	if (ao_vacuum_phase == VACOPT_AO_POST_CLEANUP_PHASE)
	{
		FaultInjector_InjectFaultIfSet(
			"compaction_before_cleanup_phase",
			DDLNotSpecified,
			"",	// databaseName
			RelationGetRelationName(onerel)); // tableName
	}
#endif

	/*
	 * Silently ignore tables that are temp tables of other backends ---
	 * trying to vacuum these will lead to great unhappiness, since their
	 * contents are probably not up-to-date on disk.  (We don't throw a
	 * warning here; it would just lead to chatter during a database-wide
	 * VACUUM.)
	 */
	if (RELATION_IS_OTHER_TEMP(onerel))
	{
		relation_close(onerel, lmode);
		PopActiveSnapshot();
		CommitTransactionCommand();
		return false;
	}

	is_appendoptimized = RelationIsAppendOptimized(onerel);

	if (ao_vacuum_phase && !is_appendoptimized)
	{
		/* We were asked to some phase of AO vacuum, but it's not an AO table. Huh? */
		elog(ERROR, "AO vacuum phase was invoked on a non-AO table");
	}

	/*
	 * Get a session-level lock too. This will protect our access to the
	 * relation across multiple transactions, so that we can vacuum the
	 * relation's TOAST table (if any) secure in the knowledge that no one is
	 * deleting the parent relation.
	 *
	 * NOTE: this cannot block, even if someone else is waiting for access,
	 * because the lock manager knows that both lock requests are from the
	 * same process.
	 */
	onerelid = onerel->rd_lockInfo.lockRelId;
	LockRelationIdForSession(&onerelid, lmode);

	/*
	 * Remember the relation's TOAST relation for later, if the caller asked
	 * us to process it.  In VACUUM FULL, though, the toast table is
	 * automatically rebuilt by cluster_rel so we shouldn't recurse to it.
	 *
	 * GPDB: Also remember the AO segment relations for later.
	 */
	if (!(options & VACOPT_SKIPTOAST) && !(options & VACOPT_FULL))
		toast_relid = onerel->rd_rel->reltoastrelid;
	else
		toast_relid = InvalidOid;
	oldcontext = MemoryContextSwitchTo(vac_context);
	toast_rangevar = makeRangeVar(get_namespace_name(get_rel_namespace(toast_relid)),
								  get_rel_name(toast_relid),
								  -1);
	MemoryContextSwitchTo(oldcontext);

	is_appendoptimized = RelationIsAppendOptimized(onerel);
	if (is_appendoptimized)
	{
		GetAppendOnlyEntryAuxOids(RelationGetRelid(onerel), NULL,
								  &aoseg_relid,
								  &aoblkdir_relid, NULL,
								  &aovisimap_relid, NULL);
		oldcontext = MemoryContextSwitchTo(vac_context);
		aoseg_rangevar = makeRangeVar(get_namespace_name(get_rel_namespace(aoseg_relid)),
									  get_rel_name(aoseg_relid),
									  -1);
		aoblkdir_rangevar = makeRangeVar(get_namespace_name(get_rel_namespace(aoblkdir_relid)),
										 get_rel_name(aoblkdir_relid),
										 -1);
		aovisimap_rangevar = makeRangeVar(get_namespace_name(get_rel_namespace(aovisimap_relid)),
										  get_rel_name(aovisimap_relid),
										  -1);
		MemoryContextSwitchTo(oldcontext);
	}

	/*
	 * If it's a partitioned relation, on entry 'relation' refers to the table
	 * that the original command was issued on, and 'relid' is the actual partition
	 * we're processing. Build a rangevar representing this partition, so that we
	 * can dispatch it.
	 */
	oldcontext = MemoryContextSwitchTo(vac_context);
	this_rangevar = makeRangeVar(get_namespace_name(onerel->rd_rel->relnamespace),
								 pstrdup(RelationGetRelationName(onerel)),
								 -1);
	MemoryContextSwitchTo(oldcontext);

	/*
	 * Switch to the table owner's userid, so that any index functions are run
	 * as that user.  Also lock down security-restricted operations and
	 * arrange to make GUC variable changes local to this command.
	 */
	GetUserIdAndSecContext(&save_userid, &save_sec_context);
	SetUserIdAndSecContext(onerel->rd_rel->relowner,
						   save_sec_context | SECURITY_RESTRICTED_OPERATION);
	save_nestlevel = NewGUCNestLevel();

	/*
	 * If there are any bitmap indexes, we have to acquire a ShareLock for the
	 * table, since reindex is used later. Otherwise, concurrent vacuum and
	 * inserts may cause deadlock. MPP-5960
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		int 		i, nindexes;
		bool 		has_bitmap = false;
		Relation   *i_rel = NULL;

		vac_open_indexes(onerel, AccessShareLock, &nindexes, &i_rel);
		if (i_rel != NULL)
		{
			for (i = 0; i < nindexes; i++)
			{
				if (RelationIsBitmapIndex(i_rel[i]))
				{
					has_bitmap = true;
					break;
				}
			}
		}
		vac_close_indexes(nindexes, i_rel, AccessShareLock);

		if (has_bitmap)
			LockRelation(onerel, ShareLock);
	}

	/*
	 * Do the actual work --- either FULL or "lazy" vacuum
	 *
	 * Append-only relations don't support, nor need, a FULL vacuum, so perform
	 * a lazy vacuum instead, even if FULL was requested. Note that we have
	 * already locked the table, and if FULL was requested, we got an
	 * AccessExclusiveLock. Therefore, FULL isn't exactly the same as non-FULL
	 * on AO tables.
	 */
	if (ao_vacuum_phase == VACOPT_AO_PRE_CLEANUP_PHASE)
	{
		ao_vacuum_rel_pre_cleanup(onerel, options, params, vac_strategy);
	}
	else if (ao_vacuum_phase == VACOPT_AO_COMPACT_PHASE)
	{
		ao_vacuum_rel_compact(onerel, options, params, vac_strategy);
	}
	else if (ao_vacuum_phase == VACOPT_AO_POST_CLEANUP_PHASE)
	{
		ao_vacuum_rel_post_cleanup(onerel, options, params, vac_strategy);
	}
	else if (is_appendoptimized)
	{
		/* Do nothing here, we will launch the stages later */
		Assert(ao_vacuum_phase == 0);
	}
	else if ((options & VACOPT_FULL))
	{
		/* close relation before vacuuming, but hold lock until commit */
		relation_close(onerel, NoLock);
		onerel = NULL;

		/* VACUUM FULL is now a variant of CLUSTER; see cluster.c */
		cluster_rel(relid, InvalidOid, false,
					(options & VACOPT_VERBOSE) != 0,
					true /* printError */);
	}
	else
	{
		lazy_vacuum_rel_heap(onerel, options, params, vac_strategy);
	}

	/* Roll back any GUC changes executed by index functions */
	AtEOXact_GUC(false, save_nestlevel);

	/* Restore userid and security context */
	SetUserIdAndSecContext(save_userid, save_sec_context);

	/* all done with this class, but hold lock until commit */
	if (onerel)
		relation_close(onerel, NoLock);

	/*
	 * Complete the transaction and free all temporary memory used.
	 */
	PopActiveSnapshot();
	CommitTransactionCommand();

	if (is_appendoptimized && ao_vacuum_phase == 0)
	{
		/* orchestrate the AO vacuum phases */
		/*
		 * Do cleanup first, to reclaim as much space as possible that
		 * was left behind from previous VACUUMs. This runs under local
		 * transactions.
		 */
		vacuum_rel(relid, this_rangevar,options | VACOPT_AO_PRE_CLEANUP_PHASE,
				   params, false);

		/* Compact. This runs in a distributed transaction.  */
		vacuum_rel(relid, this_rangevar, options | VACOPT_AO_COMPACT_PHASE,
				   params, false);

		/* Do a final round of cleanup. Hopefully, this can drop the segments
		 * that were compacted in the previous phase.
		 */
		vacuum_rel(relid, this_rangevar, options | VACOPT_AO_POST_CLEANUP_PHASE,
				   params, false);
	}

	/*
	 * In an append-only table, the auxiliary tables are cleaned up in
	 * the POST_CLEANUP phase. Ignore them in other phases.
	 */
	if (is_appendoptimized && ao_vacuum_phase != VACOPT_AO_POST_CLEANUP_PHASE)
	{
		toast_relid = InvalidOid;
		aoseg_relid = InvalidOid;
		aoblkdir_relid = InvalidOid;
		aovisimap_relid = InvalidOid;
	}

	/*
	 * If the relation has a secondary toast rel, vacuum that too while we
	 * still hold the session lock on the master table.  We do this in
	 * post-cleanup phase when it's AO table.
	 */
	if (toast_relid != InvalidOid && toast_rangevar != NULL)
		vacuum_rel(toast_relid, toast_rangevar, options & ~VACUUM_AO_PHASE_MASK,
				   params, true);

	/*
	 * If an AO/CO table is empty on a segment,
	 *
	 * Similar to toast, a VacuumStmt object for each AO auxiliary relation is
	 * constructed and dispatched separately by the QD, when vacuuming the
	 * base AO relation.  A backend executing dispatched VacuumStmt
	 * (GP_ROLE_EXECUTE), therefore, should not execute this block of code.
	 */

	/* do the same for an AO segments table, if any */
	if (aoseg_relid != InvalidOid && aoseg_rangevar != NULL)
		vacuum_rel(aoseg_relid, aoseg_rangevar, options & ~VACUUM_AO_PHASE_MASK,
				   params, true);

	/* do the same for an AO block directory table, if any */
	if (aoblkdir_relid != InvalidOid && aoblkdir_rangevar != NULL)
		vacuum_rel(aoblkdir_relid, aoblkdir_rangevar, options & ~VACUUM_AO_PHASE_MASK,
				   params, true);

	/* do the same for an AO visimap, if any */
	if (aovisimap_relid != InvalidOid && aovisimap_rangevar != NULL)
		vacuum_rel(aovisimap_relid, aovisimap_rangevar, options & ~VACUUM_AO_PHASE_MASK,
				   params, true);

	if (Gp_role == GP_ROLE_DISPATCH && !recursing &&
		(!is_appendoptimized || ao_vacuum_phase))
	{
		VacuumStatsContext stats_context;
		char	   *vsubtype;

		/*
		 * Dispatching needs a transaction. At least in some error scenarios,
		 * it uses TopTransactionContext to store stuff.
		 */
		StartTransactionCommand();
		PushActiveSnapshot(GetTransactionSnapshot());

		stats_context.updated_stats = NIL;
		dispatchVacuum(options, this_rangevar, &stats_context);
		vac_update_relstats_from_list(stats_context.updated_stats);

		/* Also update pg_stat_last_operation */
		if (IsAutoVacuumWorkerProcess())
			vsubtype = "AUTO";
		else
		{
			if ((options & VACOPT_FULL) &&
				(0 == params->freeze_min_age))
				vsubtype = "FULL FREEZE";
			else if ((options & VACOPT_FULL))
				vsubtype = "FULL";
			else if (0 == params->freeze_min_age)
				vsubtype = "FREEZE";
			else
				vsubtype = "";
		}
		MetaTrackUpdObject(RelationRelationId,
						   relid,
						   GetUserId(),
						   "VACUUM",
						   vsubtype);

		/* Restore userid and security context */
		SetUserIdAndSecContext(save_userid, save_sec_context);

		PopActiveSnapshot();
		CommitTransactionCommand();
	}

	/*
	 * Now release the session-level lock on the master table.
	 */
	UnlockRelationIdForSession(&onerelid, lmode);

	/* Report that we really did it. */
	return true;
}


/****************************************************************************
 *																			*
 *			Code for VACUUM FULL (only)										*
 *																			*
 ****************************************************************************
 */

/*
 *	scan_index() -- scan one index relation to update pg_class statistics.
 *
 * We use this when we have no deletions to do.
 */
void
scan_index(Relation indrel, double num_tuples, int elevel)
{
	IndexBulkDeleteResult *stats;
	IndexVacuumInfo ivinfo;
	PGRUsage	ru0;
	BlockNumber relallvisible;

	pg_rusage_init(&ru0);

	ivinfo.index = indrel;
	ivinfo.analyze_only = false;
	ivinfo.estimated_count = false;
	ivinfo.message_level = elevel;
	ivinfo.num_heap_tuples = num_tuples;
	ivinfo.strategy = vac_strategy;

	stats = index_vacuum_cleanup(&ivinfo, NULL);

	if (!stats)
		return;

	if (RelationIsAppendOptimized(indrel))
		relallvisible = 0;
	else
		visibilitymap_count(indrel, &relallvisible, NULL);

	/*
	 * Now update statistics in pg_class, but only if the index says the count
	 * is accurate.
	 */
	if (!stats->estimated_count)
		vac_update_relstats(indrel,
							stats->num_pages, stats->num_index_tuples,
							relallvisible,
							false,
							InvalidTransactionId,
							InvalidMultiXactId,
							false,
							true /* isvacuum */);

	ereport(elevel,
			(errmsg("index \"%s\" now contains %.0f row versions in %u pages",
					RelationGetRelationName(indrel),
					stats->num_index_tuples,
					stats->num_pages),
	errdetail("%u index pages have been deleted, %u are currently reusable.\n"
			  "%s.",
			  stats->pages_deleted, stats->pages_free,
			  pg_rusage_show(&ru0))));

	pfree(stats);
}

/*
 * Open all the vacuumable indexes of the given relation, obtaining the
 * specified kind of lock on each.  Return an array of Relation pointers for
 * the indexes into *Irel, and the number of indexes into *nindexes.
 *
 * We consider an index vacuumable if it is marked insertable (IndexIsReady).
 * If it isn't, probably a CREATE INDEX CONCURRENTLY command failed early in
 * execution, and what we have is too corrupt to be processable.  We will
 * vacuum even if the index isn't indisvalid; this is important because in a
 * unique index, uniqueness checks will be performed anyway and had better not
 * hit dangling index pointers.
 */
void
vac_open_indexes(Relation relation, LOCKMODE lockmode,
				 int *nindexes, Relation **Irel)
{
	List	   *indexoidlist;
	ListCell   *indexoidscan;
	int			i;

	Assert(lockmode != NoLock);

	indexoidlist = RelationGetIndexList(relation);

	/* allocate enough memory for all indexes */
	i = list_length(indexoidlist);

	if (i > 0)
		*Irel = (Relation *) palloc(i * sizeof(Relation));
	else
		*Irel = NULL;

	/* collect just the ready indexes */
	i = 0;
	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(indexoidscan);
		Relation	indrel;

		indrel = index_open(indexoid, lockmode);
		if (IndexIsReady(indrel->rd_index))
			(*Irel)[i++] = indrel;
		else
			index_close(indrel, lockmode);
	}

	*nindexes = i;

	list_free(indexoidlist);
}

/*
 * Release the resources acquired by vac_open_indexes.  Optionally release
 * the locks (say NoLock to keep 'em).
 */
void
vac_close_indexes(int nindexes, Relation *Irel, LOCKMODE lockmode)
{
	if (Irel == NULL)
		return;

	while (nindexes--)
	{
		Relation	ind = Irel[nindexes];

		index_close(ind, lockmode);
	}
	pfree(Irel);
}

/*
 * vacuum_delay_point --- check for interrupts and cost-based delay.
 *
 * This should be called in each major loop of VACUUM processing,
 * typically once per page processed.
 */
void
vacuum_delay_point(void)
{
	/* Always check for interrupts */
	CHECK_FOR_INTERRUPTS();

	/* Nap if appropriate */
	if (VacuumCostActive && !InterruptPending &&
		VacuumCostBalance >= VacuumCostLimit)
	{
		int			msec;

		msec = VacuumCostDelay * VacuumCostBalance / VacuumCostLimit;
		if (msec > VacuumCostDelay * 4)
			msec = VacuumCostDelay * 4;

		pg_usleep(msec * 1000L);

		VacuumCostBalance = 0;

		/* update balance values for workers */
		AutoVacuumUpdateDelay();

		/* Might have gotten an interrupt while sleeping */
		CHECK_FOR_INTERRUPTS();
	}
}

/*
 * Dispatch a Vacuum command.
 */
static void
dispatchVacuum(int options, RangeVar *relation,
			   VacuumStatsContext *ctx)
{
	CdbPgResults cdb_pgresults;
	VacuumStmt *vacstmt = makeNode(VacuumStmt);
	int flags = DF_CANCEL_ON_ERROR | DF_WITH_SNAPSHOT;

	if ((options & VACUUM_AO_PHASE_MASK) == VACOPT_AO_COMPACT_PHASE)
		flags |= DF_NEED_TWO_PHASE;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(options & VACOPT_VACUUM);
	//Assert(!(options & VACOPT_ANALYZE));

	/* VACUUM, without ANALYZE */
	options &= ~VACOPT_ANALYZE;

	vacstmt->options = options;
	vacstmt->relation = relation;
	vacstmt->va_cols = NIL;

	/* XXX: Some kinds of VACUUM assign a new relfilenode. bitmap indexes maybe? */
	CdbDispatchUtilityStatement((Node *) vacstmt, flags,
								GetAssignedOidsForDispatch(),
								&cdb_pgresults);

	vacuum_combine_stats(ctx, &cdb_pgresults);

	cdbdisp_clearCdbPgResults(&cdb_pgresults);
}

/*
 * vacuum_combine_stats
 * This function combine the stats information sent by QEs to generate
 * the final stats for QD relations.
 *
 * Note that the mirrorResults is ignored by this function.
 */
static void
vacuum_combine_stats(VacuumStatsContext *stats_context, CdbPgResults* cdb_pgresults)
{
	int			result_no;
	MemoryContext old_context;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	if (cdb_pgresults == NULL || cdb_pgresults->numResults <= 0)
		return;

	/*
	 * Process the dispatch results from the primary. Note that the QE
	 * processes also send back the new stats info, such as stats on
	 * pg_class, for the relevant table and its
	 * indexes. We parse this information, and compute the final stats
	 * for the QD.
	 *
	 * For pg_class stats, we compute the maximum number of tuples and
	 * maximum number of pages after processing the stats from each QE.
	 *
	 */
	for(result_no = 0; result_no < cdb_pgresults->numResults; result_no++)
	{
		VPgClassStats *pgclass_stats = NULL;
		ListCell *lc = NULL;
		struct pg_result *pgresult = cdb_pgresults->pg_results[result_no];

		if (pgresult->extras == NULL)
			continue;

		Assert(pgresult->extraslen > sizeof(int));

		/*
		 * Process the stats for pg_class. We simply compute the maximum
		 * number of rel_tuples and rel_pages.
		 */
		pgclass_stats = (VPgClassStats *) pgresult->extras;
		foreach (lc, stats_context->updated_stats)
		{
			VPgClassStats *tmp_stats = (VPgClassStats *) lfirst(lc);

			if (tmp_stats->relid == pgclass_stats->relid)
			{
				tmp_stats->rel_pages += pgclass_stats->rel_pages;
				tmp_stats->rel_tuples += pgclass_stats->rel_tuples;
				tmp_stats->relallvisible += pgclass_stats->relallvisible;
				break;
			}
		}

		if (lc == NULL)
		{
			Assert(pgresult->extraslen == sizeof(VPgClassStats));

			old_context = MemoryContextSwitchTo(vac_context);
			pgclass_stats = palloc(sizeof(VPgClassStats));
			memcpy(pgclass_stats, pgresult->extras, pgresult->extraslen);

			stats_context->updated_stats =
				lappend(stats_context->updated_stats, pgclass_stats);
			MemoryContextSwitchTo(old_context);
		}
	}
}
