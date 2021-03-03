/*-------------------------------------------------------------------------
 *
 * cdbdisp_dtx.h
 * routines for dispatching DTX commands to the qExec processes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdisp_dtx.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_DTX_H
#define CDBDISP_DTX_H

#include "cdb/cdbtm.h"

struct pg_result;                   /* #include "libpq-fe.h" */
struct CdbPgResults;
/*
 * CdbDispatchDtxProtocolCommand:
 * Sends a non-cancelable command to all segment dbs, primary
 *
 * Returns a malloc'ed array containing the PGresult objects thus
 * produced; the caller must PQclear() them and free() the array.
 * A NULL entry follows the last used entry in the array.
 *
 * Any error message - whether or not it is associated with an
 * PGresult object - is returned in *qeError.
 */
struct pg_result **
CdbDispatchDtxProtocolCommand(DtxProtocolCommand dtxProtocolCommand,
							  char	*dtxProtocolCommandLoggingStr,
							  char	*gid,
							  ErrorData **qeError,
							  int *resultCount,
							  List *dtxSegments,
							  char *serializedDtxContextInfo,
							  int serializedDtxContextInfoLen);


/*
 * used to take the current Transaction Snapshot and serialized a version of it
 * into the static variable serializedDtxContextInfo
 */
char *
qdSerializeDtxContextInfo(int * size, bool wantSnapshot, bool inCursor, int txnOptions, char *debugCaller);

#endif   /* CDBDISP_DTX_H */
