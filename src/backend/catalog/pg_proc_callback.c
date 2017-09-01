/*-------------------------------------------------------------------------
 *
 * pg_proc_callback.c
 *	  
 *   Auxillary extension to pg_proc to enable defining additional callback
 *   functions.  Currently the list of allowable callback functions is small
 *   and consists of:
 *     - DESCRIBE() - to support more complex pseudotype resolution
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/pg_proc_callback.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/indexing.h"
#include "catalog/pg_proc_callback.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/tqual.h"

/* ---------------------
 * deleteProcCallbacks() - Remove callbacks from pg_proc_callback
 *
 * Parameters:
 *    profnoid - remove all callbacks for this function oid
 *
 * Notes:
 *    This function does not maintain dependencies in pg_depend, that behavior
 *    is currently controlled in pg_proc.c
 * ---------------------
 */
void 
deleteProcCallbacks(Oid profnoid)
{
	Relation	rel;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;

	Insist(OidIsValid(profnoid));

	/*
	 * This is equivalent to:
	 *
	 * DELETE FROM pg_proc_callback WHERE profnoid = :1
	 */
	rel = heap_open(ProcCallbackRelationId, RowExclusiveLock);

	ScanKeyInit(&skey,
				Anum_pg_proc_callback_profnoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(profnoid));
	scan = systable_beginscan(rel, ProcCallbackProfnoidPromethodIndexId, true,
							  SnapshotNow, 1, &skey);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
		simple_heap_delete(rel, &tup->t_self);

	systable_endscan(scan);

	heap_close(rel, RowExclusiveLock);
}


/* ---------------------
 * addProcCallback() - Add a new callback to pg_proc_callback
 *
 * Parameters:
 *    profnoid    - oid of the function that has a callback
 *    procallback - oid of the callback function
 *    promethod   - role the callback function plays
 *
 * Notes:
 *    This function does not maintain dependencies in pg_depend, that behavior
 *    is currently controlled in pg_proc.c
 * ---------------------
 */
void 
addProcCallback(Oid profnoid, Oid procallback, char promethod)
{
	Relation	rel;
	bool		nulls[Natts_pg_proc_callback];
	Datum		values[Natts_pg_proc_callback];
	HeapTuple   tup;
	
	Insist(OidIsValid(profnoid));
	Insist(OidIsValid(procallback));

	/* open pg_proc_callback */
	rel = heap_open(ProcCallbackRelationId, RowExclusiveLock);

	/* Build the tuple and insert it */
	nulls[Anum_pg_proc_callback_profnoid - 1]	  = false;
	nulls[Anum_pg_proc_callback_procallback - 1]  = false;
	nulls[Anum_pg_proc_callback_promethod - 1]	  = false;
	values[Anum_pg_proc_callback_profnoid - 1]	  = ObjectIdGetDatum(profnoid);
	values[Anum_pg_proc_callback_procallback - 1] = ObjectIdGetDatum(procallback);
	values[Anum_pg_proc_callback_promethod - 1]	  = CharGetDatum(promethod);

	tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);
	
	/* Insert tuple into the relation */
	simple_heap_insert(rel, tup);
	CatalogUpdateIndexes(rel, tup);

	heap_close(rel, RowExclusiveLock);
}


/* ---------------------
 * lookupProcCallback() - Find a specified callback for a specified function
 *
 * Parameters:
 *    profnoid    - oid of the function that has a callback
 *    promethod   - which callback to find
 * ---------------------
 */
Oid  
lookupProcCallback(Oid profnoid, char promethod)
{
	Relation	rel;
	ScanKeyData skey[2];
	SysScanDesc scan;
	HeapTuple	tup;
	Oid         result;

	Insist(OidIsValid(profnoid));

	/* open pg_proc_callback */
	rel = heap_open(ProcCallbackRelationId, AccessShareLock);

	/* Lookup (profnoid, promethod) from index */
	/* (profnoid, promethod) is guaranteed unique by the index */
	ScanKeyInit(&skey[0],
				Anum_pg_proc_callback_profnoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(profnoid));
	ScanKeyInit(&skey[1],
				Anum_pg_proc_callback_promethod,
				BTEqualStrategyNumber, F_CHAREQ,
				CharGetDatum(promethod));
	scan = systable_beginscan(rel, ProcCallbackProfnoidPromethodIndexId, true,
							  SnapshotNow, 2, skey);
	tup = systable_getnext(scan);
	if (HeapTupleIsValid(tup))
	{
		Datum		d;
		bool		isnull;

		d = heap_getattr(tup, Anum_pg_proc_callback_procallback,
						 RelationGetDescr(rel), &isnull);
		Assert(!isnull);

		result = DatumGetObjectId(d);
	}
	else
		result = InvalidOid;

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	return result;
}
