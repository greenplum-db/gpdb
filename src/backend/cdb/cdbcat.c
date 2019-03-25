/*-------------------------------------------------------------------------
 *
 * cdbcat.c
 *	  Provides routines for reading info from mpp schema tables
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbcat.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/gp_policy.h"
#include "catalog/indexing.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"		/* Gp_role */
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/tqual.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

/*
 * The default numsegments when creating tables.  The value can be an integer
 * between 1 and getgpsegmentCount() or one of below magic numbers:
 *
 * - GP_DEFAULT_NUMSEGMENTS_FULL: all the segments;
 * - GP_DEFAULT_NUMSEGMENTS_RANDOM: pick a random set of segments each time;
 * - GP_DEFAULT_NUMSEGMENTS_MINIMAL: the minimal set of segments;
 *
 * A wrapper macro GP_POLICY_DEFAULT_NUMSEGMENTS() is defined to get the default
 * numsegments according to the setting of this variable, always use that macro
 * instead of this variable.
 */
int			gp_create_table_default_numsegments = GP_DEFAULT_NUMSEGMENTS_FULL;


GpPolicy *
makeGpPolicy(GpPolicyType ptype, int nattrs, int numsegments)
{
	GpPolicy   *policy;
	size_t		size;
	char	   *p;

	size = MAXALIGN(sizeof(GpPolicy)) +
		MAXALIGN(nattrs * sizeof(AttrNumber)) +
		MAXALIGN(nattrs * sizeof(Oid));
	p = palloc(size);
	policy = (GpPolicy *) p;
	p += MAXALIGN(sizeof(GpPolicy));
	if (nattrs > 0)
	{
		policy->attrs = (AttrNumber *) p;
		p += MAXALIGN(nattrs * sizeof(AttrNumber));
		policy->opclasses = (Oid *) p;
		p += MAXALIGN(nattrs * sizeof(Oid));
	}
	else
	{
		policy->attrs = NULL;
		policy->opclasses = NULL;
	}

	policy->type = T_GpPolicy;
	policy->ptype = ptype; 
	policy->numsegments = numsegments;
	policy->nattrs = nattrs; 

	Assert(numsegments > 0);
	if (numsegments == GP_POLICY_INVALID_NUMSEGMENTS())
	{
		Assert(!"what's the proper value of numsegments?");
	}

	return policy;
}

/*
 * createReplicatedGpPolicy-- Create a policy with replicated distribution
 */
GpPolicy *
createReplicatedGpPolicy(int numsegments)
{
	return makeGpPolicy(POLICYTYPE_REPLICATED, 0, numsegments);
}

/*
 * createRandomPartitionedPolicy -- Create a policy with randomly
 * partitioned distribution
 */
GpPolicy *
createRandomPartitionedPolicy(int numsegments)
{
	return makeGpPolicy(POLICYTYPE_PARTITIONED, 0, numsegments);
}

/*
 * createHashPartitionedPolicy-- Create a policy with data
 * partitioned by keys 
 */
GpPolicy *
createHashPartitionedPolicy(List *keys, List *opclasses, int numsegments)
{
	GpPolicy	*policy;
	ListCell 	*lc;
	ListCell 	*lop;
	int 		idx = 0;
	int 		len = list_length(keys);

	Assert(list_length(keys) == list_length(opclasses));

	if (len == 0)
		return createRandomPartitionedPolicy(numsegments);

	policy = makeGpPolicy(POLICYTYPE_PARTITIONED, len, numsegments);
	forboth(lc, keys, lop, opclasses)
	{
		policy->attrs[idx] = (AttrNumber) lfirst_int(lc);
		policy->opclasses[idx] = (Oid) lfirst_oid(lop);
		idx++;
	}

	return policy;	
}

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed.
 */
GpPolicy *
GpPolicyCopy(const GpPolicy *src)
{
	GpPolicy   *tgt;
	int i;

	if (!src)
		return NULL;

	tgt = makeGpPolicy(src->ptype, src->nattrs, src->numsegments);

	for (i = 0; i < src->nattrs; i++)
	{
		tgt->attrs[i] = src->attrs[i];
		tgt->opclasses[i] = src->opclasses[i];
	}

	return tgt;
}								/* GpPolicyCopy */

/*
 * GpPolicyEqual -- Test equality of policies (by value only).
 */
bool
GpPolicyEqual(const GpPolicy *lft, const GpPolicy *rgt)
{
	int			i;

	if (lft == rgt)
		return true;

	if (!lft || !rgt)
		return false;

	if (lft->ptype != rgt->ptype)
		return false;

	if (lft->numsegments != rgt->numsegments)
		return false;

	if (lft->nattrs != rgt->nattrs)
		return false;

	for (i = 0; i < lft->nattrs; i++)
	{
		if (lft->attrs[i] != rgt->attrs[i])
			return false;
		if (lft->opclasses[i] != rgt->opclasses[i])
			return false;
	}

	return true;
}								/* GpPolicyEqual */

bool
IsReplicatedTable(Oid relid)
{
	return GpPolicyIsReplicated(GpPolicyFetch(relid));
}

/*
 * Returns true only if the policy is a replicated.
 */
bool
GpPolicyIsReplicated(const GpPolicy *policy)
{
	if (policy == NULL)
		return false;

	return policy->ptype == POLICYTYPE_REPLICATED;
}

/*
 * Randomly-partitioned or keys-partitioned
 */
bool
GpPolicyIsPartitioned(const GpPolicy *policy)
{
	return (policy != NULL && policy->ptype == POLICYTYPE_PARTITIONED);
}

bool
GpPolicyIsRandomPartitioned(const GpPolicy *policy)
{
	if (policy == NULL)
		return false;

	return policy->ptype == POLICYTYPE_PARTITIONED &&
			policy->nattrs == 0;
}

bool
GpPolicyIsHashPartitioned(const GpPolicy *policy)
{
	if (policy == NULL)
		return false;

	return policy->ptype == POLICYTYPE_PARTITIONED &&
			policy->nattrs > 0;
}

bool
GpPolicyIsEntry(const GpPolicy *policy)
{
	if (policy == NULL)
		return false;

	return policy->ptype == POLICYTYPE_ENTRY;
}

/*
 * GpPolicyFetch
 *
 * Looks up the distribution policy of given relation from
 * gp_distribution_policy table (or by implicit rules for external tables)
 * Returns an GpPolicy object, allocated in the current memory context,
 * containing the information.
 *
 * The caller is responsible for passing in a valid relation oid.  This
 * function does not check, and assigns a policy of type POLICYTYPE_ENTRY
 * for any oid not found in gp_distribution_policy.
 */
GpPolicy *
GpPolicyFetch(Oid tbloid)
{
	GpPolicy   *policy = NULL;	/* The result */
	HeapTuple	gp_policy_tuple = NULL;

	/*
	 * EXECUTE-type external tables have an "ON ..." specification, stored in
	 * pg_exttable.location. See if it's "MASTER_ONLY". Other types of
	 * external tables have a gp_distribution_policy row, like normal tables.
	 */
	if (get_rel_relstorage(tbloid) == RELSTORAGE_EXTERNAL)
	{
		/*
		 * An external table really should have a pg_exttable entry, but
		 * there's currently a transient state during creation of an external
		 * table, where the pg_class entry has been created, and its loaded
		 * into the relcache, before the pg_exttable entry has been created.
		 * Silently ignore missing pg_exttable rows to cope with that.
		 */
		ExtTableEntry *e = GetExtTableEntryIfExists(tbloid);

		/*
		 * Writeable external tables have gp_distribution_policy entries, like
		 * regular tables. Readable external tables are implicitly randomly
		 * distributed, except for "EXECUTE ... ON MASTER" ones.
		 */
		if (e && !e->iswritable)
		{
			char	   *on_clause = (char *) strVal(linitial(e->execlocations));

			if (strcmp(on_clause, "MASTER_ONLY") == 0)
			{
				return makeGpPolicy(POLICYTYPE_ENTRY,
									0, getgpsegmentCount());
			}

			return createRandomPartitionedPolicy(getgpsegmentCount());
		}
	}

	/*
	 * Select by value of the localoid field
	 *
	 * SELECT * FROM gp_distribution_policy WHERE localoid = :1
	 */
	gp_policy_tuple = SearchSysCache1(GPPOLICYID,
									 ObjectIdGetDatum(tbloid));

	/*
	 * Read first (and only) tuple
	 */
	if (HeapTupleIsValid(gp_policy_tuple))
	{
		Form_gp_policy policyform = (Form_gp_policy) GETSTRUCT(gp_policy_tuple);
		bool		isNull;
		int			i;
		int			nattrs;
		int2vector *distkey;
		oidvector  *distopclasses;

		/*
		 * Sanity check of numsegments.
		 *
		 * Currently, Gxact always use a fixed size of cluster after the Gxact started,
		 * If a table is expanded after Gxact started, we should report an error,
		 * otherwise, planner will arrange a gang whose size is larger than the size
		 * of cluster and dispatcher cannot handle this.
		 */
		if ((Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_EXECUTE) &&
			policyform->numsegments > getgpsegmentCount())
		{
			ReleaseSysCache(gp_policy_tuple);
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_YET),
					 errmsg("cannot access table \"%s\" in current transaction",
							get_rel_name(tbloid)),
					 errdetail("New segments are concurrently added to the cluster during the execution of current transaction, "
							   "the table has data on some of the new segments, "
							   "but these new segments are invisible and inaccessible to current transaction."),
					 errhint("Re-run the query in a new transaction.")));
		}

		switch (policyform->policytype)
		{
			case SYM_POLICYTYPE_REPLICATED:
				policy = createReplicatedGpPolicy(policyform->numsegments);
				break;
			case SYM_POLICYTYPE_PARTITIONED:
				/*
				 * Get the attributes on which to partition.
				 */
				distkey = (int2vector *) DatumGetPointer(
					SysCacheGetAttr(GPPOLICYID, gp_policy_tuple,
									Anum_gp_policy_distkey,
									&isNull));

				/*
				 * Get distribution keys only if this table has a policy.
				 */
				if (!isNull)
				{
					nattrs = distkey->dim1;
					distopclasses = (oidvector *) DatumGetPointer(
						SysCacheGetAttr(GPPOLICYID, gp_policy_tuple,
										Anum_gp_policy_distclass,
										&isNull));
					Assert(!isNull);
					Assert(distopclasses->dim1 == nattrs);
				}
				else
					nattrs = 0;

				/* Create a GpPolicy object. */
				policy = makeGpPolicy(POLICYTYPE_PARTITIONED,
									  nattrs, policyform->numsegments);

				for (i = 0; i < nattrs; i++)
				{
					policy->attrs[i] = distkey->values[i];
					policy->opclasses[i] = distopclasses->values[i];
				}
				break;
			default:
				ReleaseSysCache(gp_policy_tuple);
				elog(ERROR, "unrecognized distribution policy type");	
		}

		ReleaseSysCache(gp_policy_tuple);
	}

	/* Interpret absence of a valid policy row as POLICYTYPE_ENTRY */
	if (policy == NULL)
	{
		return makeGpPolicy(POLICYTYPE_ENTRY,
							0, getgpsegmentCount());
	}

	return policy;
}								/* GpPolicyFetch */

/*
 * Sets the policy of a table into the gp_distribution_policy table
 * from a GpPolicy structure.
 */
void
GpPolicyStore(Oid tbloid, const GpPolicy *policy)
{
	Relation	gp_policy_rel;
	HeapTuple	gp_policy_tuple = NULL;

	bool		nulls[5];
	Datum		values[5];
	ObjectAddress myself,
				referenced;
	int			i;

	/* Sanity check the policy and its opclasses before storing it. */
	if (policy->ptype == POLICYTYPE_ENTRY)
		elog(ERROR, "cannot store entry-type policy in gp_distribution_policy");
	for (i = 0; i < policy->nattrs; i++)
	{
		if (policy->opclasses[i] == InvalidOid)
			elog(ERROR, "no hash operator class for distribution key column %d", i + 1);
	}

	nulls[0] = false;
	nulls[1] = false;
	nulls[2] = false;
	nulls[3] = false;
	nulls[4] = false;
	values[0] = ObjectIdGetDatum(tbloid);
	values[2] = Int32GetDatum(policy->numsegments);

	/*
	 * Open and lock the gp_distribution_policy catalog.
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	if (GpPolicyIsReplicated(policy))
	{
		values[1] = CharGetDatum(SYM_POLICYTYPE_REPLICATED);
	}
	else
	{
		/*
		 * Convert C arrays into Postgres arrays.
		 */
		Assert(GpPolicyIsPartitioned(policy));

		values[1] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
	}
	int2vector *attrnums = buildint2vector(policy->attrs, policy->nattrs);
	oidvector  *opclasses = buildoidvector(policy->opclasses, policy->nattrs);

	values[3] = PointerGetDatum(attrnums);
	values[4] = PointerGetDatum(opclasses);

	gp_policy_tuple = heap_form_tuple(RelationGetDescr(gp_policy_rel), values, nulls);

	/* Insert tuple into the relation */
	simple_heap_insert(gp_policy_rel, gp_policy_tuple);
	CatalogUpdateIndexes(gp_policy_rel, gp_policy_tuple);

	/*
	 * Register the table as dependent on the operator classes used in the
	 * distribution key.
	 *
	 * XXX: This prevents you from dropping the operator class, which is
	 * good. However, CASCADE behaviour is not so nice: if you do DROP
	 * OPERATOR CLASS CASCADE, we drop the whole table. Ideally, we would
	 * just change the policy to randomly distributed.
	 */
	myself.classId = RelationRelationId;
	myself.objectId = tbloid;
	myself.objectSubId = 0;
	for (i = 0; i < policy->nattrs; i++)
	{
		referenced.classId = OperatorClassRelationId;
		referenced.objectId = policy->opclasses[i];
		referenced.objectSubId = 0;

		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/*
	 * Close the gp_distribution_policy relcache entry without unlocking. We
	 * have updated the catalog: consequently the lock must be held until end
	 * of transaction.
	 */
	heap_close(gp_policy_rel, NoLock);
}								/* GpPolicyStore */

/*
 * Sets the policy of a table into the gp_distribution_policy table
 * from a GpPolicy structure.
 */
void
GpPolicyReplace(Oid tbloid, const GpPolicy *policy)
{
	Relation	gp_policy_rel;
	HeapTuple	gp_policy_tuple = NULL;
	SysScanDesc scan;
	ScanKeyData skey;
	bool		nulls[5];
	Datum		values[5];
	bool		repl[5];
	ObjectAddress myself,
				referenced;
	int			i;

	/* Sanity check the policy and its opclasses before storing it. */
	if (policy->ptype == POLICYTYPE_ENTRY)
		elog(ERROR, "cannot store entry-type policy in gp_distribution_policy");
	for (i = 0; i < policy->nattrs; i++)
	{
		if (policy->opclasses[i] == InvalidOid)
			elog(ERROR, "no hash operator class for distribution key column %d", i + 1);
	}

	nulls[0] = false;
	nulls[1] = false;
	nulls[2] = false;
	nulls[3] = false;
	nulls[4] = false;
	values[0] = ObjectIdGetDatum(tbloid);
	values[2] = Int32GetDatum(policy->numsegments);

	/*
	 * Open and lock the gp_distribution_policy catalog.
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	if (GpPolicyIsReplicated(policy))
	{
		values[1] = CharGetDatum(SYM_POLICYTYPE_REPLICATED);
	}
	else
	{
		/*
		 * Convert C arrays into Postgres arrays.
		 */
		Assert(GpPolicyIsPartitioned(policy));

		values[1] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
	}
	int2vector *attrnums = buildint2vector(policy->attrs, policy->nattrs);
	oidvector  *opclasses = buildoidvector(policy->opclasses, policy->nattrs);

	values[3] = PointerGetDatum(attrnums);
	values[4] = PointerGetDatum(opclasses);

	repl[0] = false;
	repl[1] = true;
	repl[2] = true;
	repl[3] = true;
	repl[4] = true;


	/*
	 * Select by value of the localoid field
	 */
	ScanKeyInit(&skey,
				Anum_gp_policy_localoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(tbloid));
	scan = systable_beginscan(gp_policy_rel, GpPolicyLocalOidIndexId, true,
							  NULL, 1, &skey);

	/*
	 * Read first (and only ) tuple
	 */
	if ((gp_policy_tuple = systable_getnext(scan)) != NULL)
	{
		HeapTuple	newtuple = heap_modify_tuple(gp_policy_tuple,
												 RelationGetDescr(gp_policy_rel),
												 values, nulls, repl);

		simple_heap_update(gp_policy_rel, &gp_policy_tuple->t_self, newtuple);
		CatalogUpdateIndexes(gp_policy_rel, newtuple);

		heap_freetuple(newtuple);
	}
	else
	{
		gp_policy_tuple = heap_form_tuple(gp_policy_rel->rd_att, values, nulls);
		(void) simple_heap_insert(gp_policy_rel, gp_policy_tuple);
		CatalogUpdateIndexes(gp_policy_rel, gp_policy_tuple);
	}
	systable_endscan(scan);

	/*
	 * Remove old dependencies on opclasses, and store dependencies on the
	 * new ones.
	 */
	deleteDependencyRecordsForClass(RelationRelationId, tbloid,
									OperatorClassRelationId, DEPENDENCY_NORMAL);

	myself.classId = RelationRelationId;
	myself.objectId = tbloid;
	myself.objectSubId = 0;
	for (i = 0; i < policy->nattrs; i++)
	{
		referenced.classId = OperatorClassRelationId;
		referenced.objectId = policy->opclasses[i];
		referenced.objectSubId = 0;

		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/*
	 * Close the gp_distribution_policy relcache entry without unlocking. We
	 * have updated the catalog: consequently the lock must be held until end
	 * of transaction.
	 */
	heap_close(gp_policy_rel, NoLock);
}								/* GpPolicyReplace */


/*
 * Removes the policy of a table from the gp_distribution_policy table
 * Does nothing if the policy doesn't exist.
 */
void
GpPolicyRemove(Oid tbloid)
{
	Relation	gp_policy_rel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;

	/*
	 * Open and lock the gp_distribution_policy catalog.
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	/* Delete the policy entry from the catalog. */
	ScanKeyInit(&scankey, Anum_gp_policy_localoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(tbloid));

	sscan = systable_beginscan(gp_policy_rel, GpPolicyLocalOidIndexId, true,
							   NULL, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(gp_policy_rel, &tuple->t_self);
	}

	systable_endscan(sscan);

	/*
	 * This is currently only used while dropping the whole relation, which
	 * removes all pg_depend entries. So no need to remove them here.
	 */

	/*
	 * Close the gp_distribution_policy relcache entry without unlocking. We
	 * have updated the catalog: consequently the lock must be held until end
	 * of transaction.
	 */
	heap_close(gp_policy_rel, NoLock);
}								/* GpPolicyRemove */


/*
 * Does the supplied GpPolicy support unique indexing on the specified
 * attributes?
 *
 * If the table is distributed randomly, no unique indexing is supported.
 * Otherwise, the set of columns being indexed should be a superset of the
 * policy.
 *
 * If the proposed index does not match the distribution policy but the relation
 * is empty and does not have a primary key or unique index, update the
 * distribution policy to match the index definition (MPP-101), as long as it
 * doesn't contain expressions.
 */
void
checkPolicyForUniqueIndex(Relation rel, AttrNumber *indattr, int nidxatts,
						  bool isprimary, bool has_exprs, bool has_pkey,
						  bool has_ukey)
{
	Bitmapset  *polbm = NULL;
	Bitmapset  *indbm = NULL;
	int			i;
	TupleDesc	desc = RelationGetDescr(rel);

	/*
	 * POLICYTYPE_ENTRY normally means it's a system table or a table created
	 * in utility mode, so unique/primary key is allowed anywhere.
	 */
	if (GpPolicyIsEntry(rel->rd_cdbpolicy))
		return;

	/*
	 * Firstly, unique/primary key indexes aren't supported if we're
	 * distributing randomly.
	 */
	if (GpPolicyIsRandomPartitioned(rel->rd_cdbpolicy))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("%s and DISTRIBUTED RANDOMLY are incompatible",
						isprimary ? "PRIMARY KEY" : "UNIQUE")));
	}

	/* Replicated table support unique/primary key indexes */
	if (GpPolicyIsReplicated(rel->rd_cdbpolicy))
		return;

	Assert(GpPolicyIsHashPartitioned(rel->rd_cdbpolicy));

	/*
	 * We use bitmaps to make intersection tests easier. As noted, order is
	 * not relevant so looping is just painful.
	 */
	for (i = 0; i < rel->rd_cdbpolicy->nattrs; i++)
		polbm = bms_add_member(polbm, rel->rd_cdbpolicy->attrs[i]);
	for (i = 0; i < nidxatts; i++)
	{
		if (indattr[i] < 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot create %s on system column",
							isprimary ? "primary key" : "unique index")));

		indbm = bms_add_member(indbm, indattr[i]);
	}

	Assert(bms_membership(polbm) != BMS_EMPTY_SET);
	Assert(bms_membership(indbm) != BMS_EMPTY_SET);

	/*
	 * If the existing policy is not a subset, we must either error out or
	 * update the distribution policy. It might be tempting to say that even
	 * when the policy is a subset, we should update it to match the index
	 * definition. The problem then is that if the user actually wants a
	 * distribution on (a, b) but then creates an index on (a, b, c) we'll
	 * change the policy underneath them.
	 *
	 * What is really needed is a new field in gp_distribution_policy telling
	 * us if the policy has been explicitly set.
	 */
	if (!bms_is_subset(polbm, indbm))
	{
		if ((Gp_role == GP_ROLE_DISPATCH && cdbRelMaxSegSize(rel) != 0) ||
			has_pkey || has_ukey || has_exprs)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%s must contain all columns in the "
							"distribution key of relation \"%s\"",
							isprimary ? "PRIMARY KEY" : "UNIQUE index",
							RelationGetRelationName(rel))));
		}

		/* update policy since table is not populated yet. See MPP-101 */
		GpPolicy *policy = makeGpPolicy(POLICYTYPE_PARTITIONED, nidxatts,
										rel->rd_cdbpolicy->numsegments);

		for (i = 0; i < nidxatts; i++)
		{
			AttrNumber attno = indattr[i];
			Oid			opclass;

			opclass = cdb_default_distribution_opclass_for_type(desc->attrs[attno - 1]->atttypid);
			if (!opclass)
			{
				/*
				 * The datatype has no default opclass. Can't use it in the
				 * distribution key.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("%s must contain all columns in the "
								"distribution key of relation \"%s\"",
								isprimary ? "PRIMARY KEY" : "UNIQUE index",
								RelationGetRelationName(rel))));
			}

			policy->attrs[i] = attno;
			policy->opclasses[i] = opclass;
		}

		GpPolicyReplace(rel->rd_id, policy);

		MemoryContext oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
		rel->rd_cdbpolicy = GpPolicyCopy(policy);
		MemoryContextSwitchTo(oldcontext);

		if (Gp_role == GP_ROLE_DISPATCH)
			elog(NOTICE, "updating distribution policy to match new %s", isprimary ? "primary key" : "unique index");
	}
}

