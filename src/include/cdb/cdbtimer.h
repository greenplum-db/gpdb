/*-------------------------------------------------------------------------
 *
 * cdbtimer.h
 *	  Functions to manipulate timers used in a backend.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbtimer.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBTIMER_H_
#define CDBTIMER_H_

#include <sys/time.h>

typedef struct itimers {
	struct itimerval rtimer;		/* ITIMER_REAL */
	struct itimerval vtimer;		/* ITIMER_VIRTUAL */
	struct itimerval ptimer;		/* ITIMER_PROF */
} itimers;

void resetTimers(struct itimers *timers);
void restoreTimers(struct itimers *timers);

#endif /* CDBTIMER_H_ */
