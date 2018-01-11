/*-------------------------------------------------------------------------
 *
 * cdbmirroredfilesysobj.c
 *	  Create and drop mirrored files and directories.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbmirroredfilesysobj.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#ifndef WIN32
#include <sys/fcntl.h>
#else
#include <io.h>
#endif
#include <sys/file.h>
#include <unistd.h>

#include <signal.h>

#include "cdb/cdbmirroredfilesysobj.h"
#include "storage/smgr.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "catalog/storage.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentfilespace.h"
#include "storage/lwlock.h"
#include "miscadmin.h"
#include "postmaster/primary_mirror_mode.h"
#include "cdb/cdbfilerep.h"
#include "catalog/catalog.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "utils/guc.h"
#include "cdb/cdbmirroredappendonly.h"

static void
MirroredFileSysObj_BeginMirroredCreate(
									   PersistentFileSysObjName *fsObjName,

									   MirrorDataLossTrackingState mirrorDataLossTrackingState,

									   MirroredObjectExistenceState *mirrorExistenceState,

									   MirroredRelDataSynchronizationState *relDataSynchronizationState,

									   StorageManagerMirrorMode *mirrorMode)
{
	/* Kludge for initdb */
	if (gp_initdb_mirrored)
	{
		*mirrorExistenceState = MirroredObjectExistenceState_MirrorCreatePending;
		*relDataSynchronizationState = MirroredRelDataSynchronizationState_DataSynchronized;
		*mirrorMode = StorageManagerMirrorMode_Both;

		if (Debug_persistent_print)
		{
			SUPPRESS_ERRCONTEXT_DECLARE;

			SUPPRESS_ERRCONTEXT_PUSH();

			elog(Persistent_DebugPrintLevel(),
				 "MirroredFileSysObj_BeginMirroredCreate (%s): -- initdb mirrored",
				 PersistentFileSysObjName_TypeAndObjectName(fsObjName));

			SUPPRESS_ERRCONTEXT_POP();
		}

		return;
	}

	switch (mirrorDataLossTrackingState)
	{
		case MirrorDataLossTrackingState_MirrorNotConfigured:
			*mirrorExistenceState = MirroredObjectExistenceState_NotMirrored;
			*relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
			*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			break;

		case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
			/* Since we are creating a new object. */
			*mirrorExistenceState = MirroredObjectExistenceState_MirrorCreatePending;
			*relDataSynchronizationState = MirroredRelDataSynchronizationState_DataSynchronized;
			*mirrorMode = StorageManagerMirrorMode_Both;
			break;

		case MirrorDataLossTrackingState_MirrorDown:
			*mirrorExistenceState = MirroredObjectExistenceState_MirrorDownBeforeCreate;
			*relDataSynchronizationState = MirroredRelDataSynchronizationState_FullCopy;
			*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
			break;

		default:
			elog(ERROR, "unexpected mirror data loss tracking state: %d",
				 mirrorDataLossTrackingState);

			*mirrorExistenceState = MirroredObjectExistenceState_None;
			*relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
			*mirrorMode = StorageManagerMirrorMode_None;
			/* A happy optimizer is the sound of one hand clapping. */
			break;
	}

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_BeginMirroredCreate (%s): mirror existence state '%s', relation data synchronization state '%s', mirror mode %s",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 MirroredObjectExistenceState_Name(*mirrorExistenceState),
			 MirroredRelDataSynchronizationState_Name(*relDataSynchronizationState),
			 StorageManagerMirrorMode_Name(*mirrorMode));

		SUPPRESS_ERRCONTEXT_POP();
	}
}

static void
MirroredFileSysObj_FinishMirroredCreate(
										PersistentFileSysObjName *fsObjName,

										ItemPointer persistentTid,

										int64 persistentSerialNum,

										MirroredObjectExistenceState mirrorExistenceState,

										bool mirrorDataLossOccurred)
{
	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_FinishMirroredCreate (%s): mirror existence state '%s', mirror data loss occurred %s, "
			 "serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 (mirrorDataLossOccurred ? "true" : "false"),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));

		SUPPRESS_ERRCONTEXT_POP();
	}

	if (mirrorExistenceState == MirroredObjectExistenceState_NotMirrored ||
		mirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate)
		return;

	Assert(mirrorExistenceState == MirroredObjectExistenceState_MirrorCreatePending);

	if (!mirrorDataLossOccurred)
	{
		return;
	}

	/*
	 * Our create attempt may or may not have succeeded.
	 */
	/* UNDONE: Don't fsync this change to the XLOG for now.  Determine */
	/* UNDONE: what needs to be fsync'd. */
	PersistentFileSysObj_ChangeMirrorState(
										   fsObjName->type,
										   persistentTid,
										   persistentSerialNum,
										   MirroredObjectExistenceState_MirrorDownDuringCreate,
										   MirroredRelDataSynchronizationState_FullCopy,
										    /* flushToXLog */ false);
}

void
MirroredFileSysObj_ValidateFilespaceDir(char *mirrorFilespaceLocation)
{
	char	   *parentdir = NULL;
	char	   *errdir = NULL;
	int			status;

	status = FileRepPrimary_MirrorValidation(
											 FileRep_GetDirFilespaceIdentifier(mirrorFilespaceLocation),
											 FileRepRelationTypeDir,
											 FileRepValidationFilespace
		);

	/*
	 * Check return status and return or report error.
	 *
	 * Known errors: - File or directory exists - Parent directory does not
	 * exist - Parent directory not a directory - No write permission on
	 * parent directory
	 */
	switch (status)
	{
		case FileRepStatusSuccess:
			return;

			/* Errors on the parent directory */
		case FileRepStatusNoSuchFileOrDirectory:
		case FileRepStatusNotDirectory:
			parentdir = pstrdup(mirrorFilespaceLocation);
			get_parent_directory(parentdir);
			errdir = parentdir;
			break;

			/* All other errors use the specified directory */
		case FileRepStatusDirectoryExist:
		case FileRepStatusNoPermissions:
		default:
			errdir = mirrorFilespaceLocation;
			break;
	}
	ereport(ERROR,
			(errcode_for_file_access(),
			 errmsg("%s: %s", errdir, FileRepStatusToString[status])));

	/* unreachable */
	return;
}

void
MirroredFileSysObj_TransactionCreateFilespaceDir(
												 Oid filespaceOid,

												 int16 primaryDbId,

												 char *primaryFilespaceLocation,

 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */

												 int16 mirrorDbId,

												 char *mirrorFilespaceLocation,

												 ItemPointer persistentTid,
 /* Output: The TID of the gp_persistent_filespace_node tuple. */

												 int64 *persistentSerialNum)
 /* Output: The serial number of the gp_persistent_filespace_node tuple. */
{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;
	int			primaryError;
	bool		mirrorDataLossOccurred;

	Assert(primaryFilespaceLocation != NULL);

	PersistentFileSysObjName_SetFilespaceDir(
											 &fsObjName,
											 filespaceOid);

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	/*
	 * MPP-8595 - In order to prevent commiting to filespace locations that
	 * can never be instatiated (parent directory doesn't exist, path
	 * conflicts with another segment, etc) the only solution is to block
	 * filespace creation if we do not have connectivity to the mirror.
	 *
	 * In order to remove this restriction it will be necessary to tightly
	 * integrate with gprecoverseg support and be able to change the directory
	 * of a filespace during recovery as a means of fixing badly specified
	 * filespace locations.
	 *
	 * If this is removed the corresponding check should also be removed in
	 * MirroredFileSysObj_ScheduleDropFilespaceDir.
	 */
	switch (mirrorDataLossTrackingState)
	{
		case MirrorDataLossTrackingState_MirrorNotConfigured:
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			break;

			/*
			 * We might be able to handle resync mode, but it is disabled for
			 * now as a matter of precaution.
			 */
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
			ereport(ERROR,
					(errcode(ERRCODE_GP_COMMAND_ERROR),
					 errmsg("cannot CREATE FILESPACE in resync mode"),
					 errhint("run gprecoverseg to re-establish mirror connectivity")));
			break;

		case MirrorDataLossTrackingState_MirrorDown:
			ereport(ERROR,
					(errcode(ERRCODE_GP_COMMAND_ERROR),
					 errmsg("cannot CREATE FILESPACE in change tracking mode"),
					 errhint("run gprecoverseg to re-establish mirror connectivity")));
			break;

		default:
			elog(ERROR, "unexpected mirror data loss tracking state: %d",
				 mirrorDataLossTrackingState);
			break;
	}

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);

	/*
	 * We write our intention or 'Create Pending' persistent information
	 * before we do any create filespace work on either the primary or mirror.
	 */
	smgrcreatefilespacedirpending(
								  filespaceOid,
								  primaryDbId,
								  primaryFilespaceLocation,
								  mirrorDbId,
								  mirrorFilespaceLocation,
								  mirrorExistenceState,
								  persistentTid,
								  persistentSerialNum,
								   /* flushToXLog */ true);

	/*
	 * Synchronous primary and mirror create tablespace directory.
	 */
	MirroredFileSysObj_CreateFilespaceDir(
										  filespaceOid,
										  primaryFilespaceLocation,
										  mirrorFilespaceLocation,
										  mirrorMode,
										   /* ignoreAlreadyExists */ false,
										  &primaryError,
										  &mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		LWLockRelease(MirroredLock);
		/* UNDONE: Need equiv. of GetDatabasePath here for filespace. */
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create filespace directory %s: %s",
						primaryFilespaceLocation,
						strerror(primaryError))));
	}

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											persistentTid,
											*persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);

	LWLockRelease(MirroredLock);
}

void
MirroredFileSysObj_CreateFilespaceDir(
									  Oid filespaceOid,

									  char *primaryFilespaceLocation,

 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */

									  char *mirrorFilespaceLocation,

									  StorageManagerMirrorMode mirrorMode,

									  bool ignoreAlreadyExists,

									  int *primaryError,

									  bool *mirrorDataLossOccurred)
{
	smgrcreatefilespacedir(
						   filespaceOid,
						   primaryFilespaceLocation,
						   mirrorFilespaceLocation,
						   mirrorMode,
						   ignoreAlreadyExists,
						   primaryError,
						   mirrorDataLossOccurred);
}

void
MirroredFileSysObj_ScheduleDropFilespaceDir(
											Oid filespaceOid)
{
	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;
	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	/*
	 * MPP-9893 - Dropping the filespace while in resync mode caused the
	 * mirrors to be marked as dropped and revert back to change tracking
	 * mode.
	 *
	 * Since we disable CREATE FILESPACE when in change-tracking/resync the
	 * simplest solution was to simply make the disabling symetric and disable
	 * dropping them in change-tracking/resync as well.
	 */
	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);
	switch (mirrorDataLossTrackingState)
	{
		case MirrorDataLossTrackingState_MirrorNotConfigured:
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			break;

		case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
			ereport(ERROR,
					(errcode(ERRCODE_GP_COMMAND_ERROR),
					 errmsg("cannot DROP FILESPACE in resync mode"),
					 errhint("run gprecoverseg to re-establish mirror connectivity")));
			break;

		case MirrorDataLossTrackingState_MirrorDown:
			ereport(ERROR,
					(errcode(ERRCODE_GP_COMMAND_ERROR),
					 errmsg("cannot DROP FILESPACE in change tracking mode"),
					 errhint("run gprecoverseg to re-establish mirror connectivity")));
			break;

		default:
			elog(ERROR, "unexpected mirror data loss tracking state: %d",
				 mirrorDataLossTrackingState);
			break;
	}

	PersistentFilespace_LookupTidAndSerialNum(
											  filespaceOid,
											  &persistentTid,
											  &persistentSerialNum);

	smgrschedulermfilespacedir(
							   filespaceOid,
							   &persistentTid,
							   persistentSerialNum);
}

void
MirroredFileSysObj_DropFilespaceDir(
									Oid filespaceOid,

									char *primaryFilespaceLocation,

 /*
  * The primary filespace directory path.  NOT Blank padded. Just a NULL
  * terminated string.
  */

									char *mirrorFilespaceLocation,

									bool primaryOnly,

									bool mirrorOnly,

									bool ignoreNonExistence,

									bool *mirrorDataLossOccurred)
{
	Assert(primaryFilespaceLocation != NULL);

	smgrdormfilespacedir(
						 filespaceOid,
						 primaryFilespaceLocation,
						 mirrorFilespaceLocation,
						 primaryOnly,
						 mirrorOnly,
						 ignoreNonExistence,
						 mirrorDataLossOccurred);

}

void
MirroredFileSysObj_TransactionCreateTablespaceDir(
												  TablespaceDirNode *tablespaceDirNode,

												  ItemPointer persistentTid,
 /* Output: The TID of the gp_persistent_tablespace_node tuple. */

												  int64 *persistentSerialNum)
 /* Output: The serial number of the gp_persistent_tablespace_node tuple. */
{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;
	int			primaryError;
	bool		mirrorDataLossOccurred;

	PersistentFileSysObjName_SetTablespaceDir(
											  &fsObjName,
											  tablespaceDirNode->tablespace);

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);

	/*
	 * We write our intention or 'Create Pending' persistent information
	 * before we do any create tablespace directory work on either the primary
	 * or mirror.
	 */
	smgrcreatetablespacedirpending(
								   tablespaceDirNode,
								   mirrorExistenceState,
								   persistentTid,
								   persistentSerialNum,
								    /* flushToXLog */ true);

	/*
	 * Synchronous primary and mirror create tablespace directory.
	 */
	MirroredFileSysObj_CreateTablespaceDir(
										   tablespaceDirNode->tablespace,
										   mirrorMode,
										    /* ignoreAlreadyExists */ false,
										   &primaryError,
										   &mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		LWLockRelease(MirroredLock);
		/* UNDONE: Need equiv. of GetDatabasePath here for tablespace. */
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create tablespace directory %u: %s",
						tablespaceDirNode->tablespace,
						strerror(primaryError))));
	}

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											persistentTid,
											*persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);

	LWLockRelease(MirroredLock);
}

void
MirroredFileSysObj_CreateTablespaceDir(
									   Oid tablespaceOid,

									   StorageManagerMirrorMode mirrorMode,

									   bool ignoreAlreadyExists,

									   int *primaryError,

									   bool *mirrorDataLossOccurred)
{
	smgrcreatetablespacedir(
							tablespaceOid,
							mirrorMode,
							ignoreAlreadyExists,
							primaryError,
							mirrorDataLossOccurred);
}

void
MirroredFileSysObj_ScheduleDropTablespaceDir(
											 Oid tablespaceOid)
{
	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	PersistentTablespace_LookupTidAndSerialNum(
											   tablespaceOid,
											   &persistentTid,
											   &persistentSerialNum);

	smgrschedulermtablespacedir(
								tablespaceOid,
								&persistentTid,
								persistentSerialNum);
}

void
MirroredFileSysObj_DropTablespaceDir(
									 Oid tablespaceOid,

									 bool primaryOnly,

									 bool mirrorOnly,

									 bool ignoreNonExistence,

									 bool *mirrorDataLossOccurred)
{
	smgrdormtablespacedir(
						  tablespaceOid,
						  primaryOnly,
						  mirrorOnly,
						  ignoreNonExistence,
						  mirrorDataLossOccurred);
}

void
MirroredFileSysObj_TransactionCreateDbDir(
										  DbDirNode *dbDirNode,

										  ItemPointer persistentTid,
 /* Output: The TID of the gp_persistent_relation_node tuple. */

										  int64 *persistentSerialNum)
 /* Output: The serial number of the gp_persistent_relation_node tuple. */

{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;
	int			primaryError;
	bool		mirrorDataLossOccurred;

	PersistentFileSysObjName_SetDatabaseDir(
											&fsObjName,
											dbDirNode->tablespace,
											dbDirNode->database);

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);

	/*
	 * We write our intention or 'Create Pending' persistent information
	 * before we do any create database directory work on either the primary
	 * or mirror.
	 */
	smgrcreatedbdirpending(
						   dbDirNode,
						   mirrorExistenceState,
						   persistentTid,
						   persistentSerialNum,
						    /* flushToXLog */ true);

	/*
	 * Synchronous primary and mirror create database directory.
	 */
	MirroredFileSysObj_CreateDbDir(
								   dbDirNode,
								   mirrorMode,
								    /* ignoreAlreadyExists */ false,
								   &primaryError,
								   &mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		LWLockRelease(MirroredLock);
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create database directory %s: %s",
						GetDatabasePath(dbDirNode->database, dbDirNode->tablespace),
						strerror(primaryError))));
	}

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											persistentTid,
											*persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);

	LWLockRelease(MirroredLock);
}

void
MirroredFileSysObj_CreateDbDir(
							   DbDirNode *dbDirNode,

							   StorageManagerMirrorMode mirrorMode,

							   bool ignoreAlreadyExists,

							   int *primaryError,

							   bool *mirrorDataLossOccurred)
{
	Assert(dbDirNode != NULL);

	smgrcreatedbdir(
					dbDirNode,
					mirrorMode,
					ignoreAlreadyExists,
					primaryError,
					mirrorDataLossOccurred);
}

void
MirroredFileSysObj_ScheduleDropDbDir(
									 DbDirNode *dbDirNode,

									 ItemPointer persistentTid,

									 int64 persistentSerialNum)
{
	Assert(dbDirNode != NULL);

	smgrschedulermdbdir(
						dbDirNode,
						persistentTid,
						persistentSerialNum);

}

void
MirroredFileSysObj_DropDbDir(
							 DbDirNode *dbDirNode,

							 bool primaryOnly,

							 bool mirrorOnly,

							 bool ignoreNonExistence,

							 bool *mirrorDataLossOccurred)
{
	Assert(dbDirNode != NULL);

	smgrdormdbdir(
				  dbDirNode,
				  primaryOnly,
				  mirrorOnly,
				  ignoreNonExistence,
				  mirrorDataLossOccurred);
}

static void
MirroredFileSysObj_JustInTimeDbDirCreate(
										 DbDirNode *justInTimeDbDirNode)
{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;

	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	int			primaryError;
	bool		mirrorDataLossOccurred;

	Assert(justInTimeDbDirNode != NULL);

	if (justInTimeDbDirNode->tablespace == GLOBALTABLESPACE_OID ||
		(justInTimeDbDirNode->tablespace == DEFAULTTABLESPACE_OID &&
		 justInTimeDbDirNode->database == TemplateDbOid))
	{
		/*
		 * Optimize out the common cases.
		 */
		return;
	}

	PersistentFileSysObjName_SetDatabaseDir(
											&fsObjName,
											justInTimeDbDirNode->tablespace,
											justInTimeDbDirNode->database);

	/*
	 * Acquire TablespaceCreateLock to ensure that no DROP TABLESPACE or
	 * MirroredFileSysObj_JustInTimeDbDirCreate is running concurrently.
	 */
	LWLockAcquire(TablespaceCreateLock, LW_EXCLUSIVE);

	/* Prevent cancel/die interrupt while doing this multi-step */
	HOLD_INTERRUPTS();

	if (PersistentDatabase_DirIsCreated(justInTimeDbDirNode))
	{
		RESUME_INTERRUPTS();
		LWLockRelease(TablespaceCreateLock);
		return;
	}

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);


	if (PersistentTablespace_GetState(justInTimeDbDirNode->tablespace) == PersistentFileSysState_CreatePending)
	{
		/*
		 * This tablespace was created in our explicit (i.e. BEGIN ... END)
		 * transaction.
		 *
		 * Otherwise, if would not be visible to us.
		 *
		 * Do a regular transaction database directory create.
		 */

		/*
		 * We write our intention or 'Create Pending' persistent information
		 * before we do any create database directory work on either the
		 * primary or mirror.
		 */
		smgrcreatedbdirpending(
							   justInTimeDbDirNode,
							   mirrorExistenceState,
							   &persistentTid,
							   &persistentSerialNum,
							    /* flushToXLog */ true);

		/*
		 * Synchronous primary and mirror create database directory.
		 */
		MirroredFileSysObj_CreateDbDir(
									   justInTimeDbDirNode,
									   mirrorMode,
									    /* ignoreAlreadyExists */ false,
									   &primaryError,
									   &mirrorDataLossOccurred);
		if (primaryError != 0)
		{
			RESUME_INTERRUPTS();
			LWLockRelease(MirroredLock);
			LWLockRelease(TablespaceCreateLock);
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not create database directory %u/%u: %s",
							justInTimeDbDirNode->tablespace,
							justInTimeDbDirNode->database,
							strerror(primaryError))));
		}
	}
	else
	{
		/*
		 * Do Just-In-Time non-transaction database directory create.
		 */
		smgrcreatedbdirjustintime(
								  justInTimeDbDirNode,
								  mirrorExistenceState,
								  mirrorMode,
								  &persistentTid,
								  &persistentSerialNum,
								  &primaryError,
								  &mirrorDataLossOccurred);
		if (primaryError != 0)
		{
			RESUME_INTERRUPTS();
			LWLockRelease(MirroredLock);
			LWLockRelease(TablespaceCreateLock);
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not create just-in-time database directory %u/%u: %s",
							justInTimeDbDirNode->tablespace,
							justInTimeDbDirNode->database,
							strerror(primaryError))));
		}
	}

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											&persistentTid,
											persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);


	RESUME_INTERRUPTS();

	LWLockRelease(MirroredLock);
	LWLockRelease(TablespaceCreateLock);
}

void
MirroredFileSysObj_TransactionCreateBufferPoolFile(RelFileNode *rnode,
												   PersistentFileSysRelBufpoolKind relBufpoolKind,
												   bool isLocalBuf,
												   char *relationName,
												   bool doJustInTimeDirCreate,
												   bool bufferPoolBulkLoad,
												   ItemPointer persistentTid,
												   int64 *persistentSerialNum)
{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;

	bool		mirrorDataLossOccurred;

	Assert(relationName != NULL);
	Assert(persistentTid != NULL);
	Assert(persistentSerialNum != NULL);

	if (doJustInTimeDirCreate)
	{
		DbDirNode	justInTimeDbDirNode;

		/*
		 * "Fault-in" the database directory in a tablespace if it doesn't
		 * exist yet.
		 */
		justInTimeDbDirNode.tablespace = rnode->spcNode;
		justInTimeDbDirNode.database = rnode->dbNode;
		MirroredFileSysObj_JustInTimeDbDirCreate(&justInTimeDbDirNode);
	}

	PersistentFileSysObjName_SetRelationFile(&fsObjName,
											 rnode,
											  /* segmentFileNum */ 0);

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);

	/*
	 * We write our intention or 'Create Pending' persistent information
	 * before we do any create relation work on either the primary or mirror.
	 */
	smgrcreatepending(rnode,
					   /* segmentFileNum */ 0,
					  PersistentFileSysRelStorageMgr_BufferPool,
					  relBufpoolKind,
					  mirrorExistenceState,
					  relDataSynchronizationState,
					  relationName,
					  persistentTid,
					  persistentSerialNum,
					  isLocalBuf,
					  bufferPoolBulkLoad,
					   /* flushToXLog */ true);

	/*
	 * Synchronous primary and mirror create relation.
	 */
	RelationCreateStorage(*rnode,
						  isLocalBuf,
						  relationName,
						  mirrorDataLossTrackingState,
						  mirrorDataLossTrackingSessionNum,
						  &mirrorDataLossOccurred);
	smgrclosenode(*rnode);

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											persistentTid,
											*persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);

	LWLockRelease(MirroredLock);

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_TransactionCreateBufferPoolFile: %u/%u/%u, relation name '%s', bulk load %s, mirror existence state '%s', mirror data loss occurred %s"
			 ", persistent serial number " INT64_FORMAT " at TID %s",
			 rnode->spcNode,
			 rnode->dbNode,
			 rnode->relNode,
			 (relationName == NULL ? "<null>" : relationName),
			 (bufferPoolBulkLoad ? "true" : "false"),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 (mirrorDataLossOccurred ? "true" : "false"),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));

		SUPPRESS_ERRCONTEXT_POP();
	}
}

void
MirroredFileSysObj_TransactionCreateAppendOnlyFile(
												   RelFileNode *relFileNode,

												   int32 segmentFileNum,

												   char *relationName,

												   bool doJustInTimeDirCreate,

												   ItemPointer persistentTid,
 /* Output: The TID of the gp_persistent_relation_node tuple. */

												   int64 *persistentSerialNum)
 /* Output: The serial number of the gp_persistent_relation_node tuple. */
{
	PersistentFileSysObjName fsObjName;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState = MirroredObjectExistenceState_None;
	MirroredRelDataSynchronizationState relDataSynchronizationState = MirroredRelDataSynchronizationState_None;
	StorageManagerMirrorMode mirrorMode = StorageManagerMirrorMode_None;

	int			primaryError;
	bool		mirrorDataLossOccurred;

	Assert(relationName != NULL);
	Assert(persistentTid != NULL);
	Assert(persistentSerialNum != NULL);

	if (doJustInTimeDirCreate)
	{
		DbDirNode	justInTimeDbDirNode;

		/*
		 * "Fault-in" the database directory in a tablespace if it doesn't
		 * exist yet.
		 */
		justInTimeDbDirNode.tablespace = relFileNode->spcNode;
		justInTimeDbDirNode.database = relFileNode->dbNode;
		MirroredFileSysObj_JustInTimeDbDirCreate(&justInTimeDbDirNode);
	}

	PersistentFileSysObjName_SetRelationFile(
											 &fsObjName,
											 relFileNode,
											 segmentFileNum);

	LWLockAcquire(MirroredLock, LW_SHARED);

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredFileSysObj_BeginMirroredCreate(
										   &fsObjName,
										   mirrorDataLossTrackingState,
										   &mirrorExistenceState,
										   &relDataSynchronizationState,
										   &mirrorMode);

	/*
	 * We write our intention or 'Create Pending' persistent information
	 * before we do any create relation work on either the primary or mirror.
	 */
	smgrcreatepending(
					  relFileNode,
					  segmentFileNum,
					  PersistentFileSysRelStorageMgr_AppendOnly,
					  PersistentFileSysRelBufpoolKind_None,
					  mirrorExistenceState,
					  relDataSynchronizationState,
					  relationName,
					  persistentTid,
					  persistentSerialNum,
					   /* isLocalBuf */ false,
					   /* bufferPoolBulkLoad */ false,
					   /* flushToXLog */ true);

	/*
	 * Synchronous primary and mirror create relation.
	 */
	MirroredAppendOnly_Create(
							  relFileNode,
							  segmentFileNum,
							  relationName,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							  &primaryError,
							  &mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		LWLockRelease(MirroredLock);
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create relation file '%s', relation name '%s': %s",
						relpath(*relFileNode, MAIN_FORKNUM),
						relationName,
						strerror(primaryError))));
	}

	MirroredFileSysObj_FinishMirroredCreate(
											&fsObjName,
											persistentTid,
											*persistentSerialNum,
											mirrorExistenceState,
											mirrorDataLossOccurred);

	LWLockRelease(MirroredLock);

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_TransactionCreateAppendOnlyFile: %u/%u/%u, relation name '%s', mirror existence state '%s', mirror data loss occurred %s"
			 ", persistent serial number " INT64_FORMAT " at TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 (relationName == NULL ? "<null>" : relationName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 (mirrorDataLossOccurred ? "true" : "false"),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));

		SUPPRESS_ERRCONTEXT_POP();
	}
}

void
MirroredFileSysObj_ScheduleDropBufferPoolRel(
											 Relation relation)
{
	if (!relation->rd_segfile0_relationnodeinfo.isPresent)
		RelationFetchSegFile0GpRelationNode(relation);
	Assert(relation->rd_segfile0_relationnodeinfo.isPresent);

	/*
	 * IMPORANT: ----> Relcache invalidation can close an open smgr <------
	 *
	 * So, DO NOT add other calls (such as RelationFetchSegFile0GpRelationNode
	 * above or even elog which may invoke heap_open to gather its errcontext
	 * between here and the smgrclose below.
	 *
	 * The DANGER is heap_open may process a relcache invalidation and close
	 * our relation's smgr open...
	 */
	RelationOpenSmgr(relation);
	Assert(relation->rd_smgr != NULL);
	RelationDropStorage(&relation->rd_node,
					    /* segmentFileNum */ 0,
					   PersistentFileSysRelStorageMgr_BufferPool,
					   relation->rd_isLocalBuf,
					   relation->rd_rel->relname.data,
					   &relation->rd_segfile0_relationnodeinfo.persistentTid,
					   relation->rd_segfile0_relationnodeinfo.persistentSerialNum);

	/* Now close the file and throw away the hashtable entry */
	smgrclose(relation->rd_smgr);
	Assert(relation->rd_smgr == NULL);
	/* Since RelationOpenSmgr linked the two together. */

	/*
	 * IMPORANT: ----> Relcache invalidation can close an open smgr <------
	 *
	 * See above.
	 */


	/*
	 * Only do this elog tracing outside the smgr usage above.
	 */
	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_ScheduleDropBufferPoolRel: %u/%u/%u, relation name '%s'",
			 relation->rd_node.spcNode,
			 relation->rd_node.dbNode,
			 relation->rd_node.relNode,
			 relation->rd_rel->relname.data);

		SUPPRESS_ERRCONTEXT_POP();
	}

}

void
MirroredFileSysObj_ScheduleDropBufferPoolFile(
											  RelFileNode *relFileNode,

											  bool isLocalBuf,

											  char *relationName,

											  ItemPointer persistentTid,

											  int64 persistentSerialNum)
{
	Assert(persistentTid != NULL);

	RelationDropStorage(relFileNode,
					    /* segmentFileNum */ 0,
						PersistentFileSysRelStorageMgr_BufferPool,
						isLocalBuf,
						relationName,
						persistentTid,
						persistentSerialNum);

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_ScheduleDropBufferPoolFile: %u/%u/%u, relation name '%s'",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 relationName);

		SUPPRESS_ERRCONTEXT_POP();
	}

}

void
MirroredFileSysObj_ScheduleDropAppendOnlyFile(
											  RelFileNode *relFileNode,

											  int32 segmentFileNum,

											  char *relationName,

											  ItemPointer persistentTid,

											  int64 persistentSerialNum)
{
	Assert(persistentTid != NULL);

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredFileSysObj_ScheduleDropAppendOnlyFile: %u/%u/%u, relation name '%s'",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 relationName);

		SUPPRESS_ERRCONTEXT_POP();
	}

	RelationDropStorage(relFileNode,
						segmentFileNum,
						PersistentFileSysRelStorageMgr_AppendOnly,
					    /* isLocalBuf */ false,
						relationName,
						persistentTid,
						persistentSerialNum);
}

void
MirroredFileSysObj_DropRelFile(
							   RelFileNode *relFileNode,

							   int32 segmentFileNum,

							   PersistentFileSysRelStorageMgr relStorageMgr,

							   char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

							   bool isLocalBuf,

							   bool primaryOnly,

							   bool ignoreNonExistence,

							   bool *mirrorDataLossOccurred)
{
	int			primaryError;

	switch (relStorageMgr)
	{
		case PersistentFileSysRelStorageMgr_BufferPool:
			smgrdomirroredunlink(relFileNode,
								 isLocalBuf,
								 relationName,
								 primaryOnly,
								 /* isRedo */ false,
								 ignoreNonExistence,
								 mirrorDataLossOccurred);
			break;

		case PersistentFileSysRelStorageMgr_AppendOnly:
			MirroredAppendOnly_Drop(
									relFileNode,
									segmentFileNum,
									relationName,
									primaryOnly,
									&primaryError,
									mirrorDataLossOccurred);
			if (ignoreNonExistence && primaryError == ENOENT)
				primaryError = 0;

			/* UNDONE: This needs to be an error. */
			if (primaryError != 0)
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove relation %u/%u/%u (segment file #%d): %s",
								relFileNode->spcNode,
								relFileNode->dbNode,
								relFileNode->relNode,
								segmentFileNum,
								strerror(primaryError))));
			break;

		default:
			elog(ERROR, "unexpected relation storage manager %d", relStorageMgr);
	}
}
