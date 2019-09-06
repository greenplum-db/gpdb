/*-------------------------------------------------------------------------
 *
 * perfmon_segmentinfo.h
 *	  Definitions for segment info sender process.
 *
 * This file contains the basic interface that is needed by postmaster
 * to start the segment info sender process.
 *
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/perfmon_segmentinfo.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PERFMON_SEGMENTINFO_H
#define PERFMON_SEGMENTINFO_H

/* GUCs */
extern int gp_perfmon_segment_interval;

/* Interface */
void SegmentInfoSenderMain(Datum main_arg);
bool SegmentInfoSenderStartRule(Datum main_arg);

typedef void (*cluster_state_collect_hook_type)(void);
extern PGDLLIMPORT cluster_state_collect_hook_type cluster_state_collect_hook;

#define SEGMENT_INFO_LOOP_SLEEP_MS (100)

#endif /* PERFMON_SEGMENTINFO_H */
