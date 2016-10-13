/*
 *  cdbfilerepprimaryresync.c  
 *
 *  Copyright 2009-2010 Greenplum Inc. All rights reserved.
 *
 */

#include "postgres.h"

#include <signal.h>
#include <fcntl.h>

#include "cdb/cdbvars.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepprimaryrecovery.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbfilerepresyncworker.h"
#include "cdb/cdbmirroredappendonly.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/faultinjector.h"
#include "utils/relcache.h"

#define OIDCHARS	10			/* max chars printed by %u */

static int FileRepPrimary_RunResyncWorker(void);
static int FileRepPrimary_ResyncWrite(FileRepResyncHashEntry_s	*entry);
static int FileRepPrimary_ResyncBufferPoolIncrementalWrite(ChangeTrackingRequest *request);

static bool readBufferRequest = FALSE;
static void FileRepResync_ResetReadBufferRequest(void);
static void FileRepResync_SetReadBufferRequest(void);

static void
FileRepResync_ResetReadBufferRequest(void)
{
	readBufferRequest = FALSE;
}

static void
FileRepResync_SetReadBufferRequest(void)
{
	readBufferRequest = TRUE;
}

bool
FileRepResyncWorker_IsResyncRequest(void)
{
	return (readBufferRequest == FALSE && FileRepPrimary_IsResyncWorker());	
}

/*
 * FileRepPrimary_StartResyncWorker()
 */
void 
FileRepPrimary_StartResyncWorker(void)
{	
	int	status = STATUS_OK;
	
	FileRep_InsertConfigLogEntry("start resync worker");
	
	Insist(fileRepRole == FileRepPrimaryRole);
	
	while (1) {
		
		if (status != STATUS_OK) 
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}
		
		while (FileRepSubProcess_GetState() != FileRepStateShutdown &&
			   FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
			   ! (FileRepSubProcess_GetState() == FileRepStateReady && 
			    dataState == DataStateInResync)) {
			
			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */	
		}
		
		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends) {
			
			break;
		}
		
		status = FileRepPrimary_RunResyncWorker();
		
		if (status != STATUS_OK) {
			continue;
		}
		
		break;
		
	} // while(1)	
		
}

/*
 * FileRepPrimary_RunResyncWorker()
 *
 */
static int
FileRepPrimary_RunResyncWorker(void)
{
	int							status = STATUS_OK;
	FileRepResyncHashEntry_s	*entry = NULL;
	ChangeTrackingRequest		*request = NULL;

	FileRep_InsertConfigLogEntry("run resync worker");
	
	while (1) {

		FileRepSubProcess_ProcessSignals();
		
		if (! (FileRepSubProcess_GetState() == FileRepStateReady && 
			   dataState == DataStateInResync))
		{
			break;
		}

		entry = FileRepPrimary_GetResyncEntry(&request);
				
		if (entry == NULL && request == NULL) {
			
			pg_usleep(100000L); /* 100 ms */
			continue;
		}
		
		Assert(! (entry != NULL && request != NULL));

		if (entry != NULL)
		{			
			status = FileRepPrimary_ResyncWrite(entry);
			
			if (status == STATUS_OK)
			{
				if (entry->mirrorBufpoolResyncChangedPageCount == 0)
				{
					entry->mirrorBufpoolResyncChangedPageCount = (entry->mirrorAppendOnlyNewEof - entry->mirrorAppendOnlyLossEof) / BLCKSZ;
				}					
				
				status = FileRepResync_UpdateEntry(entry);
			}
		}
		
		if (request != NULL)
		{
			status = FileRepPrimary_ResyncBufferPoolIncrementalWrite(request);
			request = NULL;
		}
		
		if (status != STATUS_OK)
		{
			break;
		}
		
	}
	
	return status;
}

/*
 *
 * RESYNC relation (Buffer Pool)
 *		INPUT parameters
 *				*) RelFileNode
 *				*) beginLSN (the earliest LSN to be re-synchronized) 
 *				*) endLSN (the latest LSN to be re-synchronized)
 *				*) endBlockNumber (the latest block in relation to be re-synchronized)
 *				*) 
 *
 *
 *	RESYNC relation (Append Only)
 *		INPUT parameters
 *				*) RelFileNode
 *				*)
 *
 * The following relations will be always resynced as scan incremental
 *		OIDs {5090, 5091, 5092, 5093, and 5096}
 *
 */

static int
FileRepPrimary_ResyncWrite(FileRepResyncHashEntry_s	*entry)
{

	int				status = STATUS_OK;
	Page			page;
	Buffer			buf; 
	BlockNumber		numBlocks;
	BlockNumber		blkno;
	SMgrRelation	smgr_relation;
	char			relidstr[OIDCHARS + 1 + OIDCHARS + 1 + OIDCHARS + 1];
	XLogRecPtr		loc;
	int				count = 0;
	int				thresholdCount = 0;
	bool			mirrorDataLossOccurred = FALSE;
		
	switch (entry->relStorageMgr)
	{

		case PersistentFileSysRelStorageMgr_BufferPool:
			
			switch (entry->mirrorDataSynchronizationState)
			{
				case MirroredRelDataSynchronizationState_BufferPoolScanIncremental:
				case MirroredRelDataSynchronizationState_FullCopy:

					smgr_relation = smgropen(entry->relFileNode);
					
					numBlocks = smgrnblocks(smgr_relation);

					snprintf(relidstr, sizeof(relidstr), "%u/%u/%u",
							 smgr_relation->smgr_rnode.spcNode,
							 smgr_relation->smgr_rnode.dbNode,
							 smgr_relation->smgr_rnode.relNode);

					if (Debug_filerep_print)
						elog(LOG, "resync buffer pool relation '%s' number of blocks '%d' ",
							 relidstr, numBlocks);

					thresholdCount = Min(numBlocks, 1024);
					
					/* 
					 * required in order to report how many blocks were synchronized 
					 * if gp_persistent_relation_node does not return that information 
					 */
					if (entry->mirrorBufpoolResyncChangedPageCount == 0)
					{
						entry->mirrorBufpoolResyncChangedPageCount = numBlocks - entry->mirrorBufpoolResyncCkptBlockNum;
					}
					
					for (blkno = entry->mirrorBufpoolResyncCkptBlockNum; blkno < numBlocks; blkno++) 
					{
						XLogRecPtr	endResyncLSN = (isFullResync() ? 
													FileRepResync_GetEndFullResyncLSN() :
													FileRepResync_GetEndIncrResyncLSN());
#ifdef FAULT_INJECTOR
						FaultInjector_InjectFaultIfSet(
													   FileRepResyncWorkerRead,
													   DDLNotSpecified,
													   "",	//databaseName
													   ""); // tableName
#endif				
						
						FileRepResync_SetReadBufferRequest();
						buf = ReadBuffer_Resync(smgr_relation, blkno);
						FileRepResync_ResetReadBufferRequest();
						
						LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
						page = BufferGetPage(buf);
						
						loc = PageGetLSN(page);
						
						if (Debug_filerep_print)
						{
							elog(LOG, 
									 "full resync buffer pool identifier '%s' num blocks '%d' blkno '%d' lsn begin change tracking '%s(%u/%u)' "
									 "lsn page '%s(%u/%u)' lsn end change tracking '%s(%u/%u)' ",
									 relidstr,
									 numBlocks,
									 blkno,
									 XLogLocationToString(&entry->mirrorBufpoolResyncCkptLoc),
									 entry->mirrorBufpoolResyncCkptLoc.xlogid,
									 entry->mirrorBufpoolResyncCkptLoc.xrecoff,
									 XLogLocationToString(&loc),
									 loc.xlogid,
									 loc.xrecoff,
									 XLogLocationToString(&endResyncLSN),
									 endResyncLSN.xlogid,
									 endResyncLSN.xrecoff);
						}
						else
						{
							char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];
							
							snprintf(tmpBuf, sizeof(tmpBuf), 
									 "full resync buffer pool identifier '%s' num blocks '%d' blkno '%d' lsn begin change tracking '%s(%u/%u)' ",
									 relidstr,
									 numBlocks,
									 blkno,
									 XLogLocationToString(&entry->mirrorBufpoolResyncCkptLoc),
									 entry->mirrorBufpoolResyncCkptLoc.xlogid,
									 entry->mirrorBufpoolResyncCkptLoc.xrecoff);
														
							FileRep_InsertConfigLogEntry(tmpBuf);
							
							snprintf(tmpBuf, sizeof(tmpBuf), 
									 "full resync buffer pool identifier '%s' lsn page '%s(%u/%u)' lsn end change tracking '%s(%u/%u)' ",
									 relidstr,
									 XLogLocationToString(&loc),
									 loc.xlogid,
									 loc.xrecoff,
									 XLogLocationToString(&endResyncLSN),
									 endResyncLSN.xlogid,
									 endResyncLSN.xrecoff);
							
							FileRep_InsertConfigLogEntry(tmpBuf);
							
						}
						
						if (XLByteLE(PageGetLSN(page), endResyncLSN) &&
							XLByteLE(entry->mirrorBufpoolResyncCkptLoc, PageGetLSN(page))) 
						{
							smgrwrite(smgr_relation, 
									  blkno,
									  (char *)BufferGetBlock(buf),
									  FALSE);
						}
						
#ifdef FAULT_INJECTOR	
						FaultInjector_InjectFaultIfSet(
													   FileRepResyncWorker, 
													   DDLNotSpecified,
													   "",	// databaseName
													   ""); // tableName
#endif				
						
						UnlockReleaseBuffer(buf);
						
						if (count > thresholdCount)
						{
							count = 0;
							FileRepSubProcess_ProcessSignals();
							
							if (! (FileRepSubProcess_GetState() == FileRepStateReady && 
								   dataState == DataStateInResync))
							{
								mirrorDataLossOccurred = TRUE;
								break;
							}
						}
						else
							count++;
					}
						
					if (mirrorDataLossOccurred)
						break;

					if (entry->mirrorDataSynchronizationState != MirroredRelDataSynchronizationState_FullCopy)
					{
						LockRelationForResyncExtension(&smgr_relation->smgr_rnode, ExclusiveLock);
					
						numBlocks = smgrnblocks(smgr_relation);
					
						smgrtruncate(smgr_relation,
								 numBlocks,
								 TRUE /* isTemp, TRUE means to not record in XLOG */,
								 FALSE /* isLocalBuf */,
								 &entry->persistentTid,
								 entry->persistentSerialNum);
								 
						UnlockRelationForResyncExtension(&smgr_relation->smgr_rnode, ExclusiveLock);
					}
					
					smgrimmedsync(smgr_relation);
					smgrclose(smgr_relation);
					
					smgr_relation = NULL;
					break;
					
				case MirroredRelDataSynchronizationState_None:										
				case MirroredRelDataSynchronizationState_DataSynchronized:
					break;
					
				default:
					ereport(LOG, 
							(errmsg("could not resynchronize relation '%u/%u/%u' "
									"mirror synchronization state:'%s(%d)' ",
									entry->relFileNode.relNode,
									entry->relFileNode.spcNode,
									entry->relFileNode.dbNode,
									MirroredRelDataSynchronizationState_Name(entry->mirrorDataSynchronizationState),
									entry->mirrorDataSynchronizationState)));
					break;
			}
			break;
			
		case PersistentFileSysRelStorageMgr_AppendOnly:
		{
			MirroredAppendOnlyOpen	mirroredOpen;
			int						primaryError;
			bool					mirrorDataLossOccurred;
			char					*buffer = NULL;
			int64					endOffset = entry->mirrorAppendOnlyNewEof;
			int64					startOffset = entry->mirrorAppendOnlyLossEof;
			int32					bufferLen = 0;
			int						retval = 0;
			
			switch (entry->mirrorDataSynchronizationState)
			{
				case MirroredRelDataSynchronizationState_AppendOnlyCatchup:
				case MirroredRelDataSynchronizationState_FullCopy:
					
					/* 
					 * required in order to report how many blocks were synchronized 
					 * if gp_persistent_relation_node does not return that information 
					 */
					if (entry->mirrorBufpoolResyncChangedPageCount == 0)
					{
						entry->mirrorBufpoolResyncChangedPageCount = (endOffset - startOffset) / BLCKSZ;
					}					
					
					/*
					 * The MirroredAppendOnly_OpenResynchonize routine knows we are a resynch worker and
					 * will open BOTH, but write only the MIRROR!!!
					 */
					MirroredAppendOnly_OpenResynchonize(
											&mirroredOpen, 
											&entry->relFileNode,
											entry->segmentFileNum,
											startOffset,
											&primaryError,
											&mirrorDataLossOccurred);
					if (primaryError != 0)
					{
						ereport(ERROR,
								(errcode_for_file_access(),
								 errmsg("could not open file %u/%u/%u.%u : %s",
										entry->relFileNode.dbNode,
										entry->relFileNode.spcNode,
										entry->relFileNode.relNode,
										entry->segmentFileNum,
										strerror(primaryError))));
						
						break;
					}

					if (mirrorDataLossOccurred)
						break;
					
					/* AO and CO Data Store writes 64k size by default */
					bufferLen = (Size) Min(2*BLCKSZ, endOffset - startOffset);
					buffer = (char*) palloc(bufferLen);
					if (buffer == NULL)
						ereport(ERROR,
								(errcode(ERRCODE_OUT_OF_MEMORY),
								 (errmsg("not enough memory for resynchronization"))));
					
					MemSet(buffer, 0, bufferLen);
					
					while (startOffset < endOffset)
					{
						retval = MirroredAppendOnly_Read(
												&mirroredOpen,
												buffer,
												bufferLen);
						
						if (retval != bufferLen) 
						{
							ereport(ERROR,
									(errcode_for_file_access(),
									 errmsg("could not read from position:" INT64_FORMAT " in file %u/%u/%u.%u : %m",
											startOffset, 
											entry->relFileNode.dbNode,
											entry->relFileNode.spcNode,
											entry->relFileNode.relNode,
											entry->segmentFileNum)));
							
							break;
						}						
						
						MirroredAppendOnly_Append(
											  &mirroredOpen,
											  buffer,
											  bufferLen,
											  &primaryError,
											  &mirrorDataLossOccurred);
						
						if (mirrorDataLossOccurred)
							break;

						Assert(primaryError == 0);	// No primary writes as resync worker.
						
						startOffset += bufferLen;
						/* AO and CO Data Store writes 64k size by default */
						bufferLen = (Size) Min(2*BLCKSZ, endOffset - startOffset);						
					}
					
					if (buffer) 
					{
						pfree(buffer);
						buffer = NULL;
					}
					
					if (mirrorDataLossOccurred)
						break;
					
					/* Flush written data on Mirror */
					MirroredAppendOnly_Flush(
										&mirroredOpen,
										&primaryError,
										&mirrorDataLossOccurred);
					if (mirrorDataLossOccurred)
						break;
					
					Assert(primaryError == 0);	// Not flushed on primary as resync worker.
					
					/* Close Primary and Mirror */
					MirroredAppendOnly_Close(
										&mirroredOpen,
										&mirrorDataLossOccurred);
								
					break;
					
				case MirroredRelDataSynchronizationState_None:										
				case MirroredRelDataSynchronizationState_DataSynchronized:
					break;					
					
				default:
					ereport(LOG, 
							(errmsg("could not resynchronize relation '%u/%u/%u' "
									"mirror synchronization state:'%s(%d)' ",
									entry->relFileNode.relNode,
									entry->relFileNode.spcNode,
									entry->relFileNode.dbNode,
									MirroredRelDataSynchronizationState_Name(entry->mirrorDataSynchronizationState),
									entry->mirrorDataSynchronizationState)));
					break;
			}
			
			break;
		}	//case
		default:
			Assert(0);
			break;
	} //switch
	
	if (mirrorDataLossOccurred)
		status = STATUS_ERROR;
	
	return status;
}

static int
FileRepPrimary_ResyncBufferPoolIncrementalWrite(ChangeTrackingRequest *request)
{
	int				status = STATUS_OK;
	Page			page;
	Buffer			buf; 
	BlockNumber		numBlocks = 0;
	SMgrRelation	smgr_relation = NULL;
	char			relidstr[OIDCHARS + 1 + OIDCHARS + 1 + OIDCHARS + 1];
	int				ii;
	XLogRecPtr		loc;
	XLogRecPtr		loc1;
	int				count = 0;
	int				thresholdCount = 0;
	bool			mirrorDataLossOccurred = FALSE;
	int				NumberOfRelations = request->count;
	
	FileRepResyncHashEntry_s	entry;
	ChangeTrackingResult		*result = NULL;	

	while (1)
	{
		/* allow flushing buffers from buffer pool during scan */
		FileRepResync_SetReadBufferRequest();
		if ((result = ChangeTracking_GetChanges(request)) != NULL) 
		{
			FileRepResync_ResetReadBufferRequest();
					
			for (ii = 0; ii < result->count; ii++)
			{
				
				if (smgr_relation == NULL)
				{
					NumberOfRelations--;
					
					smgr_relation = smgropen(result->entries[ii].relFileNode);
					
					snprintf(relidstr, sizeof(relidstr), "%u/%u/%u",
							 smgr_relation->smgr_rnode.spcNode,
							 smgr_relation->smgr_rnode.dbNode,
							 smgr_relation->smgr_rnode.relNode);

					numBlocks = smgrnblocks(smgr_relation);
					
					if (Debug_filerep_print)
						elog(LOG, "resynchronize buffer pool relation '%u/%u/%u' "
							 "number of blocks:'%u' ",
							 smgr_relation->smgr_rnode.spcNode,
							 smgr_relation->smgr_rnode.dbNode,
							 smgr_relation->smgr_rnode.relNode,
							 numBlocks);
					
					thresholdCount = Min(numBlocks, 1024);
				}
				
				loc1 =  result->entries[ii].lsn_end;
				
				/*
				 * if relation was truncated then block_num from change tracking can be beyond numBlocks 
				 */
				if (result->entries[ii].block_num >=  numBlocks)
				{
					ereport(LOG,	
							(errmsg("could not resynchonize buffer pool relation '%s' block '%d' (maybe due to truncate), "
									"lsn change tracking '%s(%u/%u)' "
									"number of blocks '%d' ",
									relidstr,
									result->entries[ii].block_num,
									XLogLocationToString(&loc1),
									loc1.xlogid,
									loc1.xrecoff,
									numBlocks),						
							 FileRep_errcontext()));						
					
					goto flush_check;
				}
				
				/* allow flushing buffers from buffer pool during scan */
				FileRepResync_SetReadBufferRequest();
				buf = ReadBuffer_Resync(smgr_relation,
										result->entries[ii].block_num);
				FileRepResync_ResetReadBufferRequest();
				
				Assert(result->entries[ii].block_num < numBlocks);
				
				LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
				page = BufferGetPage(buf);
				
				loc = PageGetLSN(page); 
				
				if(Debug_filerep_print)
				{
					elog(LOG,	
							"incremental resync buffer pool identifier '%s' num blocks '%d' blkno '%d' lsn page '%s(%u/%u)' "
							"lsn end change tracking '%s(%u/%u)' ",
							relidstr,
							numBlocks,
							result->entries[ii].block_num,
							XLogLocationToString(&loc),
							loc.xlogid,
							loc.xrecoff,
							XLogLocationToString(&loc1),
							result->entries[ii].lsn_end.xlogid,
							result->entries[ii].lsn_end.xrecoff);					
				}
				else
				{
					char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];
					
					snprintf(tmpBuf, sizeof(tmpBuf), 
							 "incremental resync buffer pool identifier '%s' num blocks '%d' blkno '%d' lsn page '%s(%u/%u)' ",
							 relidstr,
							 numBlocks,
							 result->entries[ii].block_num,
							 XLogLocationToString(&loc),
							 loc.xlogid,
							 loc.xrecoff);
					
					FileRep_InsertConfigLogEntry(tmpBuf);
					
					snprintf(tmpBuf, sizeof(tmpBuf), 
							 "incremental resync buffer pool identifier '%s' lsn end change tracking '%s(%u/%u)' ",
							 relidstr,
							 XLogLocationToString(&loc1),
							 result->entries[ii].lsn_end.xlogid,
							 result->entries[ii].lsn_end.xrecoff);
					
					FileRep_InsertConfigLogEntry(tmpBuf);
					
				}
								
				if (XLByteLE(result->entries[ii].lsn_end, PageGetLSN(page)))
				{
					if (! XLByteEQ(PageGetLSN(page), result->entries[ii].lsn_end))
					{
						ereport(LOG,
							(errmsg("Resynchonize buffer pool relation '%s' block '%d' has page lsn less than CT lsn, "
								"lsn end change tracking '%s(%u/%u)' lsn page '%s(%u/%u)' "
								"number of blocks '%d'",
								relidstr,
								result->entries[ii].block_num,
								XLogLocationToString(&loc),
								loc.xlogid,
								loc.xrecoff,
								XLogLocationToString(&loc1),
								loc1.xlogid,
								loc1.xrecoff,
								numBlocks),
							 FileRep_errcontext()));

					}

					/*
					 * It's safe and better to perform write of the page to mirror,
					 * for this case, as primary and mirror data pages should always
					 * be same. So, we might do some extra work but definitely won't
					 * loose out blocks, or error out and need to perform full recovery.
					 * Need to cover for this case as there are some known scenarios where
					 * CT file can have extra records which should have been discarded,
					 * but as we loose out information of xlog LSN cannot be discarded.
					 * One such case is when CT_TRANSIENT being compacted to CT_COMPACT
					 * with specific xlog LSN (to discard extra records) in CT mode gets
					 * interrupted by resync. Compaction during Resync collects all the
					 * CT records and doesn't have xlog LSN information to discard any
					 * extra records from CT_TRANSIENT.
					 */

					smgrwrite(smgr_relation,
							  result->entries[ii].block_num,
							  (char *)BufferGetBlock(buf),
							  FALSE);
				}

#ifdef FAULT_INJECTOR	
				FaultInjector_InjectFaultIfSet(
											   FileRepResyncWorker, 
											   DDLNotSpecified,
											   "",	// databaseName
											   ""); // tableName
#endif				
				
				UnlockReleaseBuffer(buf);
				
#ifdef FAULT_INJECTOR	
				FaultInjector_InjectFaultIfSet(
											   FileRepResyncWorker, 
											   DDLNotSpecified,
											   "",	// databaseName
											   ""); // tableName
#endif				
		
	flush_check:			
				if (((ii + 1) == result->count) ||
					! (result->entries[ii].relFileNode.spcNode == result->entries[ii+1].relFileNode.spcNode &&
					   result->entries[ii].relFileNode.dbNode == result->entries[ii+1].relFileNode.dbNode &&
					   result->entries[ii].relFileNode.relNode == result->entries[ii+1].relFileNode.relNode))
				{
					if (result->ask_for_more == false)
					{
								
						smgrimmedsync(smgr_relation);
						
						smgrclose(smgr_relation);
								 
						smgr_relation = NULL;
							
						FileRep_GetRelationPath(
												 entry.fileName, 
												 result->entries[ii].relFileNode, 
												 0 /* segment file number is always 0 for Buffer Pool */);							 
								 
						status = FileRepResync_UpdateEntry(&entry);
						if (status != STATUS_OK)
						{
							 break;
						}
					}
								 
				}			
							
				if (count > thresholdCount)
				{
					count = 0;
					FileRepSubProcess_ProcessSignals();
					
					if (! (FileRepSubProcess_GetState() == FileRepStateReady && 
						   dataState == DataStateInResync))
					{
						mirrorDataLossOccurred = TRUE;
						break;
					}
				}
				else
					count++;
			}  // for (ii = 0; ii < result->count; ii++)
			
		} // if ((result = ChangeTracking_GetChanges(request)) != NULL) 
		
		FileRepResync_ResetReadBufferRequest();
			
		if (result != NULL && result->ask_for_more == true)
		{
			Assert(request->count == 1);
			request->entries[0].lsn_start = result->next_start_lsn;
		}
		else
		{
			break;
		}

	} // while(1) 
		
	ChangeTracking_FreeRequest(request);
	ChangeTracking_FreeResult(result);
	
	Insist(NumberOfRelations == 0);
	
	if (mirrorDataLossOccurred)
		status = STATUS_ERROR;
	
	return status;	
}
