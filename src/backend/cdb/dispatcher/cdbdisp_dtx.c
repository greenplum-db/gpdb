/*-------------------------------------------------------------------------
 *
 * cdbdisp_dtx.c
 *	  Functions to dispatch DTX commands to QExecutors.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdisp_dtx.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/xact.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_dtx.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbgang.h"

#include "storage/procarray.h"	/* updateSharedLocalSnapshot */
#include "utils/snapmgr.h"

/*
 * Parameter structure for DTX protocol commands
 */
typedef struct DispatchCommandDtxProtocolParms
{
	DtxProtocolCommand dtxProtocolCommand;
	char	   *dtxProtocolCommandLoggingStr;
	char		gid[TMGIDSIZE];
	char	   *serializedDtxContextInfo;
	int			serializedDtxContextInfoLen;
} DispatchCommandDtxProtocolParms;

/*
 * STATIC STATE VARIABLES should not be declared!
 * global state will break the ability to run cursors.
 * only globals with a higher granularity than a running
 * command (i.e: transaction, session) are ok.
 */
static DtxContextInfo TempQDDtxContextInfo = DtxContextInfo_StaticInit;

static char *buildGpDtxProtocolCommand(DispatchCommandDtxProtocolParms *pDtxProtocolParms,
						  int *finalLen);

/*
 * CdbDispatchDtxProtocolCommand:
 * Sends a non-cancelable command to all segment dbs
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
							  char *dtxProtocolCommandLoggingStr,
							  char *gid,
							  ErrorData **qeError,
							  int *numresults,
							  List *dtxSegments,
							  char *serializedDtxContextInfo,
							  int serializedDtxContextInfoLen)
{
	CdbDispatcherState *ds;
	CdbDispatchResults *pr;
	CdbPgResults cdb_pgresults = {NULL, 0};

	DispatchCommandDtxProtocolParms dtxProtocolParms;
	Gang	   *primaryGang;
	char	   *queryText = NULL;
	int			queryTextLen = 0;

	*qeError = NULL;

	MemSet(&dtxProtocolParms, 0, sizeof(dtxProtocolParms));
	dtxProtocolParms.dtxProtocolCommand = dtxProtocolCommand;
	dtxProtocolParms.dtxProtocolCommandLoggingStr = dtxProtocolCommandLoggingStr;
	if (strlen(gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)", (int) strlen(gid));
	memcpy(dtxProtocolParms.gid, gid, TMGIDSIZE);
	dtxProtocolParms.serializedDtxContextInfo = serializedDtxContextInfo;
	dtxProtocolParms.serializedDtxContextInfoLen = serializedDtxContextInfoLen;

	/*
	 * Dispatch the command.
	 */
	ds = cdbdisp_makeDispatcherState(false);

	queryText = buildGpDtxProtocolCommand(&dtxProtocolParms, &queryTextLen);

	primaryGang = AllocateGang(ds, GANGTYPE_PRIMARY_WRITER, dtxSegments);

	Assert(primaryGang);

	cdbdisp_makeDispatchResults(ds, 1, false);
	cdbdisp_makeDispatchParams(ds, 1, queryText, queryTextLen);

	cdbdisp_dispatchToGang(ds, primaryGang, -1);
	addToGxactDtxSegments(primaryGang);

	cdbdisp_waitDispatchFinish(ds);

	cdbdisp_checkDispatchResult(ds, DISPATCH_WAIT_NONE);

	pr = cdbdisp_getDispatchResults(ds, qeError);

	if (!pr)
	{
		cdbdisp_destroyDispatcherState(ds);
		return NULL;
	}

	cdbdisp_returnResults(pr, &cdb_pgresults);

	cdbdisp_destroyDispatcherState(ds);

	*numresults = cdb_pgresults.numResults;
	return cdb_pgresults.pg_results;
}

char *
qdSerializeDtxContextInfo(int *size, bool wantSnapshot, bool inCursor,
						  int txnOptions, char *debugCaller)
{
	char	   *serializedDtxContextInfo;

	Snapshot	snapshot = NULL;
	int			serializedLen;
	DtxContextInfo *pDtxContextInfo = NULL;
	DtxContext currentDistributedTransactionContext = DistributedTransactionContext;

	/*
	 * If 'wantSnapshot' is set, then serialize the ActiveSnapshot. The
	 * caller better have ActiveSnapshot set.
	 */
	*size = 0;

	if (wantSnapshot)
	{
		if (!ActiveSnapshotSet())
			elog(ERROR, "could not serialize current snapshot, ActiveSnapshot not set");
		snapshot = GetActiveSnapshot();
	}

	switch (currentDistributedTransactionContext)
	{
		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		case DTX_CONTEXT_LOCAL_ONLY:
			DtxContextInfo_CreateOnMaster(&TempQDDtxContextInfo, inCursor,
										  txnOptions, snapshot);

			if (DistributedTransactionContext ==
				DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE && snapshot != NULL)
			{
				updateSharedLocalSnapshot(
					&TempQDDtxContextInfo,
					currentDistributedTransactionContext,
					snapshot,
					"qdSerializeDtxContextInfo");
			}

			pDtxContextInfo = &TempQDDtxContextInfo;
			break;

		case DTX_CONTEXT_QD_RETRY_PHASE_2:
		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
		case DTX_CONTEXT_QE_READER:
		case DTX_CONTEXT_QE_PREPARED:
		case DTX_CONTEXT_QE_FINISH_PREPARED:
			elog(FATAL, "Unexpected distribute transaction context: '%s'",
				 DtxContextToString(DistributedTransactionContext));
			break;

		default:
			elog(FATAL, "Unrecognized DTX transaction context: %d",
				 (int) DistributedTransactionContext);
	}

	serializedLen = DtxContextInfo_SerializeSize(pDtxContextInfo);
	Assert(serializedLen > 0);

	*size = serializedLen;
	serializedDtxContextInfo = palloc(*size);

	DtxContextInfo_Serialize(serializedDtxContextInfo, pDtxContextInfo);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "qdSerializeDtxContextInfo (called by %s) returning a snapshot of %d bytes (ptr is %s)",
		 debugCaller, *size,
		 (serializedDtxContextInfo != NULL ? "Non-NULL" : "NULL"));
	return serializedDtxContextInfo;
}

/*
 * Build a dtx protocol command string to be dispatched to QE.
 */
static char *
buildGpDtxProtocolCommand(DispatchCommandDtxProtocolParms *pDtxProtocolParms,
						 int *finalLen)
{
	int			dtxProtocolCommand = (int) pDtxProtocolParms->dtxProtocolCommand;
	char	   *dtxProtocolCommandLoggingStr = pDtxProtocolParms->dtxProtocolCommandLoggingStr;
	char	   *gid = pDtxProtocolParms->gid;
	char	   *serializedDtxContextInfo = pDtxProtocolParms->serializedDtxContextInfo;
	int			serializedDtxContextInfoLen = pDtxProtocolParms->serializedDtxContextInfoLen;
	int			tmp = 0;
	int			len = 0;

	int			loggingStrLen = strlen(dtxProtocolCommandLoggingStr) + 1;
	int			gidLen = strlen(gid) + 1;
	int			total_query_len = 1 /* 'T' */ +
	sizeof(len) +
	sizeof(dtxProtocolCommand) +
	sizeof(loggingStrLen) +
	loggingStrLen +
	sizeof(gidLen) +
	gidLen +
	sizeof(serializedDtxContextInfoLen) +
	serializedDtxContextInfoLen;

	char	   *shared_query = NULL;
	char	   *pos = NULL;

	/* Allocate query text within DispatcherContext */
	Assert(DispatcherContext);
	MemoryContext oldContext = MemoryContextSwitchTo(DispatcherContext);
	shared_query = palloc0(total_query_len);
	pos = shared_query;

	*pos++ = 'T';

	pos += sizeof(len);			/* placeholder for message length */

	tmp = htonl(dtxProtocolCommand);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

	tmp = htonl(loggingStrLen);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

	memcpy(pos, dtxProtocolCommandLoggingStr, loggingStrLen);
	pos += loggingStrLen;

	tmp = htonl(gidLen);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

	memcpy(pos, gid, gidLen);
	pos += gidLen;

	tmp = htonl(serializedDtxContextInfoLen);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

	if (serializedDtxContextInfoLen > 0)
	{
		memcpy(pos, serializedDtxContextInfo, serializedDtxContextInfoLen);
		pos += serializedDtxContextInfoLen;
	}

	len = pos - shared_query - 1;

	/*
	 * fill in length placeholder
	 */
	tmp = htonl(len);
	memcpy(shared_query + 1, &tmp, sizeof(tmp));

	if (finalLen)
		*finalLen = len + 1;

	MemoryContextSwitchTo(oldContext);
	return shared_query;
}
