/*-------------------------------------------------------------------------
 *
 * cdbtimer.c
 *	  Functions to manipulate timers used in a backend.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbtimer.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <stddef.h>

#include "libpq/pqsignal.h"
#include "cdb/cdbtimer.h"


/*
 * resetTimers - disable process interval timers
 *
 * Testing has shown that fork on some systems doesn't clear timers as it should and
 * exec is subjected to *fatal* interruption by timers so the timers are disabled
 * before the fork is done.  Before resetting the interval timers, the all signals
 * are blocked.  After resetting the interval timers, the signal mask is restored.
 *
 * otimers: a pointer to an itmers struct to contain the old timer values.  If
 * 		NULL, the old timer valuess are not preserved.
 *
 * Errors from setitimer are not expected and are handled using Assert (as in
 * PGSempahoreLockTimed).
 */
void
resetTimers(struct itimers *otimers)
{
	int			err;
	struct itimerval xtimer;	/* Zero timer for disabling */
	struct itimerval *p_rtimer = NULL;	/* ITIMER_REAL */
	struct itimerval *p_vtimer = NULL;	/* ITIMER_VIRTUAL */
	struct itimerval *p_ptimer = NULL;	/* ITIMER_PROF */

	if (otimers != NULL)
	{
		p_rtimer = &otimers->rtimer;
		p_vtimer = &otimers->vtimer;
		p_ptimer = &otimers->ptimer;
	}

	/*
	 * Block signals while capturing timers.
	 */
	PG_SETMASK(&BlockSig);

	/*
	 * Disable all process interval timers preserving the old values if
	 * requested.
	 */
	timerclear(&xtimer.it_interval);
	timerclear(&xtimer.it_value);

	err = setitimer(ITIMER_REAL, &xtimer, p_rtimer);
	Assert(err == 0);

	err = setitimer(ITIMER_VIRTUAL, &xtimer, p_vtimer);
	Assert(err == 0);

	err = setitimer(ITIMER_REAL, &xtimer, p_ptimer);
	Assert(err == 0);

	/*
	 * Restore signal mask.
	 */
	PG_SETMASK(&UnBlockSig);
}


/*
 * restoreTimers - restore timers from itimers struct.
 *
 * Restores the process interval timers stored in the itimers struct
 * by resetTimers.
 *
 * timers: a reference to the itimers struct passed to the resetTimers
 * 		function.
 *
 * Errors from setitimer are not expected and are handled using Assert (as in
 * PGSempahoreLockTimed).
 */
void
restoreTimers(struct itimers *timers)
{
	int			err;

	if (timers == NULL)
	{
		/* Coding error! */
		elog(FATAL, "Old timer values not provided");
	}

	/*
	 * Restore any active timers.
	 */
	if (timerisset(&timers->rtimer.it_interval) || timerisset(&timers->rtimer.it_value))
	{
		err = setitimer(ITIMER_REAL, &timers->rtimer, NULL);
		Assert(err == 0);
	}
	if (timerisset(&timers->vtimer.it_interval) || timerisset(&timers->vtimer.it_value))
	{
		err = setitimer(ITIMER_VIRTUAL, &timers->vtimer, NULL);
		Assert(err == 0);
	}
	if (timerisset(&timers->ptimer.it_interval) || timerisset(&timers->ptimer.it_value))
	{
		err = setitimer(ITIMER_PROF, &timers->ptimer, NULL);
		Assert(err == 0);
	}
}
