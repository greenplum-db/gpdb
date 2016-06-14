
/*-------------------------------------------------------------------------
 *
 * cdbdisp_dtx.c
 *	  Functions to dispatch DTX commands to QExecutors.
 *
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_dtx.h"
#include "cdb/cdbdisp_thread.h" /* for CdbDispatchCmdThreads and DispatchCommandParms */
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "utils/palloc.h"

#include "storage/procarray.h"	/* updateSharedLocalSnapshot */
#include "gp-libpq-fe.h"

/*
 * Parameter structure for DTX protocol commands
 */
typedef struct DispatchCommandDtxProtocolParms
{
	DtxProtocolCommand	dtxProtocolCommand;
	int	flags;
	char *dtxProtocolCommandLoggingStr;
	char gid[TMGIDSIZE];
	DistributedTransactionId gxid;
	char *serializedDtxContextInfo;
	int serializedDtxContextInfoLen;
} DispatchCommandDtxProtocolParms;

/*
 * STATIC STATE VARIABLES should not be declared!
 * global state will break the ability to run cursors.
 * only globals with a higher granularity than a running
 * command (i.e: transaction, session) are ok.
 */
static DtxContextInfo TempQDDtxContextInfo = DtxContextInfo_StaticInit;

static void cdbdisp_dtxParmsInit(struct CdbDispatcherState *ds,
								 DispatchCommandDtxProtocolParms *pDtxProtocolParms);

static char *
buildGpDtxProtocolCommand(MemoryContext cxt,
						  DispatchCommandDtxProtocolParms * pDtxProtocolParms,
						  int *finalLen);

/*
 * cdbdisp_dispatchDtxProtocolCommand:
 * Sends a non-cancelable command to all segment dbs
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
								   char *dtxProtocolCommandLoggingStr,
								   char *gid,
								   DistributedTransactionId gxid,
								   StringInfo errmsgbuf,
								   int *numresults,
								   bool *badGangs,
								   CdbDispatchDirectDesc * direct,
								   char *serializedDtxContextInfo,
								   int serializedDtxContextInfoLen)
{
	CdbDispatcherState ds = {NULL, NULL, NULL};

	PGresult  **resultSets = NULL;

	DispatchCommandDtxProtocolParms dtxProtocolParms;
	Gang *primaryGang;
	int	nsegdb = getgpsegmentCount();

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "cdbdisp_dispatchDtxProtocolCommand: %s for gid = %s, direct content #: %d",
		 dtxProtocolCommandLoggingStr, gid,
		 direct->directed_dispatch ? direct->content[0] : -1);

	*badGangs = false;

	MemSet(&dtxProtocolParms, 0, sizeof(dtxProtocolParms));
	dtxProtocolParms.dtxProtocolCommand = dtxProtocolCommand;
	dtxProtocolParms.flags = flags;
	dtxProtocolParms.dtxProtocolCommandLoggingStr =
		dtxProtocolCommandLoggingStr;
	if (strlen(gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)", (int) strlen(gid));
	memcpy(dtxProtocolParms.gid, gid, TMGIDSIZE);
	dtxProtocolParms.gxid = gxid;
	dtxProtocolParms.serializedDtxContextInfo = serializedDtxContextInfo;
	dtxProtocolParms.serializedDtxContextInfoLen = serializedDtxContextInfoLen;

	/*
	 * Allocate a primary QE for every available segDB in the system.
	 */
	primaryGang = allocateWriterGang();

	Assert(primaryGang);

	if (primaryGang->dispatcherActive)
	{
		elog(LOG, "cdbdisp_dispatchDtxProtocolCommand: primary gang marked active re-marking");
		primaryGang->dispatcherActive = false;
	}

	/*
	 * Dispatch the command.
	 */
	cdbdisp_makeDispatcherState(&ds, nsegdb, 0, /* cancelOnError */ false);
	cdbdisp_dtxParmsInit(&ds, &dtxProtocolParms);
	ds.primaryResults->writer_gang = primaryGang;

	cdbdisp_dispatchToGang(&ds, primaryGang, -1, direct);

	/*
	 * Wait for all QEs to finish.	Don't cancel. 
	 */
	CdbCheckDispatchResult(&ds, DISPATCH_WAIT_NONE);

	if (!gangOK(primaryGang))
	{
		*badGangs = true;

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "cdbdisp_dispatchDtxProtocolCommand: Bad gang from dispatch of %s for gid = %s",
			 dtxProtocolCommandLoggingStr, gid);
	}

	resultSets = cdbdisp_returnResults(ds.primaryResults, errmsgbuf, numresults);

	cdbdisp_destroyDispatcherState((struct CdbDispatcherState *) &ds);

	return resultSets;
}

char *
qdSerializeDtxContextInfo(int *size, bool wantSnapshot, bool inCursor,
						  int txnOptions, char *debugCaller)
{
	char *serializedDtxContextInfo;

	Snapshot snapshot;
	int	serializedLen;
	DtxContextInfo *pDtxContextInfo = NULL;

	/*
	 * If we already have a LatestSnapshot set then no reason to try
	 * and get a new one. just use that one. But... there is one important
	 * reason why this HAS to be here. ROLLBACK stmts get dispatched to QEs
	 * in the abort transaction code. This code tears down enough stuff such
	 * that you can't call GetTransactionSnapshot() within that code. So we
	 * need to use the LatestSnapshot since we can't re-gen a new one.
	 * 
	 * It is also very possible that for a single user statement which may
	 * only generate a single snapshot that we will dispatch multiple statements
	 * to our qExecs. Something like:
	 * 
	 *    					  QD			  QEs
	 *    					  |				  |
	 * User SQL Statement --->|		BEGIN	  |
	 *    					  |-------------->|
	 *    					  |		STMT	  |
	 *    					  |-------------->|
	 *    					  |    PREPARE	  |
	 *    					  |-------------->|
	 *    					  |    COMMIT	  |
	 *    					  |-------------->|
	 *    					  |				  |
	 * 
	 * This may seem like a problem because all four of those will dispatch
	 * the same snapshot with the same curcid. But... this is OK because
	 * BEGIN, PREPARE, and COMMIT don't need Snapshots on the QEs.
	 * 
	 * NOTE: This will be a problem if we ever need to dispatch more than one
	 * statement to the qExecs and more than one needs a snapshot!
	 */
	*size = 0;
	snapshot = NULL;

	if (wantSnapshot)
	{

		if (LatestSnapshot == NULL &&
			SerializableSnapshot == NULL && !IsAbortInProgress())
		{
			/*
			 * unfortunately, the dtm issues a select for prepared xacts at the
			 * beginning and this is before a snapshot has been set up.  so we need
			 * one for that but not for when we dont have a valid XID.
			 * 
			 * but we CANT do this if an ABORT is in progress... instead we'll send
			 * a NONE since the qExecs dont need the information to do a ROLLBACK.
			 */
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "qdSerializeDtxContextInfo calling GetTransactionSnapshot to make snapshot");

			GetTransactionSnapshot();
		}

		if (LatestSnapshot != NULL)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "qdSerializeDtxContextInfo using LatestSnapshot");

			snapshot = LatestSnapshot;
			elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				 "[Distributed Snapshot #%u] *QD Use Latest* currcid = %d (gxid = %u, '%s')",
				 LatestSnapshot->distribSnapshotWithLocalMapping.header.
				 distribSnapshotId, LatestSnapshot->curcid,
				 getDistributedTransactionId(),
				 DtxContextToString(DistributedTransactionContext));
		}
		else if (SerializableSnapshot != NULL)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "qdSerializeDtxContextInfo using SerializableSnapshot");

			snapshot = SerializableSnapshot;
			elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				 "[Distributed Snapshot #%u] *QD Use Serializable* currcid = %d (gxid = %u, '%s')",
				 SerializableSnapshot->distribSnapshotWithLocalMapping.header.
				 distribSnapshotId, SerializableSnapshot->curcid,
				 getDistributedTransactionId(),
				 DtxContextToString(DistributedTransactionContext));

		}
	}

	switch (DistributedTransactionContext)
	{
		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
		case DTX_CONTEXT_LOCAL_ONLY:
			if (snapshot != NULL)
			{
				DtxContextInfo_CreateOnMaster(&TempQDDtxContextInfo,
											  &snapshot->
											  distribSnapshotWithLocalMapping,
											  snapshot->curcid, txnOptions);
			}
			else
			{
				DtxContextInfo_CreateOnMaster(&TempQDDtxContextInfo,
											  NULL, 0, txnOptions);
			}

			TempQDDtxContextInfo.cursorContext = inCursor;

			if (DistributedTransactionContext ==
				DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE && snapshot != NULL)
			{
				updateSharedLocalSnapshot(&TempQDDtxContextInfo, snapshot,
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
 * Initialize CdbDispatcherState using DispatchCommandDtxProtocolParms
 *
 * Allocate query text in memory context, initialize it and assign it to
 * all DispatchCommandQueryParms in this dispatcher state.
 */
static void
cdbdisp_dtxParmsInit(struct CdbDispatcherState *ds,
					 DispatchCommandDtxProtocolParms * pDtxProtocolParms)
{
	CdbDispatchCmdThreads *dThreads = ds->dispatchThreads;
	int	i = 0;
	int	len = 0;
	DispatchCommandParms *pParms = NULL;
	MemoryContext oldContext = NULL;

	Assert(pDtxProtocolParms->dtxProtocolCommandLoggingStr != NULL);
	Assert(pDtxProtocolParms->gid != NULL);

	oldContext = MemoryContextSwitchTo(ds->dispatchStateContext);

	char *queryText = buildGpDtxProtocolCommand(ds->dispatchStateContext, pDtxProtocolParms, &len);

	MemoryContextSwitchTo(oldContext);

	for (i = 0; i < dThreads->dispatchCommandParmsArSize; i++)
	{
		pParms = &dThreads->dispatchCommandParmsAr[i];
		pParms->query_text = queryText;
		pParms->query_text_len = len;
	}
}

/*
 * Build a dtx protocol command string to be dispatched to QE.
 */
static char *
buildGpDtxProtocolCommand(MemoryContext cxt,
						  DispatchCommandDtxProtocolParms * pDtxProtocolParms,
						  int *finalLen)
{
	int	dtxProtocolCommand = (int) pDtxProtocolParms->dtxProtocolCommand;
	int	flags = pDtxProtocolParms->flags;
	char *dtxProtocolCommandLoggingStr = pDtxProtocolParms->dtxProtocolCommandLoggingStr;
	char *gid = pDtxProtocolParms->gid;
	int	gxid = pDtxProtocolParms->gxid;
	char *serializedDtxContextInfo = pDtxProtocolParms->serializedDtxContextInfo;
	int	serializedDtxContextInfoLen = pDtxProtocolParms->serializedDtxContextInfoLen;
	int	tmp = 0;
	int	len = 0;

	int	loggingStrLen = strlen(dtxProtocolCommandLoggingStr) + 1;
	int	gidLen = strlen(gid) + 1;
	int	total_query_len = 1 /* 'T' */ +
		sizeof(len) +
		sizeof(dtxProtocolCommand) +
		sizeof(flags) +
		sizeof(loggingStrLen) +
		loggingStrLen +
		sizeof(gidLen) +
		gidLen +
		sizeof(gxid) +
		sizeof(serializedDtxContextInfoLen) +
		serializedDtxContextInfoLen;

	char *shared_query = MemoryContextAlloc(cxt, total_query_len);
	char *pos = shared_query;

	*pos++ = 'T';

	pos += sizeof(len); /* placeholder for message length */

	tmp = htonl(dtxProtocolCommand);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

	tmp = htonl(flags);
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

	tmp = htonl(gxid);
	memcpy(pos, &tmp, sizeof(tmp));
	pos += sizeof(tmp);

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

	return shared_query;
}
