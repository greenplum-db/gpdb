/*-------------------------------------------------------------------------
 *
 * cluster.c
 *	  CLUSTER a table on an index.
 *
 * There is hardly anything left of Paul Brown's original implementation...
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/cluster.c,v 1.169 2008/01/30 19:46:48 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_type.h"
#include "catalog/toasting.h"
#include "catalog/aoseg.h"
#include "catalog/aoblkdir.h"
#include "catalog/aovisimap.h"
#include "catalog/pg_tablespace.h"
#include "commands/cluster.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/syscache.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbpersistentfilesysobj.h"

/*
 * This struct is used to pass around the information on tables to be
 * clustered. We need this so we can make a list of them when invoked without
 * a specific table/index pair.
 */
typedef struct
{
	Oid			tableOid;
	Oid			indexOid;
} RelToCluster;


static bool cluster_rel(RelToCluster *rv, bool recheck, ClusterStmt *stmt, bool printError);
static void rebuild_relation(Relation OldHeap, Oid indexOid, ClusterStmt *stmt);
static TransactionId copy_heap_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex);
static List *get_tables_to_cluster(MemoryContext cluster_context);



/*---------------------------------------------------------------------------
 * This cluster code allows for clustering multiple tables at once. Because
 * of this, we cannot just run everything on a single transaction, or we
 * would be forced to acquire exclusive locks on all the tables being
 * clustered, simultaneously --- very likely leading to deadlock.
 *
 * To solve this we follow a similar strategy to VACUUM code,
 * clustering each relation in a separate transaction. For this to work,
 * we need to:
 *	- provide a separate memory context so that we can pass information in
 *	  a way that survives across transactions
 *	- start a new transaction every time a new relation is clustered
 *	- check for validity of the information on to-be-clustered relations,
 *	  as someone might have deleted a relation behind our back, or
 *	  clustered one on a different index
 *	- end the transaction
 *
 * The single-relation case does not have any such overhead.
 *
 * We also allow a relation to be specified without index.	In that case,
 * the indisclustered bit will be looked up, and an ERROR will be thrown
 * if there is no index with the bit set.
 *---------------------------------------------------------------------------
 */
void
cluster(ClusterStmt *stmt, bool isTopLevel)
{
	if (stmt->relation != NULL)
	{
		/* This is the single-relation case. */
		Oid			tableOid,
					indexOid = InvalidOid;
		Relation	rel;
		RelToCluster rvtc;

		/* Find and lock the table */
		rel = heap_openrv(stmt->relation, AccessExclusiveLock);

		tableOid = RelationGetRelid(rel);

		/* Check permissions */
		if (!pg_class_ownercheck(tableOid, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
						   RelationGetRelationName(rel));

		/*
		 * Reject clustering a remote temp table ... their local buffer
		 * manager is not going to cope.
		 */
		if (isOtherTempNamespace(RelationGetNamespace(rel)))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			   errmsg("cannot cluster temporary tables of other sessions")));

		if (stmt->indexname == NULL)
		{
			ListCell   *index;

			/* We need to find the index that has indisclustered set. */
			foreach(index, RelationGetIndexList(rel))
			{
				HeapTuple	idxtuple;
				Form_pg_index indexForm;
				cqContext	*idxcqCtx;

				indexOid = lfirst_oid(index);

				idxcqCtx = caql_beginscan(
						NULL,
						cql("SELECT * FROM pg_index "
							" WHERE indexrelid = :1 ",
							ObjectIdGetDatum(indexOid)));

				idxtuple = caql_getnext(idxcqCtx);

				if (!HeapTupleIsValid(idxtuple))
					elog(ERROR, "cache lookup failed for index %u", indexOid);
				indexForm = (Form_pg_index) GETSTRUCT(idxtuple);
				if (indexForm->indisclustered)
				{
					caql_endscan(idxcqCtx);
					break;
				}
				caql_endscan(idxcqCtx);

				indexOid = InvalidOid;
			}

			if (!OidIsValid(indexOid))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("there is no previously clustered index for table \"%s\"",
								stmt->relation->relname)));
		}
		else
		{
			/*
			 * The index is expected to be in the same namespace as the
			 * relation.
			 */
			indexOid = get_relname_relid(stmt->indexname,
										 rel->rd_rel->relnamespace);
			if (!OidIsValid(indexOid))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
					   errmsg("index \"%s\" for table \"%s\" does not exist",
							  stmt->indexname, stmt->relation->relname)));
		}

		/* All other checks are done in cluster_rel() */
		rvtc.tableOid = tableOid;
		rvtc.indexOid = indexOid;

		/* close relation, keep lock till commit */
		heap_close(rel, NoLock);

		/* Do the job */
		cluster_rel(&rvtc, false, stmt, true);

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			CdbDispatchUtilityStatement((Node *) stmt, "cluster");
		}
	}
	else
	{
		/*
		 * This is the "multi relation" case. We need to cluster all tables
		 * that have some index with indisclustered set.
		 */
		MemoryContext cluster_context;
		List	   *rvs;
		ListCell   *rv;

		/*
		 * We cannot run this form of CLUSTER inside a user transaction block;
		 * we'd be holding locks way too long.
		 */
		PreventTransactionChain(isTopLevel, "CLUSTER");

		/*
		 * Create special memory context for cross-transaction storage.
		 *
		 * Since it is a child of PortalContext, it will go away even in case
		 * of error.
		 */
		cluster_context = AllocSetContextCreate(PortalContext,
												"Cluster",
												ALLOCSET_DEFAULT_MINSIZE,
												ALLOCSET_DEFAULT_INITSIZE,
												ALLOCSET_DEFAULT_MAXSIZE);

		/*
		 * Build the list of relations to cluster.	Note that this lives in
		 * cluster_context.
		 */
		rvs = get_tables_to_cluster(cluster_context);

		/* Commit to get out of starting transaction */
		CommitTransactionCommand();

		/* Ok, now that we've got them all, cluster them one by one */
		foreach(rv, rvs)
		{
			RelToCluster *rvtc = (RelToCluster *) lfirst(rv);

			/* Start a new transaction for each relation. */
			StartTransactionCommand();
			/* functions in indexes may want a snapshot set */
			ActiveSnapshot = CopySnapshot(GetTransactionSnapshot());

			if (Gp_role == GP_ROLE_DISPATCH)
				stmt->new_ind_oids = NIL; /* reset OID map for each iteration */

			bool dispatch = cluster_rel(rvtc, true, stmt, false);

			if (Gp_role == GP_ROLE_DISPATCH && dispatch)
			{

				stmt->relation = makeNode(RangeVar);
				stmt->relation->schemaname = get_namespace_name(get_rel_namespace(rvtc->tableOid));
				stmt->relation->relname = get_rel_name(rvtc->tableOid);
				CdbDispatchUtilityStatement_NoTwoPhase((Node *) stmt, "cluster");
			}
			CommitTransactionCommand();
		}

		/* Start a new transaction for the cleanup work. */
		StartTransactionCommand();

		/* Clean up working storage */
		MemoryContextDelete(cluster_context);


	}


}

/*
 * cluster_rel
 *
 * This clusters the table by creating a new, clustered table and
 * swapping the relfilenodes of the new table and the old table, so
 * the OID of the original table is preserved.	Thus we do not lose
 * GRANT, inheritance nor references to this table (this was a bug
 * in releases thru 7.3).
 *
 * Also create new indexes and swap the filenodes with the old indexes the
 * same way we do for the relation.  Since we are effectively bulk-loading
 * the new table, it's better to create the indexes afterwards than to fill
 * them incrementally while we load the table.
 *
 * Note that we don't support clustering on an AO table. If printError is true,
 * this function errors out when the relation is an AO table. Otherwise,
 * this functions prints out a warning message when the relation is an AO table.
 */
static bool
cluster_rel(RelToCluster *rvtc, bool recheck, ClusterStmt *stmt, bool printError)
{
	Relation	OldHeap;

	/* Check for user-requested abort. */
	CHECK_FOR_INTERRUPTS();

	/*
	 * We grab exclusive access to the target rel and index for the duration
	 * of the transaction.	(This is redundant for the single-transaction
	 * case, since cluster() already did it.)  The index lock is taken inside
	 * check_index_is_clusterable.
	 */
	OldHeap = try_relation_open(rvtc->tableOid, AccessExclusiveLock, false);

	/* If the table has gone away, we can skip processing it */
	if (!OldHeap)
	{
		return false;
	}
	/*
	 * We don't support cluster on an AO table. We print out
	 * a warning/error to the user, and simply return.
	 */
	if (RelationIsAoRows(OldHeap) || RelationIsAoCols(OldHeap))
	{
		int elevel = WARNING;
		
		if (printError)
			elevel = ERROR;
		
		ereport(elevel,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster append-only table \"%s\": not supported",
						RelationGetRelationName(OldHeap))));
		
		relation_close(OldHeap, AccessExclusiveLock);
		return false;
	}
	
	/*
	 * Since we may open a new transaction for each relation, we have to check
	 * that the relation still is what we think it is.
	 *
	 * If this is a single-transaction CLUSTER, we can skip these tests. We
	 * *must* skip the one on indisclustered since it would reject an attempt
	 * to cluster a not-previously-clustered index.
	 */
	if (recheck)
	{
		HeapTuple	tuple;
		Form_pg_index indexForm;
		cqContext	*idxcqCtx;

		/* Check that the user still owns the relation */
		if (!pg_class_ownercheck(rvtc->tableOid, GetUserId()))
		{
			relation_close(OldHeap, AccessExclusiveLock);
			return false;
		}

		/*
		 * Silently skip a temp table for a remote session.  Only doing this
		 * check in the "recheck" case is appropriate (which currently means
		 * somebody is executing a database-wide CLUSTER), because there is
		 * another check in cluster() which will stop any attempt to cluster
		 * remote temp tables by name.  There is another check in
		 * check_index_is_clusterable which is redundant, but we leave it for
		 * extra safety.
		 */
		if (isOtherTempNamespace(RelationGetNamespace(OldHeap)))
		{
			relation_close(OldHeap, AccessExclusiveLock);
			return false;		
		}

		/*
		 * Check that the index still exists
		 */
		if (0 == caql_getcount(
					NULL,
					cql("SELECT COUNT(*) FROM pg_class "
						" WHERE oid = :1 ",
						ObjectIdGetDatum(rvtc->indexOid))))
		{
			relation_close(OldHeap, AccessExclusiveLock);
			return false;
		}

		/*
		 * Check that the index is still the one with indisclustered set.
		 */
		idxcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_index "
					" WHERE indexrelid = :1 ",
					ObjectIdGetDatum(rvtc->indexOid)));

		tuple = caql_getnext(idxcqCtx);

		if (!HeapTupleIsValid(tuple))	/* probably can't happen */
		{
			caql_endscan(idxcqCtx);
			relation_close(OldHeap, AccessExclusiveLock);
			return false;
		}
		indexForm = (Form_pg_index) GETSTRUCT(tuple);
		if (!indexForm->indisclustered)
		{
			caql_endscan(idxcqCtx);
			relation_close(OldHeap, AccessExclusiveLock);
			return false;
		}
		caql_endscan(idxcqCtx);

	}

	/* Check index is valid to cluster on */
	check_index_is_clusterable(OldHeap, rvtc->indexOid, recheck);

	/* rebuild_relation does all the dirty work */
	rebuild_relation(OldHeap, rvtc->indexOid, stmt);

	/* NB: rebuild_relation does heap_close() on OldHeap */
	return true;
}

/*
 * Verify that the specified index is a legitimate index to cluster on
 *
 * Side effect: obtains exclusive lock on the index.  The caller should
 * already have exclusive lock on the table, so the index lock is likely
 * redundant, but it seems best to grab it anyway to ensure the index
 * definition can't change under us.
 */
void
check_index_is_clusterable(Relation OldHeap, Oid indexOid, bool recheck)
{
	Relation	OldIndex;

	OldIndex = index_open(indexOid, AccessExclusiveLock);

	/*
	 * Check that index is in fact an index on the given relation
	 */
	if (OldIndex->rd_index == NULL ||
		OldIndex->rd_index->indrelid != RelationGetRelid(OldHeap))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is not an index for table \"%s\"",
						RelationGetRelationName(OldIndex),
						RelationGetRelationName(OldHeap))));

	/*
	 * Disallow clustering on incomplete indexes (those that might not index
	 * every row of the relation).	We could relax this by making a separate
	 * seqscan pass over the table to copy the missing rows, but that seems
	 * expensive and tedious.
	 */
	if (!heap_attisnull(OldIndex->rd_indextuple, Anum_pg_index_indpred))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on partial index \"%s\"",
						RelationGetRelationName(OldIndex))));

	if (!OldIndex->rd_am->amclusterable)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on index \"%s\" because access method does not support clustering",
						RelationGetRelationName(OldIndex))));

	if (!OldIndex->rd_am->amindexnulls)
	{
		AttrNumber	colno;

		/*
		 * If the AM doesn't index nulls, then it's a partial index unless we
		 * can prove all the rows are non-null.  Note we only need look at the
		 * first column; multicolumn-capable AMs are *required* to index nulls
		 * in columns after the first.
		 */
		colno = OldIndex->rd_index->indkey.values[0];
		if (colno > 0)
		{
			/* ordinary user attribute */
			if (!OldHeap->rd_att->attrs[colno - 1]->attnotnull)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot cluster on index \"%s\" because access method does not handle null values",
								RelationGetRelationName(OldIndex)),
						 recheck
						 ? errhint("You might be able to work around this by marking column \"%s\" NOT NULL, or use ALTER TABLE ... SET WITHOUT CLUSTER to remove the cluster specification from the table.",
						 NameStr(OldHeap->rd_att->attrs[colno - 1]->attname))
						 : errhint("You might be able to work around this by marking column \"%s\" NOT NULL.",
					  NameStr(OldHeap->rd_att->attrs[colno - 1]->attname))));
		}
		else if (colno < 0)
		{
			/* system column --- okay, always non-null */
		}
		else
			/* index expression, lose... */
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot cluster on expressional index \"%s\" because its index access method does not handle null values",
							RelationGetRelationName(OldIndex))));
	}

	/*
	 * Disallow if index is left over from a failed CREATE INDEX CONCURRENTLY;
	 * it might well not contain entries for every heap row, or might not even
	 * be internally consistent.  (But note that we don't check indcheckxmin;
	 * the worst consequence of following broken HOT chains would be that we
	 * might put recently-dead tuples out-of-order in the new table, and there
	 * is little harm in that.)
	 */
	if (!IndexIsValid(OldIndex->rd_index))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot cluster on invalid index \"%s\"",
						RelationGetRelationName(OldIndex))));

	/*
	 * Disallow clustering system relations.  This will definitely NOT work
	 * for shared relations (we have no way to update pg_class rows in other
	 * databases), nor for nailed-in-cache relations (the relfilenode values
	 * for those are hardwired, see relcache.c).  It might work for other
	 * system relations, but I ain't gonna risk it.
	 */
	if (IsSystemRelation(OldHeap))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("\"%s\" is a system catalog",
						RelationGetRelationName(OldHeap))));

	/*
	 * Don't allow cluster on temp tables of other backends ... their local
	 * buffer manager is not going to cope.
	 */
	if (isOtherTempNamespace(RelationGetNamespace(OldHeap)))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			   errmsg("cannot cluster temporary tables of other sessions")));

	/*
	 * Also check for active uses of the relation in the current transaction,
	 * including open scans and pending AFTER trigger events.
	 */
	CheckTableNotInUse(OldHeap, "CLUSTER");

	/* Drop relcache refcnt on OldIndex, but keep lock */
	index_close(OldIndex, NoLock);
}

/*
 * mark_index_clustered: mark the specified index as the one clustered on
 *
 * With indexOid == InvalidOid, will mark all indexes of rel not-clustered.
 *
 * Note: we do transactional updates of the pg_index rows, which are unsafe
 * against concurrent SnapshotNow scans of pg_index.  Therefore this is unsafe
 * to execute with less than full exclusive lock on the parent table;
 * otherwise concurrent executions of RelationGetIndexList could miss indexes.
 */
void
mark_index_clustered(Relation rel, Oid indexOid)
{
	HeapTuple	indexTuple;
	Form_pg_index indexForm;
	Relation	pg_index;
	ListCell   *index;
	cqContext  *idxcqCtx;

	/*
	 * If the index is already marked clustered, no need to do anything.
	 */
	if (OidIsValid(indexOid))
	{
		idxcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_index "
					" WHERE indexrelid = :1 ",
					ObjectIdGetDatum(indexOid)));

		indexTuple = caql_getnext(idxcqCtx);

		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", indexOid);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		if (indexForm->indisclustered)
		{
			caql_endscan(idxcqCtx);
			return;
		}

		caql_endscan(idxcqCtx);
	}

	/*
	 * Check each index of the relation and set/clear the bit as needed.
	 */
	pg_index = heap_open(IndexRelationId, RowExclusiveLock);

	foreach(index, RelationGetIndexList(rel))
	{
		cqContext	cqc;
		cqContext  *pcqCtx;
		Oid			thisIndexOid = lfirst_oid(index);

		pcqCtx = caql_addrel(cqclr(&cqc), pg_index);

		indexTuple = caql_getfirst(
				pcqCtx,
				cql("SELECT * FROM pg_index "
					" WHERE indexrelid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(thisIndexOid)));

		if (!HeapTupleIsValid(indexTuple))
			elog(ERROR, "cache lookup failed for index %u", thisIndexOid);
		indexForm = (Form_pg_index) GETSTRUCT(indexTuple);

		/*
		 * Unset the bit if set.  We know it's wrong because we checked this
		 * earlier.
		 */
		if (indexForm->indisclustered)
		{
			indexForm->indisclustered = false;

			caql_update_current(pcqCtx, indexTuple);
			/* and Update indexes (implicit) */

			/* Ensure we see the update in the index's relcache entry */
			CacheInvalidateRelcacheByRelid(thisIndexOid);
		}
		else if (thisIndexOid == indexOid)
		{
			/* this was checked earlier, but let's be real sure */
			if (!IndexIsValid(indexForm))
				elog(ERROR, "cannot cluster on invalid index %u", indexOid);
			indexForm->indisclustered = true;

			caql_update_current(pcqCtx, indexTuple);
			/* and Update indexes (implicit) */

			/* Ensure we see the update in the index's relcache entry */
			CacheInvalidateRelcacheByRelid(thisIndexOid);
		}
		heap_freetuple(indexTuple);
	}

	heap_close(pg_index, RowExclusiveLock);
}

/*
 * rebuild_relation: rebuild an existing relation in index order
 *
 * OldHeap: table to rebuild --- must be opened and exclusive-locked!
 * indexOid: index to cluster by
 *
 * NB: this routine closes OldHeap at the right time; caller should not.
 */
static void
rebuild_relation(Relation OldHeap, Oid indexOid, ClusterStmt *stmt)
{
	Oid			tableOid = RelationGetRelid(OldHeap);
	Oid			tableSpace = OldHeap->rd_rel->reltablespace;
	Oid			OIDNewHeap;
	char		NewHeapName[NAMEDATALEN];
	TransactionId frozenXid;
	ObjectAddress object;

	/* Mark the correct index as clustered */
	mark_index_clustered(OldHeap, indexOid);

	/* Close relcache entry, but keep lock until transaction commit */
	heap_close(OldHeap, NoLock);

	/*
	 * Create the new heap, using a temporary name in the same namespace as
	 * the existing table.	NOTE: there is some risk of collision with user
	 * relnames.  Working around this seems more trouble than it's worth; in
	 * particular, we can't create the new heap in a different namespace from
	 * the old, or we will have problems with the TEMP status of temp tables.
	 */
	snprintf(NewHeapName, sizeof(NewHeapName), "pg_temp_%u", tableOid);

	OIDNewHeap = make_new_heap(tableOid, NewHeapName, tableSpace,
					&stmt->oidInfo, true /* createAoBlockDirectory */);

	/*
	 * We don't need CommandCounterIncrement() because make_new_heap did it.
	 */

	/*
	 * Copy the heap data into the new table in the desired order.
	 */
	frozenXid = copy_heap_data(OIDNewHeap, tableOid, indexOid);

	/* To make the new heap's data visible (probably not needed?). */
	CommandCounterIncrement();

	/* Swap the physical files of the old and new heaps. */
	swap_relation_files(tableOid, OIDNewHeap, frozenXid, true);

	CommandCounterIncrement();

	/* Destroy new heap with old filenode */
	object.classId = RelationRelationId;
	object.objectId = OIDNewHeap;
	object.objectSubId = 0;

	/*
	 * The new relation is local to our transaction and we know nothing
	 * depends on it, so DROP_RESTRICT should be OK.
	 */
	performDeletion(&object, DROP_RESTRICT);

	/* performDeletion does CommandCounterIncrement at end */

	/*
	 * Rebuild each index on the relation (but not the toast table, which is
	 * all-new at this point).	We do not need CommandCounterIncrement()
	 * because reindex_relation does it.
	 *
	 * Note: because index_build is called via reindex_relation, it will never
	 * set indcheckxmin true for the indexes.  This is OK even though in some
	 * sense we are building new indexes rather than rebuilding existing ones,
	 * because the new heap won't contain any HOT chains at all, let alone
	 * broken ones, so it can't be necessary to set indcheckxmin.
	 */
	reindex_relation(tableOid, false, false, false, false,
					 &stmt->new_ind_oids, Gp_role == GP_ROLE_DISPATCH);
}

/*
 * Create the new table that we will fill with correctly-ordered data.
 */
Oid
make_new_heap(Oid OIDOldHeap, const char *NewName, Oid NewTableSpace,
			  TableOidInfo * oidInfo, 
			  bool createAoBlockDirectory)
{
	TupleDesc	OldHeapDesc,
				tupdesc;
	Oid			OIDNewHeap = InvalidOid;
	Relation	OldHeap;
	Oid 		tOid = InvalidOid;
	Oid			tiOid = InvalidOid;
	Oid			aOid = InvalidOid;
	Oid			aiOid = InvalidOid;
	Oid			*comptypeOid = NULL;
	Oid			*comptypeArrayOid = NULL;
	Oid         blkdirOid = InvalidOid;
	Oid         blkdirIdxOid = InvalidOid;
	Oid         visimapOid = InvalidOid;
	Oid         visimapIndexOid = InvalidOid;
	Oid			*toastComptypeOid = NULL;
	Oid			*aosegComptypeOid = NULL;
	Oid			*aoblkdirComptypeOid = NULL;
	Oid         *aovisimapComptypeOid = NULL;
	HeapTuple	tuple;
	Datum		reloptions;
	bool		isNull;
	bool		is_part;
	cqContext  *pcqCtx;

	OldHeap = heap_open(OIDOldHeap, AccessExclusiveLock);
	OldHeapDesc = RelationGetDescr(OldHeap);

	is_part = !rel_needs_long_lock(OIDOldHeap);

	/* 
	 * Allocate new Oids for the heap, or check that all oids have been passed
	 * in from the master, depending on current GpRole.
	 */
	Assert(oidInfo);
	populate_oidInfo(oidInfo, NewTableSpace, OldHeap->rd_rel->relisshared, true);

	/* 
	 * Extract the oids from the oidInfo, this is used to ensure oid
	 * synchronization between the master and segments.  
	 *
	 * It is the responsibility of the caller to make sure the oidInfo is
	 * correctly dispatched.
	 */
	OIDNewHeap = oidInfo->relOid;
	tOid = oidInfo->toastOid;
	tiOid = oidInfo->toastIndexOid;
	toastComptypeOid = &oidInfo->toastComptypeOid;
	aOid = oidInfo->aosegOid;
	aiOid = oidInfo->aosegIndexOid;
	aosegComptypeOid = &oidInfo->aosegComptypeOid;
	comptypeOid = &oidInfo->comptypeOid;
	comptypeArrayOid = &oidInfo->comptypeArrayOid;
	blkdirOid = oidInfo->aoblkdirOid;
	blkdirIdxOid = oidInfo->aoblkdirIndexOid;
	aoblkdirComptypeOid = &oidInfo->aoblkdirComptypeOid;
	visimapOid = oidInfo->aovisimapOid;
	visimapIndexOid = oidInfo->aovisimapIndexOid;
	aovisimapComptypeOid = &oidInfo->aovisimapComptypeOid;

	/*
	 * Need to make a copy of the tuple descriptor, since
	 * heap_create_with_catalog modifies it.
	 */
	tupdesc = CreateTupleDescCopyConstr(OldHeapDesc);

	/*
	 * Use options of the old heap for new heap.
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(OIDOldHeap)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", OIDOldHeap);

	reloptions = caql_getattr(pcqCtx, Anum_pg_class_reloptions, &isNull);

	if (isNull)
		reloptions = (Datum) 0;

	OIDNewHeap = heap_create_with_catalog(NewName,
										  RelationGetNamespace(OldHeap),
										  NewTableSpace,
										  OIDNewHeap,
										  OldHeap->rd_rel->relowner,
										  tupdesc,
										  OldHeap->rd_rel->relam,
										  OldHeap->rd_rel->relkind,
										  OldHeap->rd_rel->relstorage,
										  OldHeap->rd_rel->relisshared,
										  true,
										  /* bufferPoolBulkLoad */ false,
										  0,
										  ONCOMMIT_NOOP,
                                          NULL,                         /*CDB*/
										  reloptions,
										  allowSystemTableModsDDL,
										  /* valid_opts */ true,
										  comptypeOid,
										  comptypeArrayOid,
						 				  /* persistentTid */ NULL,
						 				  /* persistentSerialNum */ NULL);

	if(oidInfo)
		oidInfo->relOid = OIDNewHeap;

	caql_endscan(pcqCtx);

	/*
	 * Advance command counter so that the newly-created relation's catalog
	 * tuples will be visible to heap_open.
	 */
	CommandCounterIncrement();

	/*
	 * If necessary, create a TOAST table for the new relation, or an Append
	 * Only segment table. Note that AlterTableCreateXXXTable ends with
	 * CommandCounterIncrement(), so that the new tables will be visible for
	 * insertion.
	 */
	AlterTableCreateToastTableWithOid(OIDNewHeap, tOid, tiOid,
									  toastComptypeOid, is_part);
	AlterTableCreateAoSegTableWithOid(OIDNewHeap, aOid, aiOid,
									  aosegComptypeOid, is_part);
	AlterTableCreateAoVisimapTableWithOid(OIDNewHeap, visimapOid, visimapIndexOid,
				aovisimapComptypeOid, is_part);

    if ( createAoBlockDirectory )
    {
	    AlterTableCreateAoBlkdirTableWithOid(OIDNewHeap, blkdirOid, blkdirIdxOid,
										 aoblkdirComptypeOid, is_part);
    }

	cloneAttributeEncoding(OIDOldHeap,
						   OIDNewHeap,
						   RelationGetNumberOfAttributes(OldHeap));

	heap_close(OldHeap, NoLock);

	return OIDNewHeap;
}

/*
 * Do the physical copying of heap data.  Returns the TransactionId used as
 * freeze cutoff point for the tuples.
 */
static TransactionId
copy_heap_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex)
{
	Relation	NewHeap,
				OldHeap,
				OldIndex;
	TupleDesc	oldTupDesc;
	TupleDesc	newTupDesc;
	int			natts;
	Datum	   *values;
	bool	   *isnull;
	IndexScanDesc scan;
	HeapTuple	tuple;
	bool		use_wal;
	TransactionId OldestXmin;
	TransactionId FreezeXid;
	RewriteState rwstate;

	/*
	 * Open the relations we need.
	 */
	NewHeap = heap_open(OIDNewHeap, AccessExclusiveLock);
	OldHeap = heap_open(OIDOldHeap, AccessExclusiveLock);
	OldIndex = index_open(OIDOldIndex, AccessExclusiveLock);

	/*
	 * Their tuple descriptors should be exactly alike, but here we only need
	 * assume that they have the same number of columns.
	 */
	oldTupDesc = RelationGetDescr(OldHeap);
	newTupDesc = RelationGetDescr(NewHeap);
	Assert(newTupDesc->natts == oldTupDesc->natts);

	/* Preallocate values/isnull arrays */
	natts = newTupDesc->natts;
	values = (Datum *) palloc(natts * sizeof(Datum));
	isnull = (bool *) palloc(natts * sizeof(bool));

	/*
	 * If the OldHeap has a toast table, get lock on the toast table to keep
	 * it from being vacuumed.  This is needed because autovacuum processes
	 * toast tables independently of their main tables, with no lock on the
	 * latter.  If an autovacuum were to start on the toast table after we
	 * compute our OldestXmin below, it would use a later OldestXmin, and then
	 * possibly remove as DEAD toast tuples belonging to main tuples we think
	 * are only RECENTLY_DEAD.  Then we'd fail while trying to copy those
	 * tuples.
	 *
	 * We don't need to open the toast relation here, just lock it.  The lock
	 * will be held till end of transaction.
	 */
	if (OldHeap->rd_rel->reltoastrelid)
		LockRelationOid(OldHeap->rd_rel->reltoastrelid, AccessExclusiveLock);

	/*
	 * We need to log the copied data in WAL iff WAL archiving is enabled AND
	 * it's not a temp rel.
	 */
	use_wal = XLogArchivingActive() && !NewHeap->rd_istemp;

	/* use_wal off requires rd_targblock be initially invalid */
	Assert(NewHeap->rd_targblock == InvalidBlockNumber);

	/*
	 * compute xids used to freeze and weed out dead tuples.  We use -1
	 * freeze_min_age to avoid having CLUSTER freeze tuples earlier than a
	 * plain VACUUM would.
	 */
	vacuum_set_xid_limits(-1, OldHeap->rd_rel->relisshared,
						  &OldestXmin, &FreezeXid);

	/*
	 * FreezeXid will become the table's new relfrozenxid, and that mustn't
	 * go backwards, so take the max.
	 */
	if (TransactionIdPrecedes(FreezeXid, OldHeap->rd_rel->relfrozenxid))
		FreezeXid = OldHeap->rd_rel->relfrozenxid;

	/* Initialize the rewrite operation */
	rwstate = begin_heap_rewrite(NewHeap, OldestXmin, FreezeXid, use_wal);

	/*
	 * Scan through the OldHeap in OldIndex order and copy each tuple into the
	 * NewHeap.  To ensure we see recently-dead tuples that still need to be
	 * copied, we scan with SnapshotAny and use HeapTupleSatisfiesVacuum for
	 * the visibility test.
	 */
	scan = index_beginscan(OldHeap, OldIndex,
						   SnapshotAny, 0, (ScanKey) NULL);

	while ((tuple = index_getnext(scan, ForwardScanDirection)) != NULL)
	{
		HeapTuple	copiedTuple;
		bool		isdead;
		int			i;
		MIRROREDLOCK_BUFMGR_DECLARE;

		CHECK_FOR_INTERRUPTS();

		/* -------- MirroredLock ---------- */
		MIRROREDLOCK_BUFMGR_LOCK;

		LockBuffer(scan->xs_cbuf, BUFFER_LOCK_SHARE);

		switch (HeapTupleSatisfiesVacuum(OldHeap, tuple->t_data, OldestXmin,
										 scan->xs_cbuf))
		{
			case HEAPTUPLE_DEAD:
				/* Definitely dead */
				isdead = true;
				break;
			case HEAPTUPLE_LIVE:
			case HEAPTUPLE_RECENTLY_DEAD:
				/* Live or recently dead, must copy it */
				isdead = false;
				break;
			case HEAPTUPLE_INSERT_IN_PROGRESS:

				/*
				 * We should not see this unless it's been inserted earlier in
				 * our own transaction.
				 */
				if (!TransactionIdIsCurrentTransactionId(
									  HeapTupleHeaderGetXmin(tuple->t_data)))
					elog(ERROR, "concurrent insert in progress");
				/* treat as live */
				isdead = false;
				break;
			case HEAPTUPLE_DELETE_IN_PROGRESS:

				/*
				 * We should not see this unless it's been deleted earlier in
				 * our own transaction.
				 */
				Assert(!(tuple->t_data->t_infomask & HEAP_XMAX_IS_MULTI));
				if (!TransactionIdIsCurrentTransactionId(
									  HeapTupleHeaderGetXmax(tuple->t_data)))
					elog(ERROR, "concurrent delete in progress");
				/* treat as recently dead */
				isdead = false;
				break;
			default:
				elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
				isdead = false; /* keep compiler quiet */
				break;
		}

		LockBuffer(scan->xs_cbuf, BUFFER_LOCK_UNLOCK);

		MIRROREDLOCK_BUFMGR_UNLOCK;
		/* -------- MirroredLock ---------- */

		if (isdead)
		{
			/* heap rewrite module still needs to see it... */
			rewrite_heap_dead_tuple(rwstate, tuple);
			continue;
		}

		/*
		 * We cannot simply copy the tuple as-is, for several reasons:
		 *
		 * 1. We'd like to squeeze out the values of any dropped columns, both
		 * to save space and to ensure we have no corner-case failures. (It's
		 * possible for example that the new table hasn't got a TOAST table
		 * and so is unable to store any large values of dropped cols.)
		 *
		 * 2. The tuple might not even be legal for the new table; this is
		 * currently only known to happen as an after-effect of ALTER TABLE
		 * SET WITHOUT OIDS.
		 *
		 * So, we must reconstruct the tuple from component Datums.
		 */
		heap_deform_tuple(tuple, oldTupDesc, values, isnull);

		/* Be sure to null out any dropped columns */
		for (i = 0; i < natts; i++)
		{
			if (newTupDesc->attrs[i]->attisdropped)
				isnull[i] = true;
		}

		copiedTuple = heap_form_tuple(newTupDesc, values, isnull);

		/* Preserve OID, if any */
		if (NewHeap->rd_rel->relhasoids)
			HeapTupleSetOid(copiedTuple, HeapTupleGetOid(tuple));

		/* The heap rewrite module does the rest */
		rewrite_heap_tuple(rwstate, tuple, copiedTuple);

		heap_freetuple(copiedTuple);
	}

	index_endscan(scan);

	/* Write out any remaining tuples, and fsync if needed */
	end_heap_rewrite(rwstate);

	pfree(values);
	pfree(isnull);

	index_close(OldIndex, NoLock);
	heap_close(OldHeap, NoLock);
	heap_close(NewHeap, NoLock);

	return FreezeXid;
}

/*
 * Change dependency links for objects that are being swapped.
 *
 * 'tabletype' can be "TOAST table", "aoseg", "aoblkdir".
 * It is used for printing error messages.
 */
static void
changeDependencyLinks(Oid baseOid1, Oid baseOid2, Oid oid1, Oid oid2,
					  const char *tabletype)
{
	ObjectAddress baseobject, newobject;
	long		count;

	/* Delete old dependencies */
	if (oid1)
	{
		count = deleteDependencyRecordsFor(RelationRelationId, oid1);
		if (count != 1)
			elog(ERROR, "expected one dependency record for %s table, found %ld",
				 tabletype, count);
	}
	
	if (oid2)
	{
		count = deleteDependencyRecordsFor(RelationRelationId, oid2);
		if (count != 1)
			elog(ERROR, "expected one dependency record for %s table, found %ld",
				 tabletype, count);
	}

	/* Register new dependencies */
	baseobject.classId = RelationRelationId;
	baseobject.objectSubId = 0;
	newobject.classId = RelationRelationId;
	newobject.objectSubId = 0;
	
	if (oid1)
	{
		baseobject.objectId = baseOid1;
		newobject.objectId = oid1;
		recordDependencyOn(&newobject, &baseobject, DEPENDENCY_INTERNAL);
	}
	
	if (oid2)
	{
		baseobject.objectId = baseOid2;
		newobject.objectId = oid2;
		recordDependencyOn(&newobject, &baseobject, DEPENDENCY_INTERNAL);
	}
}

/*
 * Swap the physical files of two given relations.
 *
 * We swap the physical identity (reltablespace and relfilenode) while
 * keeping the same logical identities of the two relations.
 *
 * Also swap any TOAST links, so that the toast data moves along with
 * the main-table data.
 *
 * Additionally, the first relation is marked with relfrozenxid set to
 * frozenXid.  It seems a bit ugly to have this here, but all callers would
 * have to do it anyway, so having it here saves a heap_update.  Note: the
 * TOAST table needs no special handling, because since we swapped the links,
 * the entry for the TOAST table will now contain RecentXmin in relfrozenxid,
 * which is the correct value.
 *
 * GPDB: also swap aoseg, aoblkdir links.
*/
void
swap_relation_files(Oid r1, Oid r2, TransactionId frozenXid, bool swap_stats)
{
	Relation	relRelation;
	HeapTuple	reltup1,
			reltup2, reltup0;
	Form_pg_class relform1,
				relform2;
	Oid			swaptemp;
	char		swapchar;
	bool		isAO1, isAO2;
	cqContext	cqc1;
	cqContext	cqc2;
	cqContext  *pcqCtx1;
	cqContext  *pcqCtx2;

	/* 
	 * We need writable copies of both pg_class tuples.
	 */
	relRelation = heap_open(RelationRelationId, RowExclusiveLock);

	/* NOTE: jic 20120925 a bit of a trick here.  Normally, to update
	 * a single tuple, the preferred method is caql_getfirst(), which
	 * returns a writeable copy.  However, in this case, we use
	 * caql_beginscan() and copy it manually.  We use the caql context
	 * for the beginscan/endscan block to update *both* tuples,
	 * because it is cheaper than two single updates
	 */
	pcqCtx1 = caql_beginscan(
			caql_addrel(cqclr(&cqc1), relRelation),
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(r1)));

	reltup0 = caql_getnext(pcqCtx1); /* this is the real "current"
									  * tuple for this context */

	if (!HeapTupleIsValid(reltup0))
		elog(ERROR, "cache lookup failed for relation %u", r1);

	/* copy the tuple so we can update it */
	reltup1 = heap_copytuple(reltup0);

	if (!HeapTupleIsValid(reltup1))
		elog(ERROR, "cache lookup failed for relation %u", r1);
	relform1 = (Form_pg_class) GETSTRUCT(reltup1);

	pcqCtx2 = caql_addrel(cqclr(&cqc2), relRelation);

	reltup2 = caql_getfirst(
			pcqCtx2,	
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(r2)));

	if (!HeapTupleIsValid(reltup2))
		elog(ERROR, "cache lookup failed for relation %u", r2);
	relform2 = (Form_pg_class) GETSTRUCT(reltup2);
	
	isAO1 = (relform1->relstorage == RELSTORAGE_AOROWS ||
			 relform1->relstorage == RELSTORAGE_AOCOLS);
	isAO2 = (relform2->relstorage == RELSTORAGE_AOROWS ||
			 relform2->relstorage == RELSTORAGE_AOCOLS);

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "swap_relation_files (#1): ENTER relation '%s', relation id %u, relfilenode %u, reltablespace %u, relstorage '%c'",
			 relform1->relname.data,
			 r1,
			 relform1->relfilenode,
			 relform1->reltablespace,
			 relform1->relstorage);
	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "swap_relation_files (#2): ENTER relation '%s', relation id %u, relfilenode %u, reltablespace %u, relstorage '%c'",
			 relform2->relname.data,
			 r2,
			 relform2->relfilenode,
			 relform2->reltablespace,
			 relform2->relstorage);

	/*
	 * Actually swap the fields in the two tuples
	 */
	swaptemp = relform1->relfilenode;
	relform1->relfilenode = relform2->relfilenode;
	relform2->relfilenode = swaptemp;

	swaptemp = relform1->reltablespace;
	relform1->reltablespace = relform2->reltablespace;
	relform2->reltablespace = swaptemp;

	swaptemp = relform1->reltoastrelid;
	relform1->reltoastrelid = relform2->reltoastrelid;
	relform2->reltoastrelid = swaptemp;

	/* we should not swap reltoastidxid */

	/* set rel1's frozen Xid */
	Assert(TransactionIdIsNormal(frozenXid));
	relform1->relfrozenxid = frozenXid;

	/*
	 * Swap the AO auxiliary relations and their indexes. Unlike the toast
	 * relations, we need to swap the index oids as well.
	 */
	if (isAO1 && isAO2)
	{
		SwapAppendonlyEntries(r1, r2);
	}
	else if (isAO1)
	{
		TransferAppendonlyEntry(r1, r2);
	}
	else if (isAO2)
	{
		TransferAppendonlyEntry(r2, r1);
	}
	
	/* swap size statistics too, since new rel has freshly-updated stats */
	if (swap_stats)
	{
		int4		swap_pages;
		float4		swap_tuples;

		swap_pages = relform1->relpages;
		relform1->relpages = relform2->relpages;
		relform2->relpages = swap_pages;

		swap_tuples = relform1->reltuples;
		relform1->reltuples = relform2->reltuples;
		relform2->reltuples = swap_tuples;
	}

	/*
	 * Swap relstorage so we will later know how to drop the temporary table with
	 * the right Storage Manager (i.e. Buffer Pool or Append-Only).
	 */
	swapchar = relform1->relstorage;
	relform1->relstorage = relform2->relstorage;
	relform2->relstorage = swapchar;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "swap_relation_files (#1): UPDATE relation '%s', relation id %u, relfilenode %u, reltablespace %u, relstorage '%c'",
			 relform1->relname.data,
			 r1,
			 relform1->relfilenode,
			 relform1->reltablespace,
			 relform1->relstorage);
	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "swap_relation_files (#2): UPDATE relation '%s', relation id %u, relfilenode %u, reltablespace %u, relstorage '%c'",
			 relform2->relname.data,
			 r2,
			 relform2->relfilenode,
			 relform2->reltablespace,
			 relform2->relstorage);

	/* XXX XXX: jic 20120925 don't **EVER** do this -- 
	 * First, fake out caql and make the "current" tuple reltup2, 
	 * then update it.  
	 * Then restore the "current" tuple (reltup0) and update it with
	 * the modified reltup1.  Note that if the "current" does not get
	 * restored then the underlying ReleaseSysCache() in the
	 * caql_endscan() could explode.
	 */

	caql_get_current(pcqCtx1) = reltup2;
	caql_update_current(pcqCtx1, reltup2);

	caql_get_current(pcqCtx1) = reltup0; /* restore real "current" */
	caql_update_current(pcqCtx1, reltup1);
	/* and Update indexes (implicit) */

	caql_endscan(pcqCtx1);

	/*
	 * If we have toast tables associated with the relations being swapped,
	 * change their dependency links to re-associate them with their new
	 * owning relations.  Otherwise the wrong one will get dropped ...
	 *
	 * NOTE: it is possible that only one table has a toast table; this can
	 * happen in CLUSTER if there were dropped columns in the old table, and
	 * in ALTER TABLE when adding or changing type of columns.
	 *
	 * NOTE: at present, a TOAST table's only dependency is the one on its
	 * owning table.  If more are ever created, we'd need to use something
	 * more selective than deleteDependencyRecordsFor() to get rid of only the
	 * link we want.
	 */
	if (relform1->reltoastrelid || relform2->reltoastrelid)
	{
		changeDependencyLinks(r1, r2,
							  relform1->reltoastrelid, relform2->reltoastrelid,
							  "TOAST");
	}

	CommandCounterIncrement();

	/*
	 * Blow away the old relcache entries now.	We need this kluge because
	 * relcache.c keeps a link to the smgr relation for the physical file, and
	 * that will be out of date as soon as we do CommandCounterIncrement.
	 * Whichever of the rels is the second to be cleared during cache
	 * invalidation will have a dangling reference to an already-deleted smgr
	 * relation.  Rather than trying to avoid this by ordering operations just
	 * so, it's easiest to not have the relcache entries there at all.
	 * (Fortunately, since one of the entries is local in our transaction,
	 * it's sufficient to clear out our own relcache this way; the problem
	 * cannot arise for other backends when they see our update on the
	 * non-local relation.)
	 */
	RelationForgetRelation(r1);
	RelationForgetRelation(r2);

	/* Clean up. */
	heap_freetuple(reltup1);
	heap_freetuple(reltup2);

	heap_close(relRelation, RowExclusiveLock);
}

/*
 * Get a list of tables that the current user owns and
 * have indisclustered set.  Return the list in a List * of rvsToCluster
 * with the tableOid and the indexOid on which the table is already
 * clustered.
 */
static List *
get_tables_to_cluster(MemoryContext cluster_context)
{
	cqContext	*pcqCtx;		
	HeapTuple	indexTuple;
	Form_pg_index index;
	MemoryContext old_context;
	RelToCluster *rvtc;
	List	   *rvs = NIL;

	/*
	 * Get all indexes that have indisclustered set and are owned by
	 * appropriate user. System relations or nailed-in relations cannot ever
	 * have indisclustered set, because CLUSTER will refuse to set it when
	 * called with one of them as argument.
	 */

	/* XXX XXX: could bind "AND indrelid = :2 ", GetUserId */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_index "
				" WHERE indisclustered = :1 ",
				BoolGetDatum(true)));

	while (HeapTupleIsValid(indexTuple = caql_getnext(pcqCtx)))
	{
		index = (Form_pg_index) GETSTRUCT(indexTuple);

		if (!pg_class_ownercheck(index->indrelid, GetUserId()))
			continue;

		/*
		 * We have to build the list in a different memory context so it will
		 * survive the cross-transaction processing
		 */
		old_context = MemoryContextSwitchTo(cluster_context);

		rvtc = (RelToCluster *) palloc(sizeof(RelToCluster));
		rvtc->tableOid = index->indrelid;
		rvtc->indexOid = index->indexrelid;
		rvs = lcons(rvtc, rvs);

		MemoryContextSwitchTo(old_context);
	}
	caql_endscan(pcqCtx);

	return rvs;
}


/*
 * populate_oidInfo - Fill in a new OidInfo structure 
 *
 * Note: it would be desirable not to allocate more oids than are really needed,
 * but we don't have enough context here to really know (when called from ALTER
 * TABLE any of them could be needed).
 */
void populate_oidInfo(TableOidInfo *oidInfo, Oid TableSpace, bool relisshared, 
					  bool withTypes)
{
	/* Valid pointers only please */
	Assert(oidInfo);

	/* Make sure any oids we allocate are available on all segments */
	cdb_sync_oid_to_segments();

	switch (Gp_role)
	{
		/*
		 * In Utility or dispatch mode we must allocate new relfilenodes.
		 * It may be desirable to put more restrictions on utility mode, it is
		 * dangerous to allow segment relfilenodes to drift.
		 */
		case GP_ROLE_UTILITY:
		case GP_ROLE_DISPATCH:
		{
			Relation	pg_class_desc;
			Relation	pg_type_desc;

			pg_class_desc = heap_open(RelationRelationId, RowExclusiveLock);

			/* Get new oids for pg_class entities */
			oidInfo->relOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->toastOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->toastIndexOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aosegOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aosegIndexOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aoblkdirOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aoblkdirIndexOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aovisimapOid = 
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);
			oidInfo->aovisimapIndexOid =
				GetNewRelFileNode(TableSpace, relisshared,  pg_class_desc);


			/* gonna update, so don't unlock */
			heap_close(pg_class_desc, NoLock);  

			/* Get new oids for pg_type entities */
			if (withTypes)
			{
				pg_type_desc = heap_open(TypeRelationId, RowExclusiveLock);

				oidInfo->comptypeOid = 
					GetNewRelFileNode(TableSpace, relisshared,  pg_type_desc);
				oidInfo->toastComptypeOid = 
					GetNewRelFileNode(TableSpace, relisshared,  pg_type_desc);
				oidInfo->aosegComptypeOid = 
					GetNewRelFileNode(TableSpace, relisshared,  pg_type_desc);
				oidInfo->aoblkdirComptypeOid = 
					GetNewRelFileNode(TableSpace, relisshared,  pg_type_desc);
				oidInfo->aovisimapComptypeOid = 
					GetNewRelFileNode(TableSpace, relisshared,  pg_type_desc);
				/* We don't need to keep the pg_type lock */
				heap_close(pg_type_desc, RowExclusiveLock);
			}
			break;
		}

		/* 
		 * In EXECUTE mode we expect to have recieved a valid oidInfo from
		 * the master.  Perform some small validations and return. 
		 */
		case GP_ROLE_EXECUTE:
		{
			if (oidInfo->relOid == 0 ||
				oidInfo->toastOid == 0 ||
				oidInfo->toastIndexOid == 0 ||
				oidInfo->aosegOid == 0 ||
				oidInfo->aosegIndexOid == 0 ||
				oidInfo->aoblkdirOid == 0 ||
				oidInfo->aoblkdirIndexOid == 0 ||
				oidInfo->aovisimapOid == 0 ||
				oidInfo->aovisimapIndexOid == 0 ||
				(withTypes && (
					oidInfo->comptypeOid == 0 ||
					oidInfo->toastComptypeOid == 0 ||
					oidInfo->aosegComptypeOid == 0 ||
					oidInfo->aoblkdirComptypeOid == 0 ||
					oidInfo->aovisimapComptypeOid == 0)
					)
				)
			{
				elog(ERROR, "segment recieved incomplete oidInfo from master");
			}
			break;
		}

		/* Unexpected mode? */
		default:
		{
			elog(ERROR, "populate_oidInfo from unexpected dispatch mode");
			break;
		}
	}
}
