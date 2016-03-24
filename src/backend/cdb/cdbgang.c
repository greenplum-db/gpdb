/*-------------------------------------------------------------------------
 *
 * cdbgang.c
 *	  Query Executor Factory for gangs of QEs
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>				/* getpid() */
#include <limits.h>
#include <poll.h>

#include "gp-libpq-fe.h"
#include "miscadmin.h"			/* MyDatabaseId */
#include "storage/proc.h"		/* MyProc */
#include "storage/ipc.h"
#include "utils/memutils.h"

#include "catalog/namespace.h"
#include "commands/variable.h"
#include "nodes/execnodes.h"	/* CdbProcess, Slice, SliceTable */
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "utils/portal.h"
#include "tcop/pquery.h"

extern int	CommitDelay;
extern int	CommitSiblings;
extern char *default_tablespace;

#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbgang.h"		/* me */
#include "cdb/cdbtm.h"			/* discardDtxTransaction() */
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"		/* Gp_role, etc. */
#include "storage/bfz.h"
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"

#include "utils/guc_tables.h"


#define MAX_CACHED_1_GANGS 1

static char *build_gpqeid_param(bool is_writer);

static Gang *createGang(GangType type, int gang_id, int size, int content, char *portal_name);

static void disconnectAndDestroyGang(Gang *gp);
static void disconnectAndDestroyAllReaderGangs(bool destroyAllocated);

static Gang *buildGangDefinition(GangType type, int gang_id, int size, int content, char *portal_name);

static bool isTargetPortal(const char *p1, const char *p2);

static char *build_options(bool iswriter, int segindex, bool i_am_superuser);

static bool cleanupGang(Gang * gp);

extern void resetSessionForPrimaryGangLoss(void);

static bool cdbgang_shouldRetryConnection(SegmentDatabaseDescriptor *segdbDesc, int retries);

/*
 * Points to the result of getCdbComponentDatabases()
 */
static CdbComponentDatabases *cdb_component_dbs = NULL;
static bool mirroringNotConfigured = false;

static int	largest_gangsize = 0;

static MemoryContext GangContext = NULL;

int
largestGangsize(void)
{
	return largest_gangsize;
}

static bool
segment_failure_due_to_recovery(SegmentDatabaseDescriptor *segdbDesc)
{
	char	   *fatal;
	char	   *message;
	char	   *ptr;
	int			fatal_len;

	if (segdbDesc == NULL)
		return false;

	message = PQerrorMessage(segdbDesc->conn);

	if (message == NULL)
		return false;

	fatal = _("FATAL");
	if (fatal == NULL)
		return false;

	fatal_len = strlen(fatal);

	/*
	 * it would be nice if we could check errcode for ERRCODE_CANNOT_CONNECT_NOW, instead
	 * we wind up looking for at the strings.
	 *
	 * And because if LC_MESSAGES gets set to something which changes
	 * the strings a lot we have to take extreme care with looking at
	 * the string.
	 */
	ptr = strstr(message, fatal);
	if ((ptr != NULL) && ptr[fatal_len] == ':')
	{
		if (strstr(message, _(POSTMASTER_IN_STARTUP_MSG)))
		{
			return true;
		}
		if (strstr(message, _(POSTMASTER_IN_RECOVERY_MSG)))
		{
			return true;
		}
		/* We could do retries for "sorry, too many clients already" here too */
	}

	return false;
}

/*
 * Every gang created must have a unique identifier, so the QD and Dispatch Agents can agree
 * about what they are talking about.
 *
 * Since there can only be one primary writer gang, and only one mirror writer gang, we
 * use id 1 and 2 for those two (helps in debugging).
 *
 * Reader gang ids start at 3
 */
#define PRIMARY_WRITER_GANG_ID 1
static int	gang_id_counter = 2;

/*
 * creates a new gang by logging on a session to each segDB involved
 *
 */
static Gang *
createGang(GangType type, int gang_id, int size, int content, char *portal_name)
{
	Gang	   *newGangDefinition;
	MemoryContext oldContext;
	int			successful_connections;

	SegmentDatabaseDescriptor *segdbDesc;
	CdbComponentDatabaseInfo *q;
	int			segdb_count;
	int			i;
	Portal		portal;
	bool		connectAsSuperUser;

	int			create_gang_retry_counter = 0;
	int			in_recovery_mode_count;

	/* If we're in a retry, we may need to reset our initial state, a bit */
create_gang_retry:
	portal = NULL;
	newGangDefinition = NULL;
	successful_connections = 0;
	connectAsSuperUser = false;
	in_recovery_mode_count = 0;

	Assert(size == 1 || size == getgpsegmentCount());

	/*
	 * We can't allocate more segments than we know about
	 */
	Assert(size <= getgpsegmentCount());

	Assert(CurrentResourceOwner != NULL);

	if (portal_name != 0)
	{
		/* Using a named portal */
		portal = GetPortalByName(portal_name);
		Assert(portal);
	}

	connectAsSuperUser = superuser_arg(MyProc->roleId);

	/*
	 * MemoryContexts used in this routine.
	 */
	if (GangContext == NULL)
	{
		GangContext = AllocSetContextCreate(TopMemoryContext,
											"Gang Context",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);
	}

	oldContext = MemoryContextSwitchTo(GangContext);

	newGangDefinition = buildGangDefinition(type, gang_id, size, content, portal_name);

	Assert(newGangDefinition != NULL);

	/*
	 * Loop through the segment_database_descriptors items inside newGangDefinition.
	 * For each segdb , attempt to get
	 * a connection to the Postgres instance for that database.
	 * If the connection fails, mark it (locally) as unavailable
	 * If no database serving a segindex is available, fail the user's connection request.
	 */
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "createGang type = %d, gang_id %d, size %d", type, gang_id, size);
	}
	if (type == GANGTYPE_PRIMARY_WRITER)
		Assert(gang_id == PRIMARY_WRITER_GANG_ID);

	segdb_count = newGangDefinition->size;

	Assert(segdb_count > 0);

	/*
	 * We allocate enough memory for this many DoConnectParms structures,
	 */
	if (size > largest_gangsize)
		largest_gangsize = size;

	newGangDefinition->active = true;

	/*
	 * In this loop, we check each segdb.  If it's marked as valid, and is not
	 * ourself, we prepare a thread Parm struct for connecting to it.
	 * If a segment has no valid segdbs, we recognize that here and give
	 * an error message.
	 *
	 * If any error happens, we'll close any already-opened connections in
	 * the CATCH-block.
	 */
	in_recovery_mode_count = 0;
	PG_TRY();
	{
		struct pollfd *fds;
		int		   *indexes;
		struct timeval start_ts;
		struct timeval now;
		bool		failed;

		for (i = 0; i < segdb_count; i++)
		{
			char	   *gpqeid;
			char	   *options;

			/*
			 * Create the connection requests.	If we find a segment without a
			 * valid segdb we error out.  Also, if this segdb is invalid, we must
			 * fail the connection.
			 */
			segdbDesc = &newGangDefinition->db_descriptors[i];

			if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			{
				elog(LOG, "createGang: starting connection, segment %d/%d descriptor %p (gangdef size %d)",
					 i+1, segdb_count, segdbDesc, newGangDefinition->size);
			}

			q = segdbDesc->segment_database_info;

			/*
			 * Build the connection string.  Writer-ness needs to be processed
			 * early enough now some locks are taken before command line
			 * options are recognized.
			 */
			gpqeid = build_gpqeid_param(type == GANGTYPE_PRIMARY_WRITER);
			options = build_options((type == GANGTYPE_PRIMARY_WRITER),
									q->segindex, connectAsSuperUser);

			cdbconn_doConnectStart(segdbDesc, gpqeid, options);
			segdbDesc->poll_status = PGRES_POLLING_WRITING;

			if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
			{
				if (cdbgang_shouldRetryConnection(segdbDesc, create_gang_retry_counter))
				{
					in_recovery_mode_count++;
					break;
				}
				else
				{
					/*
					 * Log the details to the server log, but give a more
					 * generic error to the client. XXX: The user would
					 * probably prefer to see a bit more details too.
					 */
					ereport(LOG,
							(errcode(ERRCODE_GP_INTERNAL_ERROR),
							 errmsg("Master unable to connect to %s with options %s: %s",
									segdbDesc->whoami,
									options,
									PQerrorMessage(segdbDesc->conn))));

					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("failed to acquire resources on one or more segments")));
				}
			}
		}

		if (in_recovery_mode_count > 0)
			break;

		/*
		 * Ok, we've now launched all the connection attempts. Start the
		 * timeout clock (= get the start timestamp), and poll until they're
		 * all completed or we reach timeout.
		 */
		gettimeofday(&start_ts, NULL);
		fds = (struct pollfd *) palloc0(sizeof(struct pollfd) * segdb_count);
		indexes = (int *) palloc(sizeof(int) * segdb_count);

		failed = false;
		while (!failed)
		{
			int			nfds = 0;
			int			nready;
			int			timeout;

			for (i = 0; i < segdb_count; i++)
			{
				segdbDesc = &newGangDefinition->db_descriptors[i];

				if (segdbDesc->conn &&
					(segdbDesc->poll_status == PGRES_POLLING_READING ||
					 segdbDesc->poll_status == PGRES_POLLING_WRITING))
				{
					indexes[nfds] = i;
					fds[nfds].fd = PQsocket(segdbDesc->conn);
					fds[nfds].events = 0;
					if (segdbDesc->poll_status == PGRES_POLLING_READING)
						fds[nfds].events |= POLLIN;
					if (segdbDesc->poll_status == PGRES_POLLING_WRITING)
						fds[nfds].events |= POLLOUT;
					nfds++;
				}
			}

			if (nfds == 0)
			{
				/* all done! */
				elog(LOG, "All connections established for gang");
				break;
			}

			if (gp_segment_connect_timeout > 0)
			{
				int64 diff_us;

				gettimeofday(&now, NULL);

				diff_us = (now.tv_sec - start_ts.tv_sec) * 1000000;
				diff_us += (int) now.tv_usec - (int) start_ts.tv_usec;

				if (diff_us > (int64) gp_segment_connect_timeout * 1000000)
					timeout = 0;
				else
				{
					timeout = gp_segment_connect_timeout * 1000 -
						diff_us / 1000;
				}
			}
			else
				timeout = -1;

			/* Wait until something happens */
			nready = poll(fds, nfds, timeout);

			if (nready < 0)
				ereport(ERROR,
						(errcode_for_socket_access(),
						 errmsg("poll() failed while connecting to segments")));

			if (nready == 0 && timeout == 0)
			{
				/* Timeout reached */
				elog(LOG, "timeout reached!");
				break;
			}

			for (i = 0; i < nfds; i++)
			{
				if (fds[i].revents == 0)
					continue;

				segdbDesc = &newGangDefinition->db_descriptors[indexes[i]];

				segdbDesc->poll_status = PQconnectPoll(segdbDesc->conn);

				if (segdbDesc->poll_status == PGRES_POLLING_OK)
				{
					cdbconn_doConnectComplete(segdbDesc);

					if (segdbDesc->motionListener == -1)
					{
						ereport(ERROR,
								(errcode(ERRCODE_GP_INTERNAL_ERROR),
								 errmsg("Internal error: No motion listener port for %s\n",
										segdbDesc->whoami)));
					}
					if (gp_log_gang >= GPVARS_VERBOSITY_VERBOSE)
						elog(LOG, "Connected to %s motionListenerPorts=%d/%d with options %s",
							 segdbDesc->whoami,
							 (segdbDesc->motionListener & 0x0ffff),
							 ((segdbDesc->motionListener >> 16) & 0x0ffff),
							 PQoptions(segdbDesc->conn));
				}
				else if (segdbDesc->poll_status == PGRES_POLLING_FAILED)
				{
					/*
					 * Connection failed, but before we error out, see if this
					 * is the kind of failure that we'd like to retry.
					 */
					if (cdbgang_shouldRetryConnection(segdbDesc, create_gang_retry_counter))
					{
						in_recovery_mode_count++;
						break;
					}
					else
					{
						/*
						 * Log the details to the server log, but give a more
						 * generic error to the client. XXX: The user would
						 * probably prefer to see a bit more details too.
						 */
						ereport(LOG,
								(errcode(ERRCODE_GP_INTERNAL_ERROR),
								 errmsg("Master unable to connect to %s with options %s: %s",
										segdbDesc->whoami,
										PQoptions(segdbDesc->conn),
										PQerrorMessage(segdbDesc->conn))));

						ereport(ERROR,
								(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								 errmsg("failed to acquire resources on one or more segments")));
					}
				}
			}
		}
	}
	PG_CATCH();
	{
		/*
		 * Non-retryable error while connecting. Disconnect all the
		 * connections already started, before propagating the error.
		 */
		int			j;

		for (j = 0; j < segdb_count; j++)
		{
			segdbDesc = &newGangDefinition->db_descriptors[i];
			if (segdbDesc->conn)
			{
				PQfinish(segdbDesc->conn);
				segdbDesc->conn = NULL;
			}
		}
		/* clean the gang before error out */
		disconnectAndDestroyGang(newGangDefinition);

		CheckForResetSession();

		PG_RE_THROW();
	}
	PG_END_TRY();

	newGangDefinition->active = false;

	/*
	 * If a connection failed in a way that we want to retry rather
	 * than error out, close all the connections, sleep, and retry.
	 */
	if (in_recovery_mode_count > 0)
	{
		int			j;

		for (j = 0; j < i; j++)
		{
			segdbDesc = &newGangDefinition->db_descriptors[i];
			if (segdbDesc->conn)
				PQfinish(segdbDesc->conn);
		}

		/*
		 * On the first retry, we want to verify that we are
		 * using the most current version of the
		 * configuration.
		 */
		if (create_gang_retry_counter == 0)
			FtsNotifyProber();

		/*
		 * MPP-10751: In case we're destroying a writer-gang, we don't want
		 * to cause a session-reset (and odd side-effect inside
		 * disconnectAndDestroyGang()) so let's pretend that this is a
		 * reader-gang.
		 */
		if (newGangDefinition->type == GANGTYPE_PRIMARY_WRITER)
		{
			disconnectAndDestroyAllReaderGangs(true);
			newGangDefinition->type = GANGTYPE_PRIMARY_READER;
		}

		disconnectAndDestroyGang(newGangDefinition); /* free up connections */
		newGangDefinition = NULL;

		MemoryContextSwitchTo(oldContext);

		CHECK_FOR_INTERRUPTS();

		pg_usleep(gp_gang_creation_retry_timer * 1000);

		CHECK_FOR_INTERRUPTS();

		create_gang_retry_counter++;
		goto create_gang_retry;
	}

	/*
	 * we only want to "just error out" if we don't have
	 * fts-enabled: for the fts-related response see further below.
	 */
	if (successful_connections != newGangDefinition->size && !isFTSEnabled())
	{
		for (i = 0; i < segdb_count; i++)
		{
			segdbDesc = &newGangDefinition->db_descriptors[i];

			if (segdbDesc->conn != NULL)
				continue;

			/* clean the gang before error out */
			disconnectAndDestroyGang(newGangDefinition);

			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("failed to acquire resources on one or more segments")));
		}
	}

	MemoryContextSwitchTo(oldContext);

	return newGangDefinition;
}

static bool
cdbgang_shouldRetryConnection(SegmentDatabaseDescriptor *segdbDesc, int retries)
{
	/* If we have exceeded the number of retries, we're finished */
	if (retries >= gp_gang_creation_retry_count)
		return false;

	/* Retry if the segment was still starting up */
	if (segment_failure_due_to_recovery(segdbDesc))
		return true;

	/* Other kinds of errors are only retried if FTS is enabled. */
	if (!isFTSEnabled())
		return false;

	/*
	 * If FTS thinks that the segment is OK, but we got an error,
	 * we must've seen some kind of transient failure.
	 */
	if (FtsTestConnection(segdbDesc->segment_database_info, false))
	{
		elog(DEBUG2, "no found fault for segment dbid %d",
			 segdbDesc->segment_database_info->dbid);
		return false;
	}

	/*
	 * KLUDGE: Do not error out if we are attempting a DTM protocol retry
	 */
	if (DistributedTransactionContext == DTX_CONTEXT_QD_RETRY_PHASE_2)
		return false;

	/*
	 * Is there a transaction active ?
	 *
	 * When the error is raised, it will abort the current DTM transaction
	 */
	if (isCurrentDtxActive())
	{
		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "cdbgang_shouldRetryConnection found an active DTM transaction, retrying.");
		return true;
	}

	/*
	 * error out if this sets read only flag, at this stage the read only
	 * transaction checking has passed, so error out, but do not error out if
	 * tm is in recovery
	 */
	if ((isFtsReadOnlySet() && !isTMInRecovery()))
		return true;

	return false;
}

/*
 *	buildGangDefinition: reads the GP catalog tables and build a CdbComponentDatabases structure.
 *	It then converts this to a Gang structure and initializes
 *	all the non-connection related fields.
 */
static Gang *
buildGangDefinition(GangType type, int gang_id, int size, int content, char *portal_name)
{
	Gang	   *newGangDefinition = NULL;

	SegmentDatabaseDescriptor *segdbDesc;
	CdbComponentDatabaseInfo *p;

	int			seg_count = 0;
	int			i;

	Assert(CurrentMemoryContext == GangContext);
	Assert(size > 0);

	/*
	 * Get the structure representing the configured segment
	 * databases.  This structure is fetched into a permanent memory
	 * context. We also allocate
	 * space for and partially construct the data structure pointed to
	 * by ClusterDefn and headed by CdbClusterDescriptor.
	 */
	newGangDefinition = (Gang *) palloc0(sizeof(Gang));

	newGangDefinition->type = type;
	newGangDefinition->size = size;
	newGangDefinition->gang_id = gang_id;
	newGangDefinition->allocated = false;
	newGangDefinition->active = false;
	newGangDefinition->noReuse = false;
	newGangDefinition->portal_name = (portal_name ? pstrdup(portal_name) : (char *) NULL);

	if (gp_log_gang >= GPVARS_VERBOSITY_VERBOSE)
	{
		const char *pn1 = "";
		const char *pn2 = "";
		const char *pn3 = "";

		if (portal_name)
		{
			pn1 = "; portal='";
			pn2 = portal_name;
			pn3 = "'";
		}

		switch (type)
		{
			case GANGTYPE_PRIMARY_WRITER:
				elog(LOG, "Starting %d qExec processes for primary writer gang%s%s%s",
					 size, pn1, pn2, pn3);
				break;
			case GANGTYPE_ENTRYDB_READER:
				elog(LOG, "Starting 1 qExec process for entry db reader gang%s%s%s",
					 pn1, pn2, pn3);
				break;
			case GANGTYPE_PRIMARY_READER:
				/* Must be size 1 or size numsegments */
				if (size == 1)
					elog(LOG, "Starting 1 qExec process for seg%d reader gang%s%s%s",
						 content, pn1, pn2, pn3);
				else
					elog(LOG, "Starting %d qExec processes for reader gang%s%s%s",
						 size, pn1, pn2, pn3);
				break;
			case GANGTYPE_UNALLOCATED:
			default:
				Insist(0);
		}
	}

	/*
	 * MPP-2613
	 * NOTE: global, and we may be leaking some here (but if we free
	 * anyone who has pointers into the free()ed space is going to
	 * freak out)
	 */
	cdb_component_dbs = getCdbComponentDatabases();

	if (cdb_component_dbs == NULL ||
		cdb_component_dbs->total_segments <= 0 ||
		cdb_component_dbs->total_segment_dbs <= 0)
	{
		insist_log(false, "schema not populated while building segworker group");
		return NULL;
	}

	/* 1-gang on entry db? */
	if (type == GANGTYPE_ENTRYDB_READER)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb_component_dbs->entry_db_info[0];

		if (cdbinfo->hostip == NULL)
		{
			/* we know we want to use localhost */
			cdbinfo->hostip = "127.0.0.1";
		}

		Assert(size == 1 && content == -1);

		/* Build segdb descriptor. */
		segdbDesc = (SegmentDatabaseDescriptor *) palloc0(sizeof(*segdbDesc));
		cdbconn_initSegmentDescriptor(segdbDesc, cdbinfo);

		newGangDefinition->size = 1;
		newGangDefinition->db_descriptors = segdbDesc;

		return newGangDefinition;
	}

	/* if mirroring is not configured */
	if (cdb_component_dbs->total_segment_dbs == cdb_component_dbs->total_segments)
	{
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "building Gang: mirroring not configured");
		}
		disableFTS();
		mirroringNotConfigured = true;
	}

	newGangDefinition->db_descriptors =
		(SegmentDatabaseDescriptor *) palloc0(cdb_component_dbs->total_segments *
										  sizeof(SegmentDatabaseDescriptor));

	Assert(cdb_component_dbs->total_segment_dbs > 0);

	newGangDefinition->segment_database_info = &cdb_component_dbs->segment_db_info[0];

	/*
	 * We loop through the segment_db_info.  Each item has a segindex.
	 * They are sorted by segindex, and there can be > 1 segment_db_info for
	 * a given segindex (currently, there can be 1 or 2)
	 */
	for (i = 0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		p = &cdb_component_dbs->segment_db_info[i];

		if (type == GANGTYPE_PRIMARY_READER && size == 1) /* handle singleton readers separately */
		{
			if (size == 1) /* singleton reader */
			{
				if (content != p->segindex)
					continue;

				/* Got our singleton */
				segdbDesc = &newGangDefinition->db_descriptors[seg_count];
				cdbconn_initSegmentDescriptor(segdbDesc, p);
				seg_count++;
				break;
			}
		}
		else /* GANGTYPE_PRIMARY_WRITER or (GANGTYPE_PRIMARY_READER AND N-READER) */
		{
			if (SEGMENT_IS_ACTIVE_PRIMARY(p))
			{
				segdbDesc = &newGangDefinition->db_descriptors[seg_count];
				cdbconn_initSegmentDescriptor(segdbDesc, p);
				seg_count++;
			}
			else
			{
				/* can't add a mirror to our gang. */
				continue;
			}
		}
	}

	/*
	 * All other gangs (N-readers, primary-writers, and 1-readers)
	 * should be the requested size, or we throw an error.
	 */
	if (newGangDefinition->size != seg_count)
	{
		FtsReConfigureMPP(false);

		if (shmDtmStarted != NULL && !*shmDtmStarted && isFTSEnabled())
		{
			elog(LOG, "Fault detected prior to dtm-startup squelching error.");
			return NULL;
		}

		elog(ERROR, "Not all primary segment instances are active and connected");
		/* not reached */
	}
	return newGangDefinition;
}

int
gp_pthread_create(pthread_t * thread,
				  void *(*start_routine) (void *),
				  void *arg, const char *caller)
{
	int			pthread_err = 0;
	pthread_attr_t t_atts;

	/*
	 * Call some init function. Before any thread is created, we need to init
	 * some static stuff. The main purpose is to guarantee the non-thread safe
	 * stuff are called in main thread, before any child thread get running.
	 * Note these staic data structure should be read only after init.	Thread
	 * creation is a barrier, so there is no need to get lock before we use
	 * these data structures.
	 *
	 * So far, we know we need to do this for getpwuid_r (See MPP-1971, glibc
	 * getpwuid_r is not thread safe).
	 */
#ifndef WIN32
	get_gp_passwdptr();
#endif

	/*
	 * save ourselves some memory: the defaults for thread stack size are
	 * large (1M+)
	 */
	pthread_err = pthread_attr_init(&t_atts);
	if (pthread_err != 0)
	{
		elog(LOG, "%s: pthread_attr_init failed.  Error %d", caller, pthread_err);
		return pthread_err;
	}

#ifdef pg_on_solaris
	/* Solaris doesn't have PTHREAD_STACK_MIN ? */
	pthread_err = pthread_attr_setstacksize(&t_atts, (256 * 1024));
#else
	pthread_err = pthread_attr_setstacksize(&t_atts, Max(PTHREAD_STACK_MIN, (256 * 1024)));
#endif
	if (pthread_err != 0)
	{
		elog(LOG, "%s: pthread_attr_setstacksize failed.  Error %d", caller, pthread_err);
		pthread_attr_destroy(&t_atts);
		return pthread_err;
	}

	pthread_err = pthread_create(thread, &t_atts, start_routine, arg);

	pthread_attr_destroy(&t_atts);

	return pthread_err;
}

static void
addOneOption(StringInfo buffer, struct config_generic *guc)
{
	Assert(guc && (guc->flags & GUC_GPDB_ADDOPT));
	switch (guc->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *bguc = (struct config_bool *) guc;

				appendStringInfo(buffer, " -c %s=%s", guc->name,
								 *(bguc->variable) ? "true" : "false");
				return;
			}
		case PGC_INT:
			{
				struct config_int *iguc = (struct config_int *) guc;

				appendStringInfo(buffer, " -c %s=%d", guc->name, *iguc->variable);
				return;
			}
		case PGC_REAL:
			{
				struct config_real *rguc = (struct config_real *) guc;

				appendStringInfo(buffer, " -c %s=%f", guc->name, *rguc->variable);
				return;
			}
		case PGC_STRING:
			{
				struct config_string *sguc = (struct config_string *) guc;
				const char *str = *sguc->variable;
				int			i;

				appendStringInfo(buffer, " -c %s=", guc->name);
				/*
				 * All whitespace characters must be escaped. See
				 * pg_split_opts() in the backend.
				 */
				for (i = 0; str[i] != '\0'; i++)
				{
					if (isspace((unsigned char) str[i]))
						appendStringInfoChar(buffer, '\\');

					appendStringInfoChar(buffer, str[i]);
				}
				return;
			}
	}

	elog(ERROR, "unrecognized GUC var type");
}

/*
 * Put all GUCs to option string.
 *
 * Returns a palloc'd string.
 */
static char *
build_options(bool iswriter, int segindex, bool i_am_superuser)
{
	struct config_generic **gucs = get_guc_variables();
	int			ngucs = get_num_guc_variables();
	StringInfoData buf;
	int			i;

	initStringInfo(&buf);

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		write_log("addOptions: iswriter %d segindex %d", iswriter, segindex);

	/* The following gucs are set will function parameter */
	appendStringInfo(&buf, "-c gp_segment=%d", segindex);

	/* GUCs need special handling */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbComponentDatabaseInfo *qdinfo;

		qdinfo = &cdb_component_dbs->entry_db_info[0];
		if (qdinfo->hostip != NULL)
			appendStringInfo(&buf, " -c gp_qd_hostname=%s", qdinfo->hostip);
		else
			appendStringInfo(&buf, " -c gp_qd_hostname=%s", qdinfo->hostname);
		appendStringInfo(&buf, " -c gp_qd_port=%d", qdinfo->port);
	}
	else
	{
		appendStringInfo(&buf, " -c gp_qd_hostname=%s", qdHostname);
		appendStringInfo(&buf, " -c gp_qd_port=%d", qdPostmasterPort);
	}

	appendStringInfo(&buf, " -c gp_qd_callback_info=port=%d", PostPortNumber);

	/*
	 * Transactions are tricky.
	 * Here is the copy and pasted code, and we know they are working.
	 * The problem, is that QE may ends up with different iso level, but
	 * postgres really does not have read uncommited and repeated read.
	 * (is this true?) and they are mapped.
	 *
	 * Put these two gucs in the generic framework works (pass make installcheck-good)
	 * if we make assign_defaultxactisolevel and assign_XactIsoLevel correct take
	 * string "readcommitted" etc.	(space stripped).  However, I do not
	 * want to change this piece of code unless I know it is broken.
	 */
	if (DefaultXactIsoLevel != XACT_READ_COMMITTED)
	{
		if (DefaultXactIsoLevel == XACT_SERIALIZABLE)
			appendStringInfo(&buf, " -c default_transaction_isolation=serializable");
	}

	if (XactIsoLevel != XACT_READ_COMMITTED)
	{
		if (XactIsoLevel == XACT_SERIALIZABLE)
			appendStringInfo(&buf, " -c transaction_isolation=serializable");
	}

	for (i = 0; i < ngucs; ++i)
	{
		struct config_generic *guc = gucs[i];

		if ((guc->flags & GUC_GPDB_ADDOPT) &&
			(guc->context == PGC_USERSET || i_am_superuser))
		{
			addOneOption(&buf, guc);
		}
	}

	return buf.data;
}

/*
 * build_gpqeid_param
 *
 * Called from the qDisp process to create the "gpqeid" parameter string
 * to be passed to a qExec that is being started.
 */
static char *
build_gpqeid_param(bool is_writer)
{
	StringInfoData buf;

	initStringInfo(&buf);

#ifdef HAVE_INT64_TIMESTAMP
#define TIMESTAMP_FORMAT INT64_FORMAT
#else
#ifndef _WIN32
#define TIMESTAMP_FORMAT "%.14a"
#else
#define TIMESTAMP_FORMAT "%g"
#endif
#endif

	appendStringInfo(&buf,
					 "%d;" TIMESTAMP_FORMAT ";%s",
					 gp_session_id,
					 PgStartTime,
					 (is_writer ? "true" : "false"));
	return buf.data;

}	/* build_gpqeid_param */

/*
 * cdbgang_parse_gpqeid_params
 *
 * Called very early in backend initialization, to interpret the "gpqeid"
 * parameter value that a qExec receives from its qDisp.
 *
 * At this point, client authentication has not been done; the backend
 * command line options have not been processed; GUCs have the settings
 * inherited from the postmaster; etc; so don't try to do too much in here.
 */
static bool
gpqeid_next_param(char **cpp, char **npp)
{
	*cpp = *npp;
	if (!*cpp)
		return false;

	*npp = strchr(*npp, ';');
	if (*npp)
	{
		**npp = '\0';
		++*npp;
	}
	return true;
}

void
cdbgang_parse_gpqeid_params(struct Port * port __attribute__((unused)), const char *gpqeid_value)
{
	char	   *gpqeid = pstrdup(gpqeid_value);
	char	   *cp;
	char	   *np = gpqeid;

	/* The presence of an gpqeid string means this backend is a qExec. */
	SetConfigOption("gp_session_role", "execute",
					PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_session_id */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_session_id", cp,
						PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* PgStartTime */
	if (gpqeid_next_param(&cp, &np))
	{
#ifdef HAVE_INT64_TIMESTAMP
		if (!scanint8(cp, true, &PgStartTime))
			goto bad;
#else
		PgStartTime = strtod(cp, NULL);
#endif
	}

	/* Gp_is_writer */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_is_writer", cp,
						PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* Too few items, or too many? */
	if (!cp || np)
		goto bad;

	if (gp_session_id <= 0 ||
		PgStartTime <= 0)
		goto bad;

	pfree(gpqeid);
	return;

bad:
	elog(FATAL, "Segment dispatched with invalid option: 'gpqeid=%s'", gpqeid_value);
}	/* cdbgang_parse_gpqeid_params */


void
cdbgang_parse_gpqdid_params(struct Port * port __attribute__((unused)), const char *gpqdid_value)
{
	char	   *gpqdid = pstrdup(gpqdid_value);
	char	   *cp;
	char	   *np = gpqdid;

	/*
	 * The presence of an gpqdid string means this is a callback from QE to
	 * QD.
	 */
	SetConfigOption("gp_is_callback", "true",
					PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_session_id */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_session_id", cp,
						PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* PgStartTime */
	if (gpqeid_next_param(&cp, &np))
	{
#ifdef HAVE_INT64_TIMESTAMP
		if (!scanint8(cp, true, &PgStartTime))
			goto bad;
#else
		PgStartTime = strtod(cp, NULL);
#endif
	}
	Assert(gp_is_callback);

	/* Too few items, or too many? */
	if (!cp || np)
		goto bad;

	if (gp_session_id <= 0 ||
		PgStartTime <= 0)
		goto bad;

	pfree(gpqdid);
	return;

bad:
	elog(FATAL, "Master callback dispatched with invalid option: 'gpqdid=%s'", gpqdid_value);
}	/* cdbgang_parse_gpqdid_params */

/*
 * This is where we keep track of all the gangs that exist for this session.
 * On a QD, gangs can either be "available" (not currently in use), or "allocated".
 *
 * On a Dispatch Agent, we just store them in the "available" lists, as the DA doesn't
 * keep track of allocations (it assumes the QD will keep track of what is allocated or not).
 *
 */

static List *allocatedReaderGangsN = NIL;
static List *availableReaderGangsN = NIL;
static List *allocatedReaderGangs1 = NIL;
static List *availableReaderGangs1 = NIL;
static Gang *primaryWriterGang = NULL;

List *
getAllReaderGangs()
{
	return list_concat(getAllIdleReaderGangs(), getAllBusyReaderGangs());
}

List *
getAllIdleReaderGangs()
{
	List	   *res = NIL;
	ListCell   *le;

	/*
	 * Do not use list_concat() here, it would destructively modify the lists!
	 */
	foreach(le, availableReaderGangsN)
	{
		res = lappend(res, lfirst(le));
	}
	foreach(le, availableReaderGangs1)
	{
		res = lappend(res, lfirst(le));
	}

	return res;
}

List *
getAllBusyReaderGangs()
{
	List	   *res = NIL;
	ListCell   *le;

	/*
	 * Do not use list_concat() here, it would destructively modify the lists!
	 */
	foreach(le, allocatedReaderGangsN)
	{
		res = lappend(res, lfirst(le));
	}
	foreach(le, allocatedReaderGangs1)
	{
		res = lappend(res, lfirst(le));
	}

	return res;
}

/*
 * This is the main routine.   It allocates a gang to a query.
 * If we have a previously created but unallocated gang lying around, we use that.
 * otherwise, we create a new gang
 *
 * This is only used on the QD.
 *
 */
Gang *
allocateGang(GangType type, int size, int content, char *portal_name)
{
	/*
	 * First, we look for an unallocated but created gang of the right type
	 * if it exists, we return it.
	 * Else, we create a new gang
	 */

	MemoryContext oldContext;

	Gang	   *gp = NULL;

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	if (type == GANGTYPE_PRIMARY_WRITER)
		return allocateWriterGang();

	insist_log(IsTransactionOrTransactionBlock(), "cannot allocate segworker group outside of transaction");

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "allocateGang for portal %s: allocatedReaderGangsN %d, availableReaderGangsN %d",
			 (portal_name ? portal_name : "<unnamed>"),
			 list_length(allocatedReaderGangsN),
			 list_length(availableReaderGangsN));
	}


	if (GangContext == NULL)
	{
		GangContext = AllocSetContextCreate(TopMemoryContext,
											"Gang Context",
											ALLOCSET_DEFAULT_MINSIZE,
											ALLOCSET_DEFAULT_INITSIZE,
											ALLOCSET_DEFAULT_MAXSIZE);
	}
	Assert(GangContext != NULL);

	oldContext = MemoryContextSwitchTo(GangContext);

	switch (type)
	{

		case GANGTYPE_PRIMARY_READER:
		case GANGTYPE_ENTRYDB_READER:
			/*
			 * Must be size 1 or size numsegments
			 * If size 1, then content tells us which segment we want
			 */
			if (size != 1)
			{
				Assert(type == GANGTYPE_PRIMARY_READER);
				if (availableReaderGangsN != NIL)		/* There are gangs
														 * already created */
				{
					if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
						elog(LOG, "Reusing an available reader N-gang for %s", (portal_name ? portal_name : "unnamed portal"));

					gp = linitial(availableReaderGangsN);
					Assert(gp != NULL);
					Assert(gp->type == type && gp->size > 1);

					/*
					 * make sure no memory is still allocated for previous
					 * portal name that this gang belonged to
					 */
					if (gp->portal_name)
						pfree(gp->portal_name);

					/* let the gang know which portal it is being assigned to */
					gp->portal_name = (portal_name ? pstrdup(portal_name) : (char *) NULL);

					availableReaderGangsN =
						list_delete_first(availableReaderGangsN);

				}
				else
					/*
					 * no pre-created gang exists
					 */
				{
					if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
						elog(LOG, "Creating a new reader N-gang for %s", (portal_name ? portal_name : "unnamed portal"));

					gp = createGang(type, gang_id_counter++, size, content, portal_name);
				}

				allocatedReaderGangsN = lappend(allocatedReaderGangsN, gp);

			}
			else
				/*
				 * size 1 gangs
				 */
			{

				if (availableReaderGangs1 != NIL)		/* There are gangs
														 * already created */
				{
					ListCell   *cell = list_head(availableReaderGangs1);
					ListCell   *nextcell;
					ListCell   *prevcell = NULL;

					/* Look for a gang with the requested segment index. */
					while (cell)
					{
						nextcell = lnext(cell);
						gp = (Gang *) lfirst(cell);
						Assert(gp->size == 1);
						if (gp->db_descriptors[0].segindex == content)
							break;
						prevcell = cell;
						cell = nextcell;
					}

					/* Remove gang from freelist and reuse. */
					if (cell)
					{
						if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
							elog(LOG, "reusing an available reader 1-gang for seg%d, portal: %s", content, (portal_name ? portal_name : "unnamed"));

						availableReaderGangs1 =
							list_delete_cell(availableReaderGangs1, cell, prevcell);

						/*
						 * make sure no memory is still allocated for previous
						 * portal name that this gang belonged to
						 */
						if (gp->portal_name)
							pfree(gp->portal_name);

						/*
						 * let the gang know which portal it is being assigned
						 * to
						 */
						gp->portal_name = (portal_name ? pstrdup(portal_name) : (char *) NULL);
					}
					else
						gp = NULL;
				}
				if (gp == NULL) /* no pre-created gang exists */
				{
					if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
						elog(LOG, "creating a new reader 1-gang for seg%d, portal: %s", content, (portal_name ? portal_name : "unnamed"));
					gp = createGang(type, gang_id_counter++, size, content, portal_name);
				}
				allocatedReaderGangs1 = lappend(allocatedReaderGangs1, gp);
			}
			break;

		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_UNALLOCATED:
		default:
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
            		errmsg_internal("could not create segworker group")));
			break;
	}

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG,
			 "allocateGang on return: allocatedReaderGangsN %d, availableReaderGangsN %d",
			 list_length(allocatedReaderGangsN),
			 list_length(availableReaderGangsN));

	/*
	 * now we enroll the gang in the current global transaction?
	 */

	MemoryContextSwitchTo(oldContext);
	insist_log(gp != NULL, "no primary segworker group allocated");

	if (gp != NULL)
	{
		gp->allocated = true;
		/* sanity check the gang */
		insist_log(gangOK(gp), "could not connect to segment: initialization of segworker group failed");
	}

	return gp;
}

Gang *
allocateWriterGang()
{
	Gang	   *writer_gang = NULL;
	MemoryContext oldContext;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	writer_gang = primaryWriterGang;

	/*
	 * First, we look for an unallocated but created gang of the right type
	 * if it exists, we return it.
	 * Else, we create a new gang
	 */
	if (writer_gang == NULL)
	{
		int			nsegdb;

		insist_log(IsTransactionOrTransactionBlock(),
				"cannot allocate segworker group outside of transaction");

		if (GangContext == NULL)
		{
			GangContext = AllocSetContextCreate(TopMemoryContext,
												"Gang Context",
												ALLOCSET_DEFAULT_MINSIZE,
												ALLOCSET_DEFAULT_INITSIZE,
												ALLOCSET_DEFAULT_MAXSIZE);
		}
		Assert(GangContext != NULL);

		oldContext = MemoryContextSwitchTo(GangContext);

		nsegdb = getgpsegmentCount();

		writer_gang =
			createGang(GANGTYPE_PRIMARY_WRITER, PRIMARY_WRITER_GANG_ID, nsegdb, -1, NULL);
		MemoryContextSwitchTo(oldContext);
	}
	else
	{
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "Reusing an existing primary writer gang");
	}

	writer_gang->allocated = true;

	/* sanity check the gang */

	if (!gangOK(writer_gang))
	{
		elog(ERROR, "could not temporarily connect to one or more segments");
	}

	primaryWriterGang = writer_gang;

	return writer_gang;
}

/*
 * When we are the dispatch agent, we get told which gang to use by its "gang_id"
 * We need to find the gang in our lists.
 *
 * keeping the gangs in the "available" lists on the Dispatch Agent is a hack,
 * as the dispatch agent doesn't differentiate allocated from available, or 1 gangs
 * from n-gangs.  It assumes the QD keeps track of all that.
 *
 * It might be nice to pass gang type and size to this routine as safety checks.
 *
 */

Gang *
findGangById(int gang_id)
{
	Assert(gang_id >= PRIMARY_WRITER_GANG_ID);

	if (primaryWriterGang && primaryWriterGang->gang_id == gang_id)
		return primaryWriterGang;

	if (gang_id == PRIMARY_WRITER_GANG_ID)
	{
		elog(LOG, "findGangById: primary writer didn't exist when we expected it to");
		return allocateWriterGang();
	}

	/*
	 * Now we iterate through the list of reader gangs
	 * to find the one that matches
	 */
	if (availableReaderGangsN != NIL)
	{
		ListCell   *cur_item;
		ListCell   *prev_item = NULL;

		cur_item = list_head(availableReaderGangsN);

		while (cur_item != NULL)
		{
			Gang	   *gp = (Gang *) lfirst(cur_item);

			if (gp && gp->gang_id == gang_id)
			{
				return gp;
			}

			/* cur_item must be preserved */
			prev_item = cur_item;
			cur_item = lnext(prev_item);

		}
	}
	if (availableReaderGangs1 != NIL)
	{
		ListCell   *cur_item;
		ListCell   *prev_item = NULL;

		cur_item = list_head(availableReaderGangs1);

		while (cur_item != NULL)
		{
			Gang	   *gp = (Gang *) lfirst(cur_item);

			if (gp && gp->gang_id == gang_id)
			{
				return gp;
			}

			/* cur_item must be preserved */
			prev_item = cur_item;
			cur_item = lnext(prev_item);

		}
	}

	/*
	 * 1-gangs can exist on some dispatch agents, and not on others.
	 *
	 * so, we can't tell if not finding the gang is an error or not.
	 *
	 * It would be good if we knew if this was a 1-gang on not.
	 *
	 * The writer gangs are always n-gangs.
	 *
	 */

	if (gang_id <= 2)
	{
		insist_log(false, "could not find segworker group %d", gang_id);
	}
	else
	{
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "could not find segworker group %d", gang_id);
		}
	}

	return NULL;


}

struct SegmentDatabaseDescriptor *
getSegmentDescriptorFromGang(const Gang *gp, int seg)
{

	int			i;

	if (gp == NULL)
		return NULL;

	for (i = 0; i < gp->size; i++)
	{
		if (gp->db_descriptors[i].segindex == seg)
		{
			return &(gp->db_descriptors[i]);
		}
	}

	return NULL;
}

/**
 * @param directDispatch may be null
 */
List *
getCdbProcessList(Gang *gang, int sliceIndex, DirectDispatchInfo *directDispatch)
{
	int			i;
	List	   *list = NIL;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(gang != NULL);

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "getCdbProcessList slice%d gangtype=%d gangsize=%d",
			 sliceIndex, gang->type, gang->size);

	if (gang != NULL && gang->type != GANGTYPE_UNALLOCATED)
	{
		CdbComponentDatabaseInfo *qeinfo;
		int			listsize = 0;

		for (i = 0; i < gang->size; i++)
		{
			CdbProcess *process;
			SegmentDatabaseDescriptor *segdbDesc;
			bool includeThisProcess = true;

			segdbDesc = &gang->db_descriptors[i];
			if (directDispatch != NULL && directDispatch->isDirectDispatch)
			{
				ListCell *cell;

				includeThisProcess = false;
				foreach(cell, directDispatch->contentIds)
				{
					if (lfirst_int(cell) == segdbDesc->segindex)
					{
						includeThisProcess = true;
						break;
					}
				}
			}

			/*
			 * We want the n-th element of the list to be the segDB that handled content n.
			 * And if no segDb is available (down mirror), we want it to be null.
			 *
			 * But the gang structure element n is not necessarily the guy who handles content n,
			 * so we need to skip some slots.
			 *
			 *
			 * We don't do this for reader 1-gangs
			 *
			 */
			if (gang->size > 1 ||
				gang->type == GANGTYPE_PRIMARY_WRITER)
			{
				while (segdbDesc->segindex > listsize)
				{
					list = lappend(list, NULL);
					listsize++;
				}
			}

			if (!includeThisProcess)
			{
				list = lappend(list, NULL);
				listsize++;
				continue;
			}

			process = (CdbProcess *) makeNode(CdbProcess);
			qeinfo = segdbDesc->segment_database_info;

			if (qeinfo == NULL)
			{
				elog(ERROR, "required segment is unavailable");
			}
			else if (qeinfo->hostip == NULL)
			{
				elog(ERROR, "required segment IP is unavailable");
			}

			process->listenerAddr = pstrdup(qeinfo->hostip);

			if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC || Gp_interconnect_type == INTERCONNECT_TYPE_UDP)
				process->listenerPort = (segdbDesc->motionListener >> 16) & 0x0ffff;
			else
				process->listenerPort = (segdbDesc->motionListener & 0x0ffff);

			process->pid = segdbDesc->backendPid;
			process->contentid = segdbDesc->segindex;

			if (gp_log_gang >= GPVARS_VERBOSITY_VERBOSE || DEBUG4 >= log_min_messages)
				elog(LOG, "Gang assignment (gang_id %d): slice%d seg%d %s:%d pid=%d",
					 gang->gang_id,
					 sliceIndex,
					 process->contentid,
					 process->listenerAddr,
					 process->listenerPort,
					 process->pid);

			list = lappend(list, process);
			listsize++;
		}

		insist_log( ! (gang->type == GANGTYPE_PRIMARY_WRITER &&
			listsize < getgpsegmentCount()),
			"master segworker group smaller than number of segments");

		if (gang->size > 1 ||
			gang->type == GANGTYPE_PRIMARY_WRITER)
		{
			while (listsize < getgpsegmentCount())
			{
				list = lappend(list, NULL);
				listsize++;
			}
		}
		Assert(listsize == 1 || listsize == getgpsegmentCount());
	}

	return list;
}

/*
 * getCdbProcessForQD:	Manufacture a CdbProcess representing the QD,
 * as if it were a worker from the executor factory.
 *
 * NOTE: Does not support multiple (mirrored) QDs.
 */
List *
getCdbProcessesForQD(int isPrimary)
{
	List	   *list = NIL;

	CdbComponentDatabaseInfo *qdinfo;
	CdbProcess *proc;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	if (!isPrimary)
	{
		elog(FATAL, "getCdbProcessesForQD: unsupported request for master mirror process");
	}

	if (cdb_component_dbs == NULL)
	{
		cdb_component_dbs = getCdbComponentDatabases();
		if (cdb_component_dbs == NULL)
			elog(ERROR, PACKAGE_NAME " schema not populated");
	}

	qdinfo = &(cdb_component_dbs->entry_db_info[0]);

	Assert(qdinfo->segindex == -1);
	Assert(SEGMENT_IS_ACTIVE_PRIMARY(qdinfo));
	Assert(qdinfo->hostip != NULL);

	proc = makeNode(CdbProcess);
	/*
	 * Set QD listener address to NULL. This
	 * will be filled during starting up outgoing
	 * interconnect connection.
	 */
	proc->listenerAddr = NULL;
	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC || Gp_interconnect_type == INTERCONNECT_TYPE_UDP)
		proc->listenerPort = (Gp_listener_port >> 16) & 0x0ffff;
	else
		proc->listenerPort = (Gp_listener_port & 0x0ffff);
	proc->pid = MyProcPid;
	proc->contentid = -1;

	/*
	 * freeCdbComponentDatabases(cdb_component_dbs);
	 */
	list = lappend(list, proc);
	return list;
}

/*
 * cleanupGang():
 *
 * A return value of "true" means that the gang was intact (or NULL).
 *
 * A return value of false, means that a problem was detected and the
 * gang has been disconnected (and so should not be put back onto the
 * available list).
 */
bool
cleanupGang(Gang *gp)
{
	int			i;

	if (gp == NULL)
		return true;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "cleanupGang: cleaning gang id %d type %d size %d, was used for portal: %s", gp->gang_id, gp->type, gp->size, (gp->portal_name ? gp->portal_name : "(unnamed)"));
	}

	/* disassociate this gang with any portal that it may have belonged to */
	if (gp->portal_name)
		pfree(gp->portal_name);

	gp->portal_name = NULL;

	if (gp->active)
	{
		elog((gp_log_gang >= GPVARS_VERBOSITY_DEBUG ? LOG : DEBUG2), "cleanupGang called on a gang that is active");
	}

	/*
	 * if the process is in the middle of blowing up... then we don't do
	 * anything here.  making libpq and other calls can definitely result in
	 * things getting HUNG.
	 */
	if (proc_exit_inprogress)
		return true;

	/*
	 * Loop through the segment_database_descriptors array and, for each
	 * SegmentDatabaseDescriptor:
	 *	   1) discard the query results (if any)
	 */
	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(gp->db_descriptors[i]);

		if (segdbDesc == NULL)
			return false;

		/*
		 * Note, we cancel all "still running" queries
		 */
		/* PQstatus() is smart enough to handle NULL */
		if (PQstatus(segdbDesc->conn) == CONNECTION_OK)
		{
			PGresult   *pRes;
			int			retry = 0;

			ExecStatusType stat;

			while (NULL != (pRes = PQgetResult(segdbDesc->conn)))
			{
				stat = PQresultStatus(pRes);

				elog(LOG, "(%s) Leftover result at freeGang time: %s %s",
					 segdbDesc->whoami,
					 PQresStatus(stat),
					 PQerrorMessage(segdbDesc->conn));

				PQclear(pRes);
				if (stat == PGRES_FATAL_ERROR)
					break;
				if (stat == PGRES_BAD_RESPONSE)
					break;
				retry++;
				if (retry > 20)
				{
					elog(LOG, "cleanup called when a segworker is still busy, waiting didn't help, trying to destroy the segworker group");
					disconnectAndDestroyGang(gp);
					gp = NULL;
					elog(FATAL, "cleanup called when a segworker is still busy");
					return false;
				}
			}
		}
		else
		{
			/*
			 * something happened to this gang, disconnect instead of
			 * sending it back onto our lists!
			 */
			elog(LOG, "lost connection with segworker group member");
			disconnectAndDestroyGang(gp);
			return false;
		}

		/* QE is no longer associated with a slice. */
		cdbconn_setSliceIndex(segdbDesc, -1);
	}

	gp->allocated = false;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "cleanupGang done");

	if (gp->noReuse)
	{
		disconnectAndDestroyGang(gp);
		gp = NULL;
		return false;
	}

	return true;
}

static bool NeedResetSession = false;
static bool NeedSessionIdChange = false;
static Oid	OldTempNamespace = InvalidOid;

/*
 * cleanupIdleReaderGangs() and cleanupAllIdleGangs().
 *
 * These two routines are used when a session has been idle for a while (waiting for the
 * client to send us SQL to execute).  The idea is to consume less resources while sitting idle.
 *
 * The expectation is that if the session is logged on, but nobody is sending us work to do,
 * we want to free up whatever resources we can.  Usually it means there is a human being at the
 * other end of the connection, and that person has walked away from their terminal, or just hasn't
 * decided what to do next.  We could be idle for a very long time (many hours).
 *
 * Of course, freeing gangs means that the next time the user does send in an SQL statement,
 * we need to allocate gangs (at least the writer gang) to do anything.  This entails extra work,
 * so we don't want to do this if we don't think the session has gone idle.
 *
 * Only call these routines from an idle session.
 *
 * These routines are called from the sigalarm signal handler (hopefully that is safe to do).
 *
 */

/*
 * Destroy all idle (i.e available) reader gangs.
 * It is always safe to get rid of the reader gangs.
 *
 * call only from an idle session.
 */
void
cleanupIdleReaderGangs(void)
{
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "cleanupIdleReaderGangs beginning");

	disconnectAndDestroyAllReaderGangs(false);

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "cleanupIdleReaderGangs done");

	return;
}

/*
 * Destroy all gangs to free all resources on the segDBs, if it is possible (safe) to do so.
 *
 * Call only from an idle session.
 */
void
cleanupAllIdleGangs(void)
{
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "cleanupAllIdleGangs beginning");

	/*
	 * It's always safe to get rid of the reader gangs.
	 *
	 * Since we are idle, any reader gangs will be available but not allocated.
	 */
	disconnectAndDestroyAllReaderGangs(false);

	/*
	 * If we are in a transaction, we can't release the writer gang, as this will abort the transaction.
	 */
	if (!IsTransactionOrTransactionBlock())
	{
		/*
		 * if we have a TempNameSpace, we can't release the writer gang, as this would drop any temp tables we own.
		 *
		 * This test really should be "do we have any temp tables", not "do we have a temp schema", but I'm still trying to figure
		 * out how to test for that.  In general, we only create a temp schema if there was a create temp table statement, and
		 * most of the time people are too lazy to drop their temp tables prior to session end, so 90% of the time this will be OK.
		 * And when we get it wrong, it just means we don't free the writer gang when we could have.
		 *
		 * Better yet would be to have the QD control the dropping of the temp tables, and not doing it by letting each segment
		 * drop it's part of the temp table independently.
		 */
		if (!TempNamespaceOidIsValid())
		{
			/*
			 * Get rid of ALL gangs... Readers, mirror writer, and primary writer.  After this, we have no
			 * resources being consumed on the segDBs at all.
			 */
			disconnectAndDestroyAllGangs();
			/*
			 * Our session wasn't destroyed due to an fatal error or FTS action, so we don't need to
			 * do anything special.  Specifically, we DON'T want to act like we are now in a new session,
			 * since that would be confusing in the log.
			 */
			NeedResetSession = false;
		}
		else
			if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
				elog(LOG, "TempNameSpaceOid is valid, can't free writer");
	}
	else
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "We are in a transaction, can't free writer");

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "cleanupAllIdleGangs done");

	return;
}

static int64
getMaxGangMop(Gang *g)
{
	int64		maxmop = 0;
	int			i;

	for (i = 0; i < g->size; ++i)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(g->db_descriptors[i]);

		if (!segdbDesc)
			continue;
		if (segdbDesc->conn && PQstatus(segdbDesc->conn) != CONNECTION_BAD)
		{
			if (segdbDesc->conn->mop_high_watermark > maxmop)
				maxmop = segdbDesc->conn->mop_high_watermark;
		}
	}

	return maxmop;
}

static List *
cleanupPortalGangList(List *gplist, int cachelimit)
{
	Gang	   *gp;

	if (gplist != NIL)
	{
		int			ngang = list_length(gplist);
		ListCell   *prev = NULL;
		ListCell   *curr = list_head(gplist);

		while (curr)
		{
			bool		destroy = ngang > cachelimit;

			gp = lfirst(curr);

			if (!destroy)
			{
				int64		maxmop = getMaxGangMop(gp);

				if ((maxmop >> 20) > gp_vmem_protect_gang_cache_limit)
					destroy = true;
			}

			if (destroy)
			{
				disconnectAndDestroyGang(gp);
				gplist = list_delete_cell(gplist, curr, prev);
				if (!prev)
					curr = list_head(gplist);
				else
					curr = lnext(prev);
				--ngang;
			}
			else
			{
				prev = curr;
				curr = lnext(prev);
			}
		}
	}

	return gplist;
}

/*
 * Portal drop... Clean up what gangs we hold
 */
void
cleanupPortalGangs(Portal portal)
{
	MemoryContext oldContext;
	const char *portal_name;

	if (portal->name && strcmp(portal->name, "") != 0)
	{
		portal_name = portal->name;
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "cleanupPortalGangs %s", portal_name);
		}
	}
	else
	{
		portal_name = NULL;
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "cleanupPortalGangs (unamed portal)");
		}
	}


	if (GangContext)
		oldContext = MemoryContextSwitchTo(GangContext);
	else
		oldContext = MemoryContextSwitchTo(TopMemoryContext);

	availableReaderGangsN = cleanupPortalGangList(availableReaderGangsN, gp_cached_gang_threshold);
	availableReaderGangs1 = cleanupPortalGangList(availableReaderGangs1, MAX_CACHED_1_GANGS);

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "cleanupPortalGangs '%s'. Reader gang inventory: "
			 "allocatedN=%d availableN=%d allocated1=%d available1=%d",
		 	(portal_name ? portal_name : "unnamed portal"),
	  		 list_length(allocatedReaderGangsN), list_length(availableReaderGangsN),
			 list_length(allocatedReaderGangs1), list_length(availableReaderGangs1));
	}

	MemoryContextSwitchTo(oldContext);
}

void
disconnectAndDestroyGang(Gang *gp)
{
	int			i;

	if (gp == NULL)
		return;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "disconnectAndDestroyGang entered: id = %d", gp->gang_id);

	if (gp->active || gp->allocated)
		elog(gp_log_gang >= GPVARS_VERBOSITY_DEBUG ? LOG : DEBUG2, "Warning: disconnectAndDestroyGang called on an %s gang",
			 gp->active ? "active" : "allocated");

	if (gp->gang_id < 1 || gp->gang_id > 100000000 || gp->type > 10 || gp->size > 100000)
	{
		elog(LOG, "disconnectAndDestroyGang on bad gang");
		return;
	}

	/*
	 * Loop through the segment_database_descriptors array and, for each
	 * SegmentDatabaseDescriptor:
	 *	   1) discard the query results (if any),
	 *	   2) disconnect the session, and
	 *	   3) discard any connection error message.
	 */
	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(gp->db_descriptors[i]);

		if (segdbDesc == NULL)
			continue;

		if (segdbDesc->conn && PQstatus(segdbDesc->conn) != CONNECTION_BAD)
		{

			PGTransactionStatusType status =
			PQtransactionStatus(segdbDesc->conn);

			elog((Debug_print_full_dtm ? LOG : (gp_log_gang >= GPVARS_VERBOSITY_DEBUG ? LOG : DEBUG5)),
				 "disconnectAndDestroyGang: got QEDistributedTransactionId = %u, QECommandId = %u, and QEDirty = %s",
				 segdbDesc->conn->QEWriter_DistributedTransactionId,
				 segdbDesc->conn->QEWriter_CommandId,
				 (segdbDesc->conn->QEWriter_Dirty ? "true" : "false"));

			if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			{
				const char *ts;

				switch (status)
				{
					case PQTRANS_IDLE:
						ts = "idle";
						break;
					case PQTRANS_ACTIVE:
						ts = "active";
						break;
					case PQTRANS_INTRANS:
						ts = "idle, within transaction";
						break;
					case PQTRANS_INERROR:
						ts = "idle, within failed transaction";
						break;
					case PQTRANS_UNKNOWN:
						ts = "unknown transaction status";
						break;
					default:
						ts = "invalid transaction status";
						break;
				}
				elog(LOG, "Finishing connection with %s; %s",
					 segdbDesc->whoami, ts);
			}

			if (status == PQTRANS_ACTIVE)
			{
				char		errbuf[256];
				PGcancel   *cn = PQgetCancel(segdbDesc->conn);

				if (Debug_cancel_print || gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
					elog(LOG, "Calling PQcancel for %s", segdbDesc->whoami);

				if (PQcancel(cn, errbuf, 256) == 0)
				{
					elog(LOG, "Unable to cancel %s: %s", segdbDesc->whoami, errbuf);
				}
				PQfreeCancel(cn);
			}

			PQfinish(segdbDesc->conn);

			segdbDesc->conn = NULL;
		}
	}

	/*
	 * when we get rid of the primary writer gang we MUST also get rid of the reader
	 * gangs due to the shared local snapshot code that is shared between
	 * readers and writers.
	 */
	if (gp->type == GANGTYPE_PRIMARY_WRITER)
	{
		disconnectAndDestroyAllReaderGangs(true);

		resetSessionForPrimaryGangLoss();
	}

	/*
	 * Discard the segment array and the cluster descriptor
	 */
	if(gp->segment_database_info) /* in case of NULL in entrydb gang. */
	{
		pfree(gp->segment_database_info);
		gp->segment_database_info = NULL;
	}

	if (gp->db_descriptors != NULL)
	{
		pfree(gp->db_descriptors);
		gp->db_descriptors = NULL;
	}

	gp->size = 0;
	if (gp->portal_name != NULL)
		pfree(gp->portal_name);
	pfree(gp);

	/*
	 * this is confusing, gp is local variable, no need to null it. gp = NULL;
	 */
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "disconnectAndDestroyGang done");
}

/*
 * freeGangsForPortal
 *
 * Free all gangs that were allocated for a specific portal
 * (could either be a cursor name or an unnamed portal)
 *
 * Be careful when moving gangs onto the available list, if
 * cleanupGang() tells us that the gang has a problem, the gang has
 * been free()ed and we should discard it -- otherwise it is good as
 * far as we can tell.
 */
void
freeGangsForPortal(char *portal_name)
{
	MemoryContext oldContext;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "freeGangsForPortal '%s'. Reader gang inventory: "
			 "allocatedN=%d availableN=%d allocated1=%d available1=%d",
			 (portal_name ? portal_name : "unnamed portal"),
			 list_length(allocatedReaderGangsN), list_length(availableReaderGangsN),
			 list_length(allocatedReaderGangs1), list_length(availableReaderGangs1));

	/*
	 * the primary and mirror writer gangs "belong" to the unnamed portal --
	 * if we have multiple active portals trying to release, we can't just
	 * release and re-release the writers each time !
	 */
	if (!portal_name)
	{
		if (!cleanupGang(primaryWriterGang))
		{
			primaryWriterGang = NULL;	/* cleanupGang called
										 * disconnectAndDestroyGang() already */
			disconnectAndDestroyAllGangs();

			elog(ERROR, "could not temporarily connect to one or more segments");

			return;
		}
	}

	if (GangContext)
		oldContext = MemoryContextSwitchTo(GangContext);
	else
		oldContext = MemoryContextSwitchTo(TopMemoryContext);

	/*
	 * Now we iterate through the list of allocated reader gangs
	 * and we free all the gangs that belong to the portal that
	 * was specified by our caller.
	 */
	if (allocatedReaderGangsN != NIL)
	{
		ListCell   *cur_item;
		ListCell   *prev_item = NULL;

		cur_item = list_head(allocatedReaderGangsN);

		while (cur_item != NULL)
		{
			Gang	   *gp = (Gang *) lfirst(cur_item);

			if (isTargetPortal(gp->portal_name, portal_name))
			{
				if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
					elog(LOG, "Returning a reader N-gang to the available list");

				/* cur_item must be removed */
				allocatedReaderGangsN = list_delete_cell(allocatedReaderGangsN, cur_item, prev_item);

				/* we only return the gang to the available list if it is good */
				if (cleanupGang(gp))
					availableReaderGangsN = lappend(availableReaderGangsN, gp);

				if (prev_item)
					cur_item = lnext(prev_item);
				else
					cur_item = list_head(allocatedReaderGangsN);
			}
			else
			{
				if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
					elog(LOG, "Skipping the release of a reader N-gang. It is used by another portal");

				/* cur_item must be preserved */
				prev_item = cur_item;
				cur_item = lnext(prev_item);
			}
		}
	}

	if (allocatedReaderGangs1 != NIL)
	{
		ListCell   *cur_item;
		ListCell   *prev_item = NULL;

		cur_item = list_head(allocatedReaderGangs1);

		while (cur_item != NULL)
		{
			Gang	   *gp = (Gang *) lfirst(cur_item);

			if (isTargetPortal(gp->portal_name, portal_name))
			{
				if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
					elog(LOG, "Returning a reader 1-gang to the available list");

				/* cur_item must be removed */
				allocatedReaderGangs1 = list_delete_cell(allocatedReaderGangs1, cur_item, prev_item);

				/* we only return the gang to the available list if it is good */
				if (cleanupGang(gp))
					availableReaderGangs1 = lappend(availableReaderGangs1, gp);

				if (prev_item)
					cur_item = lnext(prev_item);
				else
					cur_item = list_head(allocatedReaderGangs1);
			}
			else
			{
				if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
					elog(LOG, "Skipping the release of a reader 1-gang. It is used by another portal");

				/* cur_item must be preserved */
				prev_item = cur_item;
				cur_item = lnext(prev_item);
			}
		}
	}

	MemoryContextSwitchTo(oldContext);

	if (gp_log_gang >= GPVARS_VERBOSITY_VERBOSE)
	{
		if (allocatedReaderGangsN ||
			availableReaderGangsN ||
			allocatedReaderGangs1 ||
			availableReaderGangs1)
		{
			elog(LOG, "Gangs released for portal '%s'. Reader gang inventory: "
				 "allocatedN=%d availableN=%d allocated1=%d available1=%d",
				 (portal_name ? portal_name : "unnamed portal"),
				 list_length(allocatedReaderGangsN), list_length(availableReaderGangsN),
				 list_length(allocatedReaderGangs1), list_length(availableReaderGangs1));
		}
	}
}

/*
 * disconnectAndDestroyAllReaderGangs
 *
 * Here we destroy all reader gangs regardless of the portal they belong to.
 * TODO: This may need to be done more carefully when multiple cursors are
 * enabled.
 * If the parameter destroyAllocated is true, then destroy allocated as well as
 * available gangs.
 */
static void
disconnectAndDestroyAllReaderGangs(bool destroyAllocated)
{
	Gang	   *gp;

	if (allocatedReaderGangsN != NIL && destroyAllocated)
	{
		gp = linitial(allocatedReaderGangsN);
		while (gp != NULL)
		{
			disconnectAndDestroyGang(gp);
			allocatedReaderGangsN = list_delete_first(allocatedReaderGangsN);
			if (allocatedReaderGangsN != NIL)
				gp = linitial(allocatedReaderGangsN);
			else
				gp = NULL;
		}
	}
	if (availableReaderGangsN != NIL)
	{
		gp = linitial(availableReaderGangsN);
		while (gp != NULL)
		{
			disconnectAndDestroyGang(gp);
			availableReaderGangsN = list_delete_first(availableReaderGangsN);
			if (availableReaderGangsN != NIL)
				gp = linitial(availableReaderGangsN);
			else
				gp = NULL;
		}
	}
	if (allocatedReaderGangs1 != NIL && destroyAllocated)
	{
		gp = linitial(allocatedReaderGangs1);
		while (gp != NULL)
		{
			disconnectAndDestroyGang(gp);
			allocatedReaderGangs1 = list_delete_first(allocatedReaderGangs1);
			if (allocatedReaderGangs1 != NIL)
				gp = linitial(allocatedReaderGangs1);
			else
				gp = NULL;
		}
	}
	if (availableReaderGangs1 != NIL)
	{
		gp = linitial(availableReaderGangs1);
		while (gp != NULL)
		{
			disconnectAndDestroyGang(gp);
			availableReaderGangs1 = list_delete_first(availableReaderGangs1);
			if (availableReaderGangs1 != NIL)
				gp = linitial(availableReaderGangs1);
			else
				gp = NULL;
		}
	}
	if (destroyAllocated)
		allocatedReaderGangsN = NIL;
	availableReaderGangsN = NIL;

	if (destroyAllocated)
		allocatedReaderGangs1 = NIL;
	availableReaderGangs1 = NIL;
}


/*
 * Drop any temporary tables associated with the current session and
 * use a new session id since we have effectively reset the session.
 *
 * Call this procedure outside of a transaction.
 */
void
CheckForResetSession(void)
{
	int			oldSessionId = 0;
	int			newSessionId = 0;
	Oid			dropTempNamespaceOid;

	if (!NeedResetSession)
		return;

	/*
	 * Do the session id change early.
	 */
	if (NeedSessionIdChange)
	{
		/* If we have gangs, we can't change our session ID. */
		Assert(!gangsExist());

		oldSessionId = gp_session_id;
		ProcNewMppSessionId(&newSessionId);

		gp_session_id = newSessionId;
		gp_command_count = 0;

		/* Update the slotid for our singleton reader. */
		if (SharedLocalSnapshotSlot != NULL)
			SharedLocalSnapshotSlot->slotid = gp_session_id;

		elog(LOG, "The previous session was reset because its gang was disconnected (session id = %d). "
			 "The new session id = %d",
			 oldSessionId, newSessionId);

		NeedSessionIdChange = false;
	}

	if (IsTransactionOrTransactionBlock())
	{
		NeedResetSession = false;
		return;
	}


	dropTempNamespaceOid = OldTempNamespace;
	OldTempNamespace = InvalidOid;
	NeedResetSession = false;

	if (dropTempNamespaceOid != InvalidOid)
	{
		PG_TRY();
		{
			DropTempTableNamespaceForResetSession(dropTempNamespaceOid);
		}
		PG_CATCH();
		{
			/*
			 * But first demote the error to something much less
			 * scary.
			 */
			if (!elog_demote(WARNING))
			{
				elog(LOG, "unable to demote error");
				PG_RE_THROW();
			}

			EmitErrorReport();
			FlushErrorState();
		}
		PG_END_TRY();
	}

}

extern void
resetSessionForPrimaryGangLoss(void)
{
	if (ProcCanSetMppSessionId())
	{
		/*
		 * Not too early.
		 */
		NeedResetSession = true;
		NeedSessionIdChange = true;

		/*
		 * Keep this check away from transaction/catalog access, as we are
		 * possibly just after releasing ResourceOwner at the end of Tx.
		 * It's ok to remember uncommitted temporary namespace because
		 * DropTempTableNamespaceForResetSession will simply do nothing
		 * if the namespace is not visible.
		 */
		if (TempNamespaceOidIsValid())
		{
			/*
			 * Here we indicate we don't have a temporary table namespace
			 * anymore so all temporary tables of the previous session will
			 * be inaccessible.  Later, when we can start a new transaction,
			 * we will attempt to actually drop the old session tables to
			 * release the disk space.
			 */
			OldTempNamespace = ResetTempNamespace();

			elog(WARNING,
				 "Any temporary tables for this session have been dropped "
				 "because the gang was disconnected (session id = %d)",
				 gp_session_id);
		}
		else
			OldTempNamespace = InvalidOid;

	}

}

void
disconnectAndDestroyAllGangs(void)
{
	if (Gp_role == GP_ROLE_UTILITY)
		return;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "disconnectAndDestroyAllGangs");

	/* for now, destroy all readers, regardless of the portal that owns them */
	disconnectAndDestroyAllReaderGangs(true);

	disconnectAndDestroyGang(primaryWriterGang);
	primaryWriterGang = NULL;

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "disconnectAndDestroyAllGangs done");
}

bool
gangOK(Gang *gp)
{
	int			i;

	if (gp == NULL)
		return false;

	if (gp->gang_id < 1 || gp->gang_id > 100000000 || gp->type > 10 || gp->size > 100000)
		return false;

	/*
	 * Gang is direct-connect (no agents).
	 */

	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(gp->db_descriptors[i]);

		if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
			return false;
	}

	return true;
}

/*
 * Set segdb states, called by FtsReConfigureMPP.
 */
void
detectFailedConnections(void)
{
	int			i;
	CdbComponentDatabaseInfo *segInfo;
	bool		fullScan = true;

	/*
	 * check primary gang
	 */
	if (primaryWriterGang != NULL)
	{
		for (i = 0; i < primaryWriterGang->size; i++)
		{
			segInfo = primaryWriterGang->db_descriptors[i].segment_database_info;

			/*
			 * Note: the probe process is responsible for doing the
			 * actual marking of the segments.
			 */
			FtsTestConnection(segInfo, fullScan);
			fullScan = false;
		}
	}
}

CdbComponentDatabases *
getComponentDatabases(void)
{
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);
	return (cdb_component_dbs == NULL) ? getCdbComponentDatabases() : cdb_component_dbs;
}

bool
gangsExist(void)
{
	return (primaryWriterGang != NULL ||
			allocatedReaderGangsN != NIL ||
			availableReaderGangsN != NIL ||
			allocatedReaderGangs1 != NIL ||
			availableReaderGangs1 != NIL);
}

/*
 * the gang is working for portal p1. we are only interested in gangs
 * from portal p2. if p1 and p2 are the same portal return true. false
 * otherwise.
 */
static
bool
isTargetPortal(const char *p1, const char *p2)
{
	/* both are unnamed portals (represented as NULL) */
	if (!p1 && !p2)
		return true;

	/* one is unnamed, the other is named */
	if (!p1 || !p2)
		return false;

	/* both are the same named portal */
	if (strcmp(p1, p2) == 0)
		return true;

	return false;
}

#ifdef USE_ASSERT_CHECKING
/**
 * Assert that slicetable is valid. Must be called after ExecInitMotion, which sets up the slice table
 */
void
AssertSliceTableIsValid(SliceTable *st, struct PlannedStmt *pstmt)
{
	if (!st)
	{
		return;
	}

	Assert(st);
	Assert(pstmt);

	Assert(pstmt->nMotionNodes == st->nMotions);
	Assert(pstmt->nInitPlans == st->nInitPlans);

	ListCell *lc = NULL;
	int i = 0;

	int maxIndex = st->nMotions + st->nInitPlans + 1;

	Assert(maxIndex == list_length(st->slices));

	foreach (lc, st->slices)
	{
		Slice *s = (Slice *) lfirst(lc);

		/* The n-th slice entry has sliceIndex of n */
		Assert(s->sliceIndex == i && "slice index incorrect");

		/* The root index of a slice is either 0 or is a slice corresponding to an init plan */
		Assert((s->rootIndex == 0)
				|| (s->rootIndex > st->nMotions && s->rootIndex < maxIndex));

		/* Parent slice index */
		if (s->sliceIndex == s->rootIndex )
		{
			/* Current slice is a root slice. It will have parent index -1.*/
			Assert(s->parentIndex == -1 && "expecting parent index of -1");
		}
		else
		{
			/* All other slices must have a valid parent index */
			Assert(s->parentIndex >= 0 && s->parentIndex < maxIndex && "slice's parent index out of range");
		}

		/* Current slice's children must consider it the parent */
		ListCell *lc1 = NULL;
		foreach (lc1, s->children)
		{
			int childIndex = lfirst_int(lc1);
			Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
			Slice *sc = (Slice *) list_nth(st->slices, childIndex);
			Assert(sc->parentIndex == s->sliceIndex && "slice's child does not consider it the parent");
		}

		/* Current slice must be in its parent's children list */
		if (s->parentIndex >= 0)
		{
			Slice *sp = (Slice *) list_nth(st->slices, s->parentIndex);

			bool found = false;
			foreach (lc1, sp->children)
			{
				int childIndex = lfirst_int(lc1);
				Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
				Slice *sc = (Slice *) list_nth(st->slices, childIndex);

				if (sc->sliceIndex == s->sliceIndex)
				{
					found = true;
					break;
				}
			}

			Assert(found && "slice's parent does not consider it a child");
		}



		i++;
	}
}

#endif
