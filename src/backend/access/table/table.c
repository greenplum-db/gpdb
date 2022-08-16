/*-------------------------------------------------------------------------
 *
 * table.c
 *	  Generic routines for table related code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/table/table.c
 *
 *
 * NOTES
 *	  This file contains table_ routines that implement access to tables (in
 *	  contrast to other relation types like indexes) that are independent of
 *	  individual table access methods.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "storage/lmgr.h"

#include "catalog/namespace.h"
#include "cdb/cdbvars.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"


/* ----------------
 *		table_open - open a table relation by relation OID
 *
 *		This is essentially relation_open plus check that the relation
 *		is not an index nor a composite type.  (The caller should also
 *		check that it's not a view or foreign table before assuming it has
 *		storage.)
 * ----------------
 */
Relation
table_open(Oid relationId, LOCKMODE lockmode)
{
	Relation	r;

	r = relation_open(relationId, lockmode);

	if (r->rd_rel->relkind == RELKIND_INDEX ||
		r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is an index",
						RelationGetRelationName(r))));
	else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a composite type",
						RelationGetRelationName(r))));

	return r;
}

/* ----------------
 *		table_openrv - open a table relation specified
 *		by a RangeVar node
 *
 *		As above, but relation is specified by a RangeVar.
 * ----------------
 */
Relation
table_openrv(const RangeVar *relation, LOCKMODE lockmode)
{
	Relation	r;

	r = relation_openrv(relation, lockmode);

	if (r->rd_rel->relkind == RELKIND_INDEX ||
		r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is an index",
						RelationGetRelationName(r))));
	else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a composite type",
						RelationGetRelationName(r))));

	return r;
}

/* ----------------
 *		table_openrv_extended - open a table relation specified
 *		by a RangeVar node
 *
 *		As above, but optionally return NULL instead of failing for
 *		relation-not-found.
 * ----------------
 */
Relation
table_openrv_extended(const RangeVar *relation, LOCKMODE lockmode,
					  bool missing_ok)
{
	Relation	r;

	r = relation_openrv_extended(relation, lockmode, missing_ok);

	if (r)
	{
		if (r->rd_rel->relkind == RELKIND_INDEX ||
			r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is an index",
							RelationGetRelationName(r))));
		else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is a composite type",
							RelationGetRelationName(r))));
	}

	return r;
}

/* ----------------
 *		try_table_open - open a heap relation by relation OID
 *
 *		As above, but relation return NULL for relation-not-found
 * ----------------
 */
Relation
try_table_open(Oid relationId, LOCKMODE lockmode, bool noWait)
{
	Relation	r;

	r = try_relation_open(relationId, lockmode, noWait);

	if (!RelationIsValid(r))
		return NULL;

	if (r->rd_rel->relkind == RELKIND_INDEX)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is an index",
						RelationGetRelationName(r))));
	else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("\"%s\" is a composite type",
						RelationGetRelationName(r))));

	return r;
}

/* ----------------
 *		table_close - close a table
 *
 *		If lockmode is not "NoLock", we then release the specified lock.
 *
 *		Note that it is often sensible to hold a lock beyond relation_close;
 *		in that case, the lock is released automatically at xact end.
 *		----------------
 */
void
table_close(Relation relation, LOCKMODE lockmode)
{
	relation_close(relation, lockmode);
}



/*
 * CdbTryOpenTable -- Opens a table with a specified lock mode.
 *
 * CDB: Like try_table_open, except that it will upgrade the lock when needed
 * for distributed tables.
 *
 * Note1: Postgres will always hold RowExclusiveLock for DMLs
 * Note2: INSERT statement will not call this function.
 * Note3: This function may return NULL (e.g. when just before we open the table,
 *        other transaction drop the table), caller should check it.
 *
 * Greenplum only upgrade lock level for UPDATE and DELETE statement under some
 * condition:
 *   1. always upgrade when gp_enable_global_deadlock_detector is not set
 *   2. when gp_enable_global_deadlock_detector is set:
 *     a. if target table is AO|AOCO table, upgrade the lock level
 *     b. if target table is heap table, just like Postgres, do not upgrade
 */
Relation
CdbTryOpenTable(Oid relid, LOCKMODE reqmode, bool *lockUpgraded)
{
	LOCKMODE    lockmode;
	Relation    rel;

	/*
	 * This if-else statement will try to open the relation and
	 * save the lockmode it uses to open the relation.
	 *
	 * If we are doing expclicit UPDATE|DELETE on catalogs (this can
	 * only be possible when GUC allow_system_table_mods is set), the
	 * update or delete does not hold locks on catalog on segments, so
	 * we do not need to consider lock-upgrade for DML on catalogs.
	 */
	if (reqmode == RowExclusiveLock &&
		Gp_role == GP_ROLE_DISPATCH &&
		relid >= FirstNormalObjectId)
	{
		if (!gp_enable_global_deadlock_detector)
		{
			/*
			 * Without GDD, to avoid global deadlock, always
			 * upgrade locklevel to ExclusiveLock
			 */
			lockmode = ExclusiveLock;
			rel = try_table_open(relid, lockmode, false);
		}
		else
		{
			lockmode = RowExclusiveLock;
			rel = try_table_open(relid, lockmode, false);
			if (RelationIsAppendOptimized(rel))
			{
				/*
				 * AO|AOCO table does not support concurrently
				 * update or delete on segments, so we first close
				 * the relation and reopen it using upgraded lockmode.
				 * NOTE: during this time window, there is a race that
				 * the table with relid is dropped, and will lead to
				 * returning NULL. This will not cause any problem
				 * because it is caller's duty to check NULL pointer.
				 */
				table_close(rel, RowExclusiveLock);
				lockmode = ExclusiveLock;
				rel = try_table_open(relid, lockmode, false);
			}
		}
	}
	else
	{
		lockmode = reqmode;
		rel = try_table_open(relid, lockmode, false);
	}

	if (lockUpgraded != NULL && lockmode > reqmode)
		*lockUpgraded = true;

	/* inject fault after holding the lock */
	SIMPLE_FAULT_INJECTOR("upgrade_row_lock");

	return rel;
}                                       /* CdbOpenTable */

/*
 * CdbOpenTable -- Opens a table with a specified lock mode.
 *
 * CDB: Like CdbTryOpenTable, except that it guarantees either
 * an error or a valid opened table returned.
 */
Relation
CdbOpenTable(Oid relid, LOCKMODE reqmode, bool *lockUpgraded)
{
	Relation rel;

	rel = CdbTryOpenTable(relid, reqmode, lockUpgraded);

	if (!RelationIsValid(rel))
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("table not found (OID %u)", relid),
				 errdetail("This can be validly caused by a concurrent delete operation on this object.")));
	}

	return rel;

}                                       /* CdbOpenTable */
