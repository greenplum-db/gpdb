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
#include "catalog/gp_policy.h"
#include "catalog/indexing.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdbvars.h"		/* Gp_role */
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/tqual.h"
#include "utils/syscache.h"

static void extract_INT2OID_array(Datum array_datum, int *lenp, int16 **vecp);

GpPolicy *
makeGpPolicy(MemoryContext mcxt, GpPolicyType ptype, int nattrs)
{
	GpPolicy *policy;
	size_t	size;

	if (mcxt == NULL)
		mcxt = CurrentMemoryContext;

	size = sizeof(GpPolicy) + nattrs * sizeof(AttrNumber);
	policy = MemoryContextAlloc(mcxt, size);
	policy->type = T_GpPolicy;
	policy->ptype = ptype; 
	policy->nattrs = nattrs; 
	if (nattrs > 0)
		policy->attrs = (AttrNumber *) ((char*)policy + sizeof(GpPolicy));
	else
		policy->attrs = NULL;

	return policy;
}

/*
 * createReplicatedGpPolicy-- Create a policy with replicated distribution
 */
GpPolicy *
createReplicatedGpPolicy(MemoryContext mcxt)
{
	return makeGpPolicy(mcxt, POLICYTYPE_REPLICATED, 0);
}

/*
 * createRandomPartitionedPolicy -- Create a policy with randomly
 * partitioned distribution
 */
GpPolicy *
createRandomPartitionedPolicy(MemoryContext mcxt)
{
	return makeGpPolicy(mcxt, POLICYTYPE_PARTITIONED, 0);
}

/*
 * createHashPartitionedPolicy-- Create a policy with data
 * partitioned by keys 
 */
GpPolicy *
createHashPartitionedPolicy(MemoryContext mcxt, List *keys)
{
	GpPolicy	*policy;
	ListCell 	*lc;
	int 		idx = 0;
	int 		len = list_length(keys);

	if (len == 0)
		return createRandomPartitionedPolicy(mcxt);

	policy = makeGpPolicy(mcxt, POLICYTYPE_PARTITIONED, len);
	foreach(lc, keys)
	{
		policy->attrs[idx++] = (AttrNumber)lfirst_int(lc);
	}

	return policy;	
}

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed in the specified context.
 */
GpPolicy *
GpPolicyCopy(MemoryContext mcxt, const GpPolicy *src)
{
	GpPolicy   *tgt;
	int i;

	if (!src)
		return NULL;

	tgt = makeGpPolicy(mcxt, src->ptype, src->nattrs);

	for (i = 0; i < src->nattrs; i++)
		tgt->attrs[i] = src->attrs[i];

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

	if (lft->nattrs != rgt->nattrs)
		return false;

	for (i = 0; i < lft->nattrs; i++)
		if (lft->attrs[i] != rgt->attrs[i])
			return false;

	return true;
}								/* GpPolicyEqual */

bool
IsReplicatedTable(Oid relid)
{
	return GpPolicyIsReplicated(GpPolicyFetch(CurrentMemoryContext, relid));
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
 * Returns an GpPolicy object, allocated in the specified context, containing
 * the information.
 *
 * The caller is responsible for passing in a valid relation oid.  This
 * function does not check, and assigns a policy of type POLICYTYPE_ENTRY
 * for any oid not found in gp_distribution_policy.
 */
GpPolicy *
GpPolicyFetch(MemoryContext mcxt, Oid tbloid)
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
				return makeGpPolicy(mcxt, POLICYTYPE_ENTRY, 0);
			}

			return createRandomPartitionedPolicy(mcxt);
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
		bool		isNull;
		Datum		attr;
		int			i,
					nattrs = 0;
		int16	   *attrnums = NULL;

		attr = SysCacheGetAttr(GPPOLICYID, gp_policy_tuple,
							   Anum_gp_policy_type,
							   &isNull);

		Assert(!isNull);

		char ptype = DatumGetChar(attr);

		switch (ptype)
		{
			case SYM_POLICYTYPE_REPLICATED:
				policy = createReplicatedGpPolicy(mcxt);
				break;
			case SYM_POLICYTYPE_PARTITIONED:
				/*
				 * Get the attributes on which to partition.
				 */
				attr = SysCacheGetAttr(GPPOLICYID, gp_policy_tuple,
									   Anum_gp_policy_attrnums,
									   &isNull);

				/*
				 * Get distribution keys only if this table has a policy.
				 */
				if (!isNull)
				{
					extract_INT2OID_array(attr, &nattrs, &attrnums);
					Assert(nattrs >= 0);
				}

				/* Create a GpPolicy object. */
				policy = makeGpPolicy(mcxt, POLICYTYPE_PARTITIONED, nattrs);

				for (i = 0; i < nattrs; i++)
				{
					policy->attrs[i] = attrnums[i];
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
		return makeGpPolicy(mcxt, POLICYTYPE_ENTRY, 0);
	}

	return policy;
}								/* GpPolicyFetch */


/*
 * Extract len and pointer to buffer from an int16[] (vector) Datum
 * representing a PostgreSQL INT2OID type.
 */
static void
extract_INT2OID_array(Datum array_datum, int *lenp, int16 **vecp)
{
	ArrayType  *array_type;

	Assert(lenp != NULL);
	Assert(vecp != NULL);

	array_type = DatumGetArrayTypeP(array_datum);
	Assert(ARR_NDIM(array_type) == 1);
	Assert(ARR_ELEMTYPE(array_type) == INT2OID);
	Assert(ARR_LBOUND(array_type)[0] == 1);
	*lenp = ARR_DIMS(array_type)[0];
	*vecp = (int16 *) ARR_DATA_PTR(array_type);

	return;
}

/*
 * Sets the policy of a table into the gp_distribution_policy table
 * from a GpPolicy structure.
 */
void
GpPolicyStore(Oid tbloid, const GpPolicy *policy)
{
	Relation	gp_policy_rel;
	HeapTuple	gp_policy_tuple = NULL;

	ArrayType  *attrnums;

	bool		nulls[3];
	Datum		values[3];

	Insist(policy->ptype != POLICYTYPE_ENTRY);

	nulls[0] = false;
	nulls[1] = false;
	nulls[2] = false;
	values[0] = ObjectIdGetDatum(tbloid);

	/*
	 * Open and lock the gp_distribution_policy catalog.
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	if (GpPolicyIsReplicated(policy))
	{
		nulls[1] = true;
		values[2] = CharGetDatum(SYM_POLICYTYPE_REPLICATED);
	}
	else
	{
		/*
		 * Convert C arrays into Postgres arrays.
		 */
		int			i;
		Datum	   *akey;

		Assert(GpPolicyIsPartitioned(policy));

		if (policy->nattrs > 0)
		{
			akey = (Datum *) palloc(policy->nattrs * sizeof(Datum));
			for (i = 0; i < policy->nattrs; i++)
				akey[i] = Int16GetDatum(policy->attrs[i]);
			attrnums = construct_array(akey, policy->nattrs,
					INT2OID, 2, true, 's');

			values[1] = PointerGetDatum(attrnums); 
			values[2] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
		}
		else
		{
			nulls[1] = true;
			values[2] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
		}
	}

	gp_policy_tuple = heap_form_tuple(RelationGetDescr(gp_policy_rel), values, nulls);

	/* Insert tuple into the relation */
	simple_heap_insert(gp_policy_rel, gp_policy_tuple);
	CatalogUpdateIndexes(gp_policy_rel, gp_policy_tuple);

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
	ArrayType  *attrnums;
	bool		nulls[3];
	Datum		values[3];
	bool		repl[3];

	Insist(!GpPolicyIsEntry(policy));

	nulls[0] = false;
	nulls[1] = false;
	nulls[2] = false;
	values[0] = ObjectIdGetDatum(tbloid);

	/*
	 * Open and lock the gp_distribution_policy catalog.
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	if (GpPolicyIsReplicated(policy))
	{
		nulls[1] = true;
		values[2] = CharGetDatum(SYM_POLICYTYPE_REPLICATED);
	}
	else
	{
		/*
		 * Convert C arrays into Postgres arrays.
		 */
		int			i;
		Datum	   *akey;

		Assert(GpPolicyIsPartitioned(policy));

		if (policy->nattrs > 0)
		{
			akey = (Datum *) palloc(policy->nattrs * sizeof(Datum));
			for (i = 0; i < policy->nattrs; i++)
				akey[i] = Int16GetDatum(policy->attrs[i]);
			attrnums = construct_array(akey, policy->nattrs,
					INT2OID, 2, true, 's');

			values[1] = PointerGetDatum(attrnums); 
			values[2] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
		}
		else
		{

			nulls[1] = true;
			values[2] = CharGetDatum(SYM_POLICYTYPE_PARTITIONED);
		}
	}

	repl[0] = false;
	repl[1] = true;
	repl[2] = true;


	/*
	 * Select by value of the localoid field
	 */
	ScanKeyInit(&skey,
				Anum_gp_policy_localoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(tbloid));
	scan = systable_beginscan(gp_policy_rel, GpPolicyLocalOidIndexId, true,
							  SnapshotNow, 1, &skey);

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
							   SnapshotNow, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(gp_policy_rel, &tuple->t_self);
	}

	systable_endscan(sscan);

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

	/* replicated table support unique/primary key indexes */
	if (GpPolicyIsReplicated(rel->rd_cdbpolicy))
		return;

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
		GpPolicy *policy = makeGpPolicy(NULL, POLICYTYPE_PARTITIONED, nidxatts);

		for (i = 0; i < nidxatts; i++)
			policy->attrs[i] = indattr[i];

		GpPolicyReplace(rel->rd_id, policy);
		rel->rd_cdbpolicy = policy;

		if (Gp_role == GP_ROLE_DISPATCH)
			elog(NOTICE, "updating distribution policy to match new %s", isprimary ? "primary key" : "unique index");
	}
}

