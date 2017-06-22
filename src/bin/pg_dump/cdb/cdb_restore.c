/*-------------------------------------------------------------------------
 *
 * cdb_restore.c
 *	  cdb_restore is a utility for restoring a cdb cluster of postgres databases
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <pthread.h>
#include "getopt_long.h"
#include "pqexpbuffer.h"
#include "libpq-fe.h"
#include "fe-auth.h"
#include "pg_backup.h"
#include "cdb_table.h"
#include "cdb_dump_util.h"
#include "cdb_backup_state.h"
#include "cdb_dump.h"
#include <poll.h>

/* This is necessary on platforms where optreset variable is not available.
 * Look at "man getopt" documentation for details.
 */
#ifndef HAVE_OPTRESET
int			optreset;
#endif

#define DUMP_PREFIX (dump_prefix==NULL?"":dump_prefix)

/* Forward decls */
static void freeThreadParmArray(ThreadParmArray * pParmAr);
static bool createThreadParmArray(int nCount, ThreadParmArray * pParmAr);
static char *addPassThroughParm(char Parm, const char *pszValue, char *pszPassThroughParmString);
static bool fillInputOptions(int argc, char **argv, InputOptions * pInputOpts);
static bool parmValNeedsQuotes(const char *Value);
static void usage(void);
static void *threadProc(void *arg);
static void myHandler(SIGNAL_ARGS);
static void installSignalHandlers(void);
static bool restoreMaster(InputOptions * pInputOpts, PGconn *pConn, SegmentDatabase *sSegDB, SegmentDatabase *tSegDB, ThreadParm * pParm);
static void spinOffThreads(PGconn *pConn, InputOptions * pInputOpts, const RestorePairArray * restorePair, ThreadParmArray * pParmAr);
static int	reportRestoreResults(const char *pszReportDirectory, const ThreadParm * pMasterParm, const ThreadParmArray * pParmAr);
static int	reportMasterError(InputOptions inputopts, const ThreadParm * pMasterParm, const char *localMsg);
static bool g_b_SendCancelMessage = false;
typedef struct option optType;

static RestoreOptions *opts = NULL;
static bool dataOnly = false;
static bool postdataSchemaRestore = false;
static bool postdataRestore = true;
static bool schemaRestore = true;
static bool dataRestore = true;
static bool schemaOnly = false;
static bool bForcePassword = false;
static bool bIgnoreVersion = false;
static bool bAoStats = true;
static bool g_is_old_format = false;
static const char *pszAgent = "gp_restore_agent";

static const char *logInfo = "INFO";
static const char *logWarn = "WARN";
static const char *logError = "ERROR";
static const char *logFatal = "FATAL";
const char *progname;
static char * addPassThroughLongParm(const char *Parm, const char *pszValue, char *pszPassThroughParmString);
PQExpBuffer dir_buf = NULL;
PQExpBuffer dump_prefix_buf = NULL;

/* NetBackup related variable */
static char *netbackup_service_host = NULL;
static char *netbackup_block_size = NULL;

static char *change_schema_file = NULL;
static char *schema_level_file = NULL;

#ifdef USE_DDBOOST
static int dd_boost_enabled = 0;
#endif

int
main(int argc, char **argv)
{
	PGconn	   *pConn = NULL;
	int			failCount = -1;
	int			i;
	bool		found_master;
	SegmentDatabase *sourceSegDB = NULL;
	SegmentDatabase *targetSegDB = NULL;

	dir_buf = createPQExpBuffer();
	dump_prefix_buf = createPQExpBuffer();

	progname = get_progname(argv[0]);

	/* This struct holds the values of the command line parameters */
	InputOptions inputOpts;

	/*
	 * This struct holds an array of the segment databases from the master mpp
	 * tables
	 */
	RestorePairArray restorePairAr;

	/* This struct holds an array of the thread parameters */
	ThreadParmArray parmAr;
	ThreadParm	masterParm;

	memset(&inputOpts, 0, sizeof(inputOpts));
	memset(&restorePairAr, 0, sizeof(restorePairAr));
	memset(&parmAr, 0, sizeof(parmAr));
	memset(&masterParm, 0, sizeof(masterParm));

#ifdef USE_DDBOOST
	dd_boost_enabled = 0;
#endif
	/* Parse command line for options */
	if (!fillInputOptions(argc, argv, &inputOpts))
	{
		failCount = reportMasterError(inputOpts, &masterParm,
									  get_early_error());
		mpp_err_msg(logInfo, progname, "Reporting Master Error from fillInputOptions.\n");
		goto cleanup;
	}


	mpp_err_msg(logInfo, progname, "Analyzed command line options.\n");

	/* Connect to database on the master */
	mpp_err_msg(logInfo, progname, "Connecting to master segment on host %s port %s database %s.\n",
				StringNotNull(inputOpts.pszPGHost, "localhost"),
				StringNotNull(inputOpts.pszPGPort, "5432"),
				StringNotNull(inputOpts.pszMasterDBName, "?"));

	pConn = GetMasterConnection(progname, inputOpts.pszMasterDBName, inputOpts.pszPGHost,
								inputOpts.pszPGPort, inputOpts.pszUserName,
								bForcePassword, bIgnoreVersion, true);
	if (pConn == NULL)
	{
		masterParm.pOptionsData = &inputOpts;
		failCount = reportMasterError(inputOpts, &masterParm,
									  get_early_error());
		goto cleanup;

	}

	/* Read mpp segment databases configuration from the master */
	mpp_err_msg(logInfo, progname, "Reading Greenplum Database configuration info from master segment database.\n");
	if (!GetRestoreSegmentDatabaseArray(pConn, &restorePairAr, inputOpts.backupLocation, inputOpts.pszRawDumpSet, dataOnly))
		goto cleanup;

	/* find master node */
	targetSegDB = NULL;
	found_master = false;

	for (i = 0; i < restorePairAr.count; i++)
	{
		targetSegDB = &restorePairAr.pData[i].segdb_target;
		sourceSegDB = &restorePairAr.pData[i].segdb_source;

		if (targetSegDB->role == ROLE_MASTER)
		{
			found_master = true;
			break;
		}
	}

	/* Install the SIGINT and SIGTERM handlers */
	installSignalHandlers();


	/* There is no behavior change for full restore with no filter and for incremental restore.
	 * The behavior only changes when postdataSchemaRestore is specified, incase of full restore with table filters,
	 * for all other cases schemaRestore, dataRestore and postdataRestore defaults to true, hence no behavior change.
	 */

	/*
	 * restore master first. However, if we restore only data, or if master
	 * segment is not included in our restore set, skip master
	 */
	if (!dataOnly && found_master && schemaRestore)
	{
		if (!restoreMaster(&inputOpts, pConn, sourceSegDB, targetSegDB, &masterParm))
		{
			failCount = reportMasterError(inputOpts, &masterParm, NULL);
			goto cleanup;
		}

	}

	/* restore segdbs. However, if we restore schema only, skip segdbs */
	if (!schemaOnly && dataRestore)
	{
		if (restorePairAr.count > 0)
		{
			/*
			 * Create the threads to talk to each of the databases being
			 * restored. Wait for them to finish.
			 */
			spinOffThreads(pConn, &inputOpts, &restorePairAr, &parmAr);

			mpp_err_msg(logInfo, progname, "All remote %s programs are finished.\n", pszAgent);
		}

	}

	/*
	 * restore post-data items last. However, if we restore only data, or if master
	 * segment is not included in our restore set, skip this step
	 */
	if (!dataOnly && found_master && postdataRestore)
	{
		char * newParms;
		int newlen =  strlen(" --post-data-schema-only") + 1;
		if (inputOpts.pszPassThroughParms != NULL)
			newlen += strlen(inputOpts.pszPassThroughParms);
		newParms = malloc(newlen);
		newParms[0] = '\0';
		if (inputOpts.pszPassThroughParms != NULL)
		    strcpy(newParms, inputOpts.pszPassThroughParms);
		strcat(newParms, " --post-data-schema-only");
		inputOpts.pszPassThroughParms = newParms;

		if (!restoreMaster(&inputOpts, pConn, sourceSegDB, targetSegDB, &masterParm))
		{
			failCount = reportMasterError(inputOpts, &masterParm, NULL);
			goto cleanup;
		}

	}

	/* Produce results report */
	failCount = reportRestoreResults(inputOpts.pszReportDirectory, &masterParm, &parmAr);

cleanup:
	FreeRestorePairArray(&restorePairAr);
	FreeInputOptions(&inputOpts);

	if (opts != NULL)
		free(opts);

	freeThreadParmArray(&parmAr);

	if (masterParm.pszErrorMsg)
		free(masterParm.pszErrorMsg);

	if (masterParm.pszRemoteBackupPath)
		free(masterParm.pszRemoteBackupPath);

	if (pConn != NULL)
		PQfinish(pConn);

	destroyPQExpBuffer(dir_buf);
	destroyPQExpBuffer(dump_prefix_buf);

	return failCount;
}

static void
usage(void)
{
	printf(("%s restores a Greenplum Database database from an archive created by gp_dump.\n\n"), progname);
	printf(("Usage:\n"));
	printf(("  %s [OPTIONS]\n"), progname);

	printf(("\nGeneral options:\n"));
	printf(("  -d, --dbname=NAME        output database name\n"));
	printf(("  -i, --ignore-version     proceed even when server version mismatches\n"));
	printf(("  -v, --verbose            verbose mode. adds verbose information to the\n"
			"                           per segment status files\n"));
	printf(("  --help                   show this help, then exit\n"));
	printf(("  --version                output version information, then exit\n"));

	printf(("\nOptions controlling the output content:\n"));
	printf(("  -a, --data-only          restore only the data, no schema\n"));
	printf(("  -s, --schema-only        restore only the schema, no data\n"));
	printf(("  -P, --post-data-schema-only    restore only the postdataSchema\n"));

	printf(("\nConnection options:\n"));
	printf(("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(("  -p, --port=PORT          database server port number\n"));
	printf(("  -U, --username=NAME      connect as specified database user\n"));
	printf(("  -W, --password           force password prompt (should happen automatically)\n"));

	printf(("\nGreenplum Database specific options:\n"));
	printf(("  --gp-c                  use gunzip for in-line de-compression\n"));
	printf(("  --gp-d=BACKUPFILEDIR    directory where backup files are located\n"));
	printf(("  --gp-i                  ignore error\n"));
	printf(("  --gp-k=KEY              date time backup key from gp_backup run\n"));
	printf(("  --gp-r=REPORTFILEDIR    directory where report file is placed\n"));
	printf(("  --status=STATUSFILEDIR  directory where status file is placed\n"));
	printf(("  --gp-l=FILELOCATIONS    backup files are on (p)rimaries only (default)\n"));
	printf(("                          or (i)ndividual segdb (must be followed with a list of dbid's\n"));
	printf(("                          where backups are located. For example: --gp-l=i[10,12,15]\n"));
	printf(("  --gp-f=FILE             FILE, present on all machines, with tables to include in restore\n"));
	printf(("  --prefix=PREFIX         PREFIX of the dump files to be restored\n"));
	printf(("  --change-schema-file=SCHEMA_FILE  Schema file containing the name of the schema to which tables are to be restored\n"));
	printf(("  --schema-level-file=SCHEMA_FILE  Schema file containing the name of the schemas under which all tables are to be restored\n"));
	printf(("  --ddboost-storage-unit             pass the storage unit name"));
}

bool
fillInputOptions(int argc, char **argv, InputOptions * pInputOpts)
{
	int			c;
	/* Archive	  *AH; */
	/* char    *inputFileSpec; */
	extern int	optind;
	extern char *optarg;
	/* static int	use_setsessauth = 0; */
	static int	disable_triggers = 0;
	bool		bSawCDB_S_Option;
	int			lenCmdLineParms;
	int			i;

	struct option cmdopts[] = {
		{"data-only", 0, NULL, 'a'},
		{"post-data-schema-only", 0, NULL, 'P'},
		{"dbname", 1, NULL, 'd'},
		{"host", 1, NULL, 'h'},
		{"ignore-version", 0, NULL, 'i'},
		{"no-acl", 0, NULL, 'x'},
		{"no-reconnect", 0, NULL, 'R'},
		{"port", 1, NULL, 'p'},
		{"password", 0, NULL, 'W'},
		{"schema-only", 0, NULL, 's'},
		{"username", 1, NULL, 'U'},
		{"verbose", 0, NULL, 'v'},

		/*
		 * the following are Greenplum Database specific, and don't have an equivalent short option
		 */
		{"gp-c", no_argument, NULL, 1},
		{"gp-d", required_argument, NULL, 3},
		{"gp-i", no_argument, NULL, 4},
		{"gp-k", required_argument, NULL, 6},
		{"gp-r", required_argument, NULL, 7},
		{"gp-s", required_argument, NULL, 8},
		{"gp-l", required_argument, NULL, 9},

#ifdef USE_DDBOOST
                {"ddboost", no_argument, NULL, 10},
                {"ddboost-storage-unit", required_argument, NULL, 19},
#endif

		{"gp-f", required_argument, NULL, 11},
		{"gp-nostats", no_argument, NULL, 12},
		{"prefix", required_argument, NULL, 13},
		{"status", required_argument, NULL, 14},
		{"netbackup-service-host", required_argument, NULL, 15},
		{"netbackup-block-size", required_argument, NULL, 16},
		{"change-schema-file", required_argument, NULL, 17},
		{"schema-level-file", required_argument, NULL, 18},
		{"old-format", no_argument, NULL, 20},
		{NULL, 0, NULL, 0}
	};

	/* Initialize option fields  */
	memset(pInputOpts, 0, sizeof(InputOptions));

	pInputOpts->actors = SET_NO_MIRRORS;		/* restore primaries only.
												 * mirrors will get sync'ed
												 * automatically */
	pInputOpts->backupLocation = FILE_ON_PRIMARIES;
	pInputOpts->bOnErrorStop = true;
	pInputOpts->pszBackupDirectory = NULL;

	opts = (RestoreOptions *) calloc(1, sizeof(RestoreOptions));
	if (opts == NULL)
	{
		mpp_err_msg_cache(logError, progname, "error allocating memory for RestoreOptions");
		return false;
	}

	opts->format = archUnknown;
	opts->suppressDumpWarnings = false;

	bSawCDB_S_Option = false;

	if (argc == 1)
	{
		usage();
		exit(0);
	}

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage();
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("gp_restore (Greenplum Database) " GP_VERSION);
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
		return false;
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

	while ((c = getopt_long(argc, argv, "aPd:h:ip:RsuU:vwW",
							cmdopts, NULL)) != -1)
	{
		switch (c)
		{
			case 'a':			/* Dump data only */
				opts->dataOnly = 1;
				dataOnly = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;
			case 'P':			/* postdataSchemaRestore is only specified for full restore with table filters. */
				opts->postdataSchemaRestore = 1;
				postdataSchemaRestore = true;
				break;
			case 'd':
				opts->dbname = strdup(optarg);
				pInputOpts->pszDBName = Safe_strdup(opts->dbname);
				pInputOpts->pszMasterDBName = Safe_strdup(opts->dbname);
				break;
			case 'h':
				if (strlen(optarg) != 0)
				{
					opts->pghost = strdup(optarg);
					pInputOpts->pszPGHost = Safe_strdup(optarg);
				}

				break;
			case 'i':
				/* obsolete option */
				break;
			case 'p':
				if (strlen(optarg) != 0)
				{
					opts->pgport = strdup(optarg);
					pInputOpts->pszPGPort = Safe_strdup(optarg);
				}
				break;
			case 'R':
				/* no-op, still accepted for backwards compatibility */
				break;
			case 's':		/* dump schema only */
				opts->schemaOnly = 1;
				schemaOnly = true;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;
			case 'u':
				opts->promptPassword = TRI_YES;
				opts->username = simple_prompt("User name: ", 100, true);
				pInputOpts->pszUserName = Safe_strdup(opts->username);
				break;

			case 'U':
				opts->username = optarg;
				pInputOpts->pszUserName = Safe_strdup(opts->username);
				break;

			case 'v':			/* verbose */
				opts->verbose = 1;
				pInputOpts->pszPassThroughParms = addPassThroughParm(c, NULL, pInputOpts->pszPassThroughParms);
				break;

			case 'w':
				opts->promptPassword = TRI_NO;
				break;

			case 'W':
				opts->promptPassword = TRI_YES;
				break;

				/* This covers the long options equivalent to -X xxx. */
			case 0:
				break;

			case 1:
				/* gp-c remote compression program */
				pInputOpts->pszCompressionProgram = "gunzip";	/* Safe_strdup(optarg); */
				break;

			case 3:
				/* gp-d backup remote directory */
				pInputOpts->pszBackupDirectory = Safe_strdup(optarg);
				break;

			case 4:
				/* gp-e on error stop */
				pInputOpts->bOnErrorStop = false;
				break;

			case 6:
				/* gp-k backup timestamp key */
				pInputOpts->pszKey = Safe_strdup(optarg);
				break;

			case 7:
				/* gp-r report directory */
				pInputOpts->pszReportDirectory = Safe_strdup(optarg);
				break;
			case 9:

				/* gp-l backup file location */
				if (strcasecmp(optarg, "p") == 0)
				{
					pInputOpts->backupLocation = FILE_ON_PRIMARIES;
					pInputOpts->pszRawDumpSet = NULL;
				}
				else if (strncasecmp(optarg, "i", 1) == 0)
				{
					pInputOpts->backupLocation = FILE_ON_INDIVIDUAL;
					pInputOpts->pszRawDumpSet = Safe_strdup(optarg);
				}
				else
				{
					mpp_err_msg_cache(logError, progname, "invalid gp-l option %s.  Must be p (on primary segments), or i[dbid list] for \"individual\".\n", optarg);
					return false;
				}
				break;
#ifdef USE_DDBOOST
			case 10:
				dd_boost_enabled = 1;
				break;
			case 19:
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("ddboost-storage-unit", optarg, pInputOpts->pszPassThroughParms);
				break;
#endif
			case 11:
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("gp-f", optarg, pInputOpts->pszPassThroughParms);
				break;

			case 12:
				bAoStats = false;
				break;
			case 13:
				appendPQExpBuffer(dump_prefix_buf, "%s", optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("prefix", dump_prefix_buf->data, pInputOpts->pszPassThroughParms);
				break;
			case 14:
				appendPQExpBuffer(dir_buf, "%s", optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("status", dir_buf->data, pInputOpts->pszPassThroughParms);
				break;
			case 15:
				netbackup_service_host = Safe_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-service-host", netbackup_service_host, pInputOpts->pszPassThroughParms);
				if (netbackup_service_host != NULL)
					free(netbackup_service_host);
				break;
			case 16:
				netbackup_block_size = Safe_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("netbackup-block-size", netbackup_block_size, pInputOpts->pszPassThroughParms);
				if (netbackup_block_size != NULL)
					free(netbackup_block_size);
				break;
			case 17:
				change_schema_file = Safe_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("change-schema-file", change_schema_file, pInputOpts->pszPassThroughParms);
				if (change_schema_file != NULL)
					free(change_schema_file);
				break;
			case 18:
				schema_level_file = Safe_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("schema-level-file", schema_level_file, pInputOpts->pszPassThroughParms);
				if (schema_level_file != NULL)
					free(schema_level_file);
				break;
			case 20:
				g_is_old_format = true;
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("old-format", NULL, pInputOpts->pszPassThroughParms);
				break;
			default:
				mpp_err_msg_cache(logError, progname, "Try \"%s --help\" for more information.\n", progname);
				return false;
		}
	}


	/* If postdataSchemaRestore is specified with -s option then only schema is restored and not postdataSchema.*/
	if (schemaOnly && postdataSchemaRestore && !dataOnly)
	{
		schemaRestore = true;
		postdataRestore = false;
	}

	/* If postdataSchemaRestore is specified without -s and -a option then only postdataSchema is restored. */
	if (postdataSchemaRestore && !schemaOnly && !dataOnly)
	{
		postdataRestore = true;
		schemaRestore = false;
		dataRestore = false;
	}


#ifdef USE_DDBOOST
        if (dd_boost_enabled)
	{
                pInputOpts->pszPassThroughParms = addPassThroughLongParm("dd_boost_enabled", NULL, pInputOpts->pszPassThroughParms);
		if (pInputOpts->pszBackupDirectory)
                	pInputOpts->pszPassThroughParms = addPassThroughLongParm("dd_boost_dir", pInputOpts->pszBackupDirectory, pInputOpts->pszPassThroughParms);
		else
			pInputOpts->pszPassThroughParms = addPassThroughLongParm("dd_boost_dir", "db_dumps", pInputOpts->pszPassThroughParms);
	}
#endif

	/*
	 * get PG env variables, override only of no cmd-line value specified
	 */
	if (pInputOpts->pszDBName == NULL)
	{
		if (getenv("PGDATABASE") != NULL)
		{
			pInputOpts->pszDBName = Safe_strdup(getenv("PGDATABASE"));
			pInputOpts->pszMasterDBName = Safe_strdup(getenv("PGDATABASE")); /* TODO: is this variable redundant? */
		}
	}

	if (pInputOpts->pszPGPort == NULL)
	{
		if (getenv("PGPORT") != NULL)
			pInputOpts->pszPGPort = Safe_strdup(getenv("PGPORT"));
	}

	if (pInputOpts->pszPGHost == NULL)
	{
		if (getenv("PGHOST") != NULL)
			pInputOpts->pszPGHost = Safe_strdup(getenv("PGHOST"));
	}


	/*
	 * Check for cmd line parameter errors.
	 */
	if (pInputOpts->pszDBName == NULL)
	{
		mpp_err_msg_cache(logError, progname, "%s command line missing the database parameter (-d)\n", progname);
		return false;
	}

	opts->useDB = 1;

	if (pInputOpts->pszKey == NULL)
	{
		mpp_err_msg_cache(logError, progname, "%s command line missing the date time backup key parameter (--gp-k)\n", progname);
		return false;
	}

	if (pInputOpts->pszMasterDBName == NULL)
	{
		mpp_err_msg_cache(logError, progname, "%s command line missing the master database parameter (--)\n", progname);
		return false;
	}

	if ((dataOnly || schemaOnly) && pInputOpts->backupLocation == FILE_ON_INDIVIDUAL)
	{
		mpp_err_msg_cache(logError, progname, "options \"schema only\" (-s) or \"data only\" (-a) cannot be used together with --gp-s=i\n"
						  "If you want to use --gp-i and dump only data, omit dbid number 1 (master) from the individual dbid list.\n");
		return false;
	}


	if (opts->formatName == NULL)
	{
		opts->formatName = "p";
	}

	opts->disable_triggers = disable_triggers;
	bForcePassword = (opts->promptPassword == TRI_YES);
	/* bIgnoreVersion = (opts->ignoreVersion == 1); */
	if (opts->formatName)
	{

		switch (opts->formatName[0])
		{

			case 'c':
			case 'C':
				opts->format = archCustom;
				break;

			case 't':
			case 'T':
				opts->format = archTar;
				break;

			case 'p':
			case 'P':
				opts->format = archNull;
				break;

			default:
				mpp_err_msg_cache(logError, progname, "unrecognized archive format '%s'; please specify 'p', 't', or 'c'\n",
								  opts->formatName);
				return false;
		}
	}

	return true;

}

/*
 * parmValNeedsQuotes: This function checks to see whether there is any whitespace in the parameter value.
 * This is used for pass thru parameters, top know whether to enclose them in quotes or not.
 */
bool
parmValNeedsQuotes(const char *Value)
{
	static regex_t rFinder;
	static bool bCompiled = false;
	static bool bAlwaysPutQuotes = false;
	bool		bPutQuotes = false;

	if (!bCompiled)
	{
		if (0 != regcomp(&rFinder, "[[:space:]]", REG_EXTENDED | REG_NOSUB))
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
 * addPassThroughParm: this function adds to the string of pass-through parameters.  These get sent to each
 * backend and get passed to the gp_dump program.
 */
char *
addPassThroughParm(char Parm, const char *pszValue, char *pszPassThroughParmString)
{
	char	   *pszRtn;
	bool		bFirstTime = (pszPassThroughParmString == NULL);

	if (pszValue != NULL)
	{
		if (parmValNeedsQuotes(pszValue))
		{
			if (bFirstTime)
				pszRtn = MakeString("-%c \"%s\"", Parm, pszValue);
			else
				pszRtn = MakeString("%s -%c \"%s\"", pszPassThroughParmString, Parm, pszValue);
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

/* threadProc: This function is used to connect to one database that needs to be restored,
 * make a gp_restore_launch call to cause the restore process to begin on the instance hosting the
 * database.  Then it waits for notifications from the process.  It receives a notification
 * wnenever the gp_restore_agent process signals with a NOTIFY. This happens when the process starts,
 * and when it finishes.
 */
void *
threadProc(void *arg)
{
	/*
	 * The argument is a pointer to a ThreadParm structure that stays around
	 * for the entire time the program is running. so we need not worry about
	 * making a copy of it.
	 */
	ThreadParm *pParm = (ThreadParm *) arg;

	SegmentDatabase *sSegDB = pParm->pSourceSegDBData;
	SegmentDatabase *tSegDB = pParm->pTargetSegDBData;
	const InputOptions *pInputOpts = pParm->pOptionsData;
	const char *pszKey = pInputOpts->pszKey;
	PGconn	   *pConn;
	char	   *pszPassThroughCredentials;
	char	   *pszPassThroughTargetInfo;
	PQExpBuffer Qry;
	PQExpBuffer pqBuffer;
	PGresult   *pRes;
	int			sock;
	bool		bSentCancelMessage;
	bool		bIsFinished;
	bool		bIsStarted;
	int			nTries;
	char	   *pszNotifyRelName;
	char	   *pszNotifyRelNameStart;
	char	   *pszNotifyRelNameSucceed;
	char	   *pszNotifyRelNameFail;
	PGnotify   *pNotify;
	int			nNotifies;
	struct pollfd *pollInput;
	int 		pollResult = 0;
	int			pollTimeout;

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
	if (strcasecmp(pInputOpts->pszDBName, pInputOpts->pszMasterDBName) != 0)
	{
		if (sSegDB->pszDBName != NULL)
			free(sSegDB->pszDBName);

		sSegDB->pszDBName = Safe_strdup(pInputOpts->pszDBName);
	}

	/* connect to the source segDB to start gp_restore_agent there */
	pConn = MakeDBConnection(sSegDB, false);
	if (PQstatus(pConn) == CONNECTION_BAD)
	{
		g_b_SendCancelMessage = true;
		pParm->pszErrorMsg = MakeString("Connection to dbid %d on host %s failed: %s",
										sSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"), PQerrorMessage(pConn));
		mpp_err_msg_cache(logError, progname, pParm->pszErrorMsg);
		PQfinish(pConn);
		return NULL;
	}

	/* issue a LISTEN command for 3 different names */
	DoCancelNotifyListen(pConn, true, pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid, SUFFIX_START);
	DoCancelNotifyListen(pConn, true, pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid, SUFFIX_SUCCEED);
	DoCancelNotifyListen(pConn, true, pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid, SUFFIX_FAIL);

	mpp_err_msg(logInfo, progname, "Listening for messages from dbid %d server (source) for dbid %d restore\n", sSegDB->dbid, tSegDB->dbid);

	/* Execute gp_restore_launch  */

	/*
	 * If there is a password associated with this login, we pass it as a
	 * base64 encoded string in the parameter pszPassThroughCredentials
	 */
	pszPassThroughCredentials = NULL;
	if (sSegDB->pszDBPswd != NULL && *sSegDB->pszDBPswd != '\0')
		pszPassThroughCredentials = DataToBase64(sSegDB->pszDBPswd, strlen(sSegDB->pszDBPswd));

	pszPassThroughTargetInfo = MakeString("--target-dbid %d --target-host %s --target-port %d ",
								tSegDB->dbid, tSegDB->pszHost, tSegDB->port);

	Qry = createPQExpBuffer();
	appendPQExpBuffer(Qry, "SELECT * FROM gp_restore_launch('%s', '%s', '%s', '%s', '%s', '%s', %d, %s)",
					  StringNotNull(pInputOpts->pszBackupDirectory, ""),
					  pszKey,
					  StringNotNull(pInputOpts->pszCompressionProgram, ""),
					  StringNotNull(pInputOpts->pszPassThroughParms, ""),
					  StringNotNull(pszPassThroughCredentials, ""),
					  StringNotNull(pszPassThroughTargetInfo, ""),
					  tSegDB->dbid,
					  pInputOpts->bOnErrorStop ? "true" : "false");

	if (pszPassThroughCredentials != NULL)
		free(pszPassThroughCredentials);

	if (pszPassThroughTargetInfo != NULL)
		free(pszPassThroughTargetInfo);

	pRes = PQexec(pConn, Qry->data);
	if (!pRes || PQresultStatus(pRes) != PGRES_TUPLES_OK || PQntuples(pRes) == 0)
	{
		g_b_SendCancelMessage = true;
		pParm->pszErrorMsg = MakeString("could not start Greenplum Database restore: %s", PQerrorMessage(pConn));
		mpp_err_msg_cache(logError, progname, pParm->pszErrorMsg);
		PQfinish(pConn);
		return NULL;
	}

	mpp_err_msg(logInfo, progname, "Successfully launched Greenplum Database restore on dbid %d to restore dbid %d\n", sSegDB->dbid, tSegDB->dbid);

	pParm->pszRemoteBackupPath = strdup(PQgetvalue(pRes, 0, 0));

	PQclear(pRes);
	destroyPQExpBuffer(Qry);

	/* Now wait for notifications from the back end back end */
	sock = PQsocket(pConn);

	bSentCancelMessage = false;
	bIsFinished = false;
	bIsStarted = false;
	nTries = 0;

	pszNotifyRelName = MakeString("N%s_%d_%d_T%d",
						   pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid);
	pszNotifyRelNameStart = MakeString("%s_%s", pszNotifyRelName, SUFFIX_START);
	pszNotifyRelNameSucceed = MakeString("%s_%s", pszNotifyRelName, SUFFIX_SUCCEED);
	pszNotifyRelNameFail = MakeString("%s_%s", pszNotifyRelName, SUFFIX_FAIL);

	pollInput = (struct pollfd *)malloc(sizeof(struct pollfd));

	while (!bIsFinished)
	{
		/*
		 * Check to see whether another thread has failed and therefore we
		 * should cancel
		 */
		if (g_b_SendCancelMessage && !bSentCancelMessage && bIsStarted)
		{
			mpp_err_msg(logInfo, progname, "noticed that a cancel order is in effect. Informing dbid %d on host %s\n",
				  sSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"));

			/*
			 * Either one of the other threads have failed, or a Ctrl C was
			 * received.  So post a cancel message
			 */
			DoCancelNotifyListen(pConn, false, pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid, NULL);
			bSentCancelMessage = true;
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
			pParm->pszErrorMsg = MakeString("poll failed for backup key %s, source dbid %d, target dbid %d failed\n",
										 pszKey, sSegDB->dbid, tSegDB->dbid);
			mpp_err_msg(logError, progname, pParm->pszErrorMsg);
			PQfinish(pConn);
			free(pszNotifyRelName);
			free(pszNotifyRelNameStart);
			free(pszNotifyRelNameSucceed);
			free(pszNotifyRelNameFail);
			free(pollInput);
			return NULL;
		}

		/* See whether the connection went down */
		if (PQstatus(pConn) == CONNECTION_BAD)
		{
			g_b_SendCancelMessage = true;
			pParm->pszErrorMsg = MakeString("connection went down for backup key %s, source dbid %d, target dbid %d\n",
										 pszKey, sSegDB->dbid, tSegDB->dbid);
			mpp_err_msg(logFatal, progname, pParm->pszErrorMsg);
			free(pszNotifyRelName);
			free(pszNotifyRelNameStart);
			free(pszNotifyRelNameSucceed);
			free(pszNotifyRelNameFail);
			free(pollInput);
			PQfinish(pConn);
			return NULL;
		}

		PQconsumeInput(pConn);

		nNotifies = 0;
		while (NULL != (pNotify = PQnotifies(pConn)))
		{
			if (strncasecmp(pszNotifyRelName, pNotify->relname, strlen(pszNotifyRelName)) == 0)
			{
				nNotifies++;
				if (strcasecmp(pszNotifyRelNameStart, pNotify->relname) == 0)
				{
					bIsStarted = true;
					mpp_err_msg(logInfo, progname, "restore started for source dbid %d, target dbid %d on host %s\n",
								sSegDB->dbid, tSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"));
				}
				else if (strcasecmp(pszNotifyRelNameSucceed, pNotify->relname) == 0)
				{
					bIsFinished = true;
					pParm->bSuccess = true;
					mpp_err_msg(logInfo, progname, "restore succeeded for source dbid %d, target dbid %d on host %s\n",
								sSegDB->dbid, tSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"));
				}
				else if (strcasecmp(pszNotifyRelNameFail, pNotify->relname) == 0)
				{
					g_b_SendCancelMessage = true;
					bIsFinished = true;
					pParm->bSuccess = false;
					pqBuffer = createPQExpBuffer();
					/* Make call to get error message from file on server */
					ReadBackendBackupFileError(pConn, dir_buf->data, pszKey, BFT_RESTORE_STATUS, progname, pqBuffer);
					pParm->pszErrorMsg = MakeString("%s", pqBuffer->data);
					destroyPQExpBuffer(pqBuffer);

					mpp_err_msg(logError, progname, "restore failed for source dbid %d, target dbid %d on host %s\n",
								sSegDB->dbid, tSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"));
				}
			}

			PQfreemem(pNotify);
		}

		if (nNotifies == 0)
		{
			if (!bIsStarted)
			{
				nTries++;
				/* increase timeout to 10min for heavily loaded system */
				if (nTries == 300)
				{
					bIsFinished = true;
					pParm->bSuccess = false;
					pParm->pszErrorMsg = MakeString("restore failed to start for source dbid %d, target dbid %d on host %s in the required time interval\n",
													sSegDB->dbid, tSegDB->dbid, StringNotNull(sSegDB->pszHost, "localhost"));
					DoCancelNotifyListen(pConn, false, pszKey, sSegDB->role, sSegDB->dbid, tSegDB->dbid, NULL);
					bSentCancelMessage = true;
				}
			}
		}
	}

	/*
	 * If segment reports success or no errors so far, scan of the restore status file
	 * for ERRORS and report them if found
	 */
	if(pParm->bSuccess || pParm->pszErrorMsg == NULL)
	{
		pqBuffer = createPQExpBuffer();
		int status = ReadBackendBackupFileError(pConn, dir_buf == NULL ? "" : dir_buf->data, pszKey, BFT_RESTORE_STATUS, progname, pqBuffer);
		if (status != 0)
		{
			pParm->pszErrorMsg = MakeString("%s", pqBuffer->data);
		}
		destroyPQExpBuffer(pqBuffer);
	}

	if (pszNotifyRelName != NULL)
		free(pszNotifyRelName);
	if (pszNotifyRelNameStart != NULL)
		free(pszNotifyRelNameStart);
	if (pszNotifyRelNameSucceed != NULL)
		free(pszNotifyRelNameSucceed);
	if (pszNotifyRelNameFail != NULL)
		free(pszNotifyRelNameFail);

	PQfinish(pConn);

	return (NULL);
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
 * It simply sets a global variable, which is checked by each thread to see whether it
 * should cancel the restore running on the remote database.
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

bool
restoreMaster(InputOptions * pInputOpts,
			  PGconn *pConn,
			  SegmentDatabase *sSegDB,
			  SegmentDatabase *tSegDB,
			  ThreadParm * pParm)
{
	bool		rtn = false;
	bool		bForcePassword = false;
	/*bool		bIgnoreVersion = false;*/

	memset(pParm, 0, sizeof(ThreadParm));

	/* We need to restore the master */
	mpp_err_msg(logInfo, progname, "Starting to restore the master database.\n");

	if (opts != NULL && opts->promptPassword == TRI_YES)
		bForcePassword = true;

	/* Now, spin off a thread to do the restore, and wait for it to finish */
	pParm->thread = 0;
	pParm->pTargetSegDBData = tSegDB;
	pParm->pSourceSegDBData = sSegDB;
	pParm->pOptionsData = pInputOpts;
	pParm->bSuccess = true;
	pParm->pszErrorMsg = NULL;
	pParm->pszRemoteBackupPath = NULL;

	mpp_err_msg(logInfo, progname, "Creating thread to restore master database: host %s port %d database %s\n",
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
				pParm->pTargetSegDBData->port,
				pParm->pTargetSegDBData->pszDBName
		);

	pthread_create(&pParm->thread,
				   NULL,
				   threadProc,
				   pParm);

	pthread_join(pParm->thread, NULL);
	pParm->thread = (pthread_t) 0;

	if (pParm->bSuccess)
	{
		mpp_err_msg(logInfo, progname, "Successfully restored master database: host %s port %d database %s\n",
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
					pParm->pTargetSegDBData->port,
					pParm->pTargetSegDBData->pszDBName
			);
		rtn = true;
	}
	else
	{
		if (pParm->pszErrorMsg != NULL)
			mpp_err_msg(logError, progname, "see error report for details\n");
		else
		{
			mpp_err_msg(logError, progname, "Failed to restore master database: host %s port %d database %s\n",
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
						pParm->pTargetSegDBData->port,
						pParm->pTargetSegDBData->pszDBName
				);
		}
	}

	return rtn;
}


/*
 * spinOffThreads: This function deals with all the threads that drive the backend restores.
 * First, it initializes the ThreadParmArray.  Then it loops and creates each of the threads.
 * It then waits for all threads to finish.
 */
void
spinOffThreads(PGconn *pConn, InputOptions * pInputOpts, const RestorePairArray * restorePairAr, ThreadParmArray * pParmAr)
{
	int			i;

	/*
	 * Create a thread and have it work on executing the dump on the
	 * appropriate segdb.
	 */
	if (!createThreadParmArray(restorePairAr->count, pParmAr))
	{
		mpp_err_msg(logError, progname, "Cannot allocate memory for thread parameters\n");
		exit(-1);
	}

	for (i = 0; i < pParmAr->count; i++)
	{
		ThreadParm *pParm = &pParmAr->pData[i];

		pParm->pSourceSegDBData = &restorePairAr->pData[i].segdb_source;
		pParm->pTargetSegDBData = &restorePairAr->pData[i].segdb_target;
		pParm->pOptionsData = pInputOpts;
		pParm->bSuccess = true;

		/* exclude master node */
		if (pParm->pTargetSegDBData->role == ROLE_MASTER)
			continue;

		mpp_err_msg(logInfo, progname, "Creating thread to restore dbid %d (%s:%d) from backup file on dbid %d (%s:%d)\n",
					pParm->pTargetSegDBData->dbid,
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
					pParm->pTargetSegDBData->port,
					pParm->pSourceSegDBData->dbid,
				StringNotNull(pParm->pSourceSegDBData->pszHost, "localhost"),
					pParm->pSourceSegDBData->port);

		pthread_create(&pParm->thread,
					   NULL,
					   threadProc,
					   pParm);
	}

	mpp_err_msg(logInfo, progname, "Waiting for all remote %s programs to finish.\n", pszAgent);

	/* Wait for all threads to complete */
	for (i = 0; i < pParmAr->count; i++)
	{
		ThreadParm *pParm = &pParmAr->pData[i];

		/* exclude master node and not valid node */
		if (pParm->thread == (pthread_t) 0)
			continue;
		pthread_join(pParm->thread, NULL);
		pParm->thread = (pthread_t) 0;
	}
}

/*
 * freeThreadParmArray: This function frees the memory allocated for the pData element of the
 * ThreadParmArray, as well as the strings allocated within each element
 * of the array.  It does not free the pParmAr itself.
 */
void
freeThreadParmArray(ThreadParmArray * pParmAr)
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
 * createThreadParmArray: This function initializes the count and pData elements of the ThreadParmArray.
 */
bool
createThreadParmArray(int nCount, ThreadParmArray * pParmAr)
{
	int			i;

	pParmAr->count = nCount;
	pParmAr->pData = (ThreadParm *) malloc(nCount * sizeof(ThreadParm));
	if (pParmAr->pData == NULL)
		return false;

	for (i = 0; i < nCount; i++)
	{
		ThreadParm *p = &pParmAr->pData[i];

		p->thread = 0;
		p->pSourceSegDBData = NULL;
		p->pTargetSegDBData = NULL;
		p->pOptionsData = NULL;
		p->bSuccess = true;
		p->pszErrorMsg = NULL;
		p->pszRemoteBackupPath = NULL;
	}

	return true;
}

/*
 * reportRestoreResults: This function outputs a summary of the restore results to a file or to stdout
 */
int
reportRestoreResults(const char *pszReportDirectory, const ThreadParm * pMasterParm, const ThreadParmArray * pParmAr)
{
	InputOptions *pOptions;
	PQExpBuffer reportBuf = NULL;
	FILE	   *fRptFile = NULL;
	char	   *pszReportPathName = NULL;
	char	   *pszFormat;
	int			i;
	int			failCount;
	int			errorCount;
	const ThreadParm *pParm;
	char	   *pszStatus;
	char	   *pszMsg;

	assert(pParmAr != NULL && pMasterParm != NULL);

	if (pParmAr->count == 0 && pMasterParm->pOptionsData == NULL)
	{
		mpp_err_msg(logInfo, progname, "No results to report.");
		return 0;
	}

	pOptions = (pParmAr->count > 0) ? pParmAr->pData[0].pOptionsData : pMasterParm->pOptionsData;
	assert(pOptions != NULL);


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
			pszReportDirectory = getenv("MASTER_DATA_DIRECTORY");
		}
		else
		{
			/* $MASTER_DATA_DIRECTORY not set. Default to current directory */
			pszReportDirectory = "./";
		}
	}

	/* See whether we can create the file in the report directory */
	if (pszReportDirectory[strlen(pszReportDirectory) - 1] != '/')
		pszFormat = "%s/%sgp_restore_%s.rpt";
	else
		pszFormat = "%s%sgp_restore_%s.rpt";

	pszReportPathName = MakeString(pszFormat, pszReportDirectory, dump_prefix_buf == NULL ? "" : dump_prefix_buf->data, pOptions->pszKey);

	if (pszReportPathName == NULL)
	{
		mpp_err_msg(logError, progname, "Cannot allocate memory for report file path\n");
		exit(-1);
	}

	fRptFile = fopen(pszReportPathName, "w");
	if (fRptFile == NULL)
		mpp_err_msg(logWarn, progname, "Cannot open report file %s for writing.  Will use stdout instead.\n",
					pszReportPathName);

	/* buffer to store the complete restore report */
	reportBuf = createPQExpBuffer();

	/*
	 * Write to buffer a report showing the timestamp key, what the command
	 * line options were, which segment databases were backed up, to where,
	 * and any errors that occurred.
	 */
	appendPQExpBuffer(reportBuf, "\nGreenplum Database Restore Report\n");
	appendPQExpBuffer(reportBuf, "Timestamp Key: %s\n", pOptions->pszKey);

	appendPQExpBuffer(reportBuf, "gp_restore Command Line: %s\n",
					  StringNotNull(pOptions->pszCmdLineParms, "None"));

	appendPQExpBuffer(reportBuf, "Pass through Command Line Options: %s\n",
					  StringNotNull(pOptions->pszPassThroughParms, "None"));

	appendPQExpBuffer(reportBuf, "Compression Program: %s\n",
					  StringNotNull(pOptions->pszCompressionProgram, "None"));

	appendPQExpBuffer(reportBuf, "\n");
	appendPQExpBuffer(reportBuf, "Individual Results\n");

	failCount = 0;
	errorCount = 0;
	for (i = 0; i < pParmAr->count + 1; i++)
	{
		if (i == 0)
		{
			if (pMasterParm->pOptionsData != NULL)
				pParm = pMasterParm;
			else
				continue;
		}
		else
		{
			pParm = &pParmAr->pData[i - 1];
			if (pParm->pTargetSegDBData->role == ROLE_MASTER)
				continue;
		}

		pszMsg = pParm->pszErrorMsg;
		if (!pParm->bSuccess)
		{
			pszStatus = "Failed with error: \n{\n";
			failCount++;
		}
		else if (pParm->bSuccess && pszMsg != NULL)
		{
			pszStatus = "Completed but errors were found: \n{\n";
			errorCount++;
		}
		else
			pszStatus = "Succeeded";
		if (pszMsg == NULL)
			pszMsg = "";

		appendPQExpBuffer(reportBuf, "\tRestore of %s on dbid %d (%s:%d) from %s: %s%s%s\n",
						  pParm->pTargetSegDBData->pszDBName,
						  pParm->pTargetSegDBData->dbid,
				StringNotNull(pParm->pTargetSegDBData->pszHost, "localhost"),
						  pParm->pTargetSegDBData->port,
						  StringNotNull(pParm->pszRemoteBackupPath, ""),
						  pszStatus,
						  pszMsg,
						  (strcmp(pszMsg, "") == 0) ? "" : "}"
			);
	}

	if (failCount == 0 && errorCount == 0)
		appendPQExpBuffer(reportBuf, "\n%s  utility finished successfully.\n", progname);
	else if (failCount > 0)
		appendPQExpBuffer(reportBuf, "\n%s  utility finished unsuccessfully with  %d  failures.\n", progname, failCount);
	else
		appendPQExpBuffer(reportBuf, "\n%s  utility finished but errors were found.\n", progname);

	/* write report to report file */
	if (fRptFile != NULL)
	{
		mpp_msg(logInfo, progname, "Report results also written to %s.\n", pszReportPathName);
		fprintf(fRptFile, "%s", reportBuf->data);
		fclose(fRptFile);
	}

	/* write report to stdout */
	fprintf(stdout, "%s", reportBuf->data);

	if (pszReportPathName != NULL)
		free(pszReportPathName);

	destroyPQExpBuffer(reportBuf);

	if (failCount > 0)
		return 1;
	else if (errorCount > 0)
		return 2;
	return 0;
}

/*
 * the same as reportRestoreResults but only for the master. This is a little ugly
 * since we repeat some of the code but we need a majoy overhaul in dump/restore
 * anyhow - so it will do for now.
 */
int
reportMasterError(InputOptions inputopts, const ThreadParm * pMasterParm, const char *localMsg)
{
	InputOptions *pOptions;
	PQExpBuffer reportBuf = NULL;
	FILE	   *fRptFile = NULL;
	char	   *pszReportPathName = NULL;
	char	   *pszFormat;
	int			failCount;
	const ThreadParm *pParm;
	char	   *pszStatus;
	char	   *pszMsg;
	char	   *pszReportDirectory;

	assert(pMasterParm != NULL);

	if (pMasterParm->pOptionsData == NULL)
	{
		pOptions = &inputopts;
	}
	else
	{
		pOptions = pMasterParm->pOptionsData;
	}

	/* generate a timestamp key if we haven't already */
	if (!pOptions->pszKey)
		pOptions->pszKey = GenerateTimestampKey();

	/*
	 * Set the report directory if not set by user (with --gp-r)
	 */
	pszReportDirectory = pOptions->pszReportDirectory;
	if (pszReportDirectory == NULL || *pszReportDirectory == '\0')
	{

		if (getenv("MASTER_DATA_DIRECTORY") != NULL)
		{
			/*
			 * report directory not set by user - default to
			 * $MASTER_DATA_DIRECTORY
			 */
			pszReportDirectory = getenv("MASTER_DATA_DIRECTORY");
		}
		else
		{
			/* $MASTER_DATA_DIRECTORY not set. Default to current directory */
			pszReportDirectory = "./";
		}
	}

	/* See whether we can create the file in the report directory */
	if (pszReportDirectory[strlen(pszReportDirectory) - 1] != '/')
		pszFormat = "%s/gp_restore_%s.rpt";
	else
		pszFormat = "%sgp_restore_%s.rpt";

	pszReportPathName = MakeString(pszFormat, pszReportDirectory, pOptions->pszKey);

	if (pszReportPathName == NULL)
	{
		mpp_err_msg(logError, progname, "Cannot allocate memory for report file path\n");
		exit(-1);
	}

	fRptFile = fopen(pszReportPathName, "w");
	if (fRptFile == NULL)
		mpp_err_msg(logWarn, progname, "Cannot open report file %s for writing.  Will use stdout instead.\n",
					pszReportPathName);

	/* buffer to store the complete restore report */
	reportBuf = createPQExpBuffer();

	/*
	 * Write to buffer a report showing the timestamp key, what the command
	 * line options were, which segment databases were backed up, to where,
	 * and any errors that occurred.
	 */
	appendPQExpBuffer(reportBuf, "\nGreenplum Database Restore Report\n");
	appendPQExpBuffer(reportBuf, "Timestamp Key: %s\n", pOptions->pszKey);

	appendPQExpBuffer(reportBuf, "gp_restore Command Line: %s\n",
					  StringNotNull(pOptions->pszCmdLineParms, "None"));

	appendPQExpBuffer(reportBuf, "Pass through Command Line Options: %s\n",
					  StringNotNull(pOptions->pszPassThroughParms, "None"));

	appendPQExpBuffer(reportBuf, "Compression Program: %s\n",
					  StringNotNull(pOptions->pszCompressionProgram, "None"));

	appendPQExpBuffer(reportBuf, "\n");
	appendPQExpBuffer(reportBuf, "Individual Results\n");


	failCount = 1;

	pParm = pMasterParm;

	pszStatus = "Failed with error: \n{";
	if (localMsg != NULL)
		pszMsg = (char *) localMsg;
	else
		pszMsg = pParm->pszErrorMsg;
	if (pszMsg == NULL)
		pszMsg = "";

	appendPQExpBuffer(reportBuf, "\tRestore of database \"%s\" on Master database: %s%s%s\n",
					  StringNotNull(pOptions->pszDBName, "UNSPECIFIED"),
					  pszStatus,
					  pszMsg,
					  (strcmp(pszMsg, "") == 0) ? "" : "}"
		);

	appendPQExpBuffer(reportBuf, "\n%s  utility finished unsuccessfully with 1 failure.\n", progname);

	/* write report to report file */
	if (fRptFile != NULL)
	{
		mpp_msg(logInfo, progname, "Report results also written to %s.\n", pszReportPathName);
		fprintf(fRptFile, "%s", reportBuf->data);
		fclose(fRptFile);
	}

	/* write report to stdout */
	fprintf(stdout, "%s", reportBuf->data);

	if (pszReportPathName != NULL)
		free(pszReportPathName);

	destroyPQExpBuffer(reportBuf);

	return failCount;
}

/*
 * addPassThroughLongParm: this function adds a long option to the string of pass-through parameters.
 * These get sent to each backend and get passed to the gp_dump_agent program.
 */
static char *
addPassThroughLongParm(const char *Parm, const char *pszValue, char *pszPassThroughParmString)
{
        char       *pszRtn;
        bool            bFirstTime = (pszPassThroughParmString == NULL);

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
