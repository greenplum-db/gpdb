/*-------------------------------------------------------------------------
 *
 * cdbdtxcontextinfo.c
 *
 * Copyright (c) 2007-2008, Greenplum inc
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
#include "utils/tqual.h"

/*
 * process local cache used to identify "dispatch units"
 *
 * Note, this is only required because the dispatcher emits multiple statements (which will
 * correspond to multiple local-xids on the segments) under the same distributed-xid.
 *
 */
static uint32 syncCount = 1;
static DistributedTransactionId syncCacheXid = InvalidDistributedTransactionId;

void
DtxContextInfo_RewindSegmateSync(void)
{
	syncCount--;
}

static void
DtxContextInfo_CopyDistributedSnapshot(
	DistributedSnapshot 				*ds,
	DistributedSnapshotWithLocalMapping *dslm)
{
	int i;

	ds->header.distribTransactionTimeStamp = dslm->header.distribTransactionTimeStamp;	
	ds->header.xminAllDistributedSnapshots = dslm->header.xminAllDistributedSnapshots;		
	ds->header.distribSnapshotId = dslm->header.distribSnapshotId;		
	ds->header.xmin = dslm->header.xmin;		
	ds->header.xmax = dslm->header.xmax;		
	ds->header.count = dslm->header.count;	
	// Leave maxCount alone
	
	// UNDONE: Common routine?
	/*
	 * If we have allocated space for the in-progress distributed
	 * transactions, check against that space.  Otherwise,
	 * use the received maxCount as guide in allocating space.
	 */
	if (ds->header.maxCount > 0)
	{
		Assert(ds->inProgressXidArray != NULL);
		
		if(dslm->header.count > ds->header.maxCount)
			elog(ERROR,"Too many distributed transactions for snapshot (maxCount %d, count %d)",
			     ds->header.maxCount, dslm->header.count);
	}
	else
	{
		Assert(ds->inProgressXidArray == NULL);
		
		ds->inProgressXidArray = 
			(DistributedTransactionId*)
					malloc(dslm->header.maxCount * sizeof(DistributedTransactionId));
		if (ds->inProgressXidArray == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
		ds->header.maxCount = dslm->header.maxCount;
	}

	for (i = 0; i < ds->header.count; i++)
	{
		ds->inProgressXidArray[i] =
			dslm->inProgressEntryArray[i].distribXid;
	}
}

void
DtxContextInfo_CreateOnMaster(DtxContextInfo *dtxContextInfo,
							  DistributedSnapshotWithLocalMapping *dslm,
							  CommandId curcid, int txnOptions)
{
	int i;

	DtxContextInfo_Reset(dtxContextInfo);

	dtxContextInfo->distributedXid = getDistributedTransactionId();

	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		if (syncCacheXid == dtxContextInfo->distributedXid)
			dtxContextInfo->segmateSync = ++syncCount;
		else
		{
			syncCacheXid = dtxContextInfo->distributedXid;
			dtxContextInfo->segmateSync = syncCount = 1;
		}

		dtxContextInfo->distributedTimeStamp = getDtxStartTime();

		getDistributedTransactionIdentifier(dtxContextInfo->distributedId);
		dtxContextInfo->curcid = curcid;
	}
	else
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster Gp_role is DISPATCH and distributed transaction is InvalidDistributedTransactionId");

		syncCacheXid = dtxContextInfo->distributedXid;
		dtxContextInfo->segmateSync = syncCount = 1;
	}

	dtxContextInfo->nestingLevel = GetCurrentTransactionNestLevel();

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DtxContextInfo_CreateOnMaster: created dtxcontext with dxid %u/%u nestingLevel %d segmateSync %u/%u (current/cached)",
		 dtxContextInfo->distributedXid, syncCacheXid, dtxContextInfo->nestingLevel,
		 dtxContextInfo->segmateSync, syncCount);

	if (dslm == NULL)
		dtxContextInfo->haveDistributedSnapshot = false;
	else
	{
		DtxContextInfo_CopyDistributedSnapshot(&dtxContextInfo->distributedSnapshot, dslm);
		dtxContextInfo->haveDistributedSnapshot = true;
	}

	dtxContextInfo->distributedTxnOptions = txnOptions;

	if (DEBUG5 >= log_min_messages || Debug_print_full_dtm)
	{
		char gid[TMGIDSIZE];
		DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

		if (!getDistributedTransactionIdentifier(gid))
			memcpy(gid, "<empty>", 8);
		
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster Gp_role is DISPATCH and have currentGxact = %s, gxid = %u --> have distributed snapshot",
			 gid, 
			 getDistributedTransactionId());
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster distributedXid = %u, "
		     "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount %d)",
			 dtxContextInfo->distributedXid,
			 ds->header.xminAllDistributedSnapshots,
			 ds->header.xmin, 
			 ds->header.xmax,
			 ds->header.count,
			 ds->header.maxCount);
		
		for (i = 0; i < ds->header.count; i++)
		{
			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "....    distributedSnapshotData->xip[%d] = %u", 
			     i, ds->inProgressXidArray[i]);
		}
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster curcid = %u",
			 dtxContextInfo->curcid);

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "DtxContextInfo_CreateOnMaster txnOptions = 0x%x, needTwoPhase = %s, explicitBegin = %s, isoLevel = %s, readOnly = %s.", 
			 txnOptions, 
			 (isMppTxOptions_NeedTwoPhase(txnOptions) ? "true" : "false"),
			 (isMppTxOptions_ExplicitBegin(txnOptions) ? "true" : "false"),
			 IsoLevelAsUpperString(mppTxOptions_IsoLevel(txnOptions)),
			 (isMppTxOptions_ReadOnly(txnOptions) ? "true" : "false")); 
	}
}

static int
DistributedHeader_SerializeSize(DistributedSnapshotHeader *dsh)
{
	return sizeof(DistributedTransactionTimeStamp) +	// distributedTransactionTimeStamp
		   sizeof(DistributedTransactionId) +			// xminAllDistributedSnapshots
		   sizeof(DistributedSnapshotId) +				// distributedSnapshotId
	       2 * sizeof(DistributedTransactionId) +		// xmin, xmax
	       2 * sizeof(int32);							// count, maxCount
}

int 
DtxContextInfo_SerializeSize(DtxContextInfo *dtxContextInfo)
{
	int size = 0;

	size += sizeof(DistributedTransactionId);	/* distributedXid */

	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		size += sizeof(DistributedTransactionTimeStamp);
		size += TMGIDSIZE;				/* distributedId */
		size += sizeof(CommandId);		/* curcid */
	}

	size += sizeof(uint32); /* segmateSync */
	size += sizeof(uint32); /* nestingLevel */
	size += sizeof(bool);	/* haveDistributedSnapshot */
	size += sizeof(bool);	/* cursorContext */

	if (dtxContextInfo->haveDistributedSnapshot)
	{
		DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

		size += DistributedHeader_SerializeSize(&ds->header);
		size += sizeof(DistributedTransactionId) * ds->header.count;
	}

	size += sizeof(int);		/* distributedTxnOptions */
	
	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DtxContextInfo_SerializeSize is returning size = %d", size);

	return size;
}

void
DtxContextInfo_Serialize(char *buffer, DtxContextInfo *dtxContextInfo)
{
	char *p = buffer;
	int i;
	int used;
	DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

	memcpy(p, &dtxContextInfo->distributedXid, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	if (dtxContextInfo->distributedXid != InvalidDistributedTransactionId)
	{
		memcpy(p, &dtxContextInfo->distributedTimeStamp, sizeof(DistributedTransactionTimeStamp)); 
		p += sizeof(DistributedTransactionTimeStamp);
		if (strlen(dtxContextInfo->distributedId) >= TMGIDSIZE)
			elog(PANIC, "Distribute transaction identifier too long (%d)",
				 (int)strlen(dtxContextInfo->distributedId));
		memcpy(p, dtxContextInfo->distributedId, TMGIDSIZE);
		p += TMGIDSIZE;		
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
		memcpy(p, &ds->header.distribTransactionTimeStamp, sizeof(DistributedTransactionTimeStamp)); 
		p += sizeof(DistributedTransactionTimeStamp);
		memcpy(p, &ds->header.xminAllDistributedSnapshots, sizeof(DistributedTransactionId)); 
		p += sizeof(DistributedTransactionId);
		memcpy(p, &ds->header.distribSnapshotId, sizeof(DistributedSnapshotId)); 
		p += sizeof(DistributedSnapshotId);
		memcpy(p, &ds->header.xmin, sizeof(DistributedTransactionId)); 
		p += sizeof(DistributedTransactionId);
		memcpy(p, &ds->header.xmax, sizeof(DistributedTransactionId)); 
		p += sizeof(DistributedTransactionId);
		memcpy(p, &ds->header.count, sizeof(int32)); p += sizeof(int32);
		memcpy(p, &ds->header.maxCount, sizeof(int32)); p += sizeof(int32);

		memcpy(p, ds->inProgressXidArray, sizeof(DistributedTransactionId)*ds->header.count); 
		p += sizeof(DistributedTransactionId)*ds->header.count;

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
				 "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount = %d)",
				 ds->header.xminAllDistributedSnapshots,
				 ds->header.xmin, 
				 ds->header.xmax,
				 ds->header.count,
				 ds->header.maxCount);
			for (i = 0; i < ds->header.count; i++)
			{
				elog((Debug_print_full_dtm ? LOG : DEBUG5),
					 "....    inProgressXidArray[%d] = %u", 
				     i, ds->inProgressXidArray[i]);
			}
			elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
				 "[Distributed Snapshot #%u] *Serialize* currcid = %d (gxid = %u, '%s')", 
			 	 ds->header.distribSnapshotId,
			 	 dtxContextInfo->curcid,
			 	 getDistributedTransactionId(),
				 DtxContextToString(DistributedTransactionContext));
		}
		elog((Debug_print_full_dtm ? LOG : DEBUG5),"DtxContextInfo_Serialize txnOptions = 0x%x", dtxContextInfo->distributedTxnOptions);
		elog((Debug_print_full_dtm ? LOG : DEBUG5),"DtxContextInfo_Serialize copied %d bytes", used);
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
		DistributedSnapshot_Copy(
			&target->distributedSnapshot,
			&source->distributedSnapshot);

	target->distributedTxnOptions = source->distributedTxnOptions;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DtxContextInfo_Copy distributed {timestamp %u, xid %u}, id = %s, "
		 "command id %d",
		 target->distributedTimeStamp,
		 target->distributedXid,
		 target->distributedId,
		 target->curcid);

	if (target->haveDistributedSnapshot)
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "distributed snapshot header {timestamp %u, xminAllDistributedSnapshots %u, snapshot id %d, "
			 "xmin %u, count %d, xmax %u}",
			 target->distributedSnapshot.header.distribTransactionTimeStamp,
			 target->distributedSnapshot.header.xminAllDistributedSnapshots,
			 target->distributedSnapshot.header.distribSnapshotId,
			 target->distributedSnapshot.header.xmin,
			 target->distributedSnapshot.header.count,
			 target->distributedSnapshot.header.xmax);
		 
}

void
DtxContextInfo_Deserialize(const char *serializedDtxContextInfo,
						   int serializedDtxContextInfolen,
						   DtxContextInfo *dtxContextInfo)
{
	int i;
	DistributedSnapshot *ds = &dtxContextInfo->distributedSnapshot;

	DtxContextInfo_Reset(dtxContextInfo);

	if (serializedDtxContextInfolen > 0)
	{
		int xipsize;
		const char *p = serializedDtxContextInfo;
		int32 maxCount;

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
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
					 (int)strlen(dtxContextInfo->distributedId));
			p += TMGIDSIZE;
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
			memcpy(&ds->header.distribTransactionTimeStamp, p, sizeof(DistributedTransactionTimeStamp));
			p += sizeof(DistributedTransactionTimeStamp);
			memcpy(&ds->header.xminAllDistributedSnapshots, p, sizeof(DistributedTransactionId));
			p += sizeof(DistributedTransactionId);
			memcpy(&ds->header.distribSnapshotId, p, sizeof(DistributedSnapshotId));
			p += sizeof(DistributedSnapshotId);
			memcpy(&ds->header.xmin, p, sizeof(DistributedTransactionId));
			p += sizeof(DistributedTransactionId);
			memcpy(&ds->header.xmax, p, sizeof(DistributedTransactionId));
			p += sizeof(DistributedTransactionId);
			memcpy(&ds->header.count, p, sizeof(int32));
			p += sizeof(int32);

			/*
			 * Copy this one to a local variable first.
			 */
			memcpy(&maxCount, p, sizeof(int32));
			p += sizeof(int32);
			if (maxCount < 0 || ds->header.count > maxCount)
			{
				elog(ERROR, "Invalid distributed snapshot received (maxCount %d, count %d)",
				     maxCount, ds->header.count);
			}

			/*
			 * If we have allocated space for the in-progress distributed
			 * transactions, check against that space.  Otherwise,
			 * use the received maxCount as guide in allocating space.
			 */
			if (ds->inProgressXidArray != NULL)
			{
				if (ds->header.maxCount == 0)
				{
					elog(ERROR, "Bad allocation of in-progress array");
				}

				if (ds->header.count > ds->header.maxCount)
				{
					elog(ERROR, "Too many distributed transactions for snapshot (maxCount %d, count %d)",
					     ds->header.maxCount, ds->header.count);
				}
			}
			else
			{
				Assert(ds->inProgressXidArray == NULL);

				if (maxCount > 0)
				{
					if (maxCount < ds->header.maxCount)
					{
						maxCount = ds->header.maxCount;
					}
					else
					{
						ds->header.maxCount = maxCount;
					}

					ds->inProgressXidArray = (DistributedTransactionId *)malloc(maxCount * sizeof(DistributedTransactionId));
					if (ds->inProgressXidArray == NULL)
					{
						ereport(ERROR,
								(errcode(ERRCODE_OUT_OF_MEMORY),
								 errmsg("out of memory")));
					}
				}
			}

			if (ds->header.count > 0)
			{
				Assert(ds->inProgressXidArray != NULL);

				xipsize = sizeof(DistributedTransactionId) * ds->header.count;
				memcpy(ds->inProgressXidArray, p, xipsize);
				p += xipsize;
			}
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
				 "DtxContextInfo_Deserialize distributedTimeStamp %u, distributedXid = %u, "
				 "distributedId = %s",
			     dtxContextInfo->distributedTimeStamp,
				 dtxContextInfo->distributedXid,
				 dtxContextInfo->distributedId);

			if (dtxContextInfo->haveDistributedSnapshot)
			{
				elog((Debug_print_full_dtm ? LOG : DEBUG5),
				     "distributedSnapshotHeader (xminAllDistributedSnapshots %u, xmin = %u, xmax = %u, count = %d, maxCount = %d)",
				     ds->header.xminAllDistributedSnapshots,
					 ds->header.xmin,
					 ds->header.xmax,
					 ds->header.count,
					 ds->header.maxCount);

				for (i = 0; i < ds->header.count; i++)
				{
					elog((Debug_print_full_dtm ? LOG : DEBUG5),
						 "....    inProgressXidArray[%d] = %u",
					     i, ds->inProgressXidArray[i]);
				}

				elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
					 "[Distributed Snapshot #%u] *Deserialize* currcid = %d (gxid = %u, '%s')",
					 ds->header.distribSnapshotId,
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
