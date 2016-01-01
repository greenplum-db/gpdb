/*-------------------------------------------------------------------------
 *
 * analyze.c
 *	  the Postgres statistics generator
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * The new ANALYZE implementation uses in-memory processing of the sampled
 * columns to compute column-level statistics. This enables a significant
 * performance speed up over the previous approach via SPI calls.
 *
 * It uses SPI under the covers to sample the base table and to read sampled
 * columns into an array of datums in memory. The main point of
 * entry is the function analyzeStatement. This function does the
 * heavy lifting of setting up transactions and getting locks.
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/analyze.c,v 1.103 2007/01/09 02:14:11 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "catalog/catquery.h"
#include "catalog/heap.h"
#include "access/tuptoaster.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbheap.h"
#include "cdb/cdbhash.h"
#include "commands/vacuum.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "cdb/cdbvars.h"
#include "storage/backendid.h"
#include "executor/spi.h"
#include "postmaster/autovacuum.h"
#include "cdb/cdbtm.h"
#include "catalog/pg_namespace.h"
#include "nodes/makefuncs.h"

/**
 * This struct contains statistics produced during ANALYZE
 * on a column. 
 */
typedef struct AttributeStatistics
{
	float4 		ndistinct; /* number of distinct values. If less than 0, represents fraction of values that are distinct */
	float4 		nullFraction; /* fraction of values that are null */
	float4 		avgWidth;	/* average width of values */
	ArrayType  	*mcv;		/* most common values */
	ArrayType	*freq;		/* frequencies of most common values */
	ArrayType	*hist;		/* equi-depth histogram bounds */
} AttributeStatistics;

typedef struct
{
    int numColumns;
    MemoryContext memoryContext;
    ArrayType ** output;
} EachResultColumnAsArraySpec;

typedef struct
{
	Datum		value;			/* a data value */
	uint32		tupno;			/* position index for tuple it came from */
} ScalarItem;

typedef struct
{
	int			count;			/* # of duplicates */
	int			first;			/* values[] index of first occurrence */
} ScalarMCVItem;

typedef struct
{
	FmgrInfo   *cmpFn;
	int			cmpFlags;
	uint32	   *tupnoLink;
} CompareScalarsContext;

typedef struct
{
	uint32 ndistinct;
	uint32 nmultiple;
	int track_cnt;
	int num_mcv;
	int num_hist;
	ScalarMCVItem *track;
} IterateScalarsContext;

typedef struct
{
	uint32 values_cnt;		/* number of tuples that are read into the datum array */
	uint32 null_cnt;
	uint32 nonnull_cnt;
	double total_width;
	MemoryContext resultColumnMemContext;
	ScalarItem *values;		/* datum array */
	uint32 *tupnoLink;
} ResultColumnSpec;

/**
 * Logging level.
 */
static int 			elevel = -1;

/* Top level functions */
static void analyzeRelation(Relation relation, List *lAttributeNames, bool rootonly);
static void analyzeComputeAttributeStatistics(Relation rel,
		const char *attributeName,
		float4 relTuples, 
		Oid sampleTableOid,
		AttributeStatistics *stats);
static List* analyzableRelations(bool rootonly);
static bool analyzePermitted(Oid relationOid);
static List *analyzableAttributes(Relation candidateRelation);
static List	*buildExplicitAttributeNames(Oid relationOid, VacuumStmt *stmt);

/* Reltuples/relpages estimation functions */
static void gp_statistics_estimate_reltuples_relpages_heap(Relation rel, float4 *reltuples, float4 *relpages);
static void gp_statistics_estimate_reltuples_relpages_ao_rows(Relation rel, float4 *reltuples, float4 *relpages);
static void gp_statistics_estimate_reltuples_relpages_ao_cs(Relation rel, float4 *reltuples, float4 *relpages);
static void analyzeEstimateReltuplesRelpages(Oid relationOid, float4 *relTuples, float4 *relPages, bool rootonly);
static void analyzeEstimateIndexpages(Oid relationOid, Oid indexOid, float4 *indexPages);

/* Attribute-type related functions */
static bool isOrderedAndHashable(Oid typid, Oid *ltopr);
static bool hasMaxDefined(Oid typid);

/* Sampling related */
static float4 estimateSampleSize(Relation rel, const char *attributeName, float4 relTuples);
static char* temporarySampleTableName(Oid relationOid);
static Oid buildSampleTable(Oid relationOid, 
		List *lAttributeNames, 
		float4	relTuples,
		float4 	requestedSampleSize);
static void dropSampleTable(Oid sampleTableOid);

/* Column statistics */
static void sort_sample_column(ResultColumnSpec *spec, Oid typid, Oid ltopr);
static void iterate_scalars(ResultColumnSpec *spec, IterateScalarsContext *itrCxt);
static void compute_ndistinct(AttributeStatistics *stats,
							IterateScalarsContext *itrCxt,
							uint32 toowide_cnt,
							uint32 sampleTableRelTuples,
							float4 relTuples,
							bool *computeMCV);
static void compute_mcv(AttributeStatistics *stats,
						ResultColumnSpec *spec,
						IterateScalarsContext *itrCxt,
						uint32 sampleTableRelTuples,
						Form_pg_attribute attr);

static void compute_histogram(AttributeStatistics *stats,
						ResultColumnSpec *spec,
						IterateScalarsContext *itrCxt,
						Form_pg_attribute attr);

/* Catalog related */
static void updateAttributeStatisticsInCatalog(Oid relationOid, const char *attributeName, 
		AttributeStatistics *stats);
static void updateReltuplesRelpagesInCatalog(Oid relationOid, float4 relTuples, float4 relPages);

/* Sorting related */
static int compare_scalars(const void *a, const void *b, void *arg);
inline static bool datumCompare(Datum d1, Datum d2, FmgrInfo *finfo);

/* spi execution helpers */
typedef void (*spiCallback)(void *clientDataOut);
static void spiExecuteWithCallback(const char *src, bool read_only, long tcount,
           spiCallback callbackFn, void *clientData);
static void spiCallback_getSampleColumn(void *clientData);
static void spiCallback_getProcessedAsFloat4(void *clientData);
static void spiCallback_getSingleResultRowArrayAsTwoFloat4(void *clientData);

/*
 * To avoid consuming too much memory during analysis and/or too much space
 * in the resulting pg_statistic rows, we ignore varlena datums that are wider
 * than WIDTH_THRESHOLD (after detoasting!).  This is legitimate for MCV
 * and distinct-value calculations since a wide value is unlikely to be
 * duplicated at all, much less be a most-common value.  For the same reason,
 * ignoring wide values will not affect our estimates of histogram bin
 * boundaries very much.
 */
#define COLUMN_WIDTH_THRESHOLD  1024


/**
 * This is the main entry point for analyze execution. Three possible ways of calling this method.
 * 1. Full database ANALYZE. No relations are explicitly specified.
 * 2. List of relations is specified (Usually by autovacuum).
 * 3. One relation is specified (optionally, a list of columns).
 * This method can only be called in DISPATCH or UTILITY roles.
 * Input:
 * 	vacstmt - Vacuum statement.
 * 	relids  - Usually NULL except when called by autovacuum.
 */
void analyzeStatement(VacuumStmt *stmt, List *relids)
{
	/* MPP-14608: Analyze may create temp tables.
	 * Disable autostats so that analyze is not called during their creation. */

	GpAutoStatsModeValue autostatvalBackup = gp_autostats_mode;
	GpAutoStatsModeValue autostatInFunctionsvalBackup = gp_autostats_mode_in_functions;
	bool optimizerBackup = optimizer;

	gp_autostats_mode = GP_AUTOSTATS_NONE;
	gp_autostats_mode_in_functions = GP_AUTOSTATS_NONE;
	optimizer = false;

	PG_TRY();
	{
		analyzeStmt(stmt, relids);
		gp_autostats_mode = autostatvalBackup;
		gp_autostats_mode_in_functions = autostatInFunctionsvalBackup;
		optimizer = optimizerBackup;
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		gp_autostats_mode = autostatvalBackup;
		gp_autostats_mode_in_functions = autostatInFunctionsvalBackup;
		optimizer = optimizerBackup;

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();
	Assert(gp_autostats_mode == autostatvalBackup);
	Assert(gp_autostats_mode_in_functions == autostatInFunctionsvalBackup);
	Assert(optimizer == optimizerBackup);
}

/**
 * This method can only be called in DISPATCH or UTILITY roles.
 * Input:
 * 	vacstmt - Vacuum statement.
 * 	relids  - Usually NULL except when called by autovacuum.
 */
void analyzeStmt(VacuumStmt *stmt, List *relids)
{
	List	   			  	*lRelOids = NIL;
	MemoryContext			callerContext = NULL;
	MemoryContext 			analyzeStatementContext = NULL;
	MemoryContext 			analyzeRelationContext = NULL;
	bool					bUseOwnXacts = false;
	ListCell				*le1 = NULL;

	/**
	 * Ensure that an ANALYZE is requested.
	 */
	Assert(stmt->analyze);	
	
	/**
	 * Ensure that vacuum was not requested.
	 */
	Assert(!stmt->vacuum);
	
	/**
	 * Both relids and stmt->relation cannot be non-null.
	 */
	Assert(!(relids != NIL && stmt->relation != NULL));
	
	/**
	 * Works only in DISPATCH and UTILITY mode.
	 */
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);
	
	/**
	 * Only works in normal processing mode - should not be called in bootstrapping or
	 * init mode.
	 */
	Assert(IsNormalProcessingMode());
	
	/* If running in diagnostic mode, simply return */
	if (Gp_interconnect_type == INTERCONNECT_TYPE_NIL)
	{
		return;
	}
	
	if (stmt->verbose)
		elevel = INFO;
	else
		elevel = DEBUG2;

	callerContext = CurrentMemoryContext;

	/*
	 * This is the statement-level context. This will be cleaned up when we exit this
	 * function.
	 */
	analyzeStatementContext = AllocSetContextCreate(PortalContext,
			"Analyze",
			ALLOCSET_DEFAULT_MINSIZE,
			ALLOCSET_DEFAULT_INITSIZE,
			ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(analyzeStatementContext);


	/*
	 * This is a per relation context.
	 */
	analyzeRelationContext = AllocSetContextCreate(analyzeStatementContext,
			"AnalyzeRel",
			ALLOCSET_DEFAULT_MINSIZE,
			ALLOCSET_DEFAULT_INITSIZE,
			ALLOCSET_DEFAULT_MAXSIZE);

	/**
	 * What relations need to be ANALYZED.
	 */
	if (relids == NIL && stmt->relation == NULL)
	{
		/**
		 * ANALYZE entire DB.
		 */
		lRelOids = analyzableRelations(stmt->rootonly);
		if (stmt->rootonly && NIL == lRelOids)
		{
			ereport(WARNING,
					(errmsg("there are no partitioned tables in database to ANALYZE ROOTPARTITION")));
		}
	}
	else if (relids != NIL)
	{
		/**
		 * ANALYZE called by autovacuum.
		 */
		lRelOids = relids;
	}
	else
	{
		/**
		 * ANALYZE one relation (optionally, a list of columns).
		 */
		Oid relationOid = InvalidOid;
		Assert(relids == NIL);
		Assert(stmt->relation != NULL);
		relationOid = RangeVarGetRelid(stmt->relation, false);
		PartStatus ps = rel_part_status(relationOid);

		if (ps != PART_STATUS_ROOT && stmt->rootonly)
		{
			ereport(WARNING,
					(errmsg("skipping \"%s\" --- cannot analyze a non-root partition using ANALYZE ROOTPARTITION",
							get_rel_name(relationOid))));
		}
		else if (ps == PART_STATUS_ROOT)
		{
			PartitionNode *pn = get_parts(relationOid, 0 /*level*/ ,
		 	 	            0 /*parent*/, false /* inctemplate */, CurrentMemoryContext, true /*includesubparts*/);
			Assert(pn);
			if (!stmt->rootonly)
			{
				lRelOids = all_leaf_partition_relids(pn); /* all leaves */
			}
			lRelOids = lappend_oid(lRelOids, relationOid); /* root partition */
		}
		else if (ps == PART_STATUS_INTERIOR) /* analyze an interior partition directly */
		{
			/* disable analyzing mid-level partitions directly since the users are encouraged
			 * to work with the root partition only. To gather stats on mid-level partitions
			 * (for Orca's use), the user should run ANALYZE or ANALYZE ROOTPARTITION on the
			 * root level.
			 */
			ereport(WARNING,
					(errmsg("skipping \"%s\" --- cannot analyze a mid-level partition. "
							"Please run ANALYZE on the root partition table.",
							get_rel_name(relationOid))));
		}
		else
		{
			lRelOids = list_make1_oid(relationOid);
		}
	}

	/*
	 * Decide whether we need to start/commit our own transactions.
	 * The scenarios in which we can start/commit our own transactions are:
	 * 1. We are not in a transaction block and there are multiple relations specified (some of them may be implicit)
	 * 2. We are in autovacuum mode
	 */

	if ((!IsInTransactionChain((void *) stmt) && list_length(lRelOids) > 1)
			|| IsAutoVacuumProcess())
		bUseOwnXacts = true;

	/**
	 * Iterate through all relids in the list and issue analyze on all columns on each relation.
	 */

	if (bUseOwnXacts)
	{
		/*
		 * We commit the transaction started in PostgresMain() here, and start
		 * another one before exiting to match the commit waiting for us back in
		 * PostgresMain().
		 */
		CommitTransactionCommand();
		MemoryContextSwitchTo(analyzeStatementContext);
	}

	foreach (le1, lRelOids)
	{
		Oid				candidateOid	  = InvalidOid;
		Relation		candidateRelation = NULL;
		bool			bTemp;

		bTemp = false;

		Assert(analyzeStatementContext == CurrentMemoryContext);

		if (bUseOwnXacts)
		{
			/**
			 * We use a different transaction per relation so that we
			 * may release locks on relations as soon as possible.
			 */
			setupRegularDtxContext();
			StartTransactionCommand();
			ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());
			MemoryContextSwitchTo(analyzeStatementContext);
		}

		candidateOid = lfirst_oid(le1);
		candidateRelation =
				try_relation_open(candidateOid, ShareUpdateExclusiveLock, false);

		if (candidateRelation)
		{
			/**
			 * We got a lock on the relation. Good!
			 */
			if (analyzePermitted(RelationGetRelid(candidateRelation)))
			{
				/*
				 * We have permission to ANALYZE.
				 */

				/* MPP-7576: don't track internal namespace tables */
				switch (candidateRelation->rd_rel->relnamespace)
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
					bTemp = isAnyTempNamespace(
							candidateRelation->rd_rel->relnamespace);

				if (candidateRelation->rd_rel->relkind != RELKIND_RELATION)
				{
					/**
					 * Is the relation the right kind?
					 */
					ereport(WARNING,
							(errmsg("skipping \"%s\" --- cannot analyze indexes, views or special system tables",
									RelationGetRelationName(candidateRelation))));
					relation_close(candidateRelation, ShareUpdateExclusiveLock);
				}
				else if (isOtherTempNamespace(RelationGetNamespace(candidateRelation)))
				{
					/* Silently ignore tables that are temp tables of other backends. */
					relation_close(candidateRelation, ShareUpdateExclusiveLock);
				}
				else
				{
					List 		*lAttNames = NIL;

					/* Switch to per relation context */
					MemoryContextSwitchTo(analyzeRelationContext);

					if (stmt->va_cols)
					{
						/**
						 * Column names have been provided. Should have specified relation name as well.
						 */
						Assert(stmt->relation && "Column names specified but not relation name");
						lAttNames = buildExplicitAttributeNames(RelationGetRelid(candidateRelation), stmt);
					}
					else
					{
						lAttNames = analyzableAttributes(candidateRelation);
					}

					analyzeRelation(candidateRelation, lAttNames, stmt->rootonly);

					/* Switch back to statement context and reset relation context */
					MemoryContextSwitchTo(analyzeStatementContext);
					MemoryContextResetAndDeleteChildren(analyzeRelationContext);

					/*
					 * Close source relation now, but keep lock so
					 * that no one deletes it before we commit.  (If
					 * someone did, they'd fail to clean up the
					 * entries we made in pg_statistic.  Also,
					 * releasing the lock before commit would expose
					 * us to concurrent-update failures.)
					 */

					relation_close(candidateRelation, NoLock);

					/* MPP-6929: metadata tracking */
					if (!bTemp && (Gp_role == GP_ROLE_DISPATCH))
					{
						char *asubtype = "";

						if (IsAutoVacuumProcess())
							asubtype = "AUTO";

						MetaTrackUpdObject(RelationRelationId,
								RelationGetRelid(candidateRelation),
								GetUserId(),
								"ANALYZE",
								asubtype
						);
					}
				}
			}
			else
			{
				/**
				 * We don't have permissions to ANALYZE the relation. Print warning and move on
				 * to the next relation.
				 */
				ereport(WARNING,
						(errmsg("Skipping \"%s\" --- only table or database owner can analyze it",
								RelationGetRelationName(candidateRelation))));
				relation_close(candidateRelation, ShareUpdateExclusiveLock);
			} /* if (analyzePermitted(RelationGetRelid(candidateRelation))) */
		}
		else
		{
			/*
			 * Relation may have been dropped out from under us.
			 * TODO: should we print a warning here? Do we print it during
			 * ANALYZE DB or AutoVacuum?
			 */
		} /* if (candidateRelation) */

		if (bUseOwnXacts)
		{
			/**
			 * We commit the transaction so that locks on the relation may be released.
			 */
			CommitTransactionCommand();
			MemoryContextSwitchTo(analyzeStatementContext);
		}
	}

	if (bUseOwnXacts)
	{
		/**
		 * We start a new transaction command to match the one in PostgresMain().
		 */
		setupRegularDtxContext();
		StartTransactionCommand();
		ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());
		MemoryContextSwitchTo(analyzeStatementContext);
	}

	Assert(analyzeStatementContext == CurrentMemoryContext);
	MemoryContextSwitchTo(callerContext);
	MemoryContextDelete(analyzeStatementContext);
}


/*
 * This method extracts the explicit attributes listed in a vacuum statement. It must
 * be called only when it is known that explicit columns have been specified in the vacuum
 * statement. It checks if attribute exists and also if attribute has been dropped. It
 * also silently drops attributes that have stats target set to 0.
 * Input:
 * 	vacstmt - vacuum statement
 * Output:
 * 	list of attribute names that the vacuum statement requests
 */
static List*	buildExplicitAttributeNames(Oid relationOid, VacuumStmt *stmt)
{
	List	*lExplicitAttNames = NIL;
	ListCell *le = NULL;
	Assert(stmt->va_cols != NULL);
	/**
	 * va_col contains list of attributes that need to be analyzed. 
	 */
	foreach (le, stmt->va_cols)
	{
		HeapTuple	attributeTuple;
		cqContext  *pcqCtx;
		char *attributeName = strVal(lfirst(le));
		Assert(attributeName);

		pcqCtx = caql_getattname_scan(NULL, relationOid, attributeName);

		attributeTuple = caql_get_current(pcqCtx);

		/* Ensure that we can actually analyze the attribute. */
		if (HeapTupleIsValid(attributeTuple))
		{
			Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(attributeTuple);
			/* If attribute is dropped, print an error message */
			if (att_tup->attisdropped)
				ereport(ERROR, 
						(errcode(ERRCODE_UNDEFINED_COLUMN),
								errmsg("Attribute %s has been dropped in relation %s.", 
										attributeName, get_rel_name(relationOid))));						

			/* If statstarget=0 or type is "unknown", silently skip it. */
			if (att_tup->attstattarget != 0
					&& att_tup->atttypid != UNKNOWNOID)
			{
				lExplicitAttNames = lappend(lExplicitAttNames, attributeName);
			}
		}
		else
		{
			/* Attribute does not exist in relation. Error out */
			ereport(ERROR, 
					(errcode(ERRCODE_UNDEFINED_COLUMN),
							errmsg("Relation %s does not have an attribute named %s.", 
									get_rel_name(relationOid),attributeName)));
		}

		caql_endscan(pcqCtx);

	}
	return lExplicitAttNames;
}

/**
 * This function determines if a table can be analyzed or not. 
 * Input:
 * 	relationOid
 * Output:
 * 	true or false
 */
static bool analyzePermitted(Oid relationOid)
{
	return (pg_class_ownercheck(relationOid, GetUserId()) 
			|| pg_database_ownercheck(MyDatabaseId, GetUserId()));
}

/**
 * If ANALYZE is requested with no relations specified, this method is called to build
 * the implicit list of relations from pg_class. Only those with relkind == RELKIND_RELATION
 * are considered.
 * If rootonly is true, we only analyze root partition table.
 * 
 * Input:
 * 	None
 * Output:
 * 	List of relids
 */
static List* analyzableRelations(bool rootonly)
{
	List	   		*lRelOids = NIL;
	cqContext		*pcqCtx;
	HeapTuple		tuple = NULL;

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE relkind = :1 ",
				CharGetDatum(RELKIND_RELATION)));

	while (HeapTupleIsValid(tuple = caql_getnext(pcqCtx)))
	{
		Oid candidateOid = HeapTupleGetOid(tuple);
		if (rootonly && !rel_is_partitioned(candidateOid))
		{
			continue;
		}
		if (analyzePermitted(candidateOid)
				&& candidateOid != StatisticRelationId)
		{
			lRelOids = lappend_oid(lRelOids, candidateOid);
		}
	}

	caql_endscan(pcqCtx);

	return lRelOids;
}

/**
 * Given a relation's Oid, generate the list of attribute names that may be analyzed.
 * This ignores columns that have been dropped or if stattarget is set to 0 by user.
 * Input:
 * 	relation
 * Output:
 * 	List of attribute names. This will need to be free'd by the caller.
 */
static List *analyzableAttributes(Relation candidateRelation)
{
	int 	i = 0;
	List	*lAttNames = NIL;
	Assert(candidateRelation != NULL);
	for (i = 0; i < candidateRelation->rd_att->natts; i++)
	{
		Form_pg_attribute attr = candidateRelation->rd_att->attrs[i];
		Assert(attr);
		/* 
		 * Skip if one of these conditions is true:
		 * 1. Attribute is dropped
		 * 2. Stats target for attribute is 0
		 * 3. Attribute has "unknown" type.
		 */
		if (!(attr->attisdropped
				|| attr->attstattarget == 0
				|| attr->atttypid == UNKNOWNOID))
		{
			char	*attName = NULL;
			attName = pstrdup(NameStr(attr->attname)); //needs to be pfree'd by caller
			Assert(attName);
			lAttNames = lappend(lAttNames, (void *) attName);
		}
	}		
	return lAttNames;
}

/**
 * This method is called once all the transactions and snapshots have been set up.
 * At a very high-level, it performs two functions:
 * 	1. Compute and update reltuples, relages for the relation.
 * 	2. Compute and update statistics on requested attributes.
 * If the input relation is too large, it may create a sampled version of the relation
 * and compute statistics.
 * TODO: Check with Daria/Eric about messages.
 * Input:
 * 	relation		- relation
 * 	lAttributeNames - list of attribute names. 
 * Output:
 * 	None
 */
static void analyzeRelation(Relation relation, List *lAttributeNames, bool rootonly)
{
	Oid			sampleTableOid = InvalidOid;
	float4		minSampleTableSize = 0;
	bool		sampleTableRequired = true;
	ListCell	*le = NULL;
	Oid			relationOid = InvalidOid;
	float4 estimatedRelTuples = 0.0;
	float4 estimatedRelPages = 0.0;
	List	*indexOidList = NIL;
	ListCell	*lc = NULL;

	relationOid = RelationGetRelid(relation);

	/**
	 * Step 1: estimate reltuples, relpages for the relation. 
	 */
	if ('x' == relation->rd_rel->relstorage)
	{
		/* for external tables, we keep the default numbers (reltuples=1e+06, relpages=1000) in heap.c when they are created */
		elog(elevel, "ANALYZE skipping external table %s.", RelationGetRelationName(relation));

		return;
	}

	analyzeEstimateReltuplesRelpages(relationOid, &estimatedRelTuples, &estimatedRelPages, rootonly);
	
	/* limit to heap table only, since append-only tables are NOT using sampling, since VACUUM FULL won't help. */
	if ('h' == relation->rd_rel->relstorage && estimatedRelTuples == 0 && estimatedRelPages > 0)
	{
		/*
		 * NOTICE user when all sampled pages are empty
		 */
		ereport(NOTICE,
			(errmsg("ANALYZE detected all empty sample pages for table %s, please run VACUUM FULL for accurate estimation.", RelationGetRelationName(relation))));
	}

	elog(elevel, "ANALYZE estimated reltuples=%f, relpages=%f for table %s", estimatedRelTuples, estimatedRelPages, RelationGetRelationName(relation));
	
	/**
	 * Step 2: update the pg_class entry.
	 */
	updateReltuplesRelpagesInCatalog(relationOid, estimatedRelTuples, estimatedRelPages);
	
	/* Find relpages of each index and update these as well */
	indexOidList = RelationGetIndexList(relation);
	foreach (lc, indexOidList)
	{
		float4 estimatedIndexTuples = estimatedRelTuples;
		float4 estimatedIndexPages = 0;

		Oid	indexOid = lfirst_oid(lc);
		Assert(indexOid != InvalidOid);
		
		if (estimatedRelTuples < 1.0)
		{
			/**
			 * If there are no rows in the relation, no point trying to estimate
			 * number of pages in the index.
			 */
			elog(elevel, "ANALYZE skipping index %s since relation %s has no rows.", get_rel_name(indexOid), get_rel_name(relationOid));
		}
		else 
		{
			/**
			 * NOTE: we don't attempt to estimate the number of tuples in an index.
			 * We will assume it to be equal to the estimated number of tuples in the relation.
			 * This does not hold for partial indexes. The number of tuples matching will be
			 * derived in selfuncs.c using the base table statistics.
			 */
			analyzeEstimateIndexpages(relationOid, indexOid, &estimatedIndexPages);
			elog(elevel, "ANALYZE estimated reltuples=%f, relpages=%f for index %s", estimatedIndexTuples, estimatedIndexPages, get_rel_name(indexOid));
		}
		
		updateReltuplesRelpagesInCatalog(indexOid, estimatedIndexTuples, estimatedIndexPages);
	}
	
	/* report results to the stats collector, too */
	pgstat_report_analyze(relation, estimatedRelTuples, 0 /*totaldeadrows*/);
	
	/**
	 * Does the relation have any rows. If not, no point analyzing columns.
	 */
	if (estimatedRelTuples < 1.0)
	{
		elog(elevel, "ANALYZE skipping computing statistics on table %s because it has no rows.", RelationGetRelationName(relation));
		return;
	}

	/* Cannot compute statistics on pg_statistic due to locking issues */
	if (relationOid == StatisticRelationId)
	{
		return;
	}

	/**
	 * Determine how many rows need to be sampled.
	 */
	foreach (le, lAttributeNames)
	{
		const char *attributeName = (const char *) lfirst(le);
		float4 minRowsForAttribute = estimateSampleSize(relation, attributeName, estimatedRelTuples);
		minSampleTableSize = Max(minSampleTableSize, minRowsForAttribute);
	}
	
	/**
	 * If no statistics are needed on any attribute, then fall through quickly.
	 */
	if (minSampleTableSize == 0)
	{
		elog(elevel, "ANALYZE skipping computing statistics on table %s because no attribute needs it.", 
				RelationGetRelationName(relation));
		return;		
	}

	/**
	 * Determine if a sample table needs to be created. If reltuples is very small,
	 * then, we'd rather work off the entire table. Also, if the sample required is
	 * the size of the table, then we'd rather work off the entire table.
	 */
	if (estimatedRelTuples <= gp_statistics_sampling_threshold 
			|| minSampleTableSize >= estimatedRelTuples) /* maybe this should be K% of reltuples or something? */
	{
		sampleTableRequired = false;
	}

	/**
	 * Step 3: If required, create a sample table
	 */
	if (sampleTableRequired)
	{
		sampleTableOid = buildSampleTable(relationOid, lAttributeNames, 
				estimatedRelTuples, minSampleTableSize);
	}
	
	/**
	 * Step 4: ANALYZE attributes, one at a time.
	 */
	MemoryContext col_context = AllocSetContextCreate(CurrentMemoryContext,
													 "Analyze Column",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContext relationContext = MemoryContextSwitchTo(col_context);

	foreach (le, lAttributeNames)
	{
		AttributeStatistics	stats;
		const char *lAttributeName = (const char *) lfirst(le);
		elog(elevel, "ANALYZE computing statistics on attribute %s", lAttributeName);
		if (!sampleTableRequired)
		{
			sampleTableOid = relationOid;
		}
		analyzeComputeAttributeStatistics(relation, lAttributeName, estimatedRelTuples, sampleTableOid, &stats);
		updateAttributeStatisticsInCatalog(relationOid, lAttributeName, &stats);
		MemoryContextResetAndDeleteChildren(col_context);
	}

	MemoryContextSwitchTo(relationContext);
	MemoryContextDelete(col_context);

	/**
	 * Step 5: Cleanup. Drop the sample table.
	 */
	if (sampleTableRequired)
	{
		dropSampleTable(sampleTableOid);
	}
	
	return;
}

/**
 * Generates a table name for the auxiliary sample table that may be created during ANALYZE.
 * This is not super random. However, this should be sufficient for our purpose.
 * Input:
 * 	relationOid 	- relation
 * 	backendId	- pid of the backend.
 * Output:
 * 	sample table name. This must be pfree'd by the caller.
 */
static char* temporarySampleTableName(Oid relationOid)
{
	char tmpname[NAMEDATALEN];
	snprintf(tmpname, NAMEDATALEN, "pg_analyze_%u_%i", relationOid, MyBackendId);
	return pstrdup(tmpname);
}

/**
 * Determine the number of tuples from relation that will need to be sampled
 * to compute statistics on the specific attribute. This method looks up the
 * stats target for the said attribute (if one is not found, the default_statistics_target
 * is used) and employs a formula (explained below) to compute the number of rows
 * needed.
 * Input:
 * 	relationOid 		- relation whose sample size must be determined
 * 	attributeName 	- attribute
 * 	relTuples			- number of tuples in relation.
 * Output:
 * 	sample size
 * 
 */
static float4 estimateSampleSize(Relation rel, const char *attributeName, float4 relTuples)
{
	float4		sampleSize = 0.0;
	int4		statsTarget = 0.0;
	
	AttrNumber attnum = get_attnum(rel->rd_id, attributeName);
	Form_pg_attribute attr = rel->rd_att->attrs[attnum - 1];

	statsTarget = attr->attstattarget < 0 ? default_statistics_target : attr->attstattarget;
	Assert(statsTarget >= 0);
	/*--------------------
	 * The following choice of minrows is based on the paper
	 * "Random sampling for histogram construction: how much is enough?"
	 * by Surajit Chaudhuri, Rajeev Motwani and Vivek Narasayya, in
	 * Proceedings of ACM SIGMOD International Conference on Management
	 * of Data, 1998, Pages 436-447.  Their Corollary 1 to Theorem 5
	 * says that for table size n, histogram size k, maximum relative
	 * error fraction, and error probability gamma, the minimum
	 * random sample size is
	 *		r = 4 * k * ln(2*n/gamma) / error_fraction^2
	 * We use gamma=0.01 in our calculations.
	 *--------------------
	 */
	Assert(relTuples > 0.0);
	if (analyze_relative_error > 0.0)
	{
		sampleSize = (float4) 4 * statsTarget * log(2 * relTuples / 0.01) / (analyze_relative_error * analyze_relative_error);
	}
	else
	{
		sampleSize = relTuples;
	}
	
	sampleSize = Max(sampleSize, 0.0);	/* Sanity check for low relTuples */
	sampleSize = Min(sampleSize, relTuples);	/* Bound sample size to table size */
	
	return sampleSize;
}

/**
 * This is a helper method that executes a SQL statement using the SPI interface.
 * It optionally calls a callback function with result pointer.
 * Input:
 * 	src - SQL string
 * 	read_only - is it a read-only call?
 * 	tcount - execution tuple-count limit, or 0 for none
 * 	callbackFn - callback function to be executed once SPI is done.
 * 	clientData - argument to call back function (usually pointer to data-structure 
 * 				that the callback function populates).
 * 
 */
static void spiExecuteWithCallback(
		const char *src,
		bool read_only,
		long tcount,
		spiCallback callbackFn,
		void *clientData)
{
	bool connected = false;
	int ret = 0;

	PG_TRY();
	{
		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
					errmsg("Unable to connect to execute internal query.")));
		}
		connected = true;
		
		/* Do the query. */
		ret = SPI_execute(src, read_only, tcount);
		Assert(ret > 0);

		if (callbackFn)
		{
			callbackFn(clientData);
		}
		connected = false;
		SPI_finish();
	}
	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/**
 * A callback function for use with spiExecuteWithCallback.  Asserts that exactly one row was returned.
 *  Gets the row's first column as an array of float4 values and returns the two values from the array,
 *   after asserting that the array has exactly two elements
 *
 */
static void spiCallback_getSingleResultRowArrayAsTwoFloat4(void *clientData)
{
    float4 *out;
    Datum arrayDatum;
    bool isNull;
    Datum *values = NULL;
    int valuesLength, i;
    const int requiredArraySize = 2;

    Assert(SPI_tuptable != NULL);
    Assert(SPI_processed == 1);

    arrayDatum = heap_getattr(SPI_tuptable->vals[0], 1, SPI_tuptable->tupdesc, &isNull);
    Assert(!isNull);

    deconstruct_array(DatumGetArrayTypeP(arrayDatum),
            FLOAT4OID,
            sizeof(float4),
            true,
            'i',
            &values, NULL, &valuesLength);
    Assert(valuesLength == requiredArraySize);

    out = (float4*) clientData;
    for ( i = 0; i < requiredArraySize; i++)
        *out++ 	= DatumGetFloat4(values[i]);
    pfree(values);
}

/**
 * A callback function for use with spiExecuteWithCallback.  Copies the SPI_processed value into
 *    *clientDataOut, treating it as a float4 pointer.
 */
static void spiCallback_getProcessedAsFloat4(void *clientData)
{
    float4 *out = (float4*) clientData;
    *out = (float4)SPI_processed;
}

static void spiCallback_getSampleColumn(void *clientData)
{
	ResultColumnSpec *spec = (ResultColumnSpec*) clientData;
	uint32 i = 0;
	bool isNull = false;

	/* Since we are retrieving one column at a time, the attribute number
	 * should always be 0.
	 */
	int attnum = 1;

    Assert(SPI_tuptable != NULL);
    Assert(SPI_tuptable->tupdesc);

    Form_pg_attribute attr = SPI_tuptable->tupdesc->attrs[attnum-1];
    bool is_varlena = (!attr->attbyval) && attr->attlen == -1;
    bool is_varwidth = (!attr->attbyval) && attr->attlen < 0;

    MemoryContext callerContext = MemoryContextSwitchTo(spec->resultColumnMemContext);

    ScalarItem *values = (ScalarItem *) palloc(sizeof(ScalarItem) * SPI_processed);
    uint32 *tupnoLink = (uint32*) palloc(sizeof(uint32) * SPI_processed);

    for (i = 0; i < SPI_processed; i++)
    {
    	Datum dValue = heap_getattr(SPI_tuptable->vals[i], attnum, SPI_tuptable->tupdesc, &isNull);

    	if (isNull)
    	{
    		spec->null_cnt++;
    		continue;
    	}
    	spec->nonnull_cnt++;

		if (is_varlena)
		{
			spec->total_width += VARSIZE_ANY(DatumGetPointer(dValue));

			/* If the value is too width, we do not detoast it nor add it to the datum array */
			if (toast_raw_datum_size(dValue) > COLUMN_WIDTH_THRESHOLD)
			{
				continue;
			}
			dValue = PointerGetDatum(PG_DETOAST_DATUM(dValue));
		}
		else if (is_varwidth)
		{
			/* must be cstring */
			spec->total_width += strlen(DatumGetCString(dValue)) + 1;
		}

		/* need to copy datum from SPI context to current context */
		Datum dValue_copy = datumCopy(dValue, attr->attbyval, attr->attlen);
		values[spec->values_cnt].value = dValue_copy;
		values[spec->values_cnt].tupno = spec->values_cnt;
		tupnoLink[spec->values_cnt] = spec->values_cnt;
		spec->values_cnt++;
    }

    spec->values = values;
    spec->tupnoLink = tupnoLink;
    MemoryContextSwitchTo(callerContext);
}

/**
 * This method builds a sampled version of the given relation. The sampled
 * version is created in a temp namespace. Note that ANALYZE can be
 * executed only by the database owner. It is safe to assume that the database
 * owner has permissions to create temp tables. The sampling is done by
 * using the random() built-in function. 
 * 
 * TODO: Once tablesample becomes available, rewrite this function to utilize tablesample.
 * 
 * Input:
 * 	relationOid 	- relation to be sampled
 * 	lAttributeNames - attributes to be included in the sample
 * 	relTuples		- estimated size of relation
 * 	requestedSampleSize - as determined by attribute statistics requirements.
 * 	sampleLimit		- limit on size of the sample.
 * Output:
 * 	sampleTableRelTuples - number of tuples in the sample table created.
 */
static Oid buildSampleTable(Oid relationOid, 
		List *lAttributeNames, 
		float4	relTuples,
		float4 	requestedSampleSize)
{
	int nAttributes = -1;
	StringInfoData str;
	StringInfoData columnStr;
	int i = 0;
	ListCell *le = NULL;
	const char *schemaName = NULL;
	const char *tableName = NULL;
	char	*sampleSchemaName = pstrdup("pg_temp"); 
	char 	*sampleTableName = NULL;
	Oid			sampleTableOid = InvalidOid;
	float4		randomThreshold = 0.0;
	float4		sampleTableRelTuples = 0.0;
	RangeVar 	*rangeVar = NULL;
	
	Assert(requestedSampleSize > 0.0);
	Assert(relTuples > 0.0);
	
	randomThreshold = requestedSampleSize / relTuples;
	
	schemaName = get_namespace_name(get_rel_namespace(relationOid)); //must be pfreed
	tableName = get_rel_name(relationOid); //must be pfreed
	sampleTableName = temporarySampleTableName(relationOid); // must be pfreed 

	initStringInfo(&str);
	initStringInfo(&columnStr);
	appendStringInfo(&str, "create table %s.%s as ( ",
			quote_identifier(sampleSchemaName), quote_identifier(sampleTableName)); 
	
	nAttributes = list_length(lAttributeNames);

	foreach_with_count(le, lAttributeNames, i)
	{
		appendStringInfo(&columnStr, "Ta.%s", quote_identifier((const char *) lfirst(le)));
		if (i < nAttributes - 1)
		{
			appendStringInfo(&columnStr, ", ");
		}
		else
		{
			appendStringInfo(&columnStr, " ");
		}
	}
	
	/*
	 * If table is partitioned, we create a sample over all parts.
	 * The external partitions are skipped.
	 */

	if (rel_has_external_partition(relationOid))
	{
		PartitionNode *pn = get_parts(relationOid, 0 /*level*/ ,
								0 /*parent*/, false /* inctemplate */, CurrentMemoryContext, false /*includesubparts*/);

		ListCell *lc = NULL;
		bool isFirst = true;
		foreach(lc, pn->rules)
		{
			PartitionRule *rule = lfirst(lc);
			Relation rel = heap_open(rule->parchildrelid, NoLock);

			if (RelationIsExternal(rel))
			{
				heap_close(rel, NoLock);
				continue;
			}

			if (isFirst)
			{
				isFirst = false;
			}
			else
			{
				appendStringInfo(&str, " UNION ALL ");
			}

			appendStringInfo(&str, " select %s from %s.%s as Ta ",
					columnStr.data,
					quote_identifier(schemaName),
					quote_identifier(RelationGetRelationName(rel)));

			heap_close(rel, NoLock);
		}

		appendStringInfo(&str, " where random() < %.38f limit %lu ",
				randomThreshold, (unsigned long) requestedSampleSize);
	}
	else
	{
		appendStringInfo(&str, " select %s from %s.%s as Ta where random() < %.38f limit %lu ",
				columnStr.data,
				quote_identifier(schemaName),
				quote_identifier(tableName), randomThreshold, (unsigned long) requestedSampleSize);
	}

	appendStringInfo(&str, " ) distributed randomly");

	spiExecuteWithCallback(str.data, false /*readonly*/, 0 /*tcount */, 
	                        spiCallback_getProcessedAsFloat4, &sampleTableRelTuples);

	pfree(str.data);
	pfree(columnStr.data);
		
	elog(elevel, "Created sample table %s.%s with nrows=%.0f", 
			quote_identifier(sampleSchemaName), 
			quote_identifier(sampleTableName), 
			sampleTableRelTuples);

	rangeVar = makeRangeVar(sampleSchemaName, sampleTableName, -1);
	sampleTableOid = RangeVarGetRelid(rangeVar, true);

	Assert(sampleTableOid != InvalidOid);

	/**
	 * MPP-10723: Very rarely, we may be unlucky and generate an empty sample table. We error out in this case rather than
	 * generate bad statistics.
	 */
	
	if (sampleTableRelTuples < 1.0)
	{
		elog(ERROR, "ANALYZE unable to generate accurate statistics on table %s.%s. Try lowering gp_analyze_relative_error", 
				quote_identifier(schemaName), 
				quote_identifier(tableName));
	}
	
	/* 
	 * Update the sample table's reltuples, relpages. Without these, the queries to the sample table would call cdbRelsize which can be an expensive call. 
	 * We know the number of tuples in the sample table, but don't have the information about the number of pages. We set it to 2 arbitrarily.
	 */
	updateReltuplesRelpagesInCatalog(sampleTableOid, sampleTableRelTuples, 2);

	pfree((void *) rangeVar);
	pfree((void *) sampleTableName);
	pfree((void *) tableName);
	pfree((void *) schemaName);
	pfree((void *) sampleSchemaName);
	return sampleTableOid;
}

/**
 * Drops the sample table created during ANALYZE.
 */
static void dropSampleTable(Oid sampleTableOid)
{
	StringInfoData str;
	const char *sampleSchemaName = NULL;
	const char *sampleTableName = NULL;

	sampleSchemaName = get_namespace_name(get_rel_namespace(sampleTableOid)); //must be pfreed
	sampleTableName = get_rel_name(sampleTableOid); // must be pfreed 	

	initStringInfo(&str);
	appendStringInfo(&str, "drop table %s.%s", 
			quote_identifier(sampleSchemaName), 
			quote_identifier(sampleTableName));
	
	spiExecuteWithCallback(str.data, false /*readonly*/, 0 /*tcount */, 
	                        NULL, NULL);

	pfree(str.data);
	pfree((void *)sampleSchemaName);
	pfree((void *)sampleTableName);
}


/**
 * This method determines the number of pages corresponding to an index.
 * Input:
 * 	relationOid - relation being analyzed
 * 	indexOid - index whose size is to be determined
 * Output:
 * 	indexPages - number of pages in the index
 */
static void analyzeEstimateIndexpages(Oid relationOid, Oid indexOid, float4 *indexPages)
{
	StringInfoData 	sqlstmt;
	float4          spiResult[2];
	
	*indexPages = 0;			
		
	initStringInfo(&sqlstmt);
	
	if (GpPolicyFetch(CurrentMemoryContext, relationOid)->ptype == POLICYTYPE_ENTRY)
	{
		appendStringInfo(&sqlstmt, "select sum(gp_statistics_estimate_reltuples_relpages_oid(c.oid))::float4[] "
				"from pg_class c where c.oid=%d", indexOid);		
	}
	else
	{
		appendStringInfo(&sqlstmt, "select sum(gp_statistics_estimate_reltuples_relpages_oid(c.oid))::float4[] "
				"from gp_dist_random('pg_class') c where c.oid=%d", indexOid);
	}

    spiExecuteWithCallback(sqlstmt.data, true /*readonly*/, 0 /*tcount */,
        	                        spiCallback_getSingleResultRowArrayAsTwoFloat4, spiResult);
    *indexPages = spiResult[1];

	pfree(sqlstmt.data);
	return;
}

/**
 * This method estimates reltuples/relpages for a relation. To do this, it employs
 * the built-in function 'gp_statistics_estimate_reltuples_relpages'. If the table to be
 * analyzed is a system table, then it calculates statistics only using the master.
 * Input:
 * 	relationOid - relation's Oid
 * Output:
 * 	relTuples - estimated number of tuples
 * 	relPages  - estimated number of pages
 */
static void analyzeEstimateReltuplesRelpages(Oid relationOid, float4 *relTuples, float4 *relPages, bool rootonly)
{
	*relPages = 0.0;		
	*relTuples = 0.0;			
	
	List *allRelOids = NIL;

	/* rel_is_partitioned() tests whether a relation is a top-level (root) partition table.
	 * For mid-level parititions, we do not collect stats since neither planner or Orca uses them.
	 */
	if (rel_is_partitioned(relationOid))
	{
		allRelOids = rel_get_leaf_children_relids(relationOid);
	}
	else
	{
		allRelOids = list_make1_oid(relationOid);
	}

	/* iterate over all parts and add up estimates */
	ListCell *lc = NULL;
	foreach (lc, allRelOids)
	{
		Oid singleOid = lfirst_oid(lc);
		StringInfoData 	sqlstmt;
		float4          spiResult[2];

		initStringInfo(&sqlstmt);

		if (GpPolicyFetch(CurrentMemoryContext, singleOid)->ptype == POLICYTYPE_ENTRY)
		{
			appendStringInfo(&sqlstmt, "select sum(gp_statistics_estimate_reltuples_relpages_oid(c.oid))::float4[] "
					"from pg_class c where c.oid=%d", singleOid);
		}
		else
		{
			appendStringInfo(&sqlstmt, "select sum(gp_statistics_estimate_reltuples_relpages_oid(c.oid))::float4[] "
					"from gp_dist_random('pg_class') c where c.oid=%d", singleOid);
		}

		spiExecuteWithCallback(sqlstmt.data, true /*readonly*/, 0 /*tcount */,
				spiCallback_getSingleResultRowArrayAsTwoFloat4, spiResult);
		*relTuples += spiResult[0];
		*relPages += spiResult[1];

		pfree(sqlstmt.data);
	}

	return;
}

/**
 * This method updates reltuples, relpages in the pg_class entry
 * for the specified table.
 * Input:
 * 	relationOid - relation whose entry must be changed
 * 	reltuples 	- number of tuples as estimated during ANALYZE
 * 	relpages	- number of pages as estimated during ANALYZE
 */
static void updateReltuplesRelpagesInCatalog(Oid relationOid, float4 relTuples, float4 relPages)
{
	Relation	pgclass = NULL;
	HeapTuple	tuple = NULL;
	Form_pg_class pgcform = NULL;
	bool		dirty = false;
	cqContext	cqc;
	cqContext  *pcqCtx;

	Assert(relationOid != InvalidOid);
	Assert(relTuples > -1.0);
	Assert(relPages > -1.0);

	/* 
	 * We need a way to distinguish these 2 cases:
	 * a) ANALYZEd table is empty
	 * b) Table has never been ANALYZEd
	 * To do this, in case (a), we set relPages = 1. For case (b), relPages = 0.
	 */
	if (relPages < 1.0)
	{
		Assert(relTuples < 1.0);
		relPages = 1.0;
	}

	/*
	 * update number of tuples and number of pages in pg_class
	 */
	pgclass = heap_open(RelationRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), pgclass);

	/* Fetch a copy of the tuple to scribble on */
	tuple = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(relationOid)));

	/* We have locked the relation. We should not have trouble finding pg_class tuple */
	Assert(HeapTupleIsValid(tuple));
	pgcform = (Form_pg_class) GETSTRUCT(tuple);

	/* Apply required updates, if any, to copied tuple */

	if (pgcform->relpages != (int32) relPages)
	{
		pgcform->relpages = (int32) relPages;
		dirty = true;
	}
	if (pgcform->reltuples != (float4) relTuples)
	{
		pgcform->reltuples = (float4) relTuples;
		dirty = true;
	}

	elog(DEBUG3, "ANALYZE oid=%u pages=%d tuples=%f",
		 relationOid, pgcform->relpages, pgcform->reltuples);
	
	/*
	 * If anything changed, write out the tuple.  
	 */
	if (dirty)
	{
		heap_inplace_update(pgclass, tuple);
		/* the above sends a cache inval message */
	}

	heap_close(pgclass, RowExclusiveLock);
}

/**
 * Does this attribute have a total ordering defined i.e. does it have "<", "="
 * and is the attribute hashable?
 * Input:
 * 	typid - oid of column type
 * Output:
 * 	True if operators are defined and the attribute type is hashable, false otherwise.
 * 	Also returns the oid of the "<" operator if available or null otherwise
 */
static bool isOrderedAndHashable(Oid typid, Oid *ltopr)
{
	Operator	equalityOperator = NULL;
	Operator	ltOperator = NULL;
	bool		result = true;

	/* Does type have "=" operator */
	equalityOperator = equality_oper(typid, true);
	if (!equalityOperator)
	{
		result = false;
	}
	else
	{
		ReleaseOperator(equalityOperator);
	}
	/* Does type have "<" operator */
	ltOperator = ordering_oper(typid, true);
	if (!ltOperator)
	{
		*ltopr = 0;
		result = false;
	}
	else
	{
		*ltopr = oprid(ltOperator);
		ReleaseOperator(ltOperator);
	}
	
	/* Is the attribute hashable?*/
	if (!isGreenplumDbHashable(typid))
		result = false;
	
	return result;
}

/**
 * Does attribute have "max" aggregate function defined? We can compute histogram
 * only if this function is defined.
 * Input:
 * 	typid - oid of column type
 * Output:
 * 	true if "max" function is defined, false otherwise. 
 */
static bool hasMaxDefined(Oid typid)
{
	Oid			maxAggregateFunction = InvalidOid;
	bool		result = true;
	List		*funcNames = NIL;
	
	/* Does type have "max" operator */
	funcNames = list_make1(makeString("max"));
	maxAggregateFunction = LookupFuncName(funcNames, 1 /* nargs to function */, 
										&typid, true);
	if (!OidIsValid(maxAggregateFunction))
		result = false;

	return result;
}

static void sort_sample_column(ResultColumnSpec *spec, Oid typid, Oid ltopr)
{
	Oid	cmpFn;
	int	cmpFlags;
	FmgrInfo f_cmpfn;

	ScalarItem *values = spec->values;

	if (typid == TEXTOID || typid == BPCHAROID || typid == VARCHAROID)
	{
		/* For string columns, the cost of sorting an array of datums is surprisingly high
		 * comparing to the executor. The executor uses mk_sort which can see 3x speed up
		 * on string columns versus regular sort. Thus for string columns we piggy-back
		 * the sort to the previous SPI call that retrieves the sample columns.
		 * Since the values are already sorted by SPI call above, we only need to do a linear
		 * datum comparison and update spec->tupnoLink[] to facilitate ndistinct, mcv and histogram
		 * collection later.
		 */

		/* use equality comparison here since it can be much cheaper than "<".
		 * see texteq() vs text_lt().
		 */
		cmpFn = equality_oper_funcid(typid);
		fmgr_info(cmpFn, &f_cmpfn);
		long k = 0;
		long j = 0;
		uint32 curr_tupno = 0;
		for (k = spec->values_cnt - 1, j = k+1; k >= 0; k--, j--)
		{
			if (k == spec->values_cnt - 1 || !datumCompare(values[k].value, values[j].value, &f_cmpfn))
			{
				curr_tupno = values[k].tupno;
			}
			spec->tupnoLink[values[k].tupno] = curr_tupno;
		}
	}
	else
	{
		Assert(ltopr != InvalidOid); /* at this point, column type must have "<" operator defined */
		SelectSortFunction(ltopr, false /*nulls first*/, &cmpFn, &cmpFlags);
		fmgr_info(cmpFn, &f_cmpfn);
		CompareScalarsContext cxt;

		/* Sort the collected values */
		cxt.cmpFn = &f_cmpfn;
		cxt.cmpFlags = cmpFlags;
		cxt.tupnoLink = spec->tupnoLink;
		qsort_arg((void *) values, spec->values_cnt, sizeof(ScalarItem),
				  compare_scalars, (void *) &cxt);
	}
}

static void iterate_scalars(ResultColumnSpec *spec, IterateScalarsContext *itrCxt)
{
	/*
	 * Now scan the values in order, find the most common ones, and also
	 * gather info on number of distinct values.
	 *
	 * To determine which are most common, we first have to count the
	 * number of duplicates of each value.	The duplicates are adjacent in
	 * the sorted list, so a brute-force approach is to compare successive
	 * datum values until we find two that are not equal. However, that
	 * requires N-1 invocations of the datum comparison routine, which are
	 * completely redundant with work that was done during the sort.  (The
	 * sort algorithm must at some point have compared each pair of items
	 * that are adjacent in the sorted order; otherwise it could not know
	 * that it's ordered the pair correctly.) We exploit this by having
	 * compare_scalars remember the highest tupno index that each
	 * ScalarItem has been found equal to.	At the end of the sort, a
	 * ScalarItem's tupnoLink will still point to itself if and only if it
	 * is the last item of its group of duplicates (since the group will
	 * be ordered by tupno).
	 */

	uint32 dups_cnt = 0;
	uint32 i = 0;
	ScalarMCVItem *track = itrCxt->track;

	for (i = 0; i < spec->values_cnt; i++)
	{
		uint32	tupno = spec->values[i].tupno;

		dups_cnt++;
		if (spec->tupnoLink[tupno] == tupno)
		{
			/* Reached end of duplicates of this value */
			itrCxt->ndistinct++;
			if (dups_cnt > 1)
			{
				itrCxt->nmultiple++;
			}

			/*
			 * There are some difference between the implementation here and in PostgreSQL.
			 * We populate and update track and track_cnt even if dups_cnt is 1. This gives
			 * us more info in the MCV list even if the values are unique.
			 */
			if (itrCxt->track_cnt < itrCxt->num_mcv ||
				dups_cnt > track[itrCxt->track_cnt - 1].count)
			{
				/*
				 * Found a new item for the mcv list; find its
				 * position, bubbling down old items if needed. Loop
				 * invariant is that j points at an empty/replaceable
				 * slot.
				 */
				int	j = 0;

				if (itrCxt->track_cnt < itrCxt->num_mcv)
					itrCxt->track_cnt++;
				for (j = itrCxt->track_cnt - 1; j > 0; j--)
				{
					if (dups_cnt <= track[j - 1].count)
						break;
					track[j].count = track[j - 1].count;
					track[j].first = track[j - 1].first;
				}
				track[j].count = dups_cnt;
				track[j].first = i + 1 - dups_cnt;
			}
			dups_cnt = 0;
		}
	}
}

static void compute_ndistinct(AttributeStatistics *stats,
							IterateScalarsContext *itrCxt,
							uint32 toowide_cnt,
							uint32 sampleTableRelTuples,
							float4 relTuples,
							bool *computeMCV)
{
	if (itrCxt->nmultiple == 0)
	{
		/* If we found no repeated values, assume it's a unique column */
		stats->ndistinct = -1.0;

		/*
		 * If there are many more distinct values than mcv buckets,
		 * we can ignore computing mcv values.
		 */
		if (itrCxt->ndistinct > itrCxt->num_mcv)
			*computeMCV = false;
	}
	else if (toowide_cnt == 0 && itrCxt->nmultiple == itrCxt->ndistinct)
	{
		/*
		 * Every value in the sample appeared more than once.  Assume the
		 * column has just these values.
		 */
		stats->ndistinct = itrCxt->ndistinct;
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
		uint32		f1 = itrCxt->ndistinct - itrCxt->nmultiple + toowide_cnt;
		uint32		d = f1 + itrCxt->nmultiple;
		double		numer, denom;

		numer = (double) sampleTableRelTuples * (double) d;

		denom = (double) (sampleTableRelTuples - f1) +
			(double) f1 * (double) sampleTableRelTuples / relTuples;

		stats->ndistinct = numer / denom;
		/* Clamp to sane range in case of roundoff error */
		if (stats->ndistinct < (double) d)
			stats->ndistinct = (double) d;
		if (stats->ndistinct > relTuples)
			stats->ndistinct = relTuples;
		stats->ndistinct = floor(stats->ndistinct + 0.5);
	}

	/**
	 * Does ndistinct scale with reltuples?
	 */
	if (stats->ndistinct > relTuples * gp_statistics_ndistinct_scaling_ratio_threshold)
	{
		stats->ndistinct = -(stats->ndistinct / relTuples);
	}
}

static void compute_mcv(AttributeStatistics *stats,
						ResultColumnSpec *spec,
						IterateScalarsContext *itrCxt,
						uint32 sampleTableRelTuples,
						Form_pg_attribute attr)
{
	/*
	 * Decide how many values are worth storing as most-common values. If
	 * we are able to generate a complete MCV list (all the values in the
	 * sample will fit, and we think these are all the ones in the table),
	 * then do so.
	 */
	int num_mcv = itrCxt->num_mcv;
	ScalarMCVItem *track = itrCxt->track;
	uint32 i = 0;

	if (itrCxt->track_cnt < num_mcv)
	{
		num_mcv = itrCxt->track_cnt;
	}

	if (num_mcv > 0)
	{
		Datum	   *mcv_values;
		Datum	   *mcv_freqs;

		mcv_values = (Datum *) palloc(num_mcv * sizeof(Datum));
		mcv_freqs = (Datum *) palloc(num_mcv * sizeof(Datum));
		for (i = 0; i < num_mcv; i++)
		{
			mcv_values[i] = spec->values[track[i].first].value;
			mcv_freqs[i] = Float4GetDatum((float4) track[i].count / (float4) sampleTableRelTuples);
		}

		stats->mcv = construct_array(mcv_values, num_mcv, attr->atttypid, attr->attlen, attr->attbyval, attr->attalign);
		stats->freq = construct_array(mcv_freqs, num_mcv, FLOAT4OID, sizeof(float4), true, 'i');
	}
}

static void compute_histogram(AttributeStatistics *stats,
						ResultColumnSpec *spec,
						IterateScalarsContext *itrCxt,
						Form_pg_attribute attr)
{
	int num_hist = itrCxt->num_hist;

	Assert(num_hist > 0);
	uint32 bucket_size = spec->values_cnt / num_hist;
	if (bucket_size <= 1)
	{
		/* histogram will be empty if bucket_size <= 1 */
		stats->hist = NULL;
		return;
	}

	/**
	 * In legacy GPDB the histogram is constructed by choosing values that occur every
	 * 'bucketSize' interval. It also chooses the maximum value from the relation. It then
	 * removes duplicate values and orders the values in ascending order.
	 */
	int num_hist_upperbound = num_hist +
								1 /* one extra for boundary */ +
								(spec->values_cnt % num_hist) / bucket_size /* remainder */ +
								1 /* max value */;

	Datum *hist_values = (Datum *) palloc(num_hist_upperbound * sizeof(Datum));

	int hist_cnt = 0; /* the final and unique number of histogram boundaries */

	/*
	 * The new_value_flag is used to detect duplicates in the histogram boundaries.
	 * Since we know the values are sorted and spec->tupnoLink[tupno] == tupno is
	 * only true for the last value of each block of repeated values, we can reset
	 * the flag whenever we pass through a block of repeated values. This way we do not
	 * need to do datum comparisons, which are much more expensive.
	 */
	bool new_value_flag = true;
	uint32 i = 0;
	while (i < spec->values_cnt)
	{
		int tupno = spec->values[i].tupno;
		if ((i % bucket_size == 0 || i == spec->values_cnt - 1) &&
				new_value_flag)
		{
			hist_values[hist_cnt] = spec->values[i].value;
			hist_cnt++;
			new_value_flag = false;
		}
		if (spec->tupnoLink[tupno] == tupno)
		{
			new_value_flag = true;
		}
		i++;
	}

	stats->hist = construct_array(hist_values, hist_cnt, attr->atttypid, attr->attlen, attr->attbyval, attr->attalign);
}

/**
 * Responsible for computing statistics on relationOid. 
 * Input:
 * 	relationOid - original relation on which statistics must be computed.
 * 	attributeName - name of attribute
 * 	relTuples   - expected number of tuples in relation
 * 	sampleTableOid - Oid of the sampled version of the table. It is possible that sampleTableOid == relationOid.
 * 	sampleTableRelTuples - number of tuples in sampled version
 * Output:
 * 	stats - structure containing nullfrac, avgwidth, ndistinct, mcv, hist
 */
static void analyzeComputeAttributeStatistics(Relation rel,
		const char *attributeName,
		float4 relTuples, 
		Oid sampleTableOid, 
		AttributeStatistics *stats)
{
	bool computeDistinct = true;
	bool computeMCV = true;
	bool computeHist = true;
	AttrNumber attnum = get_attnum(rel->rd_id, attributeName);
	
	Assert(stats != NULL);
	
	/* Default values */
	stats->nullFraction = 0.0;
	stats->avgWidth = 0.0;
	stats->ndistinct = -1.0;
	stats->mcv = NULL;
	stats->freq = NULL;
	stats->hist = NULL;

	Form_pg_attribute attr = rel->rd_att->attrs[attnum - 1];

	StringInfo str = makeStringInfo();
	const char *sampleSchemaName = get_namespace_name(get_rel_namespace(sampleTableOid));
	const char *sampleTableName = get_rel_name(sampleTableOid);

	appendStringInfo(str, "select tbl.%s from %s.%s as tbl ",
			quote_identifier(attributeName), quote_identifier(sampleSchemaName), quote_identifier(sampleTableName));

	if (attr->atttypid == TEXTOID || attr->atttypid == BPCHAROID || attr->atttypid == VARCHAROID)
	{
		/* For string columns, let executor do the sorting since it is much faster (using mk_sort).
		 * See comments in sort_sample_column().
		 */
		appendStringInfo(str, "order by tbl.%s", quote_identifier(attributeName));
	}
	ResultColumnSpec spec;
	spec.null_cnt = 0;
	spec.nonnull_cnt = 0;
	spec.values_cnt = 0;
	spec.total_width = 0;
	spec.resultColumnMemContext = CurrentMemoryContext;

	/* read the sample column into an array of datums */
	spiExecuteWithCallback(str->data, true /*readonly*/, 0 /*tcount*/,
			spiCallback_getSampleColumn, &spec);

	uint32 sampleTableRelTuples = spec.nonnull_cnt + spec.null_cnt;
	uint32 toowide_cnt = spec.nonnull_cnt - spec.values_cnt;

	if (sampleTableRelTuples == 0)
	{
		/* nothing to analyze here */
		return;
	}

	/* compute null fraction */
	if (attr->attnotnull)
	{
		stats->nullFraction = 0.0;
	}
	else
	{
		stats->nullFraction = Min(spec.null_cnt / sampleTableRelTuples, 1.0);
	}
	
	/* compute column width */
	bool is_varwidth = !attr->attbyval && attr->attlen < 0;
	if (spec.null_cnt < ceil(sampleTableRelTuples))
	{
		Assert(spec.nonnull_cnt > 0); /* we should not be here if all values are null */
		stats->avgWidth = is_varwidth ? spec.total_width / (double) spec.nonnull_cnt : attr->attlen;
	}
	
	/* there are a couple of cases we do not collect advanced stats */
	Oid ltopr = InvalidOid;
	if (spec.values_cnt == 0 || stats->avgWidth > COLUMN_WIDTH_THRESHOLD || !isOrderedAndHashable(attr->atttypid, &ltopr))
	{
		/* We do not collect advanced stats if the column is all null values or
		 * too wide or not ordered-and-hashable */
		computeDistinct = false;
		computeMCV = false;
		computeHist = false;

		return;
	}

	if (attr->atttypid == BOOLOID)
	{
		/* For boolean type we only need to collect MCV */
		stats->ndistinct = 2;
		computeDistinct = false;
		computeHist = false;
	}
	else if (!hasMaxDefined(attr->atttypid))
	{
		/* There may be types that have comparison operator defined but no max agg function defined.
		 * We skip collecting histogram for such types.
		 */
		computeHist = false;
	}

	/* sort the array of datums, while keeping track of duplicate chunks */
	sort_sample_column(&spec, attr->atttypid, ltopr);

	/* these variables are states we keep when iterating the array */
	IterateScalarsContext itrCxt;
	itrCxt.ndistinct = 0;
	itrCxt.nmultiple = 0;
	itrCxt.track_cnt = 0;
	itrCxt.track = (ScalarMCVItem *) palloc0(itrCxt.num_mcv * sizeof(ScalarMCVItem));

	itrCxt.num_mcv = attr->attstattarget < 0 ? default_statistics_target : attr->attstattarget;
	itrCxt.num_hist = itrCxt.num_mcv;

	/* iterate the datum array to collect states that will be used for computing ndistinct,
	 * mcv and histogram below.
	 */
	iterate_scalars(&spec, &itrCxt);

	if (computeDistinct)
	{
		compute_ndistinct(stats, &itrCxt, toowide_cnt, sampleTableRelTuples, relTuples, &computeMCV);
	}

	if (computeMCV)
	{
		compute_mcv(stats, &spec, &itrCxt, sampleTableRelTuples, attr);
	}

	if (computeHist)	
	{
		compute_histogram(stats, &spec, &itrCxt, attr);
	}
}

/**
 * This method updates the pg_statistic tuple for the specific attribute. It populates
 * it with the statistics computed by ANALYZE.
 * Input:
 * 	relationOid - relation
 * 	attributeName - name of the attribute
 *  stats		- struct containing all the computed statistics
 *  
 */
static void updateAttributeStatisticsInCatalog(Oid relationOid, const char *attributeName, AttributeStatistics *stats)
{
	Datum		values[Natts_pg_statistic];
	bool		nulls[Natts_pg_statistic];
	bool		replaces[Natts_pg_statistic];
	int2		attNum = -1;	
	int 		i = 0;
	Oid			equalityOid = InvalidOid;
	Oid			ltOid = InvalidOid;
	
	Assert(stats);

	{
		/**
		 * For some reason, pg_statistic stores the "<" and "=" operator associated
		 * with the attribute. 
		 */
		HeapTuple	attributeTuple;
		Form_pg_attribute attribute;
		Operator	equalityOperator;
		Operator	ltOperator;
		cqContext  *pcqCtx;

		pcqCtx = caql_getattname_scan(NULL, relationOid, attributeName);
		
		attributeTuple = caql_get_current(pcqCtx);

		Assert(HeapTupleIsValid(attributeTuple));
		attribute = (Form_pg_attribute) GETSTRUCT(attributeTuple);
		attNum = attribute->attnum;
		equalityOperator = equality_oper(attribute->atttypid, true);
		
		if (equalityOperator)
		{
			equalityOid = oprid(equalityOperator);
			ReleaseOperator(equalityOperator);
		}
		
		ltOperator = ordering_oper(attribute->atttypid, true);
		
		if (ltOperator)
		{
			ltOid = oprid(ltOperator);
			ReleaseOperator(ltOperator);
		}
		caql_endscan(pcqCtx);
	}

	for (i = 0; i < Natts_pg_statistic; ++i)
	{
		nulls[i] = false;
		replaces[i] = true;
	}

	values[Anum_pg_statistic_starelid - 1] = ObjectIdGetDatum(relationOid);	/* starelid */
	values[Anum_pg_statistic_staattnum - 1] = Int16GetDatum(attNum);		/* staattnum */
	values[Anum_pg_statistic_stanullfrac - 1] = Float4GetDatum(stats->nullFraction);		/* stanullfrac */
	values[Anum_pg_statistic_stawidth - 1] = Int32GetDatum((int32) stats->avgWidth);	/* stawidth */
	values[Anum_pg_statistic_stadistinct - 1] = Float4GetDatum(stats->ndistinct);		/* stadistinct */


	/* We will always put MCV in slot 1 and histogram in slot 2 */
	
	if (stats->mcv)
	{
		values[Anum_pg_statistic_stakind1 - 1] = Int16GetDatum(STATISTIC_KIND_MCV);
	}
	else 
	{
		values[Anum_pg_statistic_stakind1 - 1] = Int16GetDatum((int2) 0); /* no mcv */
	}
	
	if (stats->hist)
	{
		values[Anum_pg_statistic_stakind2 - 1] = Int16GetDatum(STATISTIC_KIND_HISTOGRAM);
	}
	else
	{
		values[Anum_pg_statistic_stakind2 - 1] = Int16GetDatum((int2) 0);		
	}
	
	/* correlation */
	values[Anum_pg_statistic_stakind3 - 1] = Int16GetDatum((int2) 0); /* we do not compute correlation anymore */
	
	/* an extra slot */
	values[Anum_pg_statistic_stakind4 - 1] = Int16GetDatum((int2) 0);
	
	/* staops .. which correspond to operators. Sigh.. */
	if (stats->mcv)
	{
		values[Anum_pg_statistic_staop1 - 1] = ObjectIdGetDatum(equalityOid);
	}
	else
	{
		values[Anum_pg_statistic_staop1 - 1] = 0;
	}

	if (stats->hist)
	{
		values[Anum_pg_statistic_staop2 - 1] = ObjectIdGetDatum(ltOid);
	}
	else
	{
		values[Anum_pg_statistic_staop2 - 1] = 0;
	}
	
	/* dummy fields */
	values[Anum_pg_statistic_staop3 - 1] = 0;
	values[Anum_pg_statistic_staop4 - 1] = 0;
	
	/* Now working on stanumbers */
	if (stats->freq)
	{
		values[Anum_pg_statistic_stanumbers1 - 1] = PointerGetDatum(stats->freq);
	}
	else
	{
		nulls[Anum_pg_statistic_stanumbers1 - 1] = true;
		values[Anum_pg_statistic_stanumbers1 - 1] = 0;
	}

	/* dummy fields */
	nulls[Anum_pg_statistic_stanumbers2 - 1] = true;
	values[Anum_pg_statistic_stanumbers2 - 1] = 0;
	nulls[Anum_pg_statistic_stanumbers3 - 1] = true;
	values[Anum_pg_statistic_stanumbers3 - 1] = 0;
	nulls[Anum_pg_statistic_stanumbers4 - 1] = true;
	values[Anum_pg_statistic_stanumbers4 - 1] = 0;

	/* Now working on stavalues */
	if (stats->mcv)
	{
		values[Anum_pg_statistic_stavalues1 - 1] = PointerGetDatum(stats->mcv);
	}
	else
	{
		nulls[Anum_pg_statistic_stavalues1 - 1] = true;
		values[Anum_pg_statistic_stavalues1 - 1] = 0;
	}
	
	if (stats->hist)
	{
		values[Anum_pg_statistic_stavalues2 - 1] = PointerGetDatum(stats->hist);
	}
	else
	{
		nulls[Anum_pg_statistic_stavalues2 - 1] = true;
		values[Anum_pg_statistic_stavalues2 - 1] = 0;
	}
	
	/* dummy values */
	nulls[Anum_pg_statistic_stavalues3 - 1] = true;
	values[Anum_pg_statistic_stavalues3 - 1] = 0;
	nulls[Anum_pg_statistic_stavalues4 - 1] = true;
	values[Anum_pg_statistic_stavalues4 - 1] = 0;

	/* Now work on pg_statistic */
	{
		HeapTuple	oldStatisticsTuple;
		cqContext  *pcqCtx;

		pcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_statistic "
					" WHERE starelid = :1 "
					" AND staattnum = :2 "
					" FOR UPDATE ",
					ObjectIdGetDatum(relationOid),
					Int16GetDatum(attNum)));

		oldStatisticsTuple = caql_getnext(pcqCtx);

		if (HeapTupleIsValid(oldStatisticsTuple))
		{
			/* pg_statistic tuple exists. */
			HeapTuple stup = caql_modify_current(pcqCtx,
					values,
					nulls,
					replaces);

			caql_update_current(pcqCtx, stup);
			/* and Update indexes (implicit) */

			heap_freetuple(stup);
		}
		else
		{
			/* insert new tuple into pg_statistic. we are guaranteed
			 * no-one else will overwrite this row because of
			 * ShareUpdateExclusiveLock on the relation. 
			 */

			HeapTuple stup = caql_form_tuple(pcqCtx, values, nulls);

			caql_insert(pcqCtx, stup);
			/* and Update indexes (implicit) */

			heap_freetuple(stup);
		}
		caql_endscan(pcqCtx);
	}

}


/**
 * This method estimates the number of tuples and pages in a heaptable relation. Getting the number of blocks is straightforward.
 * Estimating the number of tuples is a little trickier. There are two factors that complicate this:
 * 	1. Tuples may be of variable length.
 * 	2. There may be dead tuples lying around.
 * To do this, it chooses a certain number of blocks (as determined by a guc) randomly. The process of choosing is not strictly
 * uniformly random since we have a target number of blocks in mind. We start processing blocks in order and choose an block 
 * with a probability p determined by the ratio of target to total blocks. It is possible that we get really unlucky and reject
 * a large number of blocks up front. We compensate for this by increasing p dynamically. Thus, we are guaranteed to choose the target number
 * of blocks. We read all heaptuples from these blocks and keep count of number of live tuples. We scale up this count to
 * estimate reltuples. Relpages is an exact value.
 * 
 * Input:
 * 	rel - Relation. Must be a heaptable. 
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */
static void gp_statistics_estimate_reltuples_relpages_heap(Relation rel, float4 *reltuples, float4 *relpages)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	float4		nrowsseen = 0;	/* # rows seen (including dead rows) */
	float4		nrowsdead = 0;	/* # rows dead */
	float4		totalEmptyPages = 0; /* # of empty pages with only dead rows */
	float4		totalSamplePages = 0; /* # of pages sampled */

	BlockNumber nblockstotal = 0;	/* nblocks in relation */
	BlockNumber nblockstarget = (BlockNumber) gp_statistics_blocks_target; 
	BlockNumber nblocksseen = 0;
	int			j = 0; /* counter */
	
	/**
	 * Ensure that the right kind of relation with the right kind of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsHeap(rel));
					
	nblockstotal = RelationGetNumberOfBlocks(rel);

	if (nblockstotal == 0 || nblockstarget == 0)
	{		
		/**
		 * If there are no blocks, there cannot be tuples.
		 */
		*reltuples = 0.0;
		*relpages = 0.0;
		return; 
	}
		
	for (j=0 ; j<nblockstotal; j++)
	{
		/**
		 * Threshold is dynamically adjusted based on how many blocks we need to examine and how many blocks
		 * are left.
		 */
		double threshold = ((double) nblockstarget - nblocksseen)/((double) nblockstotal - j);
		
		/**
		 * Random dice thrown to determine if current block is chosen.
		 */
		double diceValue = ((double) random()) / ((double) MAX_RANDOM_VALUE);
		
		if (threshold >= 1.0 || diceValue <= threshold)
		{
			totalSamplePages++;
			/**
			 * Block j shall be examined!
			 */
			BlockNumber targblock = j;
			Buffer		targbuffer;
			Page		targpage;
			OffsetNumber targoffset,
						maxoffset;

			/**
			 * Check for cancellations.
			 */
			CHECK_FOR_INTERRUPTS();

			/*
			 * We must maintain a pin on the target page's buffer to ensure that
			 * the maxoffset value stays good (else concurrent VACUUM might delete
			 * tuples out from under us).  Hence, pin the page until we are done
			 * looking at it.  We don't maintain a lock on the page, so tuples
			 * could get added to it, but we ignore such tuples.
			 */

			// -------- MirroredLock ----------
			MIRROREDLOCK_BUFMGR_LOCK;

			targbuffer = ReadBuffer(rel, targblock);
			LockBuffer(targbuffer, BUFFER_LOCK_SHARE);
			targpage = BufferGetPage(targbuffer);
			maxoffset = PageGetMaxOffsetNumber(targpage);

			/* Figure out overall nrowsdead/nrowsseen ratio */
			/* Figure out # of empty pages based on page level #rowsseen and #rowsdead.*/
			float4 pageRowsSeen = 0.0;
			float4 pageRowsDead = 0.0;

			/* Inner loop over all tuples on the selected block. */
			for (targoffset = FirstOffsetNumber; targoffset <= maxoffset; targoffset++)
			{
				ItemId itemid;
				itemid = PageGetItemId(targpage, targoffset);
				nrowsseen++;
				pageRowsSeen++;
				if(!ItemIdIsNormal(itemid))
				{
					nrowsdead += 1;
					pageRowsDead++;
				}
				else
				{
					HeapTupleData targtuple;
					ItemPointerSet(&targtuple.t_self, targblock, targoffset);
					targtuple.t_data = (HeapTupleHeader) PageGetItem(targpage, itemid);
					targtuple.t_len = ItemIdGetLength(itemid);

					if(!HeapTupleSatisfiesVisibility(rel, &targtuple, SnapshotNow, targbuffer))
					{
						nrowsdead += 1;
						pageRowsDead++;
					}
				}
			}

			/* Now release the pin on the page */
			UnlockReleaseBuffer(targbuffer);

			MIRROREDLOCK_BUFMGR_UNLOCK;
			// -------- MirroredLock ----------

			/* detect empty pages: pageRowsSeen == pageRowsDead, also log the nrowsseen (total) and nrowsdead (total) */
			if (pageRowsSeen == pageRowsDead && pageRowsSeen > 0)
			{
				totalEmptyPages++;
			}

			nblocksseen++;
		}		
	}

	Assert(nblocksseen > 0);
	/**
	 * To calculate reltuples, scale up the number of live rows per block seen to the total number
	 * of blocks. 
	 */
	*reltuples = ceil((nrowsseen - nrowsdead) * nblockstotal / nblocksseen);
	*relpages = nblockstotal;

	if (totalSamplePages * 0.5 <= totalEmptyPages && totalSamplePages != 0)
	{
		/*
		 * LOG empty pages of bloated table for each segments.
		 */
		elog(elevel, "ANALYZE detected 50%% or more empty pages (%f empty out of %f pages), please run VACUUM FULL for accurate estimation.", totalEmptyPages, totalSamplePages);
	}

	return;
}

/**
 * This method estimates the number of tuples and pages in an vertical oriented append-only relation. 
 * Require access to segment catalogs to determine reltuples.
 * Relpages is obtained by fudging AO block sizes.
 * 
 * Input:
 * 	rel - Relation. Must be an AO CS table.
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */

static void gp_statistics_estimate_reltuples_relpages_ao_cs(Relation rel, float4 *reltuples, float4 *relpages)
{
	AOCSFileSegInfo	**aocsInfo = NULL;
	int				nsegs = 0;
	double			totalBytes = 0;
	AppendOnlyEntry *aoEntry;
	int64 hidden_tupcount;
	AppendOnlyVisimap visimap;

	/**
	 * Ensure that the right kind of relation with the right type of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsAoCols(rel));
	
	*reltuples = 0.0;
	*relpages = 0.0;
	
    /* get table level statistics from the pg_aoseg table */
	aoEntry = GetAppendOnlyEntry(RelationGetRelid(rel), SnapshotNow);
	aocsInfo = GetAllAOCSFileSegInfo(rel, aoEntry, SnapshotNow, &nsegs);
	if (aocsInfo)
	{
		int i = 0;
		int j = 0;
		for(i = 0; i < nsegs; i++)
		{
			for(j = 0; j < RelationGetNumberOfAttributes(rel); j++)
			{
				AOCSVPInfoEntry *e = getAOCSVPEntry(aocsInfo[i], j);
				Assert(e);
				totalBytes += e->eof_uncompressed;
			}

			/* Do not include tuples from an awaiting drop segment file */
			if (aocsInfo[i]->state != AOSEG_STATE_AWAITING_DROP)
			{
				*reltuples += aocsInfo[i]->total_tupcount;
			}
		}
		/**
		 * The planner doesn't understand AO's blocks, so need this method to try to fudge up a number for
		 * the planner. 
		 */
		*relpages = RelationGuessNumberOfBlocks(totalBytes);
	}

	AppendOnlyVisimap_Init(&visimap, aoEntry->visimaprelid, aoEntry->visimapidxid, AccessShareLock, SnapshotNow);
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&visimap);
	AppendOnlyVisimap_Finish(&visimap, AccessShareLock);

	(*reltuples) -= hidden_tupcount;

	pfree(aoEntry);
	  
	return;
}
/**
 * This method estimates the number of visible tuples and pages in an append-only relation. AO tables maintain accurate
 * tuple counts in the catalog. Therefore, we will require access to segment catalogs to determine reltuples.
 * Relpages is obtained by fudging AO block sizes. In addition, we substract the number of invisible
 * tuples from the total number of tuples.
 * 
 * Input:
 * 	rel - Relation. Must be an AO table.
 * 
 * Output:
 * 	reltuples - estimated number of tuples in relation.
 * 	relpages  - exact number of pages.
 */

static void gp_statistics_estimate_reltuples_relpages_ao_rows(Relation rel, float4 *reltuples, float4 *relpages)
{
	FileSegTotals		*fstotal;
	AppendOnlyEntry *aoEntry;
	AppendOnlyVisimap visimap;
	int64 hidden_tupcount = 0;
	/**
	 * Ensure that the right kind of relation with the right type of storage is passed to us.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	Assert(RelationIsAoRows(rel));
	
	fstotal = GetSegFilesTotals(rel, SnapshotNow);
	Assert(fstotal);
	/**
	 * The planner doesn't understand AO's blocks, so need this method to try to fudge up a number for
	 * the planner. 
	 */
	*relpages = RelationGuessNumberOfBlocks((double)fstotal->totalbytes);

	aoEntry = GetAppendOnlyEntry(RelationGetRelid(rel), SnapshotNow);
	AppendOnlyVisimap_Init(&visimap, aoEntry->visimaprelid, aoEntry->visimapidxid, AccessShareLock, SnapshotNow);
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&visimap);
	AppendOnlyVisimap_Finish(&visimap, AccessShareLock);

	/**
	 * The number of tuples in AO table is known accurately. Therefore, we just utilize this value.
	 */
	*reltuples = (double)(fstotal->totaltuples - hidden_tupcount);

	pfree(fstotal);
	pfree(aoEntry);
	
	return;
}

/**
 * Given the oid of a relation, this method calculates reltuples, relpages. This only looks up
 * local information (on master or segments). It produces meaningful values for AO and
 * heap tables and returns [0.0,0.0] for all other relations.
 * Input: 
 * 	relationoid
 * Output:
 * 	array of two values [reltuples,relpages]
 */
Datum
gp_statistics_estimate_reltuples_relpages_oid(PG_FUNCTION_ARGS)
{
	
	float4		relpages = 0.0;		
	float4		reltuples = 0.0;			
	Oid			relOid = PG_GETARG_OID(0);
	Datum		values[2];
	ArrayType   *result;
	
	Relation rel = try_relation_open(relOid, AccessShareLock, false);

	if (rel != NULL)
	{
		if (rel->rd_rel->relkind == RELKIND_RELATION)
		{
			if (RelationIsHeap(rel))
			{
				gp_statistics_estimate_reltuples_relpages_heap(rel, &reltuples, &relpages);
			}
			else if (RelationIsAoRows(rel))
			{
				gp_statistics_estimate_reltuples_relpages_ao_rows(rel, &reltuples, &relpages);
			}
			else if	(RelationIsAoCols(rel))
			{
				gp_statistics_estimate_reltuples_relpages_ao_cs(rel, &reltuples, &relpages);
			}
		}
		else if (rel->rd_rel->relkind == RELKIND_INDEX)
		{
			reltuples = 1.0;
			relpages = RelationGetNumberOfBlocks(rel);
		}
		else
		{
			/**
			 * Should we silently return [0.0,0.0] or error out? Currently, we choose option 1.
			 */
		}
		relation_close(rel, AccessShareLock);
	}
	else
	{
		/**
		 * Should we silently return [0.0,0.0] or error out? Currently, we choose option 1.
		 */
	}
	
	values[0] = Float4GetDatum(reltuples);
	values[1] = Float4GetDatum(relpages);

	result = construct_array(values, 2,
					FLOAT4OID,
					sizeof(float4), true, 'i');

	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * qsort_arg comparator for sorting ScalarItems
 *
 * Aside from sorting the items, we update the tupnoLink[] array
 * whenever two ScalarItems are found to contain equal datums.	The array
 * is indexed by tupno; for each ScalarItem, it contains the highest
 * tupno that that item's datum has been found to be equal to.  This allows
 * us to avoid additional comparisons in compute_scalar_stats().
 */
static int
compare_scalars(const void *a, const void *b, void *arg)
{
	Datum		da = ((ScalarItem *) a)->value;
	int			ta = ((ScalarItem *) a)->tupno;
	Datum		db = ((ScalarItem *) b)->value;
	int			tb = ((ScalarItem *) b)->tupno;
	CompareScalarsContext *cxt = (CompareScalarsContext *) arg;
	int		compare;

	compare = ApplySortFunction(cxt->cmpFn, cxt->cmpFlags,
								da, false, db, false);
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
 * Comparison function for two datums
 */
inline static bool
datumCompare(Datum d1, Datum d2, FmgrInfo *finfo)
{
	return DatumGetBool(FunctionCall2(finfo, d1, d2));
}

