/*-------------------------------------------------------------------------
 *
 * cdb_ddboost_util.c
 *	  gpddboost is a utility for accessing Data Domain's ddboost
 *		  interfaces.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "postgres_fe.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"		/* Builtin functions, including lower() */
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <pthread.h>
#include "getopt_long.h"
#include "pqexpbuffer.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "libpq-fe.h"
#include "fe-auth.h"
#include "pg_backup.h"
#include "cdb_table.h"
#include "cdb_dump_util.h"
#include "cdb_backup_state.h"
#include "cdb_dump.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "libpq/pqsignal.h"

/*
 * Note: zlib.h must be included *after* libpq-fe.h, because the latter may
 * include ssl.h, which has a naming conflict with zlib.h.
 */
#include <zlib.h>
#define GZCLOSE(fh) gzclose(fh)
#define GZWRITE(p, s, n, fh) gzwrite(fh, p, (n)*(s))
#define GZGETS(p, n, fh) gzgets(fh, p, n)
#define GZOPEN(path, mode) gzopen(path, mode)
#define GZDOPEN(path, mode) gzdopen(path, mode)

#define DDWRITE(p, s, n, fh, compress) (((compress) == 1)? (GZWRITE(p, s, n, fh)) : (fwrite(p, s, n, fh)))
#define DDCLOSE(fh, compress) (((compress) == 1) ? (GZCLOSE(fh)) : (fclose(fh)))
#define DDGETS(p, n, fh, compress) (((compress) == 1) ? (GZGETS(p, n, fh)) : (fgets(p, n, fh)))
#define DDOPEN(path, mode, compress) (((compress) == 1) ? ((void *)GZOPEN(path, mode)) : ((void *)fopen(path, mode)))
#define DDDOPEN(path, mode, compress) (((compress) == 1) ? ((void *)GZDOPEN(path, mode)) : ((void *)fdopen(path, mode)))

#ifdef USE_DDBOOST
#include "ddp_api.h"

#define MAX_PATH_NAME 1024
#define DDB_POOL_SIZE (32 * 1024 * 2048)

#define DDP_FILECOPY_EXTENT_LEN 262144

#define COPY_WITHIN_SAME_DDBOOST 0
#define REPLICATE_LOCAL_TO_REMOTE 1
#define REPLICATE_REMOTE_TO_LOCAL 2

/* This is necessary on platforms where optreset variable is not available.
 * Look at "man getopt" documentation for details.
 */
#ifndef HAVE_OPTRESET
int			optreset;
#endif

static ddp_inst_desc_t ddp_inst = DDP_INVALID_DESCRIPTOR;
static ddp_conn_desc_t ddp_conn = DDP_INVALID_DESCRIPTOR;
static int	dd_boost_buf_size = 512 * 1024;
static char *DEFAULT_BACKUP_DIRECTORY = NULL;

struct schemaTableList
{
	char	   *tableName;
	char	   *schemaName;
	struct schemaTableList *next;
};

struct ddboost_options
{
	char	   *timestamp;
	char	   *directory;
	char	   *from_file;
	char	   *to_file;
	char	   *deleteDir;
	char	   *deleteFile;
	char	   *hostname;
	char	   *user;
	char	   *password;
	char	   *default_backup_directory;
	bool		show_config;
	bool		remote;
	char	   *log_level;
	char	   *log_size;
	char	   *listDirFull;
	struct schemaTableList *tableList;
	char	   *database;
	char	   *syncFilesFromTimestamp;
	bool		listDir;
	bool		copyFromDDBoost;
	bool		copyToDDBoost;
	bool		copy;
	bool		readFile;
	bool		getFreePercent;
	bool		getLatestTimestamp;
	bool		syncFiles;
	bool		writeToDDFileFromInput;
	bool		rename;
	bool		setCredential;
	bool		verify;
	bool		replicate;
	bool		recover;
	bool		get_stream_counts;
	char	   *ddboost_storage_unit;
};

static struct ddboost_options *dd_options = NULL;

/* Forward decls */
static bool fillInputOptions(int argc, char **argv, InputOptions * pInputOpts);
static bool parmValNeedsQuotes(const char *Value);
static void usage(void);
static int	readFromDDFile(FILE *fp, char *ddBoostFileName, char *ddboost_storage_unit);
static int	readFromDDFileToOutput(char *ddBoostFileName, char *ddboost_storage_unit);
static int	writeToDDFileFromInput(struct ddboost_options *dd_options);
static int	writeToDDFile(FILE *fp, char *ddBoostFileName, char *ddboost_storage_unit);
static void Replication_CancellationHandler(SIGNAL_ARGS);
static bool ReplicationCancellationRequested = false;

typedef struct option optType;

static RestoreOptions *opts = NULL;

static const char *logError = "ERROR";
const char *progname;

static char *addPassThroughLongParm(const char *Parm, const char *pszValue, char *pszPassThroughParmString);
static int	dd_boost_enabled = 1;

extern ddp_client_info_t dd_client_info;

static int	readFileFromDDBoost(struct ddboost_options *dd_options);
static int	copyFileFromDDBoost(struct ddboost_options *dd_options);
static int	copyFileToDDBoost(struct ddboost_options *dd_options);
static int	ddBoostRmdir(char *dir_path, ddp_conn_desc_t ddp_conn, char *parent_dir, char *ddboost_storage_unit);
static int	deleteDir(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static int	deleteFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static float getFreePercent(struct ddboost_options *dd_options);
static int	createDDboostDir(ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit, const char *path_name);
static int	listDirectory(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static int	createFakeRestoreFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static int	splitSchemaAndTableName(char *schemaTable, char **schema, char **table);
static int	lineStartsWithSchema(char *line, struct schemaTableList *tableList, char **curSchema);
static int	lineStartsWithTable(char *line, struct schemaTableList *tableList, char *curSchema);
static int	lineEndsWith(char *line, char *pattern);
static int	getLatestTimestamp(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath, char *database);
static int	dumpFileHasDatabaseName(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath, char *database);
static int	syncFilesFromDDBoost(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath);
static int	isFileToBeCopied(const char *);
static int	copyFilesFromDir(const char *fromDir, char *toDir, ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit);
static struct schemaTableList *getNode(char *tableName);
static void insertTableName(char *table, struct ddboost_options ** dd_options);
static void deleteTableName(char *table, char *schema, struct ddboost_options ** dd_options);
static void cleanupTableList(struct ddboost_options ** dd_options);
static int	isFileCompressed(const char *full_path);
static int	createDbdumpsDir(char *filePath);
static int	listDirectoryFull(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static int	syncFilesFromDDBoostTimestamp(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath);
static int	renameFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn);
static int	setCredential(struct ddboost_options *dd_options);
static int	copyWithinDDboost(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, int direction);
static int	get_stream_counts(ddp_conn_desc_t connd, ddp_stream_counts_t * stream_counts);

/* Helper routines for handling multiple -T options using a linked list */
struct schemaTableList *
getNode(char *schemaTable)
{
	char	   *schema = NULL,
			   *table = NULL;

	struct schemaTableList *temp = (struct schemaTableList *) malloc(sizeof(struct schemaTableList));

	if (!temp)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		return NULL;
	}

	temp->next = NULL;
	/* Get the schema and table name from the string */
	splitSchemaAndTableName(schemaTable, &schema, &table);
	temp->tableName = strdup(table);
	temp->schemaName = strdup(schema);

	free(schema);
	free(table);

	return temp;
}

void
insertTableName(char *schemaTable, struct ddboost_options ** dd_options)
{
	struct schemaTableList *temp = NULL;

	temp = getNode(schemaTable);
	temp->next = (*dd_options)->tableList;
	(*dd_options)->tableList = temp;
}

void
deleteTableName(char *table, char *schema, struct ddboost_options ** dd_options)
{
	struct schemaTableList *prev = NULL,
			   *cur = (*dd_options)->tableList;

	while (cur != NULL)
	{
		prev = cur;
		cur = cur->next;

		if ((!strcmp(table, prev->tableName)) && (!strcmp(schema, prev->schemaName)))
		{
			if (prev == (*dd_options)->tableList)
				(*dd_options)->tableList = ((*dd_options)->tableList)->next;

			free(prev->tableName);
			free(prev->schemaName);
			free(prev);
		}
	}
}

void
cleanupTableList(struct ddboost_options ** dd_options)
{
	struct schemaTableList *prev = (*dd_options)->tableList,
			   *cur = NULL;

	while (prev != NULL)
	{
		cur = prev->next;
		deleteTableName(prev->tableName, prev->schemaName, dd_options);
		prev = cur;
	}
}

int
main(int argc, char **argv)
{
	int			failCount = 0;
	int			ret = 0;
	bool		createStorageUnit = false;

	/* This struct holds the values of the command line parameters */
	InputOptions inputOpts;

	dd_options = (struct ddboost_options *) malloc(sizeof(struct ddboost_options));
	memset(dd_options, 0, sizeof(struct ddboost_options));

	progname = get_progname(argv[0]);

	/* Parse command line for options */
	if (!fillInputOptions(argc, argv, &inputOpts))
	{
		failCount = 1;
		mpp_err_msg(logError, progname, "Error in InputOptions.\n");
		goto cleanup;
	}

	if (dd_options->setCredential)
	{
		failCount = setCredential(dd_options);
		goto cleanup;
	}

	if (dd_options->show_config)
	{
		char	   *dd_boost_hostname = NULL;
		char	   *dd_boost_username = NULL;
		char	   *default_backup_directory = NULL;
		char	   *dd_boost_passwd = NULL;
		char	   *log_level = NULL;
		char	   *log_size = NULL;
		char	   *ddboost_storage_unit = NULL;
		int			err;

		err = getDDBoostCredential(&dd_boost_hostname,
								   &dd_boost_username,
								   &dd_boost_passwd,
								   &log_level,
								   &log_size,
								   &default_backup_directory,
								   &ddboost_storage_unit,
								   dd_options->remote);
		if (err)
		{
			mpp_err_msg("ERROR", "ddboost", "Parsing DDBoost login credentials failed\n");
			return -1;
		}

		printf("Data Domain Hostname:%s\n", dd_boost_hostname);
		printf("Data Domain Boost Username:%s\n", dd_boost_username);
		if (!dd_options->remote)
		{
			printf("Default Backup Directory:%s\n", default_backup_directory);
			printf("Data Domain Storage Unit:%s\n", ddboost_storage_unit);
		}
		printf("Data Domain default log level:%s\n", log_level);
		printf("Data Domain default log size:%s\n", log_size);

		if (dd_boost_hostname)
			free(dd_boost_hostname);
		if (dd_boost_username)
			free(dd_boost_username);
		if (default_backup_directory)
			free(default_backup_directory);
		if (dd_boost_passwd)
			free(dd_boost_passwd);
		if (log_level)
			free(log_level);
		if (log_size)
			free(log_size);
		goto cleanup;
	}

	/* When --verify is specified - we always create Storage Unit */
	if (dd_options->verify == true)
		createStorageUnit = true;

	int			err;

	err = ddp_init("gp_dump");
	if (err != DD_ERR_NONE)
	{
		mpp_err_msg("ERROR", "ddboost", "ddboost init failed. Err = %d\n", err);
		ret = err;
		failCount = 1;
		goto cleanup;
	}


	ret = initDDSystem(&ddp_inst, &ddp_conn, &dd_client_info, &dd_options->ddboost_storage_unit, createStorageUnit, &DEFAULT_BACKUP_DIRECTORY, dd_options->remote);

	if (ret)
	{
		/*
		 * The main reason for this 'if', is that we want to keep the same
		 * behaviour
		 */
		/* for all other cases when --verify is not specified							*/
		if (dd_options->verify)
		{
			mpp_err_msg(logError, progname, "Could not connect to DD_host with DD_user and the DD_password.\n");
		}
		else
		{
			mpp_err_msg(logError, progname, "Initialization of ddboost failed. Check login credentials\n");
		}
		failCount = 1;
		goto cleanup;
	}

	if (dd_options->get_stream_counts)
	{
		ddp_stream_counts_t stream_counts;

		failCount = get_stream_counts(ddp_conn, &stream_counts);
		goto cleanup;
	}

	/* Take appropriate actions here */
	if (dd_options->copyFromDDBoost)
	{
		failCount = copyFileFromDDBoost(dd_options);
		goto cleanup;
	}

	if (dd_options->copyToDDBoost)
	{
		failCount = copyFileToDDBoost(dd_options);
		goto cleanup;
	}

	if (dd_options->copy)
	{
		failCount = copyWithinDDboost(dd_options, ddp_conn, COPY_WITHIN_SAME_DDBOOST);
		goto cleanup;
	}

	if (dd_options->replicate)
	{
		failCount = copyWithinDDboost(dd_options, ddp_conn, REPLICATE_LOCAL_TO_REMOTE);
		goto cleanup;
	}

	if (dd_options->recover)
	{
		failCount = copyWithinDDboost(dd_options, ddp_conn, REPLICATE_REMOTE_TO_LOCAL);
		goto cleanup;
	}

	if (dd_options->deleteDir)
	{
		failCount = deleteDir(dd_options, ddp_conn);
		goto cleanup;
	}

	if (dd_options->deleteFile)
	{
		failCount = deleteFile(dd_options, ddp_conn);
		goto cleanup;
	}

	if (dd_options->getFreePercent)
	{
		return getFreePercent(dd_options);
	}

	if (dd_options->readFile)
	{
		failCount = readFileFromDDBoost(dd_options);
		goto cleanup;
	}

	if (dd_options->listDir)
	{
		if (!dd_options->directory)
		{
			mpp_err_msg(logError, progname, "Directory not specified for listDir\n");
			goto cleanup;
		}

		failCount = listDirectory(dd_options, ddp_conn);
		goto cleanup;
	}

	if (dd_options->listDirFull)
	{
		failCount = listDirectoryFull(dd_options, ddp_conn);
		goto cleanup;
	}

	if (dd_options->tableList)
	{
		failCount = createFakeRestoreFile(dd_options, ddp_conn);
		cleanupTableList(&dd_options);
		goto cleanup;
	}

	if (dd_options->getLatestTimestamp)
	{
		failCount = getLatestTimestamp(dd_options, ddp_conn, NULL, dd_options->database);
		goto cleanup;
	}

	if (dd_options->syncFiles)
	{
		failCount = syncFilesFromDDBoost(dd_options, ddp_conn, dd_options->directory);
		goto cleanup;
	}

	if (dd_options->syncFilesFromTimestamp)
	{
		failCount = syncFilesFromDDBoostTimestamp(dd_options, ddp_conn, dd_options->directory);
		goto cleanup;
	}

	if (dd_options->writeToDDFileFromInput)
	{
		failCount = writeToDDFileFromInput(dd_options);
		goto cleanup;
	}

	if (dd_options->rename)
	{
		failCount = renameFile(dd_options, ddp_conn);
		goto cleanup;
	}
cleanup:
	FreeInputOptions(&inputOpts);
	if (opts != NULL)
		free(opts);

	if (ddp_conn != DDP_INVALID_DESCRIPTOR)
	{
		err = ddp_disconnect(ddp_conn);
		if (err)
			mpp_err_msg(logError, progname, "ddp_disconnect failed with Err = %d\n", err);
	}

	if (ddp_inst != DDP_INVALID_DESCRIPTOR)
		ddp_instance_destroy(ddp_inst);

	ddp_shutdown();

	if (dd_options != NULL)
		free(dd_options);

	return (failCount == 0 ? 0 : 1);
}

static void
usage(void)
{
	printf(("\n%s Utility routine to access DDBoost from Greenplum Database\n\n"), progname);
	printf("WARNING !! gpddboost should not be used as a standalone utility\n\n");
	printf(("  %s [OPTIONS]\n"), progname);
	printf(("\nGeneral options:\n"));
	printf("  --copyFromDDBoost   --from-file=<ddboost_full_path_name>   --to-file=<local_disk_full_path_name> \n");
	printf("  Copy a file from DDBoost to GPDB\n");
	printf("  The destination directory on local disk should exist.\n\n\n");
	printf("  --copyToDDBoost    --from-file=<local_disk_full_path_name> --to-file=<ddboost_full_path_name> \n");
	printf("  Copy a file from GPDB to DDBoost.\n\n\n");
	printf("  --copy   --from-file=<ddboost_source_full_path_name> --to-file=<ddboost_dest_full_path_name> \n");
	printf("  Copy a file from one location to another within DDBoost.\n\n\n");
	printf("  --del-dir=<ddboost_full_path_name>\n");
	printf("  To delete a directory on DDboost specified by ddboost_full_path_name.\n\n\n");
	printf("  --del-file=<ddboost_full_path_name>\n");
	printf("  To delete a specific file on DDboost.\n\n\n");
	printf("  --listDirectory  --dir=<ddboost_directory_full_path_name>\n");
	printf("  To list the contents of a directory on DDboost. \n");
	printf("  If ddboost_directory_full_path_name is empty or / then the contents of the root directory\n");
	printf("  in the default storage unit is displayed.\n\n\n");
	printf("  --ls   --dir=<ddboost_directory_full_path_name> \n");
	printf("  To list the size and permissions of the contents of a directory on DDboost.\n");
	printf("  If ddboost_directory_full_path_name is empty or / then the contents of the root directory \n");
	printf("  in the default storage unit is displayed.\n\n\n");
	printf("  --readFile       --from-file=<ddboost_full_path_name>\n");
	printf("  To read a file on DDBoost. By default the output is redirected to standard output.\n\n\n");
	printf("  --write-file-from-stdin  --to-file=<ddboost_full_path_name>\n");
	printf("  To write a file to DDBoost from stdin.\n\n\n");
	printf("  --table=<schemaName.tableName>  --from-file=<ddboost_full_path_name>  --to-file=<local_disk_full_path_name>\n");
	printf("  To create a fake backup file containing only the data for the specified table.\n");
	printf("  The destination directory should exist on the local disk.\n\n\n");
	printf("  --setCredential  --hostname <DD_host>  --user=<DD_user>  --defaultBackupDirectory <DD_backup_directory>\n");
	printf("                   [--password=<DD_password>  --logLevel=<NONE, ERROR, WARN, INFO, DEBUG> --logSize=<1-1000 (MB)>]\n");
	printf("                   [--ddboost-storage-unit=<storage_unit_name>]\n");
	printf("  Set the ddboost login credentials. Hostname, user and default backup directory are required.\n");
	printf("  If the password option is not specified, it will be interactivly requested.\n");
	printf("  If the log options are not specified, the default values will be used (level will be DEBUG and size 50 MB).\n");
	printf("  DDBoost storage unit is optional, default value is \"GPDB\".\n\n\n");
	printf("  --sync    --dir=<local_disk_full_path_name> \n");
	printf("  To copy over the config, global dump, post_data and cdatabase files from DDBoost to GPDB\n");
	printf("  Generally we specify the master data directory as the destination.\n\n\n");
	printf("  --syncTimestamp=<timestamp>     --dir=<local_disk_full_path_name>\n");
	printf("  To sync all the files having a particular timestamp from DDboost to local disk\n");
	printf("  The destination is specified by local_disk_full_path_name and it needs to exist.\n\n\n");
	printf("  --dd_boost_buf_size=<size_in_bytes>\n");
	printf("  DDboost I/O buffer size. Max IO buffer size is 1MB\n\n\n");
	printf("  --rename  --from-file=<ddboost_full_path_name_source>  --to-file=<ddboost_full_path_name_dest>\n");
	printf("  Rename file specified by --from-file to a destination specified by --to-file on DDBoost\n");
	printf("  Warning !! This operation will overwrite/delete an existing file if specified as destination.\n\n\n");
	printf("  --verify\n");
	printf("  Verify the DD_user and the DD_password on the DD_host\n");
	printf("  This will also create the Storage Unit.\n\n\n");
	printf("  --replicate --from-file=<ddboost_full_path_name_primaryDD> --to-file=<ddboost_full_path_name_replicationDD>\n");
	printf("  Replicate a file from primary Data Domain system to replication Data Domain system.\n\n\n");
	printf("  --recover --from-file=<ddboost_full_path_name_replicationDD> --to-file=<ddboost_full_path_name_primaryDD>\n");
	printf("  Recover a file from replication Data Domain system to primary Data Domain system.\n\n\n");
	printf("  --ddboost-storage-unit=<storage_unit_name>\n");
	printf("  DDBoost storage unit is only used with other options, and it is optional.\n");
	printf("  If not specified when using options other than --setCredential, it defaults to the one\n");
	printf("  in the primary DDBoost server's credential configuration file.\n\n\n");
	printf("  --show-config\n");
	printf("  Show DD Boost and MFR related configuration information: DD hostname, username, default backup directory\n\n\n");
	printf("  --get_stream_counts\n");
	printf("  Show DD Boost and MFR I/O stream limit and usage information.\n\n\n");
	printf("  --remote\n");
	printf("  When combined with other options, this option will redirect operation to remote Data Domain system\n");
	printf("  Warning!! This option could not be combined with options that are not targeted at a single Data Domain system,\n");
	printf("  such as --replicate and --recover.\n\n\n");
	printf("  --help\n");
	printf("  Show this help, then exit.\n\n\n");
	printf("  --version\n");
	printf("  Output version information, then exit.\n\n\n");

}

bool
fillInputOptions(int argc, char **argv, InputOptions * pInputOpts)
{
	int			c;
	extern int	optind;
	extern char *optarg;
	int			lenCmdLineParms;
	int			i;

	struct option cmdopts[] = {
		{"key", required_argument, NULL, 1},
		{"from-file", required_argument, NULL, 2},
		{"to-file", required_argument, NULL, 3},
		{"copyFromDDBoost", no_argument, NULL, 4},
		{"copyToDDBoost", no_argument, NULL, 5},
		{"del-dir", required_argument, NULL, 6},
		{"getFreePercent", no_argument, NULL, 7},
		{"del-file", required_argument, NULL, 8},
		{"readFile", no_argument, NULL, 9},
		{"listDirectory", no_argument, NULL, 10},
		{"table", required_argument, NULL, 11},
		{"getLatestTimestamp", no_argument, NULL, 12},
		{"dir", required_argument, NULL, 13},
		{"database", required_argument, NULL, 14},
		{"sync", no_argument, NULL, 15},
		{"write-file-from-stdin", no_argument, NULL, 16},
		{"dd_boost_buf_size", required_argument, NULL, 18},
		{"ls", required_argument, NULL, 19},
		{"syncTimestamp", required_argument, NULL, 20},
		{"rename", no_argument, NULL, 21},
		{"copy", no_argument, NULL, 22},

		{"setCredential", no_argument, NULL, 23},
		{"hostname", required_argument, NULL, 24},
		{"user", required_argument, NULL, 25},
		{"password", required_argument, NULL, 26},
		{"logLevel", required_argument, NULL, 27},
		{"logSize", required_argument, NULL, 28},
		{"verify", no_argument, NULL, 29},
		{"remote", no_argument, NULL, 30},
		{"defaultBackupDirectory", required_argument, NULL, 31},
		{"show-config", no_argument, NULL, 32},
		{"replicate", no_argument, NULL, 33},
		{"recover", no_argument, NULL, 34},
		{"get_stream_counts", no_argument, NULL, 35},
		{"ddboost-storage-unit", required_argument, NULL, 36},

		{NULL, 0, NULL, 0}
	};

	/* Initialize option fields  */
	memset(pInputOpts, 0, sizeof(InputOptions));
	memset(dd_options, 0, sizeof(*dd_options));

	opts = (RestoreOptions *) calloc(1, sizeof(RestoreOptions));
	if (opts == NULL)
	{
		mpp_err_msg_cache(logError, progname, "error allocating memory for RestoreOptions");
		return false;
	}

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
			printf("gpddboost (%s)\n", dd_client_info.cl_info);
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

	while ((c = getopt_long(argc, argv, "acd:h:ip:RsuU:vwW",
							cmdopts, NULL)) != -1)
	{
		switch (c)
		{
			case 1:
				/* Timestamp */
				dd_options->timestamp = Safe_strdup(optarg);
				pInputOpts->pszPassThroughParms = addPassThroughLongParm("gp-ddb-key", optarg, pInputOpts->pszPassThroughParms);
				dd_boost_enabled = 1;
				break;
			case 2:
				/* Directory name on gpdb */
				dd_options->from_file = Safe_strdup(optarg);
				break;
			case 3:
				/* Directory name on gpdb */
				dd_options->to_file = Safe_strdup(optarg);
				break;
			case 4:
				/* Cleanup the files on DDBoost */
				dd_options->copyFromDDBoost = true;
				break;
			case 5:
				/* Copy File from DDBoost */
				dd_options->copyToDDBoost = true;
				break;
			case 6:
				/* Cleanup specified directory */
				dd_options->deleteDir = Safe_strdup(optarg);
				break;
			case 7:
				/* Get free percent */
				dd_options->getFreePercent = true;
				break;
			case 8:
				/* Cleanup specified file */
				dd_options->deleteFile = Safe_strdup(optarg);
				break;
			case 9:
				/* read file and write to stdout */
				dd_options->readFile = true;
				break;
			case 10:
				/* List directory contents */
				dd_options->listDir = true;
				break;
			case 11:
				/* Schema.Table to restore */
				if (!optarg)
				{
					mpp_err_msg_cache(logError, progname, "Try \"%s --help\" for more information.\n", progname);
					return false;
				}
				insertTableName(optarg, &dd_options);
				break;
			case 12:
				dd_options->getLatestTimestamp = true;
				break;
			case 13:
				dd_options->directory = Safe_strdup(optarg);
				if (!optarg)
				{
					mpp_err_msg_cache(logError, progname, "DDBoost directory name missing\n");
					return false;
				}
				break;
			case 14:
				dd_options->database = Safe_strdup(optarg);
				break;
				/* sync cdatabase, post_data and global dump files */
			case 15:
				dd_options->syncFiles = true;
				break;
			case 16:
				/* Pipe input from stdout to DDboost file */
				dd_options->writeToDDFileFromInput = true;
				break;
			case 18:
				/* set I/O size for DD Boost */
				dd_boost_buf_size = atoi(optarg);
				if (dd_boost_buf_size < 64 * 1024 || dd_boost_buf_size > 1024 * 1024)
				{
					mpp_err_msg(logError, progname, "dd_boost_buf_size shoule be between 64KB and 1MB\n");
					dd_boost_buf_size = 512 * 1024;		/* reset to default
														 * value */
				}
				break;
			case 19:
				/* Long listing of directories */
				dd_options->listDirFull = Safe_strdup(optarg);
				break;
			case 20:
				dd_options->syncFilesFromTimestamp = Safe_strdup(optarg);;
				break;
			case 21:
				dd_options->rename = true;
				break;
			case 22:
				dd_options->copy = true;
				break;

			case 23:
				dd_options->setCredential = true;
				break;
			case 24:
				dd_options->hostname = Safe_strdup(optarg);
				break;
			case 25:
				dd_options->user = Safe_strdup(optarg);
				break;
			case 26:
				dd_options->password = Safe_strdup(optarg);
				break;
			case 27:
				dd_options->log_level = Safe_strdup(optarg);
				break;
			case 28:
				dd_options->log_size = Safe_strdup(optarg);
				break;
			case 29:
				dd_options->verify = true;
				break;
			case 30:
				dd_options->remote = true;
				break;
			case 31:
				dd_options->default_backup_directory = Safe_strdup(optarg);
				break;
			case 32:
				dd_options->show_config = true;
				break;
			case 33:
				dd_options->replicate = true;
				break;
			case 34:
				dd_options->recover = true;
				break;
			case 35:
				dd_options->get_stream_counts = true;
				break;
			case 36:
				dd_options->ddboost_storage_unit = Safe_strdup(optarg);
				break;

			default:
				mpp_err_msg_cache(logError, progname, "Try \"%s --help\" for more information.\n", progname);
				return false;
		}
	}

	/* Check switches combinations and conflicts */
	if (dd_options->setCredential && !(dd_options->hostname && dd_options->user))
	{
		mpp_err_msg_cache(logError, progname, "When specifying the option --setCredential, the options --hostname and --user are required.\n", progname);
		return false;
	}
	else if (!dd_options->setCredential && (dd_options->hostname || dd_options->user || dd_options->password ||
							  dd_options->log_level || dd_options->log_size))
	{
		mpp_err_msg_cache(logError, progname, "The options --hostname and --user must follow the --setCredential option.\n", progname);
		return false;
	}
	else if (dd_options->setCredential && (dd_options->timestamp || dd_options->directory || dd_options->from_file || dd_options->to_file ||
										   dd_options->deleteDir || dd_options->deleteFile || dd_options->database || dd_options->syncFilesFromTimestamp ||
										   dd_options->listDir || dd_options->listDirFull || dd_options->copyFromDDBoost || dd_options->copyToDDBoost ||
										   dd_options->copy || dd_options->readFile || dd_options->getFreePercent || dd_options->getLatestTimestamp ||
										   dd_options->syncFiles || dd_options->writeToDDFileFromInput || dd_options->rename || dd_options->verify))
	{
		mpp_err_msg_cache(logError, progname, "The option --setCredential and its corresponding options are standalone."
						  "They shouldn't be used in conjunction with any other gpddboost option.\n");
		return false;
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

int
readFromDDFile(FILE *fp, char *ddBoostFileName, char *ddboost_storage_unit)
{
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	int			i = 0;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_uint64_t total_bytes = 0;
	size_t		written = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_stat_t	stat_buf;

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddBoostFileName);

	path1.su_name = ddboost_storage_unit;
	path1.path_name = full_path;

	err = ddp_open_file(ddp_conn, &path1, DDP_O_READ, 0400, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost File %s open failed. Err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	buf = (char *) malloc(rw_size);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}

	do
	{
		memset(buf, 0, rw_size);
		ret_count = 0;

		err = ddp_stat(ddp_conn, &path1, &stat_buf);
		if (err)
		{
			mpp_err_msg(logError, progname, "ddboost stat failed on %s with error %d\n", path1.path_name, err);
			break;
		}

		if (stat_buf.st_size < rw_size)
			rw_size = stat_buf.st_size;

		err = ddp_read(handle, buf, rw_size,
					   rw_size * i, &ret_count);
		if (err)
		{
			mpp_err_msg(logError, progname, "ddboost read failed on %s with err %d\n", path1.path_name, err);
			break;
		}

		written = fwrite(buf, 1, ret_count, fp);

		total_bytes += ret_count;
		i++;
	} while ((err == 0) && (ret_count > 0));


cleanup:
	if (handle != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(handle);
	if (buf)
		free(buf);
	if (full_path)
		free(full_path);

	return err;
}

int
copyFileFromDDBoost(struct ddboost_options *dd_options)
{
	char	   *ddboostFile = Safe_strdup(dd_options->from_file);
	char	   *gpdbFile = Safe_strdup(dd_options->to_file);
	FILE	   *fp = NULL;
	int			err = 0;

	if (!ddboostFile)
	{
		mpp_err_msg(logError, progname, "Source File on DDboost not specified\n");
		err = -1;
		goto cleanup;
	}

	if (!gpdbFile)
	{
		mpp_err_msg(logError, progname, "Destination file on GPDB not specified\n");
		err = -1;
		goto cleanup;
	}

	fp = fopen(gpdbFile, "w");
	if (fp == NULL)
	{
		mpp_err_msg(logError, progname, "Cannot open file %s on GPDB\n", gpdbFile);
		err = -1;
		goto cleanup;
	}

	/* Close ddboostFile in the called function */
	err = readFromDDFile(fp, ddboostFile, dd_options->ddboost_storage_unit);

cleanup:
	if (gpdbFile)
		free(gpdbFile);
	if (fp)
		fclose(fp);
	if (ddboostFile)
		free(ddboostFile);

	return err;

}

int
copyFileToDDBoost(struct ddboost_options *dd_options)
{
	char	   *gpdbFile = Safe_strdup(dd_options->from_file);
	char	   *ddBoostFile = Safe_strdup(dd_options->to_file);
	FILE	   *fp = NULL;
	int			err = 0;

	if (!ddBoostFile)
	{
		mpp_err_msg(logError, progname, "Destination file on DDboost not specified\n");
		err = -1;
		goto cleanup;
	}

	if (!gpdbFile)
	{
		mpp_err_msg(logError, progname, "Source file on GPDB not specified\n");
		err = -1;
		goto cleanup;
	}

	fp = fopen(gpdbFile, "r");
	if (fp == NULL)
	{
		mpp_err_msg(logError, progname, "Cannot open file %s on GPDB\n", gpdbFile);
		err = -1;
		goto cleanup;
	}

	/* Close ddBoostFile in the called function */
	err = writeToDDFile(fp, ddBoostFile, dd_options->ddboost_storage_unit);

cleanup:
	if (gpdbFile)
		free(gpdbFile);

	if (ddBoostFile)
		free(ddBoostFile);

	if (fp)
		fclose(fp);

	return err;

}

int
readFileFromDDBoost(struct ddboost_options *dd_options)
{
	char	   *ddBoostFile = Safe_strdup(dd_options->from_file);
	int			err = 0;

	if (!ddBoostFile)
	{
		mpp_err_msg(logError, progname, "Destination file on DDboost not specified\n");
		return -1;
	}

	err = readFromDDFileToOutput(ddBoostFile, dd_options->ddboost_storage_unit);
	if (ddBoostFile)
		free(ddBoostFile);

	return err;

}

int
deleteDir(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	char	   *ddboostDir = Safe_strdup(dd_options->deleteDir);
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		err = -1;
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostDir;

	err = ddBoostRmdir(ddboostDir, ddp_conn, ddboostDir, dd_options->ddboost_storage_unit);
	if (err)
	{
		mpp_err_msg(logError, progname, "dboost Deleting Directory %s failed with err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

cleanup:
	if (ddboostDir)
		free(ddboostDir);
	if (full_path)
		free(full_path);

	return err;
}

int
ddBoostRmdir(char *dir_path, ddp_conn_desc_t ddp_conn, char *parent_dir, char *ddboost_storage_unit)
{
	ddp_path_t	temp_path = {0};
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	dd_errno_t	err = DD_ERR_NONE;
	ddp_dirent_t ret_dirent;
	char	   *full_path = NULL;
	char	   *cur_path = NULL;

	if (!dir_path)
		return 0;

	temp_path.path_name = dir_path;
	temp_path.su_name = ddboost_storage_unit;

	err = ddp_open_dir(ddp_conn, &temp_path, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", dir_path, err);
		return -1;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", dir_path);

	cur_path = (char *) malloc(MAX_PATH_NAME);
	if (!cur_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failure\n");
		err = -1;
		goto cleanup;
	}
	memset(cur_path, 0, MAX_PATH_NAME);

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failure\n");
		err = -1;
		goto cleanup;
	}
	memset(full_path, 0, MAX_PATH_NAME);

	strcat(full_path, parent_dir);
	if (strcmp(&(full_path[strlen(full_path) - 1]), "/"))
		strcat(full_path, "/");

	/*
	 * Try to remove the directory. If the err is DD_ERR_NOTEMPTY then
	 * recursively delete the contents
	 */
	err = ddp_rmdir(ddp_conn, &temp_path);
	if (err && err != DD_ERR_NOTEMPTY)
	{
		mpp_err_msg(logError, progname, "DDboost Directory %s deletion failed with err %d\n", temp_path.path_name, err);
		err = -1;
		goto cleanup;
	}

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				goto cleanup;
			}
		}

		else
		{
			memset(cur_path, 0, MAX_PATH_NAME);
			strcat(cur_path, full_path);
			strcat(cur_path, ret_dirent.d_name);

			/* Skip deleting . and .. */
			if (strcmp("..", ret_dirent.d_name) == 0)
			{
				continue;
			}
			if (strcmp(".", ret_dirent.d_name) == 0)
			{
				continue;
			}

			/*
			 * Try to unlink the file. If the err is FILE_IS_DIR then call the
			 * same function
			 */
			temp_path.path_name = cur_path;
			err = ddp_unlink(ddp_conn, &temp_path);

			if (err && (err == DD_ERR_FILE_IS_DIR))
			{
				err = ddBoostRmdir(cur_path, ddp_conn, cur_path, ddboost_storage_unit);
				if (!err)
				{
					/* Successfully deleted directory */
					continue;
				}
				else
				{
					/* Error in deleting directory */
					mpp_err_msg(logError, progname, "ddboost Directory %s deletion failed with err %d\n", cur_path, err);
					err = -1;
					goto cleanup;
				}

			}
			else if (err)
			{
				/* Deleting file failed with error */
				mpp_err_msg(logError, progname, "ddboost Directory %s deletion failed with err %d\n", cur_path, err);
				err = -1;
				goto cleanup;
			}
			else
			{
				/* File succesfully deletd */
			}

		}
	}

	temp_path.path_name = dir_path;
	err = ddp_rmdir(ddp_conn, &temp_path);
	if (err)
	{
		/* Directory deletion failed with err */
		mpp_err_msg(logError, progname, "ddboost Directory %s deletion failed with err %d\n", temp_path.path_name, err);
		err = -1;
		goto cleanup;
	}
	else
	{
		/* Successfully deleted directory */
		err = 0;
	}

cleanup:
	if (cur_path)
		free(cur_path);
	if (full_path)
		free(full_path);
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", dir_path);
	}

	return err;
}

static int
writeToDDFile(FILE *fp, char *ddBoostFileName, char *ddboost_storage_unit)
{
	/*
	 * TODO: do we need ddboost_storage_unit as an argument? why not just use
	 * ddboost_option
	 */
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	int			i = 0;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_uint64_t total_bytes = 0;
	ddp_uint64_t read_bytes = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_stat_t	stat_buf;

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddBoostFileName);

	path1.su_name = ddboost_storage_unit;
	path1.path_name = full_path;

	err = createDDboostDir(ddp_conn, ddboost_storage_unit, ddBoostFileName);
	if (err)
	{
		mpp_err_msg(logError, progname, "Creating path %s on ddboost failed. Err %d\n", ddBoostFileName, err);
		err = -1;
		goto cleanup;
	}

	err = ddp_open_file(ddp_conn, &path1, DDP_O_CREAT | DDP_O_RDWR, 0600, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost File %s open failed. Err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	buf = (char *) malloc(rw_size);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}

	do
	{
		memset(buf, 0, rw_size);
		ret_count = 0;

		read_bytes = fread(buf, 1, rw_size, fp);

		err = ddp_write(handle, buf, read_bytes,
						rw_size * i, &ret_count);

		if (err)
		{
			mpp_err_msg(logError, progname, "ddboost write failed on %s with err %d\n", path1.path_name, err);
			err = -1;
			goto cleanup;
		}

		total_bytes += ret_count;
		i++;
	} while ((err == 0) && (ret_count > 0) && (rw_size == read_bytes));

	err = ddp_stat(ddp_conn, &path1, &stat_buf);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost stat failed on %s failed with err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	if (stat_buf.st_size < rw_size)
		rw_size = stat_buf.st_size;

cleanup:
	if (handle != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(handle);

	/* Cleanup the file if copy failed */
	if (err)
		ddp_unlink(ddp_conn, &path1);
	if (full_path)
		free(full_path);
	if (buf)
		free(buf);

	return err;
}

float
getFreePercent(struct ddboost_options *dd_options)
{
	unsigned long long total_bytes = 0;
	unsigned long long free_bytes = 0;
	double		percent = 0.0;
	ddp_statvfs_t statfs_buf;
	int			err = 0;

	err = ddp_statfs(ddp_conn, &statfs_buf);
	if (err)
		return 0.0;
	else
	{
		total_bytes = statfs_buf.f_blocks;
		free_bytes = statfs_buf.f_bfree;
	}

	percent = 100.0 * (double) free_bytes / (double) total_bytes;

	return percent;
}

int
deleteFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	ddp_path_t	temp_path = {0};
	dd_errno_t	err = DD_ERR_NONE;
	char	   *temp = NULL;

	if (!dd_options->deleteFile)
	{
		mpp_err_msg(logError, progname, "ddboost no file specified for delete\n");
		return -1;
	}

	temp = (char *) malloc(MAX_PATH_NAME);
	if (!temp)
	{
		mpp_err_msg(logError, progname, "ddboost memory allocation failed\n");
		return -1;
	}

	memset(temp, 0, MAX_PATH_NAME);

	strcat(temp, dd_options->deleteFile);

	temp_path.path_name = temp;
	temp_path.su_name = dd_options->ddboost_storage_unit;

	err = ddp_unlink(ddp_conn, &temp_path);
	if (err)
	{
		mpp_err_msg(logError, progname, "File %s cannot be deleted on DDboost. Error %d\n", temp_path.path_name, err);
	}

	free(temp);
	return err;
}

int
readFromDDFileToOutput(char *ddBoostFileName, char *ddboost_storage_unit)
{
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	int			i = 0;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_uint64_t total_bytes = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_stat_t	stat_buf;

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, ("Memory allocation failed\n"));
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddBoostFileName);

	path1.su_name = ddboost_storage_unit;
	path1.path_name = full_path;

	err = ddp_open_file(ddp_conn, &path1, DDP_O_READ, 0400, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "File %s open on ddboost failed. Err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	buf = (char *) malloc(rw_size);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}

	do
	{
		memset(buf, 0, rw_size);
		ret_count = 0;

		err = ddp_stat(ddp_conn, &path1, &stat_buf);
		if (err)
		{
			mpp_err_msg(logError, progname, "ddboost stat failed on %s with err %d\n", path1.path_name, err);
			err = -1;
			break;
		}

		if (stat_buf.st_size < rw_size)
			rw_size = stat_buf.st_size;

		err = ddp_read(handle, buf, rw_size,
					   rw_size * i, &ret_count);
		if (err)
		{
			mpp_err_msg(logError, progname, "ddboost read failed on %s with err %d\n", path1.path_name, err);
			err = -1;
			break;
		}

		write(1, buf, ret_count);

		total_bytes += ret_count;
		i++;
	} while ((err == 0) && (ret_count > 0));

cleanup:
	if (handle != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(handle);
	if (buf)
		free(buf);
	if (full_path)
		free(full_path);
	return err;
}

int
createDDboostDir(ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit, const char *filePath)
{
	char	   *pch = NULL;
	ddp_path_t	path = {0};
	int			err = 0;
	char	   *full_path = NULL;
	char	   *path_name = Safe_strdup(filePath);

	if (!filePath)
	{
		mpp_err_msg(logError, progname, "Specify path for directory\n");
		err = -1;
		goto cleanup;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(full_path, 0, MAX_PATH_NAME);
	if (filePath[0] == '/')
		strcat(full_path, "/");

	pch = strtok(path_name, " /");
	while (pch != NULL)
	{
		path.su_name = ddboost_storage_unit;
		strcat(full_path, pch);
		path.path_name = full_path;

		if (strncmp(full_path, filePath, strlen(filePath)))
		{
			err = ddp_mkdir(ddp_conn, &path, 0755);
			if ((err != 0) && (err != DD_ERR_EXIST))
			{
				mpp_err_msg(logError, progname, "mkdir failed. err %d\n", err);
				break;
			}

			strcat(full_path, "/");
			pch = strtok(NULL, " /");
		}
		else
			break;
	}

	/* It is not an error if the directory already exists */
	if (err == DD_ERR_EXIST)
		err = 0;

cleanup:
	if (full_path)
		free(full_path);
	if (path_name)
		free(path_name);
	return err;
}

int
listDirectory(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	char	   *ddboostDir = Safe_strdup(dd_options->directory);
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		err = -1;
		goto cleanup;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		err = -1;
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostDir;

	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostDir);

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "Reading directory %s on ddboost failed. Err %d\n", ddboostDir, err);
				break;
			}
		}

		else
		{
			/* Skip deleting . and .. */
			if (strcmp("..", ret_dirent.d_name) == 0)
			{
				continue;
			}
			if (strcmp(".", ret_dirent.d_name) == 0)
			{
				continue;
			}

			printf("\n%s", ret_dirent.d_name);
		}
	}
	printf("\n");

cleanup:
	if (full_path)
		free(full_path);
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostDir);
	}
	if (ddboostDir)
		free(ddboostDir);
	return err;
}

/* For selective restore we need to parse the specified dump file
 * for psql statements corresponding to the required table
 */
static int
createFakeRestoreFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	int			i = 0;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_uint64_t total_bytes = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_stat_t	stat_buf;
	int			fd[2];
	char		line[101];
	int			done = false;
	char	   *endString = NULL;
	int			output,
				in_schema,
				found;
	char	   *curSchema = NULL;
	void	   *ddfp = NULL,
			   *ddfpTemp = NULL;
	int			isCompress = 0;

	if (!dd_options->to_file)
	{
		mpp_err_msg(logError, progname, "Output file not specified for selective restore\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", dd_options->from_file);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = full_path;

	err = ddp_open_file(ddp_conn, &path1, DDP_O_READ, 0400, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "File %s open on ddboost failed. Err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	buf = (char *) malloc(rw_size);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}

	isCompress = isFileCompressed(full_path);

	/*
	 * Always try to create the Directory db_dumps and db_dumps/<date> before
	 * we start the creation of fake dump files. If the directories already
	 * exist, then ignore the error. This creation is important beacause, when
	 * we do a selective restore of an old backup onto a fresh cluster, the
	 * segment directories wont have the db_dumps and db_dumps/<date>
	 * directories
	 */
	err = createDbdumpsDir(dd_options->to_file);
	if (err)
	{
		mpp_err_msg(logError, progname, "Creating directory %s on ddboost failed. Err %d\n", dd_options->to_file, err);
		err = -1;
		goto cleanup;
	}

	/*
	 * Create two processes. In one write to named pipe. In the other read
	 * from the named pipe and use getline to parse as before. Close the pipe
	 * after the file is exhausted
	 */

	if (pipe(fd) < 0)
	{
		mpp_err_msg(logError, progname, "Creating pipe failed\n");
		err = -1;
		goto cleanup;
	}

	if (fork() == 0)
	{
		/* Child reads from DDboost and writes it to pipe */
		close(fd[0]);
		do
		{
			memset(buf, 0, rw_size);
			ret_count = 0;

			err = ddp_stat(ddp_conn, &path1, &stat_buf);
			if (err)
			{
				mpp_err_msg(logError, progname, "ddboost stat failed on  %s Err %d\n", path1.path_name, err);
				break;
			}

			if (stat_buf.st_size < rw_size)
				rw_size = stat_buf.st_size;

			err = ddp_read(handle, buf, rw_size,
						   rw_size * i, &ret_count);
			if (err)
			{
				mpp_err_msg(logError, progname, "ddboost read failed on %s Err %d\n", path1.path_name, err);
				break;
			}

			write(fd[1], buf, ret_count);

			total_bytes += ret_count;
			i++;
		} while ((err == 0) && (ret_count > 0));
		done = 1;
		close(fd[1]);
	}
	else
	{
		/* Parent reader */
		close(fd[1]);

		ddfp = DDDOPEN(fd[0], "r", isCompress);
		if (ddfp == NULL)
		{
			mpp_err_msg(logError, progname, "ddboost read failed on %s \n", path1.path_name);
			err = -1;
			goto cleanup;
		}

		ddfpTemp = DDOPEN(dd_options->to_file, "w", isCompress);
		if (ddfpTemp == NULL)
		{
			mpp_err_msg(logError, progname, "Opening file %s failed\n", dd_options->to_file);
			err = -1;
			goto cleanup;
		}

		endString = (char *) malloc(MAX_PATH_NAME);
		if (endString == NULL)
		{
			mpp_err_msg(logError, progname, "Memory allocation failed\n");
			err = -1;
			goto cleanup;
		}
		memset(endString, 0, MAX_PATH_NAME);
		snprintf(endString, MAX_PATH_NAME, "\\.");

		output = false;
		in_schema = false;
		found = false;


		do
		{
			memset(line, 0, 101);

			if (DDGETS(line, 100, ddfp, isCompress) != NULL)
			{
				if (lineStartsWithSchema(line, dd_options->tableList, &curSchema))
				{
					DDWRITE(line, 1, strlen(line), ddfpTemp, isCompress);
					in_schema = true;
				}
				else if (in_schema && lineStartsWithTable(line, dd_options->tableList, curSchema))
				{
					found = true;
					output = true;
					DDWRITE(line, 1, strlen(line), ddfpTemp, isCompress);
				}
				else if (lineEndsWith(line, endString))
				{
					if (found && in_schema)
					{
						DDWRITE(line, 1, strlen(line), ddfpTemp, isCompress);
						output = false;
						found = false;
					}
				}
				else if (found && output)
				{
					DDWRITE(line, 1, strlen(line), ddfpTemp, isCompress);
				}
			}
			else
			{
				DDCLOSE(ddfp, isCompress);
				ddfp = NULL;
				DDCLOSE(ddfpTemp, isCompress);
				ddfpTemp = NULL;
				done = true;
			}
		} while (!done);
		free(endString);
		close(fd[0]);
	}

cleanup:
	if (handle != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(handle);
	if (full_path)
		free(full_path);
	if (buf)
		free(buf);
	if (curSchema)
		free(curSchema);
	if (ddfp)
		DDCLOSE(ddfp, isCompress);
	if (ddfpTemp)
		DDCLOSE(ddfpTemp, isCompress);
	return 0;
}

/* check if the line starts with the specified pattern */
int
lineStartsWithSchema(char *line, struct schemaTableList *tableList, char **curSchema)
{
	int			i = 0;
	char		schemaLine[MAX_PATH_NAME];
	struct schemaTableList *cur = tableList;

	/* Skip spaces at the beginning */
	while (line[i] == ' ')
		i++;

	/*
	 * For every schema check if the line begins with SET SEARCH PATH <schema
	 * name>
	 */
	while (cur != NULL)
	{
		memset(schemaLine, 0, MAX_PATH_NAME);
		snprintf(schemaLine, MAX_PATH_NAME, "SET search_path = %s", cur->schemaName);
		if (strstr(line, schemaLine))
		{
			*curSchema = strdup(cur->schemaName);
			return 1;
		}
		cur = cur->next;
	}
	return 0;
}

/* check if the line starts with the specified pattern */
int
lineStartsWithTable(char *line, struct schemaTableList *tableList, char *curSchema)
{
	int			i = 0;
	char		schemaLine[MAX_PATH_NAME];
	struct schemaTableList *cur = tableList;

	/* Skip spaces at the beginning */
	while (line[i] == ' ')
		i++;

	/* For every Table check if the line begins with COPY <table name> */
	while (cur != NULL)
	{
		memset(schemaLine, 0, MAX_PATH_NAME);
		snprintf(schemaLine, MAX_PATH_NAME, "COPY %s", cur->tableName);
		if ((!strcmp(curSchema, cur->schemaName) && (strstr(line, schemaLine))))
			return 1;
		cur = cur->next;
	}
	return 0;
}

/* Check if the line ends with the specified pattern */
int
lineEndsWith(char *line, char *pattern)
{
	if (strstr(line, pattern))
		return 1;
	else
		return 0;

}

int
splitSchemaAndTableName(char *schemaTable, char **schema, char **table)
{
	char	   *pch = NULL;

	pch = strtok(schemaTable, ". ");
	*schema = Safe_strdup(pch);

	pch = strtok(NULL, ". ");
	*table = Safe_strdup(pch);

	return 0;
}

static int
getLatestTimestamp(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath, char *databaseName)
{
	char	   *ddboostDir = Safe_strdup(dd_options->directory);
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;
	char	   *recentTimestamp = NULL;
	char	   *filePath = NULL;
	int			found = 0;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostDir;

	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Directory %s open failed on ddboost. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostDir);

	recentTimestamp = (char *) malloc(MAX_PATH_NAME);
	if (!recentTimestamp)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed in gpddboost\n");
		err = -1;
		goto cleanup;
	}
	memset(recentTimestamp, 0, MAX_PATH_NAME);

	filePath = (char *) malloc(MAX_PATH_NAME);
	if (!filePath)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed in gpddboost\n");
		err = -1;
		goto cleanup;
	}
	memset(filePath, 0, MAX_PATH_NAME);

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "ddboost readdir on %s failed. Err %d\n", ddboostDir, err);
				break;
			}
		}

		else
		{
			/* Skip deleting . and .. */
			if (strcmp("..", ret_dirent.d_name) == 0)
			{
				continue;
			}
			if (strcmp(".", ret_dirent.d_name) == 0)
			{
				continue;
			}

			memset(filePath, 0, MAX_PATH_NAME);
			snprintf(filePath, MAX_PATH_NAME, "%s/%s", ddboostDir, ret_dirent.d_name);

			/* Compare only cdatabase files */
			if (strncmp(ret_dirent.d_name, "gp_cdatabase", strlen("gp_cdatabase")))
			{
				continue;
			}

			/* If database name doesn't match then continue */
			if (!dumpFileHasDatabaseName(dd_options, ddp_conn, filePath, databaseName))
				continue;

			found = 1;

			if (strcmp(ret_dirent.d_name, recentTimestamp) > 0)
			{
				memset(recentTimestamp, 0, MAX_PATH_NAME);
				strncpy(recentTimestamp, ret_dirent.d_name, strlen(ret_dirent.d_name) + 1);
			}
		}
	}

	if (found)
		printf("\n%s\n", &recentTimestamp[strlen(recentTimestamp) - 14]);
	else
		printf("\nNo dump file with the specified database exists\n");

cleanup:
	if (recentTimestamp)
		free(recentTimestamp);
	if (full_path)
		free(full_path);
	if (filePath)
		free(filePath);
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostDir);
	}
	if (ddboostDir)
		free(ddboostDir);
	return err;
}


int
dumpFileHasDatabaseName(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *fullPath, char *databaseName)
{
	/* Read the first 5 lines of the cdatabase file on DDboost */
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0,
				ret = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_stat_t	stat_buf;
	char	   *searchString = NULL;

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		ret = 0;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", fullPath);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = full_path;

	err = ddp_open_file(ddp_conn, &path1, DDP_O_READ, 0400, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost file  %s open failed. Err %d\n", full_path, err);
		ret = 0;
		goto cleanup;
	}

	buf = (char *) malloc(MAX_PATH_NAME + 1);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		ret = 0;
		goto cleanup;
	}

	/* The CREATE DATABASE statement is usually in the first 5 lines */
	memset(buf, 0, MAX_PATH_NAME + 1);
	ret_count = 0;

	err = ddp_stat(ddp_conn, &path1, &stat_buf);
	if (err)
	{
		ret = 0;
		mpp_err_msg(logError, progname, "DDboost Stat on file %s failed. Err %d\n", path1.path_name, err);
		goto cleanup;
	}

	if (stat_buf.st_size < MAX_PATH_NAME)
		rw_size = stat_buf.st_size;

	err = ddp_read(handle, buf, MAX_PATH_NAME,
				   0, &ret_count);

	if (err)
	{
		mpp_err_msg(logError, progname, "Reading file %s on ddboost failed. Err %d\n", path1.path_name, err);
		ret = 0;
		goto cleanup;
	}

	searchString = (char *) malloc(MAX_PATH_NAME);
	if (!searchString)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		ret = 0;
		goto cleanup;
	}
	memset(searchString, 0, MAX_PATH_NAME);
	snprintf(searchString, MAX_PATH_NAME, "CREATE DATABASE %s", databaseName);

	if (strstr(buf, searchString))
		ret = 1;

cleanup:
	if (buf)
		free(buf);
	if (full_path)
		free(full_path);
	if (searchString)
		free(searchString);

	return ret;
}

/* Copy over files from DDboost to GPDB master segment directory */
int
syncFilesFromDDBoost(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath)
{
	char	   *ddboostDir = DEFAULT_BACKUP_DIRECTORY;
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	char	   *dest_path = NULL;
	char	   *dest_dir = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostDir;

	/* Start traversing from default backup directory */
	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostDir);

	/*
	 * Create the user-configured Data Domain backup directory if it doesn't
	 * exist already
	 */
	dest_path = (char *) malloc(MAX_PATH_NAME);
	if (!dest_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(dest_path, 0, MAX_PATH_NAME);
	snprintf(dest_path, MAX_PATH_NAME, "%s/%s", dirPath, ddboostDir);
	err = mkdir(dest_path, S_IRWXU);
	if (err && (err != EEXIST) && (err != -1))
	{
		mpp_err_msg(logError, progname, "Directory %s creation on GPDB path failed .Err %d\n", dest_path, err);
		err = -1;
		goto cleanup;
	}

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "ddboost readdir failed on %s Err %d\n", ddboostDir, err);
				goto cleanup;
			}
		}
		else
		{
			if (strcmp("..", ret_dirent.d_name) == 0)
				continue;

			if (strcmp(".", ret_dirent.d_name) == 0)
				continue;

			/* Only check directories of the form YYYYMMDD */
			if (!isdigit(ret_dirent.d_name[0]))
				continue;
			dest_dir = (char *) malloc(MAX_PATH_NAME);
			if (dest_dir == NULL)
			{
				mpp_err_msg(logError, progname, "Memory allocation failed\n");
				err = -1;
				goto cleanup;
			}
			memset(dest_dir, 0, MAX_PATH_NAME);

			/*
			 * sync to user specified backup directory on master segment data
			 * directory
			 */
			snprintf(dest_dir, MAX_PATH_NAME, "%s/%s/%s", dirPath, ddboostDir, ret_dirent.d_name);

			/* For every date, copy the required files over to GPDB */
			memset(full_path, 0, MAX_PATH_NAME);
			snprintf(full_path, MAX_PATH_NAME, "%s/%s", ddboostDir, ret_dirent.d_name);

			copyFilesFromDir(full_path, dest_dir, ddp_conn, dd_options->ddboost_storage_unit);
		}
	}

cleanup:
	if (full_path)
		free(full_path);
	if (dest_path)
		free(dest_path);
	if (dest_dir)
		free(dest_dir);

	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostDir);
	}
	return err;
}


int
copyFilesFromDir(const char *fromDir, char *toDir, ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit)
{
	char	   *ddboostDir = Safe_strdup(fromDir);
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;
	char	   *dest_path = NULL;
	FILE	   *fp = NULL;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(full_path, 0, MAX_PATH_NAME);
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	dest_path = malloc(MAX_PATH_NAME);
	if (!dest_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(dest_path, 0, MAX_PATH_NAME);
	strncpy(dest_path, toDir, MAX_PATH_NAME);

	path1.su_name = ddboost_storage_unit;
	path1.path_name = ddboostDir;

	/* Start traversing from db_dumps/ directory */
	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostDir);

	/* Create the directory on the destination if it doesn't exist */
	err = mkdir(dest_path, S_IRWXU);
	if (err && (err != EEXIST) && (err != -1))
	{
		mpp_err_msg(logError, progname, "Directory %s creation on GPDB path failed with error %d\n", dest_path, err);
		err = -1;
		goto cleanup;
	}

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "ddboost read dir failed on %s with error %d\n", ddboostDir, err);
				goto cleanup;
			}
		}
		else
		{
			if (strcmp("..", ret_dirent.d_name) == 0)
				continue;

			if (strcmp(".", ret_dirent.d_name) == 0)
				continue;

			if (!isFileToBeCopied(ret_dirent.d_name))
			{
				continue;
			}

			/* For every date, copy the required files over to GPDB */
			memset(full_path, 0, MAX_PATH_NAME);
			snprintf(full_path, MAX_PATH_NAME, "%s/%s", ddboostDir, ret_dirent.d_name);

			memset(dest_path, 0, MAX_PATH_NAME);
			snprintf(dest_path, MAX_PATH_NAME, "%s/%s", toDir, ret_dirent.d_name);

			fp = fopen(dest_path, "w");
			if (!fp)
			{
				mpp_err_msg(logError, progname, "File %s open failed on GPDB\n", dest_path);
				err = -1;
				goto cleanup;
			}

			/* File full_path is closed in the callee */
			err = readFromDDFile(fp, full_path, ddboost_storage_unit);
			fclose(fp);
			fp = NULL;
		}
	}

cleanup:
	if (full_path)
		free(full_path);
	if (dest_path)
		free(dest_path);
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostDir);
	}
	if (ddboostDir)
		free(ddboostDir);
	return err;
}

int
isFileToBeCopied(const char *filename)
{
	if (strstr(filename, "gp_global_"))
		return 1;

	if (strstr(filename, "gp_cdatabase_"))
		return 1;

	if (strstr(filename, "_post_data"))
		return 1;

	if (strstr(filename, ".rpt"))
		return 1;

	if (strstr(filename, "_dirty_list"))
		return 1;

	if (strstr(filename, "_ao_state_file"))
		return 1;

	if (strstr(filename, "_co_state_file"))
		return 1;

	if (strstr(filename, "_last_operation"))
		return 1;

	if (strstr(filename, "_increments"))
		return 1;

	if (strstr(filename, "_table_list"))
		return 1;

	if (strstr(filename, ".tar"))
		return 1;

	if (strstr(filename, "gp_statistics_"))
		return 1;

	return 0;
}

/*
 * writeToDDFileFromInput
 * read data stream from standard input and write out to DD system
 */
static int
writeToDDFileFromInput(struct ddboost_options *dd_options)
{
	char	   *ddBoostFileName = dd_options->to_file;
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	int			err = 0;
	ddp_uint64_t ret_count = 0;
	char	   *buf = NULL;
	ddp_uint64_t rw_size = dd_boost_buf_size;
	ddp_uint64_t total_bytes = 0;
	ddp_int64_t read_bytes = 0;
	ddp_path_t	path1 = {0};
	char	   *full_path = NULL;
	char	   *buf_iogroup = NULL;
	int			buf_data_length = 0;

	if (!ddBoostFileName)
	{
		mpp_err_msg(logError, progname, "Destination filename not specified\n");
		return -1;
	}

	if (ddBoostFileName[strlen(ddBoostFileName) - 1] == '/')
	{
		mpp_err_msg(logError, progname, "Invalid filename specified. Filename cannot end with /\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (full_path == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddBoostFileName);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = full_path;

	err = createDDboostDir(ddp_conn, dd_options->ddboost_storage_unit, ddBoostFileName);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost Creating path %s failed. Err %d\n", ddBoostFileName, err);
		err = -1;
		goto cleanup;
	}

	err = ddp_open_file(ddp_conn, &path1, DDP_O_CREAT | DDP_O_RDWR, 0600, &handle);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost File %s open failed. Err %d\n", path1.path_name, err);
		err = -1;
		goto cleanup;
	}

	buf = (char *) malloc(rw_size);
	if (buf == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}

	buf_iogroup = (char *) malloc(rw_size);
	if (buf_iogroup == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(buf_iogroup, 0, rw_size);

	do
	{
		memset(buf, 0, rw_size);
		ret_count = 0;

		read_bytes = read(0, buf, rw_size);
		if (read_bytes > 0)
		{
			if (read_bytes == rw_size)	/* no I/O grouping is needed */
			{
				/* flushing existing data in I/O grouping buffer first */
				if (buf_data_length > 0)
				{
					err = ddp_write(handle, buf_iogroup, buf_data_length, total_bytes, &ret_count);
					if (err)
					{
						mpp_err_msg(logError, progname, "ddboost File %s write failed. Err %d\n", path1.path_name, err);
						break;
					}
					total_bytes += ret_count;
					memset(buf_iogroup, 0, rw_size);
					buf_data_length = 0;
				}


				err = ddp_write(handle, buf, read_bytes,
								total_bytes, &ret_count);
				if (err)
				{
					mpp_err_msg(logError, progname, "ddboost File %s write failed. Err %d\n", path1.path_name, err);
					err = -1;
					goto cleanup;
				}
				total_bytes += ret_count;
			}
			else	/* grouping small I/O */
			{
				if ((read_bytes + buf_data_length) <= rw_size)
				{
					memcpy(buf_iogroup + buf_data_length, buf, read_bytes);
					buf_data_length += read_bytes;
				}
				else
				{
					err = ddp_write(handle, buf_iogroup, buf_data_length,
									total_bytes, &ret_count);
					if (err)
					{
						mpp_err_msg(logError, progname, "ddboost File %s write failed. Err %d\n", path1.path_name, err);
						err = -1;
						goto cleanup;
					}
					total_bytes += ret_count;

					memset(buf_iogroup, 0, rw_size);
					memcpy(buf_iogroup, buf, read_bytes);
					buf_data_length = read_bytes;
				}
			}
		}
	} while ((err == 0) && (read_bytes > 0));

	/* skip writing remaining data if we already met I/O error */
	if ((err == 0) && (buf_data_length > 0))
		err = ddp_write(handle, buf_iogroup, buf_data_length, total_bytes, &ret_count);

	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost File %s write failed Err %d\n", path1.path_name, err);
		err = -1;
	}

cleanup:
	if (full_path)
		free(full_path);
	if (buf)
		free(buf);
	if (buf_iogroup)
		free(buf_iogroup);

	if (handle != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(handle);
	return err;
}

int
isFileCompressed(const char *filename)
{
	if (!filename)
		return 0;

	if (strstr(filename, ".gz"))
	{
		return 1;
	}

	return 0;
}

int
createDbdumpsDir(char *filePath)
{
	char	   *pch = NULL;
	char	   *filePathCopy = NULL;
	int			found = 0;
	char		curPath[MAX_PATH_NAME];
	int			err = 0;
	int			returnValue = 0;

	memset(curPath, 0, MAX_PATH_NAME);
	if (filePath[0] == '/')
		curPath[0] = '/';

	/* Retain a copy of the original pathname as strtok strips off the string */
	filePathCopy = Safe_strdup(filePath);

	pch = strtok(filePathCopy, " /");
	while (pch != NULL)
	{
		/* Create only the db_dumps and the next directory in the hierarchy */
		if (!found && (strcmp(pch, "db_dumps") == 0))
			found = 1;

		strcat(curPath, pch);

		/* Do not create the file here */
		if (strncmp(curPath, filePath, strlen(filePath)))
		{
			if (found)
			{
				err = mkdir(curPath, S_IRWXU);
				if (err && (err != EEXIST) && (err != -1))
				{
					mpp_err_msg(logError, progname, "Directory %s creation on GPDB path failed with error %d\n", curPath, err);
					returnValue = -1;
					goto cleanup;
				}

				/*
				 * The directory structure is <some_path>/db_dumps/<date>/
				 * When we finish creating the dated directory, we need to
				 * stop
				 */
				if (isdigit(pch[0]))
					goto cleanup;
			}

			strcat(curPath, "/");
			pch = strtok(NULL, " /");
		}
		else
			break;
	}
cleanup:
	if (filePathCopy)
		free(filePathCopy);

	return returnValue;
}

int
listDirectoryFull(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	char	   *ddboostDir = strdup(dd_options->listDirFull);
	int			err = 0;
	ddp_path_t	path1 = {0};
	ddp_path	dd_file_path = {0};
	char	   *full_path = NULL;
	char	   *temp_path = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;
	ddp_stat_t	stat_buf;
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;

	if (!ddboostDir)
	{
		mpp_err_msg(logError, progname, "Directory on DDboost is not specified\n");
		return -1;
	}

	full_path = (char *) malloc(MAX_PATH_NAME);
	if (!full_path)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path, MAX_PATH_NAME, "%s", ddboostDir);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostDir;

	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostDir);

	temp_path = (char *) malloc(MAX_PATH_NAME);
	if (!temp_path)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostDir, err);
		err = -1;
		goto cleanup;
	}

	printf("\n%20s%23s\t\t\t\t%s\t%s", "NAME", "", "MODE", "SIZE(bytes)");
	printf("\n===============================================================================================");
	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "Reading directory %s on ddboost failed. Err %d\n", ddboostDir, err);
				break;
			}
		}

		else
		{
			/* Skip deleting . and .. */
			if (strcmp("..", ret_dirent.d_name) == 0)
			{
				continue;
			}
			if (strcmp(".", ret_dirent.d_name) == 0)
			{
				continue;
			}

			memset(temp_path, 0, MAX_PATH_NAME);
			snprintf(temp_path, MAX_PATH_NAME, "%s/%s", ddboostDir, ret_dirent.d_name);

			dd_file_path.su_name = dd_options->ddboost_storage_unit;
			dd_file_path.path_name = temp_path;

			err = ddp_open_file(ddp_conn, &dd_file_path, DDP_O_READ, 0, &handle);
			if (err)
			{
				mpp_err_msg(logError, progname, "Reading file %s on ddboost failed. Err %d\n", temp_path, err);
				break;
			}

			err = ddp_fstat(handle, &stat_buf);
			if (err)
			{
				mpp_err_msg(logError, progname, "Stat on file %s on ddboost failed. Err %d\n", temp_path, err);
				ddp_close_file(handle);
				break;
			}

			if (S_ISDIR(stat_buf.st_mode))
				printf("\n%-50s\t\t\tD%o\t%llu", ret_dirent.d_name, (stat_buf.st_mode & 07777), stat_buf.st_size);
			else
				printf("\n%-50s\t\t\t%o\t%llu", ret_dirent.d_name, (stat_buf.st_mode & 07777), stat_buf.st_size);

			ddp_close_file(handle);
		}
	}
	printf("\n");

cleanup:
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostDir);
	}
	if (temp_path)
		free(temp_path);
	if (full_path)
		free(full_path);
	if (ddboostDir)
		free(ddboostDir);
	return err;
}

/* Copy over files from DDboost to GPDB master segment directory */
int
syncFilesFromDDBoostTimestamp(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, char *dirPath)
{
	char	   *ddboostDir = DEFAULT_BACKUP_DIRECTORY;
	int			err = 0;
	ddp_path_t	path1 = {0};
	char	   *ddboostPath = NULL;
	char	   *gpdbPath = NULL;
	ddp_dir_desc_t dird = DDP_INVALID_DESCRIPTOR;
	ddp_dirent_t ret_dirent;
	char		tempDate[10];
	FILE	   *fp = NULL;

	if (!dd_options->syncFilesFromTimestamp)
	{
		mpp_err_msg(logError, progname, "Timestamp not specified for sync\n");
		err = -1;
		goto cleanup;
	}

	/*
	 * Form Directory path from the timestamp. The first 8 characters of the
	 * timestamp is of the form YYYYMMDD and we need to go to the directory
	 * db_dumps/YYYYMMDD. Then we search for the file whcih has the
	 * <timestamp> in its name.
	 */
	memset(tempDate, 0, 10);
	snprintf(tempDate, 9, "%s", dd_options->syncFilesFromTimestamp);

	ddboostPath = (char *) malloc(MAX_PATH_NAME);
	if (ddboostPath == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(ddboostPath, MAX_PATH_NAME, "%s/%s", ddboostDir, tempDate);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = ddboostPath;

	/* Start traversing the db_dumps/<date> directory */
	err = ddp_open_dir(ddp_conn, &path1, &dird);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening directory %s on ddboost failed. Err %d\n", ddboostPath, err);
		err = -1;
		goto cleanup;
	}
	mpp_err_msg("DEBUG", progname, "ddp_open_dir(%s)\n", ddboostPath);

	/* Create the db_dumps directory if it doesn't exist already */
	gpdbPath = (char *) malloc(MAX_PATH_NAME);
	if (gpdbPath == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	memset(gpdbPath, 0, MAX_PATH_NAME);
	snprintf(gpdbPath, MAX_PATH_NAME, "%s/%s", dirPath, ddboostDir);

	/* Create the db_dumps directory on the destination if it doesn't exist */
	err = mkdir(gpdbPath, S_IRWXU);
	if (err && (err != EEXIST) && (err != -1))
	{
		mpp_err_msg(logError, progname, "Directory %s creation on GPDB path failed .Err %d\n", gpdbPath, err);
		err = -1;
		goto cleanup;
	}

	/* Create the destination directory on the destination if it doesn't exist */
	memset(gpdbPath, 0, MAX_PATH_NAME);
	snprintf(gpdbPath, MAX_PATH_NAME, "%s/%s/%s", dirPath, "db_dumps", tempDate);
	err = mkdir(gpdbPath, S_IRWXU);
	if (err && (err != EEXIST) && (err != -1))
	{
		mpp_err_msg(logError, progname, "Directory %s creation on GPDB path failed .Err %d\n", gpdbPath, err);
		err = -1;
		goto cleanup;
	}

	while (1)
	{
		memset(&ret_dirent, 0, sizeof(ddp_dirent_t));
		err = ddp_readdir(dird, &ret_dirent);
		if (err != DD_OK)
		{
			if (err == DD_ERR_EMPTY)
			{
				err = DD_OK;
				break;
			}
			else
			{
				mpp_err_msg(logError, progname, "ddboost readdir failed on %s Err %d\n", ddboostDir, err);
				goto cleanup;
			}
		}
		else
		{
			/* Only check directories of the form YYYYMMDD */
			if (!strstr(ret_dirent.d_name, dd_options->syncFilesFromTimestamp))
				continue;

			/* For every matching file , copy the required files over to GPDB */
			memset(ddboostPath, 0, MAX_PATH_NAME);
			snprintf(ddboostPath, MAX_PATH_NAME, "%s/%s/%s", ddboostDir, tempDate, ret_dirent.d_name);

			memset(gpdbPath, 0, MAX_PATH_NAME);
			snprintf(gpdbPath, MAX_PATH_NAME, "%s/%s/%s/%s", dirPath, ddboostDir, tempDate, ret_dirent.d_name);
			fp = fopen(gpdbPath, "w");
			if (fp == NULL)
			{
				mpp_err_msg(logError, progname, "Cannot open file %s on GPDB\n", gpdbPath);
				err = -1;
				break;
			}

			/* Close both files in the called function */
			err = readFromDDFile(fp, ddboostPath, dd_options->ddboost_storage_unit);
			fclose(fp);
			fp = NULL;
			if (err)
			{
				mpp_err_msg(logError, progname, "Copy failed from DDboost file %s to GPDB file %s\n", ddboostPath, gpdbPath);
				break;
			}

		}
	}

cleanup:
	if (fp)
		free(fp);
	if (gpdbPath)
		free(gpdbPath);
	if (dird != DDP_INVALID_DESCRIPTOR)
	{
		ddp_close_dir(dird);
		mpp_err_msg("DEBUG", progname, "ddp_close_dir(%s)\n", ddboostPath);
	}
	if (ddboostPath)
		free(ddboostPath);
	return err;
}

static int
setCredential(struct ddboost_options *dd_options)
{
	/* take password - getpass works only on linux */
	if (!dd_options->password)
	{
		dd_options->password = getpass("Password: ");
	}

	if (setDDBoostCredential(dd_options->hostname, dd_options->user, dd_options->password,
							 dd_options->log_level, dd_options->log_size,
	  dd_options->default_backup_directory, dd_options->ddboost_storage_unit,
							 dd_options->remote) < 0)
		return -1;

	return 0;
}

int
renameFile(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn)
{
	char	   *fromFile = Safe_strdup(dd_options->from_file);
	char	   *toFile;
	int			err = 0;
	ddp_path_t	path1 = {0};
	ddp_path_t	path2 = {0};
	char	   *full_path_source = NULL;
	char	   *full_path_dest = NULL;

	if (!fromFile)
	{
		mpp_err_msg(logError, progname, "Source file on DDboost not specified\n");
		return -1;
	}

	toFile = Safe_strdup(dd_options->to_file);
	if (!toFile)
	{
		mpp_err_msg(logError, progname, "Destination file on GPDB not specified\n");
		err = -1;
		goto cleanup;
	}

	full_path_source = (char *) malloc(MAX_PATH_NAME);
	if (full_path_source == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path_source, MAX_PATH_NAME, "%s", fromFile);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = full_path_source;

	full_path_dest = (char *) malloc(MAX_PATH_NAME);
	if (full_path_dest == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path_dest, MAX_PATH_NAME, "%s", toFile);
	path2.su_name = dd_options->ddboost_storage_unit;
	path2.path_name = full_path_dest;

	/* Create a directory on the destination if it doesn't exist */
	err = createDDboostDir(ddp_conn, dd_options->ddboost_storage_unit, toFile);
	if (err)
	{
		mpp_err_msg(logError, progname, "Creating path %s on ddboost failed. Err %d\n", fromFile, err);
		err = -1;
		goto cleanup;
	}

	/* Now try to rename the original file to the new name */
	err = ddp_rename(ddp_conn, &path1, &path2);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost File %s rename to %s failed. Err %d\n", path1.path_name, path2.path_name, err);
		err = -1;
		goto cleanup;
	}

cleanup:
	if (full_path_source)
		free(full_path_source);
	if (full_path_dest)
		free(full_path_dest);
	if (fromFile)
		free(fromFile);
	if (toFile)
		free(toFile);

	return 0;
}

int
copyWithinDDboost(struct ddboost_options *dd_options, ddp_conn_desc_t ddp_conn, int direction)
{
	char	   *fromFile = Safe_strdup(dd_options->from_file);
	char	   *toFile = Safe_strdup(dd_options->to_file);
	int			err = 0;
	ddp_path_t	path1 = {0};
	ddp_path_t	path2 = {0};
	char	   *full_path_source = NULL;
	char	   *full_path_dest = NULL;
	ddp_file_desc_t src_fh = DDP_INVALID_DESCRIPTOR;
	ddp_file_desc_t dst_fh = DDP_INVALID_DESCRIPTOR;
	ddp_uint64_t done_offset = 0;
	ddp_uint64_t bytescopied = 0;
	ddp_stat_t	stat_buf;
	ddp_conn_desc_t remote_ddp_conn = DDP_INVALID_DESCRIPTOR;
	ddp_conn_desc_t source_ddp_conn = DDP_INVALID_DESCRIPTOR;
	ddp_conn_desc_t target_ddp_conn = DDP_INVALID_DESCRIPTOR;
	char	   *remote_default_backup_directory = NULL;

	pqsignal(SIGINT, Replication_CancellationHandler);

	if (direction != COPY_WITHIN_SAME_DDBOOST)
	{
		int			ret = initDDSystem(&ddp_inst, &remote_ddp_conn, &dd_client_info, &dd_options->ddboost_storage_unit, false, &remote_default_backup_directory, true /* remote */ );

		if (remote_default_backup_directory)
			free(remote_default_backup_directory);

		if (ret)
		{
			mpp_err_msg(logError, progname, "Initialization of ddboost failed. Check login credentials for remote Data Domain system\n");
		}

		if (direction == REPLICATE_LOCAL_TO_REMOTE)
		{
			source_ddp_conn = ddp_conn;
			target_ddp_conn = remote_ddp_conn;
		}
		else if (direction == REPLICATE_REMOTE_TO_LOCAL)
		{
			source_ddp_conn = remote_ddp_conn;
			target_ddp_conn = ddp_conn;
		}
	}
	else
	{
		/* copy within same DD Boost */
		source_ddp_conn = target_ddp_conn = ddp_conn;
	}

	if (!fromFile)
	{
		mpp_err_msg(logError, progname, "Source file on DDboost not specified\n");
		err = -1;
		goto cleanup;
	}

	if (!toFile)
	{
		mpp_err_msg(logError, progname, "Destination file on GPDB not specified\n");
		err = -1;
		goto cleanup;
	}

	full_path_source = (char *) malloc(MAX_PATH_NAME);
	if (full_path_source == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path_source, MAX_PATH_NAME, "%s", fromFile);

	path1.su_name = dd_options->ddboost_storage_unit;
	path1.path_name = full_path_source;

	full_path_dest = (char *) malloc(MAX_PATH_NAME);
	if (full_path_dest == NULL)
	{
		mpp_err_msg(logError, progname, "Memory allocation failed\n");
		err = -1;
		goto cleanup;
	}
	snprintf(full_path_dest, MAX_PATH_NAME, "%s", toFile);
	path2.su_name = dd_options->ddboost_storage_unit;
	path2.path_name = full_path_dest;

	/* Create a directory on the destination if it doesn't exist */
	err = createDDboostDir(target_ddp_conn, dd_options->ddboost_storage_unit, toFile);
	if (err)
	{
		mpp_err_msg(logError, progname, "Creating path %s on ddboost failed. Err %d\n", fromFile, err);
		err = -1;
		goto cleanup;
	}

	/* Open the file on source before we start the copy */
	err = ddp_open_file(source_ddp_conn, &path1, DDP_O_READ, 0400, &src_fh);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening file %s on ddboost failed. Err %d\n", fromFile, err);
		err = -1;
		goto cleanup;
	}

	/* Open the file on destination before we start the copy */
	err = ddp_open_file(target_ddp_conn, &path2, DDP_O_CREAT | DDP_O_RDWR, 0644, &dst_fh);
	if (err)
	{
		mpp_err_msg(logError, progname, "Opening file %s on ddboost failed. Err %d\n", toFile, err);
		err = -1;
		goto cleanup;
	}

	/* Get the size of the source file to be copied */
	err = ddp_stat(source_ddp_conn, &path1, &stat_buf);
	if (err)
	{
		mpp_err_msg(logError, progname, "Stat on ddboost file %s failed. error %d\n", fromFile, err);
		err = -1;
		goto cleanup;
	}

	/*
	 * Now start the copy Before we start the filecopy, try to stop it * It
	 * should not be found DDFS should exit with no error *
	 */
	ddp_filecopy_stop(source_ddp_conn, dst_fh);

	/* Start the file copy */
	err = ddp_filecopy_start(src_fh, dst_fh, NULL);
	if (err)
	{
		mpp_err_msg(logError, progname, "File copy failed to start on DDboost. Src file %s Dest file %s. Err %d\n",
					fromFile, toFile, err);
		err = -1;
		goto cleanup;
	}

	done_offset = 0;
	double		previous_percentage = 0.0;
	double		current_percentage = 0.0;

	while (done_offset != stat_buf.st_size)
	{
		/* handle the cancellation request */
		if (ReplicationCancellationRequested)
		{
			ddp_filecopy_stop(source_ddp_conn, dst_fh);
			mpp_err_msg(logError, progname, "File copy on ddboost canceled. Src file %s Dest file %s.\n",
						fromFile, toFile);
			err = -1;			/* trigger delete of partially copied file */
			goto cleanup;
		}


		bytescopied = 0;
		err = ddp_filecopy_status(src_fh, dst_fh,
								  done_offset,
								  DDP_FILECOPY_EXTENT_LEN,
								  &bytescopied);
		if (err != DD_OK)
		{
			ddp_filecopy_stop(source_ddp_conn, dst_fh);
			mpp_err_msg(logError, progname, "File copy on ddboost failed. Src file %s Dest file %s. Err %d\n",
						fromFile, toFile, err);
			err = -1;
			goto cleanup;
		}
		done_offset += bytescopied;
		current_percentage = (double) done_offset / (double) stat_buf.st_size;
		if ((current_percentage - previous_percentage) > 0.01)
		{
			previous_percentage = current_percentage;
			printf("\nReplication of file %s completed %f percent, %lld bytes has been replicated out of %lld bytes\n", full_path_dest, current_percentage * 100.0, done_offset, stat_buf.st_size);

			/*
			 * flush all opened output streams so that gpmfr could read the
			 * replication status message
			 */
			fflush(NULL);
		}
	}

	/* Check if all the data has been copied */
	if (done_offset != stat_buf.st_size)
	{
		ddp_filecopy_stop(source_ddp_conn, dst_fh);
		mpp_err_msg(logError, progname, "File copy on ddboost failed to complete. Src file %s Dest file %s"
					"Total size %lld completed size %lld. Err %d\n",
					fromFile, toFile, stat_buf.st_size, done_offset, err);
		err = -1;
		goto cleanup;
	}
	else
	{
		printf("\nReplication of file %s completed %f percent, %lld bytes has been replicated out of %lld bytes\n", full_path_dest, 1 * 100.0, done_offset, stat_buf.st_size);
	}

	ddp_filecopy_stop(source_ddp_conn, dst_fh);

cleanup:
	/* If err then delete the partially copied file */
	if (err)
	{
		ddp_unlink(target_ddp_conn, &path2);
	}

	if (full_path_source)
		free(full_path_source);
	if (full_path_dest)
		free(full_path_dest);
	if (fromFile)
		free(fromFile);
	if (toFile)
		free(toFile);

	if (src_fh != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(src_fh);
	if (dst_fh != DDP_INVALID_DESCRIPTOR)
		ddp_close_file(dst_fh);

	if (remote_ddp_conn != DDP_INVALID_DESCRIPTOR)
	{
		err = ddp_disconnect(remote_ddp_conn);
		if (err)
			mpp_err_msg(logError, progname, "ddp_disconnect failed with Err = %d\n", err);
	}

	return err;
}

static int
get_stream_counts(ddp_conn_desc_t connd, ddp_stream_counts_t * stream_counts)
{
	int			err = DD_ERR_NONE;

	err = ddp_get_stream_counts(connd, stream_counts);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddp_get_stream_counts() failed with Err = %d\n", err);
		return err;
	}
	printf("\n-----Stream Count Limits----\n");
	printf("Backup Write Streams                                 : %d\n", stream_counts->limits.backup_write_streams);
	printf("Backup Read Streams                                  : %d\n", stream_counts->limits.backup_read_streams);
	printf("Replication Source Streams                           : %d\n", stream_counts->limits.repl_source_streams);
	printf("Replication Destination Streams                      : %d\n", stream_counts->limits.repl_dest_streams);
	printf("Nearline Read Streams                                : %d\n", stream_counts->limits.nearline_read_streams);
	printf("Nearline Write Streams                               : %d\n", stream_counts->limits.nearline_write_streams);
	printf("Write-Stream Hard Limit                              : %d\n", stream_counts->limits.total_write_streams_hard_limit);
	printf("Total-Stream Hard Limit                              : %d\n", stream_counts->limits.total_streams_hard_limit);
	printf("Mixed Mode Write Streams                             : %d\n", stream_counts->limits.mixed_write_streams);
	printf("Mixed Mode Read Streams:                             : %d\n", stream_counts->limits.mixed_read_streams);
	printf("Mixed Mode Replication Source Streams                : %d\n", stream_counts->limits.mixed_repl_src_streams);
	printf("Mixed Mode Replication Destination Streams           : %d\n", stream_counts->limits.mixed_repl_dst_streams);
	printf("Mixed Mode Replication Destination with Write Streams: %d\n", stream_counts->limits.mixed_repl_dst_with_write_streams);
	printf("Mixed Mode total streams                             : %d\n", stream_counts->limits.mixed_total_streams);
	printf("\n");
	printf("\n-----Stream Counts In Use-----\n");
	printf("Used Read Streams       : %d\n", stream_counts->inuse.read_streams);
	printf("Used Read/Write Streams : %d\n", stream_counts->inuse.read_write_streams);
	printf("Used Replication Streams: %d\n", stream_counts->inuse.replication_streams);
	printf("Used Filecopy Streams   : %d\n", stream_counts->inuse.filecopy_streams);

	return 0;
}

static void
Replication_CancellationHandler(SIGNAL_ARGS)
{
	ReplicationCancellationRequested = true;
}

#else
int
main(int argc, char **argv)
{
	/* TODO: change printf() to mpp_err_msg() */
	printf("\ngpddboost is not supported on this platform\n\n");
	return 0;
}
#endif
