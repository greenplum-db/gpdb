/*-------------------------------------------------------------------------
 *
 * cdb_dump.c
 *	  cdb_dump is a utility for dumping out a cdb cluster of postgres databases
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/bin/pg_dump/cdb/cdb_dump.c
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <pthread.h>
#include "getopt_long.h"
#include "dumputils.h"
#include "fe-auth.h"
#include "cdb_table.h"
#include "cdb_dump_util.h"
#include "cdb_backup_state.h"
#include "cdb_dump.h"
#include "cdb_dump_include.h"
#include <poll.h>
#include <time.h>

/* This is necessary on platforms where optreset variable is not available.
 * Look at "man getopt" documentation for details.
 */
#ifndef HAVE_OPTRESET
int			optreset;
#endif

/*
 * static helper functions.  See each function body for a description.
 */
static char *addPassThroughParm(char Parm, const char *pszValue, char *pszPassThroughParmString);
static char *addPassThroughLongParm(const char *Parm, const char *pszValue, char *pszPassThroughParmString);
static bool createThreadParmArray(int nCount, ThreadParmArray * pParmAr);
static void decrementFinishedLaunchCount(void);
static void decrementFinishedLockingCount(void);
static bool startTransactionAndLock(PGconn *pConn);
static bool execCommit(PGconn *pConn);
static char* getRelativeFileName(char* longFileName);
static void addFileNameParam(const char* flag, char* file_name, InputOptions* pInputOpts);
static bool fillInputOptions(int argc, char **argv, InputOptions * pInputOpts);
static void freeThreadParmArray(ThreadParmArray * pParmAr);
static void help(const char *progname);
static void installSignalHandlers(void);
static void myHandler(SIGNAL_ARGS);
static bool parmValNeedsQuotes(const char *Value);
static int	reportBackupResults(InputOptions inputopts, ThreadParmArray * pParmAr);
static void spinOffThreads(PGconn *pConn, InputOptions * pInputOpts, const SegmentDatabaseArray *psegDBAr,
			   ThreadParmArray * pParmAr);
static void *threadProc(void *arg);
static bool testPartitioningSupport(void);
static void transformPassThroughParms(InputOptions * pInputOpts);
static bool copyFilesToSegments(InputOptions * pInputOpts, SegmentDatabaseArray *segDBAr);
static int	getRemoteVersion(void);
static bool no_expand_children; /* Do not expand child partitions. This option is passed from gpcrondump */

/*
 * static and extern global variables left over from pg_dump
 */
extern char *optarg;
extern int	optind,
			opterr;
static int dump_inserts;		/* dump data using proper insert strings */
static int column_inserts;			/* put attr names into insert strings */
static int max_probe_retries = 10;
static int probe_interval = 60; 	/* 1 minute interval */
static char *dumpencoding = NULL;
static bool schemaOnly;
static bool incremental_backup;
static char *incremental_filter = NULL;
static bool dataOnly;
static bool aclsSkip;
static char *selectTableName = NULL;	/* name of a single table to dump */
static char *selectSchemaName = NULL;	/* name of a single schema to dump */

static char *tableFileName = NULL;	/* file name with tables to dump (--table-file)	*/
static char *schemaFileName = NULL;	/* file name with tables to dump (--schema-file)	*/
static char *excludeTableFileName = NULL; /* file name with tables to exclude (--exclude-table-file) */
static bool g_is_old_format = false;

/* NetBackup related variables */
static char *netbackup_service_host = NULL;
static char *netbackup_policy = NULL;
static char *netbackup_schedule = NULL;
static char *netbackup_block_size = NULL;
static char *netbackup_keyword = NULL;

char		g_comment_start[10];
char		g_comment_end[10];
static bool bForcePassword = false;
static bool bIgnoreVersion = false;
static bool no_lock = false;
static int  rsyncable = false;

static const char *logInfo = "INFO";
static const char *logWarn = "WARN";
static const char *logError = "ERROR";
static const char *logFatal = "FATAL";

/*
 * Global variables needed by cdb_dump_include routines.
 */

PGconn	   *g_conn;				/* the database connection */
const char *progname;			/* the program name */
bool		g_verbose;			/* User wants verbose narration of our
								 * activities. */

/* default, if no "inclusion" switches appear, is to dump everything */
static bool include_everything = true;

/*
 * Thread synchronization variables
 */
static int	nThreadsFinishedLaunch;
static int	nThreadsFinishedLocking;
static pthread_cond_t MyCondVar = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t MyMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t MyCondVar2 = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t MyMutex2 = PTHREAD_MUTEX_INITIALIZER;
static bool g_b_SendCancelMessage = false;
static const char *pszAgent = "gp_dump_agent";

PQExpBuffer dump_prefix_buf = NULL;

#ifdef USE_DDBOOST
#include "ddp_api.h"
static int dd_boost_enabled = 0;
static char *ddboost_storage_unit = NULL;
#endif

int
main(int argc, char **argv)
{
	int			rc = 0;			/* return code */
	int			remote_version;
	dump_prefix_buf = createPQExpBuffer();

	InputOptions inputOpts;		/* command line parameters */
	SegmentDatabaseArray segDBAr;		/* array of the segdbs from the master
										 * mpp tables */
	ThreadParmArray parmAr;		/* array of the thread parameters */
	PGconn	   *master_db_conn = NULL;

	memset(&inputOpts, 0, sizeof(inputOpts));
	memset(&segDBAr, 0, sizeof(segDBAr));
	memset(&parmAr, 0, sizeof(parmAr));

	/* Parse command line for options */
	if (!fillInputOptions(argc, argv, &inputOpts))
		goto cleanup;

	mpp_msg(logInfo, progname, "Command line options analyzed.\n");

	/* Connect to database on the master */
	mpp_msg(logInfo, progname, "Connecting to master database on host %s port %s database %s.\n",
			StringNotNull(inputOpts.pszPGHost, "localhost"),
			StringNotNull(inputOpts.pszPGPort, "5432"),
			StringNotNull(inputOpts.pszDBName, "?"));

	master_db_conn = g_conn = GetMasterConnection(progname, inputOpts.pszDBName, inputOpts.pszPGHost,
								  inputOpts.pszPGPort, inputOpts.pszUserName,
									  bForcePassword, bIgnoreVersion, false);
	if (master_db_conn == NULL)
		goto cleanup;

	remote_version = getRemoteVersion();

	if (!remote_version)
		goto cleanup;

	mpp_msg(logInfo, progname, "Reading Greenplum Database configuration info from master database.\n");
	if (!GetDumpSegmentDatabaseArray(master_db_conn, remote_version, &segDBAr, inputOpts.actors,
					inputOpts.pszRawDumpSet, inputOpts.pszDBName, inputOpts.pszUserName, dataOnly, schemaOnly))
		goto cleanup;

	/*
	 * Process the schema and table include/exclude lists and determine the
	 * OIDs of the tables to be dumped.
	 */
	if (!incremental_backup)
		convert_patterns_to_oids();

	/*
	 * make changes and verifications (if any) needed to be made before
	 * shipping options to the agents
	 */
	transformPassThroughParms(&inputOpts);

	/*
	 * Copy necessary files to segments (from --table-file / -- exclude-table-file options)
	 * Files will be copied to the same location on all segments: /tmp/filename
	 * If a file with the same name exists on a segment, it will be overridden.
	 *
	 */
	if (!copyFilesToSegments(&inputOpts, &segDBAr))
		goto cleanup;

	/* Start a transaction (to be released in spinOffThreads)  */
	if (!no_lock) {
	    if (!startTransactionAndLock(master_db_conn))
		    goto cleanup;
	}

	/* Install the SIGINT and SIGTERM handlers */
	installSignalHandlers();

	/*
	 * Create the threads to spawn local segment dumps using gp_dump_agent.
	 * Inside this function we release the lock taken in
	 * startTransactionAndLock. When this function returns, all the agents
	 * have completed operation.
	 */
	spinOffThreads(master_db_conn, &inputOpts, &segDBAr, &parmAr);

	/* Clean-up memory allocated */
cleanup:

	/* Produce results report */
	rc = reportBackupResults(inputOpts, &parmAr);

	freeThreadParmArray(&parmAr);
	FreeSegmentDatabaseArray(&segDBAr);
	FreeInputOptions(&inputOpts);

	/* Clean-up synchronization variables */
	pthread_mutex_destroy(&MyMutex);
	pthread_cond_destroy(&MyCondVar);
	pthread_mutex_destroy(&MyMutex2);
	pthread_cond_destroy(&MyCondVar2);

	if (master_db_conn != NULL)
		PQfinish(master_db_conn);

	destroyPQExpBuffer(dump_prefix_buf);

	exit(rc);
}

/*
 * addPassThroughParm: this function adds to the string of pass-through parameters.
 * These get sent to each backend and get passed to the gp_dump_agent program.
 */
static char *
addPassThroughParm(char Parm, const char *pszValue, char *pszPassThroughParmString)
{
	char	   *pszRtn;
	bool		bFirstTime = (pszPassThroughParmString == NULL);

	if (pszValue != NULL)
	{
		if (parmValNeedsQuotes(pszValue))
		{
			PQExpBuffer valueBuf = createPQExpBuffer();

			if (bFirstTime)
				pszRtn = MakeString("-%c \"%s\"", Parm, shellEscape(pszValue, valueBuf, false, false));
			else
				pszRtn = MakeString("%s -%c \"%s\"", pszPassThroughParmString, Parm, shellEscape(pszValue, valueBuf, false, false));

			destroyPQExpBuffer(valueBuf);
		}
		else
		{
			if (bFirstTime)
				pszRtn = MakeString("-%c %s", Parm, pszValue);
			else
				pszRtn = MakeString("%s -%c %s", pszPassThroughParmString, Parm, pszValue);
		}
	}
	else
	{
		if (bFirstTime)
			pszRtn = MakeString("-%c", Parm);
		else
			pszRtn = MakeString("%s -%c", pszPassThroughParmString, Parm);
	}

	return pszRtn;
}


/*
 * addPassThroughLongParm: this function adds a long option to the string of pass-through parameters.
 * These get sent to each backend and get passed to the gp_dump_agent program.
 */
static char *
addPassThroughLongParm(const char *Parm, const char *pszValue, char *pszPassThroughParmString)
{
	char	   *pszRtn;
	bool		bFirstTime = (pszPassThroughParmString == NULL);

	if (pszValue != NULL)
	{
		if (parmValNeedsQuotes(pszValue))
		{
			PQExpBuffer valueBuf = createPQExpBuffer();

			if (bFirstTime)
				pszRtn = MakeString("--%s \"%s\"", Parm, shellEscape(pszValue, valueBuf, false, false));
			else
				pszRtn = MakeString("%s --%s \"%s\"", pszPassThroughParmString, Parm, shellEscape(pszValue, valueBuf, false, false));

			destroyPQExpBuffer(valueBuf);
		}
		else
		{
			if (bFirstTime)
				pszRtn = MakeString("--%s %s", Parm, pszValue);
			else
				pszRtn = MakeString("%s --%s %s", pszPassThroughParmString, Parm, pszValue);
		}
	}
	else
	{
		if (bFirstTime)
			pszRtn = MakeString("--%s", Parm);
		else
			pszRtn = MakeString("%s --%s", pszPassThroughParmString, Parm);
	}

	return pszRtn;
}

/*
 * Given an input option -t (include table) or -T (exclude table) check if this is
 * a parent table of a partitioned table and if it is, add its children to the list
 * as well (see MPP-7540).
 */
static void
addChildrenToPassThrough(InputOptions *pInputOpts, char opt, SimpleOidList list)
{
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	SimpleOidListCell *cell;
	int			i;

	/* if table oids were found, add children for all parent tables */
	if (list.head != NULL)
	{
		for (cell = list.head; cell; cell = cell->next)
		{
			appendPQExpBuffer(query, "SELECT p.partitiontablename "
						  "FROM pg_partitions p, pg_class c, pg_namespace n "
							  "WHERE p.partitionschemaname = n.nspname "
							  "AND n.oid = c.relnamespace "
							  "AND c.relname = p.tablename "
							  "AND c.oid ='%u'::pg_catalog.oid;", cell->val);

			res = PQexec(g_conn, query->data);
			check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

			for (i = 0; i < PQntuples(res); i++)
			{
				char	   *childname = PQgetvalue(res, i, 0);

				pInputOpts->pszPassThroughParms = addPassThroughParm(opt, childname, pInputOpts->pszPassThroughParms);
			}

			PQclear(res);
			resetPQExpBuffer(query);
		}
	}

	destroyPQExpBuffer(query);
}

/*
 * Any transformation of the input options that is passed down to the segdb
 * dump agents should be done in here.
 *
 * For now, there's only a partitioning related transformation, see
 * addChildrenToPassThrough for more information.
 */
static void
transformPassThroughParms(InputOptions *pInputOpts)
{

	g_gp_supportsPartitioning = testPartitioningSupport();

	if (shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children))
	{
		addChildrenToPassThrough(pInputOpts, 't', table_include_oids);
		addChildrenToPassThrough(pInputOpts, 'T', table_exclude_oids);
	}
}

/*
 * Copy --table-file and --exclude-table-file files
 * to all segments.
 *
 * returns 'false' if error was reported. 'true' otherwise.
 */
static bool
copyFilesToSegments(InputOptions *pInputOpts, SegmentDatabaseArray *segDBAr)
{

	if ((tableFileName == NULL) && (excludeTableFileName == NULL))
		return true; /* nothing to do */

	/* get destination list */
	const char *hostfile_name = "/tmp/gp_dump_hostfilelist.txt";

	FILE* hostfile = fopen(hostfile_name, "w");
	if (hostfile == NULL)
	{
		mpp_err_msg_cache(logError, progname, "Can't open hostfile file %s",
				  	  	  hostfile_name);
		return false;
	}

	for (int i = 0; i < segDBAr->count; ++i)
	{
		mpp_msg(logInfo, progname, "Writing to hostfile: line #%d, segment %s@%s:%d\n",
				i,
				segDBAr->pData[i].pszDBUser,
				segDBAr->pData[i].pszHost,
				segDBAr->pData[i].port);
		fprintf(hostfile, "%s\n", segDBAr->pData[i].pszHost);

	}
	fclose(hostfile);

	/* run gpscp to copy file(s) to all segments */
	char *cmdLine = MakeString("gpscp -f %s %s %s =:/tmp/.",
							   hostfile_name,
							   StringNotNull(tableFileName, ""),
							   StringNotNull(excludeTableFileName, ""));


	mpp_msg(logInfo, progname,  "Copying table lists files with gpscp cmd: %s\n", cmdLine);
	/* This execs a shell that runs the gpscp program */
	int result = system(cmdLine);

	mpp_msg(logInfo, progname, "Finished running gpscp cmd");

	/* delete hostfile name */
	unlink(hostfile_name);
	free(cmdLine);

	if (result == -1)
	{
		mpp_err_msg_cache(logError, progname, "failed to send table files to segments (%d, %d)",
						  result, errno);
		return false;
	}

	return true;
}


/*
 * createThreadParmArray: This function initializes the count and pData elements of the ThreadParmArray.
 */
bool
createThreadParmArray(int nCount, ThreadParmArray * pParmAr)
{
	int			i;

	pParmAr->count = nCount;
	pParmAr->pData = (ThreadParm *) calloc(nCount, sizeof(ThreadParm));
	if (pParmAr->pData == NULL)
		return false;

	for (i = 0; i < nCount; i++)
	{
		ThreadParm *p = &pParmAr->pData[i];

		p->thread = 0;
		p->pSourceSegDBData = NULL;		/* not used for dump */
		p->pTargetSegDBData = NULL;
		p->pOptionsData = NULL;
		p->bSuccess = true;
		p->pszErrorMsg = NULL;
		p->pszRemoteBackupPath = NULL;
	}

	return true;
}

/*
 * decrementFinishedLaunchCount: This function handles the decrementing
 * of the finished launch count. It uses a mutex to protect the int variable.
 * If this is the last thread to get here, the variable will be 0, and the
 * condition variable is signaled, so the main thread wakes up.
 */
void
decrementFinishedLaunchCount(void)
{
	pthread_mutex_lock(&MyMutex);

	nThreadsFinishedLaunch--;

	if (nThreadsFinishedLaunch == 0)
		pthread_cond_signal(&MyCondVar);

	pthread_mutex_unlock(&MyMutex);
}

void
decrementFinishedLockingCount(void)
{
	pthread_mutex_lock(&MyMutex2);

	nThreadsFinishedLocking--;

	if (nThreadsFinishedLocking == 0)
		pthread_cond_signal(&MyCondVar2);

	pthread_mutex_unlock(&MyMutex2);
}

/*
 * startTransactionAndLock: This function uses the PGconn connection to begin
 * a transaction. After opening the transaction, we grab a lock on pg_class
 *
 * The locks obtained by this routine are released through COMMIT by
 * spinOffThreads().
 */
bool
startTransactionAndLock(PGconn *pMasterConn)
{
	bool		bRtn = false;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;

	mpp_msg(logInfo, progname, "Starting a transaction on master database %s.\n", pMasterConn->dbName);

	pQry = createPQExpBuffer();
	if (pQry == NULL)
	{
		mpp_err_msg_cache(logError, progname, "could not create PQExpBuffer");
		goto cleanup;
	}

	appendPQExpBuffer(pQry, "BEGIN;");
	pRes = PQexec(pMasterConn, pQry->data);
	if (pRes == NULL)
	{
		mpp_err_msg_cache(logError, progname, "no result from server: %s", PQerrorMessage(pMasterConn));
		goto cleanup;
	}

	if (PQresultStatus(pRes) != PGRES_COMMAND_OK)
	{
		mpp_err_msg_cache(logError, progname, "bad result from server: %s", PQerrorMessage(pMasterConn));
		goto cleanup;
	}

	PQclear(pRes);
	pRes = NULL;
	resetPQExpBuffer(pQry);

	/*
	 * Now grab a lock on pg_class on the master to avoid colliding with
	 * concurrent transaction DROP/CREATE. This lock will later be release
	 * when COMMIT is run.
	 */
	mpp_msg(logInfo, progname, "Getting a lock on pg_class in database %s.\n", pMasterConn->dbName);
	appendPQExpBuffer(pQry, "LOCK TABLE pg_catalog.pg_class IN EXCLUSIVE MODE;");
	pRes = PQexec(pMasterConn, pQry->data);
	if (pRes == NULL)
	{
		mpp_err_msg_cache(logError, progname, "no result from server: %s", PQerrorMessage(pMasterConn));
		goto cleanup;
	}
	if (PQresultStatus(pRes) != PGRES_COMMAND_OK)
	{
		mpp_err_msg_cache(logError, progname, "bad result from server: %s", PQerrorMessage(pMasterConn));
		goto cleanup;
	}

	bRtn = true;

cleanup:

	if (pRes != NULL)
		PQclear(pRes);
	if (pQry != NULL)
		destroyPQExpBuffer(pQry);

	return bRtn;
}


/*
 * testPartitioningSupport - tests whether or not the current GP
 * database includes support for partitioning.
 */
static bool
testPartitioningSupport(void)
{
	PQExpBuffer query;
	PGresult   *res;
	bool		isSupported;

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT 1 FROM pg_class WHERE relname = 'pg_partition' and relnamespace = 11;");
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	isSupported = (PQntuples(res) == 1);

	PQclear(res);
	destroyPQExpBuffer(query);

	return isSupported;
}

static int
getRemoteVersion(void)
{
	const char *remoteversion_str;
	int			remoteversion;

	remoteversion_str = PQparameterStatus(g_conn, "server_version");
	if (!remoteversion_str)
		return 0;				/* error */

	remoteversion = parse_version(remoteversion_str);

	return remoteversion;
}

/*
 * execCommit: This function simply issues a commit command using the pConn PGconn* parameter.
 */
bool
execCommit(PGconn *pConn)
{
	bool		bRtn = false;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;

	pQry = createPQExpBuffer();
	if (pQry == NULL)
	{
		mpp_err_msg_cache(logError, progname, "could not create PQExpBuffer");
		goto cleanup;
	}

	appendPQExpBuffer(pQry, " COMMIT");

	pRes = PQexec(pConn, pQry->data);
	if (pRes == NULL)
	{
		mpp_err_msg_cache(logError, progname, "no result from server: %s\n", PQerrorMessage(pConn));
		goto cleanup;
	}

	if (PQresultStatus(pRes) != PGRES_COMMAND_OK)
	{
		mpp_err_msg_cache(logError, progname, "bad result from server: %s\n", PQerrorMessage(pConn));
		goto cleanup;
	}

	bRtn = true;

cleanup:
	if (pRes != NULL)
		PQclear(pRes);
	if (pQry != NULL)
		destroyPQExpBuffer(pQry);

	return bRtn;
}


char* getRelativeFileName(char* longFileName)
{

	char *slash = strrchr(longFileName, '/');
	if (slash == NULL)
		return longFileName;

	/* return file name from after last '/' */
	return slash + 1;

}

/*
 * extract file name without path from original file_name,
 * append to a directory_name,
 * and add to to passed params with given flag option.
 */
void addFileNameParam(const char* flag, char* file_name, InputOptions* pInputOpts)
{
	static char* short_file_name = NULL;
	static char* tmp_name = NULL;
	static char* directory_name = "/tmp/";

	short_file_name = getRelativeFileName(file_name);
	tmp_name = MakeString("%s%s", directory_name, short_file_name);
	pInputOpts->pszPassThroughParms =
			addPassThroughLongParm(flag, tmp_name, pInputOpts->pszPassThroughParms);
	tableFileName = pg_strdup(file_name);
	free(tmp_name);
}

/*
 * fillInputOptions: This function gets the command line options, and
 * stores the values in the InputOptions structure. Much of this code
 * was copied from pg_dump.
 */
bool
fillInputOptions(int argc, char **argv, InputOptions * pInputOpts)
{
	bool		bRtn = false;
	int			c;
	bool		bOids = false;
	int			outputClean = 0;
	int			outputNoOwner = 0;
	static int	disable_triggers = 0;
	static int	auth = 0;
	static int	skipcat = 0;
	int			lenCmdLineParms;
	int			i;

#ifdef USE_DDBOOST
	char		*ddboost_directory = NULL;
#endif

	static struct option long_options[] =
	{
		{"data-only", no_argument, NULL, 'a'},
		{"clean", no_argument, NULL, 'c'},
		{"encoding", required_argument, NULL, 'E'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"no-reconnect", no_argument, NULL, 'R'},
		{"oids", no_argument, NULL, 'o'},
		{"no-owner", no_argument, NULL, 'O'},
		{"port", required_argument, NULL, 'p'},
		{"schema", required_argument, NULL, 'n'},
		{"exclude-schema", required_argument, NULL, 'N'},
		{"schema-only", no_argument, NULL, 's'},
		{"superuser", required_argument, NULL, 'S'},
		{"table", required_argument, NULL, 't'},
		{"exclude-table", required_argument, NULL, 'T'},
		{"password", no_argument, NULL, 'W'},
		{"username", required_argument, NULL, 'U'},
		{"verbose", no_argument, NULL, 'v'},
		{"no-privileges", no_argument, NULL, 'x'},
		{"no-acl", no_argument, NULL, 'x'},
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'V'},

		/*
		 * the following options don't have an equivalent short option letter
		 */
		{"use-set-session-authorization", no_argument, &auth, 1},
		{"attribute-inserts", no_argument, &column_inserts, 1},
		{"column-inserts", no_argument, &column_inserts, 1},
		{"disable-triggers", no_argument, &disable_triggers, 1},
		{"inserts", no_argument, &dump_inserts, 1},
		{"gp-catalog-skip", no_argument, &skipcat, 1},

		/*
		 * the following are Greenplum Database specific, and don't have an
		 * equivalent short option
		 */
		{"gp-c", no_argument, NULL, 1},
		{"gp-d", required_argument, NULL, 3},
		{"gp-r", required_argument, NULL, 4},
		{"gp-s", required_argument, NULL, 5},
		{"rsyncable", no_argument, &rsyncable, 1},

#ifdef USE_DDBOOST
		{"ddboost", no_argument, NULL, 6},
		{"ddboost-storage-unit", required_argument, NULL, 21},
#endif
		{"table-file", required_argument, NULL, 7},
		{"exclude-table-file", required_argument, NULL, 8},
		{"incremental", no_argument, NULL, 9},
		{"gp-k", required_argument, NULL, 10},
		{"no-expand-children", no_argument, NULL, 11},
		{"prefix", required_argument, NULL, 12},
		{"incremental-filter", required_argument, NULL, 13},
		{"no-lock", no_argument, NULL, 14},
		{"netbackup-service-host", required_argument, NULL, 15},
		{"netbackup-policy", required_argument, NULL, 16},
		{"netbackup-schedule", required_argument, NULL, 17},
		{"netbackup-block-size", required_argument, NULL, 18},
		{"netbackup-keyword", required_argument, NULL, 19},
		{"schema-file", required_argument, NULL, 20},
		{"old-format", no_argument, NULL, 22},

		{NULL, 0, NULL, 0}
	};

	int			optindex;

	/* Initialize option fields  */
	memset(pInputOpts, 0, sizeof(InputOptions));

	pInputOpts->actors = SET_NO_MIRRORS;		/* backup primaries only by
												 * default */
	g_verbose = false;

	strcpy(g_comment_start, "-- ");
	g_comment_end[0] = '\0';


	dataOnly = schemaOnly = incremental_backup = dump_inserts = column_inserts = no_expand_children = false;

	progname = (char *) get_progname(argv[0]);

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("gp_dump (Greenplum Database) " GP_VERSION);
			exit(0);
		}
	}

	/*
	 * Record the command line parms as a string for documentation in the
	 * ouput report
	 */
	lenCmdLineParms = 0;
	for (i = 1; i < argc; i++)
	{
		if (i > 1)
			lenCmdLineParms += 1;

		lenCmdLineParms += strlen(argv[i]);
		if (parmValNeedsQuotes(argv[i]))
			lenCmdLineParms += 2;
	}

	pInputOpts->pszCmdLineParms = (char *) malloc(lenCmdLineParms + 1);
	if (pInputOpts->pszCmdLineParms == NULL)
	{
		mpp_err_msg_cache(logError, progname, "error allocating memory for pInputOpts->pszCmdLineParms\n");
		goto cleanup;
	}

	memset(pInputOpts->pszCmdLineParms, 0, lenCmdLineParms + 1);
	for (i = 1; i < argc; i++)
	{
		if (i > 1)
			strcat(pInputOpts->pszCmdLineParms, " ");
		if (parmValNeedsQuotes(argv[i]))
		{
			strcat(pInputOpts->pszCmdLineParms, "\"");
			strcat(pInputOpts->pszCmdLineParms, argv[i]);
			strcat(pInputOpts->pszCmdLineParms, "\"");
		}
		else
			strcat(pInputOpts->pszCmdLineParms, argv[i]);
	}


	while ((c = getopt_long(argc, argv, "acdDE:h:in:N:oOp:RsS:t:T:U:vWxX:",
							long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 'a':			/* Dump data only */
				dataOnly = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'c':			/* clean (i.e., drop) schema prior to create */
				outputClean = 1;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'd':			/* dump data as proper insert strings */
				dump_inserts = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				fprintf(stderr," --inserts is preferred over -d.  -d is deprecated.\n");
				break;

			case 'D':			/* dump data as proper insert strings with
								 * attr names */
				dump_inserts = true;
				column_inserts = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				fprintf(stderr," --column-inserts is preferred over -D.  -D is deprecated.\n");
				break;

			case 'E':			/* Dump encoding */
				dumpencoding = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				break;

			case 'h':			/* server host */
				pInputOpts->pszPGHost = pg_strdup(optarg);
				break;

			case 'i':			/* ignore database version mismatch */
				bIgnoreVersion = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'n':			/* Dump data for this schema only */
				selectSchemaName = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				simple_string_list_append(&schema_include_patterns, optarg);
				include_everything = false;
				break;

			case 'N':			/* exclude schema(s) */
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				simple_string_list_append(&schema_exclude_patterns, optarg);
				break;


			case 'o':			/* Dump oids */
				bOids = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'O':			/* Don't reconnect to match owner */
				outputNoOwner = 1;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'p':			/* server port */
				pInputOpts->pszPGPort = pg_strdup(optarg);
				break;

			case 'R':
				/* no-op, still accepted for backwards compatibility */
				break;

			case 's':			/* dump schema only */
				schemaOnly = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'S':			/* Superuser username */
				if (strlen(optarg) != 0)
					pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				break;

			case 't':			/* Dump data for this table only */
				selectTableName = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				simple_string_list_append(&table_include_patterns, optarg);
				include_everything = false;
				break;

			case 'T':			/* exclude table(s) */
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, optarg, pInputOpts->pszPassThroughParms);
				simple_string_list_append(&table_exclude_patterns, optarg);
				break;

			case 'u':
				bForcePassword = true;
				pInputOpts->pszUserName = simple_prompt("User name: ", 100, true);
				break;

			case 'U':
				pInputOpts->pszUserName = pg_strdup(optarg);
				break;

			case 'v':			/* verbose */
				g_verbose = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'W':
				bForcePassword = true;
				break;

			case 'x':			/* skip ACL dump */
				aclsSkip = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

				/*
				 * Option letters were getting scarce, so I invented this new
				 * scheme: '-X feature' turns on some feature. Compare to the
				 * -f option in GCC.  You should also add an equivalent
				 * GNU-style option --feature.	Features that require
				 * arguments should use '-X feature=foo'.
				 */
			case 'X':
				if (strcmp(optarg, "use-set-session-authorization") == 0)
					auth = 1;
				else if (strcmp(optarg, "disable-triggers") == 0)
					disable_triggers = 1;
				else if (strcmp(optarg, "gp-catalog-skip") == 0)
					skipcat = 1;
				else
				{
					fprintf(stderr,
							("%s: invalid -X option -- %s\n"),
							progname, optarg);
					fprintf(stderr, ("Try \"%s --help\" for more information.\n"), progname);
					goto cleanup;
				}
				break;

			case 0:
				/* This covers the long options equivalent to -X xxx. */
				break;

			case 1:
				/* gp-c remote compression program */
				pInputOpts->pszCompressionProgram = "gzip";		/* pg_strdup(optarg); */
				break;

			case 2:
				break;

			case 3:
				/* gp-d backup remote directory */
				pInputOpts->pszBackupDirectory = pg_strdup(optarg);
				break;

			case 4:
				/* gp-r report directory */
				pInputOpts->pszReportDirectory = pg_strdup(optarg);
				break;

			case 5:
				/* gp-s backup set specification */
				if (strcasecmp(optarg, "p") == 0)
				{
					pInputOpts->actors = SET_NO_MIRRORS;
					pInputOpts->pszRawDumpSet = NULL;
				}
				else if (strncasecmp(optarg, "i", 1) == 0)
				{
					pInputOpts->actors = SET_INDIVIDUAL;
					pInputOpts->pszRawDumpSet = pg_strdup(optarg);
				}
				else
				{
					mpp_err_msg_cache(logError, progname, "invalid gp-s option %s.  Must be p for \"all primary segments\", or i[dbid list] for \"individual\".\n", optarg);
					goto cleanup;
				}
				break;
#ifdef USE_DDBOOST
			case 6:
				dd_boost_enabled = 1;
				break;
			case 21:
				ddboost_storage_unit = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("ddboost-storage-unit", ddboost_storage_unit, pInputOpts->pszPassThroughParms);
				break;
#endif
			case 7:
				/* table-file option */
				if (tableFileName != NULL)
				{
					mpp_err_msg_cache(logError, progname, "Can't use --table-file option more than once\n");
					goto cleanup;
				}

				if (!open_file_and_append_to_list(optarg, &table_include_patterns, "include tables list"))
				{
					mpp_err_msg_cache(logError, progname, "can't open file %s. Invalid --table-file parameter\n",
							          optarg);
					goto cleanup;
				}

				/* extract file name without path, and add it to passed params */
				addFileNameParam("table-file", optarg, pInputOpts);
				include_everything = false;
				break;

			case 8:
				/* exclude-table-file option */
				if (excludeTableFileName != NULL)
				{
					mpp_err_msg_cache(logError, progname, "Can't use --exclude-table-file option more than once\n");
					goto cleanup;
				}

				if (!open_file_and_append_to_list(optarg, &table_exclude_patterns, "exclude tables list"))
				{
					mpp_err_msg_cache(logError, progname, "can't open file %s. Invalid --exclude-table-file parameter\n",
							          optarg);
					goto cleanup;
				}

				/* extract file name without path, and add it to passed params */
				addFileNameParam("exclude-table-file", optarg, pInputOpts);

				break;

			case 9:
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("incremental", NULL, pInputOpts->pszPassThroughParms);
				incremental_backup = true;
				break;

			case 10:
				pInputOpts->pszTimestampKey = pg_strdup(optarg);
				if (!ValidateTimestampKey(pInputOpts->pszTimestampKey)) {
					mpp_err_msg_cache(logError, progname, "Invalid timestamp key provided\n");
					goto cleanup;
				}
				break;

			case 11:
				no_expand_children = true;
				break;
			case 12:
				appendPQExpBuffer(dump_prefix_buf, "%s", optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("prefix", dump_prefix_buf->data, pInputOpts->pszPassThroughParms);
				break;
			case 13:
				incremental_filter = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("incremental-filter", incremental_filter, pInputOpts->pszPassThroughParms);
				break;
			case 14:
				no_lock = true;
				break;
			case 15:
				netbackup_service_host = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-service-host", netbackup_service_host, pInputOpts->pszPassThroughParms);
				break;
			case 16:
				netbackup_policy = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-policy", netbackup_policy, pInputOpts->pszPassThroughParms);
				break;
			case 17:
				netbackup_schedule = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-schedule", netbackup_schedule, pInputOpts->pszPassThroughParms);
				break;
			case 18:
				netbackup_block_size = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-block-size", netbackup_block_size, pInputOpts->pszPassThroughParms);
				break;
			case 19:
				netbackup_keyword = pg_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-keyword", netbackup_keyword, pInputOpts->pszPassThroughParms);
				break;
			case 20:
				/* schema-file option */
				if (schemaFileName != NULL)
				{
					mpp_err_msg_cache(logError, progname, "Can't use --schema-file option more than once\n");
					goto cleanup;
				}

				if (!open_file_and_append_to_list(optarg, &schema_include_patterns, "include schema list"))
				{
					mpp_err_msg_cache(logError, progname, "can't open file %s. Invalid --schema-file parameter\n",
							optarg);
					goto cleanup;
				}

				/* extract file name without path, and add it to passed params */
				addFileNameParam("schema-file", optarg, pInputOpts);
				include_everything = false;
				break;
			case 22:
				g_is_old_format = true;
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("old-format", NULL, pInputOpts->pszPassThroughParms);
				break;
			default:
				mpp_err_msg_cache(logError, progname, "Try \"%s --help\" for more information.\n", progname);
				goto cleanup;
		}
	}

	if (optind < argc - 1)
	{
		fprintf(stderr, ("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind + 1]);
		fprintf(stderr, ("Try \"%s --help\" for more information.\n"),
				progname);
		goto cleanup;
	}

	 /* Add interesting long options to the passthrough arguments */
	if (auth)
		pInputOpts->pszPassThroughParms = addPassThroughLongParm("use-set-session-authorization", NULL, pInputOpts->pszPassThroughParms);
	if (disable_triggers)
		pInputOpts->pszPassThroughParms = addPassThroughLongParm("disable-triggers", NULL, pInputOpts->pszPassThroughParms);
	if (skipcat)
		pInputOpts->pszPassThroughParms = addPassThroughLongParm("gp-catalog-skip", NULL, pInputOpts->pszPassThroughParms);
	if (rsyncable)
		pInputOpts->pszPassThroughParms = addPassThroughLongParm("rsyncable", NULL, pInputOpts->pszPassThroughParms);
#ifdef USE_DDBOOST
	if (dd_boost_enabled)
	{
		pInputOpts->pszPassThroughParms = addPassThroughLongParm("dd_boost_enabled", NULL, pInputOpts->pszPassThroughParms);

		/* If no directory is specified, for example when we gp_dump, then dump to default directory db_dumps */
		if (pInputOpts->pszBackupDirectory)
			ddboost_directory = pg_strdup(pInputOpts->pszBackupDirectory);
		else
			ddboost_directory = pg_strdup("db_dumps/");

		pInputOpts->pszPassThroughParms = addPassThroughLongParm("dd_boost_dir", ddboost_directory, pInputOpts->pszPassThroughParms);
		if (ddboost_directory)
			free(ddboost_directory);
	}
#endif

	 /* Get database name from command line */
	if (optind < argc)
		pInputOpts->pszDBName = pg_strdup(argv[optind]);

	 /*
	  * get PG env variables, override only of no cmd-line value specified
	  */
	if (pInputOpts->pszDBName == NULL)
	{
		if (getenv("PGDATABASE") != NULL)
			pInputOpts->pszDBName = pg_strdup(getenv("PGDATABASE"));
	}

	if (pInputOpts->pszPGPort == NULL)
	{
		if (getenv("PGPORT") != NULL)
			pInputOpts->pszPGPort = pg_strdup(getenv("PGPORT"));
	}

	if (pInputOpts->pszPGHost == NULL)
	{
		if (getenv("PGHOST") != NULL)
			pInputOpts->pszPGHost = pg_strdup(getenv("PGHOST"));
	}

	 /*
	  * Command line options error checking
	  */
	if (pInputOpts->pszDBName == NULL)
	{
		mpp_err_msg_cache(logError, progname, "No database name specified for dump, and PGDATABASE is not set either.\n");
		goto cleanup;
	}

	if (dataOnly && schemaOnly)
	{
		mpp_err_msg_cache(logError, progname, "options \"schema only\" (-s) and \"data only\" (-a) cannot be used together\n");
		goto cleanup;
	}

	if ((dataOnly || schemaOnly) && pInputOpts->actors == SET_INDIVIDUAL)
	{
		mpp_err_msg_cache(logError, progname, "options \"schema only\" (-s) or \"data only\" (-a) cannot be used together with --gp-s=i\n"
						  "If you want to use --gp-i and dump only data, omit dbid number 1 (master) from the individual dbid list.");
		goto cleanup;
	}


	if (dataOnly && outputClean)
	{
		mpp_err_msg_cache(logError, progname, "options \"clean\" (-c) and \"data only\" (-a) cannot be used together\n");
		goto cleanup;
	}

	if (dump_inserts && bOids)
	{
		mpp_err_msg_cache(logError, progname, "INSERT (-d, -D) and OID (-o) options cannot be used together\n");
		mpp_err_msg_cache(logError, progname, "(The INSERT command cannot set OIDs.)\n");
		goto cleanup;
	}

	bRtn = true;

	if (pInputOpts->pszPassThroughParms != NULL)
		mpp_msg(logInfo, progname, "Read params: %s\n", pInputOpts->pszPassThroughParms);
	else
		mpp_msg(logInfo, progname, "Read params: <empty>\n");

	cleanup:

	return bRtn;
}

/*
 * freeThreadParmArray: This function frees the memory allocated for the pData element of the
 * ThreadParmArray, as well as the strings allocated within each element
 * of the array.  It does not free the pParmAr itself.
 */
void
freeThreadParmArray(ThreadParmArray *pParmAr)
{
	int			i;

	for (i = 0; i < pParmAr->count; i++)
	{
		ThreadParm *p = &pParmAr->pData[i];

		if (p->pszRemoteBackupPath != NULL)
			free(p->pszRemoteBackupPath);

		if (p->pszErrorMsg != NULL)
			free(p->pszErrorMsg);
	}

	if (pParmAr->pData != NULL)
		free(pParmAr->pData);

	pParmAr->count = 0;
	pParmAr->pData = NULL;
}

/*
 * help: This function prints the usage info to stdout.
 */
void
help(const char *progname)
{
	printf(("%s dumps a database as a text file or to other formats.\n\n"), progname);
	printf(("Usage:\n"));
	printf(("  %s [OPTION]... [DBNAME]\n"), progname);

	printf(("\nGeneral options:\n"));
	printf(("  -i, --ignore-version     proceed even when server version mismatches\n"
			"                           gp_dump version\n"));
	printf(("  -v, --verbose            verbose mode. adds verbose information to the\n"
			"                           per segment status files\n"));
	printf(("  --help                   show this help, then exit\n"));
	printf(("  --version                output version information, then exit\n"));

	printf(("\nOptions controlling the output content:\n"));
	printf(("  -a, --data-only          dump only the data, not the schema\n"));
	printf(("  -c, --clean              clean (drop) schema prior to create\n"));
	printf(("  -d, --inserts            dump data as INSERT, rather than COPY, commands\n"));
	printf(("  -D, --column-inserts     dump data as INSERT commands with column names\n"));
	printf(_("  -E, --encoding=ENCODING  dump the data in encoding ENCODING\n"));
	printf(("  -n, --schema=SCHEMA      dump the named schema only\n"));
	printf(_("  -N, --exclude-schema=SCHEMA do NOT dump the named schema(s)\n"));
	printf(("  -o, --oids               include OIDs in dump\n"));
	printf(("  -O, --no-owner           do not output commands to set object ownership\n"
			"                           in plain text format\n"));
	printf(("  -s, --schema-only        dump only the schema, no data\n"));
	printf(("  -S, --superuser=NAME     specify the superuser user name to use in\n"
			"                           plain text format\n"));
	printf(("  -t, --table=TABLE        dump only matching table(s) (or views or sequences)\n"));
	printf(_("  -T, --exclude-table=TABLE   do NOT dump matching table(s) (or views or sequences)\n"));
	printf(("  -x, --no-privileges      do not dump privileges (grant/revoke)\n"));
	printf(_("  --disable-triggers       disable triggers during data-only restore\n"));
	printf(("  --incremental            dump only modified patitions\n"));
	printf(_("  --use-set-session-authorization\n"
			 "                              use SESSION AUTHORIZATION commands instead of\n"
	         "                              ALTER OWNER commands to set ownership\n"));

	printf(("\nConnection options:\n"));
	printf(("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(("  -p, --port=PORT          database server port number\n"));
	printf(("  -U, --username=NAME      connect as specified database user\n"));
	printf(("  -W, --password           force password prompt (should happen automatically)\n"));

	printf(("\nGreenplum Database specific options:\n"));
	printf(("  --gp-c                  use gzip for in-line compression\n"));
	printf(("  --gp-d=BACKUPFILEDIR    directory where backup files are placed\n"));
	printf(("  --gp-k=TIMESTAMP        timestamp key to be used for the dump files\n"));
	printf(("  --gp-r=REPORTFILEDIR    directory where report file is placed\n"));
	printf(("  --gp-s=BACKUPSET        backup set indicator - (p)rimaries only (default)\n"));
	printf(("                          or (i)ndividual segdb (must be followed with a list of dbids\n"));
	printf(("                          of primary segments to dump. For example: --gp-s=i[10,12,14]\n"));
	printf(("  --rsyncable             pass --rsyncable option to gzip"));
	printf(("  --ddboost-storage-unit             pass the storage unit name"));

	printf(("\nIf no database name is supplied, then the PGDATABASE environment\n"
			"variable value is used.\n\n"));
	/* printf(("Report bugs to <bugs@greenplum.org>.\n")); */
}


/*
 * exit_nicely: This function was copied from pg_dump.
 *
 * This routine is called by functions in cdb_dump_include to exit.  When exiting
 * through this routine, a normal backup report is not generated.
 */
void
exit_nicely(void)
{
	char	   *lastMsg;
	char	   *pszErrorMsg;

	lastMsg = PQerrorMessage(g_conn);

	pszErrorMsg = MakeString("*** aborted because of error: %s\n", lastMsg);

	if (pszErrorMsg)
	{
		mpp_err_msg(logError, progname, pszErrorMsg);
		free(pszErrorMsg);
	}

	/* Clean-up synchronization variables */
	pthread_mutex_destroy(&MyMutex);
	pthread_cond_destroy(&MyCondVar);

	PQfinish(g_conn);
	g_conn = NULL;

	exit(1);
}


/*
 * installSignalHandlers: This function sets both the SIGINT and SIGTERM signal handlers
 * to the routine myHandler.
 */
void
installSignalHandlers(void)
{
	struct sigaction act,
				oact;

	act.sa_handler = myHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_flags |= SA_RESTART;

	/* Install SIGINT interrupt handler */
	if (sigaction(SIGINT, &act, &oact) < 0)
	{
		mpp_err_msg_cache(logError, progname, "Error trying to set SIGINT interrupt handler\n");
	}

	act.sa_handler = myHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_flags |= SA_RESTART;

	/* Install SIGTERM interrupt handler */
	if (sigaction(SIGTERM, &act, &oact) < 0)
	{
		mpp_err_msg_cache(logError, progname, "Error trying to set SIGTERM interrupt handler\n");
	}
}

/*
 * myHandler: This function is the signal handler for both SIGINT and SIGTERM signals.
 * It simply sets a global variable, which is checked by each thread to see whether it should cancel
 * the backup running on the remote database.
 */
void
myHandler(SIGNAL_ARGS)
{
	static bool bAlreadyHere = false;

	if (bAlreadyHere)
	{
		return;
	}

	bAlreadyHere = true;

	g_b_SendCancelMessage = true;
}

/*
 * parmValNeedsQuotes: This function checks to see whether there is any whitespace in the parameter value.
 * This is used for pass thru parameters, top know whether to enclose them in quotes or not.
 *
 * This function considers a string containing an embedded quotation mark or a dollar sign as requiring
 * quoting.  (This presumes that values checked by this function are being prepared as arguments to a
 * shell command.  Another function is required to escape these characters [and others].)
 */
static bool
parmValNeedsQuotes(const char *Value)
{
	static regex_t rFinder;
	static bool bCompiled = false;
	static bool bAlwaysPutQuotes = false;
	bool		bPutQuotes = false;

	if (!bCompiled)
	{
		if (0 != regcomp(&rFinder, "[[:space:]\"$]", REG_EXTENDED | REG_NOSUB))
			bAlwaysPutQuotes = true;

		bCompiled = true;
	}

	if (!bAlwaysPutQuotes)
	{
		if (regexec(&rFinder, Value, 0, NULL, 0) == 0)
			bPutQuotes = true;
	}
	else
		bPutQuotes = true;

	return bPutQuotes;
}

/*
 * reportBackupResults: This function outputs a summary of the backup results to a file and stdout
 */
int
reportBackupResults(InputOptions inputopts, ThreadParmArray *pParmAr)
{
	FILE	   *fRptFile = NULL;
	PQExpBuffer reportBuf = NULL;
	char	   *pszReportPathName = NULL;
	char	   *pszFormat;
	ThreadParm *pParm0;
	int			i;
	int			failCount;
	int			errorCount;
	char	   *pszStatus;
	char	   *pszMsg;
	char	   *pszReportDirectory = inputopts.pszReportDirectory;
	SegmentDatabase *pSegDB;
	ThreadParm *p;

	if (pParmAr == NULL)
	{
		mpp_err_msg(logError, progname, "Report data missing\n");
		return 2;
	}

	if (pParmAr->count == 0)
	{
		pParmAr->count = 1;		/* just use master in this case (early error) */
		pParmAr->pData = (ThreadParm *) calloc(1, sizeof(ThreadParm));

		if (pParmAr->pData == NULL)
		{
			mpp_err_msg(logError, progname, "Cannot allocate memory for final report\n");
			return 0;
		}

		p = &pParmAr->pData[0];

		p->thread = 0;
		p->pOptionsData = &inputopts;
		p->pOptionsData->pszKey = GetTimestampKey(inputopts.pszTimestampKey);
		p->bSuccess = false;
		p->pszErrorMsg = NULL;
		p->pszRemoteBackupPath = NULL;

		pSegDB = (SegmentDatabase *) calloc(1, sizeof(SegmentDatabase));
		pSegDB->dbid = 1;
		pSegDB->content = -1;
		pSegDB->role = ROLE_MASTER;
		pSegDB->port = (inputopts.pszPGPort ? atoi(inputopts.pszPGPort) : 0);
		pSegDB->pszDBName = (inputopts.pszDBName ? inputopts.pszDBName : "unknown");
		pSegDB->pszHost = inputopts.pszPGHost;
		pSegDB->pszDBUser = NULL;
		pSegDB->pszDBPswd = NULL;

		p->pTargetSegDBData = pSegDB;
	}

	/*
	 * Set the report directory if not set by user (with --gp-r)
	 */
	if (pszReportDirectory == NULL || *pszReportDirectory == '\0')
	{

		if (getenv("MASTER_DATA_DIRECTORY") != NULL)
		{
			/*
			 * report directory not set by user - default to
			 * $MASTER_DATA_DIRECTORY
			 */
			pszReportDirectory = pg_strdup(getenv("MASTER_DATA_DIRECTORY"));
		}
		else
		{
			/* $MASTER_DATA_DIRECTORY not set. Default to current directory */
			pszReportDirectory = "./";
		}
	}


	/* See whether we can create the file in the report directory */
	if (pszReportDirectory[strlen(pszReportDirectory) - 1] != '/')
		pszFormat = "%s/%sgp_dump_%s.rpt";
	else
		pszFormat = "%s%sgp_dump_%s.rpt";

	pszReportPathName = MakeString(pszFormat, pszReportDirectory, dump_prefix_buf == NULL ? "" : dump_prefix_buf->data, pParmAr->pData[0].pOptionsData->pszKey);
	if (pszReportPathName == NULL)
	{
		exit_nicely();
	}

	fRptFile = fopen(pszReportPathName, "w");
	if (fRptFile == NULL)
		mpp_err_msg(logWarn, progname, "Cannot open report file %s for writing.  Will use stdout instead.\n",
					pszReportPathName);

	/* buffer to store the complete dump report */
	reportBuf = createPQExpBuffer();

	/*
	 * Write to buffer a report showing the timestamp key, what the command
	 * line options were, which segment databases were backed up, to where,
	 * and any errors that occurred.
	 */
	pParm0 = &pParmAr->pData[0];

	appendPQExpBuffer(reportBuf, "\nGreenplum Database Backup Report\n");
	appendPQExpBuffer(reportBuf, "Timestamp Key: %s\n", pParm0->pOptionsData->pszKey);

	appendPQExpBuffer(reportBuf, "gp_dump Command Line: %s\n",
			   StringNotNull(pParm0->pOptionsData->pszCmdLineParms, "None"));

	appendPQExpBuffer(reportBuf, "Pass through Command Line Options: %s\n",
		   StringNotNull(pParm0->pOptionsData->pszPassThroughParms, "None"));

	appendPQExpBuffer(reportBuf, "Compression Program: %s\n",
		 StringNotNull(pParm0->pOptionsData->pszCompressionProgram, "None"));

	appendPQExpBuffer(reportBuf, "Backup Type: %s\n", (incremental_backup ? "Incremental" : "Full"));

	appendPQExpBuffer(reportBuf, "\nIndividual Results\n");

	failCount = 0;
	errorCount = 0;
	for (i = 0; i < pParmAr->count; i++)
	{
		ThreadParm *pParm = &pParmAr->pData[i];
		pszMsg = pParm->pszErrorMsg;
		if (!pParm->bSuccess)
		{
			pszStatus = "Failed with error: \n{";
			failCount++;
		}
		else if (pParm->bSuccess && pszMsg != NULL)
		{
			pszStatus = "Finished but errors were found: \n{";
			errorCount++;
		}
		else
			pszStatus = "Succeeded";
		if (pszMsg == NULL)
		{
			/* if we failed pre maturely try to get the predump error */
			pszMsg = get_early_error();
			if (pszMsg == NULL)
				pszMsg = "";
		}


		if (pParm->pTargetSegDBData->role == ROLE_MASTER)
			appendPQExpBuffer(reportBuf, "\tMaster ");
		else
			appendPQExpBuffer(reportBuf, "\tsegment %d ", pParm->pTargetSegDBData->content);

		appendPQExpBuffer(reportBuf, "(dbid %d) Host %s Port %d Database %s BackupFile %s: %s %s%s\n",
						  pParm->pTargetSegDBData->dbid,
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
						  pParm->pTargetSegDBData->port,
						  pParm->pTargetSegDBData->pszDBName,
						  StringNotNull(pParm->pszRemoteBackupPath, ""),
						  pszStatus,
						  pszMsg,
						  (strcmp(pszMsg, "") == 0) ? "" : "}"
			);

		/*
		 * Ugly kludge to 'fix' MPP-12697. The issue is, the dump agent
		 * doesn't return any status about the _post_data file because it's
		 * assumed that it will dump it anyway. If we change that behaviour, we
		 * must remember to change this code too. See the (terribly named)
		 * gp_backup_launch__().
		 */
		if (pParm->pTargetSegDBData->role == ROLE_MASTER)
		{
			appendPQExpBuffer(reportBuf, "\tMaster (dbid %d) Host %s Port %d Database %s BackupFile %s_post_data: %s %s%s\n",
						  pParm->pTargetSegDBData->dbid,
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
						  pParm->pTargetSegDBData->port,
						  pParm->pTargetSegDBData->pszDBName,
						  StringNotNull(pParm->pszRemoteBackupPath, ""),
						  pszStatus,
						  pszMsg,
						  (strcmp(pszMsg, "") == 0) ? "" : "}"
			);
		}
	}

	if (failCount == 0 && errorCount == 0)
		appendPQExpBuffer(reportBuf, "\n%s utility finished successfully.\n", progname);
	else if(failCount > 0)
		appendPQExpBuffer(reportBuf, "\n%s utility finished unsuccessfully with  %d  failures.\n", progname, failCount);
	else
		appendPQExpBuffer(reportBuf, "\n%s utility finished but errors were found.\n", progname);

	/* write report to report file */
	if (fRptFile != NULL)
	{
		mpp_msg(logInfo, progname, "Report results also written to %s.\n", pszReportPathName);
		fprintf(fRptFile, "%s", reportBuf->data);
		fclose(fRptFile);
	}

	/* write report to stdout */
	fprintf(stdout, "%s", reportBuf->data);

	free(pszReportPathName);

	destroyPQExpBuffer(reportBuf);

	if (failCount > 0)
		return 1;
	else if (errorCount > 0)
		return 2;
	return 0;
}

/*
 * spinOffThreads: This function deals with all the threads that drive the
 * backend backups. First, it creates the timestamp key, initializes the
 * ThreadParmArray, and the thread synchronization objects.  Then it loops
 * and creates each of the threads. It waits on a condition variable. This
 * will release when all threads have attained the SET_SERIALIZABLE state
 * (or have failed to do so). Once SET_SERIALIZABLE is reached we know that
 * all the agents are in healthy shape to start operation. We then wait for
 * them to grab locks on their objects. again, we wait on a condition variable
 * until they all done so.
 *
 * in here we release the LOCKs we have on the pg_class by committing
 * the transaction, once all the threads reached STATE_GETLOCKS
 *
 * This function waits for all threads to finish, and only then returns.
 */
void
spinOffThreads(PGconn *pConn,
			   InputOptions * pInputOpts,
			   const SegmentDatabaseArray *psegDBAr,
			   ThreadParmArray * pParmAr)
{
	int			i;
	ThreadParm *pParm = NULL;

	pthread_cond_init(&MyCondVar, NULL);
	pthread_mutex_init(&MyMutex, NULL);
	pthread_cond_init(&MyCondVar2, NULL);
	pthread_mutex_init(&MyMutex2, NULL);

	pInputOpts->pszKey = GetTimestampKey(pInputOpts->pszTimestampKey);

	/*
	 * Create a thread and have it work on executing the dump on the
	 * appropriate segment database.
	 */
	if (!createThreadParmArray(psegDBAr->count, pParmAr))
	{
		mpp_err_msg(logError, progname, "Cannot allocate memory for thread parameters\n");
		exit(1);
	}

	nThreadsFinishedLaunch = nThreadsFinishedLocking = pParmAr->count;

	mpp_msg(logInfo, progname, "About to spin off %d threads with timestamp key %s\n",
			nThreadsFinishedLaunch, pInputOpts->pszKey);

	for (i = 0; i < pParmAr->count; i++)
	{
		pParm = &pParmAr->pData[i];

		pParm->pTargetSegDBData = &psegDBAr->pData[i];
		pParm->pOptionsData = pInputOpts;
		pParm->bSuccess = true;

		mpp_msg(logInfo, progname, "Creating thread to backup dbid %d: host %s port %d database %s\n",
				pParm->pTargetSegDBData->dbid,
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
				pParm->pTargetSegDBData->port,
				pParm->pTargetSegDBData->pszDBName
			);

		pthread_create(&pParm->thread,
					   NULL,
					   threadProc,
					   pParm);
	}

	/*
	 * wait for all threads to complete launch and attain the SET_SERIALIZABLE
	 * state.
	 */
	mpp_msg(logInfo, progname, "Waiting for remote %s processes to start "
			"transactions in serializable isolation level\n",
			pszAgent);

	pthread_mutex_lock(&MyMutex);
	if (nThreadsFinishedLaunch > 0)
	{
		pthread_cond_wait(&MyCondVar, &MyMutex);
	}
	pthread_mutex_unlock(&MyMutex);

	mpp_msg(logInfo, progname, "All remote %s processes have began "
			"transactions in serializable isolation level\n",
			pszAgent);

	/*
	 * wait for all threads to lock all dumpable objects and attain the
	 * SET_GOTLOCKS state.
	 */
	mpp_msg(logInfo, progname, "Waiting for remote %s processes to obtain "
			"local locks on dumpable objects\n",
			pszAgent);


	pthread_mutex_lock(&MyMutex2);
	if (nThreadsFinishedLocking > 0)
	{
		pthread_cond_wait(&MyCondVar2, &MyMutex2);
	}
	pthread_mutex_unlock(&MyMutex2);

	mpp_msg(logInfo, progname, "All remote %s processes have obtains the necessary locks\n",
			pszAgent);

	/* End the transaction and therefore unlock the lock we took on pg_class */
	mpp_msg(logInfo, progname, "Committing transaction on the master database, thereby releasing locks.\n");
	execCommit(pConn);

	/* The main purpose of probing is to detect segment crashes during the
	 * the dump process, to prevent master from keeping pg_class lock
	 * or waiting for status update from segment infinitely. So once the
	 * pg_class lock has been released, we can increase the probing interval
	 * to reduce the overhead.
	 */
	max_probe_retries = 5;
	probe_interval = 120; 	/* 2 minute interval */

	// Create a file on the master to signal to gpcrondump that it can release its pg_class lock
	// Only do this if no-lock is passed, otherwise it can cause problems if gp_dump is called directly and does its own locks
	if (pParm->pTargetSegDBData->role == ROLE_MASTER && no_lock)
	{
		char *dumpkey = pParmAr->pData[0].pOptionsData->pszKey;
		char *filedir = NULL;
		if (pInputOpts->pszReportDirectory == NULL)
		{
			if (getenv("MASTER_DATA_DIRECTORY") != NULL)
				filedir = pg_strdup(getenv("MASTER_DATA_DIRECTORY"));
			else
				filedir = pg_strdup("./");
		}
		else
			filedir = pg_strdup(pInputOpts->pszReportDirectory);

		char *signalFileName = MakeString("%s/gp_lockfile_%s", filedir, &dumpkey[8]);
		mpp_msg(logInfo, progname, "Signal filename is %s.\n", signalFileName);
		bool success = false;
		FILE* sigfile = fopen(signalFileName, "w");
		if (sigfile != NULL)
		{
			fclose(sigfile);
			FILE* sigcheck = fopen(signalFileName, "r");
			if (sigcheck != NULL)
			{
				fclose(sigcheck);
				success = true;
			}
		}
		if (!success)
		{
			mpp_msg(logInfo, progname, "Failed to create signal file, gpcrondump will not release pg_class lock early.\n");
		}
		free(signalFileName);
		free(filedir);
	}

	mpp_msg(logInfo, progname, "Waiting for all remote %s programs to finish.\n", pszAgent);

	/* Wait for all threads to complete */
	for (i = 0; i < pParmAr->count; i++)
	{
		ThreadParm *pParm = &pParmAr->pData[i];

		if (pParm->thread == (pthread_t) 0)
			continue;
		pthread_join(pParm->thread, NULL);
		pParm->thread = (pthread_t) 0;
	}

	mpp_msg(logInfo, progname, "All remote %s programs are finished.\n", pszAgent);
}

/* threadProc: This function is used to connect to one database that needs to be backed up,
 * make a gp_backup_launch call to cause the backup process to begin on the instance hosting the
 * database.  Then it waits for notifications from the process.  It receives a notification
 * whenever the gp_dump_agent process writes a row into the gp_backup_status table.  This happens
 * when the process starts, when it attains the SET_SERIALIZABLE state, and when it finishes.
 */
void *
threadProc(void *arg)
{
	bool		decrementedLunchCount = false;
	bool		decrementedLockCount = false;

	time_t		now, last;

	/*
	 * The argument is a pointer to a ThreadParm structure that stays around
	 * for the entire time the program is running. so we need not worry about
	 * making a copy of it.
	 */
	ThreadParm *pParm = (ThreadParm *) arg;

	const SegmentDatabase *pSegDB = pParm->pTargetSegDBData;
	const InputOptions *pInputOpts = pParm->pOptionsData;
	const char *pszKey = pInputOpts->pszKey;
	PGconn	   *pConn;

	char	   *pszPassThroughCredentials;
	PQExpBuffer Qry;
	PQExpBuffer pqBuffer;
	PGresult   *pRes;
	int			sock;
	bool		bSentCancelMessage;
	BackupStateMachine *pState;
	PGnotify   *pNotify;
	int         retryCnt = 0;
	bool        notifyReceived = false;
	int         pollResult = 0;
	int         pollTimeout;
	struct pollfd *pollInput;

	/*
	 * Block SIGINT and SIGKILL signals (we can handle them in the main
	 * thread)
	 */
	sigset_t	newset;

	sigemptyset(&newset);
	sigaddset(&newset, SIGINT);
	sigaddset(&newset, SIGKILL);
	pthread_sigmask(SIG_SETMASK, &newset, NULL);

	/* Create a connection for this thread */
	pConn = MakeDBConnection(pSegDB, false);
	if (PQstatus(pConn) == CONNECTION_BAD)
	{
		g_b_SendCancelMessage = true;
		pParm->pszErrorMsg = MakeString("Connection to dbid %d  on host %s failed: %s",
										pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"), PQerrorMessage(pConn));
		mpp_err_msg_cache(logError, progname, pParm->pszErrorMsg);
		PQfinish(pConn);
		decrementFinishedLaunchCount();
		return NULL;
	}

	/* issue a LISTEN command for 5 different names */
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_START);
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_SET_SERIALIZABLE);
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_GOTLOCKS);
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_SUCCEED);
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_FAIL);
	DoCancelNotifyListen(pConn, true, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_SEGMENT_PROBE);

	mpp_msg(logInfo, progname, "Listening for messages from server on dbid %d connection\n", pSegDB->dbid);

	/*
	 * If there is a password associated with this login, we pass it as a
	 * base64 encoded string in the parameter pszPassThroughCredentials
	 */
	pszPassThroughCredentials = NULL;
	if (pSegDB->pszDBPswd != NULL && *pSegDB->pszDBPswd != '\0')
		pszPassThroughCredentials = DataToBase64(pSegDB->pszDBPswd, strlen(pSegDB->pszDBPswd));

	/* form an gp_backup_launch statement */

	/*
	 * Argument 4 of the gp_launch_backup function may contain escape
	 * sequences and must either be quoted using the PG $$ quote mechanism or
	 * using an E-type constant with embedded '\' characters doubled.
	 */
	Qry = createPQExpBuffer();
	appendPQExpBuffer(Qry, "SELECT * FROM gp_backup_launch('%s', '%s', '%s', $$%s$$, '%s')",
					  StringNotNull(pInputOpts->pszBackupDirectory, ""),
					  pszKey,
					  StringNotNull(pInputOpts->pszCompressionProgram, ""),
					  StringNotNull(pInputOpts->pszPassThroughParms, ""),
					  StringNotNull(pszPassThroughCredentials, "")
		);


	if (pszPassThroughCredentials != NULL)
		free(pszPassThroughCredentials);

	/* Execute gp_backup_launch. This will start an gp_dump agent */
	pRes = PQexec(pConn, Qry->data);

	if (!pRes || PQresultStatus(pRes) != PGRES_TUPLES_OK || PQntuples(pRes) == 0)
	{
		g_b_SendCancelMessage = true;
		pParm->pszErrorMsg = MakeString("could not start Greenplum Database backup: %s", PQerrorMessage(pConn));
		mpp_err_msg_cache(logFatal, progname, pParm->pszErrorMsg);
		PQfinish(pConn);
		decrementFinishedLaunchCount();
		decrementedLunchCount = true;
		return NULL;
	}

	mpp_msg(logInfo, progname, "Successfully launched Greenplum Database backup on dbid %d server\n", pSegDB->dbid);

	pParm->pszRemoteBackupPath = pg_strdup(PQgetvalue(pRes, 0, 0));

	PQclear(pRes);
	destroyPQExpBuffer(Qry);

	/* Now wait for notifications from the back end */
	sock = PQsocket(pConn);

	bSentCancelMessage = false;

	pState = CreateBackupStateMachine(pszKey, pSegDB->role, pSegDB->dbid);
	if (pState == NULL)
	{
		g_b_SendCancelMessage = true;
		pParm->pszErrorMsg = "error allocating memory for Greenplum Database backup";
		mpp_err_msg_cache(logError, progname, pParm->pszErrorMsg);
		PQfinish(pConn);
		decrementFinishedLaunchCount();
		decrementedLunchCount = true;
		return NULL;
	}

	pollInput = (struct pollfd *)malloc(sizeof(struct pollfd));

	/*
	 * This is a bit involved.	We are waiting for 4 messages: one that the
	 * backend process has started, one that the SET_SERIALIZABLE has
	 * happened, one that the SET_GOTLOCKS has happened, and the third that
	 * FINISH has happened. To avoid polling, we use the NOTIFY/LISTEM
	 * approach. But we use poll() for 2 secs at a time, so we can see
	 * whether or not we need to NOTIFY our backend to cancel, based on
	 * another thread failing. A BackupStateMachine object is used to manage
	 * receiving these notifications
	 */

	time(&now);
	time(&last);

	while (!IsFinalState(pState))
	{
		/*
		 * Check to see whether another thread has failed and therefore we
		 * should cancel
		 */
		if (g_b_SendCancelMessage && !bSentCancelMessage && HasStarted(pState))
		{
			mpp_msg(logInfo, progname, "noticed that a cancel order is in effect. Informing dbid %d on host %s by notifying on connection\n",
				  pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"));

			/*
			 * Either one of the other threads have failed, or a Ctrl C was
			 * received.  So post a cancel message
			 */
			DoCancelNotifyListen(pConn, false, pszKey, pSegDB->role, pSegDB->dbid, -1, NULL);
			bSentCancelMessage = true;
			goto cleanup;
		}

		/* Replacing select() by poll() here to overcome the limitations of
			select() to handle large socket file descriptor values.
		*/

		pollInput->fd = sock;
		pollInput->events = POLLIN;
		pollInput->revents = 0;
		pollTimeout = 2000;
		pollResult = poll(pollInput, 1, pollTimeout);

		if(pollResult < 0)
		{
			g_b_SendCancelMessage = true;
			pParm->pszErrorMsg = MakeString("poll failed for backup key %s, role %d, dbid %d failed\n",
										 pszKey, pSegDB->role, pSegDB->dbid);
			mpp_err_msg(logFatal, progname, pParm->pszErrorMsg);
			goto cleanup;
		}

		/* See whether the connection went down */
		if (PQstatus(pConn) == CONNECTION_BAD)
		{
			g_b_SendCancelMessage = true;
			pParm->pszErrorMsg = MakeString("connection went down for backup key %s, role %d, dbid %d\n",
										 pszKey, pSegDB->role, pSegDB->dbid);
			mpp_err_msg(logError, progname, pParm->pszErrorMsg);
			goto cleanup;
		}

		/* try to get any notification from the server */
		PQconsumeInput(pConn);

		CleanupNotifications(pState);

		/* get the next notification from the server */
		while (NULL != (pNotify = PQnotifies(pConn)))
		{
			/*
			 * compare the LISTEN/NOTIFY (without suffix) name with the one we
			 * expect
			 */
			if (strncasecmp(pState->pszNotifyRelName, pNotify->relname,
							strlen(pState->pszNotifyRelName)) == 0 &&
				strncasecmp(pState->pszNotifyRelNameSegmentProbe, pNotify->relname,
							strlen(pState->pszNotifyRelNameSegmentProbe)))
			{
				/* add this notification to our state notification array */
				if (!AddNotificationtoBackupStateMachine(pState, pNotify))
				{
					g_b_SendCancelMessage = true;
					pParm->pszErrorMsg = MakeString("error allocating memory for Greenplum Database backup\n");
					mpp_err_msg(logError, progname, pParm->pszErrorMsg);
					goto cleanup;
				}
			}
			/* As long as the segment agent is responding with notifications, don't try cancelling */
			notifyReceived = true;
		}

		ProcessInput(pState);
		if (!decrementedLunchCount && HasReceivedSetSerializable(pState))
		{
			decrementFinishedLaunchCount();
			decrementedLunchCount = true;
		}

		if (!decrementedLockCount && HasReceivedGotLocks(pState))
		{
			decrementFinishedLockingCount();
			decrementedLockCount = true;
		}

		time(&now);

		/* Wait here if no response received from segment. Once we reach the
		 * probe_interval, send out next probe notification to the segment.
		 */
		if (notifyReceived)
		{
			/* reset and start next probing cycle */
			notifyReceived = false;
			retryCnt = 0;
			last = now;
		}
		else if(difftime(now, last) >= probe_interval)
		{
			retryCnt += 1;
			last = now;

			/* Sending probe notification to the segment every probe interval */
			DoCancelNotifyListen(pConn, false, pszKey, pSegDB->role, pSegDB->dbid, -1, SUFFIX_MASTER_PROBE);
		}

		/* mark cancellation for dump agent if not receiving response for probe notifications after 10 min. */
		if (retryCnt >= max_probe_retries)
		{
			g_b_SendCancelMessage = true;
			pParm->bSuccess = false;
			pParm->pszErrorMsg = MakeString("Lost response from dump agent with dbid %d on host %s after 10 minutes.\n",
							pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"));
			mpp_err_msg(logError, progname, pParm->pszErrorMsg);
			goto cleanup;
		}
	}

	/*
	 * We don't get here unless the BackupStateMachine reached a final state.
	 * Segment may not catch all errors, scan the dump status file for ERRORS.
	 */
	if(pParm->bSuccess || pParm->pszErrorMsg == NULL)
	{
		pqBuffer = createPQExpBuffer();
		int status = ReadBackendBackupFileError(pConn, pInputOpts->pszBackupDirectory, pszKey,
							BFT_BACKUP_STATUS, progname, pqBuffer);
		if(status != 0)
		{
			pParm->pszErrorMsg = MakeString("%s", pqBuffer->data);
		}
		destroyPQExpBuffer(pqBuffer);
	}

	/*
	 * If the status already indicated a specific error, we process this error in the switch
	 * statement below.
	 */
	if (!pState->bStatus)
	{
		g_b_SendCancelMessage = true;
		pParm->bSuccess = false;

		switch (pState->currentState)
		{
			case STATE_TIMEOUT:
				DoCancelNotifyListen(pConn, false, pszKey, pSegDB->role, pSegDB->dbid, -1, NULL);
				bSentCancelMessage = true;

				/*
				 * this historically reported "failed to set transaction level
				 * serializable"
				 */
				pParm->pszErrorMsg = MakeString("backup failed before being able to begin the backup transaction. "
												"there are various reasons that may cause this to happen. Please "
							"inspect the server log of dbid %d on host %s\n "
									  "Detail from remote status file: %s\n",
				   pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"), StringNotNull(pParm->pszErrorMsg, ""));

				break;

			case STATE_UNEXPECTED_INPUT:
				DoCancelNotifyListen(pConn, false, pszKey, pSegDB->role, pSegDB->dbid, -1, NULL);
				bSentCancelMessage = true;

				pParm->pszErrorMsg = MakeString("Unexpected task id encountered while performing backup for for segment %d on host %s",
				  pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"));
				break;

			case STATE_BACKUP_ERROR:
			default:
				break;
		}

	}

	if (pParm->bSuccess)
	{
		mpp_msg(logInfo, progname, "backup succeeded for dbid %d on host %s\n",
				pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"));
	}
	else
	{
		mpp_err_msg(logError, progname, "backup failed for dbid %d on host %s\n%s\n",
				pSegDB->dbid, StringNotNull(pSegDB->pszHost, "localhost"), StringNotNull(pParm->pszErrorMsg, ""));
	}


cleanup:
	/*
	 * make sure to decrement if we haven't already, to release mutex if in
	 * error state
	 */
	if (!decrementedLunchCount)
	{
		decrementFinishedLaunchCount();
	}

	if (!decrementedLockCount)
	{
		decrementFinishedLockingCount();
	}

	PQfinish(pConn);
	DestroyBackupStateMachine(pState);

	return (NULL);
}

