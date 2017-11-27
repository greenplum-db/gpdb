/*-------------------------------------------------------------------------
 *
 * ipci.c
 *	  POSTGRES inter-process communication initialization code.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/ipc/ipci.c,v 1.99 2009/01/03 17:08:39 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include "access/clog.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/subtrans.h"
#include "access/twophase.h"
#include "access/distributedlog.h"
#include "access/appendonlywriter.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbfilerepprimaryrecovery.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdblocaldistribxact.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentrelation.h"
#include "cdb/cdbpersistentcheck.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgwriter.h"
#include "postmaster/postmaster.h"
#include "postmaster/primary_mirror_mode.h"
#include "postmaster/seqserver.h"
#include "replication/walsender.h"
#include "replication/walreceiver.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "utils/backend_cancel.h"
#include "utils/resource_manager.h"
#include "utils/faultinjector.h"
#include "utils/sharedsnapshot.h"
#include "utils/simex.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbtm.h"
#include "utils/tqual.h"
#include "postmaster/backoff.h"
#include "cdb/memquota.h"
#include "executor/spi.h"
#include "utils/workfile_mgr.h"
#include "utils/session_state.h"

shmem_startup_hook_type shmem_startup_hook = NULL;

static Size total_addin_request = 0;
static bool addin_request_allowed = true;


/*
 * RequestAddinShmemSpace
 *		Request that extra shmem space be allocated for use by
 *		a loadable module.
 *
 * This is only useful if called from the _PG_init hook of a library that
 * is loaded into the postmaster via shared_preload_libraries.	Once
 * shared memory has been allocated, calls will be ignored.  (We could
 * raise an error, but it seems better to make it a no-op, so that
 * libraries containing such calls can be reloaded if needed.)
 */
void
RequestAddinShmemSpace(Size size)
{
	if (IsUnderPostmaster || !addin_request_allowed)
		return;					/* too late */
	total_addin_request = add_size(total_addin_request, size);
}


/*
 * CreateSharedMemoryAndSemaphores
 *		Creates and initializes shared memory and semaphores.
 *
 * This is called by the postmaster or by a standalone backend.
 * It is also called by a backend forked from the postmaster in the
 * EXEC_BACKEND case.  In the latter case, the shared memory segment
 * already exists and has been physically attached to, but we have to
 * initialize pointers in local memory that reference the shared structures,
 * because we didn't inherit the correct pointer values from the postmaster
 * as we do in the fork() scenario.  The easiest way to do that is to run
 * through the same code as before.  (Note that the called routines mostly
 * check IsUnderPostmaster, rather than EXEC_BACKEND, to detect this case.
 * This is a bit code-wasteful and could be cleaned up.)
 *
 * If "makePrivate" is true then we only need private memory, not shared
 * memory.	This is true for a standalone backend, false for a postmaster.
 */
void
CreateSharedMemoryAndSemaphores(bool makePrivate, int port)
{
	if (!IsUnderPostmaster)
	{
		PGShmemHeader *seghdr;
		Size		size;
		int			numSemas;

		/*
		 * Size of the Postgres shared-memory block is estimated via
		 * moderately-accurate estimates for the big hogs, plus 100K for the
		 * stuff that's too small to bother with estimating.
		 *
		 * We take some care during this phase to ensure that the total size
		 * request doesn't overflow size_t.  If this gets through, we don't
		 * need to be so careful during the actual allocation phase.
		 */
		size = 150000;
		size = add_size(size, hash_estimate_size(SHMEM_INDEX_SIZE,
												 sizeof(ShmemIndexEnt)));
		size = add_size(size, BufferShmemSize());
		size = add_size(size, LockShmemSize());
		size = add_size(size, workfile_mgr_shmem_size());
		if (Gp_role == GP_ROLE_DISPATCH)
			size = add_size(size, AppendOnlyWriterShmemSize());

		if (IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH)
		{
			size = add_size(size, ResSchedulerShmemSize());
			size = add_size(size, ResPortalIncrementShmemSize());
		}
		else if (IsResGroupEnabled())
			size = add_size(size, ResGroupShmemSize());

		size = add_size(size, ProcGlobalShmemSize());
		size = add_size(size, XLOGShmemSize());
		size = add_size(size, DistributedLog_ShmemSize());
		size = add_size(size, CLOGShmemSize());
		size = add_size(size, ChangeTrackingShmemSize());
		size = add_size(size, SUBTRANSShmemSize());
		size = add_size(size, TwoPhaseShmemSize());
		size = add_size(size, MultiXactShmemSize());
		size = add_size(size, LWLockShmemSize());
		size = add_size(size, ProcArrayShmemSize());
		size = add_size(size, BackendStatusShmemSize());
		size = add_size(size, SharedSnapshotShmemSize());

		size = add_size(size, SInvalShmemSize());
		size = add_size(size, PMSignalShmemSize());
		size = add_size(size, ProcSignalShmemSize());
		size = add_size(size, primaryMirrorModeShmemSize());
		//size = add_size(size, AutoVacuumShmemSize());
		size = add_size(size, FtsShmemSize());
		size = add_size(size, tmShmemSize());
		size = add_size(size, SeqServerShmemSize());
		size = add_size(size, PersistentFileSysObj_ShmemSize());
		size = add_size(size, PersistentFilespace_ShmemSize());
		size = add_size(size, PersistentTablespace_ShmemSize());
		size = add_size(size, PersistentDatabase_ShmemSize());
		size = add_size(size, PersistentRelation_ShmemSize());

		/*Add shared memory for PT verification checks*/
		if (Gp_role == GP_ROLE_DISPATCH && debug_persistent_ptcat_verification)
		{
			size = add_size(size, Persistent_PostDTMRecv_ShmemSize());
		}

		if (GPAreFileReplicationStructuresRequired()) {
			size = add_size(size, FileRep_SubProcShmemSize());
			size = add_size(size, FileRep_ShmemSize());
			size = add_size(size, FileRepAck_ShmemSize());
			size = add_size(size, FileRepAckPrimary_ShmemSize());
			size = add_size(size, FileRepResync_ShmemSize()); 
			size = add_size(size, FileRepIpc_ShmemSize());
			size = add_size(size, FileRepLog_ShmemSize());
		}
		
#ifdef FAULT_INJECTOR
		size = add_size(size, FaultInjector_ShmemSize());
#endif			
		
#ifdef EXEC_BACKEND
		size = add_size(size, ShmemBackendArraySize());
#endif

#ifdef USE_TEST_UTILS
		if (gp_simex_init)
		{
			// initialize SimEx
			simex_init();
			size = add_size(size, SyncBitVector_ShmemSize(simex_get_subclass_count()));
		}
#endif

		/* This elog happens before we know the name of the log file we are supposed to use */
		elog(DEBUG1, "Size not including the buffer pool %lu",
			 (unsigned long) size);

		size = add_size(size, AutoVacuumShmemSize());
		size = add_size(size, BTreeShmemSize());
		size = add_size(size, SyncScanShmemSize());
		size = add_size(size, CheckpointerShmemSize());
		size = add_size(size, CancelBackendMsgShmemSize());

		size = add_size(size, WalSndShmemSize());
		size = add_size(size, WalRcvShmemSize());

		/* freeze the addin request size and include it */
		addin_request_allowed = false;
		size = add_size(size, total_addin_request);

		/* might as well round it off to a multiple of a typical page size */
		size = add_size(size, BLCKSZ - (size % BLCKSZ));

		/* Consider the size of the SessionState array */
		size = add_size(size, SessionState_ShmemSize());

		/*
		 * Create the shmem segment
		 */
		seghdr = PGSharedMemoryCreate(size, makePrivate, port);

		InitShmemAccess(seghdr);

		/*
		 * Create semaphores
		 */
		numSemas = ProcGlobalSemas();
		numSemas += SpinlockSemas();

		if (GPAreFileReplicationStructuresRequired()) 
		{
			numSemas += FileRepSemas();
		}
		
		elog(DEBUG3,"reserving %d semaphores",numSemas);
		PGReserveSemaphores(numSemas, port);
		
	}
	else
	{
		/*
		 * We are reattaching to an existing shared memory segment. This
		 * should only be reached in the EXEC_BACKEND case, and even then only
		 * with makePrivate == false.
		 */
#ifdef EXEC_BACKEND
		Assert(!makePrivate);
#else
		elog(PANIC, "should be attached to shared memory already");
#endif
	}

	/*
	 * Set up shared memory allocation mechanism
	 */
	if (!IsUnderPostmaster)
		InitShmemAllocation();

	/*
	 * Now initialize LWLocks, which do shared memory allocation and are
	 * needed for InitShmemIndex.
	 */
	if (!IsUnderPostmaster)
		CreateLWLocks();

	/*
	 * Set up shmem.c index hashtable
	 */
	InitShmemIndex();

	primaryMirrorModeShmemInit();

	/*
	 * Set up xlog, clog, and buffers
	 */
	XLOGShmemInit();
	CLOGShmemInit();
	ChangeTrackingShmemInit();
	DistributedLog_ShmemInit();
	SUBTRANSShmemInit();
	TwoPhaseShmemInit();
	MultiXactShmemInit();
    FtsShmemInit();
    tmShmemInit();
	InitBufferPool();

	/*
	 * Set up lock manager
	 */
	InitLocks();

	/*
	 * Set up append only writer
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
		InitAppendOnlyWriter();

	PersistentFileSysObj_ShmemInit();
	PersistentFilespace_ShmemInit();
	PersistentTablespace_ShmemInit();
	PersistentDatabase_ShmemInit();
	PersistentRelation_ShmemInit();

	if (Gp_role == GP_ROLE_DISPATCH && debug_persistent_ptcat_verification)
		Persistent_PostDTMRecv_ShmemInit();

	/*
	 * Set up resource manager 
	 */
	ResManagerShmemInit();

	if (!IsUnderPostmaster)
	{
		/* Set up process table */
		InitProcGlobal(PostmasterGetMppLocalProcessCounter());
	}

	/* Initialize SessionState shared memory array */
	SessionState_ShmemInit();
	/* Initialize vmem protection */
	GPMemoryProtect_ShmemInit();

	CreateSharedProcArray();
	CreateSharedBackendStatus();
	
	/*
	 * Set up Shared snapshot slots
	 *
	 * TODO: only need to do this if we aren't the QD. for now we are just 
	 *		 doing it all the time and wasting shemem on the QD.  This is 
	 *		 because this happens at postmaster startup time when we don't
	 *		 know who we are.  
	 */
	CreateSharedSnapshotArray();

	/*
	 * Set up shared-inval messaging
	 */
	CreateSharedInvalidationState();

	/*
	 * Set up interprocess signaling mechanisms
	 */
	PMSignalShmemInit();
	ProcSignalShmemInit();
	CheckpointerShmemInit();
	WalSndShmemInit();
	WalRcvShmemInit();
	//AutoVacuumShmemInit();
	SeqServerShmemInit();

	if (GPAreFileReplicationStructuresRequired()) {
	
		FileRep_SubProcShmemInit();
		FileRep_ShmemInit();
		FileRepAck_ShmemInit();
		FileRepAckPrimary_ShmemInit();
		FileRepResync_ShmemInit();
		FileRepIpc_ShmemInit();
		FileRepLog_ShmemInit();
	}
	
#ifdef FAULT_INJECTOR
	FaultInjector_ShmemInit();
#endif

#ifdef USE_TEST_UTILS
	if (gp_simex_init)
	{
		// initialize shmem segment for SimEx
		simex_set_sync_bitvector_container(
			SyncBitVector_ShmemInit("SimEx bit vector container", simex_get_subclass_count()));
	}
#endif /* USE_TEST_UTILS */

	/*
	 * Set up other modules that need some shared memory space
	 */
	BTreeShmemInit();
	SyncScanShmemInit();
	workfile_mgr_cache_init();
	BackendCancelShmemInit();

#ifdef EXEC_BACKEND

	/*
	 * Alloc the win32 shared backend array
	 */
	if (!IsUnderPostmaster)
		ShmemBackendArrayAllocation();
#endif

	if (gp_enable_resqueue_priority)
		BackoffStateInit();

	/*
	 * Now give loadable modules a chance to set up their shmem allocations
	 */
	if (shmem_startup_hook)
		shmem_startup_hook();
}
