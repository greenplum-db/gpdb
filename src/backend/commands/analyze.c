/*-------------------------------------------------------------------------
 *
 * analyze.c
 *	  the Postgres statistics generator
 *
 *
 * There are a few things in Greenplum that make this more complicated
 * than in upstream:
 *
 * AO tables
 * ---------
 *
 * To work with AO tables, there is a new function, acquire_sample_rows_ao(),
 * which return a set of sample rows, just like the upstream
 * acquire_sample_rows() function. The upstream acquire_sample_rows()
 * function has been renamed to acquire_sample_rows_heap(), and
 * acquire_sample_rows() is now a wrapper that calls either
 * acquire_sample_rows_heap() or acquire_sample_rows_ao(), depending on
 * the table type.
 *
 * Dispatching
 * -----------
 *
 * Greenplum is an MPP system, so we need to collect the statistics from
 * all the segments. The segment servers don't keep statistics (unless you
 * connect to a segment in utility node and run ANALYZE directly), and
 * the orchestration of ANALYZE happens in the dispatcher. The high
 * level logic is the same as in upstream, but a few functions have been
 * modified to gather data from the segments, instead of reading directly
 * from local disk:
 *
 * acquire_sample_rows(), when called in the dispatcher, calls into the
 * segments to acquire the sample across all segments.
 * RelationGetNumberOfBlocks() calls have been replaced with a wrapper
 * function, acquire_number_of_blocks(), which likewise calls into the
 * segments, to get total relation size across all segments.
 *
 * acquire_number_of_blocks() calls pg_relation_size(), which already
 * contains the logic to gather the size from all segments.
 *
 * Acquiring the sample rows is more tricky. When called in dispatcher,
 * acquire_sample_rows() calls a helper function called gp_acquire_sample_rows()
 * in the segments, to collect a sample on each segment. It then merges
 * the sample rows from each segment to produce a sample of the whole
 * cluster. gp_acquire_sample_rows() in turn calls acquire_sample_rows(), to
 * collect the sample on the segment.
 *
 * One complication with collecting the sample is the way that very
 * large datums are handled. We don't want to transfer multi-gigabyte
 * tuples from each segment. That would slow things down, and risk
 * running out of memory, if the sample contains a lot of them. They
 * are not very useful for statistics, anyway; hardly anyone builds an
 * index or does lookups where the histogram or MCV is meaningful for
 * very large keys. PostgreSQL also ignores any datums larger than
 * WIDTH_THRESHOLD (1kB) in the statistics computation, and we use the
 * same limit to restrict what gets transferred from the segments.
 * We substitute the very large datums with NULLs in the sample, but
 * keep track separately, which datums came out as NULLs because they
 * were too large, as opposed to "real" NULLs.
 *
 *
 * Merging leaf statistics with hyperloglog
 * ----------------------------------------
 *
 * TODO: explain how this works.
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/analyze.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/multixact.h"
#include "access/transam.h"
#include "access/tupconvert.h"
#include "access/tuptoaster.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_namespace.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/attoptcache.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/sortsupport.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"
#include "utils/tqual.h"

#include "catalog/heap.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "commands/analyzeutils.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "libpq-fe.h"
#include "utils/builtins.h"
#include "utils/hyperloglog/gp_hyperloglog.h"
#include "utils/snapmgr.h"


/*
 * For Hyperloglog, we define an error margin of 0.3%. If the number of
 * distinct values estimated by hyperloglog is within an error of 0.3%,
 * we consider everything as distinct.
 */
#define GP_HLL_ERROR_MARGIN  0.003

/* Data structure for Algorithm S from Knuth 3.4.2 */
typedef struct
{
	BlockNumber N;				/* number of blocks, known in advance */
	int			n;				/* desired sample size */
	BlockNumber t;				/* current block number */
	int			m;				/* blocks selected so far */
} BlockSamplerData;

typedef BlockSamplerData *BlockSampler;

/* Per-index data for ANALYZE */
typedef struct AnlIndexData
{
	IndexInfo  *indexInfo;		/* BuildIndexInfo result */
	double		tupleFract;		/* fraction of rows for partial index */
	VacAttrStats **vacattrstats;	/* index attrs to analyze */
	int			attr_cnt;
} AnlIndexData;


/* Default statistics target (GUC parameter) */
int			default_statistics_target = 100;

/* A few variables that don't seem worth passing around as parameters */
static MemoryContext anl_context = NULL;
static BufferAccessStrategy vac_strategy;

Bitmapset	**acquire_func_colLargeRowIndexes;


static void do_analyze_rel(Relation onerel, VacuumStmt *vacstmt,
			   AcquireSampleRowsFunc acquirefunc, BlockNumber relpages,
			   bool inh, bool in_outer_xact, int elevel);
static void BlockSampler_Init(BlockSampler bs, BlockNumber nblocks,
				  int samplesize);
static bool BlockSampler_HasMore(BlockSampler bs);
static BlockNumber BlockSampler_Next(BlockSampler bs);
static void compute_index_stats(Relation onerel, double totalrows,
					AnlIndexData *indexdata, int nindexes,
					HeapTuple *rows, int numrows,
					MemoryContext col_context);
static VacAttrStats *examine_attribute(Relation onerel, int attnum,
				  Node *index_expr, int elevel);
static int acquire_sample_rows_dispatcher(Relation onerel, bool inh, int elevel,
										  HeapTuple *rows, int targrows,
										  double *totalrows, double *totaldeadrows);
static BlockNumber acquire_number_of_blocks(Relation onerel);
static BlockNumber acquire_index_number_of_blocks(Relation indexrel, Relation tablerel);

static int	compare_rows(const void *a, const void *b);
static void update_attstats(Oid relid, bool inh,
				int natts, VacAttrStats **vacattrstats);
static Datum std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);
static Datum ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);

static void analyze_rel_internal(Oid relid, VacuumStmt *vacstmt,
			bool in_outer_xact, BufferAccessStrategy bstrategy);
static void acquire_hll_by_query(Relation onerel, int nattrs, VacAttrStats **attrstats, int elevel);

/*
 *	analyze_rel() -- analyze one relation
 */
void
analyze_rel(Oid relid, VacuumStmt *vacstmt,
			bool in_outer_xact, BufferAccessStrategy bstrategy)
{
	bool		optimizerBackup;

	/*
	 * Temporarily disable ORCA because it's slow to start up, and it
	 * wouldn't come up with any better plan for the simple queries that
	 * we run.
	 */
	optimizerBackup = optimizer;
	optimizer = false;

	PG_TRY();
	{
		analyze_rel_internal(relid, vacstmt, in_outer_xact, bstrategy);
	}
	/* Clean up in case of error. */
	PG_CATCH();
	{
		optimizer = optimizerBackup;

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	optimizer = optimizerBackup;
}

static void
analyze_rel_internal(Oid relid, VacuumStmt *vacstmt,
			bool in_outer_xact, BufferAccessStrategy bstrategy)
{
	Relation	onerel;
	int			elevel;
	AcquireSampleRowsFunc acquirefunc = NULL;
	BlockNumber relpages = 0;

	/* Select logging level */
	if (vacstmt->options & VACOPT_VERBOSE)
		elevel = INFO;
	else
		elevel = DEBUG2;

	/* Set up static variables */
	vac_strategy = bstrategy;

	/*
	 * Check for user-requested abort.
	 */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Open the relation, getting ShareUpdateExclusiveLock to ensure that two
	 * ANALYZEs don't run on it concurrently.  (This also locks out a
	 * concurrent VACUUM, which doesn't matter much at the moment but might
	 * matter if we ever try to accumulate stats on dead tuples.) If the rel
	 * has been dropped since we last saw it, we don't need to process it.
	 */
	if (!(vacstmt->options & VACOPT_NOWAIT))
		onerel = try_relation_open(relid, ShareUpdateExclusiveLock, false);
	else if (ConditionalLockRelationOid(relid, ShareUpdateExclusiveLock))
		onerel = try_relation_open(relid, NoLock, false);
	else
	{
		onerel = NULL;
		if (IsAutoVacuumWorkerProcess() && Log_autovacuum_min_duration >= 0)
			ereport(LOG,
					(errcode(ERRCODE_LOCK_NOT_AVAILABLE),
				  errmsg("skipping analyze of \"%s\" --- lock not available",
						 vacstmt->relation->relname)));
	}
	if (!onerel)
		return;

	/*
	 * Check permissions --- this should match vacuum's check!
	 */
	if (!(pg_class_ownercheck(RelationGetRelid(onerel), GetUserId()) ||
		  (pg_database_ownercheck(MyDatabaseId, GetUserId()) && !onerel->rd_rel->relisshared)))
	{
		/* No need for a WARNING if we already complained during VACUUM */
		if (!(vacstmt->options & VACOPT_VACUUM))
		{
			if (onerel->rd_rel->relisshared)
				ereport(WARNING,
				 (errmsg("skipping \"%s\" --- only superuser can analyze it",
						 RelationGetRelationName(onerel))));
			else if (onerel->rd_rel->relnamespace == PG_CATALOG_NAMESPACE)
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- only superuser or database owner can analyze it",
								RelationGetRelationName(onerel))));
			else
				ereport(WARNING,
						(errmsg("skipping \"%s\" --- only table or database owner can analyze it",
								RelationGetRelationName(onerel))));
		}
		relation_close(onerel, ShareUpdateExclusiveLock);
		return;
	}

	/*
	 * Silently ignore tables that are temp tables of other backends ---
	 * trying to analyze these is rather pointless, since their contents are
	 * probably not up-to-date on disk.  (We don't throw a warning here; it
	 * would just lead to chatter during a database-wide ANALYZE.)
	 */
	if (RELATION_IS_OTHER_TEMP(onerel))
	{
		relation_close(onerel, ShareUpdateExclusiveLock);
		return;
	}

	/*
	 * We can ANALYZE any table except pg_statistic. See update_attstats
	 */
	if (RelationGetRelid(onerel) == StatisticRelationId)
	{
		relation_close(onerel, ShareUpdateExclusiveLock);
		return;
	}

	/*
	 * Check that it's a plain table, materialized view, or foreign table; we
	 * used to do this in get_rel_oids() but seems safer to check after we've
	 * locked the relation.
	 */
	if ((onerel->rd_rel->relkind == RELKIND_RELATION ||
		 onerel->rd_rel->relkind == RELKIND_MATVIEW) &&
		!RelationIsExternal(onerel))
	{
		/* Regular table, so we'll use the regular row acquisition function */
		acquirefunc = acquire_sample_rows;

		/* Also get regular table's size */
		relpages = acquire_number_of_blocks(onerel);
	}
	else if (onerel->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
			 !RelationIsExternal(onerel))
	{
		/*
		 * For a foreign table, call the FDW's hook function to see whether it
		 * supports analysis.
		 */
		FdwRoutine *fdwroutine;
		bool		ok = false;

		fdwroutine = GetFdwRoutineForRelation(onerel, false);

		if (fdwroutine->AnalyzeForeignTable != NULL)
			ok = fdwroutine->AnalyzeForeignTable(onerel,
												 &acquirefunc,
												 &relpages);

		if (!ok)
		{
			ereport(WARNING,
			 (errmsg("skipping \"%s\" --- cannot analyze this foreign table",
					 RelationGetRelationName(onerel))));
			relation_close(onerel, ShareUpdateExclusiveLock);
			return;
		}
	}
	else
	{
		/* No need for a WARNING if we already complained during VACUUM */
		if (!(vacstmt->options & VACOPT_VACUUM))
			ereport(WARNING,
					(errmsg("skipping \"%s\" --- cannot analyze non-tables or special system tables",
							RelationGetRelationName(onerel))));
		relation_close(onerel, ShareUpdateExclusiveLock);
		return;
	}

	/*
	 * OK, let's do it.  First let other backends know I'm in ANALYZE.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
	MyPgXact->vacuumFlags |= PROC_IN_ANALYZE;
	LWLockRelease(ProcArrayLock);

	/*
	 * Do the normal non-recursive ANALYZE.
	 *
	 * Skip this for partitioned tables. A partitioned table, i.e. the
	 * "root partition", doesn't contain any rows.
	 */
	PartStatus ps = rel_part_status(relid);
	if (!(ps == PART_STATUS_ROOT || ps == PART_STATUS_INTERIOR))
		do_analyze_rel(onerel, vacstmt, acquirefunc, relpages,
					   false, in_outer_xact, elevel);

	/*
	 * If there are child tables, do recursive ANALYZE.
	 */
	if (onerel->rd_rel->relhassubclass)
		do_analyze_rel(onerel, vacstmt, acquirefunc, relpages,
					   true, in_outer_xact, elevel);

	/* MPP-6929: metadata tracking */
	if (!vacuumStatement_IsTemporary(onerel) && (Gp_role == GP_ROLE_DISPATCH))
	{
		char *asubtype = "";

		if (IsAutoVacuumWorkerProcess())
			asubtype = "AUTO";

		MetaTrackUpdObject(RelationRelationId,
						   RelationGetRelid(onerel),
						   GetUserId(),
						   "ANALYZE",
						   asubtype
			);
	}

	/*
	 * Close source relation now, but keep lock so that no one deletes it
	 * before we commit.  (If someone did, they'd fail to clean up the entries
	 * we made in pg_statistic.  Also, releasing the lock before commit would
	 * expose us to concurrent-update failures in update_attstats.)
	 */
	relation_close(onerel, NoLock);

	/*
	 * Reset my PGXACT flag.  Note: we need this here, and not in vacuum_rel,
	 * because the vacuum flag is cleared by the end-of-xact code.
	 */
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
	MyPgXact->vacuumFlags &= ~PROC_IN_ANALYZE;
	LWLockRelease(ProcArrayLock);
}

/*
 *	do_analyze_rel() -- analyze one relation, recursively or not
 *
 * Note that "acquirefunc" is only relevant for the non-inherited case.
 * If we supported foreign tables in inheritance trees,
 * acquire_inherited_sample_rows would need to determine the appropriate
 * acquirefunc for each child table.
 */
static void
do_analyze_rel(Relation onerel, VacuumStmt *vacstmt,
			   AcquireSampleRowsFunc acquirefunc, BlockNumber relpages,
			   bool inh, bool in_outer_xact, int elevel)
{
	int			attr_cnt,
				tcnt,
				i,
				ind;
	Relation   *Irel;
	int			nindexes;
	bool		hasindex;
	VacAttrStats **vacattrstats;
	AnlIndexData *indexdata;
	int			targrows,
				numrows;
	double		totalrows,
				totaldeadrows;
	HeapTuple  *rows;
	PGRUsage	ru0;
	TimestampTz starttime = 0;
	MemoryContext caller_context;
	Oid			save_userid;
	int			save_sec_context;
	int			save_nestlevel;
	Bitmapset **colLargeRowIndexes;
	bool		sample_needed;

	if (inh)
		ereport(elevel,
				(errmsg("analyzing \"%s.%s\" inheritance tree",
						get_namespace_name(RelationGetNamespace(onerel)),
						RelationGetRelationName(onerel))));
	else
		ereport(elevel,
				(errmsg("analyzing \"%s.%s\"",
						get_namespace_name(RelationGetNamespace(onerel)),
						RelationGetRelationName(onerel))));

	/*
	 * Set up a working context so that we can easily free whatever junk gets
	 * created.
	 */
	anl_context = AllocSetContextCreate(CurrentMemoryContext,
										"Analyze",
										ALLOCSET_DEFAULT_MINSIZE,
										ALLOCSET_DEFAULT_INITSIZE,
										ALLOCSET_DEFAULT_MAXSIZE);
	caller_context = MemoryContextSwitchTo(anl_context);

	/*
	 * Switch to the table owner's userid, so that any index functions are run
	 * as that user.  Also lock down security-restricted operations and
	 * arrange to make GUC variable changes local to this command.
	 */
	GetUserIdAndSecContext(&save_userid, &save_sec_context);
	SetUserIdAndSecContext(onerel->rd_rel->relowner,
						   save_sec_context | SECURITY_RESTRICTED_OPERATION);
	save_nestlevel = NewGUCNestLevel();

	/* measure elapsed time iff autovacuum logging requires it */
	if (IsAutoVacuumWorkerProcess() && Log_autovacuum_min_duration >= 0)
	{
		pg_rusage_init(&ru0);
		if (Log_autovacuum_min_duration > 0)
			starttime = GetCurrentTimestamp();
	}

	/*
	 * Determine which columns to analyze
	 *
	 * Note that system attributes are never analyzed, so we just reject them
	 * at the lookup stage.  We also reject duplicate column mentions.  (We
	 * could alternatively ignore duplicates, but analyzing a column twice
	 * won't work; we'd end up making a conflicting update in pg_statistic.)
	 */
	if (vacstmt->va_cols != NIL)
	{
		Bitmapset  *unique_cols = NULL;
		ListCell   *le;

		vacattrstats = (VacAttrStats **) palloc(list_length(vacstmt->va_cols) *
												sizeof(VacAttrStats *));
		tcnt = 0;
		foreach(le, vacstmt->va_cols)
		{
			char	   *col = strVal(lfirst(le));

			i = attnameAttNum(onerel, col, false);
			if (i == InvalidAttrNumber)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_COLUMN),
					errmsg("column \"%s\" of relation \"%s\" does not exist",
						   col, RelationGetRelationName(onerel))));
			if (bms_is_member(i, unique_cols))
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" of relation \"%s\" appears more than once",
								col, RelationGetRelationName(onerel))));
			unique_cols = bms_add_member(unique_cols, i);

			vacattrstats[tcnt] = examine_attribute(onerel, i, NULL, elevel);
			if (vacattrstats[tcnt] != NULL)
				tcnt++;
		}
		attr_cnt = tcnt;
	}
	else
	{
		attr_cnt = onerel->rd_att->natts;
		vacattrstats = (VacAttrStats **)
			palloc(attr_cnt * sizeof(VacAttrStats *));
		tcnt = 0;
		for (i = 1; i <= attr_cnt; i++)
		{
			vacattrstats[tcnt] = examine_attribute(onerel, i, NULL, elevel);
			if (vacattrstats[tcnt] != NULL)
				tcnt++;
		}
		attr_cnt = tcnt;
	}

	/*
	 * Open all indexes of the relation, and see if there are any analyzable
	 * columns in the indexes.  We do not analyze index columns if there was
	 * an explicit column list in the ANALYZE command, however.  If we are
	 * doing a recursive scan, we don't want to touch the parent's indexes at
	 * all.
	 */
	if (!inh)
		vac_open_indexes(onerel, AccessShareLock, &nindexes, &Irel);
	else
	{
		Irel = NULL;
		nindexes = 0;
	}
	hasindex = (nindexes > 0);
	indexdata = NULL;
	if (hasindex)
	{
		indexdata = (AnlIndexData *) palloc0(nindexes * sizeof(AnlIndexData));
		for (ind = 0; ind < nindexes; ind++)
		{
			AnlIndexData *thisdata = &indexdata[ind];
			IndexInfo  *indexInfo;

			thisdata->indexInfo = indexInfo = BuildIndexInfo(Irel[ind]);
			thisdata->tupleFract = 1.0; /* fix later if partial */
			if (indexInfo->ii_Expressions != NIL && vacstmt->va_cols == NIL)
			{
				ListCell   *indexpr_item = list_head(indexInfo->ii_Expressions);

				thisdata->vacattrstats = (VacAttrStats **)
					palloc(indexInfo->ii_NumIndexAttrs * sizeof(VacAttrStats *));
				tcnt = 0;
				for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
				{
					int			keycol = indexInfo->ii_KeyAttrNumbers[i];

					if (keycol == 0)
					{
						/* Found an index expression */
						Node	   *indexkey;

						if (indexpr_item == NULL)		/* shouldn't happen */
							elog(ERROR, "too few entries in indexprs list");
						indexkey = (Node *) lfirst(indexpr_item);
						indexpr_item = lnext(indexpr_item);
						thisdata->vacattrstats[tcnt] =
							examine_attribute(Irel[ind], i + 1, indexkey, elevel);
						if (thisdata->vacattrstats[tcnt] != NULL)
							tcnt++;
					}
				}
				thisdata->attr_cnt = tcnt;
			}
		}
	}

	/*
	 * Determine how many rows we need to sample, using the worst case from
	 * all analyzable columns.  We use a lower bound of 100 rows to avoid
	 * possible overflow in Vitter's algorithm.  (Note: that will also be the
	 * target in the corner case where there are no analyzable columns.)
	 */
	targrows = 100;
	for (i = 0; i < attr_cnt; i++)
	{
		if (targrows < vacattrstats[i]->minrows)
			targrows = vacattrstats[i]->minrows;
	}
	for (ind = 0; ind < nindexes; ind++)
	{
		AnlIndexData *thisdata = &indexdata[ind];

		for (i = 0; i < thisdata->attr_cnt; i++)
		{
			if (targrows < thisdata->vacattrstats[i]->minrows)
				targrows = thisdata->vacattrstats[i]->minrows;
		}
	}

	/*
	 * Maintain information if the row of a column exceeds WIDTH_THRESHOLD
	 */
	colLargeRowIndexes = (Bitmapset **) palloc0(sizeof(Bitmapset *) * onerel->rd_att->natts);

	if ((vacstmt->options & VACOPT_FULLSCAN) != 0)
	{
		if(rel_part_status(RelationGetRelid(onerel)) != PART_STATUS_ROOT)
		{
			acquire_hll_by_query(onerel, attr_cnt, vacattrstats, elevel);

			ereport(elevel, (errmsg("HLL FULL SCAN")));
		}
	}

	sample_needed = needs_sample(vacattrstats, attr_cnt);
	if (sample_needed)
	{
		rows = (HeapTuple *) palloc(targrows * sizeof(HeapTuple));

		/*
		 * Acquire the sample rows
		 *
		 * colLargeRowindexes is passed out-of-band, in a global variable,
		 * to avoid changing the function signature from upstream's.
		 */
		acquire_func_colLargeRowIndexes = colLargeRowIndexes;
		if (inh)
			numrows = acquire_inherited_sample_rows(onerel, elevel,
													rows, targrows,
													&totalrows, &totaldeadrows);
		else
			numrows = (*acquirefunc) (onerel, elevel,
									  rows, targrows,
									  &totalrows, &totaldeadrows);
		acquire_func_colLargeRowIndexes = NULL;
	}
	else
	{
		/* If we're just merging stats from leafs, these are not needed either */
		totalrows = 0;
		totaldeadrows = 0;
		numrows = 0;
		rows = NULL;
	}

	/*
	 * Compute the statistics.  Temporary results during the calculations for
	 * each column are stored in a child context.  The calc routines are
	 * responsible to make sure that whatever they store into the VacAttrStats
	 * structure is allocated in anl_context.
	 *
	 * When we have a root partition, we use the leaf partition statistics to
	 * derive root table statistics. In that case, we do not need to collect a
	 * sample. Therefore, the statistics calculation depends on root level have
	 * any tuples. In addition, we continue for statistics calculation if
	 * optimizer_analyze_root_partition or ROOTPARTITION is specified in the
	 * ANALYZE statement.
	 */
	if (numrows > 0 || !sample_needed)
	{
		HeapTuple *validRows = (HeapTuple *) palloc(numrows * sizeof(HeapTuple));
		MemoryContext col_context,
					old_context;

		col_context = AllocSetContextCreate(anl_context,
											"Analyze Column",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);
		old_context = MemoryContextSwitchTo(col_context);

		for (i = 0; i < attr_cnt; i++)
		{
			VacAttrStats *stats = vacattrstats[i];
			/*
			 * utilize hyperloglog and merge utilities to derive
			 * root table statistics by directly calling merge_leaf_stats()
			 * if all leaf partition attributes are analyzed
			 */
			if(stats->merge_stats)
			{
				(*stats->compute_stats) (stats, std_fetch_func, 0, 0);
				MemoryContextResetAndDeleteChildren(col_context);
				continue;
			}
			Assert(sample_needed);

			Bitmapset  *rowIndexes = colLargeRowIndexes[i];
			int			validRowsLength;

			/* If there are too wide rows in the sample, remove them
			 * from the sample being sent for stats collection
			 */
			if (rowIndexes)
			{
				validRowsLength = 0;
				for (int rownum = 0; rownum < numrows; rownum++)
				{
					/* if row is too wide, leave it out of the sample */
					if (bms_is_member(rownum, rowIndexes))
						continue;

					validRows[validRowsLength] = rows[rownum];
					validRowsLength++;
				}
				stats->rows = validRows;
			}
			else
			{
				stats->rows = rows;
				validRowsLength = numrows;
			}
			AttributeOpts *aopt =
			get_attribute_options(onerel->rd_id, stats->attr->attnum);

			stats->tupDesc = onerel->rd_att;

			if (validRowsLength > 0)
			{
				(*stats->compute_stats) (stats,
										 std_fetch_func,
										 validRowsLength, // numbers of rows in sample excluding toowide if any.
										 totalrows);
				/*
				 * Store HLL/HLL fullscan information for leaf partitions in
				 * the stats object
				 */
				if (rel_part_status(stats->attr->attrelid) == PART_STATUS_LEAF)
				{
					MemoryContext old_context;
					Datum *hll_values;

					old_context = MemoryContextSwitchTo(stats->anl_context);
					hll_values = (Datum *) palloc(sizeof(Datum));
					int16 hll_length = 0;
					int16 stakind = 0;
					if(stats->stahll_full != NULL)
					{
						hll_length = datumGetSize(PointerGetDatum(stats->stahll_full), false, -1);
						hll_values[0] = datumCopy(PointerGetDatum(stats->stahll_full), false, hll_length);
						stakind = STATISTIC_KIND_FULLHLL;
					}
					else if(stats->stahll != NULL)
					{
						((GpHLLCounter) (stats->stahll))->relPages = relpages;
						((GpHLLCounter) (stats->stahll))->relTuples = totalrows;

						hll_length = gp_hyperloglog_len((GpHLLCounter)stats->stahll);
						hll_values[0] = datumCopy(PointerGetDatum(stats->stahll), false, hll_length);
						stakind = STATISTIC_KIND_HLL;
					}
					MemoryContextSwitchTo(old_context);
					if (stakind > 0)
					{
						stats->stakind[STATISTIC_NUM_SLOTS-1] = stakind;
						stats->stavalues[STATISTIC_NUM_SLOTS-1] = hll_values;
						stats->numvalues[STATISTIC_NUM_SLOTS-1] =  1;
						stats->statyplen[STATISTIC_NUM_SLOTS-1] = hll_length;
					}
				}
			}
			else
			{
				// All the rows were too wide to be included in the sample. We cannot
				// do much in that case, but at least we know there were no NULLs, and
				// that every item was >= WIDTH_THRESHOLD in width.
				stats->stats_valid = true;
				stats->stanullfrac = 0.0;
				stats->stawidth = WIDTH_THRESHOLD;
				stats->stadistinct = 0.0;		/* "unknown" */
			}
			stats->rows = rows; // Reset to original rows

			/*
			 * If the appropriate flavor of the n_distinct option is
			 * specified, override with the corresponding value.
			 */
			aopt = get_attribute_options(onerel->rd_id, stats->attr->attnum);
			if (aopt != NULL)
			{
				float8		n_distinct;

				n_distinct = inh ? aopt->n_distinct_inherited : aopt->n_distinct;
				if (n_distinct != 0.0)
					stats->stadistinct = n_distinct;
			}

			MemoryContextResetAndDeleteChildren(col_context);
		}

		/*
		 * Datums exceeding WIDTH_THRESHOLD are masked as NULL in the sample, and
		 * are used as is to evaluate index statistics. It is less likely to have
		 * indexes on very wide columns, so the effect will be minimal.
		 */
		if (hasindex)
			compute_index_stats(onerel, totalrows,
								indexdata, nindexes,
								rows, numrows,
								col_context);

		MemoryContextSwitchTo(old_context);
		MemoryContextDelete(col_context);

		/*
		 * Emit the completed stats rows into pg_statistic, replacing any
		 * previous statistics for the target columns.  (If there are stats in
		 * pg_statistic for columns we didn't process, we leave them alone.)
		 */
		update_attstats(RelationGetRelid(onerel), inh,
						attr_cnt, vacattrstats);

		for (ind = 0; ind < nindexes; ind++)
		{
			AnlIndexData *thisdata = &indexdata[ind];

			update_attstats(RelationGetRelid(Irel[ind]), false,
							thisdata->attr_cnt, thisdata->vacattrstats);
		}
	}

	/*
	 * Update pages/tuples stats in pg_class ... but not if we're doing
	 * inherited stats.
	 *
	 * GPDB_92_MERGE_FIXME: In postgres it is sufficient to check the number of
	 * pages that are visible with visibilitymap_count(), but in GPDB this
	 * needs to be the count of all pages marked all visible across the all the
	 * QEs. We need to gather this information from the segments and then update
	 * it here.
	 */
	if (!inh)
		vac_update_relstats(onerel,
							relpages,
							totalrows,
							visibilitymap_count(onerel),
							hasindex,
							InvalidTransactionId,
							InvalidMultiXactId,
							in_outer_xact,
							false /* isvacuum */);

	/*
	 * Same for indexes. Vacuum always scans all indexes, so if we're part of
	 * VACUUM ANALYZE, don't overwrite the accurate count already inserted by
	 * VACUUM.
	 */
	if (!inh && !(vacstmt->options & VACOPT_VACUUM))
	{
		for (ind = 0; ind < nindexes; ind++)
		{
			AnlIndexData *thisdata = &indexdata[ind];
			double		totalindexrows;
			BlockNumber	estimatedIndexPages;

			if (totalrows < 1.0)
			{
				/**
				 * If there are no rows in the relation, no point trying to estimate
				 * number of pages in the index.
				 */
				elog(elevel, "ANALYZE skipping index %s since relation %s has no rows.",
					 RelationGetRelationName(Irel[ind]), RelationGetRelationName(onerel));
				estimatedIndexPages = 1;
			}
			else
			{
				/**
				 * NOTE: we don't attempt to estimate the number of tuples in an index.
				 * We will assume it to be equal to the estimated number of tuples in the relation.
				 * This does not hold for partial indexes. The number of tuples matching will be
				 * derived in selfuncs.c using the base table statistics.
				 */
				estimatedIndexPages = acquire_index_number_of_blocks(Irel[ind], onerel);
				elog(elevel, "ANALYZE estimated relpages=%u for index %s",
					 estimatedIndexPages, RelationGetRelationName(Irel[ind]));
			}

			totalindexrows = ceil(thisdata->tupleFract * totalrows);
			vac_update_relstats(Irel[ind],
								estimatedIndexPages,
								totalindexrows,
								0,
								false,
								InvalidTransactionId,
								InvalidMultiXactId,
								in_outer_xact,
								false /* isvacuum */);
		}
	}

	/*
	 * Report ANALYZE to the stats collector, too.  However, if doing
	 * inherited stats we shouldn't report, because the stats collector only
	 * tracks per-table stats.  Reset the changes_since_analyze counter only
	 * if we analyzed all columns; otherwise, there is still work for
	 * auto-analyze to do.
	 */
	if (!inh)
		pgstat_report_analyze(onerel, totalrows, totaldeadrows,
							  (vacstmt->va_cols == NIL));

	/* If this isn't part of VACUUM ANALYZE, let index AMs do cleanup */
	if (!(vacstmt->options & VACOPT_VACUUM))
	{
		for (ind = 0; ind < nindexes; ind++)
		{
			IndexBulkDeleteResult *stats;
			IndexVacuumInfo ivinfo;

			ivinfo.index = Irel[ind];
			ivinfo.analyze_only = true;
			ivinfo.estimated_count = true;
			ivinfo.message_level = elevel;
			ivinfo.num_heap_tuples = onerel->rd_rel->reltuples;
			ivinfo.strategy = vac_strategy;

			stats = index_vacuum_cleanup(&ivinfo, NULL);

			if (stats)
				pfree(stats);
		}
	}

	/* Done with indexes */
	vac_close_indexes(nindexes, Irel, NoLock);

	/* Log the action if appropriate */
	if (IsAutoVacuumWorkerProcess() && Log_autovacuum_min_duration >= 0)
	{
		if (Log_autovacuum_min_duration == 0 ||
			TimestampDifferenceExceeds(starttime, GetCurrentTimestamp(),
									   Log_autovacuum_min_duration))
			ereport(LOG,
					(errmsg("automatic analyze of table \"%s.%s.%s\" system usage: %s",
							get_database_name(MyDatabaseId),
							get_namespace_name(RelationGetNamespace(onerel)),
							RelationGetRelationName(onerel),
							pg_rusage_show(&ru0))));
	}

	/* Roll back any GUC changes executed by index functions */
	AtEOXact_GUC(false, save_nestlevel);

	/* Restore userid and security context */
	SetUserIdAndSecContext(save_userid, save_sec_context);

	/* Restore current context and release memory */
	MemoryContextSwitchTo(caller_context);
	MemoryContextDelete(anl_context);
	anl_context = NULL;
}

/*
 * Compute statistics about indexes of a relation
 */
static void
compute_index_stats(Relation onerel, double totalrows,
					AnlIndexData *indexdata, int nindexes,
					HeapTuple *rows, int numrows,
					MemoryContext col_context)
{
	MemoryContext ind_context,
				old_context;
	Datum		values[INDEX_MAX_KEYS];
	bool		isnull[INDEX_MAX_KEYS];
	int			ind,
				i;

	ind_context = AllocSetContextCreate(anl_context,
										"Analyze Index",
										ALLOCSET_DEFAULT_MINSIZE,
										ALLOCSET_DEFAULT_INITSIZE,
										ALLOCSET_DEFAULT_MAXSIZE);
	old_context = MemoryContextSwitchTo(ind_context);

	for (ind = 0; ind < nindexes; ind++)
	{
		AnlIndexData *thisdata = &indexdata[ind];
		IndexInfo  *indexInfo = thisdata->indexInfo;
		int			attr_cnt = thisdata->attr_cnt;
		TupleTableSlot *slot;
		EState	   *estate;
		ExprContext *econtext;
		List	   *predicate;
		Datum	   *exprvals;
		bool	   *exprnulls;
		int			numindexrows,
					tcnt,
					rowno;
		double		totalindexrows;

		/* Ignore index if no columns to analyze and not partial */
		if (attr_cnt == 0 && indexInfo->ii_Predicate == NIL)
			continue;

		/*
		 * Need an EState for evaluation of index expressions and
		 * partial-index predicates.  Create it in the per-index context to be
		 * sure it gets cleaned up at the bottom of the loop.
		 */
		estate = CreateExecutorState();
		econtext = GetPerTupleExprContext(estate);
		/* Need a slot to hold the current heap tuple, too */
		slot = MakeSingleTupleTableSlot(RelationGetDescr(onerel));

		/* Arrange for econtext's scan tuple to be the tuple under test */
		econtext->ecxt_scantuple = slot;

		/* Set up execution state for predicate. */
		predicate = (List *)
			ExecPrepareExpr((Expr *) indexInfo->ii_Predicate,
							estate);

		/* Compute and save index expression values */
		exprvals = (Datum *) palloc(numrows * attr_cnt * sizeof(Datum));
		exprnulls = (bool *) palloc(numrows * attr_cnt * sizeof(bool));
		numindexrows = 0;
		tcnt = 0;
		for (rowno = 0; rowno < numrows; rowno++)
		{
			HeapTuple	heapTuple = rows[rowno];

			vacuum_delay_point();

			/*
			 * Reset the per-tuple context each time, to reclaim any cruft
			 * left behind by evaluating the predicate or index expressions.
			 */
			ResetExprContext(econtext);

			/* Set up for predicate or expression evaluation */
			ExecStoreHeapTuple(heapTuple, slot, InvalidBuffer, false);

			/* If index is partial, check predicate */
			if (predicate != NIL)
			{
				if (!ExecQual(predicate, econtext, false))
					continue;
			}
			numindexrows++;

			if (attr_cnt > 0)
			{
				/*
				 * Evaluate the index row to compute expression values. We
				 * could do this by hand, but FormIndexDatum is convenient.
				 */
				FormIndexDatum(indexInfo,
							   slot,
							   estate,
							   values,
							   isnull);

				/*
				 * Save just the columns we care about.  We copy the values
				 * into ind_context from the estate's per-tuple context.
				 */
				for (i = 0; i < attr_cnt; i++)
				{
					VacAttrStats *stats = thisdata->vacattrstats[i];
					int			attnum = stats->attr->attnum;

					if (isnull[attnum - 1])
					{
						exprvals[tcnt] = (Datum) 0;
						exprnulls[tcnt] = true;
					}
					else
					{
						exprvals[tcnt] = datumCopy(values[attnum - 1],
												   stats->attrtype->typbyval,
												   stats->attrtype->typlen);
						exprnulls[tcnt] = false;
					}
					tcnt++;
				}
			}
		}

		/*
		 * Having counted the number of rows that pass the predicate in the
		 * sample, we can estimate the total number of rows in the index.
		 */
		thisdata->tupleFract = (double) numindexrows / (double) numrows;
		totalindexrows = ceil(thisdata->tupleFract * totalrows);

		/*
		 * Now we can compute the statistics for the expression columns.
		 */
		if (numindexrows > 0)
		{
			MemoryContextSwitchTo(col_context);
			for (i = 0; i < attr_cnt; i++)
			{
				VacAttrStats *stats = thisdata->vacattrstats[i];
				AttributeOpts *aopt =
				get_attribute_options(stats->attr->attrelid,
									  stats->attr->attnum);

				stats->exprvals = exprvals + i;
				stats->exprnulls = exprnulls + i;
				stats->rowstride = attr_cnt;
				(*stats->compute_stats) (stats,
										 ind_fetch_func,
										 numindexrows,
										 totalindexrows);

				/*
				 * If the n_distinct option is specified, it overrides the
				 * above computation.  For indices, we always use just
				 * n_distinct, not n_distinct_inherited.
				 */
				if (aopt != NULL && aopt->n_distinct != 0.0)
					stats->stadistinct = aopt->n_distinct;

				MemoryContextResetAndDeleteChildren(col_context);
			}
		}

		/* And clean up */
		MemoryContextSwitchTo(ind_context);

		ExecDropSingleTupleTableSlot(slot);
		FreeExecutorState(estate);
		MemoryContextResetAndDeleteChildren(ind_context);
	}

	MemoryContextSwitchTo(old_context);
	MemoryContextDelete(ind_context);
}

/*
 * examine_attribute -- pre-analysis of a single column
 *
 * Determine whether the column is analyzable; if so, create and initialize
 * a VacAttrStats struct for it.  If not, return NULL.
 *
 * If index_expr isn't NULL, then we're trying to analyze an expression index,
 * and index_expr is the expression tree representing the column's data.
 */
static VacAttrStats *
examine_attribute(Relation onerel, int attnum, Node *index_expr, int elevel)
{
	Form_pg_attribute attr = onerel->rd_att->attrs[attnum - 1];
	HeapTuple	typtuple;
	VacAttrStats *stats;
	int			i;
	bool		ok;

	/* Never analyze dropped columns */
	if (attr->attisdropped)
		return NULL;

	/* Don't analyze column if user has specified not to */
	if (attr->attstattarget == 0)
		return NULL;

	/*
	 * Create the VacAttrStats struct.  Note that we only have a copy of the
	 * fixed fields of the pg_attribute tuple.
	 */
	stats = (VacAttrStats *) palloc0(sizeof(VacAttrStats));
	stats->elevel = elevel;
	stats->attr = (Form_pg_attribute) palloc(ATTRIBUTE_FIXED_PART_SIZE);
	memcpy(stats->attr, attr, ATTRIBUTE_FIXED_PART_SIZE);

	/*
	 * When analyzing an expression index, believe the expression tree's type
	 * not the column datatype --- the latter might be the opckeytype storage
	 * type of the opclass, which is not interesting for our purposes.  (Note:
	 * if we did anything with non-expression index columns, we'd need to
	 * figure out where to get the correct type info from, but for now that's
	 * not a problem.)	It's not clear whether anyone will care about the
	 * typmod, but we store that too just in case.
	 */
	if (index_expr)
	{
		stats->attrtypid = exprType(index_expr);
		stats->attrtypmod = exprTypmod(index_expr);
	}
	else
	{
		stats->attrtypid = attr->atttypid;
		stats->attrtypmod = attr->atttypmod;
	}

	typtuple = SearchSysCacheCopy1(TYPEOID,
								   ObjectIdGetDatum(stats->attrtypid));
	if (!HeapTupleIsValid(typtuple))
		elog(ERROR, "cache lookup failed for type %u", stats->attrtypid);
	stats->attrtype = (Form_pg_type) GETSTRUCT(typtuple);
	stats->relstorage = RelationGetForm(onerel)->relstorage;
	stats->anl_context = anl_context;
	stats->tupattnum = attnum;

	/*
	 * The fields describing the stats->stavalues[n] element types default to
	 * the type of the data being analyzed, but the type-specific typanalyze
	 * function can change them if it wants to store something else.
	 */
	for (i = 0; i < STATISTIC_NUM_SLOTS-1; i++)
	{
		stats->statypid[i] = stats->attrtypid;
		stats->statyplen[i] = stats->attrtype->typlen;
		stats->statypbyval[i] = stats->attrtype->typbyval;
		stats->statypalign[i] = stats->attrtype->typalign;
	}

	/*
	 * The last slots of statistics is reserved for hyperloglog counter which
	 * is saved as a bytea. Therefore the type information is hardcoded for the
	 * bytea.
	 */
	stats->statypid[i] = BYTEAOID; // oid for bytea
	stats->statyplen[i] = -1; // variable length type
	stats->statypbyval[i] = false; // bytea is pass by reference
	stats->statypalign[i] = 'i'; // INT alignment (4-byte)

	/*
	 * Call the type-specific typanalyze function.  If none is specified, use
	 * std_typanalyze().
	 */
	if (OidIsValid(stats->attrtype->typanalyze))
		ok = DatumGetBool(OidFunctionCall1(stats->attrtype->typanalyze,
										   PointerGetDatum(stats)));
	else
		ok = std_typanalyze(stats);

	if (!ok || stats->compute_stats == NULL || stats->minrows <= 0)
	{
		heap_freetuple(typtuple);
		pfree(stats->attr);
		pfree(stats);
		return NULL;
	}

	return stats;
}

/*
 * BlockSampler_Init -- prepare for random sampling of blocknumbers
 *
 * BlockSampler is used for stage one of our new two-stage tuple
 * sampling mechanism as discussed on pgsql-hackers 2004-04-02 (subject
 * "Large DB").  It selects a random sample of samplesize blocks out of
 * the nblocks blocks in the table.  If the table has less than
 * samplesize blocks, all blocks are selected.
 *
 * Since we know the total number of blocks in advance, we can use the
 * straightforward Algorithm S from Knuth 3.4.2, rather than Vitter's
 * algorithm.
 */
static void
BlockSampler_Init(BlockSampler bs, BlockNumber nblocks, int samplesize)
{
	bs->N = nblocks;			/* measured table size */

	/*
	 * If we decide to reduce samplesize for tables that have less or not much
	 * more than samplesize blocks, here is the place to do it.
	 */
	bs->n = samplesize;
	bs->t = 0;					/* blocks scanned so far */
	bs->m = 0;					/* blocks selected so far */
}

static bool
BlockSampler_HasMore(BlockSampler bs)
{
	return (bs->t < bs->N) && (bs->m < bs->n);
}

static BlockNumber
BlockSampler_Next(BlockSampler bs)
{
	BlockNumber K = bs->N - bs->t;		/* remaining blocks */
	int			k = bs->n - bs->m;		/* blocks still to sample */
	double		p;				/* probability to skip block */
	double		V;				/* random */

	Assert(BlockSampler_HasMore(bs));	/* hence K > 0 and k > 0 */

	if ((BlockNumber) k >= K)
	{
		/* need all the rest */
		bs->m++;
		return bs->t++;
	}

	/*----------
	 * It is not obvious that this code matches Knuth's Algorithm S.
	 * Knuth says to skip the current block with probability 1 - k/K.
	 * If we are to skip, we should advance t (hence decrease K), and
	 * repeat the same probabilistic test for the next block.  The naive
	 * implementation thus requires an anl_random_fract() call for each block
	 * number.  But we can reduce this to one anl_random_fract() call per
	 * selected block, by noting that each time the while-test succeeds,
	 * we can reinterpret V as a uniform random number in the range 0 to p.
	 * Therefore, instead of choosing a new V, we just adjust p to be
	 * the appropriate fraction of its former value, and our next loop
	 * makes the appropriate probabilistic test.
	 *
	 * We have initially K > k > 0.  If the loop reduces K to equal k,
	 * the next while-test must fail since p will become exactly zero
	 * (we assume there will not be roundoff error in the division).
	 * (Note: Knuth suggests a "<=" loop condition, but we use "<" just
	 * to be doubly sure about roundoff error.)  Therefore K cannot become
	 * less than k, which means that we cannot fail to select enough blocks.
	 *----------
	 */
	V = anl_random_fract();
	p = 1.0 - (double) k / (double) K;
	while (V < p)
	{
		/* skip */
		bs->t++;
		K--;					/* keep K == N - t */

		/* adjust p to be new cutoff point in reduced range */
		p *= 1.0 - (double) k / (double) K;
	}

	/* select */
	bs->m++;
	return bs->t++;
}

/*
 * acquire_sample_rows -- acquire a random sample of rows from the table
 *
 * Selected rows are returned in the caller-allocated array rows[], which
 * must have at least targrows entries.
 * The actual number of rows selected is returned as the function result.
 * We also estimate the total numbers of live and dead rows in the table,
 * and return them into *totalrows and *totaldeadrows, respectively.
 *
 * The returned list of tuples is in order by physical position in the table.
 * (We will rely on this later to derive correlation estimates.)
 *
 * As of May 2004 we use a new two-stage method:  Stage one selects up
 * to targrows random blocks (or all blocks, if there aren't so many).
 * Stage two scans these blocks and uses the Vitter algorithm to create
 * a random sample of targrows rows (or less, if there are less in the
 * sample of blocks).  The two stages are executed simultaneously: each
 * block is processed as soon as stage one returns its number and while
 * the rows are read stage two controls which ones are to be inserted
 * into the sample.
 *
 * Although every row has an equal chance of ending up in the final
 * sample, this sampling method is not perfect: not every possible
 * sample has an equal chance of being selected.  For large relations
 * the number of different blocks represented by the sample tends to be
 * too small.  We can live with that for now.  Improvements are welcome.
 *
 * An important property of this sampling method is that because we do
 * look at a statistically unbiased set of blocks, we should get
 * unbiased estimates of the average numbers of live and dead rows per
 * block.  The previous sampling method put too much credence in the row
 * density near the start of the table.
 *
 * The returned list of tuples is in order by physical position in the table.
 * (We will rely on this later to derive correlation estimates.)
 */
static int
acquire_sample_rows_heap(Relation onerel, int elevel,
						 HeapTuple *rows, int targrows,
						 double *totalrows, double *totaldeadrows)
{
	int			numrows = 0;	/* # rows now in reservoir */
	double		samplerows = 0; /* total # rows collected */
	double		liverows = 0;	/* # live rows seen */
	double		deadrows = 0;	/* # dead rows seen */
	double		rowstoskip = -1;	/* -1 means not set yet */
	BlockNumber totalblocks;
	TransactionId OldestXmin;
	BlockSamplerData bs;
	double		rstate;

	Assert(targrows > 0);

	totalblocks = RelationGetNumberOfBlocks(onerel);

	/* Need a cutoff xmin for HeapTupleSatisfiesVacuum */
	OldestXmin = GetOldestXmin(onerel, true);

	/* Prepare for sampling block numbers */
	BlockSampler_Init(&bs, totalblocks, targrows);
	/* Prepare for sampling rows */
	rstate = anl_init_selection_state(targrows);

	/* Outer loop over blocks to sample */
	while (BlockSampler_HasMore(&bs))
	{
		BlockNumber targblock = BlockSampler_Next(&bs);
		Buffer		targbuffer;
		Page		targpage;
		OffsetNumber targoffset,
					maxoffset;

		vacuum_delay_point();

		/*
		 * We must maintain a pin on the target page's buffer to ensure that
		 * the maxoffset value stays good (else concurrent VACUUM might delete
		 * tuples out from under us).  Hence, pin the page until we are done
		 * looking at it.  We also choose to hold sharelock on the buffer
		 * throughout --- we could release and re-acquire sharelock for each
		 * tuple, but since we aren't doing much work per tuple, the extra
		 * lock traffic is probably better avoided.
		 */
		targbuffer = ReadBufferExtended(onerel, MAIN_FORKNUM, targblock,
										RBM_NORMAL, vac_strategy);
		LockBuffer(targbuffer, BUFFER_LOCK_SHARE);
		targpage = BufferGetPage(targbuffer);
		maxoffset = PageGetMaxOffsetNumber(targpage);

		/* Inner loop over all tuples on the selected page */
		for (targoffset = FirstOffsetNumber; targoffset <= maxoffset; targoffset++)
		{
			ItemId		itemid;
			HeapTupleData targtuple;
			bool		sample_it = false;

			itemid = PageGetItemId(targpage, targoffset);

			/*
			 * We ignore unused and redirect line pointers.  DEAD line
			 * pointers should be counted as dead, because we need vacuum to
			 * run to get rid of them.  Note that this rule agrees with the
			 * way that heap_page_prune() counts things.
			 */
			if (!ItemIdIsNormal(itemid))
			{
				if (ItemIdIsDead(itemid))
					deadrows += 1;
				continue;
			}

			ItemPointerSet(&targtuple.t_self, targblock, targoffset);

#if 0
			targtuple.t_tableOid = RelationGetRelid(onerel);
#endif
			targtuple.t_data = (HeapTupleHeader) PageGetItem(targpage, itemid);
			targtuple.t_len = ItemIdGetLength(itemid);

			switch (HeapTupleSatisfiesVacuum(onerel, &targtuple,
											 OldestXmin,
											 targbuffer))
			{
				case HEAPTUPLE_LIVE:
					sample_it = true;
					liverows += 1;
					break;

				case HEAPTUPLE_DEAD:
				case HEAPTUPLE_RECENTLY_DEAD:
					/* Count dead and recently-dead rows */
					deadrows += 1;
					break;

				case HEAPTUPLE_INSERT_IN_PROGRESS:

					/*
					 * Insert-in-progress rows are not counted.  We assume
					 * that when the inserting transaction commits or aborts,
					 * it will send a stats message to increment the proper
					 * count.  This works right only if that transaction ends
					 * after we finish analyzing the table; if things happen
					 * in the other order, its stats update will be
					 * overwritten by ours.  However, the error will be large
					 * only if the other transaction runs long enough to
					 * insert many tuples, so assuming it will finish after us
					 * is the safer option.
					 *
					 * A special case is that the inserting transaction might
					 * be our own.  In this case we should count and sample
					 * the row, to accommodate users who load a table and
					 * analyze it in one transaction.  (pgstat_report_analyze
					 * has to adjust the numbers we send to the stats
					 * collector to make this come out right.)
					 */
					if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(targtuple.t_data)))
					{
						sample_it = true;
						liverows += 1;
					}
					break;

				case HEAPTUPLE_DELETE_IN_PROGRESS:

					/*
					 * We count and sample delete-in-progress rows the same as
					 * live ones, so that the stats counters come out right if
					 * the deleting transaction commits after us, per the same
					 * reasoning given above.
					 *
					 * If the delete was done by our own transaction, however,
					 * we must count the row as dead to make
					 * pgstat_report_analyze's stats adjustments come out
					 * right.  (Note: this works out properly when the row was
					 * both inserted and deleted in our xact.)
					 *
					 * The net effect of these choices is that we act as
					 * though an IN_PROGRESS transaction hasn't happened yet,
					 * except if it is our own transaction, which we assume
					 * has happened.
					 *
					 * This approach ensures that we behave sanely if we see
					 * both the pre-image and post-image rows for a row being
					 * updated by a concurrent transaction: we will sample the
					 * pre-image but not the post-image.  We also get sane
					 * results if the concurrent transaction never commits.
					 */
					if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(targtuple.t_data)))
						deadrows += 1;
					else
					{
						sample_it = true;
						liverows += 1;
					}
					break;

				default:
					elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
					break;
			}

			if (sample_it)
			{
				/*
				 * The first targrows sample rows are simply copied into the
				 * reservoir. Then we start replacing tuples in the sample
				 * until we reach the end of the relation.  This algorithm is
				 * from Jeff Vitter's paper (see full citation in
				 * utils/misc/sampling.c). It works by repeatedly computing
				 * the number of tuples to skip before selecting a tuple,
				 * which replaces a randomly chosen element of the reservoir
				 * (current set of tuples).  At all times the reservoir is a
				 * true random sample of the tuples we've passed over so far,
				 * so when we fall off the end of the relation we're done.
				 */
				if (numrows < targrows)
					rows[numrows++] = heap_copytuple(&targtuple);
				else
				{
					/*
					 * t in Vitter's paper is the number of records already
					 * processed.  If we need to compute a new S value, we
					 * must use the not-yet-incremented value of samplerows as
					 * t.
					 */
					if (rowstoskip < 0)
						rowstoskip = anl_get_next_S(samplerows, targrows,
													&rstate);

					if (rowstoskip <= 0)
					{
						/*
						 * Found a suitable tuple, so save it, replacing one
						 * old tuple at random
						 */
						int			k = (int) (targrows * anl_random_fract());

						Assert(k >= 0 && k < targrows);
						heap_freetuple(rows[k]);
						rows[k] = heap_copytuple(&targtuple);
					}

					rowstoskip -= 1;
				}

				samplerows += 1;
			}
		}

		/* Now release the lock and pin on the page */
		UnlockReleaseBuffer(targbuffer);
	}

	/*
	 * If we didn't find as many tuples as we wanted then we're done. No sort
	 * is needed, since they're already in order.
	 *
	 * Otherwise we need to sort the collected tuples by position
	 * (itempointer). It's not worth worrying about corner cases where the
	 * tuples are already sorted.
	 */
	if (numrows == targrows)
		qsort((void *) rows, numrows, sizeof(HeapTuple), compare_rows);

	/*
	 * Estimate total numbers of live and dead rows in relation, extrapolating
	 * on the assumption that the average tuple density in pages we didn't
	 * scan is the same as in the pages we did scan.  Since what we scanned is
	 * a random sample of the pages in the relation, this should be a good
	 * assumption.
	 */
	if (bs.m > 0)
	{
		*totalrows = floor((liverows / bs.m) * totalblocks + 0.5);
		*totaldeadrows = floor((deadrows / bs.m) * totalblocks + 0.5);
	}
	else
	{
		*totalrows = 0.0;
		*totaldeadrows = 0.0;
	}

	/*
	 * Emit some interesting relation info
	 */
	ereport(elevel,
			(errmsg("\"%s\": scanned %d of %u pages, "
					"containing %.0f live rows and %.0f dead rows; "
					"%d rows in sample, %.0f estimated total rows",
					RelationGetRelationName(onerel),
					bs.m, totalblocks,
					liverows, deadrows,
					numrows, *totalrows)));

	return numrows;
}

/*
 * Collect a sample of rows from an AO or AOCS table.
 *
 * The block-sampling method used for heap tables doesn't work with
 * append-only tables. We could build a reasonably efficient block-sampling
 * method for AO tables, too, using the block directory, if it's available.
 * But for now, this scans the whole table.
 */
static int
acquire_sample_rows_ao(Relation onerel, int elevel,
					   HeapTuple *rows, int targrows,
					   double *totalrows, double *totaldeadrows)
{
	AppendOnlyScanDesc aoScanDesc = NULL;
	AOCSScanDesc aocsScanDesc = NULL;
	Snapshot	appendOnlyMetaDataSnapshot;
	double		rstate;
	TupleTableSlot *slot;
	int			numrows = 0;	/* # rows now in reservoir */
	double		samplerows = 0; /* total # rows collected */
	double		rowstoskip = -1;	/* -1 means not set yet */

	/*
	 * the append-only meta data should never be fetched with
	 * SnapshotAny as bogus results are returned.
	 */
	appendOnlyMetaDataSnapshot = GetTransactionSnapshot();

	if (RelationIsAoRows(onerel))
		aoScanDesc = appendonly_beginscan(onerel,
										  SnapshotSelf,
										  appendOnlyMetaDataSnapshot,
										  0, NULL);
	else
	{
		int			natts = RelationGetNumberOfAttributes(onerel);
		bool	   *proj = (bool *) palloc(natts * sizeof(bool));
		int			i;

		for(i = 0; i < natts; i++)
			proj[i] = true;

		Assert(RelationIsAoCols(onerel));
		aocsScanDesc = aocs_beginscan(onerel,
									  SnapshotSelf,
									  appendOnlyMetaDataSnapshot,
									  RelationGetDescr(onerel), proj);
	}
	slot = MakeSingleTupleTableSlot(RelationGetDescr(onerel));

	/* Prepare for sampling rows */
	rstate = anl_init_selection_state(targrows);

	for (;;)
	{
		if (aoScanDesc)
			appendonly_getnext(aoScanDesc, ForwardScanDirection, slot);
		else
			aocs_getnext(aocsScanDesc, ForwardScanDirection, slot);

		if (TupIsNull(slot))
			break;

		if (rowstoskip < 0)
			rowstoskip = anl_get_next_S(samplerows, targrows,
										&rstate);

		/* XXX: this is copied from acquire_sample_rows_heap */

		/*
		 * The first targrows sample rows are simply copied into the
		 * reservoir. Then we start replacing tuples in the sample
		 * until we reach the end of the relation.	This algorithm is
		 * from Jeff Vitter's paper (see full citation below). It
		 * works by repeatedly computing the number of tuples to skip
		 * before selecting a tuple, which replaces a randomly chosen
		 * element of the reservoir (current set of tuples).  At all
		 * times the reservoir is a true random sample of the tuples
		 * we've passed over so far, so when we fall off the end of
		 * the relation we're done.
		 */
		if (numrows < targrows)
			rows[numrows++] = ExecCopySlotHeapTuple(slot);
		else
		{
			/*
			 * t in Vitter's paper is the number of records already
			 * processed.  If we need to compute a new S value, we
			 * must use the not-yet-incremented value of samplerows as
			 * t.
			 */
			if (rowstoskip < 0)
				rowstoskip = anl_get_next_S(samplerows, targrows,
											&rstate);

			if (rowstoskip <= 0)
			{
				/*
				 * Found a suitable tuple, so save it, replacing one
				 * old tuple at random
				 */
				int			k = (int) (targrows * anl_random_fract());

				Assert(k >= 0 && k < targrows);
				heap_freetuple(rows[k]);
				rows[k] = ExecCopySlotHeapTuple(slot);
			}

			rowstoskip -= 1;
		}

		samplerows += 1;
	}

	/* Get the total tuple count in the table */
	FileSegTotals *fstotal;
	int64		hidden_tupcount = 0;

	if (aoScanDesc)
	{
		fstotal = GetSegFilesTotals(onerel, SnapshotSelf);
		hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&aoScanDesc->visibilityMap);
	}
	else
	{
		fstotal = GetAOCSSSegFilesTotals(onerel, SnapshotSelf);
		hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&aocsScanDesc->visibilityMap);
	}
	*totalrows = (double) fstotal->totaltuples - hidden_tupcount;
	/*
	 * Currently, we always report 0 dead rows on an AO table. We could
	 * perhaps get a better estimate using the AO visibility map. But this
	 * will do for now.
	 */
	*totaldeadrows = 0;

	ExecDropSingleTupleTableSlot(slot);
	if (aoScanDesc)
		appendonly_endscan(aoScanDesc);
	if (aocsScanDesc)
		aocs_endscan(aocsScanDesc);

	return numrows;
}

/*
 * Acquire a sample of rows.
 *
 * In GPDB, this is just a thin wrapper to route to the appropriate
 * subroutine, depending on the table type.
 */
int
acquire_sample_rows(Relation onerel, int elevel,
					HeapTuple *rows, int targrows,
					double *totalrows, double *totaldeadrows)
{
	if (Gp_role == GP_ROLE_DISPATCH &&
		onerel->rd_cdbpolicy && !GpPolicyIsEntry(onerel->rd_cdbpolicy))
	{
		/* Fetch sample from the segments. */
		return acquire_sample_rows_dispatcher(onerel, false, elevel,
											  rows, targrows,
											  totalrows, totaldeadrows);
	}
	/* Gather sample on this server. */
	else if (RelationIsHeap(onerel))
		return acquire_sample_rows_heap(onerel, elevel, rows, targrows,
										totalrows, totaldeadrows);
	else if (RelationIsAppendOptimized(onerel))
		return acquire_sample_rows_ao(onerel, elevel, rows, targrows,
									  totalrows, totaldeadrows);
	else
		elog(ERROR, "unsupported table type");
}

/* Select a random value R uniformly distributed in (0 - 1) */
double
anl_random_fract(void)
{
	return ((double) random() + 1) / ((double) MAX_RANDOM_VALUE + 2);
}

/*
 * These two routines embody Algorithm Z from "Random sampling with a
 * reservoir" by Jeffrey S. Vitter, in ACM Trans. Math. Softw. 11, 1
 * (Mar. 1985), Pages 37-57.  Vitter describes his algorithm in terms
 * of the count S of records to skip before processing another record.
 * It is computed primarily based on t, the number of records already read.
 * The only extra state needed between calls is W, a random state variable.
 *
 * anl_init_selection_state computes the initial W value.
 *
 * Given that we've already read t records (t >= n), anl_get_next_S
 * determines the number of records to skip before the next record is
 * processed.
 */
double
anl_init_selection_state(int n)
{
	/* Initial value of W (for use when Algorithm Z is first applied) */
	return exp(-log(anl_random_fract()) / n);
}

double
anl_get_next_S(double t, int n, double *stateptr)
{
	double		S;

	/* The magic constant here is T from Vitter's paper */
	if (t <= (22.0 * n))
	{
		/* Process records using Algorithm X until t is large enough */
		double		V,
					quot;

		V = anl_random_fract(); /* Generate V */
		S = 0;
		t += 1;
		/* Note: "num" in Vitter's code is always equal to t - n */
		quot = (t - (double) n) / t;
		/* Find min S satisfying (4.1) */
		while (quot > V)
		{
			S += 1;
			t += 1;
			quot *= (t - (double) n) / t;
		}
	}
	else
	{
		/* Now apply Algorithm Z */
		double		W = *stateptr;
		double		term = t - (double) n + 1;

		for (;;)
		{
			double		numer,
						numer_lim,
						denom;
			double		U,
						X,
						lhs,
						rhs,
						y,
						tmp;

			/* Generate U and X */
			U = anl_random_fract();
			X = t * (W - 1.0);
			S = floor(X);		/* S is tentatively set to floor(X) */
			/* Test if U <= h(S)/cg(X) in the manner of (6.3) */
			tmp = (t + 1) / term;
			lhs = exp(log(((U * tmp * tmp) * (term + S)) / (t + X)) / n);
			rhs = (((t + X) / (term + S)) * term) / t;
			if (lhs <= rhs)
			{
				W = rhs / lhs;
				break;
			}
			/* Test if U <= f(S)/cg(X) */
			y = (((U * (t + 1)) / term) * (t + S + 1)) / (t + X);
			if ((double) n < S)
			{
				denom = t;
				numer_lim = term + S;
			}
			else
			{
				denom = t - (double) n + S;
				numer_lim = t + 1;
			}
			for (numer = t + S; numer >= numer_lim; numer -= 1)
			{
				y *= numer / denom;
				denom -= 1;
			}
			W = exp(-log(anl_random_fract()) / n);		/* Generate W in advance */
			if (exp(log(y) / n) <= (t + X) / t)
				break;
		}
		*stateptr = W;
	}
	return S;
}

/*
 * qsort comparator for sorting rows[] array
 */
static int
compare_rows(const void *a, const void *b)
{
	HeapTuple	ha = *(const HeapTuple *) a;
	HeapTuple	hb = *(const HeapTuple *) b;
	BlockNumber ba = ItemPointerGetBlockNumber(&ha->t_self);
	OffsetNumber oa = ItemPointerGetOffsetNumber(&ha->t_self);
	BlockNumber bb = ItemPointerGetBlockNumber(&hb->t_self);
	OffsetNumber ob = ItemPointerGetOffsetNumber(&hb->t_self);

	if (ba < bb)
		return -1;
	if (ba > bb)
		return 1;
	if (oa < ob)
		return -1;
	if (oa > ob)
		return 1;
	return 0;
}


/*
 * acquire_inherited_sample_rows -- acquire sample rows from inheritance tree
 *
 * This has the same API as acquire_sample_rows, except that rows are
 * collected from all inheritance children as well as the specified table.
 * We fail and return zero if there are no inheritance children.
 */
int
acquire_inherited_sample_rows(Relation onerel, int elevel,
							  HeapTuple *rows, int targrows,
							  double *totalrows, double *totaldeadrows)
{
	List	   *tableOIDs;
	Relation   *rels;
	double	   *relblocks;
	double		totalblocks;
	int			numrows,
				nrels,
				i;
	ListCell   *lc;

	/*
	 * Like in acquire_sample_rows(), if we're in the QD, fetch the sample
	 * from segments.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		return acquire_sample_rows_dispatcher(onerel,
											  true, /* inherited stats */
											  elevel, rows, targrows,
											  totalrows, totaldeadrows);
	}

	/*
	 * Find all members of inheritance set.  We only need AccessShareLock on
	 * the children.
	 */
	tableOIDs =
		find_all_inheritors(RelationGetRelid(onerel), AccessShareLock, NULL);

	/*
	 * Check that there's at least one descendant, else fail.  This could
	 * happen despite analyze_rel's relhassubclass check, if table once had a
	 * child but no longer does.  In that case, we can clear the
	 * relhassubclass field so as not to make the same mistake again later.
	 * (This is safe because we hold ShareUpdateExclusiveLock.)
	 */
	if (list_length(tableOIDs) < 2)
	{
		/* CCI because we already updated the pg_class row in this command */
		CommandCounterIncrement();
		SetRelationHasSubclass(RelationGetRelid(onerel), false);
		*totalrows = 0;
		*totaldeadrows = 0;
		return 0;
	}

	/*
	 * Count the blocks in all the relations.  The result could overflow
	 * BlockNumber, so we use double arithmetic.
	 */
	rels = (Relation *) palloc(list_length(tableOIDs) * sizeof(Relation));
	relblocks = (double *) palloc(list_length(tableOIDs) * sizeof(double));
	totalblocks = 0;
	nrels = 0;
	foreach(lc, tableOIDs)
	{
		Oid			childOID = lfirst_oid(lc);
		Relation	childrel;

		/* We already got the needed lock */
		childrel = heap_open(childOID, NoLock);

		/* Ignore if temp table of another backend */
		if (RELATION_IS_OTHER_TEMP(childrel))
		{
			/* ... but release the lock on it */
			Assert(childrel != onerel);
			heap_close(childrel, AccessShareLock);
			continue;
		}

		rels[nrels] = childrel;
		relblocks[nrels] = acquire_number_of_blocks(childrel);
		totalblocks += relblocks[nrels];
		nrels++;
	}

	/*
	 * Now sample rows from each relation, proportionally to its fraction of
	 * the total block count.  (This might be less than desirable if the child
	 * rels have radically different free-space percentages, but it's not
	 * clear that it's worth working harder.)
	 */
	numrows = 0;
	*totalrows = 0;
	*totaldeadrows = 0;
	for (i = 0; i < nrels; i++)
	{
		Relation	childrel = rels[i];
		double		childblocks = relblocks[i];

		if (childblocks > 0)
		{
			int			childtargrows;

			childtargrows = (int) rint(targrows * childblocks / totalblocks);
			/* Make sure we don't overrun due to roundoff error */
			childtargrows = Min(childtargrows, targrows - numrows);
			if (childtargrows > 0)
			{
				int			childrows;
				double		trows,
							tdrows;

				/* Fetch a random sample of the child's rows */
				childrows = acquire_sample_rows(childrel,
												elevel,
												rows + numrows,
												childtargrows,
												&trows,
												&tdrows);

				/* We may need to convert from child's rowtype to parent's */
				if (childrows > 0 &&
					!equalTupleDescs(RelationGetDescr(childrel),
									 RelationGetDescr(onerel),
									 false))
				{
					TupleConversionMap *map;

					map = convert_tuples_by_name(RelationGetDescr(childrel),
												 RelationGetDescr(onerel),
								 gettext_noop("could not convert row type"));
					if (map != NULL)
					{
						int			j;

						for (j = 0; j < childrows; j++)
						{
							HeapTuple	newtup;

							newtup = do_convert_tuple(rows[numrows + j], map);
							heap_freetuple(rows[numrows + j]);
							rows[numrows + j] = newtup;
						}
						free_conversion_map(map);
					}
				}

				/* And add to counts */
				numrows += childrows;
				*totalrows += trows;
				*totaldeadrows += tdrows;
			}
		}

		/*
		 * Note: we cannot release the child-table locks, since we may have
		 * pointers to their TOAST tables in the sampled rows.
		 */
		heap_close(childrel, NoLock);
	}

	return numrows;
}

/*
 * This function acquires the HLL counter for the entire table by
 * using the hyperloglog extension gp_hyperloglog_accum().
 *
 * Unlike acquire_sample_rows(), this returns the HLL counter for
 * the entire table, and not jsut a sample, and it stores the HLL
 * counter into a separate attribute in the stats stahll_full to
 * distinguish it from the HLL for sampled data. This functions scans
 * the full table only once.
 */
static void
acquire_hll_by_query(Relation onerel, int nattrs, VacAttrStats **attrstats, int elevel)
{
	StringInfoData str, columnStr;
	int			i;
	int			ret;
	Datum	   *vals;
	MemoryContext oldcxt;
	const char *schemaName = get_namespace_name(RelationGetNamespace(onerel));

	initStringInfo(&str);
	initStringInfo(&columnStr);
	for (i = 0; i < nattrs; i++)
	{
		const char *attname = quote_identifier(NameStr(attrstats[i]->attr->attname));
		appendStringInfo(&columnStr, "pg_catalog.gp_hyperloglog_accum(%s)", attname);
		if(i != nattrs-1)
			appendStringInfo(&columnStr, ", ");
	}

	appendStringInfo(&str, "select %s from %s.%s as Ta ",
					 columnStr.data,
					 quote_identifier(schemaName),
					 quote_identifier(RelationGetRelationName(onerel)));

	oldcxt = CurrentMemoryContext;

	if (SPI_OK_CONNECT != SPI_connect())
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("unable to connect to execute internal query")));

	elog(elevel, "Executing SQL: %s", str.data);

	/*
	 * Do the query. We pass readonly==false, to force SPI to take a new
	 * snapshot. That ensures that we see all changes by our own transaction.
	 */
	ret = SPI_execute(str.data, false, 0);
	Assert(ret > 0);

	/*
	 * targrows in analyze_rel_internal() is an int,
	 * it's unlikely that this query will return more rows
	 */
	Assert(SPI_processed < 2);
	int sampleTuples = (int) SPI_processed;

	/* Ok, read in the tuples to *rows */
	MemoryContextSwitchTo(oldcxt);
	vals = (Datum *) palloc0(nattrs * sizeof(Datum));
	bool isNull = false;

	for (i = 0; i < sampleTuples; i++)
	{
		HeapTuple	sampletup = SPI_tuptable->vals[i];
		int			j;

		for (j = 0; j < nattrs; j++)
		{
			int	tupattnum = attrstats[j]->tupattnum;
			Assert(tupattnum >= 1 && tupattnum <= nattrs);

			vals[tupattnum - 1] = heap_getattr(sampletup, j + 1,
											   SPI_tuptable->tupdesc,
											   &isNull);
			if (isNull)
			{
				attrstats[j]->stahll_full = (bytea *)gp_hyperloglog_init_def();
				continue;
			}

			int16 typlen;
			bool typbyval;
			get_typlenbyval(SPI_tuptable->tupdesc->tdtypeid, &typlen, &typbyval);
			int hll_length = datumGetSize(vals[tupattnum-1], typbyval, typlen);
			attrstats[j]->stahll_full = (bytea *)datumCopy(PointerGetDatum(vals[tupattnum - 1]), false, hll_length);
		}
	}

	SPI_finish();
}

/*
 * Compute relation size.
 *
 * In upstream, this is a simple RelationGetNumberOfBlocks() call. But in
 * GPDB, we need to deal with AO and AOCS tables, and if we're in the
 * dispatcher, need to get the size from the segments.
 */
static BlockNumber
acquire_number_of_blocks(Relation onerel)
{
	int64		totalbytes;

	if (Gp_role == GP_ROLE_DISPATCH &&
		onerel->rd_cdbpolicy && !GpPolicyIsEntry(onerel->rd_cdbpolicy))
	{
		/* Query the segments using pg_relation_size(<rel>). */
		char		relsize_sql[100];

		snprintf(relsize_sql, sizeof(relsize_sql),
				 "select pg_catalog.pg_relation_size(%u, 'main')", RelationGetRelid(onerel));
		totalbytes = get_size_from_segDBs(relsize_sql);
		if (GpPolicyIsReplicated(onerel->rd_cdbpolicy))
		{
			/*
			 * pg_relation_size sums up the table size on each segment. That's
			 * correct for hash and randomly distributed tables. But for a
			 * replicated table, we want pg_class.relpages to count the data
			 * only once.
			 */
			totalbytes = totalbytes / onerel->rd_cdbpolicy->numsegments;
		}

		return RelationGuessNumberOfBlocks(totalbytes);
	}
	/* Check size on this server. */
	else if (RelationIsHeap(onerel))
	{
		/* NOTE: This covers indexes, too */
		return RelationGetNumberOfBlocks(onerel);
	}
	else if (RelationIsAoRows(onerel))
	{
		totalbytes = GetAOTotalBytes(onerel, GetActiveSnapshot());
		return RelationGuessNumberOfBlocks(totalbytes);
	}
	else if (RelationIsAoCols(onerel))
	{
		totalbytes = GetAOCSTotalBytes(onerel, GetActiveSnapshot(), true);
		return RelationGuessNumberOfBlocks(totalbytes);
	}
	else
		elog(ERROR, "unsupported table type");
}

/*
 * Compute index relation's size.
 *
 * Like acquire_number_of_blocks(), but for indexes. Indexes don't
 * have a distribution policy, so we use the parent table's policy
 * to determine whether we need to get the size on segments or
 * locally.
 */
static BlockNumber
acquire_index_number_of_blocks(Relation indexrel, Relation tablerel)
{
	int64		totalbytes;

	if (Gp_role == GP_ROLE_DISPATCH &&
		tablerel->rd_cdbpolicy && !GpPolicyIsEntry(tablerel->rd_cdbpolicy))
	{
		/* Query the segments using pg_relation_size(<rel>). */
		char		relsize_sql[100];

		snprintf(relsize_sql, sizeof(relsize_sql),
				 "select pg_catalog.pg_relation_size(%u, 'main')", RelationGetRelid(indexrel));
		totalbytes = get_size_from_segDBs(relsize_sql);
		if (GpPolicyIsReplicated(tablerel->rd_cdbpolicy))
		{
			/*
			 * pg_relation_size sums up the table size on each segment. That's
			 * correct for hash and randomly distributed tables. But for a
			 * replicated table, we want pg_class.relpages to count the data
			 * only once.
			 */
			totalbytes = totalbytes / tablerel->rd_cdbpolicy->numsegments;
		}

		return RelationGuessNumberOfBlocks(totalbytes);
	}
	/* Check size on this server. */
	else
	{
		Assert(RelationIsHeap(indexrel));
		return RelationGetNumberOfBlocks(indexrel);
	}
}

/*
 * Collect a sample from segments.
 *
 * Calls the gp_acquire_sample_rows() helper function on each segment,
 * and merges the results.
 */
static int
acquire_sample_rows_dispatcher(Relation onerel, bool inh, int elevel,
							   HeapTuple *rows, int targrows,
							   double *totalrows, double *totaldeadrows)
{
	/*
	 * 'colLargeRowIndexes' is essentially an argument, but it's passed via a
	 * global variable to avoid changing the AcquireSampleRowsFunc prototype.
	 */
	Bitmapset **colLargeRowIndexes = acquire_func_colLargeRowIndexes;
	TupleDesc	relDesc = RelationGetDescr(onerel);
	TupleDesc	newDesc;
	AttInMetadata *attinmeta;
	StringInfoData str;
	int			sampleTuples;	/* 32 bit - assume that number of tuples will not > 2B */
	char	  **values;
	int			numLiveColumns;
	int			perseg_targrows;
	CdbPgResults cdb_pgresults = {NULL, 0};
	int			i;

	Assert(targrows > 0.0);

	/*
	 * Acquire an evenly-sized sample from each segment.
	 *
	 * XXX: If there's a significant bias between the segments, i.e. some
	 * segments have a lot more rows than others, the sample will biased, too.
	 * Would be nice to improve that, but it's not clear how. We could issue
	 * another query to get the table size from each segment first, and use
	 * those to weigh the sample size to get from each segment. But that'd
	 * require an extra round-trip, which is also not good. The caller
	 * actually already did that, to get the total relation size, but it
	 * doesn't pass that down to us, let alone the per-segment sizes.
	 */
	if (GpPolicyIsReplicated(onerel->rd_cdbpolicy))
		perseg_targrows = targrows;
	else if (GpPolicyIsPartitioned(onerel->rd_cdbpolicy))
		perseg_targrows = targrows / onerel->rd_cdbpolicy->numsegments;
	else
		elog(ERROR, "acquire_sample_rows_dispatcher() cannot be used on a non-distributed table");

	/*
	 * Count the number of columns, excluding dropped columns. We'll need that
	 * later.
	 */
	numLiveColumns = 0;
	for (i = 0; i < relDesc->natts; i++)
	{
		Form_pg_attribute attr = relDesc->attrs[i];

		if (attr->attisdropped)
			continue;

		numLiveColumns++;
	}

	/*
	 * Construct SQL command to dispatch to segments.
	 */
	initStringInfo(&str);
	appendStringInfo(&str, "select * from pg_catalog.gp_acquire_sample_rows(%u, %d, '%s')",
					 RelationGetRelid(onerel),
					 perseg_targrows,
					 inh ? "t" : "f");

	/* special columns */
	appendStringInfoString(&str, " as (");
	appendStringInfoString(&str, "totalrows pg_catalog.float8, ");
	appendStringInfoString(&str, "totaldeadrows pg_catalog.float8, ");
	appendStringInfoString(&str, "oversized_cols_bitmap pg_catalog.text");

	/* table columns */
	for (i = 0; i < relDesc->natts; i++)
	{
		Form_pg_attribute attr = relDesc->attrs[i];
		Oid			typid = gp_acquire_sample_rows_col_type(attr->atttypid);

		if (attr->attisdropped)
			continue;

		appendStringInfo(&str, ", %s %s",
						 quote_identifier(NameStr(attr->attname)),
						 format_type_be(typid));
	}

	appendStringInfoString(&str, ")");

	/*
	 * Execute it.
	 */
	elog(elevel, "Executing SQL: %s", str.data);
	CdbDispatchCommand(str.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

	/*
	 * Build a modified tuple descriptor for the table.
	 *
	 * Some datatypes need special treatment, so we cannot use the relation's
	 * original tupledesc.
	 */
	newDesc = CreateTupleDescCopy(relDesc);
	for (i = 0; i < relDesc->natts; i++)
	{
		Form_pg_attribute attr = relDesc->attrs[i];
		Oid			typid = gp_acquire_sample_rows_col_type(attr->atttypid);

		newDesc->attrs[i]->atttypid = typid;
	}
	attinmeta = TupleDescGetAttInMetadata(newDesc);

	/*
	 * Read the result set from each segment. Gather the sample rows *rows,
	 * and sum up the summary rows for grand 'totalrows' and 'totaldeadrows'.
	 */
	values = (char **) palloc0(relDesc->natts * sizeof(char *));
	sampleTuples = 0;
	*totalrows = 0;
	*totaldeadrows = 0;
	for (int resultno = 0; resultno < cdb_pgresults.numResults; resultno++)
	{
		struct pg_result *pgresult = cdb_pgresults.pg_results[resultno];
		bool		got_summary = false;
		double		this_totalrows = 0;
		double		this_totaldeadrows = 0;

		if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			ereport(ERROR,
					(errmsg("unexpected result from segment: %d",
							PQresultStatus(pgresult))));
		}

		if (GpPolicyIsReplicated(onerel->rd_cdbpolicy))
		{
			/*
			 * A replicated table has the same data in all segments. Arbitrarily,
			 * use the sample from the first segment, and discard the rest.
			 * (This is rather inefficient, of course. It would be better to
			 * dispatch to only one segment, but there is no easy API for that
			 * in the dispatcher.)
			 */
			if (resultno > 0)
				continue;
		}

		for (int rowno = 0; rowno < PQntuples(pgresult); rowno++)
		{
			if (!PQgetisnull(pgresult, rowno, 0))
			{
				/* This is a summary row. */
				if (got_summary)
					elog(ERROR, "got duplicate summary row from gp_acquire_sample_rows");

				this_totalrows = DatumGetFloat8(DirectFunctionCall1(float8in,
																	CStringGetDatum(PQgetvalue(pgresult, rowno, 0))));
				this_totaldeadrows = DatumGetFloat8(DirectFunctionCall1(float8in,
																		CStringGetDatum(PQgetvalue(pgresult, rowno, 1))));
				got_summary = true;
			}
			else
			{
				/* This is a sample row. */
				int			index;

				if (sampleTuples >= targrows)
					elog(ERROR, "too many sample rows received from gp_acquire_sample_rows");

				/* Read the 'toolarge' bitmap, if any */
				if (colLargeRowIndexes && !PQgetisnull(pgresult, rowno, 2))
				{
					char	   *toolarge;

					toolarge = PQgetvalue(pgresult, rowno, 2);
					if (strlen(toolarge) != numLiveColumns)
						elog(ERROR, "'toolarge' bitmap has incorrect length");

					index = 0;
					for (i = 0; i < relDesc->natts; i++)
					{
						Form_pg_attribute attr = relDesc->attrs[i];

						if (attr->attisdropped)
							continue;

						if (toolarge[index] == '1')
							colLargeRowIndexes[i] = bms_add_member(colLargeRowIndexes[i], sampleTuples);
						index++;
					}
				}

				/* Process the columns */
				index = 0;
				for (i = 0; i < relDesc->natts; i++)
				{
					Form_pg_attribute attr = relDesc->attrs[i];

					if (attr->attisdropped)
						continue;

					if (PQgetisnull(pgresult, rowno, 3 + index))
						values[i] = NULL;
					else
						values[i] = PQgetvalue(pgresult, rowno, 3 + index);
					index++; /* Move index to the next result set attribute */
				}

				rows[sampleTuples] = BuildTupleFromCStrings(attinmeta, values);
				sampleTuples++;

				/*
				 * note: we don't set the OIDs in the sample. ANALYZE doesn't
				 * collect stats for them
				 */
			}
		}

		if (!got_summary)
			elog(ERROR, "did not get summary row from gp_acquire_sample_rows");

		if (resultno >= onerel->rd_cdbpolicy->numsegments)
		{
			/*
			 * This result is for a segment that's not holding any data for this
			 * table. Should get 0 rows.
			 */
			if (this_totalrows != 0 || this_totalrows != 0)
				elog(WARNING, "table \"%s\" contains rows in segment %d, which is outside the # of segments for the table's policy (%d segments)",
					 RelationGetRelationName(onerel), resultno, onerel->rd_cdbpolicy->numsegments);
		}

		(*totalrows) += this_totalrows;
		(*totaldeadrows) += this_totaldeadrows;
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return sampleTuples;
}

/*
 *	update_attstats() -- update attribute statistics for one relation
 *
 *		Statistics are stored in several places: the pg_class row for the
 *		relation has stats about the whole relation, and there is a
 *		pg_statistic row for each (non-system) attribute that has ever
 *		been analyzed.  The pg_class values are updated by VACUUM, not here.
 *
 *		pg_statistic rows are just added or updated normally.  This means
 *		that pg_statistic will probably contain some deleted rows at the
 *		completion of a vacuum cycle, unless it happens to get vacuumed last.
 *
 *		To keep things simple, we punt for pg_statistic, and don't try
 *		to compute or store rows for pg_statistic itself in pg_statistic.
 *		This could possibly be made to work, but it's not worth the trouble.
 *		Note analyze_rel() has seen to it that we won't come here when
 *		vacuuming pg_statistic itself.
 *
 *		Note: there would be a race condition here if two backends could
 *		ANALYZE the same table concurrently.  Presently, we lock that out
 *		by taking a self-exclusive lock on the relation in analyze_rel().
 */
static void
update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats)
{
	Relation	sd;
	int			attno;

	if (natts <= 0)
		return;					/* nothing to do */

	sd = heap_open(StatisticRelationId, RowExclusiveLock);

	for (attno = 0; attno < natts; attno++)
	{
		VacAttrStats *stats = vacattrstats[attno];
		HeapTuple	stup,
					oldtup;
		int			i,
					k,
					n;
		Datum		values[Natts_pg_statistic];
		bool		nulls[Natts_pg_statistic];
		bool		replaces[Natts_pg_statistic];

		/* Ignore attr if we weren't able to collect stats */
		if (!stats->stats_valid)
			continue;

		/*
		 * Construct a new pg_statistic tuple
		 */
		for (i = 0; i < Natts_pg_statistic; ++i)
		{
			nulls[i] = false;
			replaces[i] = true;
		}

		values[Anum_pg_statistic_starelid - 1] = ObjectIdGetDatum(relid);
		values[Anum_pg_statistic_staattnum - 1] = Int16GetDatum(stats->attr->attnum);
		values[Anum_pg_statistic_stainherit - 1] = BoolGetDatum(inh);
		values[Anum_pg_statistic_stanullfrac - 1] = Float4GetDatum(stats->stanullfrac);
		values[Anum_pg_statistic_stawidth - 1] = Int32GetDatum(stats->stawidth);
		values[Anum_pg_statistic_stadistinct - 1] = Float4GetDatum(stats->stadistinct);
		i = Anum_pg_statistic_stakind1 - 1;
		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
		{
			values[i++] = Int16GetDatum(stats->stakind[k]);		/* stakindN */
		}
		i = Anum_pg_statistic_staop1 - 1;
		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
		{
			values[i++] = ObjectIdGetDatum(stats->staop[k]);	/* staopN */
		}
		i = Anum_pg_statistic_stanumbers1 - 1;
		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
		{
			int			nnum = stats->numnumbers[k];

			if (nnum > 0)
			{
				Datum	   *numdatums = (Datum *) palloc(nnum * sizeof(Datum));
				ArrayType  *arry;

				for (n = 0; n < nnum; n++)
					numdatums[n] = Float4GetDatum(stats->stanumbers[k][n]);
				/* XXX knows more than it should about type float4: */
				arry = construct_array(numdatums, nnum,
									   FLOAT4OID,
									   sizeof(float4), FLOAT4PASSBYVAL, 'i');
				values[i++] = PointerGetDatum(arry);	/* stanumbersN */
			}
			else
			{
				nulls[i] = true;
				values[i++] = (Datum) 0;
			}
		}
		i = Anum_pg_statistic_stavalues1 - 1;
		for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
		{
			if (stats->numvalues[k] > 0)
			{
				ArrayType  *arry;

				arry = construct_array(stats->stavalues[k],
									   stats->numvalues[k],
									   stats->statypid[k],
									   stats->statyplen[k],
									   stats->statypbyval[k],
									   stats->statypalign[k]);
				values[i++] = PointerGetDatum(arry);	/* stavaluesN */
			}
			else
			{
				nulls[i] = true;
				values[i++] = (Datum) 0;
			}
		}

		/* Is there already a pg_statistic tuple for this attribute? */
		oldtup = SearchSysCache3(STATRELATTINH,
								 ObjectIdGetDatum(relid),
								 Int16GetDatum(stats->attr->attnum),
								 BoolGetDatum(inh));

		if (HeapTupleIsValid(oldtup))
		{
			/* Yes, replace it */
			stup = heap_modify_tuple(oldtup,
									 RelationGetDescr(sd),
									 values,
									 nulls,
									 replaces);
			ReleaseSysCache(oldtup);
			simple_heap_update(sd, &stup->t_self, stup);
		}
		else
		{
			/* No, insert new tuple */
			stup = heap_form_tuple(RelationGetDescr(sd), values, nulls);
			simple_heap_insert(sd, stup);
		}

		/* update indexes too */
		CatalogUpdateIndexes(sd, stup);

		heap_freetuple(stup);
	}

	heap_close(sd, RowExclusiveLock);
}

/*
 * Standard fetch function for use by compute_stats subroutines.
 *
 * This exists to provide some insulation between compute_stats routines
 * and the actual storage of the sample data.
 */
static Datum
std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{
	int			attnum = stats->tupattnum;
	HeapTuple	tuple = stats->rows[rownum];
	TupleDesc	tupDesc = stats->tupDesc;

	return heap_getattr(tuple, attnum, tupDesc, isNull);
}

/*
 * Fetch function for analyzing index expressions.
 *
 * We have not bothered to construct index tuples, instead the data is
 * just in Datum arrays.
 */
static Datum
ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{
	int			i;

	/* exprvals and exprnulls are already offset for proper column */
	i = rownum * stats->rowstride;
	*isNull = stats->exprnulls[i];
	return stats->exprvals[i];
}


/*==========================================================================
 *
 * Code below this point represents the "standard" type-specific statistics
 * analysis algorithms.  This code can be replaced on a per-data-type basis
 * by setting a nonzero value in pg_type.typanalyze.
 *
 *==========================================================================
 */


/*
 * In PostgreSQL, WIDTH_THRESHOLD is here, but we've moved it to vacuum.h in
 * GPDB.
 */

#define swapInt(a,b)	do {int _tmp; _tmp=a; a=b; b=_tmp;} while(0)
#define swapDatum(a,b)	do {Datum _tmp; _tmp=a; a=b; b=_tmp;} while(0)

/*
 * Extra information used by the default analysis routines
 */
typedef struct
{
	Oid			eqopr;			/* '=' operator for datatype, if any */
	Oid			eqfunc;			/* and associated function */
	Oid			ltopr;			/* '<' operator for datatype, if any */
} StdAnalyzeData;

typedef struct
{
	Datum		value;			/* a data value */
	int			tupno;			/* position index for tuple it came from */
} ScalarItem;

typedef struct
{
	int			count;			/* # of duplicates */
	int			first;			/* values[] index of first occurrence */
} ScalarMCVItem;

typedef struct
{
	SortSupport ssup;
	int		   *tupnoLink;
} CompareScalarsContext;


static void compute_minimal_stats(VacAttrStatsP stats,
					  AnalyzeAttrFetchFunc fetchfunc,
					  int samplerows,
					  double totalrows);
static void compute_very_minimal_stats(VacAttrStatsP stats,
					  AnalyzeAttrFetchFunc fetchfunc,
					  int samplerows,
					  double totalrows);
static void compute_scalar_stats(VacAttrStatsP stats,
					 AnalyzeAttrFetchFunc fetchfunc,
					 int samplerows,
					 double totalrows);
static void merge_leaf_stats(VacAttrStatsP stats,
								 AnalyzeAttrFetchFunc fetchfunc,
								 int samplerows,
								 double totalrows);
static int	compare_scalars(const void *a, const void *b, void *arg);
static int	compare_mcvs(const void *a, const void *b);


/*
 * std_typanalyze -- the default type-specific typanalyze function
 */
bool
std_typanalyze(VacAttrStats *stats)
{
	Form_pg_attribute attr = stats->attr;
	Oid			ltopr;
	Oid			eqopr;
	StdAnalyzeData *mystats;

	/* If the attstattarget column is negative, use the default value */
	/* NB: it is okay to scribble on stats->attr since it's a copy */
	if (attr->attstattarget < 0)
		attr->attstattarget = default_statistics_target;

	/* Look for default "<" and "=" operators for column's type */
	get_sort_group_operators(stats->attrtypid,
							 false, false, false,
							 &ltopr, &eqopr, NULL,
							 NULL);

	/* Save the operator info for compute_stats routines */
	mystats = (StdAnalyzeData *) palloc(sizeof(StdAnalyzeData));
	mystats->eqopr = eqopr;
	mystats->eqfunc = get_opcode(eqopr);
	mystats->ltopr = ltopr;
	stats->extra_data = mystats;
	stats->merge_stats = false;

	/*
	 * Determine which standard statistics algorithm to use
	 */
	List *va_cols = list_make1_int(stats->attr->attnum);
	if (rel_part_status(attr->attrelid) == PART_STATUS_ROOT &&
		leaf_parts_analyzed(stats->attr->attrelid, InvalidOid, va_cols, stats->elevel) &&
		op_hashjoinable(eqopr, stats->attrtypid))
	{
		stats->merge_stats = true;
		stats->compute_stats = merge_leaf_stats;
		stats->minrows = 300 * attr->attstattarget;
	}
	else if (OidIsValid(ltopr) && OidIsValid(eqopr))
	{
		/* Seems to be a scalar datatype */
		stats->compute_stats = compute_scalar_stats;
		/*--------------------
		 * The following choice of minrows is based on the paper
		 * "Random sampling for histogram construction: how much is enough?"
		 * by Surajit Chaudhuri, Rajeev Motwani and Vivek Narasayya, in
		 * Proceedings of ACM SIGMOD International Conference on Management
		 * of Data, 1998, Pages 436-447.  Their Corollary 1 to Theorem 5
		 * says that for table size n, histogram size k, maximum relative
		 * error in bin size f, and error probability gamma, the minimum
		 * random sample size is
		 *		r = 4 * k * ln(2*n/gamma) / f^2
		 * Taking f = 0.5, gamma = 0.01, n = 10^6 rows, we obtain
		 *		r = 305.82 * k
		 * Note that because of the log function, the dependence on n is
		 * quite weak; even at n = 10^12, a 300*k sample gives <= 0.66
		 * bin size error with probability 0.99.  So there's no real need to
		 * scale for n, which is a good thing because we don't necessarily
		 * know it at this point.
		 *--------------------
		 */
		stats->minrows = 300 * attr->attstattarget;
	}
	else if (OidIsValid(eqopr))
	{
		/* Can't do much but the minimal stuff */
		stats->compute_stats = compute_minimal_stats;
		/* Might as well use the same minrows as above */
		stats->minrows = 300 * attr->attstattarget;
	}
	else
	{
		/* Can't do much but the very minimal stuff */
		stats->compute_stats = compute_very_minimal_stats;
		/* Might as well use the same minrows as above */
		stats->minrows = 300 * attr->attstattarget;
	}
	list_free(va_cols);
	return true;
}

/*
 *	compute_minimal_stats() -- compute minimal column statistics
 *
 *	We use this when we can find only an "=" operator for the datatype.
 *
 *	We determine the fraction of non-null rows, the average width, the
 *	most common values, and the (estimated) number of distinct values.
 *
 *	The most common values are determined by brute force: we keep a list
 *	of previously seen values, ordered by number of times seen, as we scan
 *	the samples.  A newly seen value is inserted just after the last
 *	multiply-seen value, causing the bottommost (oldest) singly-seen value
 *	to drop off the list.  The accuracy of this method, and also its cost,
 *	depend mainly on the length of the list we are willing to keep.
 */
static void
compute_minimal_stats(VacAttrStatsP stats,
					  AnalyzeAttrFetchFunc fetchfunc,
					  int samplerows,
					  double totalrows)
{
	int			i;
	int			null_cnt = 0;
	int			nonnull_cnt = 0;
	int			toowide_cnt = 0;
	double		total_width = 0;
	bool		is_varlena = (!stats->attrtype->typbyval &&
							  stats->attrtype->typlen == -1);
	bool		is_varwidth = (!stats->attrtype->typbyval &&
							   stats->attrtype->typlen < 0);
	FmgrInfo	f_cmpeq;
	typedef struct
	{
		Datum		value;
		int			count;
	} TrackItem;
	TrackItem  *track;
	int			track_cnt,
				track_max;
	int			num_mcv = stats->attr->attstattarget;
	StdAnalyzeData *mystats = (StdAnalyzeData *) stats->extra_data;

	/*
	 * We track up to 2*n values for an n-element MCV list; but at least 10
	 */
	track_max = 2 * num_mcv;
	if (track_max < 10)
		track_max = 10;
	track = (TrackItem *) palloc(track_max * sizeof(TrackItem));
	track_cnt = 0;

	fmgr_info(mystats->eqfunc, &f_cmpeq);

	stats->stahll = (bytea *)gp_hyperloglog_init_def();

	ereport(DEBUG2,
			(errmsg("Computing Minimal Stats for column %s",
					get_attname(stats->attr->attrelid, stats->attr->attnum))));

	for (i = 0; i < samplerows; i++)
	{
		Datum		value;
		bool		isnull;
		bool		match;
		int			firstcount1,
					j;

		vacuum_delay_point();

		value = fetchfunc(stats, i, &isnull);

		/* Check for null/nonnull */
		if (isnull)
		{
			null_cnt++;
			continue;
		}
		nonnull_cnt++;

		stats->stahll = (bytea *)gp_hyperloglog_add_item((GpHLLCounter) stats->stahll, value, stats->attr->attlen, stats->attr->attbyval, stats->attr->attalign);

		/*
		 * If it's a variable-width field, add up widths for average width
		 * calculation.  Note that if the value is toasted, we use the toasted
		 * width.  We don't bother with this calculation if it's a fixed-width
		 * type.
		 */
		if (is_varlena)
		{
			total_width += VARSIZE_ANY(DatumGetPointer(value));

			/*
			 * If the value is toasted, we want to detoast it just once to
			 * avoid repeated detoastings and resultant excess memory usage
			 * during the comparisons.  Also, check to see if the value is
			 * excessively wide, and if so don't detoast at all --- just
			 * ignore the value.
			 */
			if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
			{
				toowide_cnt++;
				continue;
			}
			value = PointerGetDatum(PG_DETOAST_DATUM(value));
		}
		else if (is_varwidth)
		{
			/* must be cstring */
			total_width += strlen(DatumGetCString(value)) + 1;
		}

		/*
		 * See if the value matches anything we're already tracking.
		 */
		match = false;
		firstcount1 = track_cnt;
		for (j = 0; j < track_cnt; j++)
		{
			/* We always use the default collation for statistics */
			if (DatumGetBool(FunctionCall2Coll(&f_cmpeq,
											   DEFAULT_COLLATION_OID,
											   value, track[j].value)))
			{
				match = true;
				break;
			}
			if (j < firstcount1 && track[j].count == 1)
				firstcount1 = j;
		}

		if (match)
		{
			/* Found a match */
			track[j].count++;
			/* This value may now need to "bubble up" in the track list */
			while (j > 0 && track[j].count > track[j - 1].count)
			{
				swapDatum(track[j].value, track[j - 1].value);
				swapInt(track[j].count, track[j - 1].count);
				j--;
			}
		}
		else
		{
			/* No match.  Insert at head of count-1 list */
			if (track_cnt < track_max)
				track_cnt++;
			for (j = track_cnt - 1; j > firstcount1; j--)
			{
				track[j].value = track[j - 1].value;
				track[j].count = track[j - 1].count;
			}
			if (firstcount1 < track_cnt)
			{
				track[firstcount1].value = value;
				track[firstcount1].count = 1;
			}
		}
	}

	/* We can only compute real stats if we found some non-null values. */
	if (nonnull_cnt > 0)
	{
		int			nmultiple,
					summultiple;

		stats->stats_valid = true;
		/* Do the simple null-frac and width stats */
		stats->stanullfrac = (double) null_cnt / (double) samplerows;
		if (is_varwidth)
			stats->stawidth = total_width / (double) nonnull_cnt;
		else
			stats->stawidth = stats->attrtype->typlen;

		/* Count the number of values we found multiple times */
		summultiple = 0;
		for (nmultiple = 0; nmultiple < track_cnt; nmultiple++)
		{
			if (track[nmultiple].count == 1)
				break;
			summultiple += track[nmultiple].count;
		}

		((GpHLLCounter) (stats->stahll))->nmultiples = nmultiple;
		((GpHLLCounter) (stats->stahll))->ndistinct = track_cnt;
		((GpHLLCounter) (stats->stahll))->samplerows = samplerows;

		if (nmultiple == 0)
		{
			/*
			 * If we found no repeated non-null values, assume it's a unique
			 * column; but be sure to discount for any nulls we found.
			 */
			stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
		}
		else if (track_cnt < track_max && toowide_cnt == 0 &&
				 nmultiple == track_cnt)
		{
			/*
			 * Our track list includes every value in the sample, and every
			 * value appeared more than once.  Assume the column has just
			 * these values.
			 */
			stats->stadistinct = track_cnt;
		}
		else
		{
			/*----------
			 * Estimate the number of distinct values using the estimator
			 * proposed by Haas and Stokes in IBM Research Report RJ 10025:
			 *		n*d / (n - f1 + f1*n/N)
			 * where f1 is the number of distinct values that occurred
			 * exactly once in our sample of n rows (from a total of N),
			 * and d is the total number of distinct values in the sample.
			 * This is their Duj1 estimator; the other estimators they
			 * recommend are considerably more complex, and are numerically
			 * very unstable when n is much smaller than N.
			 *
			 * We assume (not very reliably!) that all the multiply-occurring
			 * values are reflected in the final track[] list, and the other
			 * nonnull values all appeared but once.  (XXX this usually
			 * results in a drastic overestimate of ndistinct.  Can we do
			 * any better?)
			 *----------
			 */
			int			f1 = nonnull_cnt - summultiple;
			int			d = f1 + nmultiple;
			double		numer,
						denom,
						stadistinct;

			numer = (double) samplerows *(double) d;

			denom = (double) (samplerows - f1) +
				(double) f1 *(double) samplerows / totalrows;

			stadistinct = numer / denom;
			/* Clamp to sane range in case of roundoff error */
			if (stadistinct < (double) d)
				stadistinct = (double) d;
			if (stadistinct > totalrows)
				stadistinct = totalrows;
			stats->stadistinct = floor(stadistinct + 0.5);
		}

		/*
		 * If we estimated the number of distinct values at more than 10% of
		 * the total row count (a very arbitrary limit), then assume that
		 * stadistinct should scale with the row count rather than be a fixed
		 * value.
		 */
		if (stats->stadistinct > 0.1 * totalrows)
			stats->stadistinct = -(stats->stadistinct / totalrows);

		/*
		 * Decide how many values are worth storing as most-common values. If
		 * we are able to generate a complete MCV list (all the values in the
		 * sample will fit, and we think these are all the ones in the table),
		 * then do so.  Otherwise, store only those values that are
		 * significantly more common than the (estimated) average. We set the
		 * threshold rather arbitrarily at 25% more than average, with at
		 * least 2 instances in the sample.
		 */
		if (track_cnt < track_max && toowide_cnt == 0 &&
			stats->stadistinct > 0 &&
			track_cnt <= num_mcv)
		{
			/* Track list includes all values seen, and all will fit */
			num_mcv = track_cnt;
		}
		else
		{
			double		ndistinct = stats->stadistinct;
			double		avgcount,
						mincount;

			if (ndistinct < 0)
				ndistinct = -ndistinct * totalrows;
			/* estimate # of occurrences in sample of a typical value */
			avgcount = (double) samplerows / ndistinct;
			/* set minimum threshold count to store a value */
			mincount = avgcount * 1.25;
			if (mincount < 2)
				mincount = 2;
			if (num_mcv > track_cnt)
				num_mcv = track_cnt;
			for (i = 0; i < num_mcv; i++)
			{
				if (track[i].count < mincount)
				{
					num_mcv = i;
					break;
				}
			}
		}

		/* Generate MCV slot entry */
		if (num_mcv > 0)
		{
			MemoryContext old_context;
			Datum	   *mcv_values;
			float4	   *mcv_freqs;

			/* Must copy the target values into anl_context */
			old_context = MemoryContextSwitchTo(stats->anl_context);
			mcv_values = (Datum *) palloc(num_mcv * sizeof(Datum));
			mcv_freqs = (float4 *) palloc(num_mcv * sizeof(float4));
			for (i = 0; i < num_mcv; i++)
			{
				mcv_values[i] = datumCopy(track[i].value,
										  stats->attrtype->typbyval,
										  stats->attrtype->typlen);
				mcv_freqs[i] = (double) track[i].count / (double) samplerows;
			}
			MemoryContextSwitchTo(old_context);

			stats->stakind[0] = STATISTIC_KIND_MCV;
			stats->staop[0] = mystats->eqopr;
			stats->stanumbers[0] = mcv_freqs;
			stats->numnumbers[0] = num_mcv;
			stats->stavalues[0] = mcv_values;
			stats->numvalues[0] = num_mcv;

			/*
			 * Accept the defaults for stats->statypid and others. They have
			 * been set before we were called (see vacuum.h)
			 */
		}
	}
	else if (null_cnt > 0)
	{
		/* We found only nulls; assume the column is entirely null */
		stats->stats_valid = true;
		stats->stanullfrac = 1.0;
		if (is_varwidth)
			stats->stawidth = 0;	/* "unknown" */
		else
			stats->stawidth = stats->attrtype->typlen;
		stats->stadistinct = 0.0;		/* "unknown" */
	}

	/* We don't need to bother cleaning up any of our temporary palloc's */
}


/*
 *	compute_very_minimal_stats() -- compute minimal column statistics
 *
 *	We use this when we cannot even find an "=" operator for the datatype.
 *	We determine the fraction of non-null rows and the average width. There
 *	isn't much else we can do. These stats are not too useful, but ORCA
 *	gives warnings if a column doesn't have a pg_statistics row, so any
 *	statistics at all is better than none.
 */
static void
compute_very_minimal_stats(VacAttrStatsP stats,
						   AnalyzeAttrFetchFunc fetchfunc,
						   int samplerows,
						   double totalrows)
{
	int			i;
	int			null_cnt = 0;
	int			nonnull_cnt = 0;
	double		total_width = 0;
	bool		is_varlena = (!stats->attr->attbyval &&
							  stats->attr->attlen == -1);
	bool		is_varwidth = (!stats->attr->attbyval &&
							   stats->attr->attlen < 0);

	ereport(DEBUG2,
			(errmsg("Computing Very Minimal Stats for column %s",
					get_attname(stats->attr->attrelid, stats->attr->attnum))));

	for (i = 0; i < samplerows; i++)
	{
		Datum		value;
		bool		isnull;

		vacuum_delay_point();

		value = fetchfunc(stats, i, &isnull);

		/* Check for null/nonnull */
		if (isnull)
		{
			null_cnt++;
			continue;
		}
		nonnull_cnt++;

		/*
		 * If it's a variable-width field, add up widths for average width
		 * calculation.  Note that if the value is toasted, we use the toasted
		 * width.  We don't bother with this calculation if it's a fixed-width
		 * type.
		 */
		if (is_varlena)
		{
			total_width += VARSIZE_ANY(DatumGetPointer(value));
		}
		else if (is_varwidth)
		{
			/* must be cstring */
			total_width += strlen(DatumGetCString(value)) + 1;
		}
	}

	/* We can only compute real stats if we found some non-null values. */
	if (nonnull_cnt > 0)
	{
		stats->stats_valid = true;
		/* Do the simple null-frac and width stats */
		stats->stanullfrac = (double) null_cnt / (double) samplerows;
		if (is_varwidth)
			stats->stawidth = total_width / (double) nonnull_cnt;
		else
			stats->stawidth = stats->attrtype->typlen;

		/* Assume it's a unique column */
		stats->stadistinct = -1.0;
	}
	else if (null_cnt > 0)
	{
		/* We found only nulls; assume the column is entirely null */
		stats->stats_valid = true;
		stats->stanullfrac = 1.0;
		if (is_varwidth)
			stats->stawidth = 0;	/* "unknown" */
		else
			stats->stawidth = stats->attrtype->typlen;
		stats->stadistinct = 0.0;		/* "unknown" */
	}

	/* We don't need to bother cleaning up any of our temporary palloc's */
}


/*
 *	compute_scalar_stats() -- compute column statistics
 *
 *	We use this when we can find "=" and "<" operators for the datatype.
 *
 *	We determine the fraction of non-null rows, the average width, the
 *	most common values, the (estimated) number of distinct values, the
 *	distribution histogram, and the correlation of physical to logical order.
 *
 *	The desired stats can be determined fairly easily after sorting the
 *	data values into order.
 */
static void
compute_scalar_stats(VacAttrStatsP stats,
					 AnalyzeAttrFetchFunc fetchfunc,
					 int samplerows,
					 double totalrows)
{
	int			i;
	int			null_cnt = 0;
	int			nonnull_cnt = 0;
	int			toowide_cnt = 0;
	double		total_width = 0;
	bool		is_varlena = (!stats->attrtype->typbyval &&
							  stats->attrtype->typlen == -1);
	bool		is_varwidth = (!stats->attrtype->typbyval &&
							   stats->attrtype->typlen < 0);
	double		corr_xysum;
	SortSupportData ssup;
	ScalarItem *values;
	int			values_cnt = 0;
	int		   *tupnoLink;
	ScalarMCVItem *track;
	int			track_cnt = 0;
	int			num_mcv = stats->attr->attstattarget;
	int			num_bins = stats->attr->attstattarget;
	StdAnalyzeData *mystats = (StdAnalyzeData *) stats->extra_data;

	values = (ScalarItem *) palloc(samplerows * sizeof(ScalarItem));
	tupnoLink = (int *) palloc(samplerows * sizeof(int));
	track = (ScalarMCVItem *) palloc(num_mcv * sizeof(ScalarMCVItem));

	memset(&ssup, 0, sizeof(ssup));
	ssup.ssup_cxt = CurrentMemoryContext;
	/* We always use the default collation for statistics */
	ssup.ssup_collation = DEFAULT_COLLATION_OID;
	ssup.ssup_nulls_first = false;

	PrepareSortSupportFromOrderingOp(mystats->ltopr, &ssup);

	/* Initialize HLL counter to be stored in stats */
	stats->stahll = (bytea *)gp_hyperloglog_init_def();

	ereport(DEBUG2,
			(errmsg("Computing Scalar Stats for column %s",
					get_attname(stats->attr->attrelid, stats->attr->attnum))));

	/* Initial scan to find sortable values */
	for (i = 0; i < samplerows; i++)
	{
		Datum		value;
		bool		isnull;

		vacuum_delay_point();

		value = fetchfunc(stats, i, &isnull);

		/* Check for null/nonnull */
		if (isnull)
		{
			null_cnt++;
			continue;
		}
		nonnull_cnt++;

		stats->stahll = (bytea *)gp_hyperloglog_add_item((GpHLLCounter) stats->stahll, value, stats->attr->attlen, stats->attr->attbyval, stats->attr->attalign);

		/*
		 * If it's a variable-width field, add up widths for average width
		 * calculation.  Note that if the value is toasted, we use the toasted
		 * width.  We don't bother with this calculation if it's a fixed-width
		 * type.
		 */
		if (is_varlena)
		{
			total_width += VARSIZE_ANY(DatumGetPointer(value));

			/*
			 * If the value is toasted, we want to detoast it just once to
			 * avoid repeated detoastings and resultant excess memory usage
			 * during the comparisons.  Also, check to see if the value is
			 * excessively wide, and if so don't detoast at all --- just
			 * ignore the value.
			 */
			if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
			{
				toowide_cnt++;
				continue;
			}
			value = PointerGetDatum(PG_DETOAST_DATUM(value));
		}
		else if (is_varwidth)
		{
			/* must be cstring */
			total_width += strlen(DatumGetCString(value)) + 1;
		}

		/* Add it to the list to be sorted */
		values[values_cnt].value = value;
		values[values_cnt].tupno = values_cnt;
		tupnoLink[values_cnt] = values_cnt;
		values_cnt++;
	}

	/* We can only compute real stats if we found some sortable values. */
	if (values_cnt > 0)
	{
		int			ndistinct,	/* # distinct values in sample */
					nmultiple,	/* # that appear multiple times */
					num_hist,
					dups_cnt;
		int			slot_idx = 0;
		CompareScalarsContext cxt;

		/* Sort the collected values */
		cxt.ssup = &ssup;
		cxt.tupnoLink = tupnoLink;
		qsort_arg((void *) values, values_cnt, sizeof(ScalarItem),
				  compare_scalars, (void *) &cxt);

		/*
		 * Now scan the values in order, find the most common ones, and also
		 * accumulate ordering-correlation statistics.
		 *
		 * To determine which are most common, we first have to count the
		 * number of duplicates of each value.  The duplicates are adjacent in
		 * the sorted list, so a brute-force approach is to compare successive
		 * datum values until we find two that are not equal. However, that
		 * requires N-1 invocations of the datum comparison routine, which are
		 * completely redundant with work that was done during the sort.  (The
		 * sort algorithm must at some point have compared each pair of items
		 * that are adjacent in the sorted order; otherwise it could not know
		 * that it's ordered the pair correctly.) We exploit this by having
		 * compare_scalars remember the highest tupno index that each
		 * ScalarItem has been found equal to.  At the end of the sort, a
		 * ScalarItem's tupnoLink will still point to itself if and only if it
		 * is the last item of its group of duplicates (since the group will
		 * be ordered by tupno).
		 */
		corr_xysum = 0;
		ndistinct = 0;
		nmultiple = 0;
		dups_cnt = 0;
		for (i = 0; i < values_cnt; i++)
		{
			int			tupno = values[i].tupno;

			corr_xysum += ((double) i) * ((double) tupno);
			dups_cnt++;
			if (tupnoLink[tupno] == tupno)
			{
				/* Reached end of duplicates of this value */
				ndistinct++;
				if (dups_cnt > 1)
				{
					nmultiple++;
					if (track_cnt < num_mcv ||
						dups_cnt > track[track_cnt - 1].count)
					{
						/*
						 * Found a new item for the mcv list; find its
						 * position, bubbling down old items if needed. Loop
						 * invariant is that j points at an empty/ replaceable
						 * slot.
						 */
						int			j;

						if (track_cnt < num_mcv)
							track_cnt++;
						for (j = track_cnt - 1; j > 0; j--)
						{
							if (dups_cnt <= track[j - 1].count)
								break;
							track[j].count = track[j - 1].count;
							track[j].first = track[j - 1].first;
						}
						track[j].count = dups_cnt;
						track[j].first = i + 1 - dups_cnt;
					}
				}
				dups_cnt = 0;
			}
		}

		stats->stats_valid = true;
		/* Do the simple null-frac and width stats */
		stats->stanullfrac = (double) null_cnt / (double) samplerows;
		if (is_varwidth)
			stats->stawidth = total_width / (double) nonnull_cnt;
		else
			stats->stawidth = stats->attrtype->typlen;

		// interpolate NDV calculation based on the hll distinct count
		// for each column in leaf partitions which will be used later
		// to merge root stats
		((GpHLLCounter) (stats->stahll))->nmultiples = nmultiple;
		((GpHLLCounter) (stats->stahll))->ndistinct = ndistinct;
		((GpHLLCounter) (stats->stahll))->samplerows = samplerows;

		if (nmultiple == 0)
		{
			/*
			 * If we found no repeated non-null values, assume it's a unique
			 * column; but be sure to discount for any nulls we found.
			 */
			stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
		}
		else if (toowide_cnt == 0 && nmultiple == ndistinct)
		{
			/*
			 * Every value in the sample appeared more than once.  Assume the
			 * column has just these values.
			 */
			stats->stadistinct = ndistinct;
		}
		else
		{
			/*----------
			 * Estimate the number of distinct values using the estimator
			 * proposed by Haas and Stokes in IBM Research Report RJ 10025:
			 *		n*d / (n - f1 + f1*n/N)
			 * where f1 is the number of distinct values that occurred
			 * exactly once in our sample of n rows (from a total of N),
			 * and d is the total number of distinct values in the sample.
			 * This is their Duj1 estimator; the other estimators they
			 * recommend are considerably more complex, and are numerically
			 * very unstable when n is much smaller than N.
			 *
			 * Overwidth values are assumed to have been distinct.
			 *----------
			 */
			int			f1 = ndistinct - nmultiple + toowide_cnt;
			int			d = f1 + nmultiple;
			double		numer,
						denom,
						stadistinct;

			numer = (double) samplerows *(double) d;

			denom = (double) (samplerows - f1) +
				(double) f1 *(double) samplerows / totalrows;

			stadistinct = numer / denom;
			/* Clamp to sane range in case of roundoff error */
			if (stadistinct < (double) d)
				stadistinct = (double) d;
			if (stadistinct > totalrows)
				stadistinct = totalrows;
			stats->stadistinct = floor(stadistinct + 0.5);
		}

		/*
		 * For FULLSCAN HLL, get ndistinct from the GpHLLCounter
		 * instead of computing it
		 */
		if (stats->stahll_full != NULL)
		{
			GpHLLCounter hLLFull = (GpHLLCounter) DatumGetByteaP(stats->stahll_full);
			GpHLLCounter hllFull_copy = gp_hll_copy(hLLFull);
			stats->stadistinct = round(gp_hyperloglog_estimate(hllFull_copy));
			pfree(hllFull_copy);
			if ((fabs(totalrows - stats->stadistinct) / (float) totalrows) < 0.05)
			{
				stats->stadistinct = -1;
			}

		}
		/*
		 * If we estimated the number of distinct values at more than 10% of
		 * the total row count (a very arbitrary limit), then assume that
		 * stadistinct should scale with the row count rather than be a fixed
		 * value.
		 */
		if (stats->stadistinct > 0.1 * totalrows)
			stats->stadistinct = -(stats->stadistinct / totalrows);

		/*
		 * Decide how many values are worth storing as most-common values. If
		 * we are able to generate a complete MCV list (all the values in the
		 * sample will fit, and we think these are all the ones in the table),
		 * then do so.  Otherwise, store only those values that are
		 * significantly more common than the (estimated) average. We set the
		 * threshold rather arbitrarily at 25% more than average, with at
		 * least 2 instances in the sample.  Also, we won't suppress values
		 * that have a frequency of at least 1/K where K is the intended
		 * number of histogram bins; such values might otherwise cause us to
		 * emit duplicate histogram bin boundaries.  (We might end up with
		 * duplicate histogram entries anyway, if the distribution is skewed;
		 * but we prefer to treat such values as MCVs if at all possible.)
		 */
		if (track_cnt == ndistinct && toowide_cnt == 0 &&
			stats->stadistinct > 0 &&
			track_cnt <= num_mcv)
		{
			/* Track list includes all values seen, and all will fit */
			num_mcv = track_cnt;
		}
		else
		{
			double		ndistinct = stats->stadistinct;
			double		avgcount,
						mincount,
						maxmincount;

			if (ndistinct < 0)
				ndistinct = -ndistinct * totalrows;
			/* estimate # of occurrences in sample of a typical value */
			avgcount = (double) samplerows / ndistinct;
			/* set minimum threshold count to store a value */
			mincount = avgcount * 1.25;
			if (mincount < 2)
				mincount = 2;
			/* don't let threshold exceed 1/K, however */
			maxmincount = (double) samplerows / (double) num_bins;
			if (mincount > maxmincount)
				mincount = maxmincount;
			if (num_mcv > track_cnt)
				num_mcv = track_cnt;
			for (i = 0; i < num_mcv; i++)
			{
				if (track[i].count < mincount)
				{
					num_mcv = i;
					break;
				}
			}
		}

		/* Generate MCV slot entry */
		if (num_mcv > 0)
		{
			MemoryContext old_context;
			Datum	   *mcv_values;
			float4	   *mcv_freqs;

			/* Must copy the target values into anl_context */
			old_context = MemoryContextSwitchTo(stats->anl_context);
			mcv_values = (Datum *) palloc(num_mcv * sizeof(Datum));
			mcv_freqs = (float4 *) palloc(num_mcv * sizeof(float4));
			for (i = 0; i < num_mcv; i++)
			{
				mcv_values[i] = datumCopy(values[track[i].first].value,
										  stats->attrtype->typbyval,
										  stats->attrtype->typlen);
				mcv_freqs[i] = (double) track[i].count / (double) samplerows;
			}
			MemoryContextSwitchTo(old_context);

			stats->stakind[slot_idx] = STATISTIC_KIND_MCV;
			stats->staop[slot_idx] = mystats->eqopr;
			stats->stanumbers[slot_idx] = mcv_freqs;
			stats->numnumbers[slot_idx] = num_mcv;
			stats->stavalues[slot_idx] = mcv_values;
			stats->numvalues[slot_idx] = num_mcv;

			/*
			 * Accept the defaults for stats->statypid and others. They have
			 * been set before we were called (see vacuum.h)
			 */
			slot_idx++;
		}

		/*
		 * Generate a histogram slot entry if there are at least two distinct
		 * values not accounted for in the MCV list.  (This ensures the
		 * histogram won't collapse to empty or a singleton.)
		 */
		num_hist = ndistinct - num_mcv;
		if (num_hist > num_bins)
			num_hist = num_bins + 1;
		if (num_hist >= 2)
		{
			MemoryContext old_context;
			Datum	   *hist_values;
			int			nvals;
			int			pos,
						posfrac,
						delta,
						deltafrac;

			/* Sort the MCV items into position order to speed next loop */
			qsort((void *) track, num_mcv,
				  sizeof(ScalarMCVItem), compare_mcvs);

			/*
			 * Collapse out the MCV items from the values[] array.
			 *
			 * Note we destroy the values[] array here... but we don't need it
			 * for anything more.  We do, however, still need values_cnt.
			 * nvals will be the number of remaining entries in values[].
			 */
			if (num_mcv > 0)
			{
				int			src,
							dest;
				int			j;

				src = dest = 0;
				j = 0;			/* index of next interesting MCV item */
				while (src < values_cnt)
				{
					int			ncopy;

					if (j < num_mcv)
					{
						int			first = track[j].first;

						if (src >= first)
						{
							/* advance past this MCV item */
							src = first + track[j].count;
							j++;
							continue;
						}
						ncopy = first - src;
					}
					else
						ncopy = values_cnt - src;
					memmove(&values[dest], &values[src],
							ncopy * sizeof(ScalarItem));
					src += ncopy;
					dest += ncopy;
				}
				nvals = dest;
			}
			else
				nvals = values_cnt;
			Assert(nvals >= num_hist);

			/* Must copy the target values into anl_context */
			old_context = MemoryContextSwitchTo(stats->anl_context);
			hist_values = (Datum *) palloc(num_hist * sizeof(Datum));

			/*
			 * The object of this loop is to copy the first and last values[]
			 * entries along with evenly-spaced values in between.  So the
			 * i'th value is values[(i * (nvals - 1)) / (num_hist - 1)].  But
			 * computing that subscript directly risks integer overflow when
			 * the stats target is more than a couple thousand.  Instead we
			 * add (nvals - 1) / (num_hist - 1) to pos at each step, tracking
			 * the integral and fractional parts of the sum separately.
			 */
			delta = (nvals - 1) / (num_hist - 1);
			deltafrac = (nvals - 1) % (num_hist - 1);
			pos = posfrac = 0;

			for (i = 0; i < num_hist; i++)
			{
				hist_values[i] = datumCopy(values[pos].value,
										   stats->attrtype->typbyval,
										   stats->attrtype->typlen);
				pos += delta;
				posfrac += deltafrac;
				if (posfrac >= (num_hist - 1))
				{
					/* fractional part exceeds 1, carry to integer part */
					pos++;
					posfrac -= (num_hist - 1);
				}
			}

			MemoryContextSwitchTo(old_context);

			stats->stakind[slot_idx] = STATISTIC_KIND_HISTOGRAM;
			stats->staop[slot_idx] = mystats->ltopr;
			stats->stavalues[slot_idx] = hist_values;
			stats->numvalues[slot_idx] = num_hist;

			/*
			 * Accept the defaults for stats->statypid and others. They have
			 * been set before we were called (see vacuum.h)
			 */
			slot_idx++;
		}

		/* Generate a correlation entry if there are multiple values */
		/*
		 * GPDB: Don't calculate correlation for AO-tables, however.
		 * The rows are not necessarily in the order that our sampling
		 * query returned them, for an append-only table.
		 */
		if (values_cnt > 1 && stats->relstorage == RELSTORAGE_HEAP)
		{
			MemoryContext old_context;
			float4	   *corrs;
			double		corr_xsum,
						corr_x2sum;

			/* Must copy the target values into anl_context */
			old_context = MemoryContextSwitchTo(stats->anl_context);
			corrs = (float4 *) palloc(sizeof(float4));
			MemoryContextSwitchTo(old_context);

			/*----------
			 * Since we know the x and y value sets are both
			 *		0, 1, ..., values_cnt-1
			 * we have sum(x) = sum(y) =
			 *		(values_cnt-1)*values_cnt / 2
			 * and sum(x^2) = sum(y^2) =
			 *		(values_cnt-1)*values_cnt*(2*values_cnt-1) / 6.
			 *----------
			 */
			corr_xsum = ((double) (values_cnt - 1)) *
				((double) values_cnt) / 2.0;
			corr_x2sum = ((double) (values_cnt - 1)) *
				((double) values_cnt) * (double) (2 * values_cnt - 1) / 6.0;

			/* And the correlation coefficient reduces to */
			corrs[0] = (values_cnt * corr_xysum - corr_xsum * corr_xsum) /
				(values_cnt * corr_x2sum - corr_xsum * corr_xsum);

			stats->stakind[slot_idx] = STATISTIC_KIND_CORRELATION;
			stats->staop[slot_idx] = mystats->ltopr;
			stats->stanumbers[slot_idx] = corrs;
			stats->numnumbers[slot_idx] = 1;
			slot_idx++;
		}
	}
	else if (nonnull_cnt > 0)
	{
		/* We found some non-null values, but they were all too wide */
		Assert(nonnull_cnt == toowide_cnt);
		stats->stats_valid = true;
		/* Do the simple null-frac and width stats */
		stats->stanullfrac = (double) null_cnt / (double) samplerows;
		if (is_varwidth)
			stats->stawidth = total_width / (double) nonnull_cnt;
		else
			stats->stawidth = stats->attrtype->typlen;
		/* Assume all too-wide values are distinct, so it's a unique column */
		stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
	}
	else if (null_cnt > 0)
	{
		/* We found only nulls; assume the column is entirely null */
		stats->stats_valid = true;
		stats->stanullfrac = 1.0;
		if (is_varwidth)
			stats->stawidth = 0;	/* "unknown" */
		else
			stats->stawidth = stats->attrtype->typlen;
		stats->stadistinct = 0.0;		/* "unknown" */
	}
	else
	{
		/*
		 * ORCA complains if a column has no statistics whatsoever, so store
		 * either the best we can figure out given what we have, or zero in
		 * case we don't have enough.
		 */
		stats->stats_valid = true;
		if (samplerows)
			stats->stanullfrac = (double) null_cnt / (double) samplerows;
		else
			stats->stanullfrac = 0.0;
		if (is_varwidth)
			stats->stawidth = 0;	/* "unknown" */
		else
			stats->stawidth = stats->attrtype->typlen;
		stats->stadistinct = 0.0;		/* "unknown" */
	}

	/* We don't need to bother cleaning up any of our temporary palloc's */
}

/*
 *	merge_leaf_stats() -- merge leaf stats for the root
 *
 *	We use this when we can find "=" and "<" operators for the datatype.
 *
 *	This is only used when the relation is the root partition and merges
 *	the statistics available in pg_statistic for the leaf partitions.
 *
 *	We determine the fraction of non-null rows, the average width, the
 *	most common values, the (estimated) number of distinct values, the
 *	distribution histogram.
 */
static void
merge_leaf_stats(VacAttrStatsP stats,
				 AnalyzeAttrFetchFunc fetchfunc,
				 int samplerows,
				 double totalrows)
{
	PartitionNode *pn =
		get_parts(stats->attr->attrelid, 0 /*level*/, 0 /*parent*/,
				  false /* inctemplate */, true /*includesubparts*/);
	Assert(pn);
	ereport(DEBUG2,
			(errmsg("Merging leaf partition stats to calculate root partition stats : column %s",
					get_attname(stats->attr->attrelid, stats->attr->attnum))));

	List *oid_list = all_leaf_partition_relids(pn); /* all leaves */
	StdAnalyzeData *mystats = (StdAnalyzeData *) stats->extra_data;
	int numPartitions = list_length(oid_list);

	ListCell *lc;
	float *relTuples = (float *) palloc0(sizeof(float) * numPartitions);
	float *nDistincts = (float *) palloc0(sizeof(float) * numPartitions);
	float *nMultiples = (float *) palloc0(sizeof(float) * numPartitions);
	int relNum = 0;
	float totalTuples = 0;
	float nmultiple = 0; // number of values that appeared more than once
	bool allDistinct = false;
	int slot_idx = 0;
	int sampleCount = 0;
	Oid ltopr = mystats->ltopr;
	Oid eqopr = mystats->eqopr;

	foreach (lc, oid_list)
	{
		Oid pkrelid = lfirst_oid(lc);

		relTuples[relNum] = get_rel_reltuples(pkrelid);
		totalTuples = totalTuples + relTuples[relNum];
		relNum++;
	}

	if (totalTuples == 0.0)
		return;

	MemoryContext old_context;

	HeapTuple *heaptupleStats =
		(HeapTuple *) palloc(numPartitions * sizeof(HeapTuple *));

	// NDV calculations
	float4 colAvgWidth = 0;
	float4 nullCount = 0;
	GpHLLCounter *hllcounters = (GpHLLCounter *) palloc0(numPartitions * sizeof(GpHLLCounter));
	GpHLLCounter *hllcounters_fullscan = (GpHLLCounter *) palloc0(numPartitions * sizeof(GpHLLCounter));
	GpHLLCounter *hllcounters_copy = (GpHLLCounter *) palloc0(numPartitions * sizeof(GpHLLCounter));

	GpHLLCounter finalHLL = NULL;
	GpHLLCounter finalHLLFull = NULL;
	int i = 0;
	double ndistinct = 0.0;
	int fullhll_count = 0;
	int samplehll_count = 0;
	int totalhll_count = 0;
	foreach (lc, oid_list)
	{
		Oid relid = lfirst_oid(lc);
		colAvgWidth =
			colAvgWidth +
			get_attavgwidth(relid, stats->attr->attnum) * relTuples[i];
		nullCount = nullCount +
					get_attnullfrac(relid, stats->attr->attnum) * relTuples[i];

		const char *attname = get_relid_attribute_name(stats->attr->attrelid, stats->attr->attnum);
		AttrNumber child_attno = get_attnum(relid, attname);

		heaptupleStats[i] = get_att_stats(relid, child_attno);

		// if there is no colstats, we can skip this partition's stats
		if (!HeapTupleIsValid(heaptupleStats[i]))
		{
			i++;
			continue;
		}

		AttStatsSlot hllSlot;

		get_attstatsslot(&hllSlot, heaptupleStats[i], STATISTIC_KIND_FULLHLL,
						 InvalidOid, ATTSTATSSLOT_VALUES);

		if (hllSlot.nvalues > 0)
		{
			hllcounters_fullscan[i] = (GpHLLCounter) DatumGetByteaP(hllSlot.values[0]);
			GpHLLCounter finalHLLFull_intermediate = finalHLLFull;
			finalHLLFull = gp_hyperloglog_merge_counters(finalHLLFull_intermediate, hllcounters_fullscan[i]);
			if (NULL != finalHLLFull_intermediate)
			{
				pfree(finalHLLFull_intermediate);
			}
			free_attstatsslot(&hllSlot);
			fullhll_count++;
			totalhll_count++;
		}

		get_attstatsslot(&hllSlot, heaptupleStats[i], STATISTIC_KIND_HLL,
						 InvalidOid, ATTSTATSSLOT_VALUES);

		if (hllSlot.nvalues > 0)
		{
			hllcounters[i] = (GpHLLCounter) DatumGetByteaP(hllSlot.values[0]);
			nDistincts[i] = (float) hllcounters[i]->ndistinct;
			nMultiples[i] = (float) hllcounters[i]->nmultiples;
			sampleCount += hllcounters[i]->samplerows;
			hllcounters_copy[i] = gp_hll_copy(hllcounters[i]);
			GpHLLCounter finalHLL_intermediate = finalHLL;
			finalHLL = gp_hyperloglog_merge_counters(finalHLL_intermediate, hllcounters[i]);
			if (NULL != finalHLL_intermediate)
			{
				pfree(finalHLL_intermediate);
			}
			free_attstatsslot(&hllSlot);
			samplehll_count++;
			totalhll_count++;
		}
		i++;
	}

	if (totalhll_count == 0)
	{
		/*
		 * If neither HLL nor HLL Full scan stats are available,
		 * continue merging stats based on the defaults, instead
		 * of reading them from HLL counter.
		 */
	}
	else
	{
		/*
		 * If all partitions have HLL full scan counters,
		 * merge root NDV's based on leaf partition HLL full scan
		 * counter
		 */
		if (fullhll_count == totalhll_count)
		{
			ndistinct = gp_hyperloglog_estimate(finalHLLFull);
			pfree(finalHLLFull);
			/*
			 * For fullscan the ndistinct is calculated based on the entire table scan
			 * so if it's within the marginal error, we consider everything as distinct,
			 * else the ndistinct value will provide the actual value and we do not ,
			 * need to do any additional calculation for the nmultiple
			 */
			if ((fabs(totalTuples - ndistinct) / (float) totalTuples) < GP_HLL_ERROR_MARGIN)
			{
				allDistinct = true;
			}
			nmultiple = ndistinct;
		}
		/*
		 * Else if all partitions have HLL counter based on sampled data,
		 * merge root NDV's based on leaf partition HLL counter on
		 * sampled data
		 */
		else if (finalHLL != NULL && samplehll_count == totalhll_count)
		{
			ndistinct = gp_hyperloglog_estimate(finalHLL);
			pfree(finalHLL);
			/*
			 * For sampled HLL counter, the ndistinct calculated is based on the
			 * sampled data. We consider everything distinct if the ndistinct
			 * calculated is within marginal error, else we need to calculate
			 * the number of distinct values for the table based on the estimator
			 * proposed by Haas and Stokes, used later in the code.
			 */
			if ((fabs(sampleCount - ndistinct) / (float) sampleCount) < GP_HLL_ERROR_MARGIN)
			{
				allDistinct = true;
			}
			else
			{
				/*
				 * The gp_hyperloglog_estimate() utility merges the number of
				 * distnct values accurately, but for the NDV estimator used later
				 * in the code, we also need additional information for nmultiples,
				 * i.e., the number of values that appeared more than once.
				 * At this point we have the information for nmultiples for each
				 * partition, but the nmultiples in one partition can be accounted as
				 * a distinct value in some other partition. In order to merge the
				 * approximate nmultiples better, we extract unique values in each
				 * partition as follows,
				 * P1 -> ndistinct1 , nmultiple1
				 * P2 -> ndistinct2 , nmultiple2
				 * P3 -> ndistinct3 , nmultiple3
				 * Root -> ndistinct(Root) (using gp_hyperloglog_estimate)
				 * nunique1 = ndistinct(Root) - gp_hyperloglog_estimate(P2 & P3)
				 * nunique2 = ndistinct(Root) - gp_hyperloglog_estimate(P1 & P3)
				 * nunique3 = ndistinct(Root) - gp_hyperloglog_estimate(P2 & P1)
				 * And finally once we have unique values in individual partitions,
				 * we can get the nmultiples on the ROOT as seen below,
				 * nmultiple(Root) = ndistinct(Root) - (sum of uniques in each partition)
				 */
				/*
				 * hllcounters_left array stores the merged hll result of all the
				 * hll counters towards the left of index i and excluding the hll
				 * counter at index i
				 */
				GpHLLCounter *hllcounters_left = (GpHLLCounter *) palloc0(numPartitions * sizeof(GpHLLCounter));

				/*
				 * hllcounters_right array stores the merged hll result of all the
				 * hll counters towards the right of index i and excluding the hll
				 * counter at index i
				 */
				GpHLLCounter *hllcounters_right = (GpHLLCounter *) palloc0(numPartitions * sizeof(GpHLLCounter));

				hllcounters_left[0] = gp_hyperloglog_init_def();
				hllcounters_right[numPartitions - 1] = gp_hyperloglog_init_def();

				/*
				 * The following loop populates the left and right array by accumulating the merged
				 * result of all the hll counters towards the left/right of the given index i excluding
				 * the counter at index i.
				 * Note that there might be empty values for some partitions, in which case the
				 * corresponding element in the left/right arrays will simply be the value
				 * of its neighbor.
				 * For E.g If the hllcounters_copy array is 1, null, 2, 3, null, 4
				 * the left and right arrays will be as follows:
				 * hllcounters_left:  default, 1, 1, (1,2), (1,2,3), (1,2,3)
				 * hllcounters_right: (2,3,4), (2,3,4), (3,4), 4, 4, default
				 */
				/*
				 * The first and the last element in the left and right arrays
				 * are default values since there is no element towards
				 * the left or right of them
				 */
				for (i = 1; i < numPartitions; i++)
				{
					/* populate left array */
					if (nDistincts[i - 1] == 0)
					{
						hllcounters_left[i] = gp_hll_copy(hllcounters_left[i - 1]);
					}
					else
					{
						GpHLLCounter hllcounter_temp1 = gp_hll_copy(hllcounters_copy[i - 1]);
						GpHLLCounter hllcounter_temp2 = gp_hll_copy(hllcounters_left[i - 1]);
						hllcounters_left[i] = gp_hyperloglog_merge_counters(hllcounter_temp1, hllcounter_temp2);
						pfree(hllcounter_temp1);
						pfree(hllcounter_temp2);
					}

					/* populate right array */
					if (nDistincts[numPartitions - i] == 0)
					{
						hllcounters_right[numPartitions - i - 1] = gp_hll_copy(hllcounters_right[numPartitions - i]);
					}
					else
					{
						GpHLLCounter hllcounter_temp1 = gp_hll_copy(hllcounters_copy[numPartitions - i]);
						GpHLLCounter hllcounter_temp2 = gp_hll_copy(hllcounters_right[numPartitions - i]);
						hllcounters_right[numPartitions - i - 1] = gp_hyperloglog_merge_counters(hllcounter_temp1, hllcounter_temp2);
						pfree(hllcounter_temp1);
						pfree(hllcounter_temp2);
					}
				}

				int nUnique = 0;
				for (i = 0; i < numPartitions; i++)
				{
					/* Skip if statistics are missing for the partition */
					if (nDistincts[i] == 0)
						continue;

					GpHLLCounter hllcounter_temp1 = gp_hll_copy(hllcounters_left[i]);
					GpHLLCounter hllcounter_temp2 = gp_hll_copy(hllcounters_right[i]);
					GpHLLCounter final = NULL;
					final = gp_hyperloglog_merge_counters(hllcounter_temp1, hllcounter_temp2);

					pfree(hllcounter_temp1);
					pfree(hllcounter_temp2);

					if (final != NULL)
					{
						float nUniques = ndistinct - gp_hyperloglog_estimate(final);
						nUnique += nUniques;
						nmultiple += nMultiples[i] * (nUniques / nDistincts[i]);
						pfree(final);
					}
					else
					{
						nUnique = ndistinct;
						break;
					}
				}

				// nmultiples for the ROOT
				nmultiple += ndistinct - nUnique;

				if (nmultiple < 0)
					nmultiple = 0;

				pfree(hllcounters_left);
				pfree(hllcounters_right);
			}
		}
		else
		{
			/* Else error out due to incompatible leaf HLL counter merge */
			pfree(hllcounters);
			pfree(hllcounters_fullscan);
			pfree(hllcounters_copy);
			pfree(nDistincts);
			pfree(nMultiples);

			ereport(ERROR,
					(errmsg("ANALYZE cannot merge since not all non-empty leaf partitions have consistent hyperloglog statistics for merge"),
					 errhint("Re-run ANALYZE or ANALYZE FULLSCAN")));
		}
	}
	pfree(hllcounters);
	pfree(hllcounters_fullscan);
	pfree(hllcounters_copy);
	pfree(nDistincts);
	pfree(nMultiples);

	if (allDistinct || (!OidIsValid(eqopr) && !OidIsValid(ltopr)))
	{
		/* If we found no repeated values, assume it's a unique column */
		ndistinct = -1.0;
	}
	else if ((int) nmultiple >= (int) ndistinct)
	{
		/*
		 * Every value in the sample appeared more than once.  Assume the
		 * column has just these values.
		 */
	}
	else
	{
		/*----------
		 * Estimate the number of distinct values using the estimator
		 * proposed by Haas and Stokes in IBM Research Report RJ 10025:
		 *		n*d / (n - f1 + f1*n/N)
		 * where f1 is the number of distinct values that occurred
		 * exactly once in our sample of n rows (from a total of N),
		 * and d is the total number of distinct values in the sample.
		 * This is their Duj1 estimator; the other estimators they
		 * recommend are considerably more complex, and are numerically
		 * very unstable when n is much smaller than N.
		 *
		 * Overwidth values are assumed to have been distinct.
		 *----------
		 */
		int f1 = ndistinct - nmultiple;
		int d = f1 + nmultiple;
		double numer, denom, stadistinct;

		numer = (double) sampleCount * (double) d;

		denom = (double) (sampleCount - f1) +
				(double) f1 * (double) sampleCount / totalTuples;

		stadistinct = numer / denom;
		/* Clamp to sane range in case of roundoff error */
		if (stadistinct < (double) d)
			stadistinct = (double) d;
		if (stadistinct > totalTuples)
			stadistinct = totalTuples;
		ndistinct = floor(stadistinct + 0.5);
	}

	ndistinct = round(ndistinct);
	if (ndistinct > 0.1 * totalTuples)
		ndistinct = -(ndistinct / totalTuples);

	// finalize NDV calculation
	stats->stadistinct = ndistinct;
	stats->stats_valid = true;
	stats->stawidth = colAvgWidth / totalTuples;
	stats->stanullfrac = (float4) nullCount / (float4) totalTuples;

	// MCV calculations
	MCVFreqPair **mcvpairArray = NULL;
	int rem_mcv = 0;
	int num_mcv = 0;
	if (ndistinct > -1 && OidIsValid(eqopr))
	{
		if (ndistinct < 0)
		{
			ndistinct = -ndistinct * totalTuples;
		}

		old_context = MemoryContextSwitchTo(stats->anl_context);

		void *resultMCV[2];

		mcvpairArray = aggregate_leaf_partition_MCVs(
			stats->attr->attrelid, stats->attr->attnum, heaptupleStats,
			relTuples, default_statistics_target, ndistinct, &num_mcv, &rem_mcv,
			resultMCV);
		MemoryContextSwitchTo(old_context);

		if (num_mcv > 0)
		{
			stats->stakind[slot_idx] = STATISTIC_KIND_MCV;
			stats->staop[slot_idx] = mystats->eqopr;
			stats->stavalues[slot_idx] = (Datum *) resultMCV[0];
			stats->numvalues[slot_idx] = num_mcv;
			stats->stanumbers[slot_idx] = (float4 *) resultMCV[1];
			stats->numnumbers[slot_idx] = num_mcv;
			slot_idx++;
		}
	}

	// Histogram calculation
	if (OidIsValid(eqopr) && OidIsValid(ltopr))
	{
		old_context = MemoryContextSwitchTo(stats->anl_context);

		void *resultHistogram[1];
		int num_hist = aggregate_leaf_partition_histograms(
			stats->attr->attrelid, stats->attr->attnum, heaptupleStats,
			relTuples, default_statistics_target, mcvpairArray + num_mcv,
			rem_mcv, resultHistogram);
		MemoryContextSwitchTo(old_context);
		if (num_hist > 0)
		{
			stats->stakind[slot_idx] = STATISTIC_KIND_HISTOGRAM;
			stats->staop[slot_idx] = mystats->ltopr;
			stats->stavalues[slot_idx] = (Datum *) resultHistogram[0];
			stats->numvalues[slot_idx] = num_hist;
			slot_idx++;
		}
	}
	for (i = 0; i < numPartitions; i++)
	{
		if (HeapTupleIsValid(heaptupleStats[i]))
			heap_freetuple(heaptupleStats[i]);
	}
	if (num_mcv > 0)
		pfree(mcvpairArray);
	pfree(heaptupleStats);
	pfree(relTuples);
}
/*
 * qsort_arg comparator for sorting ScalarItems
 *
 * Aside from sorting the items, we update the tupnoLink[] array
 * whenever two ScalarItems are found to contain equal datums.  The array
 * is indexed by tupno; for each ScalarItem, it contains the highest
 * tupno that that item's datum has been found to be equal to.  This allows
 * us to avoid additional comparisons in compute_scalar_stats().
 */
static int
compare_scalars(const void *a, const void *b, void *arg)
{
	Datum		da = ((const ScalarItem *) a)->value;
	int			ta = ((const ScalarItem *) a)->tupno;
	Datum		db = ((const ScalarItem *) b)->value;
	int			tb = ((const ScalarItem *) b)->tupno;
	CompareScalarsContext *cxt = (CompareScalarsContext *) arg;
	int			compare;

	compare = ApplySortComparator(da, false, db, false, cxt->ssup);
	if (compare != 0)
		return compare;

	/*
	 * The two datums are equal, so update cxt->tupnoLink[].
	 */
	if (cxt->tupnoLink[ta] < tb)
		cxt->tupnoLink[ta] = tb;
	if (cxt->tupnoLink[tb] < ta)
		cxt->tupnoLink[tb] = ta;

	/*
	 * For equal datums, sort by tupno
	 */
	return ta - tb;
}

/*
 * qsort comparator for sorting ScalarMCVItems by position
 */
static int
compare_mcvs(const void *a, const void *b)
{
	int			da = ((const ScalarMCVItem *) a)->first;
	int			db = ((const ScalarMCVItem *) b)->first;

	return da - db;
}
