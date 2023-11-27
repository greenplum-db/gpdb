#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/pg_inherits.h"
#include "catalog/partition.h"
#include "funcapi.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/partcache.h"

typedef struct GetPartitionsQueueItem
{
	Oid 		relid;
	Oid 		parent_relid;
	bool		isleaf;
	int 		level;
	char 		strategy;
	int 		rank;
	bool		isdefault;
} GetPartitionsQueueItem;

typedef struct GetPartitionsContext
{
	List 		*queue;
} GetPartitionsContext;

static void add_partition_children(List **queue, Relation parent, int level);

extern Datum pg_partition_rank(PG_FUNCTION_ARGS);
extern Datum pg_partition_lowest_child(PG_FUNCTION_ARGS);
extern Datum pg_partition_highest_child(PG_FUNCTION_ARGS);
extern Datum gp_get_partitions(PG_FUNCTION_ARGS);

/*
 * Calculate the rank (1-based) of a non-default range partition. We return
 * NULL for any other relation type.
 */
PG_FUNCTION_INFO_V1(pg_partition_rank);
Datum
pg_partition_rank(PG_FUNCTION_ARGS)
{
	Oid					relid = PG_GETARG_OID(0);
	Oid					parentrelid = InvalidOid;
	Relation			parentrel = NULL;
	PartitionDesc		parentpartdesc = NULL;

	if (!rel_is_range_part_nondefault(relid))
		PG_RETURN_NULL();

	parentrelid = get_partition_parent(relid);
	parentrel = relation_open(parentrelid, AccessShareLock);
	parentpartdesc = RelationRetrievePartitionDesc(parentrel);

	/* Child oids are already sorted by range bounds in ascending order. */
	for (int i = 0; i < parentpartdesc->nparts; i++)
	{
		if (relid == parentpartdesc->oids[i])
		{
			relation_close(parentrel, AccessShareLock);
			PG_RETURN_INT32(i + 1);
		}
	}

	PG_RETURN_NULL(); /* unreachable, keep compiler happy */
}

/*
 * Find the lowest child with respect to range bounds for a range partition.
 * Default partitions are not considered in this calculation.
 */
PG_FUNCTION_INFO_V1(pg_partition_lowest_child);
Datum
pg_partition_lowest_child(PG_FUNCTION_ARGS)
{
	Oid					relid = PG_GETARG_OID(0);
	Relation			rel = NULL;
	PartitionDesc		partdesc = NULL;
	PartitionKey		partkey = NULL;
	Oid					childrelid = InvalidOid;

	rel = relation_open(relid, AccessShareLock);
	partkey = RelationRetrievePartitionKey(rel);
	partdesc = RelationRetrievePartitionDesc(rel);
	if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE &&
		partkey->strategy == PARTITION_STRATEGY_RANGE &&
		partdesc->nparts > 0)
	{
		/*
		 * Child oids are already sorted by range bounds in ascending order.
		 * If default partition exists, it is the last partition.
		 */
		if (partdesc->nparts > 1 || !OidIsValid(get_default_partition_oid(relid)))
			childrelid = partdesc->oids[0];
	}

	relation_close(rel, AccessShareLock);
	if (OidIsValid(childrelid))
		PG_RETURN_OID(childrelid);
	else
		PG_RETURN_NULL();
}

/*
 * Find the highest child with respect to range bounds for a range partition.
 * Default partitions are not considered in this calculation.
 */
PG_FUNCTION_INFO_V1(pg_partition_highest_child);
Datum
pg_partition_highest_child(PG_FUNCTION_ARGS)
{
	Oid					relid = PG_GETARG_OID(0);
	Relation			rel = NULL;
	PartitionDesc		partdesc = NULL;
	PartitionKey		partkey = NULL;
	Oid					default_relid = InvalidOid;
	Oid					highest_relid = InvalidOid;

	rel = relation_open(relid, AccessShareLock);
	partkey = RelationRetrievePartitionKey(rel);
	partdesc = RelationRetrievePartitionDesc(rel);
	if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE &&
		partkey->strategy == PARTITION_STRATEGY_RANGE &&
		partdesc->nparts > 0)
	{
		/*
		 * Child oids are already sorted by range bounds in ascending order.
		 * If default partition exists, it is the last partition.
		 */
		default_relid = get_default_partition_oid(relid);
		if (!OidIsValid(default_relid))
			highest_relid = partdesc->oids[partdesc->nparts - 1];
		else if (partdesc->nparts > 1)
		{
			Assert (default_relid == partdesc->oids[partdesc->nparts - 1]);
			highest_relid = partdesc->oids[partdesc->nparts - 2];
		}
	}

	relation_close(rel, AccessShareLock);
	if (OidIsValid(highest_relid))
		PG_RETURN_OID(highest_relid);
	else
		PG_RETURN_NULL();
}

/*
 * gp_get_partitions
 *
 * Main driver for the gp_partitions view.
 *
 * Given the root (or interior node) of a partition hierarchy, return a result
 * set akin to pg_partition_tree(), complete with rank, level etc. There will be
 * 1 row per member (the root of the hierarchy isn't included, unlike
 * pg_partition_tree()).
 *
 * To obtain this set, we perform a level-order traversal of the partition
 * hierarchy.
 */
PG_FUNCTION_INFO_V1(gp_get_partitions);
Datum
gp_get_partitions(PG_FUNCTION_ARGS)
{
#define GP_GET_PARTITION_COLS	7
	Oid						rootrelid = PG_GETARG_OID(0);
	FuncCallContext 		*funcctx;
	GetPartitionsContext	*context;

	/* stuff done only on the first call of the function */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext 			oldcxt;
		TupleDesc				tupdesc;
		Relation 				rootrel;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		rootrel = relation_open(rootrelid, AccessShareLock);

		if (rootrel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
		{
			relation_close(rootrel, AccessShareLock);
			SRF_RETURN_DONE(funcctx);
		}

		/*
		 * Grab access share locks on hierarchy, they will be implicitly
		 * released at transaction end. This is similar to pg_partition_tree().
		 */
		find_all_inheritors(rootrelid, AccessShareLock, NULL);

		/* switch to memory context appropriate for multiple function calls */
		oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(GP_GET_PARTITION_COLS);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "relid",
						   REGCLASSOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "parentid",
						   REGCLASSOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "isleaf",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "partitionlevel",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "partitiontype",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "partitionrank",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "is_default",
						   BOOLOID, -1, 0);
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* Add initial entries into the queue */
		context = palloc0(sizeof(GetPartitionsContext));
		add_partition_children(&context->queue, rootrel, 0);
		funcctx->user_fctx = context;

		MemoryContextSwitchTo(oldcxt);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();
	context = (GetPartitionsContext *) funcctx->user_fctx;

	if (list_length(context->queue) > 0)
	{
		Datum					result;
		Datum					values[GP_GET_PARTITION_COLS];
		bool					nulls[GP_GET_PARTITION_COLS];
		HeapTuple				tuple;
		GetPartitionsQueueItem 	*next;
		MemoryContext 			oldcxt;

		next = (GetPartitionsQueueItem *) linitial(context->queue);

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(nulls, 0, sizeof(nulls));
		MemSet(values, 0, sizeof(values));

		/* relid */
		values[0] = ObjectIdGetDatum(next->relid);

		/* parentid */
		values[1] = ObjectIdGetDatum(next->parent_relid);

		/* isleaf */
		values[2] = BoolGetDatum(next->isleaf);

		/* partitionlevel */
		values[3] = Int32GetDatum(next->level);

		/* partitiontype */
		values[4] = CStringGetTextDatum(PartitionStrategyGetName(next->strategy));

		/*
		 * partitionrank:
		 * -1 signifies that partition rank here is N/A. See
		 * add_partition_children().
		 */
		if (next->rank != -1)
			values[5] = Int32GetDatum(next->rank);
		else
			nulls[5] = true;

		/* isdefault */
		values[6] = next->isdefault;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		/* switch to memory context appropriate for multiple function calls */
		oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		if (!next->isleaf)
		{
			/* add children to the level-order-traversal queue */
			add_partition_children(&context->queue,
								   relation_open(next->relid, NoLock),
								   next->level);
		}

		/* de-queue */
		context->queue = list_delete_first(context->queue);

		MemoryContextSwitchTo(oldcxt);

		SRF_RETURN_NEXT(funcctx, result);
	}

	/* done when there are no more elements left */
	SRF_RETURN_DONE(funcctx);
}

/*
 * For a given partition parent, add all its children to the
 * level-order-traversal queue. Assumes that the parent relation is already open
 * and locks have been taken earlier. We also close the parent relation at the
 * end of this function, since we no longer need the parent's Relation object.
 */
static void
add_partition_children(List **queue, Relation parent, int level)
{
	PartitionDesc 	pdesc;

	Assert(RelationIsValid(parent));
	Assert(parent->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);
	Assert(level >= 0);

	/*
	 * Building the partition descriptor guarantees that its children are sorted
	 * in order of their partition boundaries.
	 */
	pdesc = RelationRetrievePartitionDesc(parent);

	for (int i = 0; i < pdesc->nparts; i++)
	{
		GetPartitionsQueueItem *item = palloc0(sizeof(GetPartitionsQueueItem));
		bool isdefault = (pdesc->boundinfo->default_index == i);

		item->relid = pdesc->oids[i];
		item->parent_relid = RelationGetRelid(parent);
		item->isleaf = pdesc->is_leaf[i];
		item->level = level + 1;
		item->strategy = pdesc->boundinfo->strategy;

		if (pdesc->boundinfo->strategy == PARTITION_STRATEGY_RANGE && !isdefault)
			item->rank = i + 1; /* since oids array is sorted by part bounds */
		else
			item->rank = -1; /* doesn't have a rank */

		item->isdefault = isdefault;

		*queue = lappend(*queue, item);
	}

	/* now close the relation */
	relation_close(parent, NoLock);
}
