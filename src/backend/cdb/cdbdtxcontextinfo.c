/*-------------------------------------------------------------------------
 *
 * cdbdtxcontextinfo.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
		dtxContextInfo->distributedTimeStamp = getDtmStartTime();
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

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
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

	if (DEBUG5 >= log_min_messages || Debug_print_full_dtm)
	{
		char		gid[TMGIDSIZE];
		DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

		if (!getDistributedTransactionIdentifier(gid))
			memcpy(gid, "<empty>", 8);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster Gp_role is DISPATCH and have gid = %s, gxid = %u --> have distributed snapshot",
			 gid,
			 getDistributedTransactionId());
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster distributedXid = %u, "
			 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d)",
			 dtxContextInfo->distributedXid,
			 ds->xminAllDistributedSnapshots,
			 ds->xmin,
			 ds->xmax,
			 ds->count);

		for (i = 0; i < ds->count; i++)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "....    distributedSnapshotData->xip[%d] = %u",
				 i, ds->inProgressXidArray[i]);
		}
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster curcid = %u",
			 dtxContextInfo->curcid);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
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

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
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
		memcpy(p, &dtxContextInfo->curcid, sizeof(CommandId));
		p += sizeof(CommandId);
	}
	else
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_Serialize only copied InvalidDistributedTransactionId");
	}

	elog((Debug_print_full_dtm ? LOG : DEBUG3),
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

	if (DEBUG5 >= log_min_messages || Debug_print_full_dtm || Debug_print_snapshot_dtm)
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_Serialize distributedTimeStamp %u, distributedXid = %u, "
			 "curcid %d",
			 dtxContextInfo->distributedTimeStamp,
			 dtxContextInfo->distributedXid,
			 dtxContextInfo->curcid);

		if (dtxContextInfo->haveDistributedSnapshot)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d)",
				 ds->xminAllDistributedSnapshots,
				 ds->xmin,
				 ds->xmax,
				 ds->count);
			for (i = 0; i < ds->count; i++)
			{
				elog((Debug_print_full_dtm ? LOG : DEBUG5),
					 "....    inProgressXidArray[%d] = %u",
					 i, ds->inProgressXidArray[i]);
			}
			elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				 "[Distributed Snapshot #%u] *Serialize* currcid = %d (gxid = %u, '%s')",
				 ds->distribSnapshotId,
				 dtxContextInfo->curcid,
				 getDistributedTransactionId(),
				 DtxContextToString(DistributedTransactionContext));
		}
		elog((Debug_print_full_dtm ? LOG : DEBUG5), "DtxContextInfo_Serialize txnOptions = 0x%x", dtxContextInfo->distributedTxnOptions);
		elog((Debug_print_full_dtm ? LOG : DEBUG5), "DtxContextInfo_Serialize copied %d bytes", used);
	}
}

void
DtxContextInfo_Reset(DtxContextInfo *dtxContextInfo)
{
	dtxContextInfo->distributedTimeStamp = 0;
	dtxContextInfo->distributedXid = InvalidDistributedTransactionId;

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
	target->segmateSync = source->segmateSync;
	target->nestingLevel = source->nestingLevel;

	target->curcid = source->curcid;

	target->haveDistributedSnapshot = source->haveDistributedSnapshot;
	target->cursorContext = source->cursorContext;

	if (source->haveDistributedSnapshot)
		DistributedSnapshot_Copy(&target->distributedSnapshot,
								 &source->distributedSnapshot);

	target->distributedTxnOptions = source->distributedTxnOptions;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DtxContextInfo_Copy distributed {timestamp %u, xid %u}, "
		 "command id %d",
		 target->distributedTimeStamp,
		 target->distributedXid,
		 target->curcid);

	if (target->haveDistributedSnapshot)
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
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

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_Deserialize serializedDtxContextInfolen = %d.",
			 serializedDtxContextInfolen);

		memcpy(&dtxContextInfo->distributedXid, p, sizeof(DistributedTransactionId));
		p += sizeof(DistributedTransactionId);

		if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
		{
			memcpy(&dtxContextInfo->distributedTimeStamp, p, sizeof(DistributedTransactionTimeStamp));
			p += sizeof(DistributedTransactionTimeStamp);
			memcpy(&dtxContextInfo->curcid, p, sizeof(CommandId));
			p += sizeof(CommandId);
		}
		else
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
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

		elog((Debug_print_full_dtm ? LOG : DEBUG3),
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
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "DtxContextInfo_Deserialize no distributed snapshot");
		}

		memcpy(&dtxContextInfo->distributedTxnOptions, p, sizeof(int));
		p += sizeof(int);

		if (DEBUG5 >= log_min_messages || Debug_print_full_dtm)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "DtxContextInfo_Deserialize distributedTimeStamp %u, distributedXid = %u",
				 dtxContextInfo->distributedTimeStamp,
				 dtxContextInfo->distributedXid);

			if (dtxContextInfo->haveDistributedSnapshot)
			{
				elog((Debug_print_full_dtm ? LOG : DEBUG5),
					 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d)",
					 ds->xminAllDistributedSnapshots,
					 ds->xmin,
					 ds->xmax,
					 ds->count);

				for (i = 0; i < ds->count; i++)
				{
					elog((Debug_print_full_dtm ? LOG : DEBUG5),
						 "....    inProgressXidArray[%d] = %u",
						 i, ds->inProgressXidArray[i]);
				}

				elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
					 "[Distributed Snapshot #%u] *Deserialize* currcid = %d (gxid = %u, '%s')",
					 ds->distribSnapshotId,
					 dtxContextInfo->curcid,
					 getDistributedTransactionId(),
					 DtxContextToString(DistributedTransactionContext));
			}

			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "DtxContextInfo_Deserialize txnOptions = 0x%x",
				 dtxContextInfo->distributedTxnOptions);
		}
	}
	else
	{
		Assert(dtxContextInfo->distributedXid == InvalidDistributedTransactionId);
		Assert(dtxContextInfo->distributedTxnOptions == 0);
	}
}
