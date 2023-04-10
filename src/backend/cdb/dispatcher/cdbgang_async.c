/*-------------------------------------------------------------------------
 *
 * cdbgang_async.c
 *	  Functions for asynchronous implementation of creating gang.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbgang_async.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "access/xact.h"
#include "storage/ipc.h"		/* For proc_exit_inprogress  */
#include "pgstat.h"
#include "tcop/tcopprot.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbgang_async.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"

static int	getPollTimeout(const struct timeval *startTS);

/*
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
Gang *
cdbgang_createGang_async(List *segments, SegmentType segmentType)
{
	PostgresPollingStatusType	*pollingStatus = NULL;
	SegmentDatabaseDescriptor	*segdbDesc = NULL;
	struct timeval	startTS;
	Gang	*newGangDefinition;
	int		create_gang_retry_counter = 0;
	int		in_recovery_mode_count = 0;
	int		successful_connections = 0;
	int		poll_timeout = 0;
	int		i = 0;
	int		size = 0;
	int		totalSegs = 0;
	bool	allStatusDone = true;
	bool	retry = false;

	/* the returned events of waiteventset */
	WaitEvent		*revents = NULL;
	/* true means connection status is confirmed, either established or in recovery mode */
	bool		*connStatusDone = NULL;

	size = list_length(segments);

	ELOG_DISPATCHER_DEBUG("createGang size = %d, segment type = %d", size, segmentType);
	Assert(CurrentGangCreating == NULL);

	/* If we're in a retry, we may need to reset our initial state, a bit */
	newGangDefinition = NULL;
	/* allocate and initialize a gang structure */
	newGangDefinition = buildGangDefinition(segments, segmentType);
	CurrentGangCreating = newGangDefinition;
	/*
	 * If we're in a global transaction, and there is some segment configuration change,
	 * we have to error out so that the current global transaction can be aborted.
	 * This is because within a transaction we use cached version of configuration information 
	 * obtained at start of transaction, which we can't update in-middle of transaction.
	 * so QD will still talk to the old primary but not a new promoted one. This isn't an issue 
	 * if the old primary is completely down since we'll find a FATAL error during communication,
	 * but becomes an issue if the old primary is working and acting like normal to QD.
	 * 
	 * Before error out, we need to reset the session instead of disconnectAndDestroyAllGangs.
	 * The latter will drop CdbComponentsContext what we will use in AtAbort_Portals.
	 * Because some primary segment is down writerGangLost will be marked when recycling gangs,
	 * All Gangs will be destroyed by ResetAllGangs in AtAbort_DispatcherState.
	 *
	 * We shouldn't error out in transaction abort state to avoid recursive abort.
	 * In such case, the dispatcher would catch the error and then dtm does (retry)
	 * abort.
	 */
	if (IsTransactionState())
	{
		for (i = 0; i < size; i++)
		{
			if (FtsIsSegmentDown(newGangDefinition->db_descriptors[i]->segment_database_info))
			{
				resetSessionForPrimaryGangLoss();
				elog(ERROR, "gang was lost due to cluster reconfiguration");
			}
		}
	}
	totalSegs = getgpsegmentCount();
	Assert(totalSegs > 0);

create_gang_retry:
	Assert(newGangDefinition != NULL);
	Assert(newGangDefinition->size == size);
	successful_connections = 0;
	in_recovery_mode_count = 0;
	retry = false;

	pollingStatus = palloc(sizeof(PostgresPollingStatusType) * size);
	connStatusDone = palloc(sizeof(bool) * size);
	revents = palloc(sizeof(WaitEvent) * size);

	PG_TRY();
	{
		for (i = 0; i < size; i++)
		{
			bool		ret;
			char		gpqeid[100];
			char	   *options = NULL;
			char	   *diff_options = NULL;

			/*
			 * Create the connection requests.	If we find a segment without a
			 * valid segdb we error out.  Also, if this segdb is invalid, we
			 * must fail the connection.
			 */
			segdbDesc = newGangDefinition->db_descriptors[i];

			/* if it's a cached QE, skip */
			if (segdbDesc->conn != NULL && !cdbconn_isBadConnection(segdbDesc))
			{
				connStatusDone[i] = true;
				/* -1 means this connection is cached */
				segdbDesc->establishConnTime = -1;
				successful_connections++;
				continue;
			}

			/*
			 * Build the connection string.  Writer-ness needs to be processed
			 * early enough now some locks are taken before command line
			 * options are recognized.
			 */
			ret = build_gpqeid_param(gpqeid, sizeof(gpqeid),
									 segdbDesc->isWriter,
									 segdbDesc->identifier,
									 segdbDesc->segment_database_info->hostPrimaryCount,
									 totalSegs * 2);

			if (!ret)
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("failed to construct connectionstring")));

			makeOptions(&options, &diff_options);

			/* start connection in asynchronous way */
			cdbconn_doConnectStart(segdbDesc, gpqeid, options, diff_options);

			if (cdbconn_isBadConnection(segdbDesc))
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("failed to acquire resources on one or more segments"),
								errdetail("%s (%s)", PQerrorMessage(segdbDesc->conn), segdbDesc->whoami)));

			connStatusDone[i] = false;

			/*
			 * If connection status is not CONNECTION_BAD after
			 * PQconnectStart(), we must act as if the PQconnectPoll() had
			 * returned PGRES_POLLING_WRITING
			 */
			pollingStatus[i] = PGRES_POLLING_WRITING;
		}

		/*
		 * Ok, we've now launched all the connection attempts. Start the
		 * timeout clock (= get the start timestamp), and poll until they're
		 * all completed or we reach timeout.
		 */
		gettimeofday(&startTS, NULL);

		instr_time              starttime, endtime;
		INSTR_TIME_SET_CURRENT(starttime); /* record starttime of create gang */
		for (;;)
		{
			poll_timeout = getPollTimeout(&startTS);
			if (poll_timeout == 0)
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("failed to acquire resources on one or more segments"),
								errdetail("timeout expired\n (%s)", segdbDesc->whoami)));

			/*
			 * Since the set of FDs can change when we call PQconnectPoll() below,
			 * we must init WaitEventSet to poll on for every loop iteration.
			 */
			ResetWaitEventSet(&DispWaitSet, TopMemoryContext, size);

			for (i = 0; i < size; i++)
			{
				segdbDesc = newGangDefinition->db_descriptors[i];
				int fd = PQsocket(segdbDesc->conn);

				ELOG_DISPATCHER_DEBUG("event pollingStatus, i:%d fd:%d conn-status:%d polling-status:%d",
					i, fd, connStatusDone[i], pollingStatus[i]);

				/*
				 * Skip established connections and in-recovery-mode
				 * connections
				 */
				if (connStatusDone[i])
					continue;

				switch (pollingStatus[i])
				{
					case PGRES_POLLING_OK:
						cdbconn_doConnectComplete(segdbDesc);
						if (segdbDesc->motionListener == 0)
							ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
											errmsg("failed to acquire resources on one or more segments"),
											errdetail("Internal error: No motion listener port (%s)", segdbDesc->whoami)));
						successful_connections++;
						connStatusDone[i] = true;
						/* the connection of segdbDesc is established successfully, calculate the time of establishConnTime */
						INSTR_TIME_SET_CURRENT(endtime);
						INSTR_TIME_SUBTRACT(endtime, starttime);
						segdbDesc->establishConnTime = INSTR_TIME_GET_MILLISEC(endtime);
						continue;

					case PGRES_POLLING_READING:
						AddWaitEventToSet(DispWaitSet, WL_SOCKET_READABLE, fd, NULL,
							(void *)(long)i); /* "i" as the event's userdata */

						ELOG_DISPATCHER_DEBUG("added readable event into waitset, i:%d fd:%d", i, fd);
						break;

					case PGRES_POLLING_WRITING:
						AddWaitEventToSet(DispWaitSet, WL_SOCKET_WRITEABLE, fd, NULL,
							(void *)(long)i); /* "i" as the event's userdata */

						ELOG_DISPATCHER_DEBUG("added writable event into waitset, i:%d fd:%d", i, fd);
						break;

					case PGRES_POLLING_FAILED:
						if (segment_failure_due_to_recovery(PQerrorMessage(segdbDesc->conn)))
						{
							in_recovery_mode_count++;
							connStatusDone[i] = true;
							elog(LOG, "segment is in reset/recovery mode (%s)", segdbDesc->whoami);
						}
						else
						{
							if (segment_failure_due_to_missing_writer(PQerrorMessage(segdbDesc->conn)))
								markCurrentGxactWriterGangLost();
							ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
											errmsg("failed to acquire resources on one or more segments"),
											errdetail("%s (%s)", PQerrorMessage(segdbDesc->conn), segdbDesc->whoami)));
						}
						break;

					default:
						ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
										errmsg("failed to acquire resources on one or more segments"),
										errdetail("unknown pollstatus (%s)", segdbDesc->whoami)));
						break;
				}
			}

			allStatusDone = true;
			for (i = 0; i < size; i++)
				allStatusDone &= connStatusDone[i];
			if (allStatusDone)
			{
				break;
			}

			SIMPLE_FAULT_INJECTOR("create_gang_in_progress");

			CHECK_FOR_INTERRUPTS();

			/* Wait until something happens */
			int nready = WaitEventSetWait(DispWaitSet, poll_timeout, revents, size, WAIT_EVENT_GANG_ASSIGN);
			Assert(nready >= 0);

			if (nready == 0)
			{
				ELOG_DISPATCHER_DEBUG("cdbgang_createGang_async(): WaitEventSetWait timeout after %d ms", poll_timeout);
			}
			else if (nready > 0)
			{
				for (i = 0; i < nready; i++)
				{
					/* original position in the db_descriptors */
					long pos = (long)(revents[i].user_data);
					if (connStatusDone[pos])
						continue;

					segdbDesc = newGangDefinition->db_descriptors[pos];
					int fd_desc PG_USED_FOR_ASSERTS_ONLY = PQsocket(segdbDesc->conn);
					Assert(fd_desc > 0);
					Assert(fd_desc == revents[i].fd);

					ELOG_DISPATCHER_DEBUG("ready event[%d] pos:%ld fd:%d event:%d",
						i, pos, revents[i].fd, revents[i].events);

					if (revents[i].events & WL_SOCKET_WRITEABLE ||
						revents[i].events & WL_SOCKET_READABLE)
						/*
						 * The official documentation says:
						 * Caution: do not assume that the socket remains the same across PQconnectPoll calls.
						 *
						 * So must add all sock FDs to waiteventset again in the next loop.
						 */
						pollingStatus[pos] = PQconnectPoll(segdbDesc->conn);
				}
			}
		}

		ELOG_DISPATCHER_DEBUG("createGang: %d processes requested; %d successful connections %d in recovery",
							  size, successful_connections, in_recovery_mode_count);

		/* some segments are in reset/recovery mode */
		if (successful_connections != size)
		{
			Assert(successful_connections + in_recovery_mode_count == size);

			if (gp_gang_creation_retry_count <= 0 ||
				create_gang_retry_counter++ >= gp_gang_creation_retry_count)
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("failed to acquire resources on one or more segments"),
								errdetail("Segments are in reset/recovery mode.")));

			ELOG_DISPATCHER_DEBUG("createGang: gang creation failed, but retryable.");

			retry = true;
		} /* for(;;) */
	}
	PG_CATCH();
	{
		FtsNotifyProber();
		/* FTS shows some segment DBs are down */
		if (FtsTestSegmentDBIsDown(newGangDefinition->db_descriptors, size))
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("failed to acquire resources on one or more segments"),
							errdetail("FTS detected one or more segments are down")));

		}

		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(pollingStatus);
	pfree(connStatusDone);
	pfree(revents);

	SIMPLE_FAULT_INJECTOR("gang_created");

	if (retry)
	{
		CHECK_FOR_INTERRUPTS();
		pg_usleep(gp_gang_creation_retry_timer * 1000);
		CHECK_FOR_INTERRUPTS();

		goto create_gang_retry;
	}

	CurrentGangCreating = NULL;

	return newGangDefinition;
}

static int
getPollTimeout(const struct timeval *startTS)
{
	struct timeval now;
	int			timeout = 0;
	int64		diff_us;

	gettimeofday(&now, NULL);

	if (gp_segment_connect_timeout > 0)
	{
		diff_us = (now.tv_sec - startTS->tv_sec) * 1000000;
		diff_us += (int) now.tv_usec - (int) startTS->tv_usec;
		if (diff_us >= (int64) gp_segment_connect_timeout * 1000000)
			timeout = 0;
		else
			timeout = gp_segment_connect_timeout * 1000 - diff_us / 1000;
	}
	else
		/* wait forever */
		timeout = -1;

	return timeout;
}
