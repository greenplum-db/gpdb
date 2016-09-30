/*-------------------------------------------------------------------------
 *
 * aovisimap.c
 *   This file contains routines to support creation of append-only
 *   visibility map tables. This file is identical in functionality to aoseg.c
 *   that exists in the same directory.
 *
 * Copyright (c) 2013, Pivotal Inc.
 *
 * $Id: $
 * $Change: $
 * $DateTime: $
 * $Author: $
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/aovisimap.h"
#include "catalog/aocatalog.h"
#include "catalog/pg_opclass.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/guc.h"

/* Potentially set by contrib/pg_upgrade_support functions */
Oid			binary_upgrade_next_aovisimap_pg_class_oid = InvalidOid;
Oid			binary_upgrade_next_aovisimap_index_pg_class_oid = InvalidOid;
Oid			binary_upgrade_next_aovisimap_pg_type_oid = InvalidOid;

void
AlterTableCreateAoVisimapTableWithOid(Oid relOid, Oid newOid, Oid newIndexOid,
									  Oid * comptypeOid, bool is_part_child)
{
	Relation	rel;
	IndexInfo  *indexInfo;
	TupleDesc	tupdesc;
	Oid			classObjectId[2];
	int16		coloptions[2];

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Create visimap for relation %d, visimap relid %d, visimap idxid %d",
		   relOid, newOid, newIndexOid);

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.  (This is probably redundant in all present
	 * uses...)
	 */
	if (is_part_child)
		rel = heap_open(relOid, NoLock);
	else
		rel = heap_open(relOid, AccessExclusiveLock);

	if (!RelationIsAoRows(rel) && !RelationIsAoCols(rel))
	{
		heap_close(rel, NoLock);
		return;
	}

	/* Create a tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(Natts_pg_aovisimap, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1,
					   "segno",
					   INT4OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2,
					   "first_row_no",
					   INT8OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3,
					   "visimap",
					   BYTEAOID,
					   -1, 0);

	/*
	 * We don't want any toast columns here.
	 */
	tupdesc->attrs[0]->attstorage = 'p';
	tupdesc->attrs[1]->attstorage = 'p';
	tupdesc->attrs[2]->attstorage = 'p';

	/*
	 * Create index on segno, first_row_no.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = 2;
	indexInfo->ii_KeyAttrNumbers[0] = 1;
	indexInfo->ii_KeyAttrNumbers[1] = 2;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = NIL;
	indexInfo->ii_PredicateState = NIL;
	indexInfo->ii_Unique = true;
	indexInfo->ii_Concurrent = false;

	classObjectId[0] = INT4_BTREE_OPS_OID;
	classObjectId[1] = INT8_BTREE_OPS_OID;

	coloptions[0] = 0;
	coloptions[1] = 0;

	/* Use binary-upgrade override for pg_class.oid and pg_type.oid, if supplied. */
	if (IsBinaryUpgrade && OidIsValid(binary_upgrade_next_aovisimap_pg_class_oid))
	{
		Assert(newOid == InvalidOid);
		newOid = binary_upgrade_next_aovisimap_pg_class_oid;
		binary_upgrade_next_aovisimap_pg_class_oid = InvalidOid;
	}
	if (IsBinaryUpgrade && OidIsValid(binary_upgrade_next_aovisimap_index_pg_class_oid))
	{
		Assert(newIndexOid == InvalidOid);
		newIndexOid = binary_upgrade_next_aovisimap_index_pg_class_oid;
		binary_upgrade_next_aovisimap_index_pg_class_oid = InvalidOid;
	}
	if (IsBinaryUpgrade && OidIsValid(binary_upgrade_next_aovisimap_pg_type_oid))
	{
		Assert(*comptypeOid == InvalidOid);
		*comptypeOid = binary_upgrade_next_aovisimap_pg_type_oid;
		binary_upgrade_next_aovisimap_pg_type_oid = InvalidOid;
	}

	(void) CreateAOAuxiliaryTable(rel,
								  "pg_aovisimap",
								  RELKIND_AOVISIMAP,
								  newOid, newIndexOid, comptypeOid,
								  tupdesc, indexInfo, classObjectId, coloptions);

	heap_close(rel, NoLock);
}
