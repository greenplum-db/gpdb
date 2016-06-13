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
#include <pthread.h>
#include <limits.h>

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
#include "utils/sharedsnapshot.h"
#include "tcop/pquery.h"

#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp_query.h"
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

#define LOG_GANG_DEBUG(...) do { \
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG) elog(__VA_ARGS__); \
    } while(false);

#define MAX_CACHED_1_GANGS 1

/*
 * Which gang this QE belongs to; this would be used in PostgresMain to find out
 * the slice this QE should execute
 */
int qe_gang_id = 0;

/*
 * Points to the result of getCdbComponentDatabases()
 */
static CdbComponentDatabases *cdb_component_dbs = NULL;

static int largest_gangsize = 0;

static MemoryContext GangContext = NULL;

static bool NeedResetSession = false;
static Oid OldTempNamespace = InvalidOid;

static List *allocatedReaderGangsN = NIL;
static List *availableReaderGangsN = NIL;
static List *allocatedReaderGangs1 = NIL;
static List *availableReaderGangs1 = NIL;
static Gang *primaryWriterGang = NULL;

/*
 * Every gang created must have a unique identifier
 */
#define PRIMARY_WRITER_GANG_ID 1
static int gang_id_counter = 2;

/*
 * Parameter structure for the DoConnect threads
 */
typedef struct DoConnectParms
{
	/*
	 * db_count: The number of segdbs that this thread is responsible for
	 * connecting to.
	 * Equals the count of segdbDescPtrArray below.
	 */
	int db_count;

	/*
	 * segdbDescPtrArray: Array of SegmentDatabaseDescriptor* 's that this thread is
	 * responsible for connecting to. Has size equal to db_count.
	 */
	SegmentDatabaseDescriptor **segdbDescPtrArray;

	/* type of gang. */
	GangType type;

	int gangId;

	/* connect options. GUC etc. */
	StringInfo connectOptions;

	/* The pthread_t thread handle. */
	pthread_t thread;
} DoConnectParms;

static Gang *buildGangDefinition(GangType type, int gang_id, int size,
		int content);
static DoConnectParms* makeConnectParms(int parmsCount, GangType type, int gangId);
static void destroyConnectParms(DoConnectParms *doConnectParmsAr, int count);
static void checkConnectionStatus(Gang* gp, int* countInRecovery,
		int* countSuccessful);
static bool isPrimaryWriterGangAlive(void);
static void *thread_DoConnect(void *arg);
static void
build_gpqeid_param(char *buf, int bufsz, int segIndex,
				   bool is_writer, int gangId);
static Gang *createGang(GangType type, int gang_id, int size, int content);
static void disconnectAndDestroyGang(Gang *gp);
static void disconnectAndDestroyAllReaderGangs(bool destroyAllocated);

static bool isTargetPortal(const char *p1, const char *p2);
static void addOptions(StringInfo string, bool iswriter);
static bool cleanupGang(Gang * gp);
static void resetSessionForPrimaryGangLoss(void);
static const char* gangTypeToString(GangType);
static CdbComponentDatabaseInfo *copyCdbComponentDatabaseInfo(
		CdbComponentDatabaseInfo *dbInfo);
static CdbComponentDatabaseInfo *findDatabaseInfoBySegIndex(
		CdbComponentDatabases *cdbs, int segIndex);
static void addGangToAllocated(Gang *gp);
static Gang *getAvailableGang(GangType type, int size, int content);

/*
 * Create a reader gang.
 *
 * @type can be GANGTYPE_ENTRYDB_READER, GANGTYPE_SINGLETON_READER or GANGTYPE_PRIMARY_READER.
 */
Gang *
allocateReaderGang(GangType type, char *portal_name)
{
	MemoryContext oldContext = NULL;
	Gang *gp = NULL;
	int size = 0;
	int content = 0;

	LOG_GANG_DEBUG(LOG,
			"allocateReaderGang for portal %s: allocatedReaderGangsN %d, availableReaderGangsN %d, allocatedReaderGangs1 %d, availableReaderGangs1 %d",
			(portal_name ? portal_name : "<unnamed>"),
			list_length(allocatedReaderGangsN),
			list_length(availableReaderGangsN),
			list_length(allocatedReaderGangs1),
			list_length(availableReaderGangs1));

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	insist_log(IsTransactionOrTransactionBlock(),
			"cannot allocate segworker group outside of transaction");

	if (GangContext == NULL)
	{
		GangContext = AllocSetContextCreate(TopMemoryContext, "Gang Context",
		ALLOCSET_DEFAULT_MINSIZE,
		ALLOCSET_DEFAULT_INITSIZE,
		ALLOCSET_DEFAULT_MAXSIZE);
	}
	Assert(GangContext != NULL);
	oldContext = MemoryContextSwitchTo(GangContext);

	switch (type)
	{
	case GANGTYPE_ENTRYDB_READER:
		content = -1;
		size = 1;
		break;

	case GANGTYPE_SINGLETON_READER:
		content = gp_singleton_segindex;
		size = 1;
		break;

	case GANGTYPE_PRIMARY_READER:
		content = 0;
		size = getgpsegmentCount();
		break;

	default:
		Assert(false);
	}

	/*
	 * First, we look for an unallocated but created gang of the right type
	 * if it exists, we return it.
	 * Else, we create a new gang
	 */
	gp = getAvailableGang(type, size, content);
	if (gp == NULL)
	{
		LOG_GANG_DEBUG(LOG, "Creating a new reader size %d gang for %s", size,
				(portal_name ? portal_name : "unnamed portal"));

		gp = createGang(type, gang_id_counter++, size, content);
		gp->allocated = true;
	}

	/*
	 * make sure no memory is still allocated for previous
	 * portal name that this gang belonged to
	 */
	if (gp->portal_name)
		pfree(gp->portal_name);

	/* let the gang know which portal it is being assigned to */
	gp->portal_name = (portal_name ? pstrdup(portal_name) : (char *) NULL);

	/* sanity check the gang */
	insist_log(gangOK(gp), "could not connect to segment: initialization of segworker group failed");

	addGangToAllocated(gp);

	MemoryContextSwitchTo(oldContext);

	LOG_GANG_DEBUG(LOG,
			"on return: allocatedReaderGangs %d, availableReaderGangsN %d, allocatedReaderGangs1 %d, availableReaderGangs1 %d",
			list_length(allocatedReaderGangsN),
			list_length(availableReaderGangsN),
			list_length(allocatedReaderGangs1),
			list_length(availableReaderGangs1));

	return gp;
}

/*
 * Create a writer gang.
 */
Gang *
allocateWriterGang()
{
	Gang *writerGang = NULL;
	MemoryContext oldContext = NULL;
	int i = 0;

	LOG_GANG_DEBUG(LOG, "allocateWriterGang begin.");

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	/*
	 * First, we look for an unallocated but created gang of the right type
	 * if it exists, we return it.
	 * Else, we create a new gang
	 */
	if (primaryWriterGang == NULL)
	{
		int nsegdb = getgpsegmentCount();

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

		writerGang = createGang(GANGTYPE_PRIMARY_WRITER,
				PRIMARY_WRITER_GANG_ID, nsegdb, -1);
		writerGang->allocated = true;

		/*
		 * set "whoami" for utility statement.
		 * non-utility statement will overwrite it in function getCdbProcessList.
		 */
		for(i = 0; i < writerGang->size; i++)
			setQEIdentifier(&writerGang->db_descriptors[i], -1, writerGang->perGangContext);

		MemoryContextSwitchTo(oldContext);
	}
	else
	{
		LOG_GANG_DEBUG(LOG, "Reusing an existing primary writer gang");
		writerGang = primaryWriterGang;
	}

	/* sanity check the gang */
	if (!gangOK(writerGang))
		elog(ERROR, "could not connect to segment: initialization of segworker group failed");

	LOG_GANG_DEBUG(LOG, "allocateWriterGang end.");

	primaryWriterGang = writerGang;
	return writerGang;
}

/*
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
static Gang *
createGang(GangType type, int gang_id, int size, int content)
{
	Gang *newGangDefinition;
	SegmentDatabaseDescriptor *segdbDesc = NULL;
	DoConnectParms *doConnectParmsAr = NULL;
	DoConnectParms *pParms = NULL;
	int parmIndex = 0;
	int threadCount = 0;
	int i = 0;
	int create_gang_retry_counter = 0;
	int in_recovery_mode_count = 0;
	int successful_connections = 0;

	LOG_GANG_DEBUG(LOG, "createGang type = %d, gang_id = %d, size = %d, content = %d",
			type, gang_id, size, content);

	/* check arguments */
	Assert(size == 1 || size == getgpsegmentCount());
	Assert(CurrentResourceOwner != NULL);
	Assert(CurrentMemoryContext == GangContext);
	if (type == GANGTYPE_PRIMARY_WRITER)
		Assert(gang_id == PRIMARY_WRITER_GANG_ID);
	Assert(gp_connections_per_thread >= 0);

create_gang_retry:
	/* If we're in a retry, we may need to reset our initial state, a bit */
	newGangDefinition = NULL;
	doConnectParmsAr = NULL;
	successful_connections = 0;
	in_recovery_mode_count = 0;
	threadCount = 0;

	/* Check the writer gang first. */
	if (type != GANGTYPE_PRIMARY_WRITER && !isPrimaryWriterGangAlive())
	{
		elog(LOG, "primary writer gang is broken");
		goto exit;
	}

	/* allocate and initialize a gang structure */
	newGangDefinition = buildGangDefinition(type, gang_id, size, content);
	Assert(newGangDefinition != NULL);
	Assert(newGangDefinition->size == size);
	Assert(newGangDefinition->perGangContext != NULL);
	MemoryContextSwitchTo(newGangDefinition->perGangContext);

	/*
	 * The most threads we could have is segdb_count / gp_connections_per_thread, rounded up.
	 * This is equivalent to 1 + (segdb_count-1) / gp_connections_per_thread.
	 * We allocate enough memory for this many DoConnectParms structures,
	 * even though we may not use them all.
	 */
	if (gp_connections_per_thread == 0)
		threadCount = 1;
	else
		threadCount = 1 + (size - 1) / gp_connections_per_thread;
	Assert(threadCount > 0);

	/* initialize connect parameters */
	doConnectParmsAr = makeConnectParms(threadCount, type, gang_id);
	for (i = 0; i < size; i++)
	{
		parmIndex = i / gp_connections_per_thread;
		pParms = &doConnectParmsAr[parmIndex];
		segdbDesc = &newGangDefinition->db_descriptors[i];
		pParms->segdbDescPtrArray[pParms->db_count++] = segdbDesc;
	}

	/* start threads and doing the connect */
	for (i = 0; i < threadCount; i++)
	{
		int pthread_err;
		pParms = &doConnectParmsAr[i];

		LOG_GANG_DEBUG(LOG,
				"createGang creating thread %d of %d for libpq connections",
				i + 1, threadCount);

		pthread_err = gp_pthread_create(&pParms->thread, thread_DoConnect, pParms, "createGang");
		if (pthread_err != 0)
		{
			int j;

			/*
			 * Error during thread create (this should be caused by resource
			 * constraints). If we leave the threads running, they'll
			 * immediately have some problems -- so we need to join them, and
			 * *then* we can issue our FATAL error
			 */
			for (j = 0; j < i; j++)
			{
				pthread_join(doConnectParmsAr[j].thread, NULL);
			}

			ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("failed to create thread %d of %d", i + 1, threadCount),
					errdetail("pthread_create() failed with err %d", pthread_err)));
		}
	}

	/*
	 * wait for all of the DoConnect threads to complete.
	 */
	for (i = 0; i < threadCount; i++)
	{
		LOG_GANG_DEBUG(LOG, "joining to thread %d of %d for libpq connections",
				i + 1, threadCount);

		if (0 != pthread_join(doConnectParmsAr[i].thread, NULL))
		{
			elog(FATAL, "could not create segworker group");
		}
	}

	/*
	 * Free the memory allocated for the threadParms array
	 */
	destroyConnectParms(doConnectParmsAr, threadCount);
	doConnectParmsAr = NULL;

	/* find out the successful connections and the failed ones */
	checkConnectionStatus(newGangDefinition, &in_recovery_mode_count,
			&successful_connections);

	LOG_GANG_DEBUG(LOG,"createGang: %d processes requested; %d successful connections %d in recovery",
			size, successful_connections, in_recovery_mode_count);

	MemoryContextSwitchTo(GangContext);

	if (size == successful_connections)
	{
		largest_gangsize = Max(largest_gangsize, size);
		return newGangDefinition;
	}

	/* there'er failed connections */

	/*
	 * If this is a reader gang and the writer gang is invalid, destroy all gangs.
	 * This happens when one segment is reset.
	 */
	if (type != GANGTYPE_PRIMARY_WRITER && !isPrimaryWriterGangAlive())
	{
		elog(LOG, "primary writer gang is broken");
		goto exit;
	}

	/* FTS shows some segment DBs are down, destroy all gangs. */
	if (isFTSEnabled() &&
		FtsTestSegmentDBIsDown(newGangDefinition->db_descriptors, size))
	{
		elog(LOG, "FTS detected some segments are down");
		goto exit;
	}

	disconnectAndDestroyGang(newGangDefinition);
	newGangDefinition = NULL;

	/* Writer gang is created before reader gangs. */
	if (type == GANGTYPE_PRIMARY_WRITER)
		Insist(!gangsExist());

	/* We could do some retry here */
	if (successful_connections + in_recovery_mode_count == size &&
		gp_gang_creation_retry_count &&
		create_gang_retry_counter++ < gp_gang_creation_retry_count)
	{
		LOG_GANG_DEBUG(LOG, "createGang: gang creation failed, but retryable.");

		/*
		 * On the first retry, we want to verify that we are
		 * using the most current version of the
		 * configuration.
		 */
		if (create_gang_retry_counter == 0)
			FtsNotifyProber();

		CHECK_FOR_INTERRUPTS();
		pg_usleep(gp_gang_creation_retry_timer * 1000);
		CHECK_FOR_INTERRUPTS();

		goto create_gang_retry;
	}

exit:
	if(newGangDefinition != NULL)
		disconnectAndDestroyGang(newGangDefinition);

	disconnectAndDestroyAllGangs(true);
	CheckForResetSession();
	ereport(ERROR,
			(errcode(ERRCODE_GP_INTERCONNECTION_ERROR), errmsg("failed to acquire resources on one or more segments")));
	return NULL;
}

/*
 *	Thread procedure.
 *	Perform the connect.
 */
static void *
thread_DoConnect(void *arg)
{
	DoConnectParms *pParms = (DoConnectParms *) arg;
	SegmentDatabaseDescriptor **segdbDescPtrArray = pParms->segdbDescPtrArray;
	int db_count = pParms->db_count;

	SegmentDatabaseDescriptor *segdbDesc = NULL;
	int i = 0;

	gp_set_thread_sigmasks();

	/*
	 * The pParms contains an array of SegmentDatabaseDescriptors
	 * to connect to.
	 */
	for (i = 0; i < db_count; i++)
	{
		char gpqeid[100];

		segdbDesc = segdbDescPtrArray[i];

		if (segdbDesc == NULL || segdbDesc->segment_database_info == NULL)
		{
			write_log("thread_DoConnect: bad segment definition during gang creation %d/%d\n", i, db_count);
			continue;
		}

		/*
		 * Build the connection string.  Writer-ness needs to be processed
		 * early enough now some locks are taken before command line options
		 * are recognized.
		 */
		build_gpqeid_param(gpqeid, sizeof(gpqeid),
						   segdbDesc->segindex,
						   pParms->type == GANGTYPE_PRIMARY_WRITER,
						   pParms->gangId);

		/* check the result in createGang */
		cdbconn_doConnect(segdbDesc, gpqeid, pParms->connectOptions->data);
	}

	return (NULL);
}

/*
 * Test if the connections of the primary writer gang are alive.
 */
static bool isPrimaryWriterGangAlive(void)
{
	if (primaryWriterGang == NULL)
		return false;

	int size = primaryWriterGang->size;
	int i = 0;

	Assert(size = getgpsegmentCount());

	for (i = 0; i < size; i++)
	{
		SegmentDatabaseDescriptor *segdb = &primaryWriterGang->db_descriptors[i];
		if (!isSockAlive(segdb->conn->sock))
			return false;
	}

	return true;
}

/*
 * Check the segment failure reason by comparing connection error message.
 */
static bool segment_failure_due_to_recovery(
		SegmentDatabaseDescriptor *segdbDesc)
{
	char *fatal = NULL, *message = NULL, *ptr = NULL;
	int fatal_len = 0;

	if (segdbDesc == NULL)
		return false;

	message = segdbDesc->error_message.data;

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
 * Check all the connections of a gang.
 *
 * return the count of successful connections and
 * the count of failed connections due to recovery.
 */
static void checkConnectionStatus(Gang* gp, int* countInRecovery,
		int* countSuccessful)
{
	SegmentDatabaseDescriptor* segdbDesc = NULL;
	int size = gp->size;
	int i = 0;

	/*
	 * In this loop, we check whether the connections were successful.
	 * If not, we recreate the error message with palloc and report it.
	 */
	for (i = 0; i < size; i++)
	{
		segdbDesc = &gp->db_descriptors[i];
		/*
		 * check connection established or not, if not, we may have to
		 * re-build this gang.
		 */
		if (cdbconn_isBadConnection(segdbDesc))
		{
			/*
			 * Log failed connections.	Complete failures
			 * are taken care of later.
			 */
			Assert(segdbDesc->whoami != NULL);
			elog(LOG, "Failed connection to %s", segdbDesc->whoami);

			insist_log(segdbDesc->errcode != 0 && segdbDesc->error_message.len != 0,
					"connection is null, but no error code or error message, for segDB %d", i);

			ereport(LOG, (errcode(segdbDesc->errcode), errmsg("%s",segdbDesc->error_message.data)));
			cdbconn_resetQEErrorMessage(segdbDesc);

			/* this connect failed -- but why ? */
			if (segment_failure_due_to_recovery(segdbDesc))
				(*countInRecovery)++;
		}
		else
		{
			Assert(segdbDesc->errcode == 0 && segdbDesc->error_message.len == 0);

			/* We have a live connection! */
			(*countSuccessful)++;
		}
	}
}

/*
 * Read gp_segment_configuration catalog table and build a CdbComponentDatabases.
 *
 * Read the catalog if FTS is reconfigured.
 *
 * We don't want to destroy cdb_component_dbs when one gang get destroyed, so allocate
 * it in GangContext instead of perGangContext.
 */
CdbComponentDatabases *
getComponentDatabases(void)
{
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);
	Assert(GangContext != NULL);

	uint64 ftsVersion = getFtsVersion();
	MemoryContext oldContext = MemoryContextSwitchTo(GangContext);

	if (cdb_component_dbs == NULL)
	{
		cdb_component_dbs = getCdbComponentDatabases();
		cdb_component_dbs->fts_version = ftsVersion;
	}
	else if (cdb_component_dbs->fts_version != ftsVersion)
	{
		LOG_GANG_DEBUG(LOG, "FTS rescanned, get new component databases info.");
		freeCdbComponentDatabases(cdb_component_dbs);
		cdb_component_dbs = getCdbComponentDatabases();
		cdb_component_dbs->fts_version = ftsVersion;
	}

	MemoryContextSwitchTo(oldContext);

	return cdb_component_dbs;
}

/*
 * Make a copy of CdbComponentDatabaseInfo.
 *
 * Caller destroy it.
 */
static CdbComponentDatabaseInfo *copyCdbComponentDatabaseInfo(
		CdbComponentDatabaseInfo *dbInfo)
{
	int i = 0;
	int size = sizeof(CdbComponentDatabaseInfo);
	CdbComponentDatabaseInfo *newInfo = palloc0(size);
	memcpy(newInfo, dbInfo, size);

	if (dbInfo->hostip)
		newInfo->hostip = pstrdup(dbInfo->hostip);

	/* So far, we don't need them. */
	newInfo->address = NULL;
	newInfo->hostname = NULL;
	for (i = 0; i < COMPONENT_DBS_MAX_ADDRS; i++)
		newInfo->hostaddrs[i] = NULL;

	return newInfo;
}

/*
 * Find CdbComponentDatabases in the array by segment index.
 */
static CdbComponentDatabaseInfo *findDatabaseInfoBySegIndex(
		CdbComponentDatabases *cdbs, int segIndex)
{
	Assert(cdbs != NULL);
	int i = 0;
	CdbComponentDatabaseInfo *cdbInfo = NULL;
	for (i = 0; i < cdbs->total_segment_dbs; i++)
	{
		cdbInfo = &cdbs->segment_db_info[i];
		if (segIndex == cdbInfo->segindex)
			break;
	}

	return cdbInfo;
}

/*
 * Reads the GP catalog tables and build a CdbComponentDatabases structure.
 * It then converts this to a Gang structure and initializes all the non-connection related fields.
 *
 * Call this function in GangContext.
 * Returns a not-null pointer.
 */
static Gang *
buildGangDefinition(GangType type, int gang_id, int size, int content)
{
	Gang *newGangDefinition = NULL;
	CdbComponentDatabaseInfo *cdbinfo = NULL;
	CdbComponentDatabaseInfo *cdbInfoCopy = NULL;
	SegmentDatabaseDescriptor *segdbDesc = NULL;
	MemoryContext perGangContext = NULL;

	int segCount = 0;
	int i = 0;

	LOG_GANG_DEBUG(LOG, "buildGangDefinition:Starting %d qExec processes for %s gang", size,
			gangTypeToString(type));

	Assert(CurrentMemoryContext == GangContext);
	Assert(size == 1 || size == getgpsegmentCount());

	/* read gp_segment_configuration and build CdbComponentDatabases */
	cdb_component_dbs = getComponentDatabases();

	if (cdb_component_dbs == NULL ||
		cdb_component_dbs->total_segments <= 0 ||
		cdb_component_dbs->total_segment_dbs <= 0)
		insist_log(false, "schema not populated while building segworker group");

	/* if mirroring is not configured */
	if (cdb_component_dbs->total_segment_dbs == cdb_component_dbs->total_segments)
	{
		LOG_GANG_DEBUG(LOG, "building Gang: mirroring not configured");
		disableFTS();
	}

	perGangContext = AllocSetContextCreate(GangContext, "Per Gang Context",
					ALLOCSET_DEFAULT_MINSIZE,
					ALLOCSET_DEFAULT_INITSIZE,
					ALLOCSET_DEFAULT_MAXSIZE);
	Assert(perGangContext != NULL);
	MemoryContextSwitchTo(perGangContext);

	/* allocate a gang */
	newGangDefinition = (Gang *) palloc0(sizeof(Gang));
	newGangDefinition->type = type;
	newGangDefinition->size = size;
	newGangDefinition->gang_id = gang_id;
	newGangDefinition->allocated = false;
	newGangDefinition->noReuse = false;
	newGangDefinition->dispatcherActive = false;
	newGangDefinition->portal_name = NULL;
	newGangDefinition->perGangContext = perGangContext;
	newGangDefinition->db_descriptors =
			(SegmentDatabaseDescriptor *) palloc0(size * sizeof(SegmentDatabaseDescriptor));

	/* initialize db_descriptors */
	switch (type)
	{
	case GANGTYPE_ENTRYDB_READER:
		cdbinfo = &cdb_component_dbs->entry_db_info[0];
		cdbInfoCopy = copyCdbComponentDatabaseInfo(cdbinfo);
		segdbDesc = &newGangDefinition->db_descriptors[0];
		cdbconn_initSegmentDescriptor(segdbDesc, cdbInfoCopy);
		setQEIdentifier(segdbDesc, -1, perGangContext);
		break;

	case GANGTYPE_SINGLETON_READER:
		cdbinfo = findDatabaseInfoBySegIndex(cdb_component_dbs, content);
		cdbInfoCopy = copyCdbComponentDatabaseInfo(cdbinfo);
		segdbDesc = &newGangDefinition->db_descriptors[0];
		cdbconn_initSegmentDescriptor(segdbDesc, cdbInfoCopy);
		setQEIdentifier(segdbDesc, -1, perGangContext);
		break;

	case GANGTYPE_PRIMARY_READER:
	case GANGTYPE_PRIMARY_WRITER:
		/*
		 * We loop through the segment_db_info.  Each item has a segindex.
		 * They are sorted by segindex, and there can be > 1 segment_db_info for
		 * a given segindex (currently, there can be 1 or 2)
		 */
		for (i = 0; i < cdb_component_dbs->total_segment_dbs; i++)
		{
			cdbinfo = &cdb_component_dbs->segment_db_info[i];
			if (SEGMENT_IS_ACTIVE_PRIMARY(cdbinfo))
			{
				segdbDesc = &newGangDefinition->db_descriptors[segCount];
				cdbInfoCopy = copyCdbComponentDatabaseInfo(cdbinfo);
				cdbconn_initSegmentDescriptor(segdbDesc, cdbInfoCopy);
				setQEIdentifier(segdbDesc, -1, perGangContext);
				segCount++;
			}
		}

		if (size != segCount)
		{
			FtsReConfigureMPP(false);
			elog(ERROR, "Not all primary segment instances are active and connected");
		}
		break;

	default:
		Assert(false);
	}

	LOG_GANG_DEBUG(LOG, "buildGangDefinition done");
	MemoryContextSwitchTo(GangContext);
	return newGangDefinition;
}

/*
 * Add one GUC to the option string.
 */
static void addOneOption(StringInfo string, struct config_generic * guc)
{
	Assert(guc && (guc->flags & GUC_GPDB_ADDOPT));
	switch (guc->vartype)
	{
	case PGC_BOOL:
	{
		struct config_bool *bguc = (struct config_bool *) guc;

		appendStringInfo(string, " -c %s=%s", guc->name, *(bguc->variable) ? "true" : "false");
		break;
	}
	case PGC_INT:
	{
		struct config_int *iguc = (struct config_int *) guc;

		appendStringInfo(string, " -c %s=%d", guc->name, *iguc->variable);
		break;
	}
	case PGC_REAL:
	{
		struct config_real *rguc = (struct config_real *) guc;

		appendStringInfo(string, " -c %s=%f", guc->name, *rguc->variable);
		break;
	}
	case PGC_STRING:
	{
		struct config_string *sguc = (struct config_string *) guc;
		const char *str = *sguc->variable;
		int i;

		appendStringInfo(string, " -c %s=", guc->name);
		/*
		 * All whitespace characters must be escaped. See
		 * pg_split_opts() in the backend.
		 */
		for (i = 0; str[i] != '\0'; i++)
		{
			if (isspace((unsigned char) str[i]))
				appendStringInfoChar(string, '\\');

			appendStringInfoChar(string, str[i]);
		}
		break;
	}
	default:
		Insist(false);
	}
}

/*
 * Add GUCs to option string.
 */
static void addOptions(StringInfo string, bool iswriter)
{
	struct config_generic **gucs = get_guc_variables();
	int ngucs = get_num_guc_variables();
	CdbComponentDatabaseInfo *qdinfo = NULL;
	int i;

	Assert (Gp_role == GP_ROLE_DISPATCH);
	LOG_GANG_DEBUG(LOG, "addOptions: iswriter %d", iswriter);

	qdinfo = &cdb_component_dbs->entry_db_info[0];
	appendStringInfo(string, " -c gp_qd_hostname=%s", qdinfo->hostip);
	appendStringInfo(string, " -c gp_qd_port=%d", qdinfo->port);

	appendStringInfo(string, " -c gp_qd_callback_info=port=%d", PostPortNumber);

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
			appendStringInfo(string, " -c default_transaction_isolation=serializable");
	}

	if (XactIsoLevel != XACT_READ_COMMITTED)
	{
		if (XactIsoLevel == XACT_SERIALIZABLE)
			appendStringInfo(string, " -c transaction_isolation=serializable");
	}

	for (i = 0; i < ngucs; ++i)
	{
		struct config_generic *guc = gucs[i];

		if ((guc->flags & GUC_GPDB_ADDOPT) && (guc->context == PGC_USERSET || procRoleIsSuperuser()))
			addOneOption(string, guc);
	}
}

/*
 * Initialize a DoConnectParms structure.
 *
 * Including initialize the connect option string.
 */
static DoConnectParms* makeConnectParms(int parmsCount, GangType type, int gangId)
{
	DoConnectParms *doConnectParmsAr = (DoConnectParms*) palloc0(
			parmsCount * sizeof(DoConnectParms));
	DoConnectParms* pParms = NULL;
	StringInfo pOptions = makeStringInfo();
	int segdbPerThread = gp_connections_per_thread == 0 ? getgpsegmentCount() : gp_connections_per_thread;
	int i = 0;

	addOptions(pOptions, type == GANGTYPE_PRIMARY_WRITER);

	for (i = 0; i < parmsCount; i++)
	{
		pParms = &doConnectParmsAr[i];
		pParms->segdbDescPtrArray = (SegmentDatabaseDescriptor**) palloc0(
				segdbPerThread * sizeof(SegmentDatabaseDescriptor *));
		MemSet(&pParms->thread, 0, sizeof(pthread_t));
		pParms->db_count = 0;
		pParms->type = type;
		pParms->connectOptions = pOptions;
		pParms->gangId = gangId;
	}
	return doConnectParmsAr;
}

/*
 * Free all the memory allocated in DoConnectParms.
 */
static void destroyConnectParms(DoConnectParms *doConnectParmsAr, int count)
{
	if (doConnectParmsAr != NULL)
	{
		int i = 0;
		for (i = 0; i < count; i++)
		{
			DoConnectParms *pParms = &doConnectParmsAr[i];
			StringInfo pOptions = pParms->connectOptions;
			if (pOptions->data != NULL)
			{
				pfree(pOptions->data);
				pOptions->data = NULL;
			}
			pParms->connectOptions = NULL;

			pfree(pParms->segdbDescPtrArray);
			pParms->segdbDescPtrArray = NULL;
		}

		pfree(doConnectParmsAr);
	}
}

/*
 * build_gpqeid_param
 *
 * Called from the qDisp process to create the "gpqeid" parameter string
 * to be passed to a qExec that is being started.  NB: Can be called in a
 * thread, so mustn't use palloc/elog/ereport/etc.
 */
static void
build_gpqeid_param(char *buf, int bufsz, int segIndex,
				   bool is_writer, int gangId)
{
#ifdef HAVE_INT64_TIMESTAMP
#define TIMESTAMP_FORMAT INT64_FORMAT
#else
#ifndef _WIN32
#define TIMESTAMP_FORMAT "%.14a"
#else
#define TIMESTAMP_FORMAT "%g"
#endif
#endif

	snprintf(buf, bufsz, "%d;%d;" TIMESTAMP_FORMAT ";%s;%d",
			 gp_session_id, segIndex, PgStartTime,
			 (is_writer ? "true" : "false"), gangId);
}

static bool gpqeid_next_param(char **cpp, char **npp)
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
void
cdbgang_parse_gpqeid_params(struct Port * port __attribute__((unused)),
							const char *gpqeid_value)
{
	char *gpqeid = pstrdup(gpqeid_value);
	char *cp;
	char *np = gpqeid;

	/* The presence of an gpqeid string means this backend is a qExec. */
	SetConfigOption("gp_session_role", "execute", PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_session_id */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_session_id", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_segment */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_segment", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

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
		SetConfigOption("gp_is_writer", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

	if (gpqeid_next_param(&cp, &np))
	{
		qe_gang_id = (int) strtol(cp, NULL, 10);
	}

	/* Too few items, or too many? */
	if (!cp || np)
		goto bad;

	if (gp_session_id <= 0 || PgStartTime <= 0 || qe_gang_id <=0)
		goto bad;

	pfree(gpqeid);
	return;

bad:
	elog(FATAL, "Segment dispatched with invalid option: 'gpqeid=%s'", gpqeid_value);
}

/*
 * TODO: Dead code: remove it.
 */
void cdbgang_parse_gpqdid_params(struct Port * port __attribute__((unused)),
		const char *gpqdid_value)
{
	char *gpqdid = pstrdup(gpqdid_value);
	char *cp;
	char *np = gpqdid;

	/*
	 * The presence of an gpqdid string means this is a callback from QE to
	 * QD.
	 */
	SetConfigOption("gp_is_callback", "true", PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_session_id */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_session_id", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

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

	if (gp_session_id <= 0 || PgStartTime <= 0)
		goto bad;

	pfree(gpqdid);
	return;

bad:
	elog(FATAL, "Master callback dispatched with invalid option: 'gpqdid=%s'", gpqdid_value);
} /* cdbgang_parse_gpqdid_params */

/*
 * This is where we keep track of all the gangs that exist for this session.
 * On a QD, gangs can either be "available" (not currently in use), or "allocated".
 *
 * On a Dispatch Agent, we just store them in the "available" lists, as the DA doesn't
 * keep track of allocations (it assumes the QD will keep track of what is allocated or not).
 *
 */
List *
getAllIdleReaderGangs()
{
	List *res = NIL;
	ListCell *le;

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
getAllAllocatedReaderGangs()
{
	List *res = NIL;
	ListCell *le;

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

static Gang *getAvailableGang(GangType type, int size, int content)
{
	Gang *retGang = NULL;
	switch (type)
	{
	case GANGTYPE_SINGLETON_READER:
	case GANGTYPE_ENTRYDB_READER:
		if (availableReaderGangs1 != NULL) /* There are gangs already created */
		{
			ListCell *cell = NULL;
			ListCell *prevcell = NULL;

			foreach(cell, availableReaderGangs1)
			{
				Gang *gang = (Gang *) lfirst(cell);
				Assert(gang != NULL);
				Assert(gang->size == size);
				if (gang->db_descriptors[0].segindex == content)
				{
					LOG_GANG_DEBUG(LOG, "reusing an available reader 1-gang for seg%d", content);
					retGang = gang;
					availableReaderGangs1 = list_delete_cell(availableReaderGangs1, cell, prevcell);
					break;
				}
				prevcell = cell;
			}
		}
		break;

	case GANGTYPE_PRIMARY_READER:
		if (availableReaderGangsN != NULL) /* There are gangs already created */
		{
			LOG_GANG_DEBUG(LOG, "Reusing an available reader N-gang");

			retGang = linitial(availableReaderGangsN);
			Assert(retGang != NULL);
			Assert(retGang->type == type && retGang->size == size);

			availableReaderGangsN = list_delete_first(availableReaderGangsN);
		}
		break;

	default:
		Assert(false);
	}

	return retGang;
}

static void addGangToAllocated(Gang *gp)
{
	Assert(CurrentMemoryContext == GangContext);

	switch (gp->type)
	{
	case GANGTYPE_SINGLETON_READER:
	case GANGTYPE_ENTRYDB_READER:
		allocatedReaderGangs1 = lappend(allocatedReaderGangs1, gp);
		break;

	case GANGTYPE_PRIMARY_READER:
		allocatedReaderGangsN = lappend(allocatedReaderGangsN, gp);
		break;
	default:
		Assert(false);
	}
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
 * TODO: dispatch agent. remove or re-implement.
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
	ListCell *cell = NULL;
	foreach(cell, availableReaderGangsN)
	{
		Gang *gp = (Gang*) lfirst(cell);
		Assert(gp != NULL);
		if (gp->gang_id == gang_id)
		{
			return gp;
		}
	}

	foreach(cell, availableReaderGangs1)
	{
		Gang *gp = (Gang*) lfirst(cell);
		Assert(gp != NULL);
		if (gp->gang_id == gang_id)
		{
			return gp;
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
		LOG_GANG_DEBUG(LOG, "could not find segworker group %d", gang_id);
	}

	return NULL;
}

struct SegmentDatabaseDescriptor *
getSegmentDescriptorFromGang(const Gang *gp, int seg)
{
	int i = 0;

	if (gp == NULL)
		return NULL;

	for (i = 0; i < gp->size; i++)
	{
		if (gp->db_descriptors[i].segindex == seg)
			return &(gp->db_descriptors[i]);
	}

	return NULL;
}

static CdbProcess *makeCdbProcess(SegmentDatabaseDescriptor *segdbDesc)
{
	CdbProcess *process = (CdbProcess *) makeNode(CdbProcess);
	CdbComponentDatabaseInfo *qeinfo = segdbDesc->segment_database_info;

	if (qeinfo == NULL)
	{
		elog(ERROR, "required segment is unavailable");
	}
	else if (qeinfo->hostip == NULL)
	{
		elog(ERROR, "required segment IP is unavailable");
	}

	process->listenerAddr = pstrdup(qeinfo->hostip);
	process->listenerPort = segdbDesc->motionListener;
	process->pid = segdbDesc->backendPid;
	process->contentid = segdbDesc->segindex;
	return process;
}
/*
 * Create a list of CdbProcess and initialize with Gang information.
 *
 * 1) For primary reader gang and primary writer gang, the elements
 * in this list is order by segment index.
 * 2) For entry DB gang and singleton gang, the list length is 1.
 *
 * @directDispatch: might be null
 */
List *
getCdbProcessList(Gang *gang, int sliceIndex, DirectDispatchInfo *directDispatch)
{
	List *list = NULL;
	int i = 0;

	LOG_GANG_DEBUG(LOG, "getCdbProcessList slice%d gangtype=%d gangsize=%d",
			sliceIndex, gang->type, gang->size);

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(gang != NULL);
	Assert( (gang->type == GANGTYPE_PRIMARY_WRITER && gang->size == getgpsegmentCount()) ||
			(gang->type == GANGTYPE_PRIMARY_READER && gang->size == getgpsegmentCount()) ||
			(gang->type == GANGTYPE_ENTRYDB_READER && gang->size == 1) ||
			(gang->type == GANGTYPE_SINGLETON_READER && gang->size == 1));


	if (directDispatch != NULL && directDispatch->isDirectDispatch)
	{
		/* Currently, direct dispatch is to one segment db. */
		Assert(list_length(directDispatch->contentIds) == 1);

		/* initialize a list of NULL */
		for (i = 0; i < gang->size; i++)
			list = lappend(list, NULL);

		int directDispatchContentId = linitial_int(directDispatch->contentIds);
		SegmentDatabaseDescriptor *segdbDesc = &gang->db_descriptors[directDispatchContentId];
		CdbProcess *process = makeCdbProcess(segdbDesc);
		setQEIdentifier(segdbDesc, sliceIndex, gang->perGangContext);
		list_nth_replace(list, directDispatchContentId, process);
	}
	else
	{
		for (i = 0; i < gang->size; i++)
		{
			SegmentDatabaseDescriptor *segdbDesc = &gang->db_descriptors[i];
			CdbProcess *process = makeCdbProcess(segdbDesc);

			setQEIdentifier(segdbDesc, sliceIndex, gang->perGangContext);
			list = lappend(list, process);

			LOG_GANG_DEBUG(LOG,
					"Gang assignment (gang_id %d): slice%d seg%d %s:%d pid=%d",
					gang->gang_id, sliceIndex, process->contentid,
					process->listenerAddr, process->listenerPort, process->pid);
		}
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
	List *list = NIL;

	CdbComponentDatabaseInfo *qdinfo;
	CdbProcess *proc;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(cdb_component_dbs != NULL);

	if (!isPrimary)
	{
		elog(FATAL, "getCdbProcessesForQD: unsupported request for master mirror process");
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
	proc->listenerPort = Gp_listener_port;

	proc->pid = MyProcPid;
	proc->contentid = -1;

	list = lappend(list, proc);
	return list;
}

/*
 * Destroy or recycle Gangs
 */

/*
 * Disconnect and destroy a Gang.
 *
 * Loop through all the connections of this Gang and disconnect it.
 * Free the resource of this Gang.
 *
 * Caller needs to free all the reader Gangs if this is a writer gang.
 * Caller needs to reset session id if this is a writer gang.
 */
void
static disconnectAndDestroyGang(Gang *gp)
{
	int i = 0;

	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyGang entered: id = %d", gp->gang_id);

	if (gp == NULL)
		return;

	if (gp->allocated)
		LOG_GANG_DEBUG(LOG, "Warning: disconnectAndDestroyGang called on an allocated gang");
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
		Assert(segdbDesc != NULL);
		cdbconn_disconnect(segdbDesc);
		cdbconn_termSegmentDescriptor(segdbDesc);
	}

	MemoryContextDelete(gp->perGangContext);

	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyGang done");
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
static void disconnectAndDestroyAllReaderGangs(bool destroyAllocated)
{
	Gang *gp = NULL;
	ListCell *lc = NULL;

	foreach(lc, availableReaderGangsN)
	{
		gp = (Gang*) lfirst(lc);
		disconnectAndDestroyGang(gp);
	}
	availableReaderGangsN = NULL;

	foreach(lc, availableReaderGangs1)
	{
		gp = (Gang*) lfirst(lc);
		disconnectAndDestroyGang(gp);
	}
	availableReaderGangs1 = NULL;

	if (destroyAllocated)
	{
		foreach(lc, allocatedReaderGangsN)
		{
			gp = (Gang*) lfirst(lc);
			disconnectAndDestroyGang(gp);
		}
		allocatedReaderGangsN = NULL;

		foreach(lc, allocatedReaderGangs1)
		{
			gp = (Gang*) lfirst(lc);
			disconnectAndDestroyGang(gp);
		}
		allocatedReaderGangs1 = NULL;
	}
}

void disconnectAndDestroyAllGangs(bool resetSession)
{
	if (Gp_role == GP_ROLE_UTILITY)
		return;

	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyAllGangs");

	/* for now, destroy all readers, regardless of the portal that owns them */
	disconnectAndDestroyAllReaderGangs(true);

	disconnectAndDestroyGang(primaryWriterGang);
	primaryWriterGang = NULL;

	if (resetSession)
		resetSessionForPrimaryGangLoss();

	/*
	 * As all the reader and writer gangs are destroyed, reset the
	 * corresponding GangContext to prevent leaks
	 */
	if (NULL != GangContext)
	{
		MemoryContextReset(GangContext);
		cdb_component_dbs = NULL;
	}

	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyAllGangs done");
}

/*
 * Destroy all idle (i.e available) reader gangs.
 * It is always safe to get rid of the reader gangs.
 *
 * call only from an idle session.
 */
void disconnectAndDestroyIdleReaderGangs(void)
{
	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyIdleReaderGangs beginning");

	disconnectAndDestroyAllReaderGangs(false);

	LOG_GANG_DEBUG(LOG, "disconnectAndDestroyIdleReaderGangs done");

	return;
}

/*
 * Cleanup a Gang, make it recyclable.
 *
 * A return value of "true" means that the gang was intact (or NULL).
 *
 * A return value of false, means that a problem was detected and the
 * gang has been disconnected (and so should not be put back onto the
 * available list). Caller should call disconnectAndDestroyGang on it.
 */
static bool cleanupGang(Gang *gp)
{
	int i = 0;

	LOG_GANG_DEBUG(LOG,
			"cleanupGang: cleaning gang id %d type %d size %d, was used for portal: %s",
			gp->gang_id, gp->type, gp->size,
			(gp->portal_name ? gp->portal_name : "(unnamed)"));

	if (gp == NULL)
		return true;

	if (gp->noReuse)
		return false;

	if (gp->allocated)
		LOG_GANG_DEBUG(LOG, "cleanupGang called on a gang that is allocated");

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
		Assert(segdbDesc != NULL);

		if (cdbconn_isBadConnection(segdbDesc))
			return false;

		/* Note, we cancel all "still running" queries */
		if (!cdbconn_discardResults(segdbDesc, 20))
			elog(FATAL, "cleanup called when a segworker is still busy");

		/* QE is no longer associated with a slice. */
		setQEIdentifier(segdbDesc, /* slice index */-1, gp->perGangContext);
	}

	/* disassociate this gang with any portal that it may have belonged to */
	if (gp->portal_name != NULL)
	{
		pfree(gp->portal_name);
		gp->portal_name = NULL;
	}

	gp->allocated = false;

	LOG_GANG_DEBUG(LOG, "cleanupGang done");
	return true;
}

/*
 * Get max maxVmemChunksTracked of a gang.
 *
 * return in MB.
 */
static int getGangMaxVmem(Gang *gp)
{
	int64 maxmop = 0;
	int i = 0;

	for (i = 0; i < gp->size; ++i)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(gp->db_descriptors[i]);
		Assert(segdbDesc != NULL);

		if (!cdbconn_isBadConnection(segdbDesc))
			maxmop = Max(maxmop, segdbDesc->conn->mop_high_watermark);
	}

	return (maxmop >> 20);
}

/*
 * the gang is working for portal p1. we are only interested in gangs
 * from portal p2. if p1 and p2 are the same portal return true. false
 * otherwise.
 */
static
bool isTargetPortal(const char *p1, const char *p2)
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

/*
 * remove elements from gang list when:
 * 1. list size > cachelimit
 * 2. max mop of this gang > gp_vmem_protect_gang_cache_limit
 */
static List *
cleanupPortalGangList(List *gplist, int cachelimit)
{
	ListCell *cell = NULL;
	ListCell *prevcell = NULL;
	int nLeft = list_length(gplist);

	if (gplist == NULL)
		return NULL;

	cell = list_head(gplist);
	while (cell != NULL)
	{
		Gang *gang = (Gang *) lfirst(cell);
		Assert(gang->type != GANGTYPE_PRIMARY_WRITER);

		if (nLeft > cachelimit ||
			getGangMaxVmem(gang) > gp_vmem_protect_gang_cache_limit)
		{
			disconnectAndDestroyGang(gang);
			gplist = list_delete_cell(gplist, cell, prevcell);
			nLeft--;

			if (prevcell != NULL)
				cell = lnext(prevcell);
			else
				cell = list_head(gplist);
		}
		else
		{
			prevcell = cell;
			cell = lnext(cell);
		}
	}

	return gplist;
}

/*
 * Portal drop... Clean up what gangs we hold
 */
void cleanupPortalGangs(Portal portal)
{
	MemoryContext oldContext;
	const char *portal_name;

	if (portal->name && strcmp(portal->name, "") != 0)
	{
		portal_name = portal->name;
		LOG_GANG_DEBUG(LOG, "cleanupPortalGangs %s", portal_name);
	}
	else
	{
		portal_name = NULL;
		LOG_GANG_DEBUG(LOG, "cleanupPortalGangs (unamed portal)");
	}

	if (GangContext)
		oldContext = MemoryContextSwitchTo(GangContext);
	else
		oldContext = MemoryContextSwitchTo(TopMemoryContext);

	availableReaderGangsN = cleanupPortalGangList(availableReaderGangsN, gp_cached_gang_threshold);
	availableReaderGangs1 = cleanupPortalGangList(availableReaderGangs1, MAX_CACHED_1_GANGS);

	LOG_GANG_DEBUG(LOG, "cleanupPortalGangs '%s'. Reader gang inventory: "
			"allocatedN=%d availableN=%d allocated1=%d available1=%d",
			(portal_name ? portal_name : "unnamed portal"),
			list_length(allocatedReaderGangsN),
			list_length(availableReaderGangsN),
			list_length(allocatedReaderGangs1),
			list_length(availableReaderGangs1));

	MemoryContextSwitchTo(oldContext);
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
void freeGangsForPortal(char *portal_name)
{
	MemoryContext oldContext;
	ListCell *cur_item = NULL;
	ListCell *prev_item = NULL;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	/*
	 * the primary writer gangs "belong" to the unnamed portal --
	 * if we have multiple active portals trying to release, we can't just
	 * release and re-release the writers each time !
	 */
	if (portal_name == NULL &&
		primaryWriterGang != NULL &&
		!cleanupGang(primaryWriterGang))
	{
		primaryWriterGang = NULL;
		disconnectAndDestroyAllGangs(true);

		elog(ERROR, "could not temporarily connect to one or more segments");
		return;
	}

	if(allocatedReaderGangsN == NULL && allocatedReaderGangs1 == NULL)
		return;

	/*
	 * Now we iterate through the list of allocated reader gangs
	 * and we free all the gangs that belong to the portal that
	 * was specified by our caller.
	 */
	LOG_GANG_DEBUG(LOG, "freeGangsForPortal '%s'. Reader gang inventory: "
			"allocatedN=%d availableN=%d allocated1=%d available1=%d",
			(portal_name ? portal_name : "unnamed portal"),
			list_length(allocatedReaderGangsN),
			list_length(availableReaderGangsN),
			list_length(allocatedReaderGangs1),
			list_length(availableReaderGangs1));

	Assert (GangContext != NULL);
	oldContext = MemoryContextSwitchTo(GangContext);

	cur_item = list_head(allocatedReaderGangsN);
	while (cur_item != NULL)
	{
		Gang *gp = (Gang *) lfirst(cur_item);
		ListCell *next_item = lnext(cur_item);

		if (isTargetPortal(gp->portal_name, portal_name))
		{
			LOG_GANG_DEBUG(LOG, "Returning a reader N-gang to the available list");

			/* cur_item must be removed */
			allocatedReaderGangsN = list_delete_cell(allocatedReaderGangsN,
					cur_item, prev_item);

			/* we only return the gang to the available list if it is good */
			if (cleanupGang(gp))
				availableReaderGangsN = lappend(availableReaderGangsN, gp);
			else
				disconnectAndDestroyGang(gp);

			cur_item = next_item;
		}
		else
		{
			LOG_GANG_DEBUG(LOG, "Skipping the release of a reader N-gang. It is used by another portal");

			/* cur_item must be preserved */
			prev_item = cur_item;
			cur_item = next_item;
		}
	}

	prev_item = NULL;
	cur_item = list_head(allocatedReaderGangs1);
	while (cur_item != NULL)
	{
		Gang *gp = (Gang *) lfirst(cur_item);
		ListCell *next_item = lnext(cur_item);

		if (isTargetPortal(gp->portal_name, portal_name))
		{
			LOG_GANG_DEBUG(LOG, "Returning a reader 1-gang to the available list");

			/* cur_item must be removed */
			allocatedReaderGangs1 = list_delete_cell(allocatedReaderGangs1,
					cur_item, prev_item);

			/* we only return the gang to the available list if it is good */
			if (cleanupGang(gp))
				availableReaderGangs1 = lappend(availableReaderGangs1, gp);
			else
				disconnectAndDestroyGang(gp);

			cur_item = next_item;
		}
		else
		{
			LOG_GANG_DEBUG(LOG, "Skipping the release of a reader 1-gang. It is used by another portal");

			/* cur_item must be preserved */
			prev_item = cur_item;
			cur_item = next_item;
		}
	}

	MemoryContextSwitchTo(oldContext);

	LOG_GANG_DEBUG(LOG,
			"Gangs released for portal '%s'. Reader gang inventory: "
					"allocatedN=%d availableN=%d allocated1=%d available1=%d",
			(portal_name ? portal_name : "unnamed portal"),
			list_length(allocatedReaderGangsN),
			list_length(availableReaderGangsN),
			list_length(allocatedReaderGangs1),
			list_length(availableReaderGangs1));
}

/*
 * Drop any temporary tables associated with the current session and
 * use a new session id since we have effectively reset the session.
 *
 * Call this procedure outside of a transaction.
 */
void CheckForResetSession(void)
{
	int oldSessionId = 0;
	int newSessionId = 0;
	Oid dropTempNamespaceOid;

	if (!NeedResetSession)
		return;

	/* Do the session id change early. */

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
			"The new session id = %d", oldSessionId, newSessionId);

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
		}PG_CATCH();
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
		}PG_END_TRY();
	}
}

static void resetSessionForPrimaryGangLoss(void)
{
	if (ProcCanSetMppSessionId())
	{
		/*
		 * Not too early.
		 */
		NeedResetSession = true;

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

/*
 * Helper functions
 */

int gp_pthread_create(pthread_t * thread, void *(*start_routine)(void *),
		void *arg, const char *caller)
{
	int pthread_err = 0;
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
	pthread_err = pthread_attr_setstacksize(&t_atts,
			Max(PTHREAD_STACK_MIN, (256 * 1024)));
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

static const char* gangTypeToString(GangType type)
{
	const char *ret = "";
	switch (type)
	{
	case GANGTYPE_PRIMARY_WRITER:
		ret = "primary writer";
		break;
	case GANGTYPE_PRIMARY_READER:
		ret = "primary reader";
		break;
	case GANGTYPE_SINGLETON_READER:
		ret = "singleton reader";
		break;
	case GANGTYPE_ENTRYDB_READER:
		ret = "entry DB reader";
		break;
	case GANGTYPE_UNALLOCATED:
		ret = "unallocated";
		break;
	default:
		Assert(false);
	}
	return ret;
}

bool gangOK(Gang *gp)
{
	int i;

	if (gp == NULL)
		return false;

	if (gp->gang_id < 1 ||
		gp->gang_id > 100000000 ||
		gp->type > GANGTYPE_PRIMARY_WRITER ||
		(gp->size != getgpsegmentCount() && gp->size != 1))
		return false;

	/*
	 * Gang is direct-connect (no agents).
	 */

	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = &(gp->db_descriptors[i]);

		if (cdbconn_isBadConnection(segdbDesc))
			return false;
	}

	return true;
}

bool gangsExist(void)
{
	return (primaryWriterGang != NULL ||
			allocatedReaderGangsN != NIL ||
			availableReaderGangsN != NIL ||
			allocatedReaderGangs1 != NIL||
			availableReaderGangs1 != NIL);
}

int largestGangsize(void)
{
	return largest_gangsize;
}

#ifdef USE_ASSERT_CHECKING
/**
 * Assert that slicetable is valid. Must be called after ExecInitMotion, which sets up the slice table
 */
void AssertSliceTableIsValid(SliceTable *st, struct PlannedStmt *pstmt)
{
	if (!st)
		return;

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
		Assert(s->sliceIndex == i++ && "slice index incorrect");

		/* The root index of a slice is either 0 or is a slice corresponding to an init plan */
		Assert((s->rootIndex == 0) || (s->rootIndex > st->nMotions && s->rootIndex < maxIndex));

		/* Parent slice index */
		if (s->sliceIndex == s->rootIndex)
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
	}
}

#endif
