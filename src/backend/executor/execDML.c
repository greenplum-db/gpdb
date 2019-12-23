/*-------------------------------------------------------------------------
 *
 * execDML.c
 *        Implementation of execDML.
 *        This file performs INSERT, DELETE and UPDATE DML operations.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execDML.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_attribute.h"
#include "executor/executor.h"
#include "executor/execDML.h"

/*
 * Are two TupleDescs physically compatible?
 *
 * They are assumed to be "logically" the same. That means that columns
 * are in the same order, and there might be dropped columns.
 */
static bool
physicalEqualTupleDescs(TupleDesc tupdesc1, TupleDesc tupdesc2)
{
	if (tupdesc1->natts != tupdesc2->natts)
		return false;
	if (tupdesc1->tdhasoid != tupdesc2->tdhasoid)
		return false;

	for (int i = 0; i < tupdesc1->natts; i++)
	{
		Form_pg_attribute attr1 = tupdesc1->attrs[i];
		Form_pg_attribute attr2 = tupdesc2->attrs[i];

		if (attr1->attisdropped != attr2->attisdropped)
			return false;

		/* ignore dropped columns */
		if (attr1->attisdropped)
			continue;

		if (attr1->atttypid != attr2->atttypid)
			return false;

		/* these should all be equal, if the type OID is equal */
		Assert(attr1->attlen == attr2->attlen);
		Assert(attr1->attbyval == attr2->attbyval);
		Assert(attr1->attalign == attr2->attalign);
	}

	return true;
}

/*
 * Given a tuple in the partitioned table's parent physical format, map it to
 * the physical format of a particular partition.
 *
 * parentSlot -- slot containing the input tuple
 * childInfo  -- info pertaining to the target partition
 *
 * If no restructuring is required, 'parentSlot' is returned unmodified.
 * Otherwise, the restructured tuple is stored in childInfo->ri_resultSlot.
 */
TupleTableSlot *
reconstructPartitionTupleSlot(TupleTableSlot *parentSlot,
							  ResultRelInfo *childInfo)
{
	TupleConversionMap *map = childInfo->ri_partInsertMap;
	TupleDesc	childDesc = RelationGetDescr(childInfo->ri_RelationDesc);

	/* No map and matching tuple descriptor means no restructuring needed. */
	if (map == NULL && physicalEqualTupleDescs(parentSlot->tts_tupleDescriptor,
											   childDesc))
		return parentSlot;

	/*
	 * Mapping is required. Get the target slot ready.
	 */
	if (!childInfo->ri_resultSlot)
		childInfo->ri_resultSlot = MakeSingleTupleTableSlot(childDesc);

	if (map)
		execute_attr_map_slot(map->attrMap, parentSlot, childInfo->ri_resultSlot);
	else
	{
		/*
		 * No mapping required, but there might be differences in the physical
		 * tuple, e.g because dropped columns have differing datatypes.
		 */
		Datum	   *invalues;
		bool	   *inisnull;
		TupleTableSlot *outSlot = childInfo->ri_resultSlot;
		Datum	   *outvalues;
		bool	   *outisnull;
		int			natts;

		/* Sanity checks */
		Assert(parentSlot->tts_tupleDescriptor != NULL &&
			   outSlot->tts_tupleDescriptor != NULL);
		Assert(parentSlot->PRIVATE_tts_values != NULL && outSlot->PRIVATE_tts_values != NULL);

		natts = parentSlot->tts_tupleDescriptor->natts;

		/* Extract all the values of the in slot. */
		slot_getallattrs(parentSlot);

		/* Before doing the mapping, clear any old contents from the out slot */
		ExecClearTuple(outSlot);

		invalues = slot_get_values(parentSlot);
		inisnull = slot_get_isnull(parentSlot);
		outvalues = slot_get_values(outSlot);
		outisnull = slot_get_isnull(outSlot);

		/* Copy the values. */
		for (int i = 0; i < natts; i++)
		{
			outvalues[i] = invalues[i];
			outisnull[i] = inisnull[i];
		}

		ExecStoreVirtualTuple(outSlot);

	}

	return childInfo->ri_resultSlot;
}
