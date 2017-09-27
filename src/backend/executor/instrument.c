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

#include "executor/instrument.h"


/* Allocate new instrumentation structure(s) */
Instrumentation *
InstrAlloc(int n, int instrument_options)
{
	Instrumentation *instr;

	/* initialize all fields to zeroes, then modify as needed */
	instr = palloc0(n * sizeof(Instrumentation));
	if (instrument_options & INSTRUMENT_TIMER)
	{
		int			i;

		for (i = 0; i < n; i++)
		{
			instr[i].need_timer = true;
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
	if (instr->need_timer && INSTR_TIME_IS_ZERO(instr->starttime))
		INSTR_TIME_SET_CURRENT(instr->starttime);
	else
		elog(DEBUG2, "InstrStartNode called twice in a row");
}

/* Exit from a plan node */
void
InstrStopNode(Instrumentation *instr, double nTuples)
{
	instr_time	endtime;

	/* count the returned tuples */
	instr->tuplecount += nTuples;

	/* let's update the time only if the timer was requested */
	if (instr->need_timer)
	{

		if (INSTR_TIME_IS_ZERO(instr->starttime))
		{
			elog(DEBUG2, "InstrStopNode called without start");
			return;
		}

		INSTR_TIME_SET_CURRENT(endtime);
		INSTR_TIME_ACCUM_DIFF(instr->counter, endtime, instr->starttime);

		INSTR_TIME_SET_ZERO(instr->starttime);
	}

	/* Is this the first tuple of this cycle? */
	if (!instr->running)
	{
		instr->running = true;
		instr->firsttuple = INSTR_TIME_GET_DOUBLE(instr->counter);
		/* CDB: save this start time as the first start */
		instr->firststart = instr->starttime;
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
