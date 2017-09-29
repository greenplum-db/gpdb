/*-------------------------------------------------------------------------
 *
 * cdbfilerepprimaryrecovery.c
 *  
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepprimaryrecovery.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>

#include "access/xlog.h"
#include "access/twophase.h"
#include "access/slru.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerepprimaryrecovery.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbmirroredflatfile.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "utils/flatfiles.h"

static void FileRepPrimary_StartRecoveryInSync(void);
static void FileRepPrimary_StartRecoveryInChangeTracking(void);
static void FileRepPrimary_RunChangeTrackingCompacting(void);

static int FileRepPrimary_RunRecoveryInSync(void);

static void FileRepPrimary_RunHeartBeat(void);

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Primary RECOVERY Process)
 ****************************************************************/

void 
FileRepPrimary_StartRecovery(void)
{		
	FileRep_InsertConfigLogEntry("start recovery");
	
	switch (dataState)
	{
		case DataStateInSync:
			FileRepPrimary_StartRecoveryInSync();
			FileRepPrimary_RunHeartBeat();
			break;
			
		case DataStateInChangeTracking:
			FileRepPrimary_StartRecoveryInChangeTracking();
			FileRepPrimary_RunChangeTrackingCompacting();
			break;
			
		case DataStateInResync:
			FileRepPrimary_RunHeartBeat();
			break;
			
		default:
			Assert(0);
			break;
	}
}	

/****************************************************************
 * DataStateInSync
 ****************************************************************/

/*
 * 
 * FileRepPrimary_StartRecoveryInSync()
 *
 *
 */
static void 
FileRepPrimary_StartRecoveryInSync(void)
{	
	int	status = STATUS_OK;
	
	FileRep_InsertConfigLogEntry("run recovery");
	
	while (1) {
		
		if (status != STATUS_OK) 
		{
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			FileRepSubProcess_SetState(FileRepStateFault);
		}
		
		while (FileRepSubProcess_GetState() == FileRepStateFault ||
			   
			   (fileRepShmemArray[0]->state == FileRepStateNotInitialized &&
				FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
			    FileRepSubProcess_GetState() != FileRepStateShutdown)) {
			
			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */	
		}
		
		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends) {
			
			break;
		}
		
		if (FileRepSubProcess_GetState() == FileRepStateReady) {
			break;
		}
		
		Insist(fileRepRole == FileRepPrimaryRole);

		Insist(dataState == DataStateInSync);
		
		status = FileRepPrimary_RunRecoveryInSync();

		
	}
}

/*-----------------------------------------
 * FileRepPrimary_RunRecoveryInSync()
 *		
 *		1) Recover Flat Files
 *			a) pg_control file
 *			b) pg_database file
 *			c) pg_auth file
 *			d) pg_twophase directory
 *			e) Slru directories 
 *					*) pg_clog 
 *					*) pg_multixact 
 *					*) pg_distributedlog
 *					*) pg_distributedxidmap
 *					*) pg_subtrans
 *
 *		2) Reconcile xlog EOF
 *-----------------------------------------
 */
static int 
FileRepPrimary_RunRecoveryInSync(void)
{	
	int status = STATUS_OK;
	
	FileRep_InsertConfigLogEntry("run recovery of flat files");
	
	while (1) {
		
		status = XLogRecoverMirrorControlFile();
		
		if (status != STATUS_OK) {
			break;
		}
		
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}		
		
		status = XLogReconcileEofPrimary();
		
		if (status != STATUS_OK) {
			break;
		}
		
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}
		
		MirroredFlatFile_DropTemporaryFiles();
				
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}				

		MirroredFlatFile_MirrorDropTemporaryFiles();

		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}						
		
		status = FlatFilesRecoverMirror();
		
		if (status != STATUS_OK) {
			break;
		}
		
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}		

		status = TwoPhaseRecoverMirror();

		if (status != STATUS_OK) {
			break;
		}
		
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}	
		
		status = SlruRecoverMirror();
		
		if (status != STATUS_OK) {
			break;
		}
		
		FileRepSubProcess_ProcessSignals();
		if (FileRepSubProcess_GetState() != FileRepStateInitialization) {
			break;
		}			
				
		FileRepSubProcess_SetState(FileRepStateReady);
		break;
	}
	
	
	return status;
}

/****************************************************************
 * DataStateInChangeTracking
 ****************************************************************/

/*
 * 
 * FileRepPrimary_StartRecoveryInChangeTracking()
 *
 */
static void 
FileRepPrimary_StartRecoveryInChangeTracking(void)
{		
	FileRep_InsertConfigLogEntry("run recovery");
	
	while (1) {
					
		while (FileRepSubProcess_GetState() == FileRepStateFault) {			
			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */	
		}
		
		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends) {
			
			break;
		}
				
		Insist(fileRepRole == FileRepPrimaryRole);
		Insist(dataState == DataStateInChangeTracking);		
		Insist(FileRepSubProcess_GetState() != FileRepStateReady);
		
		if (ChangeTracking_RetrieveIsTransitionToInsync())
		{
			ChangeTracking_DropAll();
		}
		else
		{
			if (ChangeTracking_RetrieveIsTransitionToResync() == FALSE &&
				isFullResync())
			{
				ChangeTracking_MarkFullResync();
				/* segmentState == SegmentStateChangeTrackingDisabled */
				getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
				Assert(segmentState == SegmentStateChangeTrackingDisabled);
				
				/* database is resumed */
				primaryMirrorSetIOSuspended(FALSE);
				
				FileRep_InsertConfigLogEntry("change tracking recovery completed");
				
				break;				
			}
			else
			{
				ChangeTracking_MarkIncrResync();
			}
			
		}
		
		XLogInChangeTrackingTransition();

		getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
		if (segmentState != SegmentStateChangeTrackingDisabled)
		{
			/* NOTE: Any error during change tracking will result in disabling Change Tracking */
			FileRepSubProcess_SetState(FileRepStateReady);
		}
		
		/* database is resumed */
		primaryMirrorSetIOSuspended(FALSE);
				
		FileRep_InsertConfigLogEntry("change tracking recovery completed");
		
		break;
		
	}
}

/*
 * 
 * FileRepPrimary_RunChangeTrackingCompacting()
 *
 */
static void 
FileRepPrimary_RunChangeTrackingCompacting(void)
{
	int		retry = 0;

	if (segmentState != SegmentStateChangeTrackingDisabled)
		FileRep_InsertConfigLogEntry("run change tracking compacting if records has to be discarded");
	else
		FileRep_InsertConfigLogEntry("change tracking disabled, so skipping compaction");

	/*
	 * We have to check if any records have to be discarded from Change Tracking log file.
	 * Due to crash it can happen that the highest change tracking log lsn > the highest xlog lsn.
	 *
	 * Records from change tracking log file can be discarded only after database is started. 
	 * Full environhment has to be set up in order to run queries over SPI.
	 */
	while (FileRepSubProcess_GetState() != FileRepStateShutdown &&
		   FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
		   isDatabaseRunning() == FALSE) 
	{
		
		FileRepSubProcess_ProcessSignals();
		
		pg_usleep(50000L); /* 50 ms */	
	}		

	/*
	 * It is safe to initialize relcache and use heap access methods
	 * now, after crash recovery passes have finished applying xlog.
	 */
	FileRepSubProcess_InitHeapAccess();

	if (segmentState != SegmentStateChangeTrackingDisabled)
		ChangeTracking_DoFullCompactingRoundIfNeeded();

	/*---------------------------------------------------------------------
	 * Periodically check if compacting is required. 
	 * Periodic compacting is required in order to
	 *		a) reduce space for change tracking log file
	 *		b) reduce time for transition from Change Tracking to Resync
	 *---------------------------------------------------------------------
	 */
	if (segmentState != SegmentStateChangeTrackingDisabled)
		FileRep_InsertConfigLogEntry("run change tracking compacting");
	while (1) {
		
		FileRepSubProcess_ProcessSignals();
		
		while (FileRepSubProcess_GetState() == FileRepStateFault ||
			   segmentState == SegmentStateChangeTrackingDisabled) 
		{			
			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */	
		}
		
		if (! (FileRepSubProcess_GetState() == FileRepStateReady &&
			   dataState == DataStateInChangeTracking))
		{
			break;
		}				
				
		Insist(fileRepRole == FileRepPrimaryRole);
		Insist(dataState == DataStateInChangeTracking);		
		Insist(FileRepSubProcess_GetState() == FileRepStateReady);
		
		/* retry compacting of change tracking log files once per minute */
		pg_usleep(50000L); /* 50 ms */
		
		if (++retry == 1200)
		{
			ChangeTracking_CompactLogsIfPossible(); 
			retry=0;
		}
	} 
}
	
/*
 * 
 * FileRepPrimary_RunHeartBeat()
 *
 *
 */
static void 
FileRepPrimary_RunHeartBeat(void)
{	
	int retry = 0;
	
	Insist(fileRepRole == FileRepPrimaryRole);
	
	Insist(dataState == DataStateInSync ||
		   dataState == DataStateInResync);	
	
	while (1) 
	{
		FileRepSubProcess_ProcessSignals();
		
		while (FileRepSubProcess_GetState() == FileRepStateFault ||
			   
			   (fileRepShmemArray[0]->state == FileRepStateNotInitialized &&
				FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
			    FileRepSubProcess_GetState() != FileRepStateShutdown)) {
			
			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */	
		}
		
		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends) {
			
			break;
		}

		/* verify if flow from primary to mirror and back is alive once per minute */
		pg_usleep(50000L); /* 50 ms */
		
		if (FileRepSubProcess_ProcessSignals() == true ||
			FileRepSubProcess_GetState() == FileRepStateFault)
		{
			continue;
		}
		
		retry++;
		if (retry == 1200) /* 1200 * 50 ms = 60 sec */
		{
			FileRepPrimary_MirrorHeartBeat(FileRepMessageTypeXLog);
			continue;
		}
		
		if (retry == 1201) /* 1200 * 50 ms = 60 sec */
		{
			FileRepPrimary_MirrorHeartBeat(FileRepMessageTypeWriter);
			continue;
		}
		
		if (retry == 1202) /* 1200 * 50 ms = 60 sec */
		{
			FileRepPrimary_MirrorHeartBeat(FileRepMessageTypeAO01);
			retry = 0;
		}
	}
}
