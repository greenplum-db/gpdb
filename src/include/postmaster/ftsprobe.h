/*-------------------------------------------------------------------------
 *
 * ftsprobe.h
 *	  Interface for fault tolerance service Sender.
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/ftsprobe.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FTSPROBE_H
#define FTSPROBE_H

#include "cdb/ml_ipc.h" /* gettime_elapsed_ms */

#define PROBE_ERR_MSG_LEN   (256)        /* length of error message for errno */

struct pg_conn; /* PGconn ... #include "gp-libpq-fe.h" */
typedef struct ProbeConnectionInfo
{
	int16 dbId;                          /* the dbid of the segment */
	int16 segmentId;                     /* content indicator: -1 for master, 0, ..., n-1 for segments */
	char role;                           /* primary ('p'), mirror ('m') */
	char mode;                           /* sync ('s'), resync ('r'), change-tracking ('c') */
	GpMonotonicTime startTime;           /* probe start timestamp */
#ifdef USE_SEGWALREP
	probe_result *result;
#endif
	char segmentStatus;                  /* probed segment status */
	int16 probe_errno;                   /* saved errno from the latest system call */
	char errmsg[PROBE_ERR_MSG_LEN];      /* message returned by strerror() */
	struct pg_conn *conn;                        /* libpq connection object */
} ProbeConnectionInfo;

extern
#ifdef USE_SEGWALREP
void
#else
char
#endif
probeSegmentHelper(CdbComponentDatabaseInfo *dbInfo, ProbeConnectionInfo probeInfo);

#endif
