/*-------------------------------------------------------------------------
 *
 * execDML.c
 *        Implementation of execDML.
 *        This file performs INSERT, DELETE and UPDATE DML operations.
 *
 * Copyright (c) 2012, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/fileam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbpartition.h"
#include "commands/trigger.h"
#include "executor/execdebug.h"
#include "executor/execDML.h"
#include "utils/lsyscache.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h"
#include "storage/large_object.h"

/*
 * reconstructTupleValues
 *   Re-construct tuple values based on the pre-defined mapping.
 */
void
reconstructTupleValues(AttrMap *map,
					Datum *oldValues, bool *oldIsnull, int oldNumAttrs,
					Datum *newValues, bool *newIsnull, int newNumAttrs)
{
	for (int oldAttNo = 1; oldAttNo <= oldNumAttrs; oldAttNo++)
	{
		int newAttNo = attrMap(map, oldAttNo);
		if (newAttNo == 0)
		{
			continue;
		}

		Assert(newAttNo <= newNumAttrs);
		newValues[newAttNo - 1] = oldValues[oldAttNo - 1];
		newIsnull[newAttNo - 1] = oldIsnull[oldAttNo - 1];
	}
}

/*
 * Use the supplied ResultRelInfo to create an appropriately restructured
 * version of the tuple in the supplied slot, if necessary.
 *
 * slot -- slot containing the input tuple
 * resultRelInfo -- info pertaining to the target part of an insert
 *
 * If no restructuring is required, the result is the argument slot, else
 * it is the slot from the argument result info updated to hold the
 * restructured tuple.
 */
TupleTableSlot *
reconstructMatchingTupleSlot(TupleTableSlot *slot, ResultRelInfo *resultRelInfo)
{
	int natts;
	Datum *values;
	bool *isnull;
	AttrMap *map;
	TupleTableSlot *partslot;
	Datum *partvalues;
	bool *partisnull;

	map = resultRelInfo->ri_partInsertMap;

	TupleDesc inputTupDesc = slot->tts_tupleDescriptor;
	TupleDesc resultTupDesc = resultRelInfo->ri_RelationDesc->rd_att;
	bool tupleDescMatch = (resultRelInfo->tupdesc_match == 1);
	if (resultRelInfo->tupdesc_match == 0)
	{
		tupleDescMatch = equalTupleDescs(inputTupDesc, resultTupDesc, false);

		if (tupleDescMatch)
		{
			resultRelInfo->tupdesc_match = 1;
		}
		else
		{
			resultRelInfo->tupdesc_match = -1;
		}
	}

	/* No map and matching tuple descriptor means no restructuring needed. */
	if (map == NULL && tupleDescMatch)
		return slot;

	/* Put the given tuple into attribute arrays. */
	natts = slot->tts_tupleDescriptor->natts;
	slot_getallattrs(slot);
	values = slot_get_values(slot);
	isnull = slot_get_isnull(slot);

	/*
	 * Get the target slot ready. If this is a child partition table,
	 * set target slot to ri_partSlot. Otherwise, use ri_resultSlot.
	 */
	if (map != NULL)
	{
		Assert(resultRelInfo->ri_partSlot != NULL);
		partslot = resultRelInfo->ri_partSlot;
	}
	else
	{
		if (resultRelInfo->ri_resultSlot == NULL)
		{
			resultRelInfo->ri_resultSlot = MakeSingleTupleTableSlot(resultTupDesc);
		}

		partslot = resultRelInfo->ri_resultSlot;
	}

	partslot = ExecStoreAllNullTuple(partslot);
	partvalues = slot_get_values(partslot);
	partisnull = slot_get_isnull(partslot);

	/* Restructure the input tuple.  Non-zero map entries are attribute
	 * numbers in the target tuple, however, not every attribute
	 * number of the input tuple need be present.  In particular,
	 * attribute numbers corresponding to dropped attributes will be
	 * missing.
	 */
	reconstructTupleValues(map, values, isnull, natts,
						partvalues, partisnull, partslot->tts_tupleDescriptor->natts);
	partslot = ExecStoreVirtualTuple(partslot);

	return partslot;
}

/*
 * Check if the tuple being updated will stay in the same part and throw ERROR
 * if not.  This check is especially necessary for default partition that has
 * no constraint on it.  partslot is the tuple being updated, and
 * resultRelInfo is the target relation of this update.  Call this only
 * estate has valid es_result_partitions.
 */
static void
checkPartitionUpdate(EState *estate, TupleTableSlot *partslot,
					 ResultRelInfo *resultRelInfo)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	AttrNumber	max_attr;
	Datum	   *values = NULL;
	bool	   *nulls = NULL;
	TupleDesc	tupdesc = NULL;
	Oid			parentRelid;
	Oid			targetid;

	Assert(estate->es_partition_state != NULL &&
		   estate->es_partition_state->accessMethods != NULL);
	if (!estate->es_partition_state->accessMethods->part_cxt)
		estate->es_partition_state->accessMethods->part_cxt =
			GetPerTupleExprContext(estate)->ecxt_per_tuple_memory;

	Assert(PointerIsValid(estate->es_result_partitions));

	/*
	 * As opposed to INSERT, resultRelation here is the same child part
	 * as scan origin.  However, the partition selection is done with the
	 * parent partition's attribute numbers, so if this result (child) part
	 * has physically-different attribute numbers due to dropped columns,
	 * we should map the child attribute numbers to the parent's attribute
	 * numbers to perform the partition selection.
	 * EState doesn't have the parent relation information at the moment,
	 * so we have to do a hard job here by opening it and compare the
	 * tuple descriptors.  If we find we need to map attribute numbers,
	 * max_partition_attr could also be bogus for this child part,
	 * so we end up materializing the whole columns using slot_getallattrs().
	 * The purpose of this code is just to prevent the tuple from
	 * incorrectly staying in default partition that has no constraint
	 * (parts with constraint will throw an error if the tuple is changing
	 * partition keys to out of part value anyway.)  It's a bit overkill
	 * to do this complicated logic just for this purpose, which is necessary
	 * with our current partitioning design, but I hope some day we can
	 * change this so that we disallow phyisically-different tuple descriptor
	 * across partition.
	 */
	parentRelid = estate->es_result_partitions->part->parrelid;

	/*
	 * I don't believe this is the case currently, but we check the parent relid
	 * in case the updating partition has changed since the last time we opened it.
	 */
	if (resultRelInfo->ri_PartitionParent &&
		parentRelid != RelationGetRelid(resultRelInfo->ri_PartitionParent))
	{
		resultRelInfo->ri_PartCheckTupDescMatch = 0;
		if (resultRelInfo->ri_PartCheckMap != NULL)
			pfree(resultRelInfo->ri_PartCheckMap);
		if (resultRelInfo->ri_PartitionParent)
			relation_close(resultRelInfo->ri_PartitionParent, AccessShareLock);
	}

	/*
	 * Check this at the first pass only to avoid repeated catalog access.
	 */
	if (resultRelInfo->ri_PartCheckTupDescMatch == 0 &&
		parentRelid != RelationGetRelid(resultRelInfo->ri_RelationDesc))
	{
		Relation	parentRel;
		TupleDesc	resultTupdesc, parentTupdesc;

		/*
		 * We are on a child part, let's see the tuple descriptor looks like
		 * the parent's one.  Probably this won't cause deadlock because
		 * DML should have opened the parent table with appropriate lock.
		 */
		parentRel = relation_open(parentRelid, AccessShareLock);
		resultTupdesc = RelationGetDescr(resultRelationDesc);
		parentTupdesc = RelationGetDescr(parentRel);
		if (!equalTupleDescs(resultTupdesc, parentTupdesc, false))
		{
			AttrMap		   *map;
			MemoryContext	oldcontext;

			/* Tuple looks different.  Construct attribute mapping. */
			oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
			map_part_attrs(resultRelationDesc, parentRel, &map, true);
			MemoryContextSwitchTo(oldcontext);

			/* And save it for later use. */
			resultRelInfo->ri_PartCheckMap = map;

			resultRelInfo->ri_PartCheckTupDescMatch = -1;
		}
		else
			resultRelInfo->ri_PartCheckTupDescMatch = 1;

		resultRelInfo->ri_PartitionParent = parentRel;
		/* parentRel will be closed as part of ResultRelInfo cleanup */
	}

	if (resultRelInfo->ri_PartCheckMap != NULL)
	{
		Datum	   *parent_values;
		bool	   *parent_nulls;
		Relation	parentRel = resultRelInfo->ri_PartitionParent;
		TupleDesc	parentTupdesc;
		AttrMap	   *map;

		Assert(parentRel != NULL);
		parentTupdesc = RelationGetDescr(parentRel);

		/*
		 * We need to map the attribute numbers to parent's one, to
		 * select the would-be destination relation, since all partition
		 * rules are based on the parent relation's tuple descriptor.
		 * max_partition_attr can be bogus as well, so don't use it.
		 */
		slot_getallattrs(partslot);
		values = slot_get_values(partslot);
		nulls = slot_get_isnull(partslot);
		parent_values = palloc(parentTupdesc->natts * sizeof(Datum));
		parent_nulls = palloc0(parentTupdesc->natts * sizeof(bool));

		map = resultRelInfo->ri_PartCheckMap;
		reconstructTupleValues(map, values, nulls, partslot->tts_tupleDescriptor->natts,
							   parent_values, parent_nulls, parentTupdesc->natts);

		/* Now we have values/nulls in parent's view. */
		values = parent_values;
		nulls = parent_nulls;
		tupdesc = RelationGetDescr(parentRel);
	}
	else
	{
		/*
		 * map == NULL means we can just fetch values/nulls from the
		 * current slot.
		 */
		Assert(nulls == NULL && tupdesc == NULL);
		max_attr = estate->es_partition_state->max_partition_attr;
		slot_getsomeattrs(partslot, max_attr);
		/* values/nulls pointing to partslot's array. */
		values = slot_get_values(partslot);
		nulls = slot_get_isnull(partslot);
		tupdesc = partslot->tts_tupleDescriptor;
	}

	/* And select the destination relation that this tuple would go to. */
	targetid = selectPartition(estate->es_result_partitions, values,
							   nulls, tupdesc,
							   estate->es_partition_state->accessMethods);

	/* Free up if we allocated mapped attributes. */
	if (values != slot_get_values(partslot))
	{
		Assert(nulls != slot_get_isnull(partslot));
		pfree(values);
		pfree(nulls);
	}

	if (!OidIsValid(targetid))
		ereport(ERROR,
				(errcode(ERRCODE_NO_PARTITION_FOR_PARTITIONING_KEY),
				 errmsg("no partition for partitioning key")));

	if (RelationGetRelid(resultRelationDesc) != targetid)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("moving tuple from partition \"%s\" to "
						"partition \"%s\" not supported",
						get_rel_name(RelationGetRelid(resultRelationDesc)),
						get_rel_name(targetid))));
}

/* ----------------------------------------------------------------
 *		ExecInsert
 *
 *		INSERTs have to add the tuple into
 *		the base relation and insert appropriate tuples into the
 *		index relations.
 *		Insert can be part of an update operation when
 *		there is a preceding SplitUpdate node. 
 * ----------------------------------------------------------------
 */
void
ExecInsert(TupleTableSlot *slot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
	   bool isUpdate,
	   bool isExecLatefunc)
{
	void		*tuple = NULL;
	ResultRelInfo *resultRelInfo = NULL;
	Relation	resultRelationDesc = NULL;
	Oid			newId = InvalidOid;
	TupleTableSlot *partslot = NULL;

	AOTupleId	aoTupleId = AOTUPLEID_INIT;

	bool		rel_is_heap = false;
	bool 		rel_is_aorows = false;
	bool		rel_is_aocols = false;
	bool		rel_is_external = false;

	if (isExecLatefunc)
	{
	  List *args_list = estate->bypassPreprocessFunctionArgs;

	  /* Specific hack for lo_create. Please modify for any new functions that use this machinery */
	  Assert(list_length(args_list) == 1);

	  if (estate->loMode == 1)
	  {
	    Oid arg_value = linitial_oid(args_list);

            /* Parser should have set list values as OID parameters for this func\
	       tion */
	    inv_create(arg_value);
	  }
	  else if (estate->loMode == 2)
	  {
	    int attnum;
	    int arg_value;
	    bool isnull;

	    attnum = -1;
	    arg_value = -1;
	    isnull = false;

	    attnum = linitial_int(args_list);

	    Assert(attnum != -1);

	    arg_value = DatumGetUInt32(slot_getattr(slot, attnum, &isnull));

	    Assert (isnull != true);
	  /* Parser should have set list values as Var attno for this function */
	  inv_create(arg_value);
	  }
	}

	/*
	 * get information on the (current) result relation
	 */
	if (estate->es_result_partitions)
	{
		resultRelInfo = slot_get_partition(slot, estate);

		/* Check whether the user provided the correct leaf part only if required */
		if (!dml_ignore_target_partition_check)
		{
			Assert(NULL != estate->es_result_partitions->part &&
					NULL != resultRelInfo->ri_RelationDesc);

			List *resultRelations = estate->es_plannedstmt->resultRelations;
			/*
			 * Only inheritance can generate multiple result relations and inheritance
			 * is not compatible with partitions. As we are in inserting in partitioned
			 * table, we should not have more than one resultRelation
			 */
			Assert(list_length(resultRelations) == 1);
			/* We only have one resultRelations entry where the user originally intended to insert */
			int rteIdxForUserRel = linitial_int(resultRelations);
			Assert (rteIdxForUserRel > 0);
			Oid userProvidedRel = InvalidOid;

			if (1 == rteIdxForUserRel)
			{
				/* Optimization for typical case */
				userProvidedRel = ((RangeTblEntry *) estate->es_plannedstmt->rtable->head->data.ptr_value)->relid;
			}
			else
			{
				userProvidedRel = getrelid(rteIdxForUserRel, estate->es_plannedstmt->rtable);
			}

			/* Error out if user provides a leaf partition that does not match with our calculated partition */
			if (userProvidedRel != estate->es_result_partitions->part->parrelid &&
				userProvidedRel != resultRelInfo->ri_RelationDesc->rd_id)
			{
				ereport(ERROR,
						(errcode(ERRCODE_CHECK_VIOLATION),
						 errmsg("Trying to insert row into wrong partition"),
						 errdetail("Expected partition: %s, provided partition: %s",
							resultRelInfo->ri_RelationDesc->rd_rel->relname.data,
							estate->es_result_relation_info->ri_RelationDesc->rd_rel->relname.data)));
			}
		}
		estate->es_result_relation_info = resultRelInfo;
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}

	Assert (!resultRelInfo->ri_projectReturning);

	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	rel_is_heap = RelationIsHeap(resultRelationDesc);
	rel_is_aocols = RelationIsAoCols(resultRelationDesc);
	rel_is_aorows = RelationIsAoRows(resultRelationDesc);
	rel_is_external = RelationIsExternal(resultRelationDesc);

	partslot = reconstructMatchingTupleSlot(slot, resultRelInfo);
	if (rel_is_heap)
	{
		tuple = ExecFetchSlotHeapTuple(partslot);
	}
	else if (rel_is_aorows)
	{
		tuple = ExecFetchSlotMemTuple(partslot, false);
	}
	else if (rel_is_external) 
	{
		if (estate->es_result_partitions && 
			estate->es_result_partitions->part->parrelid != 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("Insert into external partitions not supported.")));			
			return;
		}
		else
		{
			tuple = ExecFetchSlotHeapTuple(partslot);
		}
	}
	else
	{
		Assert(rel_is_aocols);
		tuple = ExecFetchSlotMemTuple(partslot, true);
	}

	Assert(partslot != NULL && tuple != NULL);

	/* Execute triggers in Planner-generated plans */
	if (planGen == PLANGEN_PLANNER)
	{
		/* BEFORE ROW INSERT Triggers */
		if (resultRelInfo->ri_TrigDesc &&
			resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_INSERT] > 0)
		{
			HeapTuple	newtuple;

			/* NYI */
			if(rel_is_aocols)
				elog(ERROR, "triggers are not supported on tables that use column-oriented storage");

			newtuple = ExecBRInsertTriggers(estate, resultRelInfo, tuple);

			if (newtuple == NULL)	/* "do nothing" */
			{
				return;
			}

			if (newtuple != tuple)	/* modified by Trigger(s) */
			{
				/*
				 * Put the modified tuple into a slot for convenience of routines
				 * below.  We assume the tuple was allocated in per-tuple memory
				 * context, and therefore will go away by itself. The tuple table
				 * slot should not try to clear it.
				 */
				TupleTableSlot *newslot = estate->es_trig_tuple_slot;

				if (newslot->tts_tupleDescriptor != partslot->tts_tupleDescriptor)
					ExecSetSlotDescriptor(newslot, partslot->tts_tupleDescriptor);
				ExecStoreGenericTuple(newtuple, newslot, false);
				newslot->tts_tableOid = partslot->tts_tableOid; /* for constraints */
				tuple = newtuple;
				partslot = newslot;
			}
		}
	}
	/*
	 * Check the constraints of the tuple
	 */
	if (resultRelationDesc->rd_att->constr &&
			planGen == PLANGEN_PLANNER)
	{
		ExecConstraints(resultRelInfo, partslot, estate);
	}
	/*
	 * insert the tuple
	 *
	 * Note: heap_insert returns the tid (location) of the new tuple in the
	 * t_self field.
	 *
	 * NOTE: for append-only relations we use the append-only access methods.
	 */
	if (rel_is_aorows)
	{
		if (resultRelInfo->ri_aoInsertDesc == NULL)
		{
			/* Set the pre-assigned fileseg number to insert into */
			ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);

			resultRelInfo->ri_aoInsertDesc =
				appendonly_insert_init(resultRelationDesc,
									   ActiveSnapshot,
									   resultRelInfo->ri_aosegno,
									   false);

		}

		appendonly_insert(resultRelInfo->ri_aoInsertDesc, tuple, &newId, &aoTupleId);
	}
	else if (rel_is_aocols)
	{
		if (resultRelInfo->ri_aocsInsertDesc == NULL)
		{
			ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
			resultRelInfo->ri_aocsInsertDesc = aocs_insert_init(resultRelationDesc, 
																resultRelInfo->ri_aosegno, false);
		}

		newId = aocs_insert(resultRelInfo->ri_aocsInsertDesc, partslot);
		aoTupleId = *((AOTupleId*)slot_get_ctid(partslot));
	}
	else if (rel_is_external)
	{
		/* Writable external table */
		if (resultRelInfo->ri_extInsertDesc == NULL)
			resultRelInfo->ri_extInsertDesc = external_insert_init(resultRelationDesc);

		newId = external_insert(resultRelInfo->ri_extInsertDesc, tuple);
	}
	else
	{
		Insist(rel_is_heap);

		newId = heap_insert(resultRelationDesc,
							tuple,
							estate->es_snapshot->curcid,
							true, true, GetCurrentTransactionId());
	}

	IncrAppended();
	(estate->es_processed)++;
	(resultRelInfo->ri_aoprocessed)++;
	estate->es_lastoid = newId;

	partslot->tts_tableOid = RelationGetRelid(resultRelationDesc);

	if (rel_is_aorows || rel_is_aocols)
	{
		/*
		 * insert index entries for AO Row-Store tuple
		 */
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(partslot, (ItemPointer)&aoTupleId, estate, false);
	}
	else
	{
		/* Use parttuple for index update in case this is an indexed heap table. */
		TupleTableSlot *xslot = partslot;
		void *xtuple = tuple;

		setLastTid(&(((HeapTuple) xtuple)->t_self));

		/*
		 * insert index entries for tuple
		 */
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(xslot, &(((HeapTuple) xtuple)->t_self), estate, false);

	}

	if (planGen == PLANGEN_PLANNER)
	{
		/* AFTER ROW INSERT Triggers */
		ExecARInsertTriggers(estate, resultRelInfo, tuple);
	}
}

/* ----------------------------------------------------------------
 *		ExecDelete
 *
 *		DELETE is like UPDATE, except that we delete the tuple and no
 *		index modifications are needed.
 *		DELETE can be part of an update operation when
 *		there is a preceding SplitUpdate node. 
 *
 * ----------------------------------------------------------------
 */
void
ExecDelete(ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate)
{
	ResultRelInfo *resultRelInfo;
	Relation resultRelationDesc;
	HTSU_Result result;
	ItemPointerData update_ctid;
	TransactionId update_xmax;

	/*
	 * Get information on the (current) result relation.
	 */
	if (estate->es_result_partitions && planGen == PLANGEN_OPTIMIZER)
	{
		Assert(estate->es_result_partitions->part->parrelid);

#ifdef USE_ASSERT_CHECKING
		Oid parent = estate->es_result_partitions->part->parrelid;
#endif

		/* Obtain part for current tuple. */
		resultRelInfo = slot_get_partition(planSlot, estate);
		estate->es_result_relation_info = resultRelInfo;

#ifdef USE_ASSERT_CHECKING
		Oid part = RelationGetRelid(resultRelInfo->ri_RelationDesc);
#endif

		Assert(parent != part);
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	Assert (!resultRelInfo->ri_projectReturning);

	if (planGen == PLANGEN_PLANNER)
	{
		/* BEFORE ROW DELETE Triggers */
		if (resultRelInfo->ri_TrigDesc &&
			resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_DELETE] > 0)
		{
			bool		dodelete;

			dodelete = ExecBRDeleteTriggers(estate, resultRelInfo, tupleid,
											estate->es_snapshot->curcid);

			if (!dodelete)			/* "do nothing" */
				return;
		}
	}

	bool isHeapTable = RelationIsHeap(resultRelationDesc);
	bool isAORowsTable = RelationIsAoRows(resultRelationDesc);
	bool isAOColsTable = RelationIsAoCols(resultRelationDesc);
	bool isExternalTable = RelationIsExternal(resultRelationDesc);

	if (isExternalTable && estate->es_result_partitions && 
		estate->es_result_partitions->part->parrelid != 0)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("Delete from external partitions not supported.")));			
		return;
	}
	/*
	 * delete the tuple
	 *
	 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check that
	 * the row to be deleted is visible to that snapshot, and throw a can't-
	 * serialize error if not.	This is a special-case behavior needed for
	 * referential integrity updates in serializable transactions.
	 */
ldelete:;
	if (isHeapTable)
	{
		result = heap_delete(resultRelationDesc, tupleid,
						 &update_ctid, &update_xmax,
						 estate->es_snapshot->curcid,
						 estate->es_crosscheck_snapshot,
						 true /* wait for commit */ );
	}
	else if (isAORowsTable)
	{
		if (IsXactIsoLevelSerializable)
		{
			if (!isUpdate)
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Deletes on append-only tables are not supported in serializable transactions.")));		
			else
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Updates on append-only tables are not supported in serializable transactions.")));	
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc = 
				appendonly_delete_init(resultRelationDesc, ActiveSnapshot);
		}

		AOTupleId* aoTupleId = (AOTupleId*)tupleid;
		result = appendonly_delete(resultRelInfo->ri_deleteDesc, aoTupleId);
	} 
	else if (isAOColsTable)
	{
		if (IsXactIsoLevelSerializable)
		{
			if (!isUpdate)
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Deletes on append-only tables are not supported in serializable transactions.")));		
			else
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Updates on append-only tables are not supported in serializable transactions.")));		
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc = 
				aocs_delete_init(resultRelationDesc);
		}

		AOTupleId* aoTupleId = (AOTupleId*)tupleid;
		result = aocs_delete(resultRelInfo->ri_deleteDesc, aoTupleId);
	}
	else
	{
		Insist(0);
	}
	switch (result)
	{
		case HeapTupleSelfUpdated:
			/* already deleted by self; nothing to do */
		
			/*
			 * In an scenario in which R(a,b) and S(a,b) have 
			 *        R               S
			 *    ________         ________
			 *     (1, 1)           (1, 2)
			 *                      (1, 7)
 			 *
   			 *  An update query such as:
 			 *   UPDATE R SET a = S.b  FROM S WHERE R.b = S.a;
 			 *   
 			 *  will have an non-deterministic output. The tuple in R 
			 * can be updated to (2,1) or (7,1).
 			 * Since the introduction of SplitUpdate, these queries will 
			 * send multiple requests to delete the same tuple. Therefore, 
			 * in order to avoid a non-deterministic output, 
			 * an error is reported in such scenario.
 			 */
			if (isUpdate)
			{

				ereport(ERROR,
					(errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION ),
					errmsg("multiple updates to a row by the same query is not allowed")));
			}

			return;

		case HeapTupleMayBeUpdated:
			break;

		case HeapTupleUpdated:
			if (IsXactIsoLevelSerializable)
				ereport(ERROR,
						(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
						 errmsg("could not serialize access due to concurrent update")));
			else if (!ItemPointerEquals(tupleid, &update_ctid))
			{
				TupleTableSlot *epqslot;

				epqslot = EvalPlanQual(estate,
									   resultRelInfo->ri_RangeTableIndex,
									   &update_ctid,
									   update_xmax,
									   estate->es_snapshot->curcid);
				if (!TupIsNull(epqslot))
				{
					*tupleid = update_ctid;
					goto ldelete;
				}
			}
			/* tuple already deleted; nothing to do */
			return;

		default:
			elog(ERROR, "unrecognized heap_delete status: %u", result);
			return;
	}

	if (!isUpdate)
	{
		IncrDeleted();
		(estate->es_processed)++;
		/*
		 * To notify master if tuples deleted or not, to update mod_count.
		 */
		(resultRelInfo->ri_aoprocessed)++;
	}

	/*
	 * Note: Normally one would think that we have to delete index tuples
	 * associated with the heap tuple now...
	 *
	 * ... but in POSTGRES, we have no need to do this because VACUUM will
	 * take care of it later.  We can't delete index tuples immediately
	 * anyway, since the tuple is still visible to other transactions.
	 */


	if (planGen == PLANGEN_PLANNER)
	{
		/* AFTER ROW DELETE Triggers */
		ExecARDeleteTriggers(estate, resultRelInfo, tupleid);
	}
}

/* ----------------------------------------------------------------
 *		ExecUpdate
 *
 *		note: we can't run UPDATE queries with transactions
 *		off because UPDATEs are actually INSERTs and our
 *		scan will mistakenly loop forever, updating the tuple
 *		it just inserted..	This should be fixed but until it
 *		is, we don't want to get stuck in an infinite loop
 *		which corrupts your database..
 * ----------------------------------------------------------------
 */
void
ExecUpdate(TupleTableSlot *slot,
		   ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate)
{
	void*	tuple;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	HTSU_Result result;
	ItemPointerData update_ctid;
	TransactionId update_xmax;
	AOTupleId	aoTupleId = AOTUPLEID_INIT;
	TupleTableSlot *partslot = NULL;

	/*
	 * abort the operation if not running transactions
	 */
	if (IsBootstrapProcessingMode())
		elog(ERROR, "cannot UPDATE during bootstrap");
	
	/*
	 * get information on the (current) result relation
	 */
	resultRelInfo = estate->es_result_relation_info;
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	bool		rel_is_heap = RelationIsHeap(resultRelationDesc);
	bool 		rel_is_aorows = RelationIsAoRows(resultRelationDesc);
	bool		rel_is_aocols = RelationIsAoCols(resultRelationDesc);
	bool		rel_is_external = RelationIsExternal(resultRelationDesc);

	/*
	 * get the heap tuple out of the tuple table slot, making sure we have a
	 * writable copy
	 */
	if (rel_is_heap)
	{
		partslot = slot;
		tuple = ExecFetchSlotHeapTuple(partslot);
	}
	else if (rel_is_aorows || rel_is_aocols)
	{
		/*
		 * It is necessary to reconstruct a logically compatible tuple to
		 * a phyiscally compatible tuple.  The slot's tuple descriptor comes
		 * from the projection target list, which doesn't indicate dropped
		 * columns, and MemTuple cannot deal with cases without converting
		 * the target list back into the original relation's tuple desc.
		 */
		partslot = reconstructMatchingTupleSlot(slot, resultRelInfo);

		/*
		 * We directly inline toasted columns here as update with toasted columns
		 * would create two references to the same toasted value.
		 */
		tuple = ExecFetchSlotMemTuple(partslot, true);
	}
	else if (rel_is_external) 
	{
		if (estate->es_result_partitions && 
			estate->es_result_partitions->part->parrelid != 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("Update external partitions not supported.")));			
			return;
		}
		else
		{
			partslot = slot;
			tuple = ExecFetchSlotHeapTuple(partslot);
		}
	}
	else 
	{
		Insist(false);
	}

	/* see if this update would move the tuple to a different partition */
	if (estate->es_result_partitions)
		checkPartitionUpdate(estate, partslot, resultRelInfo);

	/* BEFORE ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_UPDATE] > 0)
	{
		HeapTuple	newtuple;

		newtuple = ExecBRUpdateTriggers(estate, resultRelInfo,
										tupleid, tuple,
										estate->es_snapshot->curcid);

		if (newtuple == NULL)	/* "do nothing" */
			return;

		if (newtuple != tuple)	/* modified by Trigger(s) */
		{
			/*
			 * Put the modified tuple into a slot for convenience of routines
			 * below.  We assume the tuple was allocated in per-tuple memory
			 * context, and therefore will go away by itself. The tuple table
			 * slot should not try to clear it.
			 */
			TupleTableSlot *newslot = estate->es_trig_tuple_slot;

			if (newslot->tts_tupleDescriptor != partslot->tts_tupleDescriptor)
				ExecSetSlotDescriptor(newslot, partslot->tts_tupleDescriptor);
			ExecStoreGenericTuple(newtuple, newslot, false);
            newslot->tts_tableOid = partslot->tts_tableOid; /* for constraints */
			partslot = newslot;
			tuple = newtuple;
		}
	}

	/*
	 * Check the constraints of the tuple
	 *
	 * If we generate a new candidate tuple after EvalPlanQual testing, we
	 * must loop back here and recheck constraints.  (We don't need to redo
	 * triggers, however.  If there are any BEFORE triggers then trigger.c
	 * will have done heap_lock_tuple to lock the correct tuple, so there's no
	 * need to do them again.)
	 */
lreplace:;
	if (resultRelationDesc->rd_att->constr)
		ExecConstraints(resultRelInfo, partslot, estate);

	if (!GpPersistent_IsPersistentRelation(resultRelationDesc->rd_id))
	{
		/*
		 * Normal UPDATE path.
		 */

		/*
		 * replace the heap tuple
		 *
		 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check that
		 * the row to be updated is visible to that snapshot, and throw a can't-
		 * serialize error if not.	This is a special-case behavior needed for
		 * referential integrity updates in serializable transactions.
		 */
		if (rel_is_heap)
		{
			result = heap_update(resultRelationDesc, tupleid, tuple,
							 &update_ctid, &update_xmax,
							 estate->es_snapshot->curcid,
							 estate->es_crosscheck_snapshot,
							 true /* wait for commit */ );
		} 
		else if (rel_is_aorows)
		{
			if (IsXactIsoLevelSerializable)
			{
				ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Updates on append-only tables are not supported in serializable transactions.")));			
			}

			if (resultRelInfo->ri_updateDesc == NULL)
			{
				ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
				resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
					appendonly_update_init(resultRelationDesc, ActiveSnapshot, resultRelInfo->ri_aosegno);
			}
			result = appendonly_update(resultRelInfo->ri_updateDesc,
								 tuple, (AOTupleId *) tupleid, &aoTupleId);
		}
		else if (rel_is_aocols)
		{
			if (IsXactIsoLevelSerializable)
			{
				ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Updates on append-only tables are not supported in serializable transactions.")));			
			}

			if (resultRelInfo->ri_updateDesc == NULL)
			{
				ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
				resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
					aocs_update_init(resultRelationDesc, resultRelInfo->ri_aosegno);
			}
			result = aocs_update(resultRelInfo->ri_updateDesc,
								 partslot, (AOTupleId *) tupleid, &aoTupleId);
		}
		else
		{
			Assert(!"We should not be here");
		}
		switch (result)
		{
			case HeapTupleSelfUpdated:
				/* already deleted by self; nothing to do */
				return;

			case HeapTupleMayBeUpdated:
				break;

			case HeapTupleUpdated:
				if (IsXactIsoLevelSerializable)
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
							 errmsg("could not serialize access due to concurrent update")));
				else if (!ItemPointerEquals(tupleid, &update_ctid))
				{
					TupleTableSlot *epqslot;

					epqslot = EvalPlanQual(estate,
										   resultRelInfo->ri_RangeTableIndex,
										   &update_ctid,
										   update_xmax,
										   estate->es_snapshot->curcid);
					if (!TupIsNull(epqslot))
					{
						*tupleid = update_ctid;
						partslot = ExecFilterJunk(estate->es_junkFilter, epqslot);
						tuple = ExecFetchSlotHeapTuple(partslot);
						goto lreplace;
					}
				}
				/* tuple already deleted; nothing to do */
				return;

			default:
				elog(ERROR, "unrecognized heap_update status: %u", result);
				return;
		}
	}
	else
	{
		HeapTuple persistentTuple;

		/*
		 * Persistent metadata path.
		 */
		persistentTuple = heap_copytuple(tuple);
		persistentTuple->t_self = *tupleid;

		frozen_heap_inplace_update(resultRelationDesc, persistentTuple);

		heap_freetuple(persistentTuple);
	}

	IncrReplaced();
	(estate->es_processed)++;
	(resultRelInfo->ri_aoprocessed)++;

	/*
	 * Note: instead of having to update the old index tuples associated with
	 * the heap tuple, all we do is form and insert new index tuples. This is
	 * because UPDATEs are actually DELETEs and INSERTs, and index tuple
	 * deletion is done later by VACUUM (see notes in ExecDelete).	All we do
	 * here is insert new index tuples.  -cim 9/27/89
	 */
	/*
	 * insert index entries for tuple
	 *
	 * Note: heap_update returns the tid (location) of the new tuple in the
	 * t_self field.
	 */
	if (rel_is_aorows || rel_is_aocols)
	{
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(partslot, (ItemPointer)&aoTupleId, estate, false);
	}
	else
	{
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(partslot, &(((HeapTuple) tuple)->t_self), estate, false);
	}

	/* AFTER ROW UPDATE Triggers */
	ExecARUpdateTriggers(estate, resultRelInfo, tupleid, tuple);

}

