/*
 * gptime.c
 *   define time-related functions used in GPDB
 * 
 * Portions Copyright (c), EMC DCD (Greenplum)
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *   src/timezone/gptime.c
 */
#include "c.h"

#include <sys/time.h>

#ifdef HAVE_LIBRT
#include <time.h>
#endif

#include "pgtime.h"

/*
 * gp_set_monotonic_begin_time: set the beginTime and endTime to the current
 * time.
 */
void
gp_set_monotonic_begin_time(GpMonotonicTime *time)
{
	time->beginTime.tv_sec = 0;
	time->beginTime.tv_usec = 0;
	time->endTime.tv_sec = 0;
	time->endTime.tv_usec = 0;
	
	gp_get_monotonic_time(time);

	time->beginTime.tv_sec = time->endTime.tv_sec;
	time->beginTime.tv_usec = time->endTime.tv_usec;
}


/*
 * gp_get_monotonic_time
 *    This function returns the time in the monotonic order.
 *
 * The new time is stored in time->endTime, which has a larger value than
 * the original value. The original endTime is lost.
 *
 * This function is intended for computing elapsed time between two
 * calls. It is not for getting the system time.
 */
void
gp_get_monotonic_time(GpMonotonicTime *time)
{
	struct timeval newTime;
	int status;

#if HAVE_LIBRT
	/* Use clock_gettime to return monotonic time value. */
	struct timespec ts;
	status = clock_gettime(CLOCK_MONOTONIC, &ts);

	newTime.tv_sec = ts.tv_sec;
	newTime.tv_usec = ts.tv_nsec / 1000;

#else

	gettimeofday(&newTime, NULL);
	status = 0; /* gettimeofday always succeeds. */

#endif

	if (status == 0 &&
		timeCmp(&time->endTime, &newTime) < 0)
	{
		time->endTime.tv_sec = newTime.tv_sec;
		time->endTime.tv_usec = newTime.tv_usec;
	}

	else
	{
		time->endTime.tv_usec = time->endTime.tv_usec + 1;

		time->endTime.tv_sec = time->endTime.tv_sec +
			(time->endTime.tv_usec / USECS_PER_SECOND);
		time->endTime.tv_usec = time->endTime.tv_usec % USECS_PER_SECOND;
	}
}
