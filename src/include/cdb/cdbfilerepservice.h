/*
 *  cdbfilerepservice.h
 *  
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef CDBFILEREPSERVICE_H
#define CDBFILEREPSERVICE_H

#include "cdb/cdbfilerep.h"



extern FileRepProcessType_e fileRepProcessType;

extern void FileRepSubProcess_Main(void);

extern void FileRepSubProcess_SetState(
					FileRepState_e fileRepStateLocal);

/*
 * Return state of FileRep process.
 */
extern FileRepState_e FileRepSubProcess_GetState(void);

extern void FileRep_SetResynchronizeReady(XLogRecPtr *resyncLoc);

extern bool FileRep_IsInRecovery(void);

extern bool FileRep_IsInResynchronizeReady(void);

/*
 * Process signals 
 * Return TRUE if shutdown was requested
 */	
extern bool FileRepSubProcess_ProcessSignals(void);

extern bool FileRepSubProcess_IsStateTransitionRequested(void);

extern void FileRepSubProcess_InitHeapAccess(void);
#endif   /* CDBFILEREPSERVICE_H */

