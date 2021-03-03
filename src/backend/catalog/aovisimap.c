/*-------------------------------------------------------------------------
 *
 * aovisimap.c
 *   This file contains routines to support creation of append-only
 *   visibility map tables. This file is identical in functionality to aoseg.c
 *   that exists in the same directory.
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/aovisimap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/table.h"
#include "catalog/aovisimap.h"
#include "catalog/aocatalog.h"
#include "catalog/pg_opclass.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/guc.h"
#include "utils/rel.h"

void
AlterTableCreateAoVisimapTable(Oid relOid)
{
	Relation	rel;
	IndexInfo  *indexInfo;
	TupleDesc	tupdesc;
	Oid			classObjectId[2];
	int16		coloptions[2];
	List	   *indexColNames;

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Create visimap for relation %d",
		   relOid);

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.  (This is probably redundant in all present
	 * uses...)
	 */
	rel = table_open(relOid, AccessExclusiveLock);

	if (!RelationIsAppendOptimized(rel))
	{
		table_close(rel, NoLock);
		return;
	}

	/* Create a tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(Natts_pg_aovisimap);
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
	/* don't toast 'visibmap' */
	tupdesc->attrs[2].attstorage = 'p';

	/*
	 * Create index on segno, first_row_no.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = 2;
	indexInfo->ii_NumIndexKeyAttrs = 2;
	indexInfo->ii_IndexAttrNumbers[0] = 1;
	indexInfo->ii_IndexAttrNumbers[1] = 2;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = NIL;
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_Unique = true;
	indexInfo->ii_Concurrent = false;

	indexColNames = list_make2("segno", "first_row_no");

	classObjectId[0] = INT4_BTREE_OPS_OID;
	classObjectId[1] = INT8_BTREE_OPS_OID;

	coloptions[0] = 0;
	coloptions[1] = 0;

	(void) CreateAOAuxiliaryTable(rel,
								  "pg_aovisimap",
								  RELKIND_AOVISIMAP,
								  tupdesc,
								  indexInfo, indexColNames,
								  classObjectId, coloptions);

	table_close(rel, NoLock);
}
