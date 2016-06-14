/*-------------------------------------------------------------------------
 *
 * cdbcat.c
 *	  Provides routines for reading info from mpp schema tables
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "catalog/catquery.h" 
#include "catalog/pg_exttable.h"
#include "catalog/pg_namespace.h"
#include "catalog/gp_policy.h"
#include "catalog/indexing.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbrelsize.h"
#include "cdb/cdbvars.h"		/* Gp_role */
#include "catalog/pg_type.h"
#include "utils/array.h"

static void extract_INT2OID_array(Datum array_datum, int *lenp, int16 **vecp);

/*
 * createRandomDistribution -- Create a policy with random distribution
 */
GpPolicy *
createRandomDistribution(void)
{
	GpPolicy   *p = NULL;

	p = (GpPolicy *) palloc0(sizeof(GpPolicy));
	p->ptype = POLICYTYPE_PARTITIONED;
	p->nattrs = 0;

	return p;
}

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed in the specified context.
 */
GpPolicy *
GpPolicyCopy(MemoryContext mcxt, const GpPolicy *src)
{
	GpPolicy  *tgt;
	size_t		nb;

	if (!src)
		return NULL;
	nb = sizeof(*src) - sizeof(src->attrs) + src->nattrs * sizeof(src->attrs[0]);
   	tgt = (GpPolicy *)MemoryContextAlloc(mcxt, nb);
	memcpy(tgt, src, nb);
	return tgt;
}	                            /* GpPolicyCopy */

/*
 * GpPolicyEqual -- Test equality of policies (by value only).
 */
bool
GpPolicyEqual(const GpPolicy *lft, const GpPolicy *rgt)
{
	int	i;
	
	if (lft == rgt)
	    return true;
	
	if (!lft || !rgt)
		return false;
	
	if ( lft->ptype != rgt->ptype )
	    return false;
	
	if ( lft->nattrs != rgt->nattrs )
	    return false;
	
	for ( i = 0; i < lft->nattrs; i++ )
	    if ( lft->attrs[i] != rgt->attrs[i] )
	        return false;
	
	return true;
}	                            /* GpPolicyEqual */

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
	GpPolicy  *policy = NULL;	/* The result */
	Relation	gp_policy_rel;
	cqContext	cqc;
	HeapTuple	gp_policy_tuple = NULL;

	/*
	 * Skip if qExec or utility mode.
	 */
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		policy = (GpPolicy *) MemoryContextAlloc(mcxt, SizeOfGpPolicy(0));
		policy->ptype = POLICYTYPE_ENTRY;
		policy->nattrs = 0;

		return policy;
	}

	/*
	 * EXECUTE-type external tables have an "ON ..." specification, stored
	 * in pg_exttable.location. See if it's "MASTER_ONLY". Other types of
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
		 * Writeable external tables have gp_distribution_policy entries,
		 * like regular tables. Readable external tables are implicitly
		 * randomly distributed, except for "EXECUTE ... ON MASTER" ones.
		 */
		if (e && !e->iswritable)
		{
			if (e->command)
			{
				char	   *on_clause = (char *) strVal(linitial(e->locations));

				if (strcmp(on_clause, "MASTER_ONLY") == 0)
				{
					policy = (GpPolicy *) MemoryContextAlloc(mcxt, SizeOfGpPolicy(0));
					policy->ptype = POLICYTYPE_ENTRY;
					policy->nattrs = 0;
					return policy;
				}
			}
			policy = (GpPolicy *) MemoryContextAlloc(mcxt, SizeOfGpPolicy(0));
			policy->ptype = POLICYTYPE_PARTITIONED;
			policy->nattrs = 0;
			return policy;
		}
	}

	/*
	 * We need to read the gp_distribution_policy table
	 */
	gp_policy_rel = heap_open(GpPolicyRelationId, AccessShareLock);

	/*
	 * Select by value of the localoid field
	 */
	gp_policy_tuple = caql_getfirst(
			caql_addrel(cqclr(&cqc), gp_policy_rel), 
			cql("SELECT * FROM gp_distribution_policy "
				" WHERE localoid = :1 ",
				ObjectIdGetDatum(tbloid)));

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

		/*
		 * Get the attributes on which to partition.
		 */
		attr = heap_getattr(gp_policy_tuple, Anum_gp_policy_attrnums, 
							RelationGetDescr(gp_policy_rel), &isNull);

		/*
		 * Get distribution keys only if this table has a policy.
		 */
		if(!isNull)
		{
			extract_INT2OID_array(attr, &nattrs, &attrnums);
			Assert(nattrs >= 0);
		}

		/* Create a GpPolicy object. */
		policy = (GpPolicy *) MemoryContextAlloc(mcxt, SizeOfGpPolicy(nattrs));
		policy->ptype = POLICYTYPE_PARTITIONED;
		policy->nattrs = nattrs;
		for (i = 0; i < nattrs; i++)
		{
			policy->attrs[i] = attrnums[i];
		}
	}

	/*
	 * Cleanup the scan and relation objects.
	 */
	heap_close(gp_policy_rel, AccessShareLock);

	/* Interpret absence of a valid policy row as POLICYTYPE_ENTRY */
	if (policy == NULL)
	{
		policy = (GpPolicy *) MemoryContextAlloc(mcxt, SizeOfGpPolicy(0));
		policy->ptype = POLICYTYPE_ENTRY;
		policy->nattrs = 0;
	}

	return policy;
}                               /* GpPolicyFetch */


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

	bool		nulls[2];
	Datum		values[2];
	cqContext	cqc;
	cqContext  *pcqCtx;

	Insist(policy->ptype == POLICYTYPE_PARTITIONED);

    /*
     * Open and lock the gp_distribution_policy catalog.
     */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), gp_policy_rel),
			cql("INSERT INTO gp_distribution_policy ",
				NULL));

	/*
	 * Convert C arrays into Postgres arrays.
	 */
	if (policy->nattrs > 0)
	{
		int			i;
		Datum	   *akey;

		akey = (Datum *) palloc(policy->nattrs * sizeof(Datum));
		for (i = 0; i < policy->nattrs; i++)
			akey[i] = Int16GetDatum(policy->attrs[i]);
		attrnums = construct_array(akey, policy->nattrs,
								   INT2OID, 2, true, 's');
	}
	else
	{
		attrnums = NULL;
	}

	nulls[0] = false;
	nulls[1] = false;
	values[0] = ObjectIdGetDatum(tbloid);

	if (attrnums)
		values[1] = PointerGetDatum(attrnums);
	else
		nulls[1] = true;

	gp_policy_tuple = caql_form_tuple(pcqCtx, values, nulls);

	/* Insert tuple into the relation */
	caql_insert(pcqCtx, gp_policy_tuple); /* implicit update of index as well*/

	/*
     * Close the gp_distribution_policy relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be held until
     * end of transaction.
     */
	caql_endscan(pcqCtx);
    heap_close(gp_policy_rel, NoLock);
}                               /* GpPolicyStore */

/*
 * Sets the policy of a table into the gp_distribution_policy table
 * from a GpPolicy structure.
 */
void
GpPolicyReplace(Oid tbloid, const GpPolicy *policy)
{
	Relation	gp_policy_rel;
	HeapTuple	gp_policy_tuple = NULL;
	cqContext	cqc;
	cqContext  *pcqCtx;

	ArrayType  *attrnums;

	bool		nulls[2];
	Datum		values[2];
	bool		repl[2];

	Insist(policy->ptype == POLICYTYPE_PARTITIONED);

    /*
     * Open and lock the gp_distribution_policy catalog.
     */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), gp_policy_rel);

	/*
	 * Convert C arrays into Postgres arrays.
	 */
	if (policy->nattrs > 0)
	{
		int			i;
		Datum	   *akey;

		akey = (Datum *) palloc(policy->nattrs * sizeof(Datum));
		for (i = 0; i < policy->nattrs; i++)
			akey[i] = Int16GetDatum(policy->attrs[i]);
		attrnums = construct_array(akey, policy->nattrs,
								   INT2OID, 2, true, 's');
	}
	else
	{
		attrnums = NULL;
	}

	nulls[0] = false;
	nulls[1] = false;
	values[0] = ObjectIdGetDatum(tbloid);

	if (attrnums)
		values[1] = PointerGetDatum(attrnums);
	else
		nulls[1] = true;
		
	repl[0] = false;
	repl[1] = true;


	/*
	 * Select by value of the localoid field
	 */
	gp_policy_tuple = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM gp_distribution_policy "
				" WHERE localoid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(tbloid)));

	/*
	 * Read first (and only ) tuple
	 */

	if (HeapTupleIsValid(gp_policy_tuple))
	{
		
		HeapTuple newtuple = caql_modify_current(pcqCtx, values,
												 nulls, repl);
								
		caql_update_current(pcqCtx, newtuple); 
		/* and Update indexes (implicit) */

		heap_freetuple(newtuple);
	}
	else
	{
		gp_policy_tuple = caql_form_tuple(pcqCtx, values, nulls);
		caql_insert(pcqCtx, gp_policy_tuple);
		/* and Update indexes (implicit) */
	}
	
	/*
     * Close the gp_distribution_policy relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be held until
     * end of transaction.
     */
    heap_close(gp_policy_rel, NoLock);
}                               /* GpPolicyReplace */


/*
 * Removes the policy of a table from the gp_distribution_policy table
 * Does nothing if the policy doesn't exist.
 */
void
GpPolicyRemove(Oid tbloid)
{
	Relation	gp_policy_rel;
	cqContext	cqc;

    /*
     * Open and lock the gp_distribution_policy catalog.
     */
	gp_policy_rel = heap_open(GpPolicyRelationId, RowExclusiveLock);

	/* Delete the policy entry from the catalog. */
	(void) caql_getcount(
		caql_addrel(cqclr(&cqc), gp_policy_rel),
		cql("DELETE FROM gp_distribution_policy "
			" WHERE localoid = :1 ",
			ObjectIdGetDatum(tbloid)));

	/*
     * Close the gp_distribution_policy relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be held until
     * end of transaction.
     */
    heap_close(gp_policy_rel, NoLock);
}                               /* GpPolicyRemove */

/*
 * Returns true only if the policy is a randomly distributed.  In other cases,
 * including non-distributed table case, returns false.
 */
bool
GpPolicyIsRandomly(GpPolicy *policy)
{
	if (policy == NULL)
		return false;

	return policy->ptype == POLICYTYPE_PARTITIONED && policy->nattrs == 0;
}

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
	Bitmapset *polbm = NULL;
	Bitmapset *indbm = NULL;
	int i;
	GpPolicy *pol = rel->rd_cdbpolicy;

	/* 
	 * Firstly, unique/primary key indexes aren't supported if we're
	 * distributing randomly.
	 */
	if (GpPolicyIsRandomly(pol))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("%s and DISTRIBUTED RANDOMLY are incompatible",
						isprimary ? "PRIMARY KEY" : "UNIQUE")));
	}

	/* 
	 * We use bitmaps to make intersection tests easier. As noted, order is
	 * not relevant so looping is just painful.
	 */
	for (i = 0; i < pol->nattrs; i++)
		polbm = bms_add_member(polbm, pol->attrs[i]);
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
	 * What is really needed is a new field in gp_distribution_policy telling us
	 * if the policy has been explicitly set.
	 */
	if (!bms_is_subset(polbm, indbm))
	{
		if (cdbRelSize(rel) != 0 || has_pkey || has_ukey || has_exprs)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("%s must contain all columns in the "
							"distribution key of relation \"%s\"",
							isprimary ? "PRIMARY KEY" : "UNIQUE index",
						RelationGetRelationName(rel))));
		}
		else
		{
			/* update policy since table is not populated yet. See MPP-101 */
			GpPolicy *policy = palloc(sizeof(GpPolicy) + 
									  (sizeof(AttrNumber) * nidxatts));
			policy->ptype = POLICYTYPE_PARTITIONED;
			policy->nattrs = 0;
			for (i = 0; i < nidxatts; i++)
				policy->attrs[policy->nattrs++] = indattr[i];	

			GpPolicyReplace(rel->rd_id, policy);

			if (isprimary)
				elog(NOTICE, "updating distribution policy to match new primary key");
			else
				elog(NOTICE, "updating distribution policy to match new unique index");
		}
	}
}
