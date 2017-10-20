/*-------------------------------------------------------------------------
 *
 * instrument.c
 *	 functions for instrumentation of plan execution
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 2001-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/instrument.c,v 1.20 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#include <cdb/cdbvars.h>

#include "storage/spin.h"
#include "executor/instrument.h"
#include "utils/memutils.h"

static bool shouldPickInstrInShmem(NodeTag tag);
static Instrumentation *pickInstrFromShmem(const Plan *plan, int instrument_options);
static void instrShmemRecycleCallback(ResourceReleasePhase phase, bool isCommit,
						  bool isTopLevel, void *arg);

InstrumentationHeader *InstrumentGlobal = NULL;

static int  scanNodeCounter = 0;
static int  shmemNumSlots = -1;
static bool instrumentResownerCallbackRegistered = false;
static InstrumentationResownerSet *slotsOccupied = NULL;

/* Allocate new instrumentation structure(s) */
Instrumentation *
InstrAlloc(int n, int instrument_options)
{
	Instrumentation *instr = palloc0(n * sizeof(Instrumentation));

	if (instrument_options & (INSTRUMENT_TIMER | INSTRUMENT_CDB))
	{
		bool		need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
		bool		need_cdb = (instrument_options & INSTRUMENT_CDB) != 0;
		int			i;

		for (i = 0; i < n; i++)
		{
			instr[i].need_timer = need_timer;
			instr[i].need_cdb = need_cdb;
		}
	}

	/* we don't need to do any initialization except zero 'em */
	instr->numPartScanned = 0;

	return instr;
}

/* Entry to a plan node */
/*
 * GPDB Note: Macro INSTR_START_NODE replaces InstrStartNode in ExecProcNode for
 * performance benefits, other files keep using InstrStartNode. Pay attention
 * to keep InstrStartNode/INSTR_START_NODE synchronized when modifying this function.
 */
void
InstrStartNode(Instrumentation *instr)
{
	if (INSTR_TIME_IS_ZERO(instr->starttime))
		INSTR_TIME_SET_CURRENT(instr->starttime);
	else
		elog(DEBUG2, "InstrStartNode called twice in a row");
}

/* Exit from a plan node */
/*
 * GPDB Note: Macro INSTR_STOP_NODE replaces InstrStopNode in ExecProcNode for
 * performance benefits, other files keep using InstrStopNode. Pay attention
 * to keep InstrStopNode/INSTR_STOP_NODE synchronized when modifying this function.
 */
void
InstrStopNode(Instrumentation *instr, uint64 nTuples)
{
	instr_time	endtime;

	/* count the returned tuples */
	instr->tuplecount += nTuples;

	if (INSTR_TIME_IS_ZERO(instr->starttime))
	{
		elog(DEBUG2, "InstrStopNode called without start");
		return;
	}

	INSTR_TIME_SET_CURRENT(endtime);
	INSTR_TIME_ACCUM_DIFF(instr->counter, endtime, instr->starttime);

	/* Is this the first tuple of this cycle? */
	if (!instr->running)
	{
		instr->running = true;
		instr->firsttuple = INSTR_TIME_GET_DOUBLE(instr->counter);
		/* CDB: save this start time as the first start */
		instr->firststart = instr->starttime;
	}

	INSTR_TIME_SET_ZERO(instr->starttime);
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
		elog(DEBUG2, "InstrEndLoop called on running node");

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
	if (Gp_session_role == GP_ROLE_UTILITY)
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
	if (Gp_session_role == GP_ROLE_UTILITY)
		return false;

	if (!gp_enable_query_metrics || NULL == InstrumentGlobal)
		return false;

	switch (tag)
	{
		case T_SeqScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:

			/*
			 * If table has many partitions, legacy planner will generate a
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
		slot->segid = (int16) Gp_segment;
		slot->pid = MyProcPid;
		gpmon_gettmid(&(slot->tmid));
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
