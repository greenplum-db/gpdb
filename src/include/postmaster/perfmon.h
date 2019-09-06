/*-------------------------------------------------------------------------
 *
 * perfmon.h
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/perfmon.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PERFMON_H
#define PERFMON_H

extern bool PerfmonStartRule(Datum main_arg);

extern void PerfmonMain(Datum main_arg);

#endif   /* PERFMON_H */
