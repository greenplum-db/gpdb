/*-------------------------------------------------------------------------
 *
 * walreceiver.c
 *
 * The WAL receiver process (walreceiver) is new as of Postgres 9.0. It
 * is the process in the standby server that takes charge of receiving
 * XLOG records from a primary server during streaming replication.
 *
 * When the startup process determines that it's time to start streaming,
 * it instructs postmaster to start walreceiver. Walreceiver first connects
 * to the primary server (it will be served by a walsender process
 * in the primary server), and then keeps receiving XLOG records and
 * writing them to the disk as long as the connection is alive. As XLOG
 * records are received and flushed to disk, it updates the
 * WalRcv->receivedUpto variable in shared memory, to inform the startup
 * process of how far it can proceed with XLOG replay.
 *
 * Normal termination is by SIGTERM, which instructs the walreceiver to
 * exit(0). Emergency termination is by SIGQUIT; like any postmaster child
 * process, the walreceiver will simply abort and exit on SIGQUIT. A close
 * of the connection and a FATAL error are treated not as a crash but as
 * normal operation.
 *
 * This file contains the server-facing parts of walreceiver. The libpq-
 * specific parts are in the libpqwalreceiver module.
 *
 * Portions Copyright (c) 2010-2012, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/walreceiver.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "replication/walprotocol.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"
#include "storage/procarray.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "utils/resowner.h"
#include "utils/timestamp.h"
#include "utils/faultinjector.h"


/* GUC variables */
int			wal_receiver_status_interval = 10;

#define NAPTIME_PER_CYCLE 100	/* max sleep time between cycles (100ms) */

/*
 * These variables are used similarly to openLogFile/SegNo/Off,
 * but for walreceiver to write the XLOG. recvFileTLI is the TimeLineID
 * corresponding the filename of recvFile.
 */
static File		recvFile = -1;
static TimeLineID	recvFileTLI = 0;
static uint32 recvId = 0;
static uint32 recvSeg = 0;
static uint32 recvOff = 0;

/*
 * Flags set by interrupt handlers of walreceiver for later service in the
 * main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGTERM = false;

/* Following flags should be strictly used for testing purposes ONLY */
static volatile sig_atomic_t wait_before_rcv = false;
static volatile sig_atomic_t wait_before_send = false;
static volatile sig_atomic_t wait_before_send_ack = false;
static volatile sig_atomic_t resume = false;

/*
 * LogstreamResult indicates the byte positions that we have already
 * written/fsynced.
 */
static struct
{
	XLogRecPtr	Write;			/* last byte + 1 written out in the standby */
	XLogRecPtr	Flush;			/* last byte + 1 flushed in the standby */
}	LogstreamResult;

static StandbyReplyMessage reply_message;

/*
 * About SIGTERM handling:
 *
 * We can't just exit(1) within SIGTERM signal handler, because the signal
 * might arrive in the middle of some critical operation, like while we're
 * holding a spinlock. We also can't just set a flag in signal handler and
 * check it in the main loop, because we perform some blocking operations
 * like libpqrcv_PQexec(), which can take a long time to finish.
 *
 * We use a combined approach: When WalRcvImmediateInterruptOK is true, it's
 * safe for the signal handler to elog(FATAL) immediately. Otherwise it just
 * sets got_SIGTERM flag, which is checked in the main loop when convenient.
 *
 * This is very much like what regular backends do with ImmediateInterruptOK,
 * ProcessInterrupts() etc.
 */
static volatile bool WalRcvImmediateInterruptOK = false;

/* Prototypes for private functions */
static void ProcessWalRcvInterrupts(void);
static void EnableWalRcvImmediateExit(void);
static void DisableWalRcvImmediateExit(void);
static void WalRcvDie(int code, Datum arg);
static void XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len);
static void XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr);
static void XLogWalRcvFlush(bool dying);
static void XLogWalRcvSendReply(void);
static void ProcessWalSndrMessage(XLogRecPtr walEnd, TimestampTz sendTime);

/* Signal handlers */
static void WalRcvSigHupHandler(SIGNAL_ARGS);
static void WalRcvShutdownHandler(SIGNAL_ARGS);
static void WalRcvQuickDieHandler(SIGNAL_ARGS);
static void WalRcvUsr2Handler(SIGNAL_ARGS);
static void WalRcvCrashHandler(SIGNAL_ARGS);


static void
ProcessWalRcvInterrupts(void)
{
	/*
	 * Although walreceiver interrupt handling doesn't use the same scheme as
	 * regular backends, call CHECK_FOR_INTERRUPTS() to make sure we receive
	 * any incoming signals on Win32.
	 */
	CHECK_FOR_INTERRUPTS();

	if (got_SIGTERM)
	{
		WalRcvImmediateInterruptOK = false;
		ereport(FATAL,
				(errcode(ERRCODE_ADMIN_SHUTDOWN),
				 errmsg("terminating walreceiver process due to administrator command")));
	}
}

static void
EnableWalRcvImmediateExit(void)
{
	WalRcvImmediateInterruptOK = true;
	ProcessWalRcvInterrupts();
}

static void
DisableWalRcvImmediateExit(void)
{
	WalRcvImmediateInterruptOK = false;
	ProcessWalRcvInterrupts();
}

/* Main entry point for walreceiver process */
void
WalReceiverMain(void)
{
	char		conninfo[MAXCONNINFO];
	XLogRecPtr	startpoint;
	File		pid_file;

	/* use volatile pointer to prevent code rearrangement */
	volatile WalRcvData *walrcv = WalRcv;
	sigjmp_buf	local_sigjmp_buf;

	/*
	 * WalRcv should be set up already (if we are a backend, we inherit this
	 * by fork() or EXEC_BACKEND mechanism from the postmaster).
	 */
	Assert(walrcv != NULL);

	/*
	 * Mark walreceiver as running in shared memory.
	 *
	 * Do this as early as possible, so that if we fail later on, we'll set
	 * state to STOPPED. If we die before this, the startup process will keep
	 * waiting for us to start up, until it times out.
	 */
	SpinLockAcquire(&walrcv->mutex);
	Assert(walrcv->pid == 0);
	switch (walrcv->walRcvState)
	{
		case WALRCV_STOPPING:
			/* If we've already been requested to stop, don't start up. */
			walrcv->walRcvState = WALRCV_STOPPED;
			/* fall through */

		case WALRCV_STOPPED:
			SpinLockRelease(&walrcv->mutex);
			proc_exit(1);
			break;

		case WALRCV_STARTING:
			/* The usual case */
			break;

		case WALRCV_RUNNING:
			/* Shouldn't happen */
			elog(PANIC, "walreceiver still running according to shared memory state");
	}
	/* Advertise our PID so that the startup process can kill us */
	walrcv->pid = MyProcPid;
	walrcv->walRcvState = WALRCV_RUNNING;

	elogif(debug_walrepl_rcv, LOG,
			"WAL receiver state is set to '%s'",
			WalRcvGetStateString(walrcv->walRcvState));

	/* Fetch information required to start streaming */
	strlcpy(conninfo, (char *) walrcv->conninfo, MAXCONNINFO);
	startpoint = walrcv->receiveStart;

	/* Initialise to a sanish value */
	walrcv->lastMsgSendTime = walrcv->lastMsgReceiptTime = walrcv->latestWalEndTime = GetCurrentTimestamp();

	SpinLockRelease(&walrcv->mutex);

	/* Arrange to clean up at walreceiver exit */
	on_shmem_exit(WalRcvDie, 0);

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.	(walreceiver probably never has
	 * any child processes, but for consistency we make all postmaster child
	 * processes do this.)
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/* Properly accept or ignore signals the postmaster might send us */
	pqsignal(SIGHUP, WalRcvSigHupHandler);		/* set flag to read config
												 * file */
	pqsignal(SIGINT, SIG_IGN);
	pqsignal(SIGTERM, WalRcvShutdownHandler);	/* request shutdown */
	pqsignal(SIGQUIT, WalRcvQuickDieHandler);	/* hard crash time */
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN);
	pqsignal(SIGUSR2, WalRcvUsr2Handler);

	/* Reset some signals that are accepted by postmaster but not here */
	pqsignal(SIGCHLD, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);

#ifdef SIGILL
	pqsignal(SIGILL, WalRcvCrashHandler);
#endif
#ifdef SIGSEGV
	pqsignal(SIGSEGV, WalRcvCrashHandler);
#endif
#ifdef SIGBUS
	pqsignal(SIGBUS, WalRcvCrashHandler);
#endif

	/* We allow SIGQUIT (quickdie) at all times */
	sigdelset(&BlockSig, SIGQUIT);

	/*
	 * Create a resource owner to keep track of our resources (not clear that
	 * we need this, but may as well have one).
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "Wal Receiver");

	/*
	 * In case of ERROR, walreceiver just dies cleanly. Startup process
	 * will invoke another one if necessary.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		EmitErrorReport();

		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	/* Unblock signals (they were blocked when the postmaster forked us) */
	PG_SETMASK(&UnBlockSig);

	/* Establish the connection to the primary for XLOG streaming */
	EnableWalRcvImmediateExit();

	/*
	 * (Testing Purpose) - Create a PID file under $PGDATA directory
	 * This file existence is very temporary and will be removed once the WAL receiver
	 * sets up successful connection. Verifying the presence of the PID file
	 * helps to find if the WAL Receiver was actually started or not.
	 * Again, This is just for testing purpose. With enhancements in future,
	 * this change can/will go away.
	 */
	if ((pid_file = PathNameOpenFile("wal_rcv.pid", O_RDWR | O_CREAT| PG_BINARY, 0600)) < 0)
	{
		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for reading: %m",
						"wal_rcv.pid")));
	}
	else
		FileClose(pid_file);

	walrcv_connect(conninfo, startpoint);

	/* (Testing Purpose) - Now drop the PID file */
	unlink("wal_rcv.pid");

	DisableWalRcvImmediateExit();

	/* Initialize LogstreamResult, reply_message */
	LogstreamResult.Write = LogstreamResult.Flush = GetXLogReplayRecPtr(NULL);
	MemSet(&reply_message, 0, sizeof(reply_message));

	/* Loop until end-of-streaming or error */
	for (;;)
	{
		unsigned char type;
		char	   *buf;
		int			len;

		/*
		 * Emergency bailout if postmaster has died.  This is to avoid the
		 * necessity for manual cleanup of all postmaster children.
		 */
		if (!PostmasterIsAlive(true))
			exit(1);

		/*
		 * Exit walreceiver if we're not in recovery. This should not happen,
		 * but cross-check the status here.
		 */
		if (!RecoveryInProgress())
			ereport(FATAL,
					(errmsg("cannot continue WAL streaming, recovery has already ended"),
					errSendAlert(true)));

		/* Process any requests or signals received recently */
		ProcessWalRcvInterrupts();

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		/* Perform suspend if signaled by an external entity (Testing Purpose) */
		if (wait_before_rcv)
		{
			while(true)
			{
				if (resume)
				{
					resume = false;
					break;
				}
				pg_usleep(5000);
			}
			wait_before_rcv = false;
		}

		/* Wait a while for data to arrive */
		if (walrcv_receive(NAPTIME_PER_CYCLE, &type, &buf, &len))
		{
			/* Accept the received data, and process it */
			XLogWalRcvProcessMsg(type, buf, len);

			/* Receive any more data we can without sleeping */
			while (walrcv_receive(0, &type, &buf, &len))
				XLogWalRcvProcessMsg(type, buf, len);

			/* Let the master know that we received some data. */
			XLogWalRcvSendReply();

			/*
			 * If we've written some records, flush them to disk and let the
			 * startup process and primary server know about them.
			 */
			XLogWalRcvFlush(false);
		}
		else
		{
			/*
			 * We didn't receive anything new, but send a status update to the
			 * master anyway, to report any progress in applying WAL.
			 */
			XLogWalRcvSendReply();
		}
	}
}

/*
 * This is the handler for USR2 signal. Currently, it is used for testing
 * purposes. Normally, an external entity signals (USR2) the WAL receiver process
 * and based on the input from an external file on disk the WAL Receiver acts
 * upon that.
 */
static void
WalRcvUsr2Handler(SIGNAL_ARGS)
{
#define BUF_SIZE 80
	File file;
	int nbytes = 0;
	char buf[BUF_SIZE];

	/* Read the type of action to take later from the file in the $PGDATA */
	if ((file = PathNameOpenFile("wal_rcv_test", O_RDONLY | PG_BINARY, 0600)) < 0)
	{
		/* Do not error out inside signal handler. Ignore it.*/
		return;
	}
	else
	{
		nbytes = FileRead(file, buf, BUF_SIZE - 1);
		if (nbytes <= 0)
		{
			/* Cleanup */
			FileClose(file);

			/* Don't error out. Ignore. */
			return;
		}
		FileClose(file);

		Assert(nbytes < BUF_SIZE);
		buf[nbytes] = '\0';

		if (strcmp(buf,"wait_before_send_ack") == 0)
			wait_before_send_ack = true;
		else if (strcmp(buf,"wait_before_rcv") == 0)
			wait_before_rcv = true;
		else if (strcmp(buf,"wait_before_send") == 0)
			wait_before_send = true;
		else if (strcmp(buf,"resume") == 0)
			resume = true;
		else
			return;
	}
}

/*
 * Mark us as STOPPED in shared memory at exit.
 */
static void
WalRcvDie(int code, Datum arg)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile WalRcvData *walrcv = WalRcv;

	/* Ensure that all WAL records received are flushed to disk */
	XLogWalRcvFlush(true);

	SpinLockAcquire(&walrcv->mutex);
	Assert(walrcv->walRcvState == WALRCV_RUNNING ||
		   walrcv->walRcvState == WALRCV_STOPPING);
	walrcv->walRcvState = WALRCV_STOPPED;
	walrcv->pid = 0;
	SpinLockRelease(&walrcv->mutex);

	/* Terminate the connection gracefully. */
	walrcv_disconnect();
}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
WalRcvSigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;
}

/* SIGTERM: set flag for main loop, or shutdown immediately if safe */
static void
WalRcvShutdownHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_SIGTERM = true;

	/* Don't joggle the elbow of proc_exit */
	if (!proc_exit_inprogress && WalRcvImmediateInterruptOK)
		ProcessWalRcvInterrupts();

	errno = save_errno;
}

/*
 * WalRcvQuickDieHandler() occurs when signalled SIGQUIT by the postmaster.
 *
 * Some backend has bought the farm, so we need to stop what we're doing and
 * exit.
 */
static void
WalRcvQuickDieHandler(SIGNAL_ARGS)
{
	PG_SETMASK(&BlockSig);

	/*
	 * We DO NOT want to run proc_exit() callbacks -- we're here because
	 * shared memory may be corrupted, so we don't want to try to clean up our
	 * transaction.  Just nail the windows shut and get out of town.  Now that
	 * there's an atexit callback to prevent third-party code from breaking
	 * things by calling exit() directly, we have to reset the callbacks
	 * explicitly to make this work as intended.
	 */
	on_exit_reset();

	/*
	 * Note we do exit(2) not exit(0).	This is to force the postmaster into a
	 * system reset cycle if some idiot DBA sends a manual SIGQUIT to a random
	 * backend.  This is necessary precisely because we don't clean up our
	 * shared memory state.  (The "dead man switch" mechanism in pmsignal.c
	 * should ensure the postmaster sees this as a crash, too, but no harm in
	 * being doubly sure.)
	 */
	exit(2);
}

static void
WalRcvCrashHandler(SIGNAL_ARGS)
{
	StandardHandlerForSigillSigsegvSigbus_OnMainThread("walreceiver",
														PASS_SIGNAL_ARGS);
}

/*
 * Accept the message from XLOG stream, and process it.
 */
static void
XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len)
{
	switch (type)
	{
		case 'w':				/* WAL records */
			{
				WalDataMessageHeader msghdr;

				if (len < sizeof(WalDataMessageHeader))
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid WAL message received from primary"),
							 errSendAlert(true)));
				/* memcpy is required here for alignment reasons */
				memcpy(&msghdr, buf, sizeof(WalDataMessageHeader));

				elogif(debug_walrepl_rcv, LOG,
					   "walrcv msg metadata -- datastart %s, buflen %d",
					    XLogLocationToString(&(msghdr.dataStart)),(int)len);

				ProcessWalSndrMessage(msghdr.walEnd, msghdr.sendTime);

				buf += sizeof(WalDataMessageHeader);
				len -= sizeof(WalDataMessageHeader);

				XLogWalRcvWrite(buf, len, msghdr.dataStart);
				break;
			}
		case 'k':				/* Keepalive */
			{
				PrimaryKeepaliveMessage keepalive;

				if (len != sizeof(PrimaryKeepaliveMessage))
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid keepalive message received from primary"),
							 errSendAlert(true)));
				/* memcpy is required here for alignment reasons */
				memcpy(&keepalive, buf, sizeof(PrimaryKeepaliveMessage));

				ProcessWalSndrMessage(keepalive.walEnd, keepalive.sendTime);
				break;
			}
		default:
			ereport(ERROR,
					(errcode(ERRCODE_PROTOCOL_VIOLATION),
					 errmsg_internal("invalid replication message type %d",
									 type),	errSendAlert(true)));
	}
}

/*
 * Write XLOG data to disk.
 */
static void
XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr)
{
	int			startoff;
	int			byteswritten;

	while (nbytes > 0)
	{
		int			segbytes;

		if (recvFile < 0 || !XLByteInSeg(recptr, recvId, recvSeg))
		{
			bool		use_existent;

			/*
			 * fsync() and close current file before we switch to next one. We
			 * would otherwise have to reopen this file to fsync it later
			 */
			if (recvFile >= 0)
			{
				char		xlogfname[MAXFNAMELEN];

				XLogWalRcvFlush(false);

				/*
				 * XLOG segment files will be re-read by recovery in startup
				 * process soon, so we don't advise the OS to release cache
				 * pages associated with the file like XLogFileClose() does.
				 */
				FileClose(recvFile);

				/*
				 * Create .done file forcibly to prevent the restored segment from
				 * being archived again later.
				 */
				XLogFileName(xlogfname, recvFileTLI, recvId, recvSeg);
			}
			recvFile = -1;

			/* Create/use new log file */
			XLByteToSeg(recptr, recvId, recvSeg);
			use_existent = true;
			/* For now this only happens on master, so don't care mirror */
			recvFile = XLogFileInitExt(recvId, recvSeg, &use_existent, true);
			recvFileTLI = ThisTimeLineID;
			recvOff = 0;
		}

		/* Calculate the start offset of the received logs */
		startoff = recptr.xrecoff % XLogSegSize;

		if (startoff + nbytes > XLogSegSize)
			segbytes = XLogSegSize - startoff;
		else
			segbytes = nbytes;

		/* Need to seek in the file? */
		if (recvOff != startoff)
		{
			if (FileSeek(recvFile, (off_t) startoff, SEEK_SET) < 0)
				ereport(PANIC,
						(errcode_for_file_access(),
						 errmsg("could not seek in log file %u, "
								"segment %u to offset %u: %m",
								recvId, recvSeg, startoff),
								errSendAlert(true)));
			recvOff = startoff;
		}

		/* OK to write the logs */
		errno = 0;

		byteswritten = FileWrite(recvFile, buf, segbytes);
		if (byteswritten <= 0)
		{
			/* if write didn't set errno, assume no disk space */
			if (errno == 0)
				errno = ENOSPC;
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not write to log file %u, segment %u "
							"at offset %u, length %lu: %m",
							recvId, recvSeg,
							recvOff, (unsigned long) segbytes),
							errSendAlert(true)));
		}

		/* Update state for write */
		XLByteAdvance(recptr, byteswritten);

		recvOff += byteswritten;
		nbytes -= byteswritten;
		buf += byteswritten;

		LogstreamResult.Write = recptr;

		elogif(debug_walrepl_rcv, LOG,
			   "walrcv write -- Wrote %d bytes in file (logid %u, seg %u, offset %d)."
			   "Latest write location is %X/%X.",
			   byteswritten, recvId, recvSeg, startoff,
			   LogstreamResult.Write.xlogid, LogstreamResult.Write.xrecoff);
	}
}

/*
 * Flush the log to disk.
 *
 * If we're in the midst of dying, it's unwise to do anything that might throw
 * an error, so we skip sending a reply in that case.
 */
static void
XLogWalRcvFlush(bool dying)
{
	if (XLByteLT(LogstreamResult.Flush, LogstreamResult.Write))
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile WalRcvData *walrcv = WalRcv;

		if (FileSync(recvFile) != 0)
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not fsync log file %u, segment %u: %m",
							 recvId, recvSeg),
							 errSendAlert(true)));

		LogstreamResult.Flush = LogstreamResult.Write;

		/* Update shared-memory status */
		SpinLockAcquire(&walrcv->mutex);
		if (XLByteLT(walrcv->receivedUpto, LogstreamResult.Flush))
		{
			walrcv->latestChunkStart = walrcv->receivedUpto;
			walrcv->receivedUpto = LogstreamResult.Flush;
		}
		SpinLockRelease(&walrcv->mutex);

		/* Signal the startup process and walsender that new WAL has arrived */
		WakeupRecovery();

		/* Report XLOG streaming progress in PS display */
		if (update_process_title)
		{
			char		activitymsg[50];

			snprintf(activitymsg, sizeof(activitymsg), "streaming %X/%X",
					 LogstreamResult.Write.xlogid,
					 LogstreamResult.Write.xrecoff);
			set_ps_display(activitymsg, false);
		}

		/* Also let the master know that we made some progress */
		if (!dying)
		{
			/* Perform suspend if signaled by an external entity (Testing Purpose) */
			if (wait_before_send_ack)
			{
				while(true)
				{
					if (resume)
					{
						resume = false;
						break;
					}
					pg_usleep(5000);
				}
				wait_before_send_ack = false;
			}
			XLogWalRcvSendReply();
		}

		elogif(debug_walrepl_rcv, LOG,
			   "walrcv fsync -- Fsync'd xlog file (logid %u seg %u). "
			   "Latest flush location is %X/%X with latestchunkstart(%X/%X) receivedupto(%X/%X).",
			   recvId, recvSeg,
			   LogstreamResult.Flush.xlogid, LogstreamResult.Flush.xrecoff,
			   walrcv->latestChunkStart.xlogid, walrcv->latestChunkStart.xrecoff,
			   walrcv->receivedUpto.xlogid, walrcv->receivedUpto.xrecoff);
	}
}

/*
 * Send reply message to primary, indicating our current XLOG positions and
 * the current time.
 */
static void
XLogWalRcvSendReply(void)
{
	char		buf[sizeof(StandbyReplyMessage) + 1];
	TimestampTz now;

	/*
	 * If the user doesn't want status to be reported to the master, be sure
	 * to exit before doing anything at all.
	 */
	if (wal_receiver_status_interval <= 0)
		return;

	/* Get current timestamp. */
	now = GetCurrentTimestamp();

	/*
	 * We can compare the write and flush positions to the last message we
	 * sent without taking any lock, but the apply position requires a spin
	 * lock, so we don't check that unless something else has changed or 10
	 * seconds have passed.  This means that the apply log position will
	 * appear, from the master's point of view, to lag slightly, but since
	 * this is only for reporting purposes and only on idle systems, that's
	 * probably OK.
	 */
	if (XLByteEQ(reply_message.write, LogstreamResult.Write)
		&& XLByteEQ(reply_message.flush, LogstreamResult.Flush)
		&& !TimestampDifferenceExceeds(reply_message.sendTime, now,
									   wal_receiver_status_interval * 1000))
		return;

	/* Construct a new message */
	reply_message.write = LogstreamResult.Write;
	reply_message.flush = LogstreamResult.Flush;
	reply_message.apply = GetXLogReplayRecPtr(NULL);
	reply_message.sendTime = now;

	/* Prepend with the message type and send it. */
	buf[0] = 'r';
	memcpy(&buf[1], &reply_message, sizeof(StandbyReplyMessage));

	/* Perform suspend if signaled by an external entity (Testing Purpose) */
	if (wait_before_send)
	{
		while(true)
		{
			if (resume)
			{
				resume = false;
				break;
			}
			pg_usleep(5000);
		}
		wait_before_send = false;
	}

	walrcv_send(buf, sizeof(StandbyReplyMessage) + 1);

	elog(LOG,
		   "walrcv reply -- Sent write %X/%X flush %X/%X apply %X/%X",
		   reply_message.write.xlogid, reply_message.write.xrecoff,
		   reply_message.flush.xlogid, reply_message.flush.xrecoff,
		   reply_message.apply.xlogid, reply_message.apply.xrecoff);
}

/*
 * Keep track of important messages from primary.
 */
static void
ProcessWalSndrMessage(XLogRecPtr walEnd, TimestampTz sendTime)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile WalRcvData *walrcv = WalRcv;

	TimestampTz lastMsgReceiptTime = GetCurrentTimestamp();

	/* Update shared-memory status */
	SpinLockAcquire(&walrcv->mutex);
	if (XLByteLT(walrcv->latestWalEnd, walEnd))
		walrcv->latestWalEndTime = sendTime;
	walrcv->latestWalEnd = walEnd;
	walrcv->lastMsgSendTime = sendTime;
	walrcv->lastMsgReceiptTime = lastMsgReceiptTime;
	SpinLockRelease(&walrcv->mutex);

	elogif(debug_walrepl_rcv, LOG,
		   "walrcv process msg -- "
		   "sendtime %s, receipttime %s, replication apply delay %d ms "
		   "transfer latency %d ms",
		   timestamptz_to_str(sendTime),
		   timestamptz_to_str(lastMsgReceiptTime),
		   GetReplicationApplyDelay(),
		   GetReplicationTransferLatency());
}

/*
 * Return a string constant representing the state.
 */
const char *
WalRcvGetStateString(WalRcvState state)
{
	switch (state)
	{
		case WALRCV_STOPPED:
			return "stopped";
		case WALRCV_STARTING:
			return "starting";
		case WALRCV_RUNNING:
			return "running";
		case WALRCV_STOPPING:
			return "stopping";
	}
	return "UNKNOWN";
}
