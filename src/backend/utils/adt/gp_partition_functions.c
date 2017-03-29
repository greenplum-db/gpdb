/*
 * gp_partition_functions.c
 *
 * Copyright(c) 2012 - present, EMC/Greenplum
 */

#include "postgres.h"

#include "access/heapam.h"
#include "cdb/cdbpartition.h"
#include "funcapi.h"
#include "nodes/execnodes.h"
#include "utils/array.h"
#include "utils/hsearch.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/elog.h"
#include "utils/guc.h"

/*
 * increaseScanArraySize
 *   Increase the array size for dynamic table scans.
 *
 * The final array size is the maximum of the following two values:
 *   (1) (dynamicTableScanInfo->numScans + NUM_PID_INDEXES_ADDED)
 *   (2) newMaxPartIndex + 1.
 */
static void
increaseScanArraySize(EState *estate, int newMaxPartIndex)
{
	DynamicTableScanInfo *dynamicTableScanInfo = estate->dynamicTableScanInfo;
	int oldNumScans = dynamicTableScanInfo->numScans;
	int newNumScans = oldNumScans + NUM_PID_INDEXES_ADDED;
	if (newNumScans < newMaxPartIndex)
	{
		newNumScans = newMaxPartIndex;
	}

	dynamicTableScanInfo->numScans = newNumScans;

	if (dynamicTableScanInfo->pidIndexes == NULL)
	{
		dynamicTableScanInfo->pidIndexes = (HTAB **)
			palloc0(dynamicTableScanInfo->numScans * sizeof(HTAB*));

		Assert(dynamicTableScanInfo->iterators == NULL);
		dynamicTableScanInfo->iterators = palloc0(dynamicTableScanInfo->numScans * sizeof(DynamicPartitionIterator*));
	}
	else
	{
		dynamicTableScanInfo->pidIndexes = (HTAB **)
			repalloc(dynamicTableScanInfo->pidIndexes,
					 dynamicTableScanInfo->numScans * sizeof(HTAB*));

		dynamicTableScanInfo->iterators = repalloc(dynamicTableScanInfo->iterators,
				dynamicTableScanInfo->numScans * sizeof(DynamicPartitionIterator*));

		for (int scanNo = oldNumScans; scanNo < dynamicTableScanInfo->numScans; scanNo++)
		{
			dynamicTableScanInfo->pidIndexes[scanNo] = NULL;
			dynamicTableScanInfo->iterators[scanNo] = NULL;
		}
	}
}

/*
 * createPidIndex
 *   Create the pid index for a given dynamic table scan.
 */
static HTAB *
createPidIndex(EState *estate, int index)
{
	Assert((estate->dynamicTableScanInfo->pidIndexes)[index - 1] == NULL);

	HASHCTL hashCtl;
	MemSet(&hashCtl, 0, sizeof(HASHCTL));
	hashCtl.keysize = sizeof(Oid);
	hashCtl.entrysize = sizeof(PartOidEntry);
	hashCtl.hash = oid_hash;
	hashCtl.hcxt = estate->es_query_cxt;

	return hash_create("Dynamic Table Scan Pid Index",
					   INITIAL_NUM_PIDS,
					   &hashCtl,
					   HASH_ELEM | HASH_CONTEXT | HASH_FUNCTION);
}

/*
 * InsertPidIntoDynamicTableScanInfo
 * 		Inserts a partition oid into the dynamicTableScanInfo's
 * 		pidIndexes at the provided index. If partOid is an invalid
 * 		oid, it doesn't insert that, but ensures that the dynahash
 * 		exists at the index position in dynamicTableScanInfo.
 */
void
InsertPidIntoDynamicTableScanInfo(EState *estate, int32 index, Oid partOid, int32 selectorId)
{
	DynamicTableScanInfo *dynamicTableScanInfo = estate->dynamicTableScanInfo;

	Assert(dynamicTableScanInfo != NULL);

	/* It's 1 based indexing */
	Assert(index > 0);

	MemoryContext oldCxt = MemoryContextSwitchTo(estate->es_query_cxt);

	if (index > dynamicTableScanInfo->numScans)
	{
		increaseScanArraySize(estate, index);
	}
	
	Assert(index <= dynamicTableScanInfo->numScans);
	if ((dynamicTableScanInfo->pidIndexes)[index - 1] == NULL)
	{
		dynamicTableScanInfo->pidIndexes[index - 1] = createPidIndex(estate, index);
	}

	Assert(dynamicTableScanInfo->pidIndexes[index - 1] != NULL);
	
	if (partOid != InvalidOid)
	{
		bool found = false;
		PartOidEntry *hashEntry =
			hash_search(dynamicTableScanInfo->pidIndexes[index - 1],
						&partOid, HASH_ENTER, &found);

		if (found)
		{
			Assert(hashEntry->partOid == partOid);
			Assert(NIL != hashEntry->selectorList);
			hashEntry->selectorList = list_append_unique_int(hashEntry->selectorList, selectorId);
		}
		else
		{
			hashEntry->partOid = partOid;
			hashEntry->selectorList = list_make1_int(selectorId);
		}
	}

	MemoryContextSwitchTo(oldCxt);
}

/*
 * dumpDynamicTableScanPidIndex
 *   Write out pids for a given dynamic table scan.
 */
void
dumpDynamicTableScanPidIndex(EState *estate, int index)
{
	DynamicTableScanInfo *dynamicTableScanInfo = estate->dynamicTableScanInfo;

	if (index < 0 ||
		dynamicTableScanInfo == NULL ||
		index > dynamicTableScanInfo->numScans ||
		dynamicTableScanInfo->pidIndexes[index] == NULL)
	{
		return;
	}
	
	Assert(dynamicTableScanInfo != NULL &&
		   index < dynamicTableScanInfo->numScans &&
		   dynamicTableScanInfo->pidIndexes[index] != NULL);
	
	HASH_SEQ_STATUS status;
	hash_seq_init(&status, dynamicTableScanInfo->pidIndexes[index]);

	StringInfoData pids;
	initStringInfo(&pids);

	Oid *partOid = NULL;
	while ((partOid = (Oid *)hash_seq_search(&status)) != NULL)
	{
		appendStringInfo(&pids, "%d ", *partOid);
	}

	elog(LOG, "Dynamic Table Scan %d pids: %s", index, pids.data);
	pfree(pids.data);
}

bool
isPartitionSelected(EState *estate, int index, Oid partOid)
{
	DynamicTableScanInfo *dynamicTableScanInfo = estate->dynamicTableScanInfo;
	bool		found = false;

	Assert(dynamicTableScanInfo != NULL);

	/* It's 1 based indexing */
	Assert(index > 0);

	if (index > dynamicTableScanInfo->numScans)
		elog(ERROR, "cannot execute PartSelectedExpr before PartitionSelector");

	if ((dynamicTableScanInfo->pidIndexes)[index - 1] == NULL)
	{
		dynamicTableScanInfo->pidIndexes[index - 1] = createPidIndex(estate, index);
	}

	Assert(dynamicTableScanInfo->pidIndexes[index - 1] != NULL);
	
	(void) hash_search(dynamicTableScanInfo->pidIndexes[index - 1],
					   &partOid, HASH_FIND, &found);

	return found;
}
