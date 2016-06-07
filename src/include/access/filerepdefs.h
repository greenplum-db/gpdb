/*
 *  cdbfilerepdefs.h
 *  
 * Definitions needed by in header files outside of cdb.
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef FILEREPDEFS_H
#define FILEREPDEFS_H

#include "storage/relfilenode.h"

typedef enum MirrorDataLossTrackingState {
	MirrorDataLossTrackingState_MirrorNotConfigured=0,
	
	MirrorDataLossTrackingState_MirrorCurrentlyUpInSync,
	
	MirrorDataLossTrackingState_MirrorCurrentlyUpInResync,
	
	MirrorDataLossTrackingState_MirrorDown,
	
} MirrorDataLossTrackingState;


typedef struct MirroredBufferPoolBulkLoadInfo
{
	MirrorDataLossTrackingState	mirrorDataLossTrackingState;
	
	int64 						mirrorDataLossTrackingSessionNum;
	
	RelFileNode 				relFileNode;

	ItemPointerData				persistentTid;

	bool tidAllowedToBeZero;

	int64						persistentSerialNum;

} MirroredBufferPoolBulkLoadInfo;

extern char * 
MirrorDataLossTrackingState_Name(MirrorDataLossTrackingState state);

#endif   /* FILEREPDEFS_H */
