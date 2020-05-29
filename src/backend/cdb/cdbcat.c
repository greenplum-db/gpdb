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
#include "access/hash.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/gp_policy.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "cdb/cdbcat.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"		/* Gp_role */
#include "foreign/foreign.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/tqual.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

static int errdetails_index_policy(char *attname,
								   Oid policy_indclass,
								   Oid policy_eqop,
								   Oid found_indclass,
								   Oid *exclop,
								   index_check_policy_compatible_context *context);

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

	Assert(numsegments > 0 ||
		   (ptype == POLICYTYPE_ENTRY && numsegments == -1));

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
	 * EXECUTE-type external tables have an "ON ..." specification.
	 * See if it's "MASTER_ONLY". Other types of external tables have a
	 * gp_distribution_policy row, like normal tables.
	 */
	if (rel_is_external_table(tbloid))
	{
		/*
		 * An external table really should have a catalog entry, but
		 * there's currently a transient state during creation of an external
		 * table, where the pg_class entry has been created, and its loaded
		 * into the relcache, before the catalog entry has been created.
		 * Silently ignore missing catalog rows to cope with that.
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
				return makeGpPolicy(POLICYTYPE_ENTRY, 0, -1);
			}

			return createRandomPartitionedPolicy(getgpsegmentCount());
		}
	}
	else if (get_rel_relstorage(tbloid) == RELSTORAGE_FOREIGN)
	{
		/*
		 * Similar to the external table creation, there is a transient state
		 * during creation of a foreign table, where the pg_class entry has
		 * been created, before the pg_foreign_table entry has been created.
		 */
		HeapTuple	tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(tbloid));

		if (HeapTupleIsValid(tp))
		{
			ReleaseSysCache(tp);

			ForeignTable *f = GetForeignTable(tbloid);

			if (f->exec_location == FTEXECLOCATION_ALL_SEGMENTS)
			{
				/*
				 * Currently, foreign tables do not support a distribution
				 * policy, as opposed to writable external tables. For now,
				 * we will create a random partitioned policy for foreign
				 * tables that run on all segments. This will allow writing
				 * to foreign tables from all segments when the mpp_execute
				 * option is set to 'all segments'
				 */
				return createRandomPartitionedPolicy(getgpsegmentCount());
			}
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
		return makeGpPolicy(POLICYTYPE_ENTRY, 0, -1);
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
 * Is the supplied GpPolicy compatible with a unique or exclusion constraint
 * index?
 *
 * This is used both when a new index is created (CREATE INDEX), and when
 * a table's distribution key is about to be changed (ALTER TABLE SET
 * DISTRIBUTED BY).
 *
 * The set of columns being indexed needs to be a superset of the distribution
 * policy. If the table is distributed randomly, no unique / exclusion
 * indexing is supported. If the table is replicated, all constraints all
 * supported.
 *
 * The index is described by 'indattr', 'indclasses', 'exclop', 'nidxatts'
 * parameters. Note that the parameters don't include expressions for an
 * expression index; expressions can never match distribution keys, so
 * they can be ignored here.
 *
 * Returns 'true', if the policy is compatible with the index.
 */
bool
index_check_policy_compatible(GpPolicy *policy,
							  TupleDesc desc,
							  AttrNumber *indattr,
							  Oid *indclasses,
							  Oid *exclop,
							  int nidxatts,
							  bool report_error,
							  index_check_policy_compatible_context *error_context)
{
	int			i;
	int			j;

	/*
	 * POLICYTYPE_ENTRY normally means it's a system table or a table created
	 * in utility mode, so unique/primary key is allowed anywhere.
	 */
	if (GpPolicyIsEntry(policy))
		return true;

	/*
	 * Firstly, unique indexes / exclusion constraints are not supported at
	 * all on randomly distributed tables.
	 *
	 * XXX: The error message here is worded as if we're adding a constraint. This
	 * function is also used for ALTER TABLE SET DISTRIBUTED BY, but as of this
	 * writing, with ALTER TABLE SET DISTRIBUTED BY the caller checks for these
	 * cases before calling this function, with a different error message. That
	 * seems redundant, but as long as the caller does that, we can amke that
	 * assumption in the error message.
	 */
	if (GpPolicyIsRandomPartitioned(policy))
	{
		if (report_error)
		{
			if (error_context->is_primarykey)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("PRIMARY KEY and DISTRIBUTED RANDOMLY are incompatible")));
			else if (exclop)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("exclusion constraint and DISTRIBUTED RANDOMLY are incompatible")));
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("UNIQUE and DISTRIBUTED RANDOMLY are incompatible")));
		}
		return false;
	}

	/*
	 * On the other hand, everything is supported on replicated tables.
	 */
	if (GpPolicyIsReplicated(policy))
		return true;

	/*
	 * Otherwise, it's hash distributed.
	 *
	 * Loop through each distribution key column, and check that it's part
	 * of the index.
	 */
	Assert(GpPolicyIsHashPartitioned(policy));
	Assert(policy->nattrs > 0);
	for (i = 0; i < policy->nattrs; i++)
	{
		int			policy_attr;
		Oid			policy_opclass;
		Oid			policy_opfamily;
		Oid			policy_typeid;
		Oid			policy_eqop;
		bool		found;
		bool		found_col;
		Oid			found_col_indclass;

		/* Look up the equality operator for the distribution key opclass */
		policy_attr = policy->attrs[i];
		policy_typeid = desc->attrs[policy_attr - 1]->atttypid;
		policy_opclass = policy->opclasses[i];
		policy_opfamily = get_opclass_family(policy_opclass);
		policy_eqop = cdb_eqop_in_hash_opfamily(policy_opfamily, policy_typeid);

		/*
		 * Scan the index columns to see if any of them match the distribution
		 * key.
		 */
		found = false;
		found_col = false;
		found_col_indclass = InvalidOid;
		for (j = 0; j < nidxatts; j++)
		{
			Oid			indopfamily;
			Oid			opcintype;
			Oid			indeqop;

			if (indattr[j] != policy_attr)
				continue;
			found_col = true;

			/*
			 * Is the index's operator class is compatible with the
			 * distribution key's operator class? It's compatible, if it
			 * has the same equality operator.
			 *
			 * If this is an exclusion constraint, rather than a unique index,
			 * then we compare the exclusion operator instead. The exclusion
			 * operator should be the same operator as the distribution key
			 * opclass's equality operator.
			 */
			if (exclop)
				indeqop = exclop[j];
			else
			{
				found_col_indclass = indclasses[j];

				indopfamily = get_opclass_family(indclasses[j]);
				opcintype = get_opclass_input_type(indclasses[j]);
				indeqop = get_opfamily_member(indopfamily,
											  opcintype,
											  opcintype,
											  BTEqualStrategyNumber);
			}
			if (indeqop == policy_eqop)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			/*
			 * If the caller asked for an ERROR, construct a suitable error
			 * message. The details of the message depend on the kind of
			 * constraint it is, and whether the distribution key was missing
			 * from the constraint altogther, or if it just had different
			 * opclass.
			 */
			if (report_error)
				ereport(ERROR,
						(errdetails_index_policy(NameStr(TupleDescAttr(desc, policy_attr - 1)->attname),
												 policy_opclass,
												 policy_eqop,
												 found_col_indclass,
												 exclop,
												 error_context)));
			return false;
		}
	}

	return true;
}


/*
 * Print the name of an operator class, for error messages.
 */
static const char *
format_opclass(Oid opclass)
{
	HeapTuple	ht_opc;
	Form_pg_opclass opcrec;
	char	   *opcname;
	char	   *nspname;
	const char *result;

	ht_opc = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
	if (!HeapTupleIsValid(ht_opc))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);
	opcrec = (Form_pg_opclass) GETSTRUCT(ht_opc);

	/* do we need to schema-qualify the name? */
	opcname = NameStr(opcrec->opcname);
	if (OpclassIsVisible(opclass))
		result = quote_identifier(opcname);
	else
	{
		nspname = get_namespace_name(opcrec->opcnamespace);
		result = psprintf("%s.%s",
						  quote_identifier(nspname),
						  quote_identifier(opcname));
	}
	ReleaseSysCache(ht_opc);

	return result;
}

static int
errdetails_index_policy(char *attname,
						Oid policy_opclass,
						Oid policy_eqop,
						Oid found_indclass,
						Oid *exclop,
						index_check_policy_compatible_context *context)
{
	errcode(ERRCODE_INVALID_TABLE_DEFINITION);

	/*
	 * Print main message. The wording depends on whether we're ALTERing a
	 * table's distribution key, and the new distribution key isn't compatible
	 * with existing indexes, or if we're trying to build a new index that's
	 * not compatible with the table's distribution key. Both variants contain
	 * the same information, but we try to say that the thing we're changing
	 * is not compatible with the existing stuff, rather than the other way
	 * round.
	 *
	 * In the ALTER TABLE SET DISTRIBUTED BY, we print the name of the conflicting
	 * constraint/index. When we're adding an index, we leave that out, because
	 * that's right there in the CREATE INDEX or ALTER TABLE ADD CONSTRAINT command.
	 *
	 * XXX: If it's a CREATE TABLE with multiple UNIQUE constraints, it would be
	 * to printout which UNIQUE constraint is causing trouble. But we can't
	 * distinguish CREATE TABLE subcommands from a straight CREATE INDEX here.
	 */
	if (context->for_alter_dist_policy)
	{
		if (context->is_primarykey)
			errmsg("distribution policy is not compatible with the table's PRIMARY KEY");
		else if (exclop)
			errmsg("distribution policy is not compatible with exclusion constraint \"%s\"",
				   context->constraint_name);
		else
		{
			Assert (context->is_unique);
			if (context->is_constraint)
				errmsg("distribution policy is not compatible with UNIQUE constraint \"%s\"",
					   context->constraint_name);
			else
				errmsg("distribution policy is not compatible with UNIQUE index \"%s\"",
					   context->constraint_name);
		}
	}
	else
	{
		if (context->is_primarykey)
			errmsg("PRIMARY KEY definition must contain all columns in the table's distribution key");
		else if (exclop)
			errmsg("exclusion constraint is not compatible with the table's distribution policy");
		else
		{
			Assert (context->is_unique);
			if (context->is_constraint)
				errmsg("UNIQUE constraint must contain all columns in the table's distribution key");
			else
				errmsg("UNIQUE index must contain all columns in the table's distribution key");
		}
	}

	/*
	 * Print details of which distribution column is causing the trouble.
	 */
	if (exclop)
	{
		errdetail("Distribution key column \"%s\" is not included in the constraint.",
				  attname);
		errhint("Add \"%s\" to the constraint with the %s operator.",
				attname, format_operator(policy_eqop));
	}
	else if (found_indclass != InvalidOid)
	{
		/*
		 * A unique index contained the distribution key column, but with
		 * an incompatible opclass.
		 *
		 * It would be nice to hint what a compatible operator class be.
		 * But it'd take some effort to dig that from the catalogs.
		 */
		errdetail("Operator class %s of distribution key column \"%s\" is not compatible with operator class %s used in the constraint.",
				  format_opclass(policy_opclass),
				  attname,
				  format_opclass(found_indclass));
	}
	else
	{
		errdetail("Distribution key column \"%s\" is not included in the constraint.",
				  attname);
	}

	return 0;
}

/*
 * If the proposed index does not match the distribution policy but the relation
 * is empty and does not have a primary key or unique index, update the
 * distribution policy to match the index definition (MPP-101).
 */
bool
change_policy_to_match_index(Relation rel,
							 AttrNumber *indattr,
							 Oid *indclasses,
							 Oid *exclop,
							 int nidxatts)
{
	TupleDesc	desc = RelationGetDescr(rel);
	GpPolicy *policy;
	int			i;
	MemoryContext oldcontext;

	/*
	 * Don't do anything with exclusion constraints, for now. We could
	 * do the analogous thing we do with unique indexes, if any of the
	 * exclusion operators have a compatible hash opclass. But we don't
	 * bother.
	 */
	if (exclop)
		return false;

	policy = makeGpPolicy(POLICYTYPE_PARTITIONED, nidxatts,
						  rel->rd_cdbpolicy->numsegments);
	for (i = 0; i < nidxatts; i++)
	{
		AttrNumber	attno = indattr[i];
		Oid			typeid = desc->attrs[attno - 1]->atttypid;
		Oid			policy_opclass;
		Oid			policy_opfamily;
		Oid			policy_eqop;
		Oid			indopfamily;
		Oid			indeqop;

		policy_opclass = cdb_default_distribution_opclass_for_type(typeid);
		if (!policy_opclass)
		{
			/*
			 * The datatype has no default opclass. Can't use it in the
			 * distribution key.
			 */
			return false;
		}

		policy_opfamily = get_opclass_family(policy_opclass);
		policy_eqop = get_opfamily_member(policy_opfamily,
										  typeid,
										  typeid,
										  HTEqualStrategyNumber);

		indopfamily = get_opclass_family(indclasses[i]);
		indeqop = get_opfamily_member(indopfamily,
									  typeid,
									  typeid,
									  BTEqualStrategyNumber);
		if (policy_eqop != indeqop)
		{
			/*
			 * The default hash opclass isn't compatible with the index opclass.
			 * That is, they use a different equality operator. Give up.
			 *
			 * We could perhaps work a bit harder, and search for a different
			 * hash opclass that would be compatible. But doesn't seem worth
			 * the trouble.
			 */
			return false;
		}

		policy->attrs[i] = attno;
		policy->opclasses[i] = policy_opclass;
	}

	GpPolicyReplace(rel->rd_id, policy);

	oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));
	rel->rd_cdbpolicy = GpPolicyCopy(policy);
	MemoryContextSwitchTo(oldcontext);

	return true;
}
