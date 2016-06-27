/*-------------------------------------------------------------------------
 *
 * cdbdtxcontextinfo.h
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDTXCONTEXTINFO_H
#define CDBDTXCONTEXTINFO_H

#define DtxContextInfo_StaticInit {0,InvalidDistributedTransactionId,TmGid_Init,0,false,false,DistributedSnapshot_StaticInit,0,0}

typedef struct DtxContextInfo
{
	DistributedTransactionTimeStamp	distributedTimeStamp;
	
	DistributedTransactionId 		distributedXid;
	
	char					 		distributedId[TMGIDSIZE];

	CommandId				 		curcid;	/* in my xact, CID < curcid are visible */

	bool							haveDistributedSnapshot;
	bool							cursorContext;
	
	DistributedSnapshot		 		distributedSnapshot;

	int 							distributedTxnOptions;

	uint32							segmateSync;
	uint32							nestingLevel;
} DtxContextInfo;

extern DtxContextInfo QEDtxContextInfo;	

extern void DtxContextInfo_Reset(DtxContextInfo *dtxContextInfo);
extern void DtxContextInfo_CreateOnMaster(DtxContextInfo *dtxContextInfo,
										  DistributedSnapshotWithLocalMapping *dslm,
										  CommandId curcid,
										  int txnOptions);
extern int DtxContextInfo_SerializeSize(DtxContextInfo *dtxContextInfo);

extern void DtxContextInfo_Serialize(char *buffer, DtxContextInfo *dtxContextInfo);
extern void DtxContextInfo_Deserialize(const char *serializedDtxContextInfo,
									   int serializedDtxContextInfolen,
									   DtxContextInfo *dtxContextInfo);

extern void DtxContextInfo_Copy(DtxContextInfo *target, DtxContextInfo *source);

extern void DtxContextInfo_RewindSegmateSync(void);

#endif   /* CDBDTXCONTEXTINFO_H */
