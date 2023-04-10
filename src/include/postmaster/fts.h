/*-------------------------------------------------------------------------
 *
 * fts.h
 *	  Interface for fault tolerance service (FTS).
 *
 * Portions Copyright (c) 2005-2010, Greenplum Inc.
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *		src/include/postmaster/fts.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FTS_H
#define FTS_H

#include "utils/guc.h"
#include "cdb/cdbutil.h"


/* Queries for FTS messages */
#define	FTS_MSG_PROBE "PROBE"
#define FTS_MSG_SYNCREP_OFF "SYNCREP_OFF"
#define FTS_MSG_PROMOTE "PROMOTE"

/*
 * This is used for constructing string to store the full fts message request
 * string from QD to QE. Format for which is defined using FTS_MSG_FORMAT and
 * first part of it string to define type of fts message like FTS_MSG_PROBE,
 * FTS_MSG_SYNCREP_OFF or FTS_MSG_PROMOTE.
 */
#define FTS_MSG_MAX_LEN 100

/*
 * If altering the fts message format, consider if FTS_MSG_MAX_LEN is enough
 * to store the modified format.
 */
#define FTS_MSG_FORMAT "%s dbid=%d contid=%d"

#define Natts_fts_message_response 5
#define Anum_fts_message_response_is_mirror_up 0
#define Anum_fts_message_response_is_in_sync 1
#define Anum_fts_message_response_is_syncrep_enabled 2
#define Anum_fts_message_response_is_role_mirror 3
#define Anum_fts_message_response_request_retry 4

#define FTS_MESSAGE_RESPONSE_NTUPLES 1

typedef struct FtsResponse
{
	bool IsMirrorUp;
	bool IsInSync;
	bool IsSyncRepEnabled;
	bool IsRoleMirror;
	bool RequestRetry;
} FtsResponse;

extern bool am_ftsprobe;
extern bool am_ftshandler;
extern bool am_mirror;

/* buffer size for SQL command */
#define SQL_CMD_BUF_SIZE     1024

/*
 * Interface for checking if FTS is active
 */
extern bool FtsIsActive(void);

/*
 * Interface for WALREP specific checking
 */
extern void HandleFtsMessage(const char* query_string);
extern void probeWalRepUpdateConfig(int16 dbid, int16 segindex, char role,
									bool IsSegmentAlive, bool IsInSync);

extern bool FtsProbeStartRule(Datum main_arg);
extern void FtsProbeMain (Datum main_arg);
extern pid_t FtsProbePID(void);

#endif   /* FTS_H */
