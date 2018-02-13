/*
 * gp_fault_strategy.c
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 */

#include "postgres.h"

#include "catalog/gp_fault_strategy.h"
#include "access/genam.h"
#include "access/heapam.h"

/*
 * Get the gp_fault_strategy entry
 */
char
get_gp_fault_strategy(void)
{
	Relation rel;
	HeapTuple tuple;
	SysScanDesc sscan;
	Datum strategy_datum;
	char strategy;

	rel = heap_open(GpFaultStrategyRelationId, AccessShareLock);

	/* SELECT * FROM gp_fault_strategy */
	sscan = systable_beginscan(rel, InvalidOid, false, SnapshotNow, 0, NULL);

	/* there should only be one row in table */
	tuple = systable_getnext(sscan);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR, (errmsg("could not read gp_fault_strategy")));

	strategy_datum = heap_getattr(tuple, Anum_gp_fault_strategy_fault_strategy, RelationGetDescr(rel), NULL);
	strategy = DatumGetChar(strategy_datum);

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);
	Assert(strategy == GpFaultStrategyMirrorLess || strategy == GpFaultStrategyFileRepMirrored);

	return strategy;
}
