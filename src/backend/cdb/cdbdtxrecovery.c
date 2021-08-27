/*-------------------------------------------------------------------------
 *
 * cdbdtxrecovery.c
 *	  Routine to recover distributed transactions
 *
 *
 * Copyright (c) 2018-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <unistd.h>

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/procarray.h"
#include "utils/faultinjector.h"

#include "access/xact.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbdisp_query.h"
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "libpq-int.h"

#define MAX_FREQ_CHECK_TIMES 12
static int frequent_check_times;

volatile bool *shmDtmStarted = NULL;
volatile pid_t *shmDtxRecoveryPid = NULL;

/* transactions need recover */
TMGXACT_LOG *shmCommittedGxactArray;
volatile int *shmNumCommittedGxacts;

static int	redoFileFD = -1;
static int	redoFileOffset;

static volatile sig_atomic_t got_SIGHUP = false;

typedef struct InDoubtDtx
{
	char		gid[TMGIDSIZE];
} InDoubtDtx;

/*
 * Directory where Utility Mode DTM REDO file reside within PGDATA
 */
#define UTILITYMODEDTMREDO_DIR "pg_utilitymodedtmredo"

/*
 * File name for Utility Mode DTM REDO
 */
#define UTILITYMODEDTMREDO_FILE "savedtmredo.file"

static void recoverTM(void);
static bool recoverInDoubtTransactions(void);
static HTAB *gatherRMInDoubtTransactions(int prepared_seconds, bool raiseError);
static void abortRMInDoubtTransactions(HTAB *htab);
static void doAbortInDoubt(char *gid);
static bool doNotifyCommittedInDoubt(char *gid);
static void UtilityModeSaveRedo(bool committed, TMGXACT_LOG *gxact_log);
static void ReplayRedoFromUtilityMode(void);
static void RemoveRedoUtilityModeFile(void);
static void AbortOrphanedPreparedTransactions(void);

static bool
doNotifyCommittedInDoubt(char *gid)
{
	bool		succeeded;

	/* UNDONE: Pass real gxid instead of InvalidDistributedTransactionId. */
	succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_RECOVERY_COMMIT_PREPARED,
											 gid,
											 NULL, /* raiseError */ false,
											 cdbcomponent_getCdbComponentsList(), NULL, 0);
	if (!succeeded)
		elog(FATAL, "Crash recovery broadcast of the distributed transaction "
			 		"'Commit Prepared' broadcast failed to one or more segments for gid = %s.", gid);
	else
		elog(LOG, "Crash recovery broadcast of the distributed transaction "
			 	  "'Commit Prepared' broadcast succeeded for gid = %s.", gid);

	return succeeded;
}

static void
doAbortInDoubt(char *gid)
{
	bool		succeeded;

	/* UNDONE: Pass real gxid instead of InvalidDistributedTransactionId. */
	succeeded = doDispatchDtxProtocolCommand(DTX_PROTOCOL_COMMAND_RECOVERY_ABORT_PREPARED,
											 gid,
											 NULL, /* raiseError */ false,
											 cdbcomponent_getCdbComponentsList(), NULL, 0);
	if (!succeeded)
	{
		DisconnectAndDestroyAllGangs(true);
		CheckForResetSession();
		elog(LOG, "Crash recovery retry of the distributed transaction "
			 		"'Abort Prepared' broadcast failed to one or more segments "
					"for gid = %s.  System will retry again later", gid);
	}
	else
		elog(LOG, "Crash recovery broadcast of the distributed transaction "
			 	  "'Abort Prepared' broadcast succeeded for gid = %s", gid);
}

static void
GetRedoFileName(char *path)
{
	snprintf(path, MAXPGPATH,
			 "%s/" UTILITYMODEDTMREDO_DIR "/" UTILITYMODEDTMREDO_FILE, DataDir);
	elogif(Debug_print_full_dtm, LOG,
		   "Returning save DTM redo file path = %s", path);
}

/*
 * recoverTM:
 * perform TM recovery, this connects to all QE and resolve all in-doubt txn.
 *
 * This gets called when there is not any other DTM activity going on.
 *
 * First, we'll replay the dtm log and get our shmem as up to date as possible
 * in order to help resolve in-doubt transactions.	Then we'll go through and
 * try and resolve in-doubt transactions based on information in the DTM log.
 * The remaining in-doubt transactions that remain (ones the DTM doesn't know
 * about) are all ABORTed.
 *
 * If we're in read-only mode; we need to get started, but we can't run the
 * full recovery. So we go get the highest distributed-xid, but don't run
 * the recovery.
 */
static void
recoverTM(void)
{
	elogif(Debug_print_full_dtm, LOG,
		   "Starting to Recover DTM...");

	/*
	 * attempt to recover all in-doubt transactions.
	 *
	 * first resolve all in-doubt transactions from the DTM's perspective and
	 * then resolve any remaining in-doubt transactions that the RMs have.
	 */
	recoverInDoubtTransactions();

	/* finished recovery successfully. */
	*shmDtmStarted = true;
	elog(LOG, "DTM Started");

	/*
	 * dtx recovery process won't exit, so signal postmaster to launch
	 * bg workers that depend on dtx recovery.
	 */
	SendPostmasterSignal(PMSIGNAL_BACKGROUND_WORKER_CHANGE);
}

/* 
 * recoverInDoubtTransactions:
 * Go through all in-doubt transactions that the DTM knows about and
 * resolve them.
 */
static bool
recoverInDoubtTransactions(void)
{
	int			i;
	HTAB	   *htab;

	elogif(Debug_print_full_dtm, LOG,
		   "recover in-doubt distributed transactions");

	ReplayRedoFromUtilityMode();

	/*
	 * For each committed transaction found in the redo pass that was not
	 * matched by a forget committed record, change its state indicating
	 * committed notification needed.  Attempt a notification.
	 */
	elogif(Debug_print_full_dtm, LOG,
		   "Going to retry commit notification for distributed transactions (count = %d)",
		   *shmNumCommittedGxacts);

	for (i = 0; i < *shmNumCommittedGxacts; i++)
	{
		TMGXACT_LOG *gxact_log = &shmCommittedGxactArray[i];

		Assert(gxact_log->gxid != InvalidDistributedTransactionId);

		elogif(Debug_print_full_dtm, LOG,
			   "Recovering committed distributed transaction gid = %s",
			   gxact_log->gid);

		doNotifyCommittedInDoubt(gxact_log->gid);

		RecordDistributedForgetCommitted(gxact_log);
	}

	*shmNumCommittedGxacts = 0;

	/*
	 * Any in-doubt transctions found will be for aborted
	 * transactions. Gather in-doubt transactions and issue aborts.
	 */
	htab = gatherRMInDoubtTransactions(0, true);

	/*
	 * go through and resolve any remaining in-doubt transactions that the
	 * RM's have AFTER recoverDTMInDoubtTransactions.  ALL of these in doubt
	 * transactions will be ABORT'd.  The fact that the DTM doesn't know about
	 * them means that something bad happened before everybody voted to
	 * COMMIT.
	 */
	abortRMInDoubtTransactions(htab);

	/* get rid of the hashtable */
	hash_destroy(htab);

	RemoveRedoUtilityModeFile();

	return true;
}

/*
 * gatherRMInDoubtTransactions:
 * Builds a hashtable of all of the in-doubt transactions that exist on the
 * segment databases.  The hashtable basically just serves as a single list
 * without duplicates of all the in-doubt transactions.  It does not keep track
 * of which seg db's have which transactions in-doubt.  It currently doesn't
 * need to due to the way we handle this information later.
 *
 * Parameter prepared_seconds: Gather prepared transactions which have
 * existed for at least prepared_seconds seconds.
 * Parameter raiseError: if true, rethrow the error else ignore it.
 */
static HTAB *
gatherRMInDoubtTransactions(int prepared_seconds, bool raiseError)
{
	CdbPgResults cdb_pgresults = {NULL, 0};
	PGresult   *rs;
	char		cmdbuf[256];
	InDoubtDtx *lastDtx = NULL;

	HASHCTL		hctl;
	HTAB	   *htab = NULL;
	int			i,
				j,
				rows;
	bool		found;

	snprintf(cmdbuf, sizeof(cmdbuf), "select gid from pg_prepared_xacts where "
			 "prepared < now() - interval'%d seconds'",
			 prepared_seconds);

	PG_TRY();
	{
		CdbDispatchCommand(cmdbuf, DF_NONE, &cdb_pgresults);
	}
	PG_CATCH();
	{
		cdbdisp_clearCdbPgResults(&cdb_pgresults);

		if (raiseError)
			PG_RE_THROW();
		else
		{
			if (!elog_demote(WARNING))
			{
				elog(LOG, "unable to demote an error");
				PG_RE_THROW();
			}

			EmitErrorReport();
			FlushErrorState();
			DisconnectAndDestroyAllGangs(true);
		}
	}
	PG_END_TRY();

	/* if any result set is nonempty, there are in-doubt transactions. */
	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		rs = cdb_pgresults.pg_results[i];
		rows = PQntuples(rs);

		for (j = 0; j < rows; j++)
		{
			char	   *gid;

			/*
			 * we dont setup our hashtable until we know we have at least one
			 * in doubt transaction
			 */
			if (htab == NULL)
			{
				/* setup a hash table */
				hctl.keysize = TMGIDSIZE;	/* GID */
				hctl.entrysize = sizeof(InDoubtDtx);

				htab = hash_create("InDoubtDtxHash", 10, &hctl, HASH_ELEM);

				if (htab == NULL)
					ereport(FATAL,
							(errcode(ERRCODE_OUT_OF_MEMORY),
							 errmsg("DTM could not allocate hash table for InDoubtDtxList")));
			}

			gid = PQgetvalue(rs, j, 0);

			/* now we can add entry to hash table */
			lastDtx = (InDoubtDtx *) hash_search(htab, gid, HASH_ENTER, &found);

			/*
			 * only need to bother doing work if there isn't already an entry
			 * for our GID
			 */
			if (!found)
			{
				elog(DEBUG3, "Found in-doubt transaction with GID: %s on remote RM", gid);

				strncpy(lastDtx->gid, gid, TMGIDSIZE);
			}
		}
	}

	return htab;
}

/*
 * abortRMInDoubtTransactions:
 * Goes through all the InDoubtDtx's in the provided htab and ABORTs them
 * across all of the QEs by sending a ROLLBACK PREPARED.
 *
 */
static void
abortRMInDoubtTransactions(HTAB *htab)
{
	HASH_SEQ_STATUS status;
	InDoubtDtx *entry = NULL;

	if (htab == NULL)
		return;

	/*
	 * now we have a nice hashtable full of in-doubt dtx's that we need to
	 * resolve.  so we'll use a nice big hammer to get this job done.  instead
	 * of keeping track of which QEs have a prepared txn to be aborted and
	 * which ones don't.  we just issue a ROLLBACK to all of them and ignore
	 * any pesky errors.  This is certainly not an elegant solution but is OK
	 * for now.
	 */
	hash_seq_init(&status, htab);

	while ((entry = (InDoubtDtx *) hash_seq_search(&status)) != NULL)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "Aborting in-doubt transaction with gid = %s", entry->gid);

		doAbortInDoubt(entry->gid);
	}
}

/*
 * abortOrphanedTransactions:
 * Goes through all the InDoubtDtx's in the provided htab and find orphaned
 * ones and then abort them.
 */
static void
abortOrphanedTransactions(HTAB *htab)
{
	HASH_SEQ_STATUS status;
	InDoubtDtx *entry = NULL;
	DistributedTransactionTimeStamp	distribTimeStamp;
	DistributedTransactionId	gxid;

	if (htab == NULL)
		return;

	hash_seq_init(&status, htab);

	while ((entry = (InDoubtDtx *) hash_seq_search(&status)) != NULL)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "Finding orphaned transactions with gid = %s", entry->gid);

		dtxCrackOpenGid(entry->gid, &distribTimeStamp, &gxid);

		if (!IsDtxInProgress(distribTimeStamp, gxid))
		{
			elog(LOG, "Aborting orphaned transactions with gid = %s", entry->gid);
			doAbortInDoubt(entry->gid);
		}
	}
}

static void
UtilityModeSaveRedo(bool committed, TMGXACT_LOG *gxact_log)
{
	TMGXACT_UTILITY_MODE_REDO utilityModeRedo;
	int			write_len;

	utilityModeRedo.committed = committed;
	memcpy(&utilityModeRedo.gxact_log, gxact_log, sizeof(TMGXACT_LOG));

	elogif(Debug_print_full_dtm, LOG,
		   "Writing {committed = %s, gid = %s, gxid = %u} to DTM redo file",
		   (utilityModeRedo.committed ? "true" : "false"),
		   utilityModeRedo.gxact_log.gid,
		   utilityModeRedo.gxact_log.gxid);

	write_len = write(redoFileFD, &utilityModeRedo, sizeof(TMGXACT_UTILITY_MODE_REDO));
	if (write_len != sizeof(TMGXACT_UTILITY_MODE_REDO))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write save DTM redo file : %m")));
}

static void
ReplayRedoFromUtilityMode(void)
{
	TMGXACT_UTILITY_MODE_REDO utilityModeRedo;

	int			fd;
	int			read_len;
	int			errno;
	char		path[MAXPGPATH];
	int			entries;

	entries = 0;

	GetRedoFileName(path);

	fd = open(path, O_RDONLY, 0);
	if (fd < 0)
	{
		/* UNDONE: Distinquish "not found" from other errors. */
		elogif(Debug_print_full_dtm, LOG,
			   "Could not open DTM redo file %s for reading", path);
		return;
	}

	elogif(Debug_print_full_dtm, LOG,
		   "Successfully opened DTM redo file %s for reading", path);

	while (true)
	{
		errno = 0;
		read_len = read(fd, &utilityModeRedo, sizeof(TMGXACT_UTILITY_MODE_REDO));

		if (read_len == 0)
			break;
		else if (read_len != sizeof(TMGXACT_UTILITY_MODE_REDO) && errno == 0)
			elog(ERROR, "Bad redo length (expected %d and found %d)",
				 (int) sizeof(TMGXACT_UTILITY_MODE_REDO), read_len);
		else if (errno != 0)
		{
			close(fd);
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("error reading DTM redo file: %m")));
		}

		elogif(Debug_print_full_dtm, LOG,
			   "Read {committed = %s, gid = %s, gxid = %u} from DTM redo file",
			   (utilityModeRedo.committed ? "true" : "false"),
			   utilityModeRedo.gxact_log.gid,
			   utilityModeRedo.gxact_log.gxid);
		if (utilityModeRedo.committed)
			redoDistributedCommitRecord(&utilityModeRedo.gxact_log);
		else
			redoDistributedForgetCommitRecord(&utilityModeRedo.gxact_log);

		entries++;
	}

	elogif(Debug_print_full_dtm, LOG,
		   "Processed %d entries from DTM redo file", entries);
	close(fd);
}

static void
RemoveRedoUtilityModeFile(void)
{
	char		path[MAXPGPATH];
	bool		removed;

	GetRedoFileName(path);
	removed = (unlink(path) == 0);
	elogif(Debug_print_full_dtm, LOG,
		   "Removed DTM redo file %s (%s)", path, (removed ? "true" : "false"));
}

void
UtilityModeCloseDtmRedoFile(void)
{
	if (Gp_role != GP_ROLE_UTILITY)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "Not in Utility Mode (role = %s)-- skipping closing DTM redo file",
			   role_to_string(Gp_role));
		return;
	}
	elogif(Debug_print_full_dtm, LOG, "Closing DTM redo file");
	close(redoFileFD);
}

void
UtilityModeFindOrCreateDtmRedoFile(void)
{
	char		path[MAXPGPATH];

	if (Gp_role != GP_ROLE_UTILITY)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "Not in Utility Mode (role = %s) -- skipping finding or creating DTM redo file",
			   role_to_string(Gp_role));
		return;
	}
	GetRedoFileName(path);

	redoFileFD = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (redoFileFD < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create save DTM redo file \"%s\"", path)));

	redoFileOffset = lseek(redoFileFD, 0, SEEK_END);
	elogif(Debug_print_full_dtm, LOG,
		   "Successfully opened DTM redo file %s (end offset %d)",
		   path, redoFileOffset);
}

/*
 * Redo transaction commit log record.
 */
void
redoDtxCheckPoint(TMGXACT_CHECKPOINT *gxact_checkpoint)
{
	int			committedCount;

	int			i;

	/*
	 * For checkpoint same as REDO, lets add entries to file in utility and
	 * in-memory if Dispatch.
	 */
	committedCount = gxact_checkpoint->committedCount;
	elogif(Debug_print_full_dtm, LOG,
		   "redoDtxCheckPoint has committedCount = %d", committedCount);

	for (i = 0; i < committedCount; i++)
		redoDistributedCommitRecord(&gxact_checkpoint->committedGxactArray[i]);
}

/*
 * Redo transaction commit log record.
 */
void
redoDistributedCommitRecord(TMGXACT_LOG *gxact_log)
{
	int			i;

	/*
	 * The length check here requires the identifer have a trailing NUL
	 * character.
	 */
	if (strlen(gxact_log->gid) >= TMGIDSIZE)
		elog(PANIC, "Distribute transaction identifier too long (%d)",
			 (int) strlen(gxact_log->gid));

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "DB in Utility mode.  Save DTM distributed commit until later.");
		UtilityModeSaveRedo(true, gxact_log);
		return;
	}

	for (i = 0; i < *shmNumCommittedGxacts; i++)
	{
		if (strcmp(gxact_log->gid, shmCommittedGxactArray[i].gid) == 0)
			return;
	}

	if (i == *shmNumCommittedGxacts)
	{
#ifdef FAULT_INJECTOR
		if (SIMPLE_FAULT_INJECTOR("standby_gxacts_overflow") == FaultInjectorTypeSkip)
		{
			max_tm_gxacts = 1;
			elog(LOG, "Committed gid array length: %d", *shmNumCommittedGxacts);
		}
#endif

		/*
		 * Transaction not found, this is the first log of this transaction.
		 */
		if (*shmNumCommittedGxacts >= max_tm_gxacts)
		{
			StringInfoData gxact_array;

			initStringInfo(&gxact_array);
			for (int j = 0; j < *shmNumCommittedGxacts; j++)
			{
				appendStringInfo(&gxact_array, "shmCommittedGxactArray[%d]: %s\n",
					j, shmCommittedGxactArray[j].gid);
			}
			ereport(FATAL,
					(errmsg("the limit of %d distributed transactions has been reached "\
							"while adding gid = %s. Committed gid array length: %d, dump:\n%s",
							max_tm_gxacts, gxact_log->gid, *shmNumCommittedGxacts, gxact_array.data),
					 errdetail("It should not happen. Temporarily increase "
							   "max_connections (need postmaster reboot) on "
							   "the postgres (master or standby) to work "
							   "around this issue and then report a bug")));
		}

		shmCommittedGxactArray[(*shmNumCommittedGxacts)++] = *gxact_log;
		elogif(Debug_print_full_dtm, LOG,
			   "Crash recovery redo added committed distributed transaction gid = %s", gxact_log->gid);
	}
}

/*
 * Redo transaction forget commit log record.
 */
void
redoDistributedForgetCommitRecord(TMGXACT_LOG *gxact_log)
{
	int			i;

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elogif(Debug_print_full_dtm, LOG,
			   "DB in Utility mode.  Save DTM disributed forget until later.");
		UtilityModeSaveRedo(false, gxact_log);
		return;
	}

	for (i = 0; i < *shmNumCommittedGxacts; i++)
	{
		if (strcmp(gxact_log->gid, shmCommittedGxactArray[i].gid) == 0)
		{
			/* found an active global transaction */
			elogif(Debug_print_full_dtm, INFO,
				   "Crash recovery redo removed committed distributed transaction gid = %s for forget",
				   gxact_log->gid);

			/*
			 * there's no concurrent access to shmCommittedGxactArray during
			 * recovery
			 */
			(*shmNumCommittedGxacts)--;
			if (i != *shmNumCommittedGxacts)
				shmCommittedGxactArray[i] = shmCommittedGxactArray[*shmNumCommittedGxacts];

			return;
		}
	}

	elogif(Debug_print_full_dtm, WARNING,
		   "Crash recovery redo did not find committed distributed transaction gid = %s for forget",
		   gxact_log->gid);
}

bool
DtxRecoveryStartRule(Datum main_arg)
{
	/* only start dtx recovery in dispatch mode */
	if (IsUnderMasterDispatchMode())
		return true;

	return false;
}

static void
AbortOrphanedPreparedTransactions()
{
	HTAB	   *htab;

#ifdef FAULT_INJECTOR
	if (SIMPLE_FAULT_INJECTOR("before_orphaned_check") == FaultInjectorTypeSkip)
		return;
#endif

	StartTransactionCommand();
	htab = gatherRMInDoubtTransactions(gp_dtx_recovery_prepared_period, false);

	/* in case an error happens somehow. */
	if (htab != NULL)
	{
		abortOrphanedTransactions(htab);

		/* get rid of the hashtable */
		hash_destroy(htab);
	}

	CommitTransactionCommand();
	DisconnectAndDestroyAllGangs(true);

	SIMPLE_FAULT_INJECTOR("after_orphaned_check");
}

static void
sigIntHandler(SIGNAL_ARGS)
{
	if (frequent_check_times == 0)
		frequent_check_times = MAX_FREQ_CHECK_TIMES;

	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

static void
sigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;

	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

pid_t
DtxRecoveryPID(void)
{
	return *shmDtxRecoveryPid;
}

/*
 * DtxRecoveryMain
 */
void
DtxRecoveryMain(Datum main_arg)
{
	*shmDtxRecoveryPid = MyProcPid;

	/*
	 * reread postgresql.conf if requested
	 */
	pqsignal(SIGHUP, sigHupHandler);
	pqsignal(SIGINT, sigIntHandler);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to postgres */
	BackgroundWorkerInitializeConnection(DB_FOR_COMMON_ACCESS, NULL);

	/*
	 * Do dtx recovery process.  It is possible that *shmDtmStarted is true
	 * here if we terminate after this code block, e.g. due to error and then
	 * postmaster restarts dtx recovery.
	 */
	if (!*shmDtmStarted)
	{
		set_ps_display("recovering", false);

		StartTransactionCommand();
		recoverTM();
		CommitTransactionCommand();
		DisconnectAndDestroyAllGangs(true);

		set_ps_display("", false);
	}

	while (true)
	{
		int rc;

		CHECK_FOR_INTERRUPTS();

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		/* Find orphaned prepared transactions and abort them. */
		AbortOrphanedPreparedTransactions();

		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   frequent_check_times > 0 ?
					   5 * 1000L : gp_dtx_recovery_interval * 1000L);
		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);

		if (frequent_check_times > 0)
			frequent_check_times--;
	}

	/* One iteration done, go away */
	proc_exit(0);
}
