/*-------------------------------------------------------------------------
 *
 * aoseg.c
 *	  This file contains routines to support creation of append-only segment
 *    entry tables. This file is identical in functionality to toasting.c that
 *    exists in the same directory. One is in charge of creating toast tables
 *    (pg_toast_<reloid>) and the other append only segment position tables
 *    (pg_aoseg_<reloid>).
 *
 * Portions Copyright (c) 2008-2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/aoseg.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/aoseg.h"
#include "catalog/pg_opclass.h"
#include "catalog/aocatalog.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/syscache.h"


void
AlterTableCreateAoSegTable(Oid relOid, bool is_part_child)
{
	TupleDesc	tupdesc;
	Relation	rel;
	const char *prefix;
	Oid			classObjectId[1];
	int16		coloptions[1];
	Relation	class_rel;
	HeapTuple	reltup;

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.  (This is probably redundant in all present
	 * uses...)
	 */
	if (is_part_child)
		rel = heap_open(relOid, NoLock);
	else
		rel = heap_open(relOid, AccessExclusiveLock);

	if(RelationIsAoRows(rel))
	{
		prefix = "pg_aoseg";

		/* this is pretty painful...  need a tuple descriptor */
		tupdesc = CreateTemplateTupleDesc(8, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1,
						"segno",
						INT4OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2,
						"eof",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3,
						"tupcount",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4,
						"varblockcount",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5,
						"eofuncompressed",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6,
						"modcount",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7,
						"formatversion",
						INT2OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8,
						"state",
						INT2OID,
						-1, 0);

	}
	else if (RelationIsAoCols(rel))
	{
		prefix = "pg_aocsseg";

		/*
		 * XXX
		 * At this moment, we hardwire the rel aocs info.
		 * Essentially, we assume total vertical partition, and
		 * we do not do datatype specific compression.
		 *
		 * In order to make things right, we need to first fix
		 * the DefineRelation, so that we store the per column
		 * info, then, we need to open the catalog, pull out
		 * info here.
		 */

		/*
		 * XXX We do not handle add/drop column etc nicely yet.
		 */

		/*
		 * Assuming full vertical partition, we want to include
		 * the following in the seg table.
		 *
		 * segno int,               -- whatever purpose ao use it
		 * tupcount bigint          -- total tup
		 * varblockcount bigint,    -- total varblock
		 * vpinfo varbinary(max)    -- vertical partition info encoded in 
		 *                             binary. NEEDS TO BE REFACTORED
		 *                             INTO MULTIPLE COLUMNS!!
		 * state (smallint)         -- state of the segment file
		 */

		tupdesc = CreateTemplateTupleDesc(7, false);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1,
						   "segno",
						   INT4OID,
						   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2,
						   "tupcount",
						   INT8OID,
						   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3,
						   "varblockcount",
						   INT8OID,
						   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4,
						   "vpinfo",
						   BYTEAOID,
						   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5,
						"modcount",
						INT8OID,
						-1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6,
						   "formatversion",
						   INT2OID,
						   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7,
						   "state",
						   INT2OID,
						   -1, 0);
	}
	else
	{
		heap_close(rel, NoLock);
		return;
	}

	classObjectId[0] = INT4_BTREE_OPS_OID;

	coloptions[0] = 0;

	(void) CreateAOAuxiliaryTable(rel, prefix, RELKIND_AOSEGMENTS,
								  tupdesc, NULL, classObjectId, coloptions);

	/*
	 * Store the toast table's OID in the parent relation's pg_class row
	 */
	class_rel = heap_open(RelationRelationId, RowExclusiveLock);

	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(relOid),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))
		elog(ERROR, "cache lookup failed for relation %u", relOid);

	if (!IsBootstrapProcessingMode())
	{
		/* normal case, use a transactional update */
		simple_heap_update(class_rel, &reltup->t_self, reltup);

		/* Keep catalog indexes current */
		CatalogUpdateIndexes(class_rel, reltup);
	}
	else
	{
		/* While bootstrapping, we cannot UPDATE, so overwrite in-place */
		heap_inplace_update(class_rel, reltup);
	}

	heap_freetuple(reltup);

	heap_close(class_rel, RowExclusiveLock);

	heap_close(rel, NoLock);
}
