/*-------------------------------------------------------------------------
 *
 * cdbdisp_dtx.h
 * routines for dispatching DTX commands to the qExec processes.
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_DTX_H
#define CDBDISP_DTX_H

#include "lib/stringinfo.h"         /* StringInfo */
#include "cdb/cdbtm.h"

struct pg_result;                   /* #include "gp-libpq-fe.h" */
struct CdbPgResults;
/*
 * cdbdisp_dispatchDtxProtocolCommand:
 * Sends a non-cancelable command to all segment dbs, primary
 *
 * Returns a malloc'ed array containing the PGresult objects thus
 * produced; the caller must PQclear() them and free() the array.
 * A NULL entry follows the last used entry in the array.
 *
 * Any error messages - whether or not they are associated with
 * PGresult objects - are appended to a StringInfo buffer provided
 * by the caller.
 */
struct pg_result **
cdbdisp_dispatchDtxProtocolCommand(DtxProtocolCommand dtxProtocolCommand,
								   int flags,
								   char	*dtxProtocolCommandLoggingStr,
								   char	*gid,
								   DistributedTransactionId	gxid,
								   StringInfo errmsgbuf,
								   int *resultCount,
								   bool* badGangs,
								   CdbDispatchDirectDesc *direct,
								   char *argument, int argumentLength );


/*
 * used to take the current Transaction Snapshot and serialized a version of it
 * into the static variable serializedDtxContextInfo
 */
char *
qdSerializeDtxContextInfo(int * size, bool wantSnapshot, bool inCursor, int txnOptions, char *debugCaller);

#endif   /* CDBDISP_DTX_H */
