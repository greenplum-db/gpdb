/*-------------------------------------------------------------------------
 *
 * perfmon_segmentinfo.c
 *    Send segment information to perfmon
 *
 * This file contains functions for sending segment information to
 * perfmon. At startup the postmaster process forks a new process
 * that sends segment info in predefined intervals using UDP packets.
 *
 *  Created on: Feb 28, 2010
 *      Author: kkrik
 *
 * Portions Copyright (c) 2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/postmaster/perfmon_segmentinfo.c
 *
 *-------------------------------------------------------------------------
*/

#include <unistd.h>
#include <signal.h>

#include "postmaster/perfmon_segmentinfo.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"

#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/backendid.h"
#include "storage/pmsignal.h"			/* PostmasterIsAlive */

#include "utils/resowner.h"
#include "utils/ps_status.h"

#include "miscadmin.h"
#include "libpq/pqsignal.h"
#include "tcop/tcopprot.h"
#include "cdb/cdbvars.h"
#include "utils/vmem_tracker.h"

/* Sender-related routines */
static void SegmentInfoSender(void);
static void SegmentInfoSenderLoop(void);
NON_EXEC_STATIC void SegmentInfoSenderMain(int argc, char *argv[]);
static void SegmentInfoRequestShutdown(SIGNAL_ARGS);

static volatile bool senderShutdownRequested = false;
static volatile bool isSenderProcess = false;

/* Static gpmon_seginfo_t item, (re)used for sending UDP packets. */
static gpmon_packet_t seginfopkt;

/* GpmonPkt-related routines */
static void InitSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt);
static void UpdateSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt);

/**
 * Main entry point for segment info process. This forks off a sender process
 * and calls SegmentInfoSenderMain(), which does all the setup.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
perfmon_segmentinfo_start(void)
{
	pid_t		segmentInfoId = -1;

	switch ((segmentInfoId = fork_process()))
	{
		case -1:
			ereport(LOG,
				(errmsg("could not fork stats sender process: %m")));
		return 0;

		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			SegmentInfoSenderMain(0, NULL);
			break;
		default:
			return (int)segmentInfoId;
	}

	/* shouldn't get here */
	Assert(false);
	return 0;
}


/**
 * This method is called after fork of the stats sender process. It sets up signal
 * handlers and does initialization that is required by a postgres backend.
 */
NON_EXEC_STATIC void SegmentInfoSenderMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;

	IsUnderPostmaster = true;
	isSenderProcess = true;

	/* Stay away from PMChildSlot */
	MyPMChildSlot = -1;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/* Identify myself via ps */
	init_ps_display("stats sender process", "", "", "");

	SetProcessingMode(InitProcessing);

	/* Set up signal handlers, see equivalent code in tcop/postgres.c. */
	pqsignal(SIGHUP, SIG_IGN);
	pqsignal(SIGINT, SIG_IGN);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN);

	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, die);
	pqsignal(SIGUSR2, SegmentInfoRequestShutdown);

	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/* Copied from bgwriter */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "Segment info sender process");

	/* Early initialization */
	BaseInit();

	/* See InitPostgres()... */
	InitProcess();

	SetProcessingMode(NormalProcessing);

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

	MyBackendId = InvalidBackendId;

	/* Init gpmon connection */
	gpmon_init();

	/* Create and initialize gpmon_pkt */
	InitSegmentInfoGpmonPkt(&seginfopkt);

	/* main loop */
	SegmentInfoSenderLoop();

	/* One iteration done, go away */
	proc_exit(0);
}

/**
 * Main loop of the sender process. It wakes up every
 * gp_perfmon_segment_interval ms to send segment
 * information to perfmon
 */
static void
SegmentInfoSenderLoop(void)
{

	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (senderShutdownRequested)
		{
			break;
		}

		/* no need to live on if postmaster has died */
		if (!PostmasterIsAlive(true))
			exit(1);

		SegmentInfoSender();

		/* Sleep a while. */
		Assert(gp_perfmon_segment_interval > 0);
		pg_usleep(gp_perfmon_segment_interval * 1000);
	} /* end server loop */

	return;
}

/**
 * Note the request to shut down.
 */
static void
SegmentInfoRequestShutdown(SIGNAL_ARGS)
{
	senderShutdownRequested = true;
}

/**
 * Sends a UDP packet to perfmon containing current segment statistics.
 */
static void
SegmentInfoSender()
{
	UpdateSegmentInfoGpmonPkt(&seginfopkt);
	gpmon_send(&seginfopkt);
}

/**
 * InitSegmentInfoGpmonPkt -- initialize the gpmon packet.
 */
static void
InitSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt)
{
	Assert(gpmon_pkt);
	memset(gpmon_pkt, 0, sizeof(gpmon_packet_t));

	gpmon_pkt->magic = GPMON_MAGIC;
	gpmon_pkt->version = GPMON_PACKET_VERSION;
	gpmon_pkt->pkttype = GPMON_PKTTYPE_SEGINFO;

	gpmon_pkt->u.seginfo.dbid = GpIdentity.dbid;
	UpdateSegmentInfoGpmonPkt(gpmon_pkt);
}

/**
 * UpdateSegmentInfoGpmonPkt -- update segment info
 */
static void
UpdateSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt)
{
	Assert(gpmon_pkt);
	Assert(GPMON_PKTTYPE_SEGINFO == gpmon_pkt->pkttype);

	uint64 mem_alloc_available = VmemTracker_GetAvailableVmemBytes();
	uint64 mem_alloc_limit = VmemTracker_GetVmemLimitBytes();
	gpmon_pkt->u.seginfo.dynamic_memory_used = mem_alloc_limit - mem_alloc_available;
	gpmon_pkt->u.seginfo.dynamic_memory_available =	mem_alloc_available;
}
