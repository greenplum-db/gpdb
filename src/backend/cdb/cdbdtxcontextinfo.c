/*-------------------------------------------------------------------------
 *
 * cdbdtxcontextinfo.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbdtxcontextinfo.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbdistributedsnapshot.h"
#include "cdb/cdblocaldistribxact.h"
#include "cdb/cdbdtxcontextinfo.h"
#include "miscadmin.h"
#include "access/transam.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbtm.h"
#include "access/xact.h"
#include "utils/guc.h"
#include "utils/session_state.h"
#include "utils/tqual.h"

/*
 * process local cache used to identify "dispatch units"
 *
 * Note, this is only required because the dispatcher emits multiple statements (which will
 * correspond to multiple local-xids on the segments) under the same distributed-xid.
 *
 */
static uint32 syncCount = 1;

void
DtxContextInfo_CreateOnMaster(DtxContextInfo *dtxContextInfo, bool inCursor,
							  int txnOptions, Snapshot snapshot)
{
	int			i;
	CommandId	curcid = 0;

	if (snapshot)
		curcid = snapshot->curcid;

	DtxContextInfo_Reset(dtxContextInfo);

	dtxContextInfo->distributedXid = getDistributedTransactionId();
	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		dtxContextInfo->distributedTimeStamp = getDtxStartTime();

		getDistributedTransactionIdentifier(dtxContextInfo->distributedId);
		dtxContextInfo->curcid = curcid;
	}

	/*
	 * When this is an extended query, all the dispatchs will go to
	 * the reader gangs, don't increase 'syncCount' so that all the
	 * dispatch could share the same snapshot created by 'gp_write_shared_snapshot'.
	 */
	dtxContextInfo->segmateSync = inCursor ? syncCount : ++syncCount;
	if (dtxContextInfo->segmateSync == (~(uint32)0))
		ereport(FATAL,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("cannot have more than 2^32-2 commands in a session")));

	AssertImply(inCursor,
				dtxContextInfo->distributedXid != InvalidDistributedTransactionId &&
				gp_command_count == MySessionState->latestCursorCommandId);

	dtxContextInfo->cursorContext = inCursor;
	dtxContextInfo->nestingLevel = GetCurrentTransactionNestLevel();

	elogif(Debug_print_full_dtm, LOG,
		   "DtxContextInfo_CreateOnMaster: created dtxcontext with dxid %u nestingLevel %d segmateSync %u/%u (current/cached)",
		   dtxContextInfo->distributedXid, dtxContextInfo->nestingLevel,
		   dtxContextInfo->segmateSync, syncCount);

	dtxContextInfo->haveDistributedSnapshot = false;
	if (snapshot && snapshot->haveDistribSnapshot)
	{
		DistributedSnapshot_Copy(&dtxContextInfo->distributedSnapshot,
								 &snapshot->distribSnapshotWithLocalMapping.ds);
		dtxContextInfo->haveDistributedSnapshot = true;
	}

	dtxContextInfo->distributedTxnOptions = txnOptions;

	if (Debug_print_full_dtm)
	{
		char		gid[TMGIDSIZE];
		DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

		if (!getDistributedTransactionIdentifier(gid))
			memcpy(gid, "<empty>", 8);

		elog(LOG,
			 "DtxContextInfo_CreateOnMaster Gp_role is DISPATCH and have gid = %s, gxid = %u --> have distributed snapshot",
			 gid,
			 getDistributedTransactionId());
		elog(LOG,
			 "DtxContextInfo_CreateOnMaster distributedXid = %u, "
			 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount %d)",
			 dtxContextInfo->distributedXid,
			 ds->xminAllDistributedSnapshots,
			 ds->xmin,
			 ds->xmax,
			 ds->count,
			 ds->maxCount);

		for (i = 0; i < ds->count; i++)
		{
			elog(LOG, "....    distributedSnapshotData->xip[%d] = %u",
				 i, ds->inProgressXidArray[i]);
		}
		elog(LOG,
			 "DtxContextInfo_CreateOnMaster curcid = %u",
			 dtxContextInfo->curcid);

		elog(LOG,
			 "DtxContextInfo_CreateOnMaster txnOptions = 0x%x, needDtx = %s, explicitBegin = %s, isoLevel = %s, readOnly = %s.",
			 txnOptions,
			 (isMppTxOptions_NeedDtx(txnOptions) ? "true" : "false"),
			 (isMppTxOptions_ExplicitBegin(txnOptions) ? "true" : "false"),
			 IsoLevelAsUpperString(mppTxOptions_IsoLevel(txnOptions)),
			 (isMppTxOptions_ReadOnly(txnOptions) ? "true" : "false"));
	}
}

int
DtxContextInfo_SerializeSize(DtxContextInfo *dtxContextInfo)
{
	int			size = 0;

	size += sizeof(DistributedTransactionId);	/* distributedXid */

	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		size += sizeof(DistributedTransactionTimeStamp);
		size += TMGIDSIZE;		/* distributedId */
		size += sizeof(CommandId);	/* curcid */
	}

	size += sizeof(uint32);		/* segmateSync */
	size += sizeof(uint32);		/* nestingLevel */
	size += sizeof(bool);		/* haveDistributedSnapshot */
	size += sizeof(bool);		/* cursorContext */

	if (dtxContextInfo->haveDistributedSnapshot)
	{
		size += DistributedSnapshot_SerializeSize(
												  &dtxContextInfo->distributedSnapshot);
	}

	size += sizeof(int);		/* distributedTxnOptions */

	elogif(Debug_print_full_dtm, LOG,
		   "DtxContextInfo_SerializeSize is returning size = %d", size);

	return size;
}

void
DtxContextInfo_Serialize(char *buffer, DtxContextInfo *dtxContextInfo)
{
	char	   *p = buffer;
	int			i;
	int			used;
	DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

	memcpy(p, &dtxContextInfo->distributedXid, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		memcpy(p, &dtxContextInfo->distributedTimeStamp, sizeof(DistributedTransactionTimeStamp));
		p += sizeof(DistributedTransactionTimeStamp);
		if (strlen(dtxContextInfo->distributedId) >= TMGIDSIZE)
			elog(PANIC, "Distribute transaction identifier too long (%d)",
				 (int) strlen(dtxContextInfo->distributedId));
		memcpy(p, dtxContextInfo->distributedId, TMGIDSIZE);
		p += TMGIDSIZE;
		memcpy(p, &dtxContextInfo->curcid, sizeof(CommandId));
		p += sizeof(CommandId);
	}
	else
	{
		elogif(Debug_print_full_dtm, LOG,
			   "DtxContextInfo_Serialize only copied InvalidDistributedTransactionId");
	}

	elogif(Debug_print_full_dtm, LOG,
		   "DtxContextInfo_Serialize distributedTimeStamp %u, distributedXid = %u, curcid %d nestingLevel %d segmateSync %u",
		   dtxContextInfo->distributedTimeStamp, dtxContextInfo->distributedXid,
		   dtxContextInfo->curcid, dtxContextInfo->nestingLevel, dtxContextInfo->segmateSync);

	memcpy(p, &dtxContextInfo->segmateSync, sizeof(uint32));
	p += sizeof(uint32);

	memcpy(p, &dtxContextInfo->nestingLevel, sizeof(uint32));
	p += sizeof(uint32);

	memcpy(p, &dtxContextInfo->haveDistributedSnapshot, sizeof(bool));
	p += sizeof(bool);

	memcpy(p, &dtxContextInfo->cursorContext, sizeof(bool));
	p += sizeof(bool);

	if (dtxContextInfo->haveDistributedSnapshot)
	{
		p += DistributedSnapshot_Serialize(ds, p);
	}

	memcpy(p, &dtxContextInfo->distributedTxnOptions, sizeof(int));
	p += sizeof(int);

	used = (p - buffer);

	if (Debug_print_full_dtm || Debug_print_snapshot_dtm)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "DtxContextInfo_Serialize distributedTimeStamp %u, distributedXid = %u, "
			   "curcid %d",
			   dtxContextInfo->distributedTimeStamp,
			   dtxContextInfo->distributedXid,
			   dtxContextInfo->curcid);

		if (dtxContextInfo->haveDistributedSnapshot)
		{
			elogif(Debug_print_full_dtm, LOG,
				   "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount = %d)",
				   ds->xminAllDistributedSnapshots,
				   ds->xmin,
				   ds->xmax,
				   ds->count,
				   ds->maxCount);
			if (Debug_print_full_dtm)
			{
				for (i = 0; i < ds->count; i++)
				{
					elog(LOG, "....    inProgressXidArray[%d] = %u",
						 i, ds->inProgressXidArray[i]);
				}
			}
			elogif(Debug_print_snapshot_dtm, LOG,
				   "[Distributed Snapshot #%u] *Serialize* currcid = %d (gxid = %u, '%s')",
				   ds->distribSnapshotId,
				   dtxContextInfo->curcid,
				   getDistributedTransactionId(),
				   DtxContextToString(DistributedTransactionContext));
		}
		elogif(Debug_print_full_dtm, LOG, "DtxContextInfo_Serialize txnOptions = 0x%x", dtxContextInfo->distributedTxnOptions);
		elogif(Debug_print_full_dtm, LOG, "DtxContextInfo_Serialize copied %d bytes", used);
	}
}

void
DtxContextInfo_Reset(DtxContextInfo *dtxContextInfo)
{
	dtxContextInfo->distributedTimeStamp = 0;
	dtxContextInfo->distributedXid = InvalidDistributedTransactionId;
	memcpy(dtxContextInfo->distributedId, TmGid_Init, TMGIDSIZE);
	Assert(strlen(dtxContextInfo->distributedId) < TMGIDSIZE);

	dtxContextInfo->curcid = 0;
	dtxContextInfo->segmateSync = 0;
	dtxContextInfo->nestingLevel = 0;

	dtxContextInfo->haveDistributedSnapshot = false;

	DistributedSnapshot_Reset(&dtxContextInfo->distributedSnapshot);

	dtxContextInfo->distributedTxnOptions = 0;
}

void
DtxContextInfo_Copy(
					DtxContextInfo *target,
					DtxContextInfo *source)
{
	DtxContextInfo_Reset(target);

	target->distributedTimeStamp = source->distributedTimeStamp;
	target->distributedXid = source->distributedXid;
	Assert(strlen(source->distributedId) < TMGIDSIZE);
	memcpy(
		   target->distributedId,
		   source->distributedId,
		   TMGIDSIZE);

	target->segmateSync = source->segmateSync;
	target->nestingLevel = source->nestingLevel;

	target->curcid = source->curcid;

	target->haveDistributedSnapshot = source->haveDistributedSnapshot;
	target->cursorContext = source->cursorContext;

	if (source->haveDistributedSnapshot)
		DistributedSnapshot_Copy(&target->distributedSnapshot,
								 &source->distributedSnapshot);

	target->distributedTxnOptions = source->distributedTxnOptions;

	elogif(Debug_print_full_dtm, LOG,
		   "DtxContextInfo_Copy distributed {timestamp %u, xid %u}, id = %s, "
		   "command id %d",
		   target->distributedTimeStamp,
		   target->distributedXid,
		   target->distributedId,
		   target->curcid);

	if (target->haveDistributedSnapshot)
		elogif(Debug_print_full_dtm, LOG,
			   "distributed snapshot {timestamp %u, xminAllDistributedSnapshots %u, snapshot id %d, "
			   "xmin %u, count %d, xmax %u}",
			   target->distributedSnapshot.distribTransactionTimeStamp,
			   target->distributedSnapshot.xminAllDistributedSnapshots,
			   target->distributedSnapshot.distribSnapshotId,
			   target->distributedSnapshot.xmin,
			   target->distributedSnapshot.count,
			   target->distributedSnapshot.xmax);

}

void
DtxContextInfo_Deserialize(const char *serializedDtxContextInfo,
						   int serializedDtxContextInfolen,
						   DtxContextInfo *dtxContextInfo)
{
	int			i;
	DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

	DtxContextInfo_Reset(dtxContextInfo);

	if (serializedDtxContextInfolen > 0)
	{
		const char *p = serializedDtxContextInfo;

		elogif(Debug_print_full_dtm, LOG,
			   "DtxContextInfo_Deserialize serializedDtxContextInfolen = %d.",
			   serializedDtxContextInfolen);

		memcpy(&dtxContextInfo->distributedXid, p, sizeof(DistributedTransactionId));
		p += sizeof(DistributedTransactionId);

		if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
		{
			memcpy(&dtxContextInfo->distributedTimeStamp, p, sizeof(DistributedTransactionTimeStamp));
			p += sizeof(DistributedTransactionTimeStamp);
			memcpy(dtxContextInfo->distributedId, p, TMGIDSIZE);
			if (strlen(dtxContextInfo->distributedId) >= TMGIDSIZE)
				elog(PANIC, "Distribute transaction identifier too long (%d)",
					 (int) strlen(dtxContextInfo->distributedId));
			p += TMGIDSIZE;
			memcpy(&dtxContextInfo->curcid, p, sizeof(CommandId));
			p += sizeof(CommandId);
		}
		else
		{
			elogif(Debug_print_full_dtm, LOG,
				   "DtxContextInfo_Deserialize distributedXid was InvalidDistributedTransactionId");
		}

		memcpy(&dtxContextInfo->segmateSync, p, sizeof(uint32));
		p += sizeof(uint32);
		memcpy(&dtxContextInfo->nestingLevel, p, sizeof(uint32));
		p += sizeof(uint32);
		memcpy(&dtxContextInfo->haveDistributedSnapshot, p, sizeof(bool));
		p += sizeof(bool);

		memcpy(&dtxContextInfo->cursorContext, p, sizeof(bool));
		p += sizeof(bool);

		elogif(Debug_print_full_dtm, LOG,
			   "DtxContextInfo_Deserialize distributedTimeStamp %u, distributedXid = %u, curcid %d nestingLevel %d segmateSync %u as %s",
			   dtxContextInfo->distributedTimeStamp, dtxContextInfo->distributedXid,
			   dtxContextInfo->curcid, dtxContextInfo->nestingLevel,
			   dtxContextInfo->segmateSync, (Gp_is_writer ? "WRITER" : "READER"));

		if (dtxContextInfo->haveDistributedSnapshot)
		{
			p += DistributedSnapshot_Deserialize(p, ds);
		}
		else
		{
			elogif(Debug_print_full_dtm, LOG,
				   "DtxContextInfo_Deserialize no distributed snapshot");
		}

		memcpy(&dtxContextInfo->distributedTxnOptions, p, sizeof(int));
		p += sizeof(int);

		if (Debug_print_full_dtm)
		{
			elog(LOG,
				 "DtxContextInfo_Deserialize distributedTimeStamp %u, distributedXid = %u, "
				 "distributedId = %s",
				 dtxContextInfo->distributedTimeStamp,
				 dtxContextInfo->distributedXid,
				 dtxContextInfo->distributedId);

			if (dtxContextInfo->haveDistributedSnapshot)
			{
				elog(LOG,
					 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount = %d)",
					 ds->xminAllDistributedSnapshots,
					 ds->xmin,
					 ds->xmax,
					 ds->count,
					 ds->maxCount);

				for (i = 0; i < ds->count; i++)
				{
					elog(LOG, "....    inProgressXidArray[%d] = %u",
						 i, ds->inProgressXidArray[i]);
				}

				elogif(Debug_print_snapshot_dtm, LOG,
					   "[Distributed Snapshot #%u] *Deserialize* currcid = %d (gxid = %u, '%s')",
					   ds->distribSnapshotId,
					   dtxContextInfo->curcid,
					   getDistributedTransactionId(),
					   DtxContextToString(DistributedTransactionContext));
			}

			elog(LOG, "DtxContextInfo_Deserialize txnOptions = 0x%x",
				 dtxContextInfo->distributedTxnOptions);
		}
	}
	else
	{
		Assert(dtxContextInfo->distributedXid == InvalidDistributedTransactionId);
		Assert(dtxContextInfo->distributedTxnOptions == 0);
	}
}
