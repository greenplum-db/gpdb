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

#include "postgres.h"

/* GUCs */
extern int gp_perfmon_segment_interval;

/* Interface */
extern int perfmon_segmentinfo_start(void);

#endif /* PERFMON_SEGMENTINFO_H */
