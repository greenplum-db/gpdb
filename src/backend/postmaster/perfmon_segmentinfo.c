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
#include "postgres.h"

#include <unistd.h>
#include <signal.h>

#include "postmaster/perfmon_segmentinfo.h"
#include "postmaster/bgworker.h"

#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/pmsignal.h"			/* PostmasterIsAlive */

#include "utils/metrics_utils.h"

#include "gpmon/gpmon.h"
#include "tcop/tcopprot.h"
#include "cdb/cdbvars.h"
#include "utils/vmem_tracker.h"

/* Sender-related routines */
static void SegmentInfoSender(void);
static void SegmentInfoSenderLoop(void);

/* Static gpmon_seginfo_t item, (re)used for sending UDP packets. */
static gpmon_packet_t seginfopkt;

/* GpmonPkt-related routines */
static void InitSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt);
static void UpdateSegmentInfoGpmonPkt(gpmon_packet_t *gpmon_pkt);

/*
 * cluster state collector hook
 * Use this hook to collect cluster wide state data periodically.
 */
cluster_state_collect_hook_type cluster_state_collect_hook = NULL;

/*
 * query info collector hook
 * Use this hook to collect real-time query information and status data.
 */
query_info_collect_hook_type query_info_collect_hook = NULL;

/**
 * This method is called after fork of the stats sender process. It sets up signal
 * handlers and does initialization that is required by a postgres backend.
 */
void SegmentInfoSenderMain(Datum main_arg)
{
	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Init gpmon connection */
	gpmon_init();

	/* Create and initialize gpmon_pkt */
	InitSegmentInfoGpmonPkt(&seginfopkt);

	/* main loop */
	SegmentInfoSenderLoop();

	/* One iteration done, go away */
	proc_exit(0);
}

bool
SegmentInfoSenderStartRule(Datum main_arg)
{
	if (!gp_enable_gpperfmon)
		return false;

	/* FIXME: even for the utility mode? */
	return true;
}

/**
 * Main loop of the sender process. It wakes up every
 * gp_perfmon_segment_interval ms to send segment
 * information to perfmon
 */
static void
SegmentInfoSenderLoop(void)
{
	int rc;
	int counter;

	for (counter = 0;; counter += SEGMENT_INFO_LOOP_SLEEP_MS)
	{
		if (cluster_state_collect_hook)
			cluster_state_collect_hook();

		if (gp_enable_gpperfmon && counter >= gp_perfmon_segment_interval)
		{
			SegmentInfoSender();
			counter = 0;
		}

		/* Sleep a while. */
		rc = WaitLatch(&MyProc->procLatch,
				WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
				SEGMENT_INFO_LOOP_SLEEP_MS);
		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);
	} /* end server loop */

	return;
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
