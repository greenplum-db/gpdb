/*-------------------------------------------------------------------------
 *
 * testutils.h
 *		Collection of testing utilities.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/testutils.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TESTUTILS_H_
#define TESTUTILS_H_

#ifdef USE_TEST_UTILS

#define CHECK_TIME_SLICE()      TimeSliceCheck(__FILE__, __LINE__)

/* external variables */
extern PGDLLIMPORT volatile int32 InterruptHoldoffCount;
extern PGDLLIMPORT volatile int32 CritSectionCount;

/* time slicing */
extern void TimeSliceReset(void);
extern void TimeSliceCheck(const char *file, int line);

#if defined(__i386) || defined(__x86_64__)

#define USE_TEST_UTILS_X86   1

/* detect database-lightweight lock conflict */
extern void LWLockHeldDetect(const void *locktag, int lockmode);

#endif /* defined(__i386) || defined(__x86_64__) */

/* GUCs */
extern bool gp_test_time_slice;          /* session GUC, controls time slice violation checking */
extern int  gp_test_time_slice_interval; /* session GUC, sets time slice interval in ms */
extern int  gp_test_time_slice_report_level;
                                         /* session GUC, sets level of violation report messages */
extern bool gp_test_deadlock_hazard;     /* session GUC, controls database-lightweight lock conflict detection */
extern int  gp_test_deadlock_hazard_report_level;
                                         /* session GUC, sets level of hazard report messages */

#endif /* USE_TEST_UTILS */

#endif /* TESTUTILS_H_ */


/* EOF */
