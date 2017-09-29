
/*-------------------------------------------------------------------------
 *
 * cdbgang_async.c
 *	  Functions for asynchronous implementation of creating gang.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbgang_async.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <limits.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "storage/ipc.h"		/* For proc_exit_inprogress  */
#include "tcop/tcopprot.h"
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/resowner.h"

static int	getPollTimeout(const struct timeval *startTS);
static Gang *createGang_async(GangType type, int gang_id, int size, int content);

CreateGangFunc pCreateGangFuncAsync = createGang_async;

/*
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
static Gang *
createGang_async(GangType type, int gang_id, int size, int content)
{
	Gang	   *newGangDefinition;
	SegmentDatabaseDescriptor *segdbDesc = NULL;
	int			i = 0;
	int			create_gang_retry_counter = 0;
	int			in_recovery_mode_count = 0;
	int			successful_connections = 0;
	bool		retry = false;
	int			poll_timeout = 0;
	struct timeval startTS;
	PostgresPollingStatusType *pollingStatus = NULL;

	/*
	 * true means connection status is confirmed, either established or in
	 * recovery mode
	 */
	bool	   *connStatusDone = NULL;

	ELOG_DISPATCHER_DEBUG("createGang type = %d, gang_id = %d, size = %d, content = %d",
						  type, gang_id, size, content);

	/* check arguments */
	Assert(size == 1 || size == getgpsegmentCount());
	Assert(CurrentResourceOwner != NULL);
	Assert(CurrentMemoryContext == GangContext);
	/* Writer gang is created before reader gangs. */
	if (type == GANGTYPE_PRIMARY_WRITER)
		Insist(!GangsExist());

	Assert(CurrentGangCreating == NULL);

create_gang_retry:
	/* If we're in a retry, we may need to reset our initial state, a bit */
	newGangDefinition = NULL;
	successful_connections = 0;
	in_recovery_mode_count = 0;
	retry = false;

	/* allocate and initialize a gang structure */
	newGangDefinition = buildGangDefinition(type, gang_id, size, content);
	CurrentGangCreating = newGangDefinition;

	Assert(newGangDefinition != NULL);
	Assert(newGangDefinition->size == size);
	Assert(newGangDefinition->perGangContext != NULL);
	MemoryContextSwitchTo(newGangDefinition->perGangContext);

	/*
	 * allocate memory within perGangContext and will be freed automatically
	 * when gang is destroyed
	 */
	pollingStatus = palloc(sizeof(PostgresPollingStatusType) * size);
	connStatusDone = palloc(sizeof(bool) * size);

	struct pollfd *fds;

	PG_TRY();
	{
		for (i = 0; i < size; i++)
		{
			char		gpqeid[100];
			char	   *options;

			/*
			 * Create the connection requests.	If we find a segment without a
			 * valid segdb we error out.  Also, if this segdb is invalid, we
			 * must fail the connection.
			 */
			segdbDesc = &newGangDefinition->db_descriptors[i];

			/*
			 * Build the connection string.  Writer-ness needs to be processed
			 * early enough now some locks are taken before command line
			 * options are recognized.
			 */
			build_gpqeid_param(gpqeid, sizeof(gpqeid),
							   segdbDesc->segindex,
							   type == GANGTYPE_PRIMARY_WRITER,
							   gang_id,
							   segdbDesc->segment_database_info->hostSegs);

			options = makeOptions();

			/* start connection in asynchronous way */
			cdbconn_doConnectStart(segdbDesc, gpqeid, options);

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
		fds = (struct pollfd *) palloc0(sizeof(struct pollfd) * size);

		for (;;)
		{
			int			nready;
			int			nfds = 0;

			poll_timeout = getPollTimeout(&startTS);

			for (i = 0; i < size; i++)
			{
				segdbDesc = &newGangDefinition->db_descriptors[i];

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
						continue;

					case PGRES_POLLING_READING:
						fds[nfds].fd = PQsocket(segdbDesc->conn);
						fds[nfds].events = POLLIN;
						nfds++;
						break;

					case PGRES_POLLING_WRITING:
						fds[nfds].fd = PQsocket(segdbDesc->conn);
						fds[nfds].events = POLLOUT;
						nfds++;
						break;

					case PGRES_POLLING_FAILED:
						if (segment_failure_due_to_recovery(PQerrorMessage(segdbDesc->conn)))
						{
							in_recovery_mode_count++;
							connStatusDone[i] = true;
							elog(LOG, "segment is in recovery mode (%s)", segdbDesc->whoami);
						}
						else
						{
							ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
											errmsg("failed to acquire resources on one or more segments"),
											errdetail("%s (%s)", PQerrorMessage(segdbDesc->conn), segdbDesc->whoami)));
						}
						break;

					default:
						ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
										errmsg("failed to acquire resources on one or more segments"),
										errdetail("unknow pollstatus (%s)", segdbDesc->whoami)));
						break;
				}

				if (poll_timeout == 0)
					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("failed to acquire resources on one or more segments"),
									errdetail("timeout expired\n (%s)", segdbDesc->whoami)));
			}

			if (nfds == 0)
				break;

			CHECK_FOR_INTERRUPTS();

			/* Wait until something happens */
			nready = poll(fds, nfds, poll_timeout);

			if (nready < 0)
			{
				int			sock_errno = SOCK_ERRNO;

				if (sock_errno == EINTR)
					continue;

				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("failed to acquire resources on one or more segments"),
								errdetail("poll() failed: errno = %d", sock_errno)));
			}
			else if (nready > 0)
			{
				int			currentFdNumber = 0;

				for (i = 0; i < size; i++)
				{
					segdbDesc = &newGangDefinition->db_descriptors[i];
					if (connStatusDone[i])
						continue;

					Assert(PQsocket(segdbDesc->conn) > 0);
					Assert(PQsocket(segdbDesc->conn) == fds[currentFdNumber].fd);

					if (fds[currentFdNumber].revents & fds[currentFdNumber].events ||
						fds[currentFdNumber].revents & (POLLERR | POLLHUP | POLLNVAL))
						pollingStatus[i] = PQconnectPoll(segdbDesc->conn);

					currentFdNumber++;

				}
			}
		}

		ELOG_DISPATCHER_DEBUG("createGang: %d processes requested; %d successful connections %d in recovery",
							  size, successful_connections, in_recovery_mode_count);

		MemoryContextSwitchTo(GangContext);

		/* some segments are in recovery mode */
		if (successful_connections != size)
		{
			Assert(successful_connections + in_recovery_mode_count == size);

			if (gp_gang_creation_retry_count <= 0 ||
				create_gang_retry_counter++ >= gp_gang_creation_retry_count ||
				type != GANGTYPE_PRIMARY_WRITER)
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("failed to acquire resources on one or more segments"),
								errdetail("segments is in recovery mode")));

			ELOG_DISPATCHER_DEBUG("createGang: gang creation failed, but retryable.");

			DisconnectAndDestroyGang(newGangDefinition);
			newGangDefinition = NULL;
			CurrentGangCreating = NULL;
			retry = true;
		}
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(GangContext);

		/* FTS shows some segment DBs are down */
		if (isFTSEnabled() &&
			FtsTestSegmentDBIsDown(newGangDefinition->db_descriptors, size))
		{

			DisconnectAndDestroyGang(newGangDefinition);
			newGangDefinition = NULL;
			CurrentGangCreating = NULL;
			DisconnectAndDestroyAllGangs(true);
			CheckForResetSession();
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("failed to acquire resources on one or more segments"),
							errdetail("FTS detected one or more segments are down")));

		}

		DisconnectAndDestroyGang(newGangDefinition);
		newGangDefinition = NULL;
		CurrentGangCreating = NULL;

		if (type == GANGTYPE_PRIMARY_WRITER)
		{
			DisconnectAndDestroyAllGangs(true);
			CheckForResetSession();
		}

		PG_RE_THROW();
	}
	PG_END_TRY();

	SIMPLE_FAULT_INJECTOR(GangCreated);

	if (retry)
	{
		CHECK_FOR_INTERRUPTS();
		pg_usleep(gp_gang_creation_retry_timer * 1000);
		CHECK_FOR_INTERRUPTS();

		goto create_gang_retry;
	}

	setLargestGangsize(size);

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
