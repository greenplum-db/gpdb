/*-------------------------------------------------------------------------
 *
 * pgtime.h
 *	  PostgreSQL internal timezone library
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/include/pgtime.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PGTIME_H
#define _PGTIME_H

#include <sys/time.h>

#define USECS_PER_SECOND 1000000
#define MSECS_PER_SECOND 1000


/*
 * The API of this library is generally similar to the corresponding
 * C library functions, except that we use pg_time_t which (we hope) is
 * 64 bits wide, and which is most definitely signed not unsigned.
 */

typedef int64 pg_time_t;

struct pg_tm
{
	int			tm_sec;
	int			tm_min;
	int			tm_hour;
	int			tm_mday;
	int			tm_mon;			/* origin 0, not 1 */
	int			tm_year;		/* relative to 1900 */
	int			tm_wday;
	int			tm_yday;
	int			tm_isdst;
	long int	tm_gmtoff;
	const char *tm_zone;
};

typedef struct pg_tz pg_tz;
typedef struct pg_tzenum pg_tzenum;

/* Maximum length of a timezone name (not including trailing null) */
#define TZ_STRLEN_MAX 255

/* these functions are in localtime.c */

extern struct pg_tm *pg_localtime(const pg_time_t *timep, const pg_tz *tz);
extern struct pg_tm *pg_gmtime(const pg_time_t *timep);
extern int pg_next_dst_boundary(const pg_time_t *timep,
					 long int *before_gmtoff,
					 int *before_isdst,
					 pg_time_t *boundary,
					 long int *after_gmtoff,
					 int *after_isdst,
					 const pg_tz *tz);
extern bool pg_interpret_timezone_abbrev(const char *abbrev,
                             const pg_time_t *timep,
                             long int *gmtoff,
                             int *isdst,
                             const pg_tz *tz);
extern size_t pg_strftime(char *s, size_t max, const char *format,
						  const struct pg_tm * tm);
extern bool pg_get_timezone_offset(const pg_tz *tz, long int *gmtoff);
extern const char *pg_get_timezone_name(pg_tz *tz);
extern bool pg_tz_acceptable(pg_tz *tz);

/* these functions are in strftime.c */

extern size_t pg_strftime(char *s, size_t max, const char *format,
			const struct pg_tm * tm);

/* these functions and variables are in pgtz.c */

extern PGDLLIMPORT pg_tz *session_timezone;
extern pg_tz *log_timezone;

extern void pg_timezone_initialize(void);
extern pg_tz *pg_tzset(const char *tzname);
extern pg_tz *pg_tzset_offset(long gmtoffset);

extern pg_tzenum *pg_tzenumerate_start(void);
extern pg_tz *pg_tzenumerate_next(pg_tzenum *dir);
extern void pg_tzenumerate_end(pg_tzenum *dir);

/*
 * GpMonotonicTime: used to guarantee that the elapsed time is in
 * the monotonic order between two gp_get_monotonic_time calls.
 */
typedef struct GpMonotonicTime
{
	struct timeval beginTime;
	struct timeval endTime;
} GpMonotonicTime;

/*
 * gp_set_monotonic_begin_time: set the beginTime to the current
 * time.
 */
void gp_set_monotonic_begin_time(GpMonotonicTime *time);

/*
 * Compare two times.
 *
 * If t1 > t2, return 1.
 * If t1 == t2, return 0.
 * If t1 < t2, return -1;
 */
static inline int
timeCmp(struct timeval *t1, struct timeval *t2)
{
	if (t1->tv_sec == t2->tv_sec &&
		t1->tv_usec == t2->tv_usec)
		return 0;
	
	if (t1->tv_sec > t2->tv_sec ||
		(t1->tv_sec == t2->tv_sec &&
		 t1->tv_usec > t2->tv_usec))
		return 1;
	
	return -1;
}

/*
 * gp_get_monotonic_time
 *    This function returns the time in the monotonic order.
 *
 * The new time is stored in time->endTime, which has a larger value than
 * anything stored there when this function is called. The original
 * endTime is copied to beginTime, and the original beginTime will be
 * overwritten.
 *
 * This function is intended for computing elapsed time between two
 * calls. It is not for getting the system time.
 */
extern void gp_get_monotonic_time(GpMonotonicTime *time);

/*
 * gp_get_elapsed_us -- return the elapsed time in microseconds
 * after the given time->beginTime.
 *
 * If time->beginTime is not set (0), then return 0.
 *
 * Note that the beginTime is not changed, but the endTime is set
 * to the current time.
 */
static inline uint64
gp_get_elapsed_us(GpMonotonicTime *time)
{
	if (time->beginTime.tv_sec == 0 &&
		time->beginTime.tv_usec == 0)
		return 0;

	gp_get_monotonic_time(time);

	return ((time->endTime.tv_sec - time->beginTime.tv_sec) * USECS_PER_SECOND +
			(time->endTime.tv_usec - time->beginTime.tv_usec));
}

static inline uint64
gp_get_elapsed_ms(GpMonotonicTime *time)
{
    return gp_get_elapsed_us(time) / (USECS_PER_SECOND / MSECS_PER_SECOND);
}

#endif   /* _PGTIME_H */
