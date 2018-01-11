/*
 * aocatalog.c
 *
 * Helper function to support the creation of
 * append-only auxiliary relations such as block directories and visimaps.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/aocatalog.c
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/aocatalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "catalog/gp_fastsequence.h"

/*
 * Create append-only auxiliary relations for target relation rel.
 * Returns true if they are newly created.  If pg_appendonly has already
 * known those tables, don't create them and returns false.
 */
bool
CreateAOAuxiliaryTable(
		Relation rel,
		const char *auxiliaryNamePrefix,
		char relkind,
		TupleDesc tupledesc,
		IndexInfo  *indexInfo,
		Oid	*classObjectId,
		int16 *coloptions)
{
	char aoauxiliary_relname[NAMEDATALEN];
	char aoauxiliary_idxname[NAMEDATALEN];
	bool shared_relation;
	Oid relOid, aoauxiliary_relid = InvalidOid;
	Oid aoauxiliary_idxid = InvalidOid;
	ObjectAddress baseobject;
	ObjectAddress aoauxiliaryobject;

	Assert(RelationIsValid(rel));
	Assert(RelationIsAoRows(rel) || RelationIsAoCols(rel));
	Assert(auxiliaryNamePrefix);
	Assert(tupledesc);
	Assert(classObjectId);
	if (relkind != RELKIND_AOSEGMENTS)
		Assert(indexInfo);

	shared_relation = rel->rd_rel->relisshared;
	/*
	 * We cannot allow creating an auxiliary table for a shared relation
	 * after initdb (because there's no way to let other databases know
	 * this visibility map.
	 */
	if (shared_relation && !IsBootstrapProcessingMode())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("shared tables cannot have append-only auxiliary relations after initdb")));

	relOid = RelationGetRelid(rel);

	switch(relkind)
	{
		case RELKIND_AOVISIMAP:
			GetAppendOnlyEntryAuxOids(relOid, SnapshotNow, NULL,
				NULL, NULL, &aoauxiliary_relid, &aoauxiliary_idxid);
			break;
		case RELKIND_AOBLOCKDIR:
			GetAppendOnlyEntryAuxOids(relOid, SnapshotNow, NULL,
				&aoauxiliary_relid, &aoauxiliary_idxid, NULL, NULL);
			break;
		case RELKIND_AOSEGMENTS:
			GetAppendOnlyEntryAuxOids(relOid, SnapshotNow,
				&aoauxiliary_relid,
				NULL, NULL, NULL, NULL);
			break;
		default:
			elog(ERROR, "unsupported auxiliary relkind '%c'", relkind);
	}

	/*
	 * Does it have the auxiliary relation?
	 */
	if (OidIsValid(aoauxiliary_relid))
	{
		return false;
	}

	snprintf(aoauxiliary_relname, sizeof(aoauxiliary_relname),
			 "%s_%u", auxiliaryNamePrefix, relOid);
	snprintf(aoauxiliary_idxname, sizeof(aoauxiliary_idxname),
			 "%s_%u_index", auxiliaryNamePrefix, relOid);

	/*
	 * We place auxiliary relation in the pg_aoseg namespace
	 * even if its master relation is a temp table. There cannot be
	 * any naming collision, and the auxiliary relation will be
	 * destroyed when its master is, so there is no need to handle
	 * the aovisimap relation as temp.
	 */
	aoauxiliary_relid = heap_create_with_catalog(aoauxiliary_relname,
											     PG_AOSEGMENT_NAMESPACE,
											     rel->rd_rel->reltablespace,
											     InvalidOid,
											     rel->rd_rel->relowner,
											     tupledesc,
												 NIL,
											     /* relam */ InvalidOid,
											     relkind,
											     RELSTORAGE_HEAP,
											     shared_relation,
											     true,
											     /* bufferPoolBulkLoad */ false,
											     0,
											     ONCOMMIT_NOOP,
											     NULL, /* GP Policy */
											     (Datum) 0,
											     true,
												 /* valid_opts */ false,
											     /* persistentTid */ NULL,
											     /* persistentSerialNum */ NULL);

	/* Make this table visible, else index creation will fail */
	CommandCounterIncrement();

	/* Create an index on AO auxiliary tables (like visimap) except for pg_aoseg table */
	if (relkind != RELKIND_AOSEGMENTS)
	{
		aoauxiliary_idxid = index_create(aoauxiliary_relid,
										 aoauxiliary_idxname,
										 InvalidOid,
										 indexInfo,
										 BTREE_AM_OID,
										 rel->rd_rel->reltablespace,
										 classObjectId, coloptions, (Datum) 0,
										 true, false, true, false,
										 false, NULL);

		/* Unlock target table -- no one can see it */
		UnlockRelationOid(aoauxiliary_relid, ShareLock);

		/* Unlock the index -- no one can see it anyway */
		UnlockRelationOid(aoauxiliary_idxid, AccessExclusiveLock);
	}

	/*
	 * Store the auxiliary table's OID in the parent relation's pg_appendonly row.
	 * TODO (How to generalize this?)
	 */
	switch (relkind)
	{
		case RELKIND_AOVISIMAP:
			UpdateAppendOnlyEntryAuxOids(relOid, InvalidOid,
								 InvalidOid, InvalidOid,
								 aoauxiliary_relid, aoauxiliary_idxid);
			break;
		case RELKIND_AOBLOCKDIR:
			UpdateAppendOnlyEntryAuxOids(relOid, InvalidOid,
								 aoauxiliary_relid, aoauxiliary_idxid,
								 InvalidOid, InvalidOid);
			break;
		case RELKIND_AOSEGMENTS:
			/* Add initial entries in gp_fastsequence */
			InsertInitialFastSequenceEntries(aoauxiliary_relid);

			UpdateAppendOnlyEntryAuxOids(relOid,
								 aoauxiliary_relid,
								 InvalidOid, InvalidOid,
								 InvalidOid, InvalidOid);
			break;
		default:
			elog(ERROR, "unsupported auxiliary relkind '%c'", relkind);
	}

	/*
	 * Register dependency from the auxiliary table to the master, so that the
	 * aoseg table will be deleted if the master is.
	 */
	baseobject.classId = RelationRelationId;
	baseobject.objectId = relOid;
	baseobject.objectSubId = 0;
	aoauxiliaryobject.classId = RelationRelationId;
	aoauxiliaryobject.objectId = aoauxiliary_relid;
	aoauxiliaryobject.objectSubId = 0;

	recordDependencyOn(&aoauxiliaryobject, &baseobject, DEPENDENCY_INTERNAL);

	/*
	 * Make changes visible
	 */
	CommandCounterIncrement();

	return true;
}
