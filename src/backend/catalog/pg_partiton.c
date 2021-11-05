
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/tupconvert.h"
#include "access/sysattr.h"
#include "catalog/indexing.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_inherits.h"
#include "nodes/makefuncs.h"
#include "rewrite/rewriteManip.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "heapam.h"


static Oid	get_partition_parent_worker(Relation inhRel, Oid relid);

/*
 * get_partition_parent
 *		Obtain direct parent of given relation
 *
 * Returns inheritance parent of a partition by scanning pg_inherits
 *
 * Note: Because this function assumes that the relation whose OID is passed
 * as an argument will have precisely one parent, it should only be called
 * when it is known that the relation is a partition.
 */
Oid
get_partition_parent(Oid relid)
{
	Relation	catalogRelation;
	Oid			result;

	catalogRelation = relation_open(InheritsRelationId, AccessShareLock);

	result = get_partition_parent_worker(catalogRelation, relid);

	if (!OidIsValid(result))
		elog(ERROR, "could not find tuple for parent of relation %u", relid);

	relation_close(catalogRelation, AccessShareLock);

	return result;
}


/*
 * get_partition_parent_worker
 *		Scan the pg_inherits relation to return the OID of the parent of the
 *		given relation
 */
static Oid
get_partition_parent_worker(Relation inhRel, Oid relid)
{
	SysScanDesc scan;
	ScanKeyData key[2];
	Oid			result = InvalidOid;
	HeapTuple	tuple;

	ScanKeyInit(&key[0],
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&key[1],
				Anum_pg_inherits_inhseqno,
				BTEqualStrategyNumber, F_INT4EQ,
				Int32GetDatum(1));

	scan = systable_beginscan(inhRel, InheritsRelidSeqnoIndexId, true,
							  NULL, 2, key);
	tuple = systable_getnext(scan);
	if (HeapTupleIsValid(tuple))
	{
		Form_pg_inherits form = (Form_pg_inherits) GETSTRUCT(tuple);

		result = form->inhparent;
	}

	systable_endscan(scan);

	return result;
}
