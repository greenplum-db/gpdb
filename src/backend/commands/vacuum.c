/*-------------------------------------------------------------------------
 *
 * vacuum.c
 *	  The postgres vacuum cleaner.
 *
 * This file includes the "full" version of VACUUM, as well as control code
 * used by all three of full VACUUM, lazy VACUUM, and ANALYZE.	See
 * vacuumlazy.c and analyze.c for the rest of the code for the latter two.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/vacuum.c,v 1.375 2008/06/05 15:47:32 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/time.h>
#include <unistd.h>

#include "access/clog.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/appendonlywriter.h"
#include "access/appendonlytid.h"
#include "catalog/heap.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/appendonly_compaction.h"
#include "access/appendonly_visimap.h"
#include "access/aocs_compaction.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_database.h"
#include "catalog/pg_index.h"
#include "catalog/indexing.h"
#include "catalog/pg_namespace.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbdispatchresult.h"      /* CdbDispatchResults */
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "executor/executor.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"             /* pq_beginmessage() etc. */
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/flatfiles.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "access/distributedlog.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "nodes/makefuncs.h"     /* makeRangeVar */
#include "pgstat.h"


/*
 * GUC parameters
 */
int			vacuum_freeze_min_age;

/*
 * VacPage structures keep track of each page on which we find useful
 * amounts of free space.
 */
typedef struct VacPageData
{
	BlockNumber blkno;			/* BlockNumber of this Page */
	Size		free;			/* FreeSpace on this Page */
	uint16		offsets_used;	/* Number of OffNums used by vacuum */
	uint16		offsets_free;	/* Number of OffNums free or to be free */
	OffsetNumber offsets[1];	/* Array of free OffNums */
} VacPageData;

typedef VacPageData *VacPage;

typedef struct VacPageListData
{
	BlockNumber empty_end_pages;	/* Number of "empty" end-pages */
	int			num_pages;		/* Number of pages in pagedesc */
	int			num_allocated_pages;	/* Number of allocated pages in
										 * pagedesc */
	VacPage    *pagedesc;		/* Descriptions of pages */
} VacPageListData;

typedef VacPageListData *VacPageList;

/*
 * The "vtlinks" array keeps information about each recently-updated tuple
 * ("recent" meaning its XMAX is too new to let us recycle the tuple).
 * We store the tuple's own TID as well as its t_ctid (its link to the next
 * newer tuple version).  Searching in this array allows us to follow update
 * chains backwards from newer to older tuples.  When we move a member of an
 * update chain, we must move *all* the live members of the chain, so that we
 * can maintain their t_ctid link relationships (we must not just overwrite
 * t_ctid in an existing tuple).
 *
 * Note: because t_ctid links can be stale (this would only occur if a prior
 * VACUUM crashed partway through), it is possible that new_tid points to an
 * empty slot or unrelated tuple.  We have to check the linkage as we follow
 * it, just as is done in EvalPlanQual.
 */
typedef struct VTupleLinkData
{
	ItemPointerData new_tid;	/* t_ctid of an updated tuple */
	ItemPointerData this_tid;	/* t_self of the tuple */
} VTupleLinkData;

typedef VTupleLinkData *VTupleLink;

/*
 * We use an array of VTupleMoveData to plan a chain tuple move fully
 * before we do it.
 */
typedef struct VTupleMoveData
{
	ItemPointerData tid;		/* tuple ID */
	VacPage		vacpage;		/* where to move it to */
	bool		cleanVpd;		/* clean vacpage before using? */
} VTupleMoveData;

typedef VTupleMoveData *VTupleMove;

/*
 * VRelStats contains the data acquired by scan_heap for use later
 */
typedef struct VRelStats
{
	/* miscellaneous statistics */
	BlockNumber rel_pages;		/* pages in relation */
	double		rel_tuples;		/* tuples that remain after vacuuming */
	double		rel_indexed_tuples;		/* indexed tuples that remain */
	Size		min_tlen;		/* min surviving tuple size */
	Size		max_tlen;		/* max surviving tuple size */
	bool		hasindex;
	/* vtlinks array for tuple chain following - sorted by new_tid */
	int			num_vtlinks;
	VTupleLink	vtlinks;
} VRelStats;

/*----------------------------------------------------------------------
 * ExecContext:
 *
 * As these variables always appear together, we put them into one struct
 * and pull initialization and cleanup into separate routines.
 * ExecContext is used by repair_frag() and move_xxx_tuple().  More
 * accurately:	It is *used* only in move_xxx_tuple(), but because this
 * routine is called many times, we initialize the struct just once in
 * repair_frag() and pass it on to move_xxx_tuple().
 */
typedef struct ExecContextData
{
	ResultRelInfo *resultRelInfo;
	EState	   *estate;
	TupleTableSlot *slot;
} ExecContextData;

typedef ExecContextData *ExecContext;

typedef struct VacuumStatsContext
{
	List	   *updated_stats;
} VacuumStatsContext;

/*
 * State information used during the (full)
 * vacuum of indexes on append-only tables
 */
typedef struct AppendOnlyIndexVacuumState
{
	AppendOnlyVisimap visiMap;
	AppendOnlyBlockDirectory blockDirectory;
	AppendOnlyBlockDirectoryEntry blockDirectoryEntry;
} AppendOnlyIndexVacuumState;

static void
ExecContext_Init(ExecContext ec, Relation rel)
{
	TupleDesc	tupdesc = RelationGetDescr(rel);

	/*
	 * We need a ResultRelInfo and an EState so we can use the regular
	 * executor's index-entry-making machinery.
	 */
	ec->estate = CreateExecutorState();

	ec->resultRelInfo = makeNode(ResultRelInfo);
	ec->resultRelInfo->ri_RangeTableIndex = 1;	/* dummy */
	ec->resultRelInfo->ri_RelationDesc = rel;
	ec->resultRelInfo->ri_TrigDesc = NULL;		/* we don't fire triggers */

	ExecOpenIndices(ec->resultRelInfo);

	ec->estate->es_result_relations = ec->resultRelInfo;
	ec->estate->es_num_result_relations = 1;
	ec->estate->es_result_relation_info = ec->resultRelInfo;

	/* Set up a tuple slot too */
	ec->slot = MakeSingleTupleTableSlot(tupdesc);
}

static void
ExecContext_Finish(ExecContext ec)
{
	ExecDropSingleTupleTableSlot(ec->slot);
	ExecCloseIndices(ec->resultRelInfo);
	FreeExecutorState(ec->estate);
}

/*
 * End of ExecContext Implementation
 *----------------------------------------------------------------------
 */

/* A few variables that don't seem worth passing around as parameters */
static MemoryContext vac_context = NULL;

static int	elevel = -1;

static TransactionId OldestXmin;
static TransactionId FreezeLimit;

/*
 * For two-step full vacuum, we optimize the second scan by remembering
 * relation stats figured by the first scan.  Since QE runs in a different
 * mpp command/transaction, there is no place to keep this information
 * than global variable.  It is very ugly, but as far as QD runs the
 * right order of operation, it should be ok.
 */
/* we need the max number of aux relation for one base rel. */
#define MaxVacFullInitialStatsSize 8
static VPgClassStats VacFullInitialStats[MaxVacFullInitialStatsSize];
static int VacFullInitialStatsSize = 0;

static BufferAccessStrategy vac_strategy;

/* non-export function prototypes */
static List *get_rel_oids(Oid relid, VacuumStmt *vacstmt,
			 const char *stmttype);
static void vac_truncate_clog(TransactionId frozenXID);
static void vacuum_rel(Relation onerel, VacuumStmt *vacstmt, LOCKMODE lmode, List *updated_stats,
		   bool for_wraparound);
static bool full_vacuum_rel(Relation onerel, VacuumStmt *vacstmt, List *updated_stats);
static void scan_heap_for_truncate(VRelStats *vacrelstats, Relation onerel,
		  VacPageList vacuum_pages);
static void scan_heap(VRelStats *vacrelstats, Relation onerel,
		  VacPageList vacuum_pages, VacPageList fraged_pages);
static bool repair_frag(VRelStats *vacrelstats, Relation onerel,
			VacPageList vacuum_pages, VacPageList fraged_pages,
						int nindexes, Relation *Irel, List *updated_stats,
						int reindex_count);
static void move_chain_tuple(Relation rel,
				 Buffer old_buf, Page old_page, HeapTuple old_tup,
				 Buffer dst_buf, Page dst_page, VacPage dst_vacpage,
				 ExecContext ec, ItemPointer ctid, bool cleanVpd);
static void move_plain_tuple(Relation rel,
				 Buffer old_buf, Page old_page, HeapTuple old_tup,
				 Buffer dst_buf, Page dst_page, VacPage dst_vacpage,
				 ExecContext ec);
static void vacuum_heap(VRelStats *vacrelstats, Relation onerel,
			VacPageList vacpagelist);
static void vacuum_page(Relation onerel, Buffer buffer, VacPage vacpage);
static void vacuum_index(VacPageList vacpagelist, Relation indrel,
						 double num_tuples, int keep_tuples, List *updated_stats,
						 bool check_stats);
static void scan_index(Relation indrel, double num_tuples, List *updated_stats, bool isfull,
			bool check_stats);
static bool tid_reaped(ItemPointer itemptr, void *state);
static bool appendonly_tid_reaped(ItemPointer itemptr, void *state);
static void vac_update_fsm(Relation onerel, VacPageList fraged_pages,
			   BlockNumber rel_pages);
static VacPage copy_vac_page(VacPage vacpage);
static void vpage_insert(VacPageList vacpagelist, VacPage vpnew);
static void *vac_bsearch(const void *key, const void *base,
			size_t nelem, size_t size,
			int (*compar) (const void *, const void *));
static int	vac_cmp_blk(const void *left, const void *right);
static int	vac_cmp_offno(const void *left, const void *right);
static int	vac_cmp_vtlinks(const void *left, const void *right);
static bool enough_space(VacPage vacpage, Size len);
static Size PageGetFreeSpaceWithFillFactor(Relation relation, Page page);
static void dispatchVacuum(VacuumStmt *vacstmt, VacuumStatsContext *ctx);
static Relation open_relation_and_check_permission(VacuumStmt *vacstmt,
												   Oid relid,
												   char expected_relkind,
												   bool forceAccessExclusiveLock);
static void vacuumStatement_Relation(VacuumStmt *vacstmt, Oid relid,
						 List *relations, BufferAccessStrategy bstrategy,
						 bool for_wraparound, bool isTopLevel);

static void
vacuum_combine_stats(VacuumStatsContext *stats_context,
					CdbPgResults* cdb_pgresults);

static void vacuum_appendonly_index(Relation indexRelation,
		AppendOnlyIndexVacuumState *vacuumIndexState,
		List* updated_stats, double rel_tuple_count, bool isfull);

/****************************************************************************
 *																			*
 *			Code common to all flavors of VACUUM and ANALYZE				*
 *																			*
 ****************************************************************************
 */

/*
 * Primary entry point for VACUUM and ANALYZE commands.
 *
 * relid is normally InvalidOid; if it is not, then it provides the relation
 * OID to be processed, and vacstmt->relation is ignored.  (The non-invalid
 * case is currently only used by autovacuum.)
 *
 * for_wraparound is used by autovacuum to let us know when it's forcing
 * a vacuum for wraparound, which should not be auto-cancelled.
 *
 * bstrategy is normally given as NULL, but in autovacuum it can be passed
 * in to use the same buffer strategy object across multiple vacuum() calls.
 *
 * isTopLevel should be passed down from ProcessUtility.
 *
 * It is the caller's responsibility that vacstmt and bstrategy
 * (if given) be allocated in a memory context that won't disappear
 * at transaction commit.
 */
void
vacuum(VacuumStmt *vacstmt, Oid relid,
	   BufferAccessStrategy bstrategy, bool for_wraparound, bool isTopLevel)
{
	const char *stmttype = vacstmt->vacuum ? "VACUUM" : "ANALYZE";
	volatile MemoryContext anl_context = NULL;
	volatile bool all_rels,
				in_outer_xact,
				use_own_xacts;
	List	   *vacuum_relations;
	List	   *analyze_relations;

	if (vacstmt->vacuum && vacstmt->rootonly)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("ROOTPARTITION option cannot be used together with VACUUM, try ANALYZE ROOTPARTITION")));

	if (vacstmt->verbose)
		elevel = INFO;
	else
		elevel = DEBUG2;

	if (Gp_role == GP_ROLE_DISPATCH)
		elevel = DEBUG2; /* vacuum messages aren't interesting from the QD */

	/*
	 * We cannot run VACUUM inside a user transaction block; if we were inside
	 * a transaction, then our commit- and start-transaction-command calls
	 * would not have the intended effect! Furthermore, the forced commit that
	 * occurs before truncating the relation's file would have the effect of
	 * committing the rest of the user's transaction too, which would
	 * certainly not be the desired behavior.  (This only applies to VACUUM
	 * FULL, though.  We could in theory run lazy VACUUM inside a transaction
	 * block, but we choose to disallow that case because we'd rather commit
	 * as soon as possible after finishing the vacuum.	This is mainly so that
	 * we can let go the AccessExclusiveLock that we may be holding.)
	 *
	 * ANALYZE (without VACUUM) can run either way.
	 */
	if (vacstmt->vacuum)
	{
		if (Gp_role == GP_ROLE_DISPATCH)
			PreventTransactionChain(isTopLevel, stmttype);
		in_outer_xact = false;
	}
	else
		in_outer_xact = IsInTransactionChain(isTopLevel);

	/*
	 * Send info about dead objects to the statistics collector, unless we are
	 * in autovacuum --- autovacuum.c does this for itself.
	 */
	if (vacstmt->vacuum && !IsAutoVacuumWorkerProcess())
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

	/* Remember whether we are processing everything in the DB */
	all_rels = (!OidIsValid(relid) && vacstmt->relation == NULL);

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
	if (vacstmt->vacuum)
		vacuum_relations = get_rel_oids(relid, vacstmt, stmttype);
	if (vacstmt->analyze)
		analyze_relations = get_rel_oids(relid, vacstmt, stmttype);

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
	if (vacstmt->vacuum)
		use_own_xacts = true;
	else
	{
		Assert(vacstmt->analyze);
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
	 * If we are running ANALYZE without per-table transactions, we'll need a
	 * memory context with table lifetime.
	 */
	if (!use_own_xacts)
		anl_context = AllocSetContextCreate(PortalContext,
											"Analyze",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);

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
		/* ActiveSnapshot is not set by autovacuum */
		if (ActiveSnapshotSet())
			PopActiveSnapshot();

		/* matches the StartTransaction in PostgresMain() */
		if (Gp_role != GP_ROLE_EXECUTE)
			CommitTransactionCommand();
	}

	/* Turn vacuum cost accounting on or off */
	PG_TRY();
	{
		ListCell   *cur;

		VacuumCostActive = (VacuumCostDelay > 0);
		VacuumCostBalance = 0;

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			vacstmt->appendonly_compaction_segno = NIL;
			vacstmt->appendonly_compaction_insert_segno = NIL;
			vacstmt->appendonly_compaction_vacuum_cleanup = false;
			vacstmt->appendonly_relation_empty = false;
		}

		if (vacstmt->vacuum)
		{
			/*
			 * Loop to process each selected relation which needs to be
			 * vacuumed.
			 */
			foreach(cur, vacuum_relations)
			{
				Oid			relid = lfirst_oid(cur);
				vacuumStatement_Relation(vacstmt, relid, vacuum_relations, bstrategy, for_wraparound, isTopLevel);
			}
		}

		if (vacstmt->analyze)
		{
			/*
			 * If there are no partition tables in the database and ANALYZE
			 * ROOTPARTITION ALL is executed, report a WARNING as no root
			 * partitions are there to be analyzed
			 */
			if (vacstmt->rootonly && NIL == analyze_relations && !vacstmt->relation)
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
				MemoryContext old_context = NULL;

				/*
				 * If using separate xacts, start one for analyze. Otherwise,
				 * we can use the outer transaction, but we still need to call
				 * analyze_rel in a memory context that will be cleaned up on
				 * return (else we leak memory while processing multiple
				 * tables).
				 */
				if (use_own_xacts)
				{
					StartTransactionCommand();
					/* functions in indexes may want a snapshot set */
					PushActiveSnapshot(GetTransactionSnapshot());
				}
				else
					old_context = MemoryContextSwitchTo(anl_context);

				analyze_rel(relid, vacstmt, vac_strategy);

				if (use_own_xacts)
				{
					PopActiveSnapshot();
					CommitTransactionCommand();
				}
				else
				{
					MemoryContextSwitchTo(old_context);
					MemoryContextResetAndDeleteChildren(anl_context);
				}
			}
		}
	}
	PG_CATCH();
	{
		/* Make sure cost accounting is turned off after error */
		VacuumCostActive = false;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* Turn off vacuum cost accounting */
	VacuumCostActive = false;

	/*
	 * Finish up processing.
	 */
	if (use_own_xacts)
	{
		/* here, we are not in a transaction */

		/*
		 * This matches the CommitTransaction waiting for us in
		 * PostgresMain().
		 *
		 * MPP-7632 and MPP-7984: if we're in a vacuum analyze we need to
		 * make sure that this transaction we're in has the right
		 * properties
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			/* Set up the distributed transaction context. */
			setupRegularDtxContext();
		}
		StartTransactionCommand();
	}

	if (vacstmt->vacuum && !IsAutoVacuumWorkerProcess())
	{
		/*
		 * Update pg_database.datfrozenxid, and truncate pg_clog if possible.
		 * (autovacuum.c does this for itself.)
		 */
		vac_update_datfrozenxid();

		/*
		 * If it was a database-wide VACUUM, print FSM usage statistics (we
		 * don't make you be superuser to see these).  We suppress this in
		 * autovacuum, too.
		 */
		if (all_rels)
			PrintFreeSpaceMapStatistics(elevel);
	}

	/*
	 * Clean up working storage --- note we must do this after
	 * StartTransactionCommand, else we might be trying to delete the active
	 * context!
	 */
	MemoryContextDelete(vac_context);
	vac_context = NULL;

	if (anl_context)
		MemoryContextDelete(anl_context);
}

/*
 * Assigns the compaction segment information.
 *
 * The vacuum statement will be modified.
 *
 */
static bool vacuum_assign_compaction_segno(
		Relation onerel,
		List *compactedSegmentFileList,
		List *insertedSegmentFileList,
		VacuumStmt *vacstmt)
{
	List *new_compaction_list;
	List *insert_segno;
	bool is_drop;

	Assert(Gp_role != GP_ROLE_EXECUTE);
	Assert(vacstmt->appendonly_compaction_segno == NIL);
	Assert(vacstmt->appendonly_compaction_insert_segno == NIL);
	Assert (RelationIsValid(onerel));

	/*
	 * Assign a compaction segment num and insert segment num
	 * on master or on segment if in utility mode
	 */
	if (!(RelationIsAoRows(onerel) || RelationIsAoCols(onerel)) || !gp_appendonly_compaction)
	{
		return true;
	}

	if (HasSerializableBackends(false))
	{
		elog(LOG, "Skip compaction because of concurrent serializable transactions");
		return false;
	}

	new_compaction_list = SetSegnoForCompaction(onerel,
			compactedSegmentFileList, insertedSegmentFileList, &is_drop);
	if (new_compaction_list)
	{
		if (!is_drop)
		{
			insert_segno = lappend_int(NIL, SetSegnoForCompactionInsert(onerel,
				new_compaction_list, compactedSegmentFileList, insertedSegmentFileList));
		}
		else
		{
			/*
			 * If we continue an aborted drop phase, we do not assign a real
			 * insert segment file.
			 */
			insert_segno = list_make1_int(APPENDONLY_COMPACTION_SEGNO_INVALID);
		}

		elogif(Debug_appendonly_print_compaction, LOG,
				"Schedule compaction on AO table: "
				"compact segno list length %d, insert segno length %d",
				list_length(new_compaction_list), list_length(insert_segno));
	}

	if (!new_compaction_list)
	{
		elog(DEBUG3, "No valid compact segno for releation %s (%d)",
				RelationGetRelationName(onerel),
				RelationGetRelid(onerel));
		return false;
	}
	else
	{
		vacstmt->appendonly_compaction_insert_segno = insert_segno;
		vacstmt->appendonly_compaction_segno = new_compaction_list;
		return true;
	}
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
			if (allowSystemTableModsDDL)
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
 * Modify the Vacuum statement to vacuum an individual
 * relation. This ensures that only one relation will be
 * locked for vacuum, when the user issues a "vacuum <db>"
 * command, or a "vacuum <parent_partition_table>"
 * command.
 */
static void
vacuumStatement_AssignRelation(VacuumStmt *vacstmt, Oid relid, List *relations)
{
	if (list_length(relations) > 1 || vacstmt->relation == NULL)
	{
		char	*relname		= get_rel_name(relid);
		char	*namespace_name =
			get_namespace_name(get_rel_namespace(relid));

		if (relname == NULL)
		{
			elog(ERROR, "Relation name does not exist for relation with oid %d", relid);
			return;
		}

		if (namespace_name == NULL)
		{
			elog(ERROR, "Namespace does not exist for relation with oid %d", relid);
			return;
		}

		/* XXX: dispatch OID than name */
		vacstmt->relation = makeRangeVar(namespace_name, relname, -1);
	}
}

/*
 * Chose a source and destination segfile for compaction.  It assumes that we
 * are in the vacuum memory context, and executing in DISPATCH or UTILITY mode.
 * Return false if we are done with all segfiles.
 */
static bool
vacuumStatement_AssignAppendOnlyCompactionInfo(VacuumStmt *vacstmt,
		Relation onerel,
		List *compactedSegmentFileList,
		List *insertedSegmentFileList,
		bool *getnextrelation)
{
	Assert(Gp_role != GP_ROLE_EXECUTE);
	Assert(vacstmt);
	Assert(getnextrelation);
	Assert(RelationIsAoRows(onerel) || RelationIsAoCols(onerel));

	if (!vacuum_assign_compaction_segno(onerel,
				compactedSegmentFileList,
				insertedSegmentFileList,
				vacstmt))
	{
		/* There is nothing left to do for this relation */
		if (list_length(compactedSegmentFileList) > 0)
		{
			/*
			 * We now need to vacuum the auxility relations of the
			 * append-only relation
			 */
			vacstmt->appendonly_compaction_vacuum_cleanup = true;

			/* Provide the list of all compacted segment numbers with it */
			list_free(vacstmt->appendonly_compaction_segno);
			vacstmt->appendonly_compaction_segno = list_copy(compactedSegmentFileList);
			list_free(vacstmt->appendonly_compaction_insert_segno);
			vacstmt->appendonly_compaction_insert_segno = list_copy(insertedSegmentFileList);
		}
		else
		{
			return false;
		}
	}

	if (vacstmt->appendonly_compaction_segno &&
			vacstmt->appendonly_compaction_insert_segno &&
			!vacstmt->appendonly_compaction_vacuum_cleanup)
	{
		/*
		 * as long as there are real segno to compact, we
		 * keep processing this relation.
		 */
		*getnextrelation = false;
	}
	return true;
}

bool
vacuumStatement_IsInAppendOnlyDropPhase(VacuumStmt *vacstmt)
{
	Assert(vacstmt);
	return (vacstmt->appendonly_compaction_segno &&
			!vacstmt->appendonly_compaction_insert_segno &&
			!vacstmt->appendonly_compaction_vacuum_prepare &&
			!vacstmt->appendonly_compaction_vacuum_cleanup);
}

bool
vacuumStatement_IsInAppendOnlyCompactionPhase(VacuumStmt *vacstmt)
{
	Assert(vacstmt);
	return (vacstmt->appendonly_compaction_segno &&
			vacstmt->appendonly_compaction_insert_segno &&
			!vacstmt->appendonly_compaction_vacuum_prepare &&
			!vacstmt->appendonly_compaction_vacuum_cleanup);
}

bool
vacuumStatement_IsInAppendOnlyPseudoCompactionPhase(VacuumStmt *vacstmt)
{
	Assert(vacstmt);
	return (vacstmt->appendonly_compaction_segno &&
			vacstmt->appendonly_compaction_insert_segno &&
			linitial_int(vacstmt->appendonly_compaction_insert_segno)
				== APPENDONLY_COMPACTION_SEGNO_INVALID &&
			!vacstmt->appendonly_compaction_vacuum_prepare &&
			!vacstmt->appendonly_compaction_vacuum_cleanup);
}

bool
vacuumStatement_IsInAppendOnlyPreparePhase(VacuumStmt* vacstmt)
{
	Assert(vacstmt);
	return (vacstmt->appendonly_compaction_vacuum_prepare);
}

bool
vacummStatement_IsInAppendOnlyCleanupPhase(VacuumStmt *vacstmt)
{
	Assert(vacstmt);
	return (vacstmt->appendonly_compaction_vacuum_cleanup);
}

/*
 * Processing of the vacuumStatement for given relid.
 *
 * The function is called by vacuumStatement once for each relation to vacuum.
 * In order to connect QD and QE work for vacuum, we employ a little
 * complicated mechanism here; we separate one relation vacuum process
 * to a separate steps, depending on the type of storage (heap/AO),
 * and perform each step in separate transactions, so that QD can open
 * a distributed transaction and embrace QE work inside it.  As opposed to
 * old postgres code, where one transaction is opened and closed for each
 * auxiliary relation, here a transaction processes them as a set starting
 * from the base relation.  This is the entry point of one base relation,
 * and QD makes some decision what kind of stage we perform, and tells it
 * to QE with vacstmt fields through dispatch.
 *
 * For heap VACUUM FULL, we need two transactions.  One is to move tuples
 * from a page to another, to empty out last pages, which typically goes
 * into repair_frag.  We used to perform truncate operation there, but
 * it required to record transaction commit locally, which is not pleasant
 * if QD decides to cancel the whoe distributed transaction.  So the truncate
 * step is separated to a second transaction.  This two step operation is
 * performed on both base relation and toast relation at the same time.
 *
 * Lazy vacuum to heap is one step operation.
 *
 * AO compaction is rather complicated.  There are four phases.
 *   - prepare phase
 *   - compaction phase
 *   - drop phase
 *   - cleanup phase
 * Out of these, compaction and drop phase might repeat multiple times.
 * We go through the list of available segment files by looking up catalog,
 * and perform a compaction operation, which appends the whole segfile
 * to another available one, if the source segfile looks to be dirty enough.
 * If we find such one and perform compaction, the next step is drop. In
 * order to allow concurrent read it is required for the drop phase to
 * be a separate transaction.  We mark the segfile as an awaiting-drop
 * in the catalog, and the drop phase actually drops the segfile from the
 * disk.  There are some cases where we cannot drop the segfile immediately,
 * in which case we just skip it and leave the catalog to have awaiting-drop
 * state for this segfile.  Aside from the compaction and drop phases, the
 * rest is much simpler.  The prepare phase is to truncate unnecessary
 * blocks after the logical EOF, and the cleanup phase does normal heap
 * vacuum on auxiliary relations (toast, aoseg, block directory, visimap,)
 * as well as updating stats info in catalog.  Keep in mind that if the
 * vacuum is full, we need the same two steps as the heap base relation
 * case.  So cleanup phase in AO may consume two transactions.
 *
 * While executing these multiple transactions, we acquire a session
 * lock across transactions, in order to keep concurrent work on the
 * same relation away.  It doesn't look intuitive, though, if you look
 * at QE work, because from its perspective it is always one step, therefore
 * there is no session lock technically (we actually acquire and release
 * it as it's harmless.)  Session lock doesn't work here, because QE
 * is under a distributed transaction and we haven't supported session
 * lock recording in transaction prepare.  This should be ok as far as
 * we are dealing with user table, because other MPP work also tries
 * to take a relation lock, which would conflict with this vacuum work
 * on master.  Be careful with catalog tables, because we take locks on
 * them and release soon much before the end of transaction.  That means
 * QE still needs to deal with concurrent work well.
 */
static void
vacuumStatement_Relation(VacuumStmt *vacstmt, Oid relid,
						 List *relations, BufferAccessStrategy bstrategy,
						 bool for_wraparound, bool isTopLevel)
{
	LOCKMODE			lmode = NoLock;
	Relation			onerel;
	LockRelId			onerelid;
	MemoryContext		oldctx;
	VacuumStatsContext stats_context;

	vacstmt = copyObject(vacstmt);
	vacstmt->analyze = false;
	vacstmt->vacuum = true;

	/*
	 * We compact segment file by segment file.
	 * Therefore in some cases, we have multiple vacuum dispatches
	 * per relation.
	 */
	bool getnextrelation = false;

	/* Number of rounds performed on this relation */
	int relationRound = 0;

	List* compactedSegmentFileList = NIL;
	List* insertedSegmentFileList = NIL;

	bool dropPhase = false;
	bool truncatePhase = false;

	Assert(vacstmt);

	if (Gp_role != GP_ROLE_EXECUTE)
	{
		/* First call on a relation is the prepare phase */
		vacstmt->appendonly_compaction_vacuum_prepare = true;

		/*
		 * Reset truncate flag always as we may iterate more than one relation.
		 */
		vacstmt->heap_truncate = false;
	}

	while (!getnextrelation)
	{
		getnextrelation = true;

		/*
		 * The following block of code is relevant only for AO tables.  The
		 * only phases relevant are prepare phase, compaction phase and the
		 * first phase of cleanup for VACUUM FULL (truncatePhase set to true
		 * refers to this phase). 
		 */
		if (Gp_role != GP_ROLE_EXECUTE && (!dropPhase || truncatePhase))
		{
			 /* Reset the compaction segno if new relation or segment file is
			  * started
			  */
			list_free(vacstmt->appendonly_compaction_segno);
			list_free(vacstmt->appendonly_compaction_insert_segno);
			vacstmt->appendonly_compaction_segno = NIL;
			vacstmt->appendonly_compaction_insert_segno = NIL;
			/*
			 * We should not reset the cleanup flag for truncate phase because
			 * it is a sub part of the cleanup phase for VACUUM FULL case 
			 */ 
			if (!truncatePhase)
				vacstmt->appendonly_compaction_vacuum_cleanup = false;
		}

		/* Set up the distributed transaction context. */
		if (Gp_role == GP_ROLE_DISPATCH)
			setupRegularDtxContext();

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

		if (!vacstmt->full)
		{
			/*
			 * PostgreSQL does this:
			 * During a lazy VACUUM we can set the PROC_IN_VACUUM flag, which lets other
			 * concurrent VACUUMs know that they can ignore this one while
			 * determining their OldestXmin.  (The reason we don't set it during a
			 * full VACUUM is exactly that we may have to run user- defined
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
			 * We also set the VACUUM_FOR_WRAPAROUND flag, which is passed down
			 * by autovacuum; it's used to avoid cancelling a vacuum that was
			 * invoked in an emergency.
			 *
			 * Note: this flag remains set until CommitTransaction or
			 * AbortTransaction.  We don't want to clear it until we reset
			 * MyProc->xid/xmin, else OldestXmin might appear to go backwards,
			 * which is probably Not Good.
			 */
			LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
#if 0 /* Upstream code not applicable to GPDB */
			MyProc->vacuumFlags |= PROC_IN_VACUUM;
#endif
			if (for_wraparound)
				MyProc->vacuumFlags |= PROC_VACUUM_FOR_WRAPAROUND;
			LWLockRelease(ProcArrayLock);
		}

		/*
		 * AO only: QE can tell drop phase here with dispatched vacstmt.
		 */
		if (Gp_role == GP_ROLE_EXECUTE)
			dropPhase = vacuumStatement_IsInAppendOnlyDropPhase(vacstmt);

		/*
		 * Open the relation with an appropriate lock, and check the permission.
		 */
		onerel = open_relation_and_check_permission(vacstmt, relid, RELKIND_RELATION, dropPhase);

		/*
		 * onerel can be NULL for the following cases:
		 * 1. If the user does not have the permissions to vacuum the table
		 * 2. For AO tables if the drop phase cannot be performed and should be skipped 
		 */
		if (onerel == NULL)
		{
			if ((Gp_role != GP_ROLE_EXECUTE) && dropPhase)
			{
				/*
				 * To ensure that vacuum decreases the age for appendonly
				 * tables even if drop phase is getting skipped, perform
				 * cleanup phase so that the relfrozenxid value is updated
				 * correctly in pg_class
				 */
				vacstmt->appendonly_compaction_vacuum_cleanup = true;
				dropPhase = false;
				/*
				 * Since the drop phase needs to be skipped, we need to
				 * deregister the segnos which were marked for drop in the
				 * compaction phase
				 */
				DeregisterSegnoForCompactionDrop(relid,
								vacstmt->appendonly_compaction_segno);
				onerel = open_relation_and_check_permission(vacstmt, relid, RELKIND_RELATION, false);
			}		

			if (onerel == NULL)
			{
				PopActiveSnapshot();
				CommitTransactionCommand();
				continue;
			}
		}

		vacuumStatement_AssignRelation(vacstmt, relid, relations);

		if (Gp_role != GP_ROLE_EXECUTE)
		{
			/*
			 * Keep things generated by this QD decision beyond a transaction.
			 */
			oldctx = MemoryContextSwitchTo(vac_context);
			if (RelationIsHeap(onerel))
			{
				/*
				 * We perform truncate in the second transaction, to avoid making
				 * it necessary to record transaction commit in the middle of
				 * vacuum operation in case we move tuples across pages.  It may
				 * not need to do so if the relation is clean, but the decision
				 * to perform truncate is segment-local and QD cannot tell if
				 * everyone can skip it.
				 */
				if (vacstmt->full)
				{
					Assert(relationRound == 0 || relationRound == 1);
					if (relationRound == 0)
						getnextrelation = false;
					else if (relationRound == 1)
						vacstmt->heap_truncate = true;
				}
			}
			else
			{
				/* the rest is about AO tables */
				if (vacstmt->appendonly_compaction_vacuum_prepare)
				{
					getnextrelation = false;
					dropPhase = false;
				}
				else if (dropPhase)
				{
					if (HasSerializableBackends(false))
					{
						/*
						 * Checking at this point is safe because
						 * any serializable transaction that could start afterwards
						 * will already see the state with AWAITING_DROP. We
						 * have only to deal with transactions that started before
						 * our transaction.
						 *
						 * We immediatelly get the next relation. There is no
						 * reason to stay in this relation. Actually, all
						 * other ao relation will skip the compaction step.
						 */
						elogif(Debug_appendonly_print_compaction, LOG,
								"Skipping freeing compacted append-only segment file "
								"because of concurrent serializable transaction");

						DeregisterSegnoForCompactionDrop(relid, vacstmt->appendonly_compaction_segno);
						vacstmt->appendonly_compaction_vacuum_cleanup = true;
						dropPhase = false;
						getnextrelation = false;
					}
					else
					{
						elogif(Debug_appendonly_print_compaction, LOG,
								"Dispatch drop transaction on append-only relation %s",
								RelationGetRelationName(onerel));

						RegisterSegnoForCompactionDrop(relid, vacstmt->appendonly_compaction_segno);
						list_free(vacstmt->appendonly_compaction_insert_segno);
						vacstmt->appendonly_compaction_insert_segno = NIL;
						dropPhase = false;
						getnextrelation = false;
					}
				}
				else
				{
					/* Either we are in cleanup or in compaction phase */
					if (!vacstmt->appendonly_compaction_vacuum_cleanup)
					{
						/* This block is only relevant for compaction */
						if (!vacuumStatement_AssignAppendOnlyCompactionInfo(vacstmt,
									onerel, compactedSegmentFileList,
									insertedSegmentFileList, &getnextrelation))
						{
							MemoryContextSwitchTo(oldctx);
							/* Nothing left to do for this relation */
							relation_close(onerel, NoLock);
							PopActiveSnapshot();
							CommitTransactionCommand();
							/* don't dispatch this iteration */
							continue;
						}

						compactedSegmentFileList =
							list_union_int(compactedSegmentFileList,
								vacstmt->appendonly_compaction_segno);
						insertedSegmentFileList =
							list_union_int(insertedSegmentFileList,
								vacstmt->appendonly_compaction_insert_segno);

						dropPhase = !getnextrelation;
					}
				}
				MemoryContextSwitchTo(oldctx);

				/*
				 * If VACUUM FULL and in cleanup phase, perform two-step for
				 * aux relations.
				 */
				if (vacstmt->full &&
					vacstmt->appendonly_compaction_vacuum_cleanup)
				{
					if (truncatePhase)
					{
						truncatePhase = false;
						vacstmt->heap_truncate = true;
					}
					else
					{
						truncatePhase = true;
						getnextrelation = false;
					}
				}
			}
		}

		/*
		 * Reset the global array if this step is not for heap truncate.
		 * We use this array only when trancating.
		 */
		if (!vacstmt->heap_truncate)
			VacFullInitialStatsSize = 0;

		/*
		 * Record the relation that is in the vacuum process, so
		 * that we can clear up its freespace map entry when the
		 * vacuum process crashes or is cancelled.
		 *
		 * XXX: Have to allocate the space inside TopMemoryContext,
		 * since it is required during commit.
		 */
		oldctx = MemoryContextSwitchTo(TopMemoryContext);
		AppendRelToVacuumRels(onerel);
		MemoryContextSwitchTo(oldctx);

		/*
		 * If we are in the dispatch mode, dispatch this modified
		 * vacuum statement to QEs, and wait for them to finish.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			int 		i, nindexes;
			bool 		has_bitmap = false;
			Relation   *i_rel = NULL;

			stats_context.updated_stats = NIL;

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

			/*
			 * We have to acquire a ShareLock for the relation which has bitmap
			 * indexes, since reindex is used later. Otherwise, concurrent
			 * vacuum and inserts may cause deadlock. MPP-5960
			 */
			if (has_bitmap)
				LockRelation(onerel, ShareLock);

			dispatchVacuum(vacstmt, &stats_context);
		}

		if (vacstmt->full)
			lmode = AccessExclusiveLock;
		else if (RelationIsAoRows(onerel) || RelationIsAoCols(onerel))
			lmode = AccessShareLock;
		else
			lmode = ShareUpdateExclusiveLock;

		if (relationRound == 0)
		{
			onerelid = onerel->rd_lockInfo.lockRelId;

			/*
			 * Get a session-level lock too. This will protect our
			 * access to the relation across multiple transactions, so
			 * that we can vacuum the relation's TOAST table (if any)
			 * secure in the knowledge that no one is deleting the
			 * parent relation.
			 *
			 * NOTE: this cannot block, even if someone else is
			 * waiting for access, because the lock manager knows that
			 * both lock requests are from the same process.
			 */
			LockRelationIdForSession(&onerelid, lmode);
		}
		vacuum_rel(onerel, vacstmt, lmode, stats_context.updated_stats,
				   for_wraparound);

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			list_free_deep(stats_context.updated_stats);
			stats_context.updated_stats = NIL;

			/*
			 * Update ao master tupcount the hard way after the compaction and
			 * after the drop.
			 */
			if (vacstmt->appendonly_compaction_segno)
			{
				Assert(RelationIsAoRows(onerel) || RelationIsAoCols(onerel));

				if (vacuumStatement_IsInAppendOnlyCompactionPhase(vacstmt) &&
						!vacuumStatement_IsInAppendOnlyPseudoCompactionPhase(vacstmt))
				{
					/* In the compact phase, we need to update the information of the segment file we inserted into */
					UpdateMasterAosegTotalsFromSegments(onerel, SnapshotNow, vacstmt->appendonly_compaction_insert_segno, 0);
				}
				else if (vacuumStatement_IsInAppendOnlyDropPhase(vacstmt))
				{
					/* In the drop phase, we need to update the information of the compacted segment file(s) */
					UpdateMasterAosegTotalsFromSegments(onerel, SnapshotNow, vacstmt->appendonly_compaction_segno, 0);
				}
			}

			/*
			 * We need some transaction to update the catalog.  We could do
			 * it on the outer vacuumStatement, but it is useful to track
			 * relation by relation.
			 */
			if (relationRound == 0 && !vacuumStatement_IsTemporary(onerel))
			{
				char *vsubtype = ""; /* NOFULL */

				if (IsAutoVacuumWorkerProcess())
					vsubtype = "AUTO";
				else
				{
					if (vacstmt->full &&
						(0 == vacstmt->freeze_min_age))
						vsubtype = "FULL FREEZE";
					else if (vacstmt->full)
						vsubtype = "FULL";
					else if (0 == vacstmt->freeze_min_age)
						vsubtype = "FREEZE";
				}
				MetaTrackUpdObject(RelationRelationId,
								   relid,
								   GetUserId(),
								   "VACUUM",
								   vsubtype);
			}
		}

		/*
		 * Close source relation now, but keep lock so that no one
		 * deletes it before we commit.  (If someone did, they'd
		 * fail to clean up the entries we made in pg_statistic.
		 * Also, releasing the lock before commit would expose us
		 * to concurrent-update failures in update_attstats.)
		 */
		relation_close(onerel, NoLock);


		if (list_length(relations) > 1)
		{
			pfree(vacstmt->relation->schemaname);
			pfree(vacstmt->relation->relname);
			pfree(vacstmt->relation);
			vacstmt->relation = NULL;
		}
		vacstmt->appendonly_compaction_vacuum_prepare = false;

		PopActiveSnapshot();

		/*
		 * Transaction commit is always executed on QD.
		 */
		if (Gp_role != GP_ROLE_EXECUTE)
			CommitTransactionCommand();

		if (relationRound == 0)
		{
			SIMPLE_FAULT_INJECTOR(VacuumRelationEndOfFirstRound);
		}

		relationRound++;
	}

	if (lmode != NoLock)
	{
		UnlockRelationIdForSession(&onerelid, lmode);
	}

	if (compactedSegmentFileList)
	{
		list_free(compactedSegmentFileList);
		compactedSegmentFileList = NIL;
	}
	if (insertedSegmentFileList)
	{
		list_free(insertedSegmentFileList);
		insertedSegmentFileList = NIL;
	}
	if (vacstmt->appendonly_compaction_segno)
	{
		list_free(vacstmt->appendonly_compaction_segno);
		vacstmt->appendonly_compaction_segno = NIL;
	}
	if (vacstmt->appendonly_compaction_insert_segno)
	{
		list_free(vacstmt->appendonly_compaction_insert_segno);
		vacstmt->appendonly_compaction_insert_segno = NIL;
	}
}

/*
 * Build a list of Oids for each relation to be processed
 *
 * The list is built in vac_context so that it will survive across our
 * per-relation transactions.
 */
static List *
get_rel_oids(Oid relid, VacuumStmt *vacstmt, const char *stmttype)
{
	List	   *oid_list = NIL;
	MemoryContext oldcontext;

	/* OID supplied by VACUUM's caller? */
	if (OidIsValid(relid))
	{
		oldcontext = MemoryContextSwitchTo(vac_context);
		oid_list = lappend_oid(oid_list, relid);
		MemoryContextSwitchTo(oldcontext);
	}
	else if (vacstmt->relation)
	{
		if (strcmp(stmttype, "VACUUM") == 0)
		{
			/* Process a specific relation */
			Oid			relid;
			List	   *prels = NIL;

			relid = RangeVarGetRelid(vacstmt->relation, false);

			if (rel_is_partitioned(relid))
			{
				PartitionNode *pn;

				pn = get_parts(relid, 0, 0, false, true /*includesubparts*/);

				prels = all_partition_relids(pn);
			}
			else if (rel_is_child_partition(relid))
			{
				/* get my children */
				prels = find_all_inheritors(relid);
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

			relationOid = RangeVarGetRelid(vacstmt->relation, false);
			PartStatus ps = rel_part_status(relationOid);

			if (ps != PART_STATUS_ROOT && vacstmt->rootonly)
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
				if (!vacstmt->rootonly)
				{
					oid_list = all_leaf_partition_relids(pn); /* all leaves */

					if (optimizer_analyze_midlevel_partition)
					{
						oid_list = list_concat(oid_list, all_interior_partition_relids(pn)); /* interior partitions */
					}
				}
				oid_list = lappend_oid(oid_list, relationOid); /* root partition */
			}
			else if (ps == PART_STATUS_INTERIOR) /* analyze an interior partition directly */
			{
				/* disable analyzing mid-level partitions directly since the users are encouraged
				 * to work with the root partition only. To gather stats on mid-level partitions
				 * (for Orca's use), the user should run ANALYZE or ANALYZE ROOTPARTITION on the
				 * root level with optimizer_analyze_midlevel_partition GUC set to ON.
				 * Planner uses the stats on leaf partitions, so its unnecesary to collect stats on
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
		/* Process all plain relations listed in pg_class */
		Relation	pgclass;
		HeapScanDesc scan;
		HeapTuple	tuple;
		ScanKeyData key;
		Oid candidateOid;

		ScanKeyInit(&key,
					Anum_pg_class_relkind,
					BTEqualStrategyNumber, F_CHAREQ,
					CharGetDatum(RELKIND_RELATION));

		pgclass = heap_open(RelationRelationId, AccessShareLock);

		scan = heap_beginscan(pgclass, SnapshotNow, 1, &key);

		while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
		{
			Form_pg_class classForm = (Form_pg_class) GETSTRUCT(tuple);

			/*
			 * Don't include non-vacuum-able relations:
			 *   - External tables
			 *   - Foreign tables
			 *   - etc.
			 */
			if (classForm->relkind == RELKIND_RELATION && (
					classForm->relstorage == RELSTORAGE_EXTERNAL ||
					classForm->relstorage == RELSTORAGE_FOREIGN  ||
					classForm->relstorage == RELSTORAGE_VIRTUAL))
				continue;

			/* Skip persistent tables for Vacuum full. Vacuum full could turn
			 * out dangerous as it has potential to move tuples around causing
			 * the TIDs for tuples to change, which violates its reference from
			 * gp_relation_node. One scenario where this can happen is zero-page
			 * due to failure after page extension but before page initialization.
			 */
			if (vacstmt->full &&
				GpPersistent_IsPersistentRelation(HeapTupleGetOid(tuple)))
				continue;

			/* Make a relation list entry for this guy */
			candidateOid = HeapTupleGetOid(tuple);

			/* Skip non root partition tables if ANALYZE ROOTPARTITION ALL is executed */
			if (vacstmt->rootonly && !rel_is_partitioned(candidateOid))
			{
				continue;
			}

			// skip mid-level partition tables if we have disabled collecting statistics for them
			PartStatus ps = rel_part_status(candidateOid);
			if (!optimizer_analyze_midlevel_partition && ps == PART_STATUS_INTERIOR)
			{
				continue;
			}

			oldcontext = MemoryContextSwitchTo(vac_context);
			oid_list = lappend_oid(oid_list, candidateOid);
			MemoryContextSwitchTo(oldcontext);
		}

		heap_endscan(scan);
		heap_close(pgclass, AccessShareLock);
	}

	return oid_list;
}

/*
 * vacuum_set_xid_limits() -- compute oldest-Xmin and freeze cutoff points
 */
void
vacuum_set_xid_limits(int freeze_min_age, bool sharedRel,
					  TransactionId *oldestXmin,
					  TransactionId *freezeLimit)
{
	int			freezemin;
	TransactionId limit;
	TransactionId safeLimit;

	/*
	 * We can always ignore processes running lazy vacuum.	This is because we
	 * use these values only for deciding which tuples we must keep in the
	 * tables.	Since lazy vacuum doesn't write its XID anywhere, it's safe to
	 * ignore it.  In theory it could be problematic to ignore lazy vacuums on
	 * a full vacuum, but keep in mind that only one vacuum process can be
	 * working on a particular table at any time, and that each vacuum is
	 * always an independent transaction.
	 */
	*oldestXmin = GetOldestXmin(sharedRel, true);

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
}

void
vac_update_relstats_from_list(Relation rel,
							  BlockNumber num_pages, double num_tuples,
							  bool hasindex, TransactionId frozenxid,
							  List *updated_stats)
{
	/*
	 * If this is QD, use the stats collected in updated_stats instead of
	 * the one provided through 'num_pages' and 'num_tuples'.  It doesn't
	 * seem worth doing so for system tables, though (it'd better say
	 * "non-distributed" tables than system relations here, but for now
	 * it's effectively the same.)
	 */
	if (Gp_role == GP_ROLE_DISPATCH && !IsSystemRelation(rel))
	{
		ListCell *lc;
		num_pages = 0;
		num_tuples = 0.0;
		foreach (lc, updated_stats)
		{
			VPgClassStats *stats = (VPgClassStats *) lfirst(lc);
			if (stats->relid == RelationGetRelid(rel))
			{
				num_pages += stats->rel_pages;
				num_tuples += stats->rel_tuples;
				break;
			}
		}
	}

	vac_update_relstats(RelationGetRelid(rel), num_pages, num_tuples,
						hasindex, frozenxid);
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
 *		safe since the new values are correct whether or not this transaction
 *		commits.  The reason for this is that if we updated these tuples in
 *		the usual way, vacuuming pg_class itself wouldn't work very well ---
 *		by the time we got done with a vacuum cycle, most of the tuples in
 *		pg_class would've been obsoleted.  Of course, this only works for
 *		fixed-size never-null columns, but these are.
 *
 *		This routine is shared by full VACUUM, lazy VACUUM, and stand-alone
 *		ANALYZE.
 */
void
vac_update_relstats(Oid relid, BlockNumber num_pages, double num_tuples,
					bool hasindex, TransactionId frozenxid)
{
	Relation	rd;
	HeapTuple	ctup;
	Form_pg_class pgcform;
	bool		dirty;

	Assert(relid != InvalidOid);

	/*
	 * CDB: send the number of tuples and the number of pages in pg_class located
	 * at QEs through the dispatcher.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		/* cdbanalyze_get_relstats(rel, &num_pages, &num_tuples);*/
		StringInfoData buf;
		VPgClassStats stats;

		pq_beginmessage(&buf, 'y');
		pq_sendstring(&buf, "VACUUM");
		stats.relid = relid;
		stats.rel_pages = num_pages;
		stats.rel_tuples = num_tuples;
		stats.empty_end_pages = 0;
		pq_sendint(&buf, sizeof(VPgClassStats), sizeof(int));
		pq_sendbytes(&buf, (char *) &stats, sizeof(VPgClassStats));
		pq_endmessage(&buf);
	}

	/*
	 * We need a way to distinguish these 2 cases:
	 * a) ANALYZEd/VACUUMed table is empty
	 * b) Table has never been ANALYZEd/VACUUMed
	 * To do this, in case (a), we set relPages = 1. For case (b), relPages = 0.
	 */
	if (num_pages < 1.0)
	{
		Assert(num_tuples < 1.0);
		num_pages = 1.0;
	}

	/*
	 * update number of tuples and number of pages in pg_class
	 */
	rd = heap_open(RelationRelationId, RowExclusiveLock);

	/* Fetch a copy of the tuple to scribble on */
	ctup = SearchSysCacheCopy(RELOID,
							  ObjectIdGetDatum(relid),
							  0, 0, 0);
	if (!HeapTupleIsValid(ctup))
		elog(ERROR, "pg_class entry for relid %u vanished during vacuuming",
			 relid);
	pgcform = (Form_pg_class) GETSTRUCT(ctup);

	/* Apply required updates, if any, to copied tuple */

	dirty = false;
	if (pgcform->relpages != (int32) num_pages)
	{
		pgcform->relpages = (int32) num_pages;
		dirty = true;
	}
	if (pgcform->reltuples != (float4) num_tuples)
	{
		pgcform->reltuples = (float4) num_tuples;
		dirty = true;
	}
	if (pgcform->relhasindex != hasindex)
	{
		pgcform->relhasindex = hasindex;
		dirty = true;
	}

	elog(DEBUG2, "Vacuum oid=%u pages=%d tuples=%f",
		 relid, pgcform->relpages, pgcform->reltuples);
	/*
	 * If we have discovered that there are no indexes, then there's no
	 * primary key either.	This could be done more thoroughly...
	 */
	if (!hasindex)
	{
		if (pgcform->relhaspkey)
		{
			pgcform->relhaspkey = false;
			dirty = true;
		}
	}

	/*
	 * relfrozenxid should never go backward.  Caller can pass
	 * InvalidTransactionId if it has no new data.
	 */
	if (TransactionIdIsNormal(frozenxid) &&
		TransactionIdIsValid(pgcform->relfrozenxid) &&
		TransactionIdPrecedes(pgcform->relfrozenxid, frozenxid))
	{
		pgcform->relfrozenxid = frozenxid;
		dirty = true;
	}

	/*
	 * If anything changed, write out the tuple.  Even if nothing changed,
	 * force relcache invalidation so all backends reset their rd_targblock
	 * --- otherwise it might point to a page we truncated away.
	 */
	if (dirty)
	{
		heap_inplace_update(rd, ctup);
		/* the above sends a cache inval message */
	}
	else
	{
		/* no need to change tuple, but force relcache inval anyway */
		CacheInvalidateRelcacheByTuple(ctup);
	}

	heap_close(rd, RowExclusiveLock);
}


/*
 *	vac_update_datfrozenxid() -- update pg_database.datfrozenxid for our DB
 *
 *		Update pg_database's datfrozenxid entry for our database to be the
 *		minimum of the pg_class.relfrozenxid values.  If we are able to
 *		advance pg_database.datfrozenxid, also try to truncate pg_clog.
 *
 *		We violate transaction semantics here by overwriting the database's
 *		existing pg_database tuple with the new value.	This is reasonably
 *		safe since the new value is correct whether or not this transaction
 *		commits.  As with vac_update_relstats, this avoids leaving dead tuples
 *		behind after a VACUUM.
 *
 *		This routine is shared by full and lazy VACUUM.
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
	bool		dirty = false;

	/*
	 * Initialize the "min" calculation with GetOldestXmin, which is a
	 * reasonable approximation to the minimum relfrozenxid for not-yet-
	 * committed pg_class entries for new tables; see AddNewRelationTuple().
	 * Se we cannot produce a wrong minimum by starting with this.
	 */
	newFrozenXid = GetOldestXmin(true, true);

	/*
	 * We must seqscan pg_class to find the minimum Xid, because there is no
	 * index that can help us here.
	 */
	relation = heap_open(RelationRelationId, AccessShareLock);

	scan = systable_beginscan(relation, InvalidOid, false,
							  SnapshotNow, 0, NULL);

	while ((classTup = systable_getnext(scan)) != NULL)
	{
		Form_pg_class classForm = (Form_pg_class) GETSTRUCT(classTup);

		if (!should_have_valid_relfrozenxid(HeapTupleGetOid(classTup),
											classForm->relkind,
											classForm->relstorage))
		{
			Assert(!TransactionIdIsValid(classForm->relfrozenxid));
			continue;
		}

		Assert(TransactionIdIsNormal(classForm->relfrozenxid));

		if (TransactionIdPrecedes(classForm->relfrozenxid, newFrozenXid))
			newFrozenXid = classForm->relfrozenxid;
	}

	/* we're done with pg_class */
	systable_endscan(scan);
	heap_close(relation, AccessShareLock);

	Assert(TransactionIdIsNormal(newFrozenXid));

	/* Now fetch the pg_database tuple we need to update. */
	relation = heap_open(DatabaseRelationId, RowExclusiveLock);

	/* Fetch a copy of the tuple to scribble on */
	tuple = SearchSysCacheCopy(DATABASEOID,
							   ObjectIdGetDatum(MyDatabaseId),
							   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "could not find tuple for database %u", MyDatabaseId);
	dbform = (Form_pg_database) GETSTRUCT(tuple);

	/*
	 * Don't allow datfrozenxid to go backward (probably can't happen anyway);
	 * and detect the common case where it doesn't go forward either.
	 */
	if (TransactionIdPrecedes(dbform->datfrozenxid, newFrozenXid))
	{
		dbform->datfrozenxid = newFrozenXid;
		dirty = true;
	}

	if (dirty)
		heap_inplace_update(relation, tuple);

	heap_freetuple(tuple);
	heap_close(relation, RowExclusiveLock);

	/*
	 * If we were able to advance datfrozenxid, mark the flat-file copy of
	 * pg_database for update at commit, and see if we can truncate pg_clog.
	 */
	if (dirty)
	{
		database_file_update_needed();
		vac_truncate_clog(newFrozenXid);
	}
}


/*
 *	vac_truncate_clog() -- attempt to truncate the commit log
 *
 *		Scan pg_database to determine the system-wide oldest datfrozenxid,
 *		and use it to truncate the transaction commit log (pg_clog).
 *		Also update the XID wrap limit info maintained by varsup.c.
 *
 *		The passed XID is simply the one I just wrote into my pg_database
 *		entry.	It's used to initialize the "min" calculation.
 *
 *		This routine is shared by full and lazy VACUUM.  Note that it's
 *		only invoked when we've managed to change our DB's datfrozenxid
 *		entry.
 */
static void
vac_truncate_clog(TransactionId frozenXID)
{
	TransactionId myXID = GetCurrentTransactionId();
	Relation	relation;
	HeapScanDesc scan;
	HeapTuple	tuple;
	NameData	oldest_datname;
	bool		frozenAlreadyWrapped = false;

	/* init oldest_datname to sync with my frozenXID */
	namestrcpy(&oldest_datname, get_database_name(MyDatabaseId));

	/*
	 * Scan pg_database to compute the minimum datfrozenxid
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

	scan = heap_beginscan(relation, SnapshotNow, 0, NULL);

	while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Form_pg_database dbform = (Form_pg_database) GETSTRUCT(tuple);

		Assert(TransactionIdIsNormal(dbform->datfrozenxid));

		/*
		 * MPP-20053: Skip databases that cannot be connected to in computing
		 * the oldest database.
		 */
		if (dbform->datallowconn)
		{
			if (TransactionIdPrecedes(myXID, dbform->datfrozenxid))
				frozenAlreadyWrapped = true;
			else if (TransactionIdPrecedes(dbform->datfrozenxid, frozenXID))
			{
				frozenXID = dbform->datfrozenxid;
				namecpy(&oldest_datname, &dbform->datname);
			}
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

	/* Truncate CLOG to the oldest frozenxid */
	TruncateCLOG(frozenXID);
	DistributedLog_Truncate(frozenXID);

	/*
	 * Update the wrap limit for GetNewTransactionId.  Note: this function
	 * will also signal the postmaster for an(other) autovac cycle if needed.
	 */
	SetTransactionIdLimit(frozenXID, &oldest_datname);
}


/****************************************************************************
 *																			*
 *			Code common to both flavors of VACUUM							*
 *																			*
 ****************************************************************************
 */


/*
 *	vacuum_rel() -- vacuum one heap relation
 *
 *		Doing one heap at a time incurs extra overhead, since we need to
 *		check that the heap exists again just before we vacuum it.	The
 *		reason that we do this is so that vacuuming can be spread across
 *		many small transactions.  Otherwise, two-phase locking would require
 *		us to lock the entire database during one pass of the vacuum cleaner.
 */
static void
vacuum_rel(Relation onerel, VacuumStmt *vacstmt, LOCKMODE lmode, List *updated_stats,
		   bool for_wraparound)
{
	Oid			toast_relid;
	Oid			aoseg_relid = InvalidOid;
	Oid         aoblkdir_relid = InvalidOid;
	Oid         aovisimap_relid = InvalidOid;
	Oid			save_userid;
	int			save_sec_context;
	int			save_nestlevel;
	bool		heldoff;

	/*
	 * Check for user-requested abort.	Note we want this to be inside a
	 * transaction, so xact.c doesn't issue useless WARNING.
	 */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Remember the relation's TOAST and AO segments relations for later
	 */
	toast_relid = onerel->rd_rel->reltoastrelid;
	if (RelationIsAoRows(onerel) || RelationIsAoCols(onerel))
	{
		GetAppendOnlyEntryAuxOids(RelationGetRelid(onerel), SnapshotNow,
								  &aoseg_relid,
								  &aoblkdir_relid, NULL,
								  &aovisimap_relid, NULL);
		vacstmt->appendonly_relation_empty =
				AppendOnlyCompaction_IsRelationEmpty(onerel);
	}


	/*
	 * Switch to the table owner's userid, so that any index functions are run
	 * as that user.  Also lock down security-restricted operations and
	 * arrange to make GUC variable changes local to this command.
	 * (This is unnecessary, but harmless, for lazy VACUUM.)
	 */
	GetUserIdAndSecContext(&save_userid, &save_sec_context);
	SetUserIdAndSecContext(onerel->rd_rel->relowner,
						   save_sec_context | SECURITY_RESTRICTED_OPERATION);
	save_nestlevel = NewGUCNestLevel();

	/*
	 * Do the actual work --- either FULL or "lazy" vacuum
	 */
	if (vacstmt->full)
		heldoff = full_vacuum_rel(onerel, vacstmt, updated_stats);
	else
		heldoff = lazy_vacuum_rel(onerel, vacstmt, vac_strategy, updated_stats);

	/* Roll back any GUC changes executed by index functions */
	AtEOXact_GUC(false, save_nestlevel);

	/* Restore userid and security context */
	SetUserIdAndSecContext(save_userid, save_sec_context);

	/*
	 * Complete the transaction and free all temporary memory used.
	 * NOT in GPDB, though! The caller still needs to have the relation open.
	 */
#if 0
	if (vacstmt->full)
		PopActiveSnapshot();
	CommitTransactionCommand();
#endif

	/* now we can allow interrupts again, if disabled */
	if (heldoff)
		RESUME_INTERRUPTS();

	/*
	 * If the relation has a secondary toast rel, vacuum that too while we
	 * still hold the session lock on the master table.  We do this in
	 * cleanup phase when it's AO table or in prepare phase if it's an
	 * empty AO table.
	 */
	if ((RelationIsHeap(onerel) && toast_relid != InvalidOid) ||
		(!RelationIsHeap(onerel) && (
				vacstmt->appendonly_compaction_vacuum_cleanup ||
				vacstmt->appendonly_relation_empty)))
	{
		Relation toast_rel = open_relation_and_check_permission(vacstmt, toast_relid,
																RELKIND_TOASTVALUE, false);
		if (toast_rel != NULL)
		{
			vacuum_rel(toast_rel, vacstmt, lmode, updated_stats, for_wraparound);

			/* all done with this class, but hold lock until commit */
			relation_close(toast_rel, NoLock);
		}
	}

	/*
	 * If an AO/CO table is empty on a segment,
	 * vacstmt->appendonly_relation_empty will get set to true even in the
	 * compaction phase. In such a case, we end up updating the auxiliary
	 * tables and try to vacuum them all in the same transaction. This causes
	 * the auxiliary relation to not get vacuumed and it generates a notice to
	 * the user saying that transaction is already in progress. Hence we want
	 * to vacuum the auxliary relations only in cleanup phase or if we are in
	 * the prepare phase and the AO/CO table is empty.
	 */
	if (vacstmt->appendonly_compaction_vacuum_cleanup ||
		(vacstmt->appendonly_relation_empty && vacstmt->appendonly_compaction_vacuum_prepare))
	{
		/* do the same for an AO segments table, if any */
		if (aoseg_relid != InvalidOid)
		{
			Relation aoseg_rel = open_relation_and_check_permission(vacstmt, aoseg_relid,
																	RELKIND_AOSEGMENTS, false);
			if (aoseg_rel != NULL)
			{
				vacuum_rel(aoseg_rel, vacstmt, lmode, updated_stats, for_wraparound);

				/* all done with this class, but hold lock until commit */
				relation_close(aoseg_rel, NoLock);
			}
		}

		/* do the same for an AO block directory table, if any */
		if (aoblkdir_relid != InvalidOid)
		{
			Relation aoblkdir_rel = open_relation_and_check_permission(vacstmt, aoblkdir_relid,
																	   RELKIND_AOBLOCKDIR, false);
			if (aoblkdir_rel != NULL)
			{
				vacuum_rel(aoblkdir_rel, vacstmt, lmode, updated_stats, for_wraparound);

				/* all done with this class, but hold lock until commit */
				relation_close(aoblkdir_rel, NoLock);
			}
		}

		/* do the same for an AO visimap, if any */
		if (aovisimap_relid != InvalidOid)
		{
			Relation aovisimap_rel = open_relation_and_check_permission(vacstmt, aovisimap_relid,
																	   RELKIND_AOVISIMAP, false);
			if (aovisimap_rel != NULL)
			{
				vacuum_rel(aovisimap_rel, vacstmt, lmode, updated_stats, for_wraparound);

				/* all done with this class, but hold lock until commit */
				relation_close(aovisimap_rel, NoLock);
			}
		}
	}
}


/****************************************************************************
 *																			*
 *			Code for VACUUM FULL (only)										*
 *																			*
 ****************************************************************************
 */

/*
 * Remember the relation stats that will be used in the next truncate phase.
 */
static void
save_vacstats(Oid relid, BlockNumber rel_pages, double rel_tuples, BlockNumber empty_end_pages)
{
	VPgClassStats *stats;

	if (VacFullInitialStatsSize >= MaxVacFullInitialStatsSize)
		elog(ERROR, "out of stats slot");

	stats = &VacFullInitialStats[VacFullInitialStatsSize++];

	stats->relid = relid;
	stats->rel_pages = rel_pages;
	stats->rel_tuples = rel_tuples;
	stats->empty_end_pages = empty_end_pages;

	/* Should not happen */
	if (stats->rel_pages < stats->empty_end_pages)
		elog(ERROR, "rel_pages %u < empty_end_pages %u",
					stats->rel_pages, stats->empty_end_pages);
}

static bool vacuum_appendonly_index_should_vacuum(Relation aoRelation,
		VacuumStmt *vacstmt,
		AppendOnlyIndexVacuumState *vacuumIndexState, double *rel_tuple_count)
{
	int64 hidden_tupcount;
	FileSegTotals *totals;

	Assert(RelationIsAoRows(aoRelation) || RelationIsAoCols(aoRelation));

	if(Gp_role == GP_ROLE_DISPATCH)
	{
		if (rel_tuple_count)
		{
			*rel_tuple_count = 0.0;
		}
		return false;
	}

	if(RelationIsAoRows(aoRelation))
	{
		totals = GetSegFilesTotals(aoRelation, SnapshotNow);
	}
	else
	{
		Assert(RelationIsAoCols(aoRelation));
		totals = GetAOCSSSegFilesTotals(aoRelation, SnapshotNow);
	}
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&vacuumIndexState->visiMap);

	if(rel_tuple_count)
	{
		*rel_tuple_count = (double)(totals->totaltuples - hidden_tupcount);
		Assert((*rel_tuple_count) > -1.0);
	}

	pfree(totals);

	if(hidden_tupcount > 0 || vacstmt->full)
	{
		return true;
	}
	return false;
}

/*
 * vacuum_appendonly_indexes()
 *
 * Perform a vacuum on all indexes of an append-only relation.
 *
 * The page and tuplecount information in vacrelstats are used, the
 * nindex value is set by this function.
 *
 * It returns the number of indexes on the relation.
 */
int
vacuum_appendonly_indexes(Relation aoRelation,
		VacuumStmt *vacstmt,
		List* updated_stats)
{
	int reindex_count = 1;
	int i;
	Relation   *Irel;
	int			nindexes;
	AppendOnlyIndexVacuumState vacuumIndexState;
	FileSegInfo **segmentFileInfo = NULL; /* Might be a casted AOCSFileSegInfo */
	int totalSegfiles;

	Assert(RelationIsAoRows(aoRelation) || RelationIsAoCols(aoRelation));
	Assert(vacstmt);

	memset(&vacuumIndexState, 0, sizeof(vacuumIndexState));

	elogif (Debug_appendonly_print_compaction, LOG,
			"Vacuum indexes for append-only relation %s",
			RelationGetRelationName(aoRelation));

	/* Now open all indexes of the relation */
	if (vacstmt->full)
		vac_open_indexes(aoRelation, AccessExclusiveLock, &nindexes, &Irel);
	else
		vac_open_indexes(aoRelation, RowExclusiveLock, &nindexes, &Irel);

	if (RelationIsAoRows(aoRelation))
	{
		segmentFileInfo = GetAllFileSegInfo(aoRelation, SnapshotNow, &totalSegfiles);
	}
	else
	{
		Assert(RelationIsAoCols(aoRelation));
		segmentFileInfo = (FileSegInfo **)GetAllAOCSFileSegInfo(aoRelation, SnapshotNow, &totalSegfiles);
	}

	AppendOnlyVisimap_Init(
			&vacuumIndexState.visiMap,
			aoRelation->rd_appendonly->visimaprelid,
			aoRelation->rd_appendonly->visimapidxid,
			AccessShareLock,
			SnapshotNow);

	AppendOnlyBlockDirectory_Init_forSearch(&vacuumIndexState.blockDirectory,
			SnapshotNow,
			segmentFileInfo,
			totalSegfiles,
			aoRelation,
			1,
			RelationIsAoCols(aoRelation),
			NULL);

	/* Clean/scan index relation(s) */
	if (Irel != NULL)
	{
		double rel_tuple_count = 0.0;
		if (vacuum_appendonly_index_should_vacuum(aoRelation, vacstmt,
					&vacuumIndexState, &rel_tuple_count))
		{
			Assert(rel_tuple_count > -1.0);

			for (i = 0; i < nindexes; i++)
			{
				vacuum_appendonly_index(Irel[i], &vacuumIndexState, updated_stats,
						rel_tuple_count, vacstmt->full);
			}
			reindex_count++;
		}
		else
		{
			/* just scan indexes to update statistic */
			for (i = 0; i < nindexes; i++)
				scan_index(Irel[i], rel_tuple_count, updated_stats, vacstmt->full, true);
		}
	}

	AppendOnlyVisimap_Finish(&vacuumIndexState.visiMap, AccessShareLock);
	AppendOnlyBlockDirectory_End_forSearch(&vacuumIndexState.blockDirectory);

	if (segmentFileInfo)
	{
		if (RelationIsAoRows(aoRelation))
		{
			FreeAllSegFileInfo(segmentFileInfo, totalSegfiles);
		}
		else
		{
			FreeAllAOCSSegFileInfo((AOCSFileSegInfo **)segmentFileInfo, totalSegfiles);
		}
		pfree(segmentFileInfo);
	}

	vac_close_indexes(nindexes, Irel, NoLock);
	return nindexes;
}

/*
 * vacuum_heap_rel()
 *
 * This is the workhorse of full_vacuum_rel for heap tables.  This is called
 * twice per relation per command.  In the first call, we scan the relation
 * first to identify dead tuples and find free spaces, then clean up indexes
 * and move tuples from end pages to head pages if available.  In the second,
 * vacstmt->truncate is true, and we scan the heap again to verify the empty
 * end pages are still empty, and truncate if so.  In the second transaction,
 * we don't check the number of tuple integrity with indexes.
 */
static bool
vacuum_heap_rel(Relation onerel, VacuumStmt *vacstmt,
		VRelStats *vacrelstats, List *updated_stats)
{
	VacPageListData vacuum_pages;		/* List of pages to vacuum and/or
										 * clean indexes */
	VacPageListData fraged_pages =		/* List of pages with space enough for */
		{								/* re-using */
		0, /* empty_end_pages */
		0, /* num_pages */
		0, /* num_allocated_pages */
		NULL /* pageesc */
		};

	Relation   *Irel;
	int			nindexes;
	int			i;
	bool		heldoff = false;
	int			reindex_count = 1;
	bool		check_stats;
	bool		save_disable_tuple_hints;

	Assert(RelationIsHeap(onerel));

	/*
	 * scan the heap
	 *
	 * repair_frag() assumes that scan_heap() has set all hint bits on the
	 * tuples, so temporarily turn off 'gp_disable_tuple_hints', i.e. allow
	 * hint bits to be set, if we're running in FULL mode.
	 */
	vacuum_pages.num_pages = fraged_pages.num_pages = 0;

	save_disable_tuple_hints = gp_disable_tuple_hints;
	PG_TRY();
	{
		if (vacstmt->full)
			gp_disable_tuple_hints = false;

		if (vacstmt->heap_truncate)
			scan_heap_for_truncate(vacrelstats, onerel, &vacuum_pages);
		else
			scan_heap(vacrelstats, onerel, &vacuum_pages, &fraged_pages);

		gp_disable_tuple_hints = save_disable_tuple_hints;
	}
	PG_CATCH();
	{
		gp_disable_tuple_hints = save_disable_tuple_hints;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* Now open all indexes of the relation */
	vac_open_indexes(onerel, AccessExclusiveLock, &nindexes, &Irel);
	if (nindexes > 0)
		vacrelstats->hasindex = true;

	/*
	 * Since the truncate transaction doesn't read all pages, it may not be
	 * the exact number of tuples.  vacuum_index should not check the
	 * stat consistency.
	 */
	check_stats = !vacstmt->heap_truncate;
	/* Clean/scan index relation(s) */
	if (Irel != NULL)
	{
		if (vacuum_pages.num_pages > 0)
		{
			for (i = 0; i < nindexes; i++)
			{
				vacuum_index(&vacuum_pages, Irel[i],
							 vacrelstats->rel_indexed_tuples, 0, updated_stats,
							 check_stats);
			}
			reindex_count++;
		}
		else if (check_stats)
		{
			/* just scan indexes to update statistic */
			for (i = 0; i < nindexes; i++)
				scan_index(Irel[i], vacrelstats->rel_indexed_tuples, updated_stats, true,
						   check_stats);
		}
	}

	/*
	 * For heap tables FULL vacuum we perform truncate-only transaction as
	 * the second step, after moving tuples across pages if any.  By
	 * separating transactions, we don't loose transactional changes
	 * by non-transactional truncate operation.  Note scan_heap still
	 * performs some xlog operation in non-empty pages, which is ok with
	 * this truncate operation in the same transaction.
	 */
	if (vacstmt->heap_truncate)
	{
		Assert(vacrelstats->rel_pages >= vacuum_pages.empty_end_pages);

		SIMPLE_FAULT_INJECTOR(VacuumFullBeforeTruncate);

		if (vacuum_pages.empty_end_pages > 0)
		{
			BlockNumber relblocks;

			relblocks = vacrelstats->rel_pages - vacuum_pages.empty_end_pages;
			RelationTruncate(onerel, relblocks, true);
			vacrelstats->rel_pages = relblocks;
		}
		vac_close_indexes(nindexes, Irel, NoLock);

		SIMPLE_FAULT_INJECTOR(VacuumFullAfterTruncate);
	}
	else
	{
		if (fraged_pages.num_pages > 0)
		{
			/* Try to shrink heap */
			heldoff = repair_frag(vacrelstats, onerel, &vacuum_pages, &fraged_pages,
								  nindexes, Irel, updated_stats, reindex_count);
			vac_close_indexes(nindexes, Irel, NoLock);
		}
		else
		{
			vac_close_indexes(nindexes, Irel, NoLock);
			if (vacuum_pages.num_pages > 0)
			{
				/* Clean pages from vacuum_pages list */
				vacuum_heap(vacrelstats, onerel, &vacuum_pages);
			}
		}

		/*
		 * Store the relation stats in global array, so that we can
		 * resume the truncate work later.
		 */
		save_vacstats(RelationGetRelid(onerel), vacrelstats->rel_pages,
					  vacrelstats->rel_tuples, vacuum_pages.empty_end_pages);
		/* update shared free space map with final free space info */
		vac_update_fsm(onerel, &fraged_pages, vacrelstats->rel_pages);
	}

	return heldoff;
}

/*
 *	full_vacuum_rel() -- perform FULL VACUUM for one heap relation
 *
 *		This routine vacuums a single heap, cleans out its indexes, and
 *		updates its num_pages and num_tuples statistics.
 *
 *		At entry, we have already established a transaction and opened
 *		and locked the relation.
 *
 *		The return value indicates whether this function has held off
 *		interrupts -- caller must RESUME_INTERRUPTS() after commit if true.
 */
static bool
full_vacuum_rel(Relation onerel, VacuumStmt *vacstmt, List *updated_stats)
{
	VRelStats* vacrelstats;
	bool		heldoff = false;
	bool update_relstats = true;

	vacuum_set_xid_limits(vacstmt->freeze_min_age, onerel->rd_rel->relisshared,
						  &OldestXmin, &FreezeLimit);

	/*
	 * Flush any previous async-commit transactions.  This does not guarantee
	 * that we will be able to set hint bits for tuples they inserted, but it
	 * improves the probability, especially in simple sequential-commands
	 * cases.  See scan_heap() and repair_frag() for more about this.
	 */
	XLogAsyncCommitFlush();

	/*
	 * Set up statistics-gathering machinery.
	 */
	vacrelstats = (VRelStats *) palloc(sizeof(VRelStats));
	vacrelstats->rel_pages = 0;
	vacrelstats->rel_tuples = 0;
	vacrelstats->hasindex = false;

	if(RelationIsAoRows(onerel) || RelationIsAoCols(onerel))
	{
		if(vacuumStatement_IsInAppendOnlyPreparePhase(vacstmt))
		{
			elogif(Debug_appendonly_print_compaction, LOG,
					"Vacuum full prepare phase %s", RelationGetRelationName(onerel));

			vacuum_appendonly_indexes(onerel, vacstmt, updated_stats);
			if (RelationIsAoRows(onerel))
				AppendOnlyTruncateToEOF(onerel);
			else
				AOCSTruncateToEOF(onerel);
			update_relstats = false;
		}
		else if(!vacummStatement_IsInAppendOnlyCleanupPhase(vacstmt))
		{
			vacuum_appendonly_rel(onerel, vacstmt);
			update_relstats = false;
		}
		else
		{
			elogif(Debug_appendonly_print_compaction, LOG,
					"Vacuum full cleanup phase %s", RelationGetRelationName(onerel));
			vacuum_appendonly_fill_stats(onerel, GetActiveSnapshot(),
										 &vacrelstats->rel_pages,
										 &vacrelstats->rel_tuples,
										 &vacrelstats->hasindex);
			/* Reset the remaining VRelStats values */
			vacrelstats->min_tlen = 0;
			vacrelstats->max_tlen = 0;
			vacrelstats->num_vtlinks = 0;
			vacrelstats->vtlinks = NULL;
		}
	}
	else
	{
		/* For heap. */
		heldoff = vacuum_heap_rel(onerel, vacstmt, vacrelstats, updated_stats);
	}

	/* Do not run update the relstats if the vacuuming has been skipped */
	if (update_relstats)
	{
		/* update statistics in pg_class */
		vac_update_relstats_from_list(onerel, vacrelstats->rel_pages,
						vacrelstats->rel_tuples, vacrelstats->hasindex,
						FreezeLimit, updated_stats);

		/* report results to the stats collector, too */
		pgstat_report_vacuum(RelationGetRelid(onerel), onerel->rd_rel->relisshared, true,
						 vacstmt->analyze, vacrelstats->rel_tuples);
	}

	pfree(vacrelstats);

	return heldoff;
}

/*
 * This is a small version of scan_heap, performed in the second transaction of
 * heap vacuum full.  We assume we did the first transaction and kept some of
 * the stats information already, so start from the last known truncate point,
 * and rescan to the end to see if they are still empty.  Note someone might
 * have already modified these pages before we come back from QD, in case of
 * catalog table, because concurrent DDL can go in QE even if QD is holding
 * an exclusive lock on the catalog table, and QE just releases locks between
 * separate transactions.
 *
 * We don't touch other pages than the ones that are potentially truncated.
 * Note index may also have such tuples that are inserted after the first
 * transaction, and it'd not be easy to clean them up all.  Here we just
 * focus on truncate.  We skip checking stats in scan_index or vacuum_index,
 * as our reltuples may not be exactly correct.
 */
static void
scan_heap_for_truncate(VRelStats *vacrelstats, Relation onerel,
					   VacPageList vacuum_pages)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	BlockNumber nblocks, blkno;
	char	   *relname;
	VacPage		vacpage;
	BlockNumber empty_end_pages;
	double		num_tuples;
	bool		do_shrinking = true;
	int			i;
	VPgClassStats *prev_stats = NULL;

	relname = RelationGetRelationName(onerel);

	empty_end_pages = 0;
	num_tuples = 0;

	nblocks = RelationGetNumberOfBlocks(onerel);

	vacpage = (VacPage) palloc(sizeof(VacPageData) + MaxOffsetNumber * sizeof(OffsetNumber));

	/* Fetch gp_persistent_relation_node information for XLOG. */
	RelationFetchGpRelationNodeForXLog(onerel);

	/* Retrieve the relation stats info from the previous transaction. */
	for (i = 0; i < VacFullInitialStatsSize; i++)
	{
		VPgClassStats *stats = &VacFullInitialStats[i];
		if (stats->relid == RelationGetRelid(onerel))
		{
			prev_stats = stats;
			break;
		}
	}
	if (prev_stats == NULL)
		elog(ERROR, "could not find previous vacuum infomation for %s", relname);

	Assert(prev_stats->rel_pages >= prev_stats->empty_end_pages);
	blkno = prev_stats->rel_pages - prev_stats->empty_end_pages;
	for (; blkno < nblocks; blkno++)
	{
		Buffer		buf;
		Page		page;
		OffsetNumber offnum, maxoff;
		VacPage		vacpagecopy;
		bool		notup = true;

		vacuum_delay_point();

		// -------- MirroredLock ----------
		MIRROREDLOCK_BUFMGR_LOCK;

		buf = ReadBufferWithStrategy(onerel, blkno, vac_strategy);
		page = BufferGetPage(buf);

		LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

		vacpage->blkno = blkno;
		vacpage->offsets_used = 0;
		vacpage->offsets_free = 0;

		/*
		 * If the page is empty, just remember it and delete index pointers
		 * later if there are any tuples pointing to this page.
		 */
		if (PageIsNew(page) || PageIsEmpty(page))
		{
			empty_end_pages++;
			vacpagecopy = copy_vac_page(vacpage);
			vpage_insert(vacuum_pages, vacpagecopy);
			UnlockReleaseBuffer(buf);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

			continue;
		}

		maxoff = PageGetMaxOffsetNumber(page);
		for (offnum = FirstOffsetNumber;
			 offnum <= maxoff;
			 offnum = OffsetNumberNext(offnum))
		{
			ItemId		itemid = PageGetItemId(page, offnum);
			HeapTupleData	tuple;
			bool			tupgone = false;

			if (!ItemIdIsUsed(itemid))
				continue;

			if (ItemIdIsDead(itemid))
				continue;

			tuple.t_data = (HeapTupleHeader) PageGetItem(page, itemid);
			tuple.t_len = ItemIdGetLength(itemid);
			ItemPointerSet(&(tuple.t_self), blkno, offnum);

			switch (HeapTupleSatisfiesVacuum(onerel, tuple.t_data, OldestXmin, buf))
			{
				case HEAPTUPLE_DEAD:
					tupgone = true;
					break;
				case HEAPTUPLE_LIVE:
				case HEAPTUPLE_RECENTLY_DEAD:
					break;
				case HEAPTUPLE_INSERT_IN_PROGRESS:
					ereport(LOG,
							(errmsg("relation \"%s\" TID %u/%u: InsertTransactionInProgress %u --- can't shrink relation",
									relname, blkno, offnum, HeapTupleHeaderGetXmin(tuple.t_data))));
					do_shrinking = false;
					break;
				case HEAPTUPLE_DELETE_IN_PROGRESS:
					ereport(LOG,
							(errmsg("relation \"%s\" TID %u/%u: DeleteTransactionInProgress %u --- can't shrink relation",
									relname, blkno, offnum, HeapTupleHeaderGetXmax(tuple.t_data))));
					do_shrinking = false;
					break;

				default:
					elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
					break;
			}

			if (!tupgone)
			{
				num_tuples += 1;
				notup = false;
			}
		}

		if (notup)
		{
			empty_end_pages++;
			vacpagecopy = copy_vac_page(vacpage);
			vpage_insert(vacuum_pages, vacpagecopy);
		}
		else
		{
			/*
			 * If we are seeing live tuples in those pages that should have
			 * been truncated in the previous transaction, someone already
			 * modified them.  In that case it's safer to not truncate
			 * at all.
			 */
			do_shrinking = false;
			empty_end_pages = 0;
		}

		UnlockReleaseBuffer(buf);

		MIRROREDLOCK_BUFMGR_UNLOCK;
		// -------- MirroredLock ----------
	}

	pfree(vacpage);

	vacrelstats->rel_tuples = prev_stats->rel_tuples + num_tuples;
	vacrelstats->rel_pages = nblocks;
	if (!do_shrinking)
	{
		int		i;

		vacuum_pages->empty_end_pages = 0;
		for (i = 0; i < vacuum_pages->num_pages; i++)
			pfree(vacuum_pages->pagedesc[i]);
		vacuum_pages->num_pages = 0;
	}
	else
	{
		vacuum_pages->empty_end_pages = empty_end_pages;
	}
}


/*
 *	scan_heap() -- scan an open heap relation
 *
 *		This routine sets commit status bits, constructs vacuum_pages (list
 *		of pages we need to compact free space on and/or clean indexes of
 *		deleted tuples), constructs fraged_pages (list of pages with free
 *		space that tuples could be moved into), and calculates statistics
 *		on the number of live tuples in the heap.
 */
static void
scan_heap(VRelStats *vacrelstats, Relation onerel,
		  VacPageList vacuum_pages, VacPageList fraged_pages)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	BlockNumber nblocks,
				blkno;
	char	   *relname;
	VacPage		vacpage;
	BlockNumber empty_pages,
				empty_end_pages;
	double		num_tuples,
				num_indexed_tuples,
				tups_vacuumed,
				nkeep,
				nunused;
	double		free_space,
				usable_free_space;
	Size		min_tlen = MaxHeapTupleSize;
	Size		max_tlen = 0;
	bool		do_shrinking = true;
	VTupleLink	vtlinks = (VTupleLink) palloc(100 * sizeof(VTupleLinkData));
	int			num_vtlinks = 0;
	int			free_vtlinks = 100;
	PGRUsage	ru0;

	pg_rusage_init(&ru0);

	relname = RelationGetRelationName(onerel);
	ereport(elevel,
			(errmsg("vacuuming \"%s.%s\"",
					get_namespace_name(RelationGetNamespace(onerel)),
					relname)));

	empty_pages = empty_end_pages = 0;
	num_tuples = num_indexed_tuples = tups_vacuumed = nkeep = nunused = 0;
	free_space = 0;

	nblocks = RelationGetNumberOfBlocks(onerel);

	/*
	 * We initially create each VacPage item in a maximal-sized workspace,
	 * then copy the workspace into a just-large-enough copy.
	 */
	vacpage = (VacPage) palloc(sizeof(VacPageData) + MaxOffsetNumber * sizeof(OffsetNumber));

	/* Fetch gp_persistent_relation_node information for XLOG. */
	RelationFetchGpRelationNodeForXLog(onerel);

	for (blkno = 0; blkno < nblocks; blkno++)
	{
		Page		page,
					tempPage = NULL;
		bool		do_reap,
					do_frag;
		Buffer		buf;
		OffsetNumber offnum,
					maxoff;
		bool		notup;
		OffsetNumber frozen[MaxOffsetNumber];
		int			nfrozen;

		vacuum_delay_point();

		// -------- MirroredLock ----------
		MIRROREDLOCK_BUFMGR_LOCK;

		buf = ReadBufferWithStrategy(onerel, blkno, vac_strategy);
		page = BufferGetPage(buf);

		/*
		 * Since we are holding exclusive lock on the relation, no other
		 * backend can be accessing the page; however it is possible that the
		 * background writer will try to write the page if it's already marked
		 * dirty.  To ensure that invalid data doesn't get written to disk, we
		 * must take exclusive buffer lock wherever we potentially modify
		 * pages.  In fact, we insist on cleanup lock so that we can safely
		 * call heap_page_prune().	(This might be overkill, since the
		 * bgwriter pays no attention to individual tuples, but on the other
		 * hand it's unlikely that the bgwriter has this particular page
		 * pinned at this instant.	So violating the coding rule would buy us
		 * little anyway.)
		 */
		LockBufferForCleanup(buf);

		vacpage->blkno = blkno;
		vacpage->offsets_used = 0;
		vacpage->offsets_free = 0;

		if (PageIsNew(page))
		{
			VacPage		vacpagecopy;

			ereport(WARNING,
			   (errmsg("relation \"%s\" page %u is uninitialized --- fixing",
					   relname, blkno)));
			PageInit(page, BufferGetPageSize(buf), 0);
			MarkBufferDirty(buf);
			vacpage->free = PageGetFreeSpaceWithFillFactor(onerel, page);
			free_space += vacpage->free;
			empty_pages++;
			empty_end_pages++;
			vacpagecopy = copy_vac_page(vacpage);
			vpage_insert(vacuum_pages, vacpagecopy);
			vpage_insert(fraged_pages, vacpagecopy);
			UnlockReleaseBuffer(buf);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

			continue;
		}

		if (PageIsEmpty(page))
		{
			VacPage		vacpagecopy;

			vacpage->free = PageGetFreeSpaceWithFillFactor(onerel, page);
			free_space += vacpage->free;
			empty_pages++;
			empty_end_pages++;
			vacpagecopy = copy_vac_page(vacpage);
			vpage_insert(vacuum_pages, vacpagecopy);
			vpage_insert(fraged_pages, vacpagecopy);
			UnlockReleaseBuffer(buf);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

			continue;
		}

		/*
		 * Prune all HOT-update chains in this page.
		 *
		 * We use the redirect_move option so that redirecting line pointers
		 * get collapsed out; this allows us to not worry about them below.
		 *
		 * We count tuples removed by the pruning step as removed by VACUUM.
		 */
		tups_vacuumed += heap_page_prune(onerel, buf, OldestXmin,
										 true, false);

		/*
		 * Now scan the page to collect vacuumable items and check for tuples
		 * requiring freezing.
		 */
		nfrozen = 0;
		notup = true;
		maxoff = PageGetMaxOffsetNumber(page);
		for (offnum = FirstOffsetNumber;
			 offnum <= maxoff;
			 offnum = OffsetNumberNext(offnum))
		{
			ItemId		itemid = PageGetItemId(page, offnum);
			bool		tupgone = false;
			HeapTupleData tuple;

			/*
			 * Collect un-used items too - it's possible to have indexes
			 * pointing here after crash.  (That's an ancient comment and is
			 * likely obsolete with WAL, but we might as well continue to
			 * check for such problems.)
			 */
			if (!ItemIdIsUsed(itemid))
			{
				vacpage->offsets[vacpage->offsets_free++] = offnum;
				nunused += 1;
				continue;
			}

			/*
			 * DEAD item pointers are to be vacuumed normally; but we don't
			 * count them in tups_vacuumed, else we'd be double-counting (at
			 * least in the common case where heap_page_prune() just freed up
			 * a non-HOT tuple).
			 */
			if (ItemIdIsDead(itemid))
			{
				vacpage->offsets[vacpage->offsets_free++] = offnum;
				continue;
			}

			/* Shouldn't have any redirected items anymore */
			if (!ItemIdIsNormal(itemid))
				elog(ERROR, "relation \"%s\" TID %u/%u: unexpected redirect item",
					 relname, blkno, offnum);

			tuple.t_data = (HeapTupleHeader) PageGetItem(page, itemid);
			tuple.t_len = ItemIdGetLength(itemid);
			ItemPointerSet(&(tuple.t_self), blkno, offnum);

			switch (HeapTupleSatisfiesVacuum(onerel, tuple.t_data, OldestXmin, buf))
			{
				case HEAPTUPLE_LIVE:
					/* Tuple is good --- but let's do some validity checks */
					if (onerel->rd_rel->relhasoids &&
						!OidIsValid(HeapTupleGetOid(&tuple)))
						elog(WARNING, "relation \"%s\" TID %u/%u: OID is invalid",
							 relname, blkno, offnum);

					/*
					 * The shrinkage phase of VACUUM FULL requires that all
					 * live tuples have XMIN_COMMITTED set --- see comments in
					 * repair_frag()'s walk-along-page loop.  Use of async
					 * commit may prevent HeapTupleSatisfiesVacuum from
					 * setting the bit for a recently committed tuple.	Rather
					 * than trying to handle this corner case, we just give up
					 * and don't shrink.
					 */
					if (do_shrinking &&
						!(tuple.t_data->t_infomask & HEAP_XMIN_COMMITTED))
					{
						ereport(LOG,
								(errmsg("relation \"%s\" TID %u/%u: XMIN_COMMITTED not set for transaction %u --- cannot shrink relation",
										relname, blkno, offnum,
									 HeapTupleHeaderGetXmin(tuple.t_data))));
						do_shrinking = false;
					}
					break;
				case HEAPTUPLE_DEAD:

					/*
					 * Ordinarily, DEAD tuples would have been removed by
					 * heap_page_prune(), but it's possible that the tuple
					 * state changed since heap_page_prune() looked.  In
					 * particular an INSERT_IN_PROGRESS tuple could have
					 * changed to DEAD if the inserter aborted.  So this
					 * cannot be considered an error condition, though it does
					 * suggest that someone released a lock early.
					 *
					 * If the tuple is HOT-updated then it must only be
					 * removed by a prune operation; so we keep it as if it
					 * were RECENTLY_DEAD, and abandon shrinking. (XXX is it
					 * worth trying to make the shrinking code smart enough to
					 * handle this?  It's an unusual corner case.)
					 *
					 * DEAD heap-only tuples can safely be removed if they
					 * aren't themselves HOT-updated, although this is a bit
					 * inefficient since we'll uselessly try to remove index
					 * entries for them.
					 */
					if (HeapTupleIsHotUpdated(&tuple))
					{
						nkeep += 1;
						if (do_shrinking)
							ereport(LOG,
									(errmsg("relation \"%s\" TID %u/%u: dead HOT-updated tuple --- cannot shrink relation",
											relname, blkno, offnum)));
						do_shrinking = false;
					}
					else
					{
						tupgone = true; /* we can delete the tuple */

						/*
						 * We need not require XMIN_COMMITTED or
						 * XMAX_COMMITTED to be set, since we will remove the
						 * tuple without any further examination of its hint
						 * bits.
						 */
					}
					break;
				case HEAPTUPLE_RECENTLY_DEAD:

					/*
					 * If tuple is recently deleted then we must not remove it
					 * from relation.
					 */
					nkeep += 1;

					/*
					 * As with the LIVE case, shrinkage requires
					 * XMIN_COMMITTED to be set.
					 */
					if (do_shrinking &&
						!(tuple.t_data->t_infomask & HEAP_XMIN_COMMITTED))
					{
						ereport(LOG,
								(errmsg("relation \"%s\" TID %u/%u: XMIN_COMMITTED not set for transaction %u --- cannot shrink relation",
										relname, blkno, offnum,
									 HeapTupleHeaderGetXmin(tuple.t_data))));
						do_shrinking = false;
					}

					/*
					 * If we do shrinking and this tuple is updated one then
					 * remember it to construct updated tuple dependencies.
					 */
					if (do_shrinking &&
						!(ItemPointerEquals(&(tuple.t_self),
											&(tuple.t_data->t_ctid))))
					{
						if (free_vtlinks == 0)
						{
							free_vtlinks = 1000;
							vtlinks = (VTupleLink) repalloc(vtlinks,
											   (free_vtlinks + num_vtlinks) *
													 sizeof(VTupleLinkData));
						}
						vtlinks[num_vtlinks].new_tid = tuple.t_data->t_ctid;
						vtlinks[num_vtlinks].this_tid = tuple.t_self;
						free_vtlinks--;
						num_vtlinks++;
					}
					break;
				case HEAPTUPLE_INSERT_IN_PROGRESS:

					/*
					 * This should not happen, since we hold exclusive lock on
					 * the relation; shouldn't we raise an error?  (Actually,
					 * it can happen in system catalogs, since we tend to
					 * release write lock before commit there.)  As above, we
					 * can't apply repair_frag() if the tuple state is
					 * uncertain.
					 */
					if (do_shrinking)
						ereport(LOG,
								(errmsg("relation \"%s\" TID %u/%u: InsertTransactionInProgress %u --- cannot shrink relation",
										relname, blkno, offnum,
									 HeapTupleHeaderGetXmin(tuple.t_data))));
					do_shrinking = false;
					break;
				case HEAPTUPLE_DELETE_IN_PROGRESS:

					/*
					 * This should not happen, since we hold exclusive lock on
					 * the relation; shouldn't we raise an error?  (Actually,
					 * it can happen in system catalogs, since we tend to
					 * release write lock before commit there.)  As above, we
					 * can't apply repair_frag() if the tuple state is
					 * uncertain.
					 */
					if (do_shrinking)
						ereport(LOG,
								(errmsg("relation \"%s\" TID %u/%u: DeleteTransactionInProgress %u --- cannot shrink relation",
										relname, blkno, offnum,
									 HeapTupleHeaderGetXmax(tuple.t_data))));
					do_shrinking = false;
					break;
				default:
					elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
					break;
			}

			if (tupgone)
			{
				ItemId		lpp;

				/*
				 * Here we are building a temporary copy of the page with dead
				 * tuples removed.	Below we will apply
				 * PageRepairFragmentation to the copy, so that we can
				 * determine how much space will be available after removal of
				 * dead tuples.  But note we are NOT changing the real page
				 * yet...
				 */
				if (tempPage == NULL)
				{
					Size		pageSize;

					pageSize = PageGetPageSize(page);
					tempPage = (Page) palloc(pageSize);
					memcpy(tempPage, page, pageSize);
				}

				/* mark it unused on the temp page */
				lpp = PageGetItemId(tempPage, offnum);
				ItemIdSetUnused(lpp);

				vacpage->offsets[vacpage->offsets_free++] = offnum;
				tups_vacuumed += 1;
			}
			else
			{
				num_tuples += 1;
				if (!HeapTupleIsHeapOnly(&tuple))
					num_indexed_tuples += 1;
				notup = false;
				if (tuple.t_len < min_tlen)
					min_tlen = tuple.t_len;
				if (tuple.t_len > max_tlen)
					max_tlen = tuple.t_len;

				/*
				 * Each non-removable tuple must be checked to see if it needs
				 * freezing.
				 */
				if (heap_freeze_tuple(tuple.t_data, &FreezeLimit,
									  InvalidBuffer, false))
					frozen[nfrozen++] = offnum;
			}
		}						/* scan along page */

		if (tempPage != NULL)
		{
			/* Some tuples are removable; figure free space after removal */
			PageRepairFragmentation(tempPage);
			vacpage->free = PageGetFreeSpaceWithFillFactor(onerel, tempPage);
			pfree(tempPage);
			do_reap = true;
		}
		else
		{
			/* Just use current available space */
			vacpage->free = PageGetFreeSpaceWithFillFactor(onerel, page);
			/* Need to reap the page if it has UNUSED or DEAD line pointers */
			do_reap = (vacpage->offsets_free > 0);
		}

		free_space += vacpage->free;

		/*
		 * Add the page to vacuum_pages if it requires reaping, and add it to
		 * fraged_pages if it has a useful amount of free space.  "Useful"
		 * means enough for a minimal-sized tuple.  But we don't know that
		 * accurately near the start of the relation, so add pages
		 * unconditionally if they have >= BLCKSZ/10 free space.  Also
		 * forcibly add pages with no live tuples, to avoid confusing the
		 * empty_end_pages logic.  (In the presence of unreasonably small
		 * fillfactor, it seems possible that such pages might not pass
		 * the free-space test, but they had better be in the list anyway.)
		 */
		do_frag = (vacpage->free >= min_tlen || vacpage->free >= BLCKSZ / 10 ||
				   notup);

		if (do_reap || do_frag)
		{
			VacPage		vacpagecopy = copy_vac_page(vacpage);

			if (do_reap)
				vpage_insert(vacuum_pages, vacpagecopy);
			if (do_frag)
				vpage_insert(fraged_pages, vacpagecopy);
		}

		/*
		 * Include the page in empty_end_pages if it will be empty after
		 * vacuuming; this is to keep us from using it as a move destination.
		 * Note that such pages are guaranteed to be in fraged_pages.
		 */
		if (notup)
		{
			empty_pages++;
			empty_end_pages++;
		}
		else
			empty_end_pages = 0;

		/*
		 * If we froze any tuples, mark the buffer dirty, and write a WAL
		 * record recording the changes.  We must log the changes to be
		 * crash-safe against future truncation of CLOG.
		 */
		if (nfrozen > 0)
		{
			MarkBufferDirty(buf);
			/* no XLOG for temp tables, though */
			if (!onerel->rd_istemp)
			{
				XLogRecPtr	recptr;

				recptr = log_heap_freeze(onerel, buf, FreezeLimit,
										 frozen, nfrozen);
				PageSetLSN(page, recptr);
			}
		}

		UnlockReleaseBuffer(buf);

		MIRROREDLOCK_BUFMGR_UNLOCK;
		// -------- MirroredLock ----------

	}

	pfree(vacpage);

	/* save stats in the rel list for use later */
	vacrelstats->rel_tuples = num_tuples;
	vacrelstats->rel_indexed_tuples = num_indexed_tuples;
	vacrelstats->rel_pages = nblocks;
	if (num_tuples == 0)
		min_tlen = max_tlen = 0;
	vacrelstats->min_tlen = min_tlen;
	vacrelstats->max_tlen = max_tlen;

	vacuum_pages->empty_end_pages = empty_end_pages;
	fraged_pages->empty_end_pages = empty_end_pages;

	/*
	 * Clear the fraged_pages list if we found we couldn't shrink. Else,
	 * remove any "empty" end-pages from the list, and compute usable free
	 * space = free space in remaining pages.
	 */
	if (do_shrinking)
	{
		int			i;

		Assert((BlockNumber) fraged_pages->num_pages >= empty_end_pages);
		fraged_pages->num_pages -= empty_end_pages;
		usable_free_space = 0;
		for (i = 0; i < fraged_pages->num_pages; i++)
			usable_free_space += fraged_pages->pagedesc[i]->free;
	}
	else
	{
		fraged_pages->num_pages = 0;
		usable_free_space = 0;
	}

	/* don't bother to save vtlinks if we will not call repair_frag */
	if (fraged_pages->num_pages > 0 && num_vtlinks > 0)
	{
		qsort((char *) vtlinks, num_vtlinks, sizeof(VTupleLinkData),
			  vac_cmp_vtlinks);
		vacrelstats->vtlinks = vtlinks;
		vacrelstats->num_vtlinks = num_vtlinks;
	}
	else
	{
		vacrelstats->vtlinks = NULL;
		vacrelstats->num_vtlinks = 0;
		pfree(vtlinks);
	}

	ereport(elevel,
			(errmsg("\"%s\": found %.0f removable, %.0f nonremovable row versions in %u pages",
					RelationGetRelationName(onerel),
					tups_vacuumed, num_tuples, nblocks),
			 errdetail("%.0f dead row versions cannot be removed yet.\n"
			  "Nonremovable row versions range from %lu to %lu bytes long.\n"
					   "There were %.0f unused item pointers.\n"
	   "Total free space (including removable row versions) is %.0f bytes.\n"
					   "%u pages are or will become empty, including %u at the end of the table.\n"
	 "%u pages containing %.0f free bytes are potential move destinations.\n"
					   "%s.",
					   nkeep,
					   (unsigned long) min_tlen, (unsigned long) max_tlen,
					   nunused,
					   free_space,
					   empty_pages, empty_end_pages,
					   fraged_pages->num_pages, usable_free_space,
					   pg_rusage_show(&ru0))));
}


/*
 *	repair_frag() -- try to repair relation's fragmentation
 *
 *		This routine marks dead tuples as unused and tries re-use dead space
 *		by moving tuples (and inserting indexes if needed). It constructs
 *		Nvacpagelist list of free-ed pages (moved tuples) and clean indexes
 *		for them after committing (in hack-manner - without losing locks
 *		and freeing memory!) current transaction. It truncates relation
 *		if some end-blocks are gone away.
 *
 *		The return value indicates whether this function has held off
 *		interrupts -- caller must RESUME_INTERRUPTS() after commit if true.
 */
static bool
repair_frag(VRelStats *vacrelstats, Relation onerel,
			VacPageList vacuum_pages, VacPageList fraged_pages,
			int nindexes, Relation *Irel, List *updated_stats,
			int reindex_count)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	TransactionId myXID = GetCurrentTransactionId();
	Buffer		dst_buffer = InvalidBuffer;
	BlockNumber nblocks,
				blkno;
	BlockNumber last_move_dest_block = 0,
				last_vacuum_block;
	Page		dst_page = NULL;
	ExecContextData ec;
	VacPageListData Nvacpagelist = {0, 0, 0, NULL};
	VacPage		dst_vacpage = NULL,
				last_vacuum_page,
				vacpage,
			   *curpage;
	int			i;
	int			num_moved = 0,
				num_fraged_pages,
				vacuumed_pages;
	int			keep_tuples = 0;
	int			keep_indexed_tuples = 0;
	PGRUsage	ru0;
	bool		heldoff = false;

	pg_rusage_init(&ru0);

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(onerel);

	ExecContext_Init(&ec, onerel);

	Nvacpagelist.num_pages = 0;
	num_fraged_pages = fraged_pages->num_pages;
	Assert((BlockNumber) vacuum_pages->num_pages >= vacuum_pages->empty_end_pages);
	vacuumed_pages = vacuum_pages->num_pages - vacuum_pages->empty_end_pages;
	if (vacuumed_pages > 0)
	{
		/* get last reaped page from vacuum_pages */
		last_vacuum_page = vacuum_pages->pagedesc[vacuumed_pages - 1];
		last_vacuum_block = last_vacuum_page->blkno;
	}
	else
	{
		last_vacuum_page = NULL;
		last_vacuum_block = InvalidBlockNumber;
	}

	vacpage = (VacPage) palloc(sizeof(VacPageData) + MaxOffsetNumber * sizeof(OffsetNumber));
	vacpage->offsets_used = vacpage->offsets_free = 0;

	/*
	 * Scan pages backwards from the last nonempty page, trying to move tuples
	 * down to lower pages.  Quit when we reach a page that we have moved any
	 * tuples onto, or the first page if we haven't moved anything, or when we
	 * find a page we cannot completely empty (this last condition is handled
	 * by "break" statements within the loop).
	 *
	 * NB: this code depends on the vacuum_pages and fraged_pages lists being
	 * in order by blkno.
	 */
	nblocks = vacrelstats->rel_pages;
	for (blkno = nblocks - vacuum_pages->empty_end_pages - 1;
		 blkno > last_move_dest_block;
		 blkno--)
	{
		Buffer		buf;
		Page		page;
		OffsetNumber offnum,
					maxoff;
		bool		isempty,
					chain_tuple_moved;

		vacuum_delay_point();

		/*
		 * Forget fraged_pages pages at or after this one; they're no longer
		 * useful as move targets, since we only want to move down. Note that
		 * since we stop the outer loop at last_move_dest_block, pages removed
		 * here cannot have had anything moved onto them already.
		 *
		 * Also note that we don't change the stored fraged_pages list, only
		 * our local variable num_fraged_pages; so the forgotten pages are
		 * still available to be loaded into the free space map later.
		 */
		while (num_fraged_pages > 0 &&
			   fraged_pages->pagedesc[num_fraged_pages - 1]->blkno >= blkno)
		{
			Assert(fraged_pages->pagedesc[num_fraged_pages - 1]->offsets_used == 0);
			--num_fraged_pages;
		}

		/*
		 * Process this page of relation.
		 */

		// -------- MirroredLock ----------
		MIRROREDLOCK_BUFMGR_LOCK;

		buf = ReadBufferWithStrategy(onerel, blkno, vac_strategy);
		page = BufferGetPage(buf);

		vacpage->offsets_free = 0;

		isempty = PageIsEmpty(page);

		/* Is the page in the vacuum_pages list? */
		if (blkno == last_vacuum_block)
		{
			if (last_vacuum_page->offsets_free > 0)
			{
				/* there are dead tuples on this page - clean them */
				Assert(!isempty);
				LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
				vacuum_page(onerel, buf, last_vacuum_page);
				LockBuffer(buf, BUFFER_LOCK_UNLOCK);
			}
			else
				Assert(isempty);
			--vacuumed_pages;
			if (vacuumed_pages > 0)
			{
				/* get prev reaped page from vacuum_pages */
				last_vacuum_page = vacuum_pages->pagedesc[vacuumed_pages - 1];
				last_vacuum_block = last_vacuum_page->blkno;
			}
			else
			{
				last_vacuum_page = NULL;
				last_vacuum_block = InvalidBlockNumber;
			}
			if (isempty)
			{

				MIRROREDLOCK_BUFMGR_UNLOCK;
				// -------- MirroredLock ----------

				ReleaseBuffer(buf);
				continue;
			}
		}
		else
			Assert(!isempty);

		chain_tuple_moved = false;		/* no one chain-tuple was moved off
										 * this page, yet */
		vacpage->blkno = blkno;
		maxoff = PageGetMaxOffsetNumber(page);
		for (offnum = FirstOffsetNumber;
			 offnum <= maxoff;
			 offnum = OffsetNumberNext(offnum))
		{
			Size		tuple_len;
			HeapTupleData tuple;
			ItemId		itemid = PageGetItemId(page, offnum);

			if (!ItemIdIsUsed(itemid))
				continue;

			if (ItemIdIsDead(itemid))
			{
				/* just remember it for vacuum_page() */
				vacpage->offsets[vacpage->offsets_free++] = offnum;
				continue;
			}

			/* Shouldn't have any redirected items now */
			Assert(ItemIdIsNormal(itemid));

			tuple.t_data = (HeapTupleHeader) PageGetItem(page, itemid);
			tuple_len = tuple.t_len = ItemIdGetLength(itemid);
			ItemPointerSet(&(tuple.t_self), blkno, offnum);

			/* ---
			 * VACUUM FULL has an exclusive lock on the relation.  So
			 * normally no other transaction can have pending INSERTs or
			 * DELETEs in this relation.  A tuple is either:
			 *		(a) live (XMIN_COMMITTED)
			 *		(b) known dead (XMIN_INVALID, or XMAX_COMMITTED and xmax
			 *			is visible to all active transactions)
			 *		(c) inserted and deleted (XMIN_COMMITTED+XMAX_COMMITTED)
			 *			but at least one active transaction does not see the
			 *			deleting transaction (ie, it's RECENTLY_DEAD)
			 *		(d) moved by the currently running VACUUM
			 *		(e) inserted or deleted by a not yet committed transaction,
			 *			or by a transaction we couldn't set XMIN_COMMITTED for.
			 * In case (e) we wouldn't be in repair_frag() at all, because
			 * scan_heap() detects those cases and shuts off shrinking.
			 * We can't see case (b) here either, because such tuples were
			 * already removed by vacuum_page().  Cases (a) and (c) are
			 * normal and will have XMIN_COMMITTED set.  Case (d) is only
			 * possible if a whole tuple chain has been moved while
			 * processing this or a higher numbered block.
			 * ---
			 */

			/*
			 * In PostgreSQL, we assume that the first pass of vacuum already
			 * set the hint bit. However, we cannot rely on that in GPDB,
			 * because of gp_disable_tuple_hints GUC. If it's ever set, then
			 * the first pass might've seen that all the hint bits on the page
			 * were already set, but the backend that set those bits didn't
			 * mark the buffer as dirty. If the buffer is subsequently evicted
			 * from the buffer cache, the hint bit updates are lost, and we
			 * will see them as not set here, even though they were set in the
			 * first pass.
			 *
			 * To fix that, just call HeapTupleSatisfiesVacuum() here to set
			 * the hint bits again, if not set already.
			 */
			LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
			(void) HeapTupleSatisfiesVacuum(onerel, tuple.t_data, OldestXmin, buf);
			LockBuffer(buf, BUFFER_LOCK_UNLOCK);

			if (!(tuple.t_data->t_infomask & HEAP_XMIN_COMMITTED))
			{
				if (tuple.t_data->t_infomask & HEAP_MOVED_IN)
					elog(ERROR, "HEAP_MOVED_IN was not expected");
				if (!(tuple.t_data->t_infomask & HEAP_MOVED_OFF))
					elog(ERROR, "HEAP_MOVED_OFF was expected");

				/*
				 * MOVED_OFF by another VACUUM would have caused the
				 * visibility check to set XMIN_COMMITTED or XMIN_INVALID.
				 */
				if (HeapTupleHeaderGetXvac(tuple.t_data) != myXID)
					elog(ERROR, "invalid XVAC in tuple header");

				/*
				 * If this (chain) tuple is moved by me already then I have to
				 * check is it in vacpage or not - i.e. is it moved while
				 * cleaning this page or some previous one.
				 */

				/* Can't we Assert(keep_tuples > 0) here? */
				if (keep_tuples == 0)
					continue;
				if (chain_tuple_moved)
				{
					/* some chains were moved while cleaning this page */
					Assert(vacpage->offsets_free > 0);
					for (i = 0; i < vacpage->offsets_free; i++)
					{
						if (vacpage->offsets[i] == offnum)
							break;
					}
					if (i >= vacpage->offsets_free)		/* not found */
					{
						vacpage->offsets[vacpage->offsets_free++] = offnum;

						/*
						 * If this is not a heap-only tuple, there must be an
						 * index entry for this item which will be removed in
						 * the index cleanup. Decrement the
						 * keep_indexed_tuples count to remember this.
						 */
						if (!HeapTupleHeaderIsHeapOnly(tuple.t_data))
							keep_indexed_tuples--;
						keep_tuples--;
					}
				}
				else
				{
					vacpage->offsets[vacpage->offsets_free++] = offnum;

					/*
					 * If this is not a heap-only tuple, there must be an
					 * index entry for this item which will be removed in the
					 * index cleanup. Decrement the keep_indexed_tuples count
					 * to remember this.
					 */
					if (!HeapTupleHeaderIsHeapOnly(tuple.t_data))
						keep_indexed_tuples--;
					keep_tuples--;
				}
				continue;
			}

			/*
			 * If this tuple is in a chain of tuples created in updates by
			 * "recent" transactions then we have to move the whole chain of
			 * tuples to other places, so that we can write new t_ctid links
			 * that preserve the chain relationship.
			 *
			 * This test is complicated.  Read it as "if tuple is a recently
			 * created updated version, OR if it is an obsoleted version". (In
			 * the second half of the test, we needn't make any check on XMAX
			 * --- it must be recently obsoleted, else scan_heap would have
			 * deemed it removable.)
			 *
			 * NOTE: this test is not 100% accurate: it is possible for a
			 * tuple to be an updated one with recent xmin, and yet not match
			 * any new_tid entry in the vtlinks list.  Presumably there was
			 * once a parent tuple with xmax matching the xmin, but it's
			 * possible that that tuple has been removed --- for example, if
			 * it had xmin = xmax and wasn't itself an updated version, then
			 * HeapTupleSatisfiesVacuum would deem it removable as soon as the
			 * xmin xact completes.
			 *
			 * To be on the safe side, we abandon the repair_frag process if
			 * we cannot find the parent tuple in vtlinks.	This may be overly
			 * conservative; AFAICS it would be safe to move the chain.
			 *
			 * Also, because we distinguish DEAD and RECENTLY_DEAD tuples
			 * using OldestXmin, which is a rather coarse test, it is quite
			 * possible to have an update chain in which a tuple we think is
			 * RECENTLY_DEAD links forward to one that is definitely DEAD.
			 * In such a case the RECENTLY_DEAD tuple must actually be dead,
			 * but it seems too complicated to try to make VACUUM remove it.
			 * We treat each contiguous set of RECENTLY_DEAD tuples as a
			 * separately movable chain, ignoring any intervening DEAD ones.
			 */
			if (((tuple.t_data->t_infomask & HEAP_UPDATED) &&
				 !TransactionIdPrecedes(HeapTupleHeaderGetXmin(tuple.t_data),
										OldestXmin)) ||
				(!(tuple.t_data->t_infomask & (HEAP_XMAX_INVALID |
											   HEAP_IS_LOCKED)) &&
				 !(ItemPointerEquals(&(tuple.t_self),
									 &(tuple.t_data->t_ctid)))))
			{
				Buffer		Cbuf = buf;
				bool		freeCbuf = false;
				bool		chain_move_failed = false;
				bool		moved_target = false;
				ItemPointerData Ctid;
				HeapTupleData tp = tuple;
				Size		tlen = tuple_len;
				VTupleMove	vtmove;
				int			num_vtmove;
				int			free_vtmove;
				VacPage		to_vacpage = NULL;
				int			to_item = 0;
				int			ti;

				if (dst_buffer != InvalidBuffer)
				{
					ReleaseBuffer(dst_buffer);
					dst_buffer = InvalidBuffer;
				}

				/* Quick exit if we have no vtlinks to search in */
				if (vacrelstats->vtlinks == NULL)
				{
					elog(DEBUG2, "parent item in update-chain not found --- cannot continue repair_frag");
					break;		/* out of walk-along-page loop */
				}

				/*
				 * If this tuple is in the begin/middle of the chain then we
				 * have to move to the end of chain.  As with any t_ctid
				 * chase, we have to verify that each new tuple is really the
				 * descendant of the tuple we came from; however, here we need
				 * even more than the normal amount of paranoia. If t_ctid
				 * links forward to a tuple determined to be DEAD, then
				 * depending on where that tuple is, it might already have
				 * been removed, and perhaps even replaced by a MOVED_IN
				 * tuple.  We don't want to include any DEAD tuples in the
				 * chain, so we have to recheck HeapTupleSatisfiesVacuum.
				 */
				while (!(tp.t_data->t_infomask & (HEAP_XMAX_INVALID |
												  HEAP_IS_LOCKED)) &&
					   !(ItemPointerEquals(&(tp.t_self),
										   &(tp.t_data->t_ctid))))
				{
					ItemPointerData nextTid;
					TransactionId priorXmax;
					Buffer		nextBuf;
					Page		nextPage;
					OffsetNumber nextOffnum;
					ItemId		nextItemid;
					HeapTupleHeader nextTdata;
					HTSV_Result nextTstatus;

					nextTid = tp.t_data->t_ctid;
					priorXmax = HeapTupleHeaderGetXmax(tp.t_data);
					/* assume block# is OK (see heap_fetch comments) */
					nextBuf = ReadBufferWithStrategy(onerel,
										 ItemPointerGetBlockNumber(&nextTid),
													 vac_strategy);
					nextPage = BufferGetPage(nextBuf);
					/* If bogus or unused slot, assume tp is end of chain */
					nextOffnum = ItemPointerGetOffsetNumber(&nextTid);
					if (nextOffnum < FirstOffsetNumber ||
						nextOffnum > PageGetMaxOffsetNumber(nextPage))
					{
						ReleaseBuffer(nextBuf);
						break;
					}
					nextItemid = PageGetItemId(nextPage, nextOffnum);
					if (!ItemIdIsNormal(nextItemid))
					{
						ReleaseBuffer(nextBuf);
						break;
					}
					/* if not matching XMIN, assume tp is end of chain */
					nextTdata = (HeapTupleHeader) PageGetItem(nextPage,
															  nextItemid);
					if (!TransactionIdEquals(HeapTupleHeaderGetXmin(nextTdata),
											 priorXmax))
					{
						ReleaseBuffer(nextBuf);
						break;
					}

					/*
					 * Must check for DEAD or MOVED_IN tuple, too.	This could
					 * potentially update hint bits, so we'd better hold the
					 * buffer content lock.
					 */
					LockBuffer(nextBuf, BUFFER_LOCK_SHARE);
					nextTstatus = HeapTupleSatisfiesVacuum(onerel,
														   nextTdata,
														   OldestXmin,
														   nextBuf);
					if (nextTstatus == HEAPTUPLE_DEAD ||
						nextTstatus == HEAPTUPLE_INSERT_IN_PROGRESS)
					{
						UnlockReleaseBuffer(nextBuf);
						break;
					}
					LockBuffer(nextBuf, BUFFER_LOCK_UNLOCK);
					/* if it's MOVED_OFF we shoulda moved this one with it */
					if (nextTstatus == HEAPTUPLE_DELETE_IN_PROGRESS)
						elog(ERROR, "updated tuple is already HEAP_MOVED_OFF");
					/* OK, switch our attention to the next tuple in chain */
					tp.t_data = nextTdata;
					tp.t_self = nextTid;
					tlen = tp.t_len = ItemIdGetLength(nextItemid);
					if (freeCbuf)
						ReleaseBuffer(Cbuf);
					Cbuf = nextBuf;
					freeCbuf = true;
				}

				/* Set up workspace for planning the chain move */
				vtmove = (VTupleMove) palloc(100 * sizeof(VTupleMoveData));
				num_vtmove = 0;
				free_vtmove = 100;

				/*
				 * Now, walk backwards up the chain (towards older tuples) and
				 * check if all items in chain can be moved.  We record all
				 * the moves that need to be made in the vtmove array.
				 */
				for (;;)
				{
					Buffer		Pbuf;
					Page		Ppage;
					ItemId		Pitemid;
					HeapTupleHeader PTdata;
					VTupleLinkData vtld,
							   *vtlp;

					/* Identify a target page to move this tuple to */
					if (to_vacpage == NULL ||
						!enough_space(to_vacpage, tlen))
					{
						for (i = 0; i < num_fraged_pages; i++)
						{
							if (enough_space(fraged_pages->pagedesc[i], tlen))
								break;
						}

						if (i == num_fraged_pages)
						{
							/* can't move item anywhere */
							chain_move_failed = true;
							break;		/* out of check-all-items loop */
						}
						to_item = i;
						to_vacpage = fraged_pages->pagedesc[to_item];
					}
					to_vacpage->free -= MAXALIGN(tlen);
					if (to_vacpage->offsets_used >= to_vacpage->offsets_free)
						to_vacpage->free -= sizeof(ItemIdData);
					(to_vacpage->offsets_used)++;

					/* Add an entry to vtmove list */
					if (free_vtmove == 0)
					{
						free_vtmove = 1000;
						vtmove = (VTupleMove)
							repalloc(vtmove,
									 (free_vtmove + num_vtmove) *
									 sizeof(VTupleMoveData));
					}
					vtmove[num_vtmove].tid = tp.t_self;
					vtmove[num_vtmove].vacpage = to_vacpage;
					if (to_vacpage->offsets_used == 1)
						vtmove[num_vtmove].cleanVpd = true;
					else
						vtmove[num_vtmove].cleanVpd = false;
					free_vtmove--;
					num_vtmove++;

					/* Remember if we reached the original target tuple */
					if (ItemPointerGetBlockNumber(&tp.t_self) == blkno &&
						ItemPointerGetOffsetNumber(&tp.t_self) == offnum)
						moved_target = true;

					/* Done if at beginning of chain */
					if (!(tp.t_data->t_infomask & HEAP_UPDATED) ||
					 TransactionIdPrecedes(HeapTupleHeaderGetXmin(tp.t_data),
										   OldestXmin))
						break;	/* out of check-all-items loop */

					/* Move to tuple with prior row version */
					vtld.new_tid = tp.t_self;
					vtlp = (VTupleLink)
						vac_bsearch((void *) &vtld,
									(void *) (vacrelstats->vtlinks),
									vacrelstats->num_vtlinks,
									sizeof(VTupleLinkData),
									vac_cmp_vtlinks);
					if (vtlp == NULL)
					{
						/* see discussion above */
						elog(DEBUG2, "parent item in update-chain not found --- cannot continue repair_frag");
						chain_move_failed = true;
						break;	/* out of check-all-items loop */
					}
					tp.t_self = vtlp->this_tid;
					Pbuf = ReadBufferWithStrategy(onerel,
									 ItemPointerGetBlockNumber(&(tp.t_self)),
												  vac_strategy);
					Ppage = BufferGetPage(Pbuf);
					Pitemid = PageGetItemId(Ppage,
								   ItemPointerGetOffsetNumber(&(tp.t_self)));
					/* this can't happen since we saw tuple earlier: */
					if (!ItemIdIsNormal(Pitemid))
						elog(ERROR, "parent itemid marked as unused");
					PTdata = (HeapTupleHeader) PageGetItem(Ppage, Pitemid);

					/* ctid should not have changed since we saved it */
					Assert(ItemPointerEquals(&(vtld.new_tid),
											 &(PTdata->t_ctid)));

					/*
					 * Read above about cases when !ItemIdIsUsed(nextItemid)
					 * (child item is removed)... Due to the fact that at the
					 * moment we don't remove unuseful part of update-chain,
					 * it's possible to get non-matching parent row here. Like
					 * as in the case which caused this problem, we stop
					 * shrinking here. I could try to find real parent row but
					 * want not to do it because of real solution will be
					 * implemented anyway, later, and we are too close to 6.5
					 * release. - vadim 06/11/99
					 */
					if ((PTdata->t_infomask & HEAP_XMAX_IS_MULTI) ||
						!(TransactionIdEquals(HeapTupleHeaderGetXmax(PTdata),
										 HeapTupleHeaderGetXmin(tp.t_data))))
					{
						ReleaseBuffer(Pbuf);
						elog(DEBUG2, "too old parent tuple found --- cannot continue repair_frag");
						chain_move_failed = true;
						break;	/* out of check-all-items loop */
					}
					tp.t_data = PTdata;
					tlen = tp.t_len = ItemIdGetLength(Pitemid);
					if (freeCbuf)
						ReleaseBuffer(Cbuf);
					Cbuf = Pbuf;
					freeCbuf = true;
				}				/* end of check-all-items loop */

				if (freeCbuf)
					ReleaseBuffer(Cbuf);
				freeCbuf = false;

				/* Double-check that we will move the current target tuple */
				if (!moved_target && !chain_move_failed)
				{
					elog(DEBUG2, "failed to chain back to target --- cannot continue repair_frag");
					chain_move_failed = true;
				}

				if (chain_move_failed)
				{
					/*
					 * Undo changes to offsets_used state.	We don't bother
					 * cleaning up the amount-free state, since we're not
					 * going to do any further tuple motion.
					 */
					for (i = 0; i < num_vtmove; i++)
					{
						Assert(vtmove[i].vacpage->offsets_used > 0);
						(vtmove[i].vacpage->offsets_used)--;
					}
					pfree(vtmove);
					break;		/* out of walk-along-page loop */
				}

				/*
				 * Okay, move the whole tuple chain in reverse order.
				 *
				 * Ctid tracks the new location of the previously-moved tuple.
				 */
				ItemPointerSetInvalid(&Ctid);
				for (ti = 0; ti < num_vtmove; ti++)
				{
					VacPage		destvacpage = vtmove[ti].vacpage;
					Page		Cpage;
					ItemId		Citemid;

					/* Get page to move from */
					tuple.t_self = vtmove[ti].tid;
					Cbuf = ReadBufferWithStrategy(onerel,
								  ItemPointerGetBlockNumber(&(tuple.t_self)),
												  vac_strategy);

					/* Get page to move to */
					dst_buffer = ReadBufferWithStrategy(onerel,
														destvacpage->blkno,
														vac_strategy);

					LockBuffer(dst_buffer, BUFFER_LOCK_EXCLUSIVE);
					if (dst_buffer != Cbuf)
						LockBuffer(Cbuf, BUFFER_LOCK_EXCLUSIVE);

					dst_page = BufferGetPage(dst_buffer);
					Cpage = BufferGetPage(Cbuf);

					Citemid = PageGetItemId(Cpage,
								ItemPointerGetOffsetNumber(&(tuple.t_self)));
					tuple.t_data = (HeapTupleHeader) PageGetItem(Cpage, Citemid);
					tuple_len = tuple.t_len = ItemIdGetLength(Citemid);

					move_chain_tuple(onerel, Cbuf, Cpage, &tuple,
									 dst_buffer, dst_page, destvacpage,
									 &ec, &Ctid, vtmove[ti].cleanVpd);

					/*
					 * If the tuple we are moving is a heap-only tuple, this
					 * move will generate an additional index entry, so
					 * increment the rel_indexed_tuples count.
					 */
					if (HeapTupleHeaderIsHeapOnly(tuple.t_data))
						vacrelstats->rel_indexed_tuples++;

					num_moved++;
					if (destvacpage->blkno > last_move_dest_block)
						last_move_dest_block = destvacpage->blkno;

					/*
					 * Remember that we moved tuple from the current page
					 * (corresponding index tuple will be cleaned).
					 */
					if (Cbuf == buf)
						vacpage->offsets[vacpage->offsets_free++] =
							ItemPointerGetOffsetNumber(&(tuple.t_self));
					else
					{
						/*
						 * When we move tuple chains, we may need to move
						 * tuples from a block that we haven't yet scanned in
						 * the outer walk-along-the-relation loop. Note that
						 * we can't be moving a tuple from a block that we
						 * have already scanned because if such a tuple
						 * exists, then we must have moved the chain along
						 * with that tuple when we scanned that block. IOW the
						 * test of (Cbuf != buf) guarantees that the tuple we
						 * are looking at right now is in a block which is yet
						 * to be scanned.
						 *
						 * We maintain two counters to correctly count the
						 * moved-off tuples from blocks that are not yet
						 * scanned (keep_tuples) and how many of them have
						 * index pointers (keep_indexed_tuples).  The main
						 * reason to track the latter is to help verify that
						 * indexes have the expected number of entries when
						 * all the dust settles.
						 */
						if (!HeapTupleHeaderIsHeapOnly(tuple.t_data))
							keep_indexed_tuples++;
						keep_tuples++;
					}

					ReleaseBuffer(dst_buffer);
					ReleaseBuffer(Cbuf);
				}				/* end of move-the-tuple-chain loop */

				dst_buffer = InvalidBuffer;
				pfree(vtmove);
				chain_tuple_moved = true;

				/* advance to next tuple in walk-along-page loop */
				continue;
			}					/* end of is-tuple-in-chain test */

			/* try to find new page for this tuple */
			if (dst_buffer == InvalidBuffer ||
				!enough_space(dst_vacpage, tuple_len))
			{
				if (dst_buffer != InvalidBuffer)
				{
					ReleaseBuffer(dst_buffer);
					dst_buffer = InvalidBuffer;
				}
				for (i = 0; i < num_fraged_pages; i++)
				{
					if (enough_space(fraged_pages->pagedesc[i], tuple_len))
						break;
				}
				if (i == num_fraged_pages)
					break;		/* can't move item anywhere */
				dst_vacpage = fraged_pages->pagedesc[i];
				dst_buffer = ReadBufferWithStrategy(onerel,
													dst_vacpage->blkno,
													vac_strategy);
				LockBuffer(dst_buffer, BUFFER_LOCK_EXCLUSIVE);
				dst_page = BufferGetPage(dst_buffer);
				/* if this page was not used before - clean it */
				if (!PageIsEmpty(dst_page) && dst_vacpage->offsets_used == 0)
					vacuum_page(onerel, dst_buffer, dst_vacpage);
			}
			else
				LockBuffer(dst_buffer, BUFFER_LOCK_EXCLUSIVE);

			LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

			move_plain_tuple(onerel, buf, page, &tuple,
							 dst_buffer, dst_page, dst_vacpage, &ec);

			/*
			 * If the tuple we are moving is a heap-only tuple, this move will
			 * generate an additional index entry, so increment the
			 * rel_indexed_tuples count.
			 */
			if (HeapTupleHeaderIsHeapOnly(tuple.t_data))
				vacrelstats->rel_indexed_tuples++;

			num_moved++;
			if (dst_vacpage->blkno > last_move_dest_block)
				last_move_dest_block = dst_vacpage->blkno;

			/*
			 * Remember that we moved tuple from the current page
			 * (corresponding index tuple will be cleaned).
			 */
			vacpage->offsets[vacpage->offsets_free++] = offnum;
		}						/* walk along page */

		/*
		 * If we broke out of the walk-along-page loop early (ie, still have
		 * offnum <= maxoff), then we failed to move some tuple off this page.
		 * No point in shrinking any more, so clean up and exit the per-page
		 * loop.
		 */
		if (offnum < maxoff && keep_tuples > 0)
		{
			OffsetNumber off;

			/*
			 * Fix vacpage state for any unvisited tuples remaining on page
			 */
			for (off = OffsetNumberNext(offnum);
				 off <= maxoff;
				 off = OffsetNumberNext(off))
			{
				ItemId		itemid = PageGetItemId(page, off);
				HeapTupleHeader htup;

				if (!ItemIdIsUsed(itemid))
					continue;
				/* Shouldn't be any DEAD or REDIRECT items anymore */
				Assert(ItemIdIsNormal(itemid));

				htup = (HeapTupleHeader) PageGetItem(page, itemid);
				if (htup->t_infomask & HEAP_XMIN_COMMITTED)
					continue;

				/*
				 * See comments in the walk-along-page loop above about why
				 * only MOVED_OFF tuples should be found here.
				 */
				if (htup->t_infomask & HEAP_MOVED_IN)
					elog(ERROR, "HEAP_MOVED_IN was not expected");
				if (!(htup->t_infomask & HEAP_MOVED_OFF))
					elog(ERROR, "HEAP_MOVED_OFF was expected");
				if (HeapTupleHeaderGetXvac(htup) != myXID)
					elog(ERROR, "invalid XVAC in tuple header");

				if (chain_tuple_moved)
				{
					/* some chains were moved while cleaning this page */
					Assert(vacpage->offsets_free > 0);
					for (i = 0; i < vacpage->offsets_free; i++)
					{
						if (vacpage->offsets[i] == off)
							break;
					}
					if (i >= vacpage->offsets_free)		/* not found */
					{
						vacpage->offsets[vacpage->offsets_free++] = off;
						Assert(keep_tuples > 0);

						/*
						 * If this is not a heap-only tuple, there must be an
						 * index entry for this item which will be removed in
						 * the index cleanup. Decrement the
						 * keep_indexed_tuples count to remember this.
						 */
						if (!HeapTupleHeaderIsHeapOnly(htup))
							keep_indexed_tuples--;
						keep_tuples--;
					}
				}
				else
				{
					vacpage->offsets[vacpage->offsets_free++] = off;
					Assert(keep_tuples > 0);
					if (!HeapTupleHeaderIsHeapOnly(htup))
						keep_indexed_tuples--;
					keep_tuples--;
				}
			}
		}

		if (vacpage->offsets_free > 0)	/* some tuples were moved */
		{
			if (chain_tuple_moved)		/* else - they are ordered */
			{
				qsort((char *) (vacpage->offsets), vacpage->offsets_free,
					  sizeof(OffsetNumber), vac_cmp_offno);
			}
			vpage_insert(&Nvacpagelist, copy_vac_page(vacpage));
		}

		MIRROREDLOCK_BUFMGR_UNLOCK;
		// -------- MirroredLock ----------

		ReleaseBuffer(buf);

		if (offnum <= maxoff)
			break;				/* had to quit early, see above note */

	}							/* walk along relation */

	blkno++;					/* new number of blocks */

	if (dst_buffer != InvalidBuffer)
	{
		Assert(num_moved > 0);
		ReleaseBuffer(dst_buffer);
	}

	/*
	 * In GPDB, the moving of relation tuples and truncating the relation is
	 * performed in two separate transactions one after the other so we don't
	 * need to commit the transaction here unlike the upstream code. The
	 * transactions are started and ended in vacuumStatement_Relation().
	 */

	/*
	 * We are not going to move any more tuples across pages, but we still
	 * need to apply vacuum_page to compact free space in the remaining pages
	 * in vacuum_pages list.  Note that some of these pages may also be in the
	 * fraged_pages list, and may have had tuples moved onto them; if so, we
	 * already did vacuum_page and needn't do it again.
	 */
	for (i = 0, curpage = vacuum_pages->pagedesc;
		 i < vacuumed_pages;
		 i++, curpage++)
	{
		vacuum_delay_point();

		Assert((*curpage)->blkno < blkno);
		if ((*curpage)->offsets_used == 0)
		{
			Buffer		buf;
			Page		page;

			/* this page was not used as a move target, so must clean it */

			// -------- MirroredLock ----------
			MIRROREDLOCK_BUFMGR_LOCK;

			buf = ReadBufferWithStrategy(onerel,
										 (*curpage)->blkno,
										 vac_strategy);
			LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
			page = BufferGetPage(buf);
			if (!PageIsEmpty(page))
				vacuum_page(onerel, buf, *curpage);
			UnlockReleaseBuffer(buf);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

		}
	}

	/*
	 * It'd be cleaner to make this report at the bottom of this routine, but
	 * then the rusage would double-count the second pass of index vacuuming.
	 * So do it here and ignore the relatively small amount of processing that
	 * occurs below.
	 */
	ereport(elevel,
			(errmsg("\"%s\": moved %u row versions, will truncate %u to %u pages",
					RelationGetRelationName(onerel),
					num_moved, nblocks, blkno),
			 errdetail("%s.",
					   pg_rusage_show(&ru0))));

	/*
	 * Reflect the motion of system tuples to catalog cache here.
	 */
	CommandCounterIncrement();

	/* clean up */
	pfree(vacpage);
	if (vacrelstats->vtlinks != NULL)
		pfree(vacrelstats->vtlinks);

	ExecContext_Finish(&ec);

	vacuum_pages->empty_end_pages = nblocks - blkno;

	SIMPLE_FAULT_INJECTOR(RepairFragEnd);

	return heldoff;
}

/*
 *	move_chain_tuple() -- move one tuple that is part of a tuple chain
 *
 *		This routine moves old_tup from old_page to dst_page.
 *		old_page and dst_page might be the same page.
 *		On entry old_buf and dst_buf are locked exclusively, both locks (or
 *		the single lock, if this is a intra-page-move) are released before
 *		exit.
 *
 *		Yes, a routine with ten parameters is ugly, but it's still better
 *		than having these 120 lines of code in repair_frag() which is
 *		already too long and almost unreadable.
 */
static void
move_chain_tuple(Relation rel,
				 Buffer old_buf, Page old_page, HeapTuple old_tup,
				 Buffer dst_buf, Page dst_page, VacPage dst_vacpage,
				 ExecContext ec, ItemPointer ctid, bool cleanVpd)
{
	TransactionId myXID = GetCurrentTransactionId();
	HeapTupleData newtup;
	OffsetNumber newoff;
	ItemId		newitemid;
	Size		tuple_len = old_tup->t_len;

	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	/*
	 * make a modifiable copy of the source tuple.
	 */
	heap_copytuple_with_tuple(old_tup, &newtup);

	/*
	 * register invalidation of source tuple in catcaches.
	 */
	CacheInvalidateHeapTuple(rel, old_tup);

	/* NO EREPORT(ERROR) TILL CHANGES ARE LOGGED */
	START_CRIT_SECTION();

	/*
	 * mark the source tuple MOVED_OFF.
	 */
	old_tup->t_data->t_infomask &= ~(HEAP_XMIN_COMMITTED |
									 HEAP_XMIN_INVALID |
									 HEAP_MOVED_IN);
	old_tup->t_data->t_infomask |= HEAP_MOVED_OFF;
	HeapTupleHeaderSetXvac(old_tup->t_data, myXID);

	/*
	 * If this page was not used before - clean it.
	 *
	 * NOTE: a nasty bug used to lurk here.  It is possible for the source and
	 * destination pages to be the same (since this tuple-chain member can be
	 * on a page lower than the one we're currently processing in the outer
	 * loop).  If that's true, then after vacuum_page() the source tuple will
	 * have been moved, and tuple.t_data will be pointing at garbage.
	 * Therefore we must do everything that uses old_tup->t_data BEFORE this
	 * step!!
	 *
	 * This path is different from the other callers of vacuum_page, because
	 * we have already incremented the vacpage's offsets_used field to account
	 * for the tuple(s) we expect to move onto the page. Therefore
	 * vacuum_page's check for offsets_used == 0 is wrong. But since that's a
	 * good debugging check for all other callers, we work around it here
	 * rather than remove it.
	 */
	if (!PageIsEmpty(dst_page) && cleanVpd)
	{
		int			sv_offsets_used = dst_vacpage->offsets_used;

		dst_vacpage->offsets_used = 0;
		vacuum_page(rel, dst_buf, dst_vacpage);
		dst_vacpage->offsets_used = sv_offsets_used;
	}

	/*
	 * Update the state of the copied tuple, and store it on the destination
	 * page.  The copied tuple is never part of a HOT chain.
	 */
	newtup.t_data->t_infomask &= ~(HEAP_XMIN_COMMITTED |
								   HEAP_XMIN_INVALID |
								   HEAP_MOVED_OFF);
	newtup.t_data->t_infomask |= HEAP_MOVED_IN;
	HeapTupleHeaderClearHotUpdated(newtup.t_data);
	HeapTupleHeaderClearHeapOnly(newtup.t_data);
	HeapTupleHeaderSetXvac(newtup.t_data, myXID);
	newoff = PageAddItem(dst_page, (Item) newtup.t_data, tuple_len,
						 InvalidOffsetNumber, false, true);
	if (newoff == InvalidOffsetNumber)
		elog(PANIC, "failed to add item with len = %lu to page %u while moving tuple chain",
			 (unsigned long) tuple_len, dst_vacpage->blkno);
	newitemid = PageGetItemId(dst_page, newoff);
	/* drop temporary copy, and point to the version on the dest page */
	pfree(newtup.t_data);
	newtup.t_data = (HeapTupleHeader) PageGetItem(dst_page, newitemid);

	ItemPointerSet(&(newtup.t_self), dst_vacpage->blkno, newoff);

	/*
	 * Set new tuple's t_ctid pointing to itself if last tuple in chain, and
	 * to next tuple in chain otherwise.  (Since we move the chain in reverse
	 * order, this is actually the previously processed tuple.)
	 */
	if (!ItemPointerIsValid(ctid))
		newtup.t_data->t_ctid = newtup.t_self;
	else
		newtup.t_data->t_ctid = *ctid;
	*ctid = newtup.t_self;

	MarkBufferDirty(dst_buf);
	if (dst_buf != old_buf)
		MarkBufferDirty(old_buf);

	/* XLOG stuff */
	if (!rel->rd_istemp)
	{
		XLogRecPtr	recptr = log_heap_move(rel, old_buf, old_tup->t_self,
										   dst_buf, &newtup);

		if (old_buf != dst_buf)
		{
			PageSetLSN(old_page, recptr);
		}
		PageSetLSN(dst_page, recptr);
	}

	END_CRIT_SECTION();

	LockBuffer(dst_buf, BUFFER_LOCK_UNLOCK);
	if (dst_buf != old_buf)
		LockBuffer(old_buf, BUFFER_LOCK_UNLOCK);

	/* Create index entries for the moved tuple */
	if (ec->resultRelInfo->ri_NumIndices > 0)
	{
		ExecStoreHeapTuple(&newtup, ec->slot, InvalidBuffer, false);
		ExecInsertIndexTuples(ec->slot, &(newtup.t_self), ec->estate, true);
		ResetPerTupleExprContext(ec->estate);
	}
}

/*
 *	move_plain_tuple() -- move one tuple that is not part of a chain
 *
 *		This routine moves old_tup from old_page to dst_page.
 *		On entry old_buf and dst_buf are locked exclusively, both locks are
 *		released before exit.
 *
 *		Yes, a routine with eight parameters is ugly, but it's still better
 *		than having these 90 lines of code in repair_frag() which is already
 *		too long and almost unreadable.
 */
static void
move_plain_tuple(Relation rel,
				 Buffer old_buf, Page old_page, HeapTuple old_tup,
				 Buffer dst_buf, Page dst_page, VacPage dst_vacpage,
				 ExecContext ec)
{
	TransactionId myXID = GetCurrentTransactionId();
	HeapTupleData newtup;
	OffsetNumber newoff;
	ItemId		newitemid;
	Size		tuple_len = old_tup->t_len;

	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	/* copy tuple */
	heap_copytuple_with_tuple(old_tup, &newtup);

	/*
	 * register invalidation of source tuple in catcaches.
	 *
	 * (Note: we do not need to register the copied tuple, because we are not
	 * changing the tuple contents and so there cannot be any need to flush
	 * negative catcache entries.)
	 */
	CacheInvalidateHeapTuple(rel, old_tup);

	/* NO EREPORT(ERROR) TILL CHANGES ARE LOGGED */
	START_CRIT_SECTION();

	/*
	 * Mark new tuple as MOVED_IN by me; also mark it not HOT.
	 */
	newtup.t_data->t_infomask &= ~(HEAP_XMIN_COMMITTED |
								   HEAP_XMIN_INVALID |
								   HEAP_MOVED_OFF);
	newtup.t_data->t_infomask |= HEAP_MOVED_IN;
	HeapTupleHeaderClearHotUpdated(newtup.t_data);
	HeapTupleHeaderClearHeapOnly(newtup.t_data);
	HeapTupleHeaderSetXvac(newtup.t_data, myXID);

	/* add tuple to the page */
	newoff = PageAddItem(dst_page, (Item) newtup.t_data, tuple_len,
						 InvalidOffsetNumber, false, true);
	if (newoff == InvalidOffsetNumber)
		elog(PANIC, "failed to add item with len = %lu to page %u (free space %lu, nusd %u, noff %u)",
			 (unsigned long) tuple_len,
			 dst_vacpage->blkno, (unsigned long) dst_vacpage->free,
			 dst_vacpage->offsets_used, dst_vacpage->offsets_free);
	newitemid = PageGetItemId(dst_page, newoff);
	pfree(newtup.t_data);
	newtup.t_data = (HeapTupleHeader) PageGetItem(dst_page, newitemid);
	ItemPointerSet(&(newtup.t_data->t_ctid), dst_vacpage->blkno, newoff);
	newtup.t_self = newtup.t_data->t_ctid;

	/*
	 * Mark old tuple as MOVED_OFF by me.
	 */
	old_tup->t_data->t_infomask &= ~(HEAP_XMIN_COMMITTED |
									 HEAP_XMIN_INVALID |
									 HEAP_MOVED_IN);
	old_tup->t_data->t_infomask |= HEAP_MOVED_OFF;
	HeapTupleHeaderSetXvac(old_tup->t_data, myXID);

	MarkBufferDirty(dst_buf);
	MarkBufferDirty(old_buf);

	/* XLOG stuff */
	if (!rel->rd_istemp)
	{
		XLogRecPtr	recptr = log_heap_move(rel, old_buf, old_tup->t_self,
										   dst_buf, &newtup);

		PageSetLSN(old_page, recptr);
		PageSetLSN(dst_page, recptr);
	}

	END_CRIT_SECTION();

	dst_vacpage->free = PageGetFreeSpaceWithFillFactor(rel, dst_page);
	LockBuffer(dst_buf, BUFFER_LOCK_UNLOCK);
	LockBuffer(old_buf, BUFFER_LOCK_UNLOCK);

	dst_vacpage->offsets_used++;

	/* insert index' tuples if needed */
	if (ec->resultRelInfo->ri_NumIndices > 0)
	{
		ExecStoreHeapTuple(&newtup, ec->slot, InvalidBuffer, false);
		ExecInsertIndexTuples(ec->slot, &(newtup.t_self), ec->estate, true);
		ResetPerTupleExprContext(ec->estate);
	}
}

/*
 *	vacuum_heap() -- free dead tuples
 *
 *		This routine marks dead tuples as unused and truncates relation
 *		if there are "empty" end-blocks.
 */
static void
vacuum_heap(VRelStats *vacrelstats, Relation onerel, VacPageList vacuum_pages)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	Buffer		buf;
	VacPage    *vacpage;
	int			nblocks;
	int			i;

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(onerel);

	nblocks = vacuum_pages->num_pages;
	nblocks -= vacuum_pages->empty_end_pages;	/* nothing to do with them */

	for (i = 0, vacpage = vacuum_pages->pagedesc; i < nblocks; i++, vacpage++)
	{
		vacuum_delay_point();

		if ((*vacpage)->offsets_free > 0)
		{

			// -------- MirroredLock ----------
			MIRROREDLOCK_BUFMGR_LOCK;

			buf = ReadBufferWithStrategy(onerel,
										 (*vacpage)->blkno,
										 vac_strategy);
			LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
			vacuum_page(onerel, buf, *vacpage);
			UnlockReleaseBuffer(buf);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

		}
	}
}

/*
 *	vacuum_page() -- free dead tuples on a page
 *					 and repair its fragmentation.
 *
 * Caller must hold pin and lock on buffer.
 */
static void
vacuum_page(Relation onerel, Buffer buffer, VacPage vacpage)
{
	Page		page = BufferGetPage(buffer);
	int			i;

	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	/* There shouldn't be any tuples moved onto the page yet! */
	Assert(vacpage->offsets_used == 0);

	START_CRIT_SECTION();

	for (i = 0; i < vacpage->offsets_free; i++)
	{
		ItemId		itemid = PageGetItemId(page, vacpage->offsets[i]);

		ItemIdSetUnused(itemid);
	}

	PageRepairFragmentation(page);

	MarkBufferDirty(buffer);

	/* XLOG stuff */
	if (!onerel->rd_istemp)
	{
		XLogRecPtr	recptr;

		recptr = log_heap_clean(onerel, buffer,
								NULL, 0, NULL, 0,
								vacpage->offsets, vacpage->offsets_free,
								false);
		PageSetLSN(page, recptr);
	}

	END_CRIT_SECTION();
}

/*
 *	scan_index() -- scan one index relation to update pg_class statistics.
 *
 * We use this when we have no deletions to do.
 */
static void
scan_index(Relation indrel, double num_tuples, List *updated_stats, bool isfull, bool check_stats)
{
	IndexBulkDeleteResult *stats;
	IndexVacuumInfo ivinfo;
	PGRUsage	ru0;

	pg_rusage_init(&ru0);

	ivinfo.index = indrel;
	ivinfo.vacuum_full = isfull;
	ivinfo.message_level = elevel;
	ivinfo.num_heap_tuples = num_tuples;
	ivinfo.strategy = vac_strategy;

	stats = index_vacuum_cleanup(&ivinfo, NULL);

	if (!stats)
		return;

	/* now update statistics in pg_class */
	vac_update_relstats_from_list(indrel,
						stats->num_pages, stats->num_index_tuples,
						false, InvalidTransactionId, updated_stats);

	ereport(elevel,
			(errmsg("index \"%s\" now contains %.0f row versions in %u pages",
					RelationGetRelationName(indrel),
					stats->num_index_tuples,
					stats->num_pages),
	errdetail("%u index pages have been deleted, %u are currently reusable.\n"
			  "%s.",
			  stats->pages_deleted, stats->pages_free,
			  pg_rusage_show(&ru0))));

	/*
	 * Check for tuple count mismatch.	If the index is partial, then it's OK
	 * for it to have fewer tuples than the heap; else we got trouble.
	 */
	if (check_stats && stats->num_index_tuples != num_tuples)
	{
		if (stats->num_index_tuples > num_tuples ||
			!vac_is_partial_index(indrel))
			ereport(WARNING,
					(errmsg("index \"%s\" contains %.0f row versions, but table contains %.0f row versions",
							RelationGetRelationName(indrel),
							stats->num_index_tuples, num_tuples),
					 errhint("Rebuild the index with REINDEX.")));
	}

	pfree(stats);
}

/*
 * Vacuums an index on an append-only table.
 *
 * This is called after an append-only segment file compaction to move
 * all tuples from the compacted segment files.
 * The segmentFileList is an
 */
static void
vacuum_appendonly_index(Relation indexRelation,
		AppendOnlyIndexVacuumState* vacuumIndexState,
		List *updated_stats,
		double rel_tuple_count,
		bool isfull)
{
	Assert(RelationIsValid(indexRelation));
	Assert(vacuumIndexState);

	IndexBulkDeleteResult *stats;
	IndexVacuumInfo ivinfo;
	PGRUsage	ru0;

	pg_rusage_init(&ru0);

	ivinfo.index = indexRelation;
	ivinfo.vacuum_full = isfull;
	ivinfo.message_level = elevel;
	ivinfo.num_heap_tuples = rel_tuple_count;
	ivinfo.strategy = vac_strategy;

	/* Do bulk deletion */
	stats = index_bulk_delete(&ivinfo, NULL, appendonly_tid_reaped,
			(void *) vacuumIndexState);

	/* Do post-VACUUM cleanup */
	stats = index_vacuum_cleanup(&ivinfo, stats);

	if (!stats)
		return;

	/* now update statistics in pg_class */
	vac_update_relstats_from_list(indexRelation,
						stats->num_pages, stats->num_index_tuples,
						false, InvalidTransactionId, updated_stats);

	ereport(elevel,
			(errmsg("index \"%s\" now contains %.0f row versions in %u pages",
					RelationGetRelationName(indexRelation),
					stats->num_index_tuples,
					stats->num_pages),
			 errdetail("%.0f index row versions were removed.\n"
			 "%u index pages have been deleted, %u are currently reusable.\n"
					   "%s.",
					   stats->tuples_removed,
					   stats->pages_deleted, stats->pages_free,
					   pg_rusage_show(&ru0))));

	pfree(stats);

}

/*
 *	vacuum_index() -- vacuum one index relation.
 *
 *		Vpl is the VacPageList of the heap we're currently vacuuming.
 *		It's locked. Indrel is an index relation on the vacuumed heap.
 *
 *		We don't bother to set locks on the index relation here, since
 *		the parent table is exclusive-locked already.
 *
 *		Finally, we arrange to update the index relation's statistics in
 *		pg_class.
 */
static void
vacuum_index(VacPageList vacpagelist, Relation indrel,
			 double num_tuples, int keep_tuples, List *updated_stats,
			 bool check_stats)
{
	IndexBulkDeleteResult *stats;
	IndexVacuumInfo ivinfo;
	PGRUsage	ru0;

	pg_rusage_init(&ru0);

	ivinfo.index = indrel;
	ivinfo.vacuum_full = true;
	ivinfo.message_level = elevel;
	ivinfo.num_heap_tuples = num_tuples + keep_tuples;
	ivinfo.strategy = vac_strategy;

	/* Do bulk deletion */
	stats = index_bulk_delete(&ivinfo, NULL, tid_reaped, (void *) vacpagelist);

	/* Do post-VACUUM cleanup */
	stats = index_vacuum_cleanup(&ivinfo, stats);

	if (!stats)
		return;

	/* now update statistics in pg_class */
	vac_update_relstats_from_list(indrel,
						stats->num_pages, stats->num_index_tuples,
						false, InvalidTransactionId, updated_stats);

	ereport(elevel,
			(errmsg("index \"%s\" now contains %.0f row versions in %u pages",
					RelationGetRelationName(indrel),
					stats->num_index_tuples,
					stats->num_pages),
			 errdetail("%.0f index row versions were removed.\n"
			 "%u index pages have been deleted, %u are currently reusable.\n"
					   "%s.",
					   stats->tuples_removed,
					   stats->pages_deleted, stats->pages_free,
					   pg_rusage_show(&ru0))));

	/*
	 * Check for tuple count mismatch.	If the index is partial, then it's OK
	 * for it to have fewer tuples than the heap; else we got trouble.
	 */
	if (check_stats && stats->num_index_tuples != num_tuples + keep_tuples)
	{
		if (stats->num_index_tuples > num_tuples + keep_tuples ||
			!vac_is_partial_index(indrel))
			ereport(WARNING,
					(errmsg("index \"%s\" contains %.0f row versions, but table contains %.0f row versions",
							RelationGetRelationName(indrel),
						  stats->num_index_tuples, num_tuples + keep_tuples),
					 errhint("Rebuild the index with REINDEX.")));
	}

	pfree(stats);
}

static bool
appendonly_tid_reapded_check_block_directory(AppendOnlyIndexVacuumState* vacuumState,
		AOTupleId* aoTupleId)
{
	if (vacuumState->blockDirectory.currentSegmentFileNum ==
			AOTupleIdGet_segmentFileNum(aoTupleId) &&
			AppendOnlyBlockDirectoryEntry_RangeHasRow(&vacuumState->blockDirectoryEntry,
				AOTupleIdGet_rowNum(aoTupleId)))
	{
		return true;
	}

	if (!AppendOnlyBlockDirectory_GetEntry(&vacuumState->blockDirectory,
		aoTupleId,
		0,
		&vacuumState->blockDirectoryEntry))
	{
		return false;
	}
	return (vacuumState->blockDirectory.currentSegmentFileNum ==
			AOTupleIdGet_segmentFileNum(aoTupleId) &&
			AppendOnlyBlockDirectoryEntry_RangeHasRow(&vacuumState->blockDirectoryEntry,
				AOTupleIdGet_rowNum(aoTupleId)));
}

/*
 * appendonly_tid_reaped()
 *
 * Is a particular tid for an appendonly reaped?
 * state should contain an integer list of all compacted
 * segment files.
 *
 * This has the right signature to be an IndexBulkDeleteCallback.
 */
static bool
appendonly_tid_reaped(ItemPointer itemptr, void *state)
{
	AOTupleId* aoTupleId;
	AppendOnlyIndexVacuumState* vacuumState;
	bool reaped;

	Assert(itemptr);
	Assert(state);

	aoTupleId = (AOTupleId *)itemptr;
	vacuumState = (AppendOnlyIndexVacuumState *)state;

	reaped = !appendonly_tid_reapded_check_block_directory(vacuumState,
			aoTupleId);
	if (!reaped)
	{
		/* Also check visi map */
		reaped = !AppendOnlyVisimap_IsVisible(&vacuumState->visiMap,
		aoTupleId);
	}

	elogif(Debug_appendonly_print_compaction, DEBUG3,
			"Index vacuum %s %d",
			AOTupleIdToString(aoTupleId), reaped);
	return reaped;
}

/*
 *	tid_reaped() -- is a particular tid reaped?
 *
 *		This has the right signature to be an IndexBulkDeleteCallback.
 *
 *		vacpagelist->VacPage_array is sorted in right order.
 */
static bool
tid_reaped(ItemPointer itemptr, void *state)
{
	VacPageList vacpagelist = (VacPageList) state;
	OffsetNumber ioffno;
	OffsetNumber *voff;
	VacPage		vp,
			   *vpp;
	VacPageData vacpage;

	vacpage.blkno = ItemPointerGetBlockNumber(itemptr);
	ioffno = ItemPointerGetOffsetNumber(itemptr);

	vp = &vacpage;
	vpp = (VacPage *) vac_bsearch((void *) &vp,
								  (void *) (vacpagelist->pagedesc),
								  vacpagelist->num_pages,
								  sizeof(VacPage),
								  vac_cmp_blk);

	if (vpp == NULL)
		return false;

	/* ok - we are on a partially or fully reaped page */
	vp = *vpp;

	if (vp->offsets_free == 0)
	{
		/* this is EmptyPage, so claim all tuples on it are reaped!!! */
		return true;
	}

	voff = (OffsetNumber *) vac_bsearch((void *) &ioffno,
										(void *) (vp->offsets),
										vp->offsets_free,
										sizeof(OffsetNumber),
										vac_cmp_offno);

	if (voff == NULL)
		return false;

	/* tid is reaped */
	return true;
}

/*
 * Update the shared Free Space Map with the info we now have about
 * free space in the relation, discarding any old info the map may have.
 */
static void
vac_update_fsm(Relation onerel, VacPageList fraged_pages,
			   BlockNumber rel_pages)
{
	int			nPages = fraged_pages->num_pages;
	VacPage    *pagedesc = fraged_pages->pagedesc;
	Size		threshold;
	FSMPageData *pageSpaces;
	int			outPages;
	int			i;

	/*
	 * We only report pages with free space at least equal to the average
	 * request size --- this avoids cluttering FSM with uselessly-small bits
	 * of space.  Although FSM would discard pages with little free space
	 * anyway, it's important to do this prefiltering because (a) it reduces
	 * the time spent holding the FSM lock in RecordRelationFreeSpace, and (b)
	 * FSM uses the number of pages reported as a statistic for guiding space
	 * management.	If we didn't threshold our reports the same way
	 * vacuumlazy.c does, we'd be skewing that statistic.
	 */
	threshold = GetAvgFSMRequestSize(&onerel->rd_node);

	pageSpaces = (FSMPageData *) palloc(nPages * sizeof(FSMPageData));
	outPages = 0;

	for (i = 0; i < nPages; i++)
	{
		/*
		 * fraged_pages may contain entries for pages that we later decided to
		 * truncate from the relation; don't enter them into the free space
		 * map!
		 */
		if (pagedesc[i]->blkno >= rel_pages)
			break;

		if (pagedesc[i]->free >= threshold)
		{
			FSMPageSetPageNum(&pageSpaces[outPages], pagedesc[i]->blkno);
			FSMPageSetSpace(&pageSpaces[outPages], pagedesc[i]->free);
			outPages++;
		}
	}

	RecordRelationFreeSpace(&onerel->rd_node, outPages, outPages, pageSpaces);

	pfree(pageSpaces);
}

/* Copy a VacPage structure */
static VacPage
copy_vac_page(VacPage vacpage)
{
	VacPage		newvacpage;

	/* allocate a VacPageData entry */
	newvacpage = (VacPage) palloc(sizeof(VacPageData) +
							   vacpage->offsets_free * sizeof(OffsetNumber));

	/* fill it in */
	if (vacpage->offsets_free > 0)
		memcpy(newvacpage->offsets, vacpage->offsets,
			   vacpage->offsets_free * sizeof(OffsetNumber));
	newvacpage->blkno = vacpage->blkno;
	newvacpage->free = vacpage->free;
	newvacpage->offsets_used = vacpage->offsets_used;
	newvacpage->offsets_free = vacpage->offsets_free;

	return newvacpage;
}

/*
 * Add a VacPage pointer to a VacPageList.
 *
 *		As a side effect of the way that scan_heap works,
 *		higher pages come after lower pages in the array
 *		(and highest tid on a page is last).
 */
static void
vpage_insert(VacPageList vacpagelist, VacPage vpnew)
{
#define PG_NPAGEDESC 1024

	/* allocate a VacPage entry if needed */
	if (vacpagelist->num_pages == 0)
	{
		vacpagelist->pagedesc = (VacPage *) palloc(PG_NPAGEDESC * sizeof(VacPage));
		vacpagelist->num_allocated_pages = PG_NPAGEDESC;
	}
	else if (vacpagelist->num_pages >= vacpagelist->num_allocated_pages)
	{
		vacpagelist->num_allocated_pages *= 2;
		vacpagelist->pagedesc = (VacPage *) repalloc(vacpagelist->pagedesc, vacpagelist->num_allocated_pages * sizeof(VacPage));
	}
	vacpagelist->pagedesc[vacpagelist->num_pages] = vpnew;
	(vacpagelist->num_pages)++;
}

/*
 * vac_bsearch: just like standard C library routine bsearch(),
 * except that we first test to see whether the target key is outside
 * the range of the table entries.	This case is handled relatively slowly
 * by the normal binary search algorithm (ie, no faster than any other key)
 * but it occurs often enough in VACUUM to be worth optimizing.
 */
static void *
vac_bsearch(const void *key, const void *base,
			size_t nelem, size_t size,
			int (*compar) (const void *, const void *))
{
	int			res;
	const void *last;

	if (nelem == 0)
		return NULL;
	res = compar(key, base);
	if (res < 0)
		return NULL;
	if (res == 0)
		return (void *) base;
	if (nelem > 1)
	{
		last = (const void *) ((const char *) base + (nelem - 1) * size);
		res = compar(key, last);
		if (res > 0)
			return NULL;
		if (res == 0)
			return (void *) last;
	}
	if (nelem <= 2)
		return NULL;			/* already checked 'em all */
	return bsearch(key, base, nelem, size, compar);
}

/*
 * Comparator routines for use with qsort() and bsearch().
 */
static int
vac_cmp_blk(const void *left, const void *right)
{
	BlockNumber lblk,
				rblk;

	lblk = (*((VacPage *) left))->blkno;
	rblk = (*((VacPage *) right))->blkno;

	if (lblk < rblk)
		return -1;
	if (lblk == rblk)
		return 0;
	return 1;
}

static int
vac_cmp_offno(const void *left, const void *right)
{
	if (*(OffsetNumber *) left < *(OffsetNumber *) right)
		return -1;
	if (*(OffsetNumber *) left == *(OffsetNumber *) right)
		return 0;
	return 1;
}

static int
vac_cmp_vtlinks(const void *left, const void *right)
{
	if (((VTupleLink) left)->new_tid.ip_blkid.bi_hi <
		((VTupleLink) right)->new_tid.ip_blkid.bi_hi)
		return -1;
	if (((VTupleLink) left)->new_tid.ip_blkid.bi_hi >
		((VTupleLink) right)->new_tid.ip_blkid.bi_hi)
		return 1;
	/* bi_hi-es are equal */
	if (((VTupleLink) left)->new_tid.ip_blkid.bi_lo <
		((VTupleLink) right)->new_tid.ip_blkid.bi_lo)
		return -1;
	if (((VTupleLink) left)->new_tid.ip_blkid.bi_lo >
		((VTupleLink) right)->new_tid.ip_blkid.bi_lo)
		return 1;
	/* bi_lo-es are equal */
	if (((VTupleLink) left)->new_tid.ip_posid <
		((VTupleLink) right)->new_tid.ip_posid)
		return -1;
	if (((VTupleLink) left)->new_tid.ip_posid >
		((VTupleLink) right)->new_tid.ip_posid)
		return 1;
	return 0;
}


/*
 * Open all the vacuumable indexes of the given relation, obtaining the
 * specified kind of lock on each.	Return an array of Relation pointers for
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
 * Release the resources acquired by vac_open_indexes.	Optionally release
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
 * Is an index partial (ie, could it contain fewer tuples than the heap?)
 */
bool
vac_is_partial_index(Relation indrel)
{
	/*
	 * If the index's AM doesn't support nulls, it's partial for our purposes
	 */
	if (!indrel->rd_am->amindexnulls)
		return true;

	/* Otherwise, look to see if there's a partial-index predicate */
	if (!heap_attisnull(indrel->rd_indextuple, Anum_pg_index_indpred))
		return true;

	return false;
}


static bool
enough_space(VacPage vacpage, Size len)
{
	len = MAXALIGN(len);

	if (len > vacpage->free)
		return false;

	/* if there are free itemid(s) and len <= free_space... */
	if (vacpage->offsets_used < vacpage->offsets_free)
		return true;

	/* noff_used >= noff_free and so we'll have to allocate new itemid */
	if (len + sizeof(ItemIdData) <= vacpage->free)
		return true;

	return false;
}

static Size
PageGetFreeSpaceWithFillFactor(Relation relation, Page page)
{
	/*
	 * It is correct to use PageGetExactFreeSpace() here, *not*
	 * PageGetHeapFreeSpace().  This is because (a) we do our own, exact
	 * accounting for whether line pointers must be added, and (b) we will
	 * recycle any LP_DEAD line pointers before starting to add rows to a
	 * page, but that may not have happened yet at the time this function is
	 * applied to a page, which means PageGetHeapFreeSpace()'s protection
	 * against too many line pointers on a page could fire incorrectly.  We do
	 * not need that protection here: since VACUUM FULL always recycles all
	 * dead line pointers first, it'd be physically impossible to insert more
	 * than MaxHeapTuplesPerPage tuples anyway.
	 */
	Size		freespace = PageGetExactFreeSpace(page);
	Size		targetfree;

	targetfree = RelationGetTargetPageFreeSpace(relation,
												HEAP_DEFAULT_FILLFACTOR);
	if (freespace > targetfree)
		return freespace - targetfree;
	else
		return 0;
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
dispatchVacuum(VacuumStmt *vacstmt, VacuumStatsContext *ctx)
{
	CdbPgResults cdb_pgresults;

	/* should these be marked volatile ? */

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(vacstmt);
	Assert(vacstmt->vacuum);
	Assert(!vacstmt->analyze);

	/* XXX: Some kinds of VACUUM assign a new relfilenode. bitmap indexes maybe? */
	CdbDispatchUtilityStatement((Node *) vacstmt,
								DF_CANCEL_ON_ERROR|
								DF_WITH_SNAPSHOT|
								DF_NEED_TWO_PHASE,
								GetAssignedOidsForDispatch(),
								&cdb_pgresults);

	vacuum_combine_stats(ctx, &cdb_pgresults);

	cdbdisp_clearCdbPgResults(&cdb_pgresults);
}

/*
 * open_relation_and_check_permission -- open the relation with an appropriate
 * lock based on the vacuum statement, and check for the permissions on this
 * relation.
 */
static Relation
open_relation_and_check_permission(VacuumStmt *vacstmt,
								   Oid relid,
								   char expected_relkind,
								   bool isDropTransaction)
{
	Relation onerel;
	LOCKMODE lmode;
	bool dontWait = false;

	/*
	 * If this is a drop transaction and there is another parallel drop transaction
	 * (on any relation) active. We drop out there. The other drop transaction
	 * might be on the same relation and that would be upgrade deadlock.
	 *
	 * Note: By the time we would have reached try_relation_open the other
	 * drop transaction might already be completed, but we don't take that
	 * risk here.
	 *
	 * My marking the drop transaction as busy before checking, the worst
	 * thing that can happen is that both transaction see each other and
	 * both cancel the drop.
	 *
	 * The upgrade deadlock is not applicable to vacuum full because
	 * it begins with an AccessExclusive lock and doesn't need to
	 * upgrade it.
	 */

	if (isDropTransaction && !vacstmt->full)
	{
		MyProc->inDropTransaction = true;
		SIMPLE_FAULT_INJECTOR(VacuumRelationOpenRelationDuringDropPhase);
		if (HasDropTransaction(false))
		{
			elogif(Debug_appendonly_print_compaction, LOG,
					"Skip drop because of concurrent drop transaction");

			return NULL;
		}
	}

	/*
	 * Determine the type of lock we want --- hard exclusive lock for a FULL
	 * vacuum, but just ShareUpdateExclusiveLock for concurrent vacuum. Either
	 * way, we can be sure that no other backend is vacuuming the same table.
	 * For analyze, we use ShareUpdateExclusiveLock.
	 */
	if (isDropTransaction)
	{
		lmode = AccessExclusiveLock;
		dontWait = true;
	}
	else if (!vacstmt->vacuum)
		lmode = ShareUpdateExclusiveLock;
	else
		lmode = vacstmt->full ? AccessExclusiveLock : ShareUpdateExclusiveLock;

	/*
	 * Open the relation and get the appropriate lock on it.
	 *
	 * There's a race condition here: the rel may have gone away since the
	 * last time we saw it.  If so, we don't need to vacuum it.
	 */
	onerel = try_relation_open(relid, lmode, dontWait);

	if (!RelationIsValid(onerel))
		return NULL;

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
		return NULL;
	}

	/*
	 * Check that it's a plain table; we used to do this in get_rel_oids() but
	 * seems safer to check after we've locked the relation.
	 */
	if (onerel->rd_rel->relkind != expected_relkind ||
		RelationIsExternal(onerel) ||
		(vacstmt->full && GpPersistent_IsPersistentRelation(RelationGetRelid(onerel))))
	{
		ereport(WARNING,
				(errmsg("skipping \"%s\" --- cannot vacuum indexes, views, external tables, or special system tables",
						RelationGetRelationName(onerel))));
		relation_close(onerel, lmode);
		return NULL;
	}

	/*
	 * Silently ignore tables that are temp tables of other backends ---
	 * trying to vacuum these will lead to great unhappiness, since their
	 * contents are probably not up-to-date on disk.  (We don't throw a
	 * warning here; it would just lead to chatter during a database-wide
	 * VACUUM.)
	 */
	if (isOtherTempNamespace(RelationGetNamespace(onerel)))
	{
		relation_close(onerel, lmode);
		return NULL;
	}

	/*
	 * We can ANALYZE any table except pg_statistic. See update_attstats
	 */
	if (vacstmt->analyze && RelationGetRelid(onerel) == StatisticRelationId)
	{
		relation_close(onerel, ShareUpdateExclusiveLock);
		return NULL;
	}

	return onerel;
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
	int result_no;

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
		 * Process the stats for pg_class. We simple compute the maximum
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
				break;
			}
		}

		if (lc == NULL)
		{
			Assert(pgresult->extraslen == sizeof(VPgClassStats));

			pgclass_stats = palloc(sizeof(VPgClassStats));
			memcpy(pgclass_stats, pgresult->extras, pgresult->extraslen);

			stats_context->updated_stats =
					lappend(stats_context->updated_stats, pgclass_stats);
		}
	}
}
