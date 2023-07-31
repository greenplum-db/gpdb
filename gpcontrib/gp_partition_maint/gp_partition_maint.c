#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "catalog/partition.h"
#include "fmgr.h"
#include "nodes/parsenodes.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static bool
rel_is_range_part_nondefault(Oid relid)
{
	HeapTuple			tuple;
	Form_pg_class		classForm;
	Datum				datum;
	bool				isnull;
	PartitionBoundSpec *boundspec = NULL;
	bool				retval = false;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);

	/* check if the relation is a partition */
	classForm = (Form_pg_class) GETSTRUCT(tuple);
	if ((classForm->relkind == RELKIND_RELATION ||
		classForm->relkind == RELKIND_PARTITIONED_TABLE) &&
		classForm->relispartition)
	{
		datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relpartbound, &isnull);
		if (!isnull)
		{
			boundspec = stringToNode(TextDatumGetCString(datum));
			/* check if the partition is a non-default range partition */
			if (boundspec->strategy == PARTITION_STRATEGY_RANGE &&
				!boundspec->is_default)
				retval = true;
		}
	}

	ReleaseSysCache(tuple);
	return retval;
}

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum pg_partition_rank(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pg_partition_rank);

Datum
pg_partition_rank(PG_FUNCTION_ARGS)
{
	Oid					relid = PG_GETARG_OID(0);
	Oid					parentrelid = InvalidOid;
	Relation			parentrel = NULL;
	PartitionDesc		parentpartdesc = NULL;
	PartitionKey		parentpartkey = NULL;

	if (!rel_is_range_part_nondefault(relid))
		PG_RETURN_NULL();

	parentrelid = get_partition_parent(relid);
	parentrel = relation_open(parentrelid, AccessShareLock);
	parentpartkey = RelationGetPartitionKey(parentrel);
	parentpartdesc = RelationGetPartitionDesc(parentrel);

	/*
	 * Child oids are already sorted by range bounds in ascending order.
	 * If default partition exists, it is the last partition.
	 */
	Assert(parentpartkey->strategy == PARTITION_STRATEGY_RANGE);
	for (int i = 0; i < parentpartdesc->nparts; i++)
	{
		if (relid == parentpartdesc->oids[i])
		{
			relation_close(parentrel, AccessShareLock);
			PG_RETURN_INT32(i + 1);
		}
	}
	elog(ERROR, "partition not found in parent");
}
