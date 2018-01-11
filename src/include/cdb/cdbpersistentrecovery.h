/*-------------------------------------------------------------------------
 *
 * cdbpersistentrecovery.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentrecovery.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTRECOVERY_H
#define CDBPERSISTENTRECOVERY_H

#include "access/xlogdefs.h"
#include "access/xlog.h"
#include "cdb/cdbpersistentstore.h"

inline static int PersistentRecovery_DebugPrintLevel(void)
{
	if (Debug_persistent_bootstrap_print && IsBootstrapProcessingMode())
		return WARNING;
	else
		return Debug_persistent_recovery_print_level;
}

extern bool
PersistentRecovery_ShouldHandlePass1XLogRec(
	XLogRecPtr		*beginLoc,

	XLogRecPtr 		*lsn,
	
	XLogRecord 		*record);

extern void
PersistentRecovery_HandlePass2XLogRec(
	XLogRecPtr		*beginLoc,

	XLogRecPtr 		*lsn,
	
	XLogRecord 		*record);

extern bool
PersistentRecovery_ShouldHandlePass3XLogRec(
	XLogRecPtr		*beginLoc,

	XLogRecPtr 		*lsn,
	
	XLogRecord 		*record);

extern void
PersistentRecovery_CompletePass(void);

extern void
PersistentRecovery_Scan(void);

extern void
PersistentRecovery_CrashAbort(void);

extern void
PersistentRecovery_Update(void);

extern void
PersistentRecovery_Drop(void);

extern void
PersistentRecovery_UpdateAppendOnlyMirrorResyncEofs(void);

extern void
PersistentRecovery_SerializeRedoRelationFile(
	int redoRelationFile);

extern void
PersistentRecovery_DeserializeRedoRelationFile(
	int redoRelationFile);

#endif   /* CDBPERSISTENTRECOVERY_H */
