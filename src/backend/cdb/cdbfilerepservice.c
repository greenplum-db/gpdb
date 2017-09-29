/*-------------------------------------------------------------------------
 *
 * cdbfilerepservice.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc. All rights reserved.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepservice.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "miscadmin.h"
#include "catalog/catalog.h"
#include "catalog/pg_database.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerepprimaryrecovery.h"
#include "cdb/cdbfilerepmirror.h"
#include "cdb/cdbfilerepmirrorack.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbfilerepresyncworker.h"
#include "cdb/cdbvars.h"
#include "libpq/pqsignal.h"
#include "postmaster/postmaster.h"
#include "storage/backendid.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/sinvaladt.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/resowner.h"
#include "utils/syscache.h"

/*
 * Initialize external variables.
 */
FileRepProcessType_e fileRepProcessType = FileRepProcessTypeNotInitialized;

static FileRepState_e fileRepState = FileRepStateNotInitialized;

/* state of FileRep process */


/*
 * Parameters set by signal handlers for later service n the main loop
 */
static volatile sig_atomic_t reloadConfigFile = false;

static volatile sig_atomic_t shutdownRequested = false;

/* graceful shutdown informed by SIGUSR2 signal from postmaster */

/* state change informed by SIGUSR1 signal from postmaster...when state change request comes in
 *  this counter is incremented.
 */
static volatile sig_atomic_t stateChangeRequestCounter = 0;

/**
 * This value increases when we process state change requests.  There is no pending state change request if
 *   lastChangeRequestProcessCounterValue == stateChangeRequestCounter
 *
 * Note that this value is not actually updated in the signal handlers, but must match or exceed the size of
 *    stateChangeRequestCounter so we use its type
 */
static volatile sig_atomic_t lastChangeRequestProcessCounterValue = 0;

/* it is set to TRUE in order to read configuration information at start */

static void FileRepSubProcess_SigHupHandler(SIGNAL_ARGS);
static void FileRepSubProcess_ImmediateShutdownHandler(SIGNAL_ARGS);
static void FileRepSubProcess_ShutdownHandler(SIGNAL_ARGS);
static void FileRepSubProcess_FileRepStateHandler(SIGNAL_ARGS);
static void FileRepSubProcess_HandleCrash(SIGNAL_ARGS);

static void FileRepSubProcess_ConfigureSignals(void);

/*
 *  SIGHUP signal from main file rep process
 *  It re-loads configuration file at next convenient time.
 */
static void
FileRepSubProcess_SigHupHandler(SIGNAL_ARGS)
{
	reloadConfigFile = true;
}

/*
 *  SIGQUIT signal from main file rep process
 */
static void
FileRepSubProcess_ImmediateShutdownHandler(SIGNAL_ARGS)
{
	quickdie(PASS_SIGNAL_ARGS);
}

/*
 *  SIGUSR2 signal from main file rep process
 */
static void
FileRepSubProcess_ShutdownHandler(SIGNAL_ARGS)
{
	bool		isInTransition = FALSE;
	DataState_e dataStateTransition;

	shutdownRequested = true;

	/*
	 * Exit the process if recv() call is hanging or compacting is running.
	 * Compacting can take many minutes.
	 */
	if (fileRepProcessType == FileRepProcessTypePrimaryReceiverAck ||
		fileRepProcessType == FileRepProcessTypeMirrorReceiver ||
		fileRepProcessType == FileRepProcessTypePrimaryRecovery)
	{
		/* workaround for gcov testing */
		if (Debug_filerep_gcov)
		{
			getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, &isInTransition, &dataStateTransition);

			if (isInTransition == TRUE &&
				dataStateTransition == DataStateInChangeTracking)
			{
				proc_exit(0);
				return;
			}
		}

		die(PASS_SIGNAL_ARGS);
	}

	if (FileRepIsBackendSubProcess(fileRepProcessType))
	{
		if (FileRepPrimary_IsResyncManagerOrWorker())
		{
			getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, &isInTransition, &dataStateTransition);

			if (isInTransition == TRUE &&
				dataStateTransition == DataStateInChangeTracking)
			{
				/*
				 * Resync workers and manager may be waiting on lock that is
				 * acquired by backend process that is suspended during
				 * transition to Change Tracking and so FileRep backend
				 * shutdown may never be completed.
				 */
				if (fileRepProcessType == FileRepProcessTypeResyncManager)
				{
					FileRepResync_Cleanup();
				}
				else
				{
					LockReleaseAll(DEFAULT_LOCKMETHOD, false);
				}

				/*
				 * We remove ourself from LW waiter list (if applicable).
				 *
				 * If the current backend is waiting on a LWLock and exits w/o
				 * any cleanup (remove from waiters list) it can cause a
				 * breakage in the LWlock's waiters linked list after it dies.
				 * This can lead to unpleasant issues causing starvation for
				 * subsequent waiters because the current backend is already
				 * dead without assigning the LWLock to the next waiter.
				 *
				 * XXX Side note - Although implemented here, avoid exiting
				 * inside an signal handler.
				 */
				LWLockWaitCancel();
				LWLockReleaseAll();

				proc_exit(0);
				return;
			}
		}

		/*
		 * call the normal postgres die so that it requests query
		 * cancel/procdie
		 */
		die(PASS_SIGNAL_ARGS);
	}
}

/*
 *  SIGUSR1 signal from main file rep process
 *  It signals about data and/or segment state change.
 */
static void
FileRepSubProcess_FileRepStateHandler(SIGNAL_ARGS)
{
	++stateChangeRequestCounter;
}

/*
 *  FileRepSubProcess_ProcessSignals()
 *
 */
bool
FileRepSubProcess_ProcessSignals()
{
	bool		processExit = false;

	if (reloadConfigFile)
	{
		reloadConfigFile = false;
		ProcessConfigFile(PGC_SIGHUP);

		FileRep_SetFileRepRetry();
	}

	if (shutdownRequested)
	{
		SegmentState_e segmentState;

		getPrimaryMirrorStatusCodes(NULL, &segmentState, NULL, NULL);

		shutdownRequested = false;

		if (segmentState == SegmentStateShutdownFilerepBackends)
		{
			processExit = FileRepIsBackendSubProcess(fileRepProcessType);
			FileRepSubProcess_SetState(FileRepStateShutdownBackends);
		}
		else
		{
			processExit = true;
			FileRepSubProcess_SetState(FileRepStateShutdown);
		}
	}

	/*
	 * Immediate shutdown if postmaster or main filerep process (parent) is
	 * not alive to avoid manual cleanup.
	 */
	if (!PostmasterIsAlive(false /* amDirectChild */ ) || !ParentProcIsAlive())
	{
		quickdie_impl();
	}

	for (;;)
	{
		/* check to see if change required */
		sig_atomic_t curStateChangeRequestCounter = stateChangeRequestCounter;

		if (curStateChangeRequestCounter == lastChangeRequestProcessCounterValue)
			break;
		lastChangeRequestProcessCounterValue = curStateChangeRequestCounter;

		/* do the change in local memory */
		getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
		switch (segmentState)
		{

			case SegmentStateNotInitialized:
				FileRepSubProcess_SetState(FileRepStateNotInitialized);
				break;

			case SegmentStateInitialization:
				FileRepSubProcess_SetState(FileRepStateInitialization);
				break;

			case SegmentStateInResyncTransition:
				FileRepSubProcess_SetState(FileRepStateInitialization);
				break;

			case SegmentStateInChangeTrackingTransition:
			case SegmentStateInSyncTransition:
				/* fileRepState remains Ready */
				break;

			case SegmentStateChangeTrackingDisabled:
			case SegmentStateReady:
				FileRepSubProcess_SetState(FileRepStateReady);
				break;

			case SegmentStateFault:
				FileRepSubProcess_SetState(FileRepStateFault);
				break;

			case SegmentStateShutdownFilerepBackends:
				if (fileRepRole == FileRepPrimaryRole)
				{
					FileRepSubProcess_SetState(FileRepStateShutdownBackends);
				}
				else
				{
					processExit = true;
					FileRepSubProcess_SetState(FileRepStateShutdown);
				}
				break;

			case SegmentStateImmediateShutdown:
			case SegmentStateShutdown:
				processExit = true;
				FileRepSubProcess_SetState(FileRepStateShutdown);
				break;

			default:
				Assert(0);
				break;
		} //switch ()

				if (processExit == true)
				{
					FileRep_IpcSignalAll();
				}
	}

	return (processExit);
}

bool
FileRepSubProcess_IsStateTransitionRequested(void)
{

	bool		isStateTransitionRequested = FALSE;

	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);

	switch (fileRepProcessType)
	{
		case FileRepProcessTypeMain:
			/* Handle Shutdown request */
			if (segmentState == SegmentStateImmediateShutdown)
			{
				isStateTransitionRequested = TRUE;
			}

			break;

		case FileRepProcessTypeNotInitialized:

			if (segmentState == SegmentStateShutdownFilerepBackends &&
				fileRepShmemArray[0]->state == FileRepStateFault)
			{
				FileRep_InsertConfigLogEntry("failure is detected in segment mirroring during backend shutdown, abort requested");
			}
			/* no break */
		default:

			if (fileRepProcessType != FileRepProcessTypeNotInitialized)
			{
				FileRepSubProcess_ProcessSignals();
			}

			if (dataState == DataStateInChangeTracking)
			{
				isStateTransitionRequested = TRUE;
			}

			switch (segmentState)
			{
				case SegmentStateFault:
				case SegmentStateImmediateShutdown:
				case SegmentStateShutdown:

					isStateTransitionRequested = TRUE;
					break;

				default:
					break;
			}

			break;
	}

	if (isStateTransitionRequested)
	{
		FileRep_InsertConfigLogEntry("state transition requested ");
	}
	return isStateTransitionRequested;
}

/*
 *  FileRepSubProcess_GetState()
 *  Return state of FileRep sub-process
 */
FileRepState_e
FileRepSubProcess_GetState(void)
{
	Assert(fileRepState != FileRepStateShutdownBackends);
	return fileRepState;
}

/*
 *  Set state in FileRep process and sent signal to postmaster
 */
void
FileRepSubProcess_SetState(FileRepState_e fileRepStateLocal)
{
	bool		doAssignment = true;

	if (fileRepStateLocal == FileRepStateShutdownBackends)
	{
		if (FileRepIsBackendSubProcess(fileRepProcessType))
		{
			/* the current process must shutdown! */
			fileRepStateLocal = FileRepStateShutdown;
		}
		else
		{
			/*
			 * the current process doesn't care about shutdown backends --
			 * leave it as shutdown
			 */
			doAssignment = false;
		}
	}

	if (!doAssignment)
	{
		return;
	}

	switch (fileRepState)
	{
		case FileRepStateNotInitialized:

			fileRepState = fileRepStateLocal;
			break;

		case FileRepStateInitialization:

			switch (fileRepStateLocal)
			{
				case FileRepStateNotInitialized:
					ereport(WARNING,
							(errmsg("mirror failure, "
									"unexpected filerep state transition from '%s' to '%s' "
									"failover requested",
									FileRepStateToString[fileRepState],
									FileRepStateToString[fileRepStateLocal]),
							 errhint("run gprecoverseg to re-establish mirror connectivity")));

					fileRepState = FileRepStateFault;
					break;

				default:
					fileRepState = fileRepStateLocal;
					break;
			}
			break;

		case FileRepStateReady:

			switch (fileRepStateLocal)
			{
				case FileRepStateFault:
				case FileRepStateShutdown:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
					ereport(WARNING,
							(errmsg("mirror failure, "
									"unexpected filerep state transition from '%s' to '%s' "
									"failover requested",
									FileRepStateToString[fileRepState],
									FileRepStateToString[fileRepStateLocal]),
							 errhint("run gprecoverseg to re-establish mirror connectivity")));

					fileRepState = FileRepStateFault;
					break;
				case FileRepStateInitialization:

					/*
					 * don't do assignment -- this can happen when going from
					 * segmentState Ready to InSyncTransition
					 */
					doAssignment = false;
					break;
				case FileRepStateReady:
					break;
				default:
					Assert(0);
					break;
			}
			break;
		case FileRepStateFault:

			switch (fileRepStateLocal)
			{
				case FileRepStateFault:
				case FileRepStateShutdown:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
				case FileRepStateInitialization:
				case FileRepStateReady:
					ereport(WARNING,
							(errmsg("mirror failure, "
									"unexpected filerep state transition from '%s' to '%s' "
									"failover requested",
									FileRepStateToString[fileRepState],
									FileRepStateToString[fileRepStateLocal]),
							 errhint("run gprecoverseg to re-establish mirror connectivity")));

					fileRepState = FileRepStateFault;

					break;
				default:
					Assert(0);
					break;
			}

			break;
		case FileRepStateShutdownBackends:
			Assert(!"process filerep state should never be in ShutdownBackends");
			break;
		case FileRepStateShutdown:

			switch (fileRepStateLocal)
			{
				case FileRepStateShutdown:
					fileRepState = fileRepStateLocal;
					break;
				case FileRepStateNotInitialized:
				case FileRepStateInitialization:
				case FileRepStateReady:
					ereport(WARNING,
							(errmsg("mirror failure, "
									"unexpected filerep state transition from '%s' to '%s' "
									"failover requested",
									FileRepStateToString[fileRepState],
									FileRepStateToString[fileRepStateLocal]),
							 errhint("run gprecoverseg to re-establish mirror connectivity")));
					fileRepState = FileRepStateFault;

				case FileRepStateFault:
					break;
				default:
					Assert(0);
					break;
			}

			break;
		default:
			Assert(0);
			break;
	}

	/* check doAssignment again -- may have changed value in the switch above */
	if (!doAssignment)
	{
		return;
	}

	/* now update in shared memory if needed */
	switch (fileRepState)
	{
		case FileRepStateReady:
			if (segmentState != SegmentStateChangeTrackingDisabled)
			{
				FileRep_SetSegmentState(SegmentStateReady, FaultTypeNotInitialized);
			}
			break;

		case FileRepStateFault:

			/*
			 * update shared memory configuration bool
			 * updateSegmentState(FAULT); return TRUE if state was updated;
			 * return FALSE if state was already set to FAULT change signal to
			 * PMSIGNAL_FILEREP_SEGMENT_STATE_CHANGE
			 */
			FileRep_SetSegmentState(SegmentStateFault, FaultTypeMirror);
			break;

		case FileRepStateInitialization:
		case FileRepStateShutdown:
		case FileRepStateNotInitialized:
			/* No operation */
			break;
		case FileRepStateShutdownBackends:
			Assert(0);
			break;
		default:
			Assert(0);
			break;
	}

	/* report the change */
	if (fileRepState != FileRepStateShutdown)
	{
		FileRep_InsertConfigLogEntry("set filerep state");
	}

}

static void
FileRepSubProcess_InitProcess(void)
{
	SetProcessingMode(InitProcessing);

	/*
	 * Create a resource owner to keep track of our resources
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL,
											   FileRepProcessTypeToString[fileRepProcessType]);


	InitXLOGAccess();

	SetProcessingMode(NormalProcessing);

	InitBufferPoolAccess();

	/*
	 * Don't add Filerep backend subprocesses to the proc array.
	 *
	 * This avoids any deadlock situations during Filerep transition. E.g. If
	 * a normal backend has acquired ProcArrayLock and is waiting for Filerep
	 * transition to finish, the Filerep backend subprocesses will deadlock
	 * forever as they can't acquire the ProcArray lock to remove themselves
	 * from the ProcArray. This directly causes the transition to stall and
	 * thus the whole system.
	 */

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	SharedInvalBackendInit(false);

	if (MyBackendId > MaxBackends || MyBackendId <= 0)
		elog(FATAL, "bad backend id: %d", MyBackendId);

	/*
	 * bufmgr needs another initialization call too
	 */
	InitBufferPoolBackend();
}

void
FileRepSubProcess_InitHeapAccess(void)
{
	char	   *fullpath;
	static bool heapAccessInitialized = false;

	if (heapAccessInitialized)
		return;


	/* heap access requires the rel-cache */
	RelationCacheInitialize();
	InitCatalogCache();

	/*
	 * It's now possible to do real access to the system catalogs.
	 *
	 * Load relcache entries for the system catalogs.  This must create at
	 * least the minimum set of "nailed-in" cache entries.
	 */
	RelationCacheInitializePhase2();

	/*
	 * In order to access the catalog, we need a database, and a tablespace;
	 * our access to the heap is going to be slightly limited, so we'll just
	 * use some defaults.
	 */
	MyDatabaseId = TemplateDbOid;
	MyDatabaseTableSpace = DEFAULTTABLESPACE_OID;

	/* Now we can mark our PGPROC entry with the database ID */
	/* (We assume this is an atomic store so no lock is needed) */
	MyProc->databaseId = MyDatabaseId;

	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

	SetDatabasePath(fullpath);

	RelationCacheInitializePhase3();

	heapAccessInitialized = true;
}

static void
FileRepSubProcess_HandleCrash(SIGNAL_ARGS)
{
	StandardHandlerForSigillSigsegvSigbus_OnMainThread("a file replication subprocess", PASS_SIGNAL_ARGS);
}

/*
 *
 */
static void
FileRepSubProcess_ConfigureSignals(void)
{
	/* Accept Signals */
	/* emergency shutdown */
	pqsignal(SIGQUIT, FileRepSubProcess_ImmediateShutdownHandler);

	/* graceful shutdown */
	pqsignal(SIGUSR2, FileRepSubProcess_ShutdownHandler);

	/* reload configuration file */
	pqsignal(SIGHUP, FileRepSubProcess_SigHupHandler);

	/* data or segment state changed */
	pqsignal(SIGUSR1, FileRepSubProcess_FileRepStateHandler);

	/* Ignore Signals */
	pqsignal(SIGTERM, SIG_IGN);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);

	/* Use default action */
	pqsignal(SIGCHLD, SIG_DFL);
	pqsignal(SIGINT, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);

#ifdef SIGSEGV
	pqsignal(SIGSEGV, FileRepSubProcess_HandleCrash);
#endif

#ifdef SIGILL
	pqsignal(SIGILL, FileRepSubProcess_HandleCrash);
#endif

#ifdef SIGBUS
	pqsignal(SIGBUS, FileRepSubProcess_HandleCrash);
#endif

}

/*
 *
 */
void
FileRepSubProcess_Main()
{
	const char *statmsg;

	MemoryContext fileRepSubProcessMemoryContext;

	sigjmp_buf	local_sigjmp_buf;

	MyProcPid = getpid();

	MyStartTime = time(NULL);

	/*
	 * Create a PGPROC so we can use LWLocks in FileRep sub-processes.  The
	 * routine also register clean up at process exit
	 */
	InitAuxiliaryProcess();

	InitBufferPoolBackend();

	FileRepSubProcess_ConfigureSignals();

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		LWLockReleaseAll();

		if (FileRepPrimary_IsResyncManagerOrWorker())
		{
			LockReleaseAll(DEFAULT_LOCKMETHOD, false);
		}

		if (FileRepIsBackendSubProcess(fileRepProcessType))
		{
			AbortBufferIO();
			UnlockBuffers();

			/* buffer pins are released here: */
			ResourceOwnerRelease(CurrentResourceOwner,
								 RESOURCE_RELEASE_BEFORE_LOCKS,
								 false, true);
		}

		/*
		 * We can now go away.	Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	/*
	 * Identify myself via ps
	 */

	statmsg = FileRepProcessTypeToString[fileRepProcessType];

	init_ps_display(statmsg, "", "", "");

	/* Create the memory context where cross-transaction state is stored */
	fileRepSubProcessMemoryContext = AllocSetContextCreate(TopMemoryContext,
														   "filerep subprocess memory context",
														   ALLOCSET_DEFAULT_MINSIZE,
														   ALLOCSET_DEFAULT_INITSIZE,
														   ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(fileRepSubProcessMemoryContext);

	stateChangeRequestCounter++;

	FileRepSubProcess_ProcessSignals();

	switch (fileRepProcessType)
	{
		case FileRepProcessTypePrimarySender:
			FileRepPrimary_StartSender();
			break;

		case FileRepProcessTypeMirrorReceiver:
			FileRepMirror_StartReceiver();
			break;

		case FileRepProcessTypeMirrorConsumer:
		case FileRepProcessTypeMirrorConsumerWriter:
		case FileRepProcessTypeMirrorConsumerAppendOnly1:
			FileRepMirror_StartConsumer();
			break;

		case FileRepProcessTypeMirrorSenderAck:
			FileRepAckMirror_StartSender();
			break;

		case FileRepProcessTypePrimaryReceiverAck:
			FileRepAckPrimary_StartReceiver();
			break;

		case FileRepProcessTypePrimaryConsumerAck:
			FileRepAckPrimary_StartConsumer();
			break;

		case FileRepProcessTypePrimaryRecovery:
			FileRepSubProcess_InitProcess();

			/*
			 * At this point, database is starting up and xlog is not yet
			 * replayed.  Initializing relcache now is dangerous, a sequential
			 * scan of catalog tables may end up with incorrect hint bits.
			 * E.g. a committed transaction's dirty heap pages made it to disk
			 * but pg_clog update was still in memory and we crashed.  If a
			 * tuple inserted by this transaction is read during relcache
			 * initialization, status of the tuple's xmin will be incorrectly
			 * determined as "not commited" from pg_clog. And
			 * HEAP_XMIN_INVALID hint bit will be set, rendering the tuple
			 * perpetually invisible.  Relcache initialization must be
			 * deferred to only after all of xlog has been replayed.
			 */
			FileRepPrimary_StartRecovery();

			ResourceOwnerRelease(CurrentResourceOwner,
								 RESOURCE_RELEASE_BEFORE_LOCKS,
								 false, true);
			break;

		case FileRepProcessTypeResyncManager:
			FileRepSubProcess_InitProcess();
			FileRepPrimary_StartResyncManager();

			ResourceOwnerRelease(CurrentResourceOwner,
								 RESOURCE_RELEASE_BEFORE_LOCKS,
								 false, true);
			break;

		case FileRepProcessTypeResyncWorker1:
		case FileRepProcessTypeResyncWorker2:
		case FileRepProcessTypeResyncWorker3:
		case FileRepProcessTypeResyncWorker4:
			FileRepSubProcess_InitProcess();
			FileRepPrimary_StartResyncWorker();

			ResourceOwnerRelease(CurrentResourceOwner,
								 RESOURCE_RELEASE_BEFORE_LOCKS,
								 false, true);
			break;

		default:
			elog(PANIC, "unrecognized process type: %s(%d)",
				 statmsg, fileRepProcessType);
			break;
	}

	switch (FileRepSubProcess_GetState())
	{
		case FileRepStateShutdown:
		case FileRepStateReady:
			proc_exit(0);
			break;

		default:
			proc_exit(2);
			break;
	}
}
