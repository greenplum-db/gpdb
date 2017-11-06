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

InstrumentationHeader *InstrumentGlobal = NULL;
int scan_node_counter = 0;
List	   *slotsOccupied = NIL;

/* Allocate new instrumentation structure(s) */
Instrumentation *
InstrAlloc(int n, int instrument_options)
{
	Instrumentation *instr = palloc0(n * sizeof(Instrumentation));

	if (instrument_options & (INSTRUMENT_TIMER | INSTRUMENT_CDB))
	{
		int			i;
		bool		need_timer = instrument_options & INSTRUMENT_TIMER;
		bool		need_cdb = instrument_options & INSTRUMENT_CDB;

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
void
InstrStartNode(Instrumentation *instr)
{
	if (INSTR_TIME_IS_ZERO(instr->starttime))
		INSTR_TIME_SET_CURRENT(instr->starttime);
	else
		elog(DEBUG2, "InstrStartNode called twice in a row");
}

/* Exit from a plan node */
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

/* Allocate a header and an array of Instrumentation slots */
Size
InstrShmemSize(void)
{
	Size		size = 0;

	/* If start in utility mode, disallow Instrumentation on Shmem */
	if (Gp_role != GP_ROLE_UTILITY && gp_max_shmem_instruments > 0)
	{
		size = add_size(size, sizeof(InstrumentationHeader));
		size = add_size(size, mul_size(gp_max_shmem_instruments, sizeof(InstrumentationSlot)));
	}

	return size;
}

/* Initialize Shmem space to construct a free list of Instrumentation */
void
InstrShmemInit(void)
{
	Size		size;
	InstrumentationSlot *slot;
	InstrumentationHeader *header;
	int			i;

	size = InstrShmemSize();
	if (size <= 0)
		return;

	/* Allocate space from Shmem */
	header = (InstrumentationHeader *) ShmemAlloc(size);
	if (!header)
		ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));

	/* Initialize header and all slots to zeroes, then modify as needed */
	MemSet(header, 0x00, size);

	/* pointer to the first Instrumentation slot */
	slot = (InstrumentationSlot *) (header + 1);

	/* header points to the first slot */
	header->head = slot;
	header->used = 0;
	header->free = gp_max_shmem_instruments;
	SpinLockInit(&header->lock);

	/* Each slot points to next one to construct the free list */
	for (i = 0; i < gp_max_shmem_instruments - 1; i++)
		GetInstrumentNext(&slot[i]) = &slot[i + 1];
	GetInstrumentNext(&slot[i]) = NULL;

	/* Finished init the free list */
	InstrumentGlobal = header;
}

/*
 * This is a replacement of InstrAlloc for ExecInitNode to new an Instrumentation
 * When gp_enable_query_metrics is on and Shmem initilized successfully,
 * this function will try to fetch a free slot from reserved Instrumentation
 * slots in Shmem. Otherwise it will allocte it in local memory.
 * Instrumentation returned by this function require to call InstrShmemRecycle
 * to recycle the slot back to the free list on ExecEndNode. On query abort
 * or error, should also ensure it is recycled.
 */
Instrumentation *
InstrShmemPick(Plan *plan, int eflags, int instrument_options)
{
	Instrumentation *instr = NULL;
	InstrumentationSlot *slot = NULL;
	MemoryContext contextSave;

	if (gp_enable_query_metrics && NULL != InstrumentGlobal && Gp_session_role != GP_ROLE_UTILITY)
	{
		/* Lock to protect write to header */
		SpinLockAcquire(&InstrumentGlobal->lock);

		/* Pick the first free slot */
		slot = InstrumentGlobal->head;
		if (NULL != slot)
		{
			/* Header points to the next free slot */
			InstrumentGlobal->head = GetInstrumentNext(slot);
			InstrumentGlobal->free--;
			InstrumentGlobal->used++;
		}

		SpinLockRelease(&InstrumentGlobal->lock);

		if (NULL != slot)
		{
			/* initialize the picked slot */
			GetInstrumentNext(slot) = NULL;
			instr = &(slot->data);
			instr->in_shmem = true;
			slot->segid = Gp_segment;
			slot->pid = MyProcPid;
			gpmon_gettmid(&(slot->tmid));
			slot->ssid = gp_session_id;
			slot->ccnt = gp_command_count;
			slot->eflags = eflags;
			slot->nid = plan->plan_node_id;

			contextSave = MemoryContextSwitchTo(TopMemoryContext);
			slotsOccupied = lappend(slotsOccupied, slot);
			MemoryContextSwitchTo(contextSave);
		}
	}

	if (NULL == instr)
	{
		/*
		 * Alloc Instrumentation in local memory when gp_enable_query_metrics
		 * is off or failed to pick a slot allocte Instrumentation in local
		 * memory and return it.
		 */
		instr = palloc0(sizeof(Instrumentation));
	}

	if (instrument_options & (INSTRUMENT_TIMER | INSTRUMENT_CDB))
	{
		instr->need_timer = instrument_options & INSTRUMENT_TIMER;
		instr->need_cdb = instrument_options & INSTRUMENT_CDB;
	}

	return instr;
}

/*
 * Recycle the Instrumentation back to Shmem free list
 *  - Node finished normally, should recycle in ExecEndNode
 *  - Query abort or error, should recycle in mppExecutorCleanup
 *  - Fatal error should recycle in proc_exit_prepare
 */
static Instrumentation *
InstrShmemRecycle(Instrumentation *instr)
{
	InstrumentationSlot *slot;
	Instrumentation *clone = NULL;

	if (NULL == instr || NULL == InstrumentGlobal || !instr->in_shmem)
		return instr;

	/* Recycle Instrumentation slot back to the free list */
	slot = (InstrumentationSlot *) instr;

	if (slot->eflags & EXEC_FLAG_EXPLAIN_ANALYZE)
	{
		/*
		 * When EXPLAIN ANALYZE encontered an error, we can't set instrument
		 * to NULL because it is needed in ExplainOnePlan, so migrate it to
		 * local memory and recycle the slot on Shmem
		 */
		clone = (Instrumentation *) palloc0(sizeof(Instrumentation));
		memcpy(clone, instr, sizeof(Instrumentation));
		clone->in_shmem = false;
	}

	MemSet(slot, 0x00, sizeof(InstrumentationSlot));

	SpinLockAcquire(&InstrumentGlobal->lock);

	GetInstrumentNext(slot) = InstrumentGlobal->head;
	InstrumentGlobal->head = slot;
	InstrumentGlobal->free++;
	InstrumentGlobal->used--;

	SpinLockRelease(&InstrumentGlobal->lock);

	return clone;
}

/*
 * Recycle instrumentation in shmem on each backend exit or abort
 */
void
InstrShmemRecycleCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg)
{
	ListCell   *cell;

	foreach(cell, slotsOccupied)
	{
		InstrumentationSlot *slot = lfirst(cell);

		InstrShmemRecycle(&(slot->data));
	}
	list_free(slotsOccupied);
	slotsOccupied = NIL;
}
