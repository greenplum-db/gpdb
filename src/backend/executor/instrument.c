/*-------------------------------------------------------------------------
 *
 * instrument.c
 *	 functions for instrumentation of plan execution
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 2001-2016, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/executor/instrument.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "cdb/cdbvars.h"
#include "storage/spin.h"
#include "executor/instrument.h"
#include "utils/memutils.h"
#include "miscadmin.h"
#include "storage/shmem.h"
#include "cdb/cdbdtxcontextinfo.h"
#include "cdb/cdbtm.h"

BufferUsage pgBufferUsage;
static BufferUsage save_pgBufferUsage;

static void BufferUsageAdd(BufferUsage *dst, const BufferUsage *add);
static void BufferUsageAccumDiff(BufferUsage *dst,
					 const BufferUsage *add, const BufferUsage *sub);

/* GPDB specific */
static bool shouldPickInstrInShmem(NodeTag tag);
static Instrumentation *pickInstrFromShmem(const Plan *plan, int instrument_options);
static void instrShmemRecycleCallback(ResourceReleasePhase phase, bool isCommit,
						  bool isTopLevel, void *arg);
static void gp_gettmid(int32* tmid);

InstrumentationHeader *InstrumentGlobal = NULL;
static int  scanNodeCounter = 0;
static int  shmemNumSlots = -1;
static bool instrumentResownerCallbackRegistered = false;
static InstrumentationResownerSet *slotsOccupied = NULL;

/* Allocate new instrumentation structure(s) */
Instrumentation *
InstrAlloc(int n, int instrument_options)
{
	Instrumentation *instr;

	/* initialize all fields to zeroes, then modify as needed */
	instr = palloc0(n * sizeof(Instrumentation));
	if (instrument_options & (INSTRUMENT_BUFFERS | INSTRUMENT_TIMER | INSTRUMENT_CDB))
	{
		bool		need_buffers = (instrument_options & INSTRUMENT_BUFFERS) != 0;
		bool		need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
		bool		need_cdb = (instrument_options & INSTRUMENT_CDB) != 0;
		int			i;

		for (i = 0; i < n; i++)
		{
			instr[i].need_bufusage = need_buffers;
			instr[i].need_timer = need_timer;
			instr[i].need_cdb = need_cdb;
		}
	}

	return instr;
}

/* Initialize an pre-allocated instrumentation structure. */
void
InstrInit(Instrumentation *instr, int instrument_options)
{
	memset(instr, 0, sizeof(Instrumentation));
	instr->need_bufusage = (instrument_options & INSTRUMENT_BUFFERS) != 0;
	instr->need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
}

/* Entry to a plan node */
void
InstrStartNode(Instrumentation *instr)
{
	if (instr->need_timer)
	{
		if (INSTR_TIME_IS_ZERO(instr->starttime))
			INSTR_TIME_SET_CURRENT(instr->starttime);
		else
			elog(ERROR, "InstrStartNode called twice in a row");
	}

	/* save buffer usage totals at node entry, if needed */
	if (instr->need_bufusage)
		instr->bufusage_start = pgBufferUsage;
}

/* Exit from a plan node */
void
InstrStopNode(Instrumentation *instr, uint64 nTuples)
{
	instr_time	endtime;
	instr_time	starttime;

	starttime = instr->starttime;

	/* count the returned tuples */
	instr->tuplecount += nTuples;

	/* let's update the time only if the timer was requested */
	if (instr->need_timer)
	{
		if (INSTR_TIME_IS_ZERO(instr->starttime))
			elog(ERROR, "InstrStopNode called without start");

		INSTR_TIME_SET_CURRENT(endtime);
		INSTR_TIME_ACCUM_DIFF(instr->counter, endtime, instr->starttime);

		INSTR_TIME_SET_ZERO(instr->starttime);
	}

	/* Add delta of buffer usage since entry to node's totals */
	if (instr->need_bufusage)
		BufferUsageAccumDiff(&instr->bufusage,
							 &pgBufferUsage, &instr->bufusage_start);

	/* Is this the first tuple of this cycle? */
	if (!instr->running)
	{
		instr->running = true;
		instr->firsttuple = INSTR_TIME_GET_DOUBLE(instr->counter);
		/* CDB: save this start time as the first start */
		instr->firststart = starttime;
	}
}

/* Finish a run cycle for a plan node */
void
InstrEndLoop(Instrumentation *instr)
{
	double		totaltime;

	/* Skip if nothing has happened, or already shut down */
	if (!instr->running)
		return;

	if (!INSTR_TIME_IS_ZERO(instr->starttime))
		elog(ERROR, "InstrEndLoop called on running node");

	/* Accumulate per-cycle statistics into totals */
	totaltime = INSTR_TIME_GET_DOUBLE(instr->counter);

	/* CDB: Report startup time from only the first cycle. */
	if (instr->nloops == 0)
		instr->startup = instr->firsttuple;

	instr->total += totaltime;
	instr->ntuples += instr->tuplecount;
	instr->nloops += 1;

	/* Reset for next cycle (if any) */
	instr->running = false;
	INSTR_TIME_SET_ZERO(instr->starttime);
	INSTR_TIME_SET_ZERO(instr->counter);
	instr->firsttuple = 0;
	instr->tuplecount = 0;
}

/* aggregate instrumentation information */
void
InstrAggNode(Instrumentation *dst, Instrumentation *add)
{
	if (!dst->running && add->running)
	{
		dst->running = true;
		dst->firsttuple = add->firsttuple;
	}
	else if (dst->running && add->running && dst->firsttuple > add->firsttuple)
		dst->firsttuple = add->firsttuple;

	INSTR_TIME_ADD(dst->counter, add->counter);

	dst->tuplecount += add->tuplecount;
	dst->startup += add->startup;
	dst->total += add->total;
	dst->ntuples += add->ntuples;
	dst->nloops += add->nloops;
	dst->nfiltered1 += add->nfiltered1;
	dst->nfiltered2 += add->nfiltered2;

	/* Add delta of buffer usage since entry to node's totals */
	if (dst->need_bufusage)
		BufferUsageAdd(&dst->bufusage, &add->bufusage);
}

/* note current values during parallel executor startup */
void
InstrStartParallelQuery(void)
{
	save_pgBufferUsage = pgBufferUsage;
}

/* report usage after parallel executor shutdown */
void
InstrEndParallelQuery(BufferUsage *result)
{
	memset(result, 0, sizeof(BufferUsage));
	BufferUsageAccumDiff(result, &pgBufferUsage, &save_pgBufferUsage);
}

/* accumulate work done by workers in leader's stats */
void
InstrAccumParallelQuery(BufferUsage *result)
{
	BufferUsageAdd(&pgBufferUsage, result);
}

/* dst += add */
static void
BufferUsageAdd(BufferUsage *dst, const BufferUsage *add)
{
	dst->shared_blks_hit += add->shared_blks_hit;
	dst->shared_blks_read += add->shared_blks_read;
	dst->shared_blks_dirtied += add->shared_blks_dirtied;
	dst->shared_blks_written += add->shared_blks_written;
	dst->local_blks_hit += add->local_blks_hit;
	dst->local_blks_read += add->local_blks_read;
	dst->local_blks_dirtied += add->local_blks_dirtied;
	dst->local_blks_written += add->local_blks_written;
	dst->temp_blks_read += add->temp_blks_read;
	dst->temp_blks_written += add->temp_blks_written;
	INSTR_TIME_ADD(dst->blk_read_time, add->blk_read_time);
	INSTR_TIME_ADD(dst->blk_write_time, add->blk_write_time);
}

/* dst += add - sub */
static void
BufferUsageAccumDiff(BufferUsage *dst,
					 const BufferUsage *add,
					 const BufferUsage *sub)
{
	dst->shared_blks_hit += add->shared_blks_hit - sub->shared_blks_hit;
	dst->shared_blks_read += add->shared_blks_read - sub->shared_blks_read;
	dst->shared_blks_dirtied += add->shared_blks_dirtied - sub->shared_blks_dirtied;
	dst->shared_blks_written += add->shared_blks_written - sub->shared_blks_written;
	dst->local_blks_hit += add->local_blks_hit - sub->local_blks_hit;
	dst->local_blks_read += add->local_blks_read - sub->local_blks_read;
	dst->local_blks_dirtied += add->local_blks_dirtied - sub->local_blks_dirtied;
	dst->local_blks_written += add->local_blks_written - sub->local_blks_written;
	dst->temp_blks_read += add->temp_blks_read - sub->temp_blks_read;
	dst->temp_blks_written += add->temp_blks_written - sub->temp_blks_written;
	INSTR_TIME_ACCUM_DIFF(dst->blk_read_time,
						  add->blk_read_time, sub->blk_read_time);
	INSTR_TIME_ACCUM_DIFF(dst->blk_write_time,
						  add->blk_write_time, sub->blk_write_time);
}

/* Calculate number slots from gp_instrument_shmem_size */
Size
InstrShmemNumSlots(void)
{
	if (shmemNumSlots < 0) {
		shmemNumSlots = (int)(gp_instrument_shmem_size * 1024 - sizeof(InstrumentationHeader)) / sizeof(InstrumentationSlot);
		shmemNumSlots = (shmemNumSlots < 0) ? 0 : shmemNumSlots;
	}
	return shmemNumSlots;
}

/* Allocate a header and an array of Instrumentation slots */
Size
InstrShmemSize(void)
{
	Size		size = 0;
	Size		number_slots;

	/* If start in utility mode, disallow Instrumentation on Shmem */
	if (Gp_role == GP_ROLE_UTILITY)
		return size;

	/* If GUCs not enabled, bypass Instrumentation on Shmem */
	if (!gp_enable_query_metrics || gp_instrument_shmem_size <= 0)
		return size;

	number_slots = InstrShmemNumSlots();

	if (number_slots <= 0)
		return size;

	size = add_size(size, sizeof(InstrumentationHeader));
	size = add_size(size, mul_size(number_slots, sizeof(InstrumentationSlot)));

	return size;
}

/* Initialize Shmem space to construct a free list of Instrumentation */
void
InstrShmemInit(void)
{
	Size		size, number_slots;
	InstrumentationSlot *slot;
	InstrumentationHeader *header;
	int			i;

	number_slots = InstrShmemNumSlots();
	size = InstrShmemSize();
	if (size <= 0)
		return;

	/* Allocate space from Shmem */
	header = (InstrumentationHeader *) ShmemAlloc(size);
	if (!header)
		ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));

	/* Initialize header and all slots to zeroes, then modify as needed */
	memset(header, PATTERN, size);

	/* pointer to the first Instrumentation slot */
	slot = (InstrumentationSlot *) (header + 1);

	/* header points to the first slot */
	header->head = slot;
	header->free = number_slots;
	SpinLockInit(&header->lock);

	/* Each slot points to next one to construct the free list */
	for (i = 0; i < number_slots - 1; i++)
		GetInstrumentNext(&slot[i]) = &slot[i + 1];
	GetInstrumentNext(&slot[i]) = NULL;

	/* Finished init the free list */
	InstrumentGlobal = header;

	if (NULL != InstrumentGlobal && !instrumentResownerCallbackRegistered)
	{
		/*
		 * Register a callback function in ResourceOwner to recycle Instr in
		 * shmem
		 */
		RegisterResourceReleaseCallback(instrShmemRecycleCallback, NULL);
		instrumentResownerCallbackRegistered = true;
	}
}

/*
 * This is GPDB replacement of InstrAlloc for ExecInitNode to get an
 * Instrumentation struct
 *
 * Use shmem if gp_enable_query_metrics is on and there is free slot.
 * Otherwise use local memory.
 */
Instrumentation *
GpInstrAlloc(const Plan *node, int instrument_options)
{
	Instrumentation *instr = NULL;

	if (shouldPickInstrInShmem(nodeTag(node)))
		instr = pickInstrFromShmem(node, instrument_options);

	if (instr == NULL)
		instr = InstrAlloc(1, instrument_options);

	return instr;
}

static bool
shouldPickInstrInShmem(NodeTag tag)
{
	/* For utility mode, don't alloc in shmem */
	if (Gp_role == GP_ROLE_UTILITY)
		return false;

	if (!gp_enable_query_metrics || NULL == InstrumentGlobal)
		return false;

	switch (tag)
	{
		case T_SeqScan:

			/*
			 * If table has many partitions, Postgres planner will generate a
			 * plan with many SCAN nodes under a APPEND node. If the number of
			 * partitions are too many, this plan will occupy too many slots.
			 * Here is a limitation on number of shmem slots used by scan
			 * nodes for each backend. Instruments exceeding the limitation
			 * are allocated local memory.
			 */
			if (scanNodeCounter >= MAX_SCAN_ON_SHMEM)
				return false;
			scanNodeCounter++;
			break;
		default:
			break;
	}
	return true;
}

/*
 * Pick an Instrumentation from free slots in Shmem.
 * Return NULL when no more free slots in Shmem.
 *
 * Instrumentation returned by this function requires to be
 * recycled back to the free slots list when the query is done.
 * See instrShmemRecycleCallback for recycling behavior
 */
static Instrumentation *
pickInstrFromShmem(const Plan *plan, int instrument_options)
{
	Instrumentation *instr = NULL;
	InstrumentationSlot *slot = NULL;
	InstrumentationResownerSet *item;

	/* Lock to protect write to header */
	SpinLockAcquire(&InstrumentGlobal->lock);

	/* Pick the first free slot */
	slot = InstrumentGlobal->head;
	if (NULL != slot && SlotIsEmpty(slot))
	{
		/* Header points to the next free slot */
		InstrumentGlobal->head = GetInstrumentNext(slot);
		InstrumentGlobal->free--;
	}

	SpinLockRelease(&InstrumentGlobal->lock);

	if (NULL != slot && SlotIsEmpty(slot))
	{
		memset(slot, 0x00, sizeof(InstrumentationSlot));
		/* initialize the picked slot */
		instr = &(slot->data);
		slot->segid = (int16) GpIdentity.segindex;
		slot->pid = MyProcPid;
		gp_gettmid(&(slot->tmid));
		slot->ssid = gp_session_id;
		slot->ccnt = gp_command_count;
		slot->nid = (int16) plan->plan_node_id;

		MemoryContext contextSave = MemoryContextSwitchTo(TopMemoryContext);

		item = (InstrumentationResownerSet *) palloc0(sizeof(InstrumentationResownerSet));
		item->owner = CurrentResourceOwner;
		item->slot = slot;
		item->next = slotsOccupied;
		slotsOccupied = item;
		MemoryContextSwitchTo(contextSave);
	}

	if (NULL != instr && instrument_options & (INSTRUMENT_TIMER | INSTRUMENT_CDB))
	{
		instr->need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
		instr->need_cdb = (instrument_options & INSTRUMENT_CDB) != 0;
	}

	return instr;
}

/*
 * Recycle instrumentation in shmem
 */
static void
instrShmemRecycleCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg)
{
	InstrumentationResownerSet *next;
	InstrumentationResownerSet *curr;
	InstrumentationSlot *slot;

	if (NULL == InstrumentGlobal || NULL == slotsOccupied || phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	/* Reset scanNodeCounter */
	scanNodeCounter = 0;

	next = slotsOccupied;
	slotsOccupied = NULL;
	SpinLockAcquire(&InstrumentGlobal->lock);
	while (next)
	{
		curr = next;
		next = curr->next;
		if (curr->owner != CurrentResourceOwner)
		{
			curr->next = slotsOccupied;
			slotsOccupied = curr;
			continue;
		}

		slot = curr->slot;

		/* Recycle Instrumentation slot back to the free list */
		memset(slot, PATTERN, sizeof(InstrumentationSlot));

		GetInstrumentNext(slot) = InstrumentGlobal->head;
		InstrumentGlobal->head = slot;
		InstrumentGlobal->free++;

		pfree(curr);
	}
	SpinLockRelease(&InstrumentGlobal->lock);
}

static void gp_gettmid(int32* tmid)
{
	if (QEDtxContextInfo.distributedSnapshot.distribTransactionTimeStamp > 0)
		/* On QE */
		*tmid = (int32)QEDtxContextInfo.distributedSnapshot.distribTransactionTimeStamp;
	else
		/* On QD */
		*tmid = (int32)getDtmStartTime();
}
