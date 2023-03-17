/*-------------------------------------------------------------------------
 *
 * aoblkdir.c
 *   This file contains routines to support creation of append-only block
 *   directory tables. This file is identical in functionality to aoseg.c
 *   that exists in the same directory.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/aoblkdir.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aosegfiles.h"
#include "access/aocssegfiles.h"
#include "access/table.h"
#include "catalog/pg_opclass.h"
#include "catalog/aoblkdir.h"
#include "catalog/aocatalog.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/faultinjector.h"
#include "utils/rel.h"
#include "utils/syscache.h"

void
AlterTableCreateAoBlkdirTable(Oid relOid)
{
	Relation	rel;
	TupleDesc	tupdesc;
	IndexInfo  *indexInfo;
	Oid			classObjectId[3];
	int16		coloptions[3];
	List	   *indexColNames;
	bool	   isAO;

	SIMPLE_FAULT_INJECTOR("before_acquire_lock_during_create_ao_blkdir_table");

	/*
	 * Check if this is an appendoptimized table, without acquiring any lock.
	 */
	rel = table_open(relOid, NoLock);
	isAO = RelationIsAppendOptimized(rel);
	table_close(rel, NoLock);
	if (!isAO)
		return;

	/*
	 * Block directory creation must block any transactions that may create
	 * or update indexes such as insert, vacuum and create-index. Concurrent
	 * sequential scans (select) transactions need not be blocked. Index scans
	 * cannot happen because the fact that we are creating block directory
	 * implies no index is yet defined on this appendoptimized table.
	 * Using ShareRowExclusiveLock for this purpose as we allow read-only transactions
	 * being running concurrently. 
	 * 
	 * P.S. GPDB has specific behavior on select statement with locking clause,
	 * refer to comments around checkCanOptSelectLockingClause() for detail. 
	 */
	rel = table_open(relOid, ShareRowExclusiveLock);

	/* Create a tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(4);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1,
					   "segno",
					   INT4OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2,
					   "columngroup_no",
					   INT4OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3,
					   "first_row_no",
					   INT8OID,
					   -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4,
					   "minipage",
					   BYTEAOID,
					   -1, 0);
	/* don't toast 'minipage' */
	tupdesc->attrs[3].attstorage = 'p';

	/*
	 * Create index on segno, columngroup_no and first_row_no.
	 */
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = 3;
	indexInfo->ii_NumIndexKeyAttrs = 3;
	indexInfo->ii_IndexAttrNumbers[0] = 1;
	indexInfo->ii_IndexAttrNumbers[1] = 2;
	indexInfo->ii_IndexAttrNumbers[2] = 3;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = NIL;
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_Unique = true;
	indexInfo->ii_Concurrent = false;
	indexColNames = list_make3("segno", "columngroup_no", "first_row_no");

	classObjectId[0] = INT4_BTREE_OPS_OID;
	classObjectId[1] = INT4_BTREE_OPS_OID;
	classObjectId[2] = INT8_BTREE_OPS_OID;

	coloptions[0] = 0;
	coloptions[1] = 0;
	coloptions[2] = 0;

	(void) CreateAOAuxiliaryTable(rel,
								  "pg_aoblkdir",
								  RELKIND_AOBLOCKDIR,
								  tupdesc,
								  indexInfo, indexColNames,
								  classObjectId,
								  coloptions);

	table_close(rel, NoLock);
}
