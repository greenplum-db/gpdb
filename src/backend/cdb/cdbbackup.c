/*-------------------------------------------------------------------------
 *
 * cdbbackup.c
 *
 *
 *
 */
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>

#include "postgres.h"

#include "regex/regex.h"
#include "libpq/libpq-be.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbbackup.h"
#include "cdb/cdbtimer.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "utils/guc.h"

#define DUMP_PREFIX (dump_prefix==NULL?"":dump_prefix)

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define EXIT_CODE_BACKUP_RESTORE_ERROR 127

/* static helper functions */
static bool createBackupDirectory(char *pszPathName);
static char *findAcceptableBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, int instid, int segid);
static char *formBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool is_compress, bool isPostData, const char *suffix);
static char *formStatusFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool bIsBackup);
static char *formThrottleCmd(char *pszBackupFileName, int directIO_read_chunk_mb, bool bIsThrottlingEnabled);
static char *relativeToAbsolute(char *pszRelativeDirectory);
static void testCmdsExist(char *pszCompressionProgram, char *pszBackupProgram);
static char *testProgramExists(char *pszProgramName);
static void validateBackupDirectory(char *pszBackupDirectory);
static char *positionToError(char *source);
static char *parse_prefix_from_params(char *params);
static char *parse_status_from_params(char *params);
static char *parse_option_from_params(char *params, char *option);
static char *queryNBUBackupFilePathName(char *netbackupServiceHost, char *netbackupRestoreFileName);
static char *shellEscape(const char *shellArg, bool addQuote);


#ifdef USE_DDBOOST

static char *formDDBoostFileName(char *pszBackupKey, bool isPostData, bool isCompress);
static int	dd_boost_enabled = 0;

#ifndef MAX_PATH_NAME
#define MAX_PATH_NAME 1024
#endif

static char *gpDDBoostPg = NULL;

#endif

static char *compPg = NULL,
		   *bkPg = NULL,
		   *dump_prefix = NULL,
		   *statusDirectory = NULL;

/* NetBackup related variables */
static char *netbackup_service_host = NULL,
		   *netbackup_policy = NULL,
		   *netbackup_schedule = NULL,
		   *netbackup_block_size = NULL,
		   *netbackup_keyword = NULL;
static int	netbackup_enabled = 0;
static char *gpNBUDumpPg = NULL;
static char *gpNBUQueryPg = NULL;
static bool is_old_format = 0;

typedef enum backup_file_type
{
	BFT_BACKUP = 0,
	BFT_BACKUP_STATUS = 1,
	BFT_RESTORE_STATUS = 2
} BackupFileType;

static char *
parse_prefix_from_params(char *params)
{
	if (params == NULL)
		return NULL;
	char	   *temp = pstrdup(params);
	char	   *pch = strstr(temp, "--prefix");

	if (pch)
	{
		pch = pch + strlen("--prefix");
		char	   *ptr = strtok(pch, " ");

		if (ptr == NULL)
			return NULL;
		return ptr;
	}
	return NULL;
}

static char *
parse_status_from_params(char *params)
{
	if (params == NULL)
		return NULL;
	char	   *temp = pstrdup(params);
	char	   *pch = strstr(temp, "--status");

	if (pch)
	{
		pch = pch + strlen("--status");
		char	   *ptr = strtok(pch, " ");

		if (ptr == NULL)
			return NULL;
		return ptr;
	}
	return NULL;
}

static char *
parse_option_from_params(char *params, char *option)
{
	if ((params == NULL) || (option == NULL))
		return NULL;
	char	   *temp = pstrdup(params);
	char	   *pch = strstr(temp, option);

	if (pch)
	{
		pch = pch + strlen(option);
		char	   *ptr = strtok(pch, " ");

		if (ptr == NULL)
		{
			pfree(temp);
			return NULL;
		}
		return ptr;
	}

	pfree(temp);
	return NULL;
}


/*
 * gp_backup_launch__( TEXT, TEXT, TEXT, TEXT, TEXT ) returns TEXT
 *
 * Called by gp_dump.c to launch gp_dump_agent on this host.
 */
PG_FUNCTION_INFO_V1(gp_backup_launch__);
Datum
gp_backup_launch__(PG_FUNCTION_ARGS)
{
	/* Fetch the parameters from the argument list. */
	char	   *pszBackupDirectory = "./";
	char	   *pszBackupKey = "";
	char	   *pszCompressionProgram = "";
	char	   *pszPassThroughParameters = "";
	char	   *pszPassThroughCredentials = "";
	char	   *pszDataOption = "";
	char	   *pszBackupFileName,
			   *pszStatusFileName;
	char	   *pszSaveBackupfileName;
	char	   *pszDBName,
			   *pszUserName;
	pid_t		newpid;
	char	   *pszCmdLine;
	int			port;
	char	   *pszKeyParm;		/* dispatch node */
	int			segid;
	int			instid;			/* dispatch node */
	bool		is_compress;
	itimers		savetimers;

	char	   *pszThrottleCmd;
	StringInfoData cmdBuf;
	char	   *temp;

#ifdef USE_DDBOOST
	char	   *pszDDBoostFileName = NULL;
	char	   *pszDDBoostDirName = "db_dumps"; /* Default directory */
	char	   *pszDDBoostStorageUnitName = NULL;
	char	   *gpDDBoostCmdLine = NULL;
	char	   *pch = NULL,
			   *pchs = NULL,
			   *pStu = NULL,
			   *pStus = NULL;
#endif

	/* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = TextDatumGetCString(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = TextDatumGetCString(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszCompressionProgram = TextDatumGetCString(PG_GETARG_TEXT_P(2));

	if (!PG_ARGISNULL(3))
		pszPassThroughParameters = TextDatumGetCString(PG_GETARG_TEXT_P(3));

	if (!PG_ARGISNULL(4))
		pszPassThroughCredentials = TextDatumGetCString(PG_GETARG_TEXT_P(4));

	/*
	 * dump_prefix must be allocated in TopMemoryContext, because it is looked
	 * at by later gp_read_backup_file() calls in the same backend.
	 */
	if (dump_prefix)
		pfree(dump_prefix);
	temp = parse_prefix_from_params(pszPassThroughParameters);
	if (temp)
		dump_prefix = MemoryContextStrdup(TopMemoryContext, temp);

	netbackup_service_host = parse_option_from_params(pszPassThroughParameters, "--netbackup-service-host");
	netbackup_policy = parse_option_from_params(pszPassThroughParameters, "--netbackup-policy");
	netbackup_schedule = parse_option_from_params(pszPassThroughParameters, "--netbackup-schedule");
	netbackup_block_size = parse_option_from_params(pszPassThroughParameters, "--netbackup-block-size");
	netbackup_keyword = parse_option_from_params(pszPassThroughParameters, "--netbackup-keyword");

	if (strstr(pszPassThroughParameters, "--old-format") != NULL)
		is_old_format = 1;

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);

	/*
	 * See whether the directory exists. If not, try to create it. If so, make
	 * sure it's a directory
	 */
	validateBackupDirectory(pszBackupDirectory);

	/*
	 * Validate existence of gp_dump_agent and compression program, if
	 * possible
	 */
	testCmdsExist(pszCompressionProgram, "gp_dump_agent");

	/* are we going to use a compression program? */
	is_compress = (pszCompressionProgram != NULL && *pszCompressionProgram != '\0' ? true : false);

	/* Form backup file path name */
	pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_compress, false, NULL);
	pszStatusFileName = formStatusFilePathName(pszBackupDirectory, pszBackupKey, true);

	int			pszBackupFileLen = strnlen(pszBackupFileName, PATH_MAX);

	pszSaveBackupfileName = (char *) palloc(sizeof(char) * (1 + pszBackupFileLen));
	strncpy(pszSaveBackupfileName, pszBackupFileName, pszBackupFileLen);
	pszSaveBackupfileName[pszBackupFileLen] = '\0';

	/* Initialise the DDBoost connection and use it to open the file */
#ifdef USE_DDBOOST
	if (strstr(pszPassThroughParameters, "--dd_boost_enabled") != NULL)
		dd_boost_enabled = 1;
	else
		dd_boost_enabled = 0;

	if (dd_boost_enabled)
	{
		pszDDBoostFileName = formDDBoostFileName(pszBackupKey, false, is_compress);

		gpDDBoostPg = testProgramExists("gpddboost");
		if (gpDDBoostPg == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("%s not found in PGPATH or PATH (PGPATH: %s | PATH:  %s)",
							"gpddboost", getenv("PGPATH"), getenv("PATH")),
					 errhint("Restart the server and try again")));
		}
	}
#endif

	if (strstr(pszPassThroughParameters, "--netbackup-service-host") != NULL)
	{
		netbackup_enabled = 1;
	}

	if (netbackup_enabled)
	{
		gpNBUDumpPg = testProgramExists("gp_bsa_dump_agent");
		if (gpNBUDumpPg == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("%s not found in PGPATH or PATH (PGPATH: %s | PATH:  %s)",
							"gp_bsa_dump_agent", getenv("PGPATH"), getenv("PATH")),
					 errhint("Restart the server and try again")));
		}
	}

	pszDBName = NULL;
	pszUserName = (char *) NULL;
	if (MyProcPort != NULL)
	{
		pszDBName = MyProcPort->database_name;
		pszUserName = MyProcPort->user_name;
	}

	if (pszDBName == NULL)
		pszDBName = "";
	else
	{
		pszDBName = shellEscape(pszDBName, true);
	}

	if (pszUserName == NULL)
		pszUserName = "";


	if (strstr(pszPassThroughParameters, "--rsyncable") != NULL)
	{
		elog(DEBUG1, "--rsyncable found, ptp %s", pszPassThroughParameters);

		/*
		 * Remove from gp_dump_agent parameters, because this parameter is for
		 * gzip
		 */
		strncpy(strstr(pszPassThroughParameters, "--rsyncable"), "            ", strlen("--rsyncable"));
		elog(DEBUG1, "modified to ptp %s", pszPassThroughParameters);

		/*
		 * If a compression program is set, and doesn't already include the
		 * parameter
		 */
		if (strlen(pszCompressionProgram) > 1 && strstr(pszCompressionProgram, "--rsyncable") == NULL)
		{
			/* add the parameter */
			/*
			 * You would think we'd add this to pszCompressionProgram, but
			 * actually testCommandExists() moved it to compPg, where we use
			 * it later.
			 *
			 * But, much of the code assumes that if pszCompressionProgram is
			 * set, we WILL have a compression program, and the code will fail
			 * if we don't, so why we do this is a mystery.
			 */
			compPg = psprintf("%s --rsyncable", compPg);

			elog(DEBUG1, "compPG %s", compPg);
		}
	}


	/*
	 * If a compression program is set, and doesn't already include the
	 * parameter
	 */
	if (strlen(pszCompressionProgram) > 1 && strstr(pszCompressionProgram, " -1") == NULL &&
		pszCompressionProgram[0] == 'g')
	{
		/* add the -1 parameter to gzip */
		/*
		 * You would think we'd add this to pszCompressionProgram, but
		 * actually testCommandExists() moved it to compPg, where we use it
		 * later.
		 *
		 * But, much of the code assumes that if pszCompressionProgram is set,
		 * we WILL have a compression program, and the code will fail if we
		 * don't, so why we do this is a mystery.
		 */
		compPg = psprintf("%s -1", compPg);
		elog(DEBUG1, "new compPG %s", compPg);
	}


	/* Clear process interval timers */
	resetTimers(&savetimers);

	/* Child Process */
	port = PostPortNumber;

	if (gp_backup_directIO)
	{
		if (testProgramExists("throttlingD.py") == NULL)
		{
			/* throttlingD.py does not exist */
			const char *gp_home = getenv("GPHOME");

			if (!gp_home)
				gp_home = "";

			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("throttlingD.py not found in GPHOME\\bin (GPHOME: %s)", gp_home)));
		}

		pszThrottleCmd = formThrottleCmd(pszBackupFileName, gp_backup_directIO_read_chunk_mb, true);
	}
	else
	{
		/*
		 * when directIO is disabled --> need to pipe the dump to backupFile
		 * (No throttling will be done)
		 */
		pszThrottleCmd = formThrottleCmd(pszBackupFileName, 0, false);
	}

#ifdef USE_DDBOOST
	if (dd_boost_enabled)
	{
		temp = pstrdup(pszPassThroughParameters);
		pch = strstr(temp, "--dd_boost_dir");
		if (pch)
		{
			pch = pch + strlen("--dd_boost_dir");
			pchs = strtok(pch, " ");
			if (pchs)
				pszDDBoostDirName = pstrdup(pchs);
			else
				pszDDBoostDirName = pstrdup("db_dumps/");
		}

		pStu = strstr(temp, "--ddboost-storage-unit");
		int			pStu_len = 0;

		if (pStu)
		{
			pStu = pStu + strlen("--ddboost-storage-unit");
			pStus = strtok(pStu, " ");
			if (pStus)
			{
				pszDDBoostStorageUnitName = pstrdup(pStus);
				pStu_len = strlen(pszDDBoostStorageUnitName);
			}
		}
		pfree(temp);

		if (pszDDBoostFileName == NULL)
			elog(ERROR, "\nDDboost filename is NULL\n");

		/* Create the gpddboost parameter string */
		gpDDBoostCmdLine = psprintf(
									"%s --write-file-from-stdin --to-file=%s/%s --dd_boost_buf_size=%d ",
									gpDDBoostPg,
									pszDDBoostDirName,
									pszDDBoostFileName,
									ddboost_buf_size);

		if (pszDDBoostStorageUnitName)
		{
			gpDDBoostCmdLine = psprintf("%s --ddboost-storage-unit=%s ",
										gpDDBoostCmdLine,
										pszDDBoostStorageUnitName);
		}
	}


#endif

	/*
	 * dump schema and data for entry db, and data only for segment db. why
	 * dump data for entry db? because that will include statements to update
	 * the sequence tables (last values). As for actual data - there isn't
	 * any, so we will just dump empty COPY statements...
	 */
	pszDataOption = (instid == MASTER_CONTENT_ID) ? "--pre-and-post-data-schema-only" : "-a";

	/* Create the --gp-k parameter string */
	if (is_old_format)
		pszKeyParm = psprintf("%s_%d_%d_%s", pszBackupKey, (instid == -1) ? 1 : 0, segid, pszPassThroughCredentials);
	else
		pszKeyParm = psprintf("%s_%d_%d_%s", pszBackupKey, instid, segid, pszPassThroughCredentials);

	initStringInfo(&cmdBuf);

#ifdef USE_DDBOOST
	if (dd_boost_enabled)
	{

		if (pszCompressionProgram[0] != '\0')
		{
			/*
			 * gp_dump_agent + options, pipe into compression program, direct
			 * stdout to backup file and stderr to status file
			 */
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s --dd_boost_file %s --dd_boost_dir %s --dd_boost_buf_size %d -p %d -U %s %s %s %s 2> %s | %s | %s",
							 bkPg, pszKeyParm, pszBackupDirectory, pszDDBoostFileName, pszDDBoostDirName, ddboost_buf_size,
							 port, pszUserName, pszPassThroughParameters, pszDataOption, pszDBName, pszStatusFileName, compPg,
							 gpDDBoostCmdLine);
		}
		else
		{
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s --dd_boost_file %s --dd_boost_dir %s --dd_boost_buf_size %d -p %d -U %s %s %s %s 2> %s | %s",
							 bkPg, pszKeyParm, pszBackupDirectory, pszDDBoostFileName, pszDDBoostDirName, ddboost_buf_size,
							 port, pszUserName, pszPassThroughParameters, pszDataOption, pszDBName,
							 pszStatusFileName, gpDDBoostCmdLine);
		}
	}
	else
#endif
	if (netbackup_enabled)
	{
		/*
		 * gp_dump_agent + options, pipe into compression program, direct
		 * stdout to backup file and stderr to status file
		 */
		if (pszCompressionProgram[0] != '\0')
		{
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s -p %d -U %s %s %s %s 2> %s | %s -f | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
							 bkPg, pszKeyParm, pszBackupDirectory,
							 port, pszUserName, pszPassThroughParameters, pszDataOption, pszDBName,
							 pszStatusFileName, compPg, gpNBUDumpPg, netbackup_service_host,
							 netbackup_policy, netbackup_schedule, pszBackupFileName);
		}
		else
		{
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s -p %d -U %s %s %s %s 2> %s | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
							 bkPg, pszKeyParm, pszBackupDirectory,
							 port, pszUserName, pszPassThroughParameters, pszDataOption, pszDBName,
							 pszStatusFileName, gpNBUDumpPg, netbackup_service_host,
							 netbackup_policy, netbackup_schedule, pszBackupFileName);
		}
		if (netbackup_block_size)
		{
			appendStringInfo(&cmdBuf, " --netbackup-block-size %s", netbackup_block_size);
		}
		if (netbackup_keyword)
		{
			appendStringInfo(&cmdBuf, " --netbackup-keyword %s", netbackup_keyword);
		}
	}
	else
	{
		/* if user selected a compression program */
		if (pszCompressionProgram[0] != '\0')
		{
			/*
			 * gp_dump_agent + options, pipe into compression program, direct
			 * stdout to backup file and stderr to status file
			 */
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s -p %d -U %s %s %s %s 2> %s | %s %s",
							 bkPg, pszKeyParm, pszBackupDirectory, port, pszUserName, pszPassThroughParameters,
							 pszDataOption, pszDBName, pszStatusFileName, compPg, pszThrottleCmd);
		}
		else
		{
			appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s -p %d -U %s %s %s %s 2> %s %s",
							 bkPg, pszKeyParm, pszBackupDirectory,
							 port, pszUserName, pszPassThroughParameters, pszDataOption, pszDBName,
							 pszStatusFileName, pszThrottleCmd);
		}
	}
	pszCmdLine = cmdBuf.data;

	elog(LOG, "gp_dump_agent command line: %s", pszCmdLine);


	/* Fork off gp_dump_agent	*/
#ifdef _WIN32
	exit(1);
#else
	newpid = fork();
#endif
	if (newpid < 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Could not fork a process for backup of database %s", pszDBName)));
	}
	else if (newpid == 0)
	{

		/* This execs a shell that runs the gp_dump_agent program	*/
		execl("/bin/sh", "sh", "-c", pszCmdLine, NULL);

		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error in gp_backup_launch - execl of %s with Command Line %s failed",
						"/bin/sh", pszCmdLine)));
		_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
	}

	/*
	 * If we are the master, we do both the pre and post data schema dump
	 */
	if (instid == MASTER_CONTENT_ID)
	{
		int			stat;

		pid_t		w = waitpid(newpid, &stat, 0);

		if (w == -1)
		{
			elog(ERROR, "Dump agent failure: %m");
		}

		/* re-format dump file name and create temp file */
		pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_compress, true, NULL);
		char	   *pszTempBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, false, true, "_temp");

		if (gp_backup_directIO)
		{
			pszThrottleCmd = formThrottleCmd(pszBackupFileName, gp_backup_directIO_read_chunk_mb, true);
		}
		else
		{
			pszThrottleCmd = formThrottleCmd(pszBackupFileName, 0, false);
		}

		initStringInfo(&cmdBuf);

#ifdef USE_DDBOOST
		if (dd_boost_enabled)
		{
			pszDDBoostFileName = formDDBoostFileName(pszBackupKey, true, is_compress);

			appendStringInfo(&cmdBuf,
							 "%s --write-file-from-stdin --to-file=%s/%s "
							 "--dd_boost_buf_size=%d ",
							 gpDDBoostPg,
							 pszDDBoostDirName,
							 pszDDBoostFileName,
							 ddboost_buf_size);

			if (pszDDBoostStorageUnitName)
			{
				appendStringInfo(&cmdBuf, "--ddboost-storage-unit=%s", pszDDBoostStorageUnitName);
			}
			gpDDBoostCmdLine = cmdBuf.data;

			/* if user selected a compression program */
			if (pszCompressionProgram[0] != '\0')
			{
				/*
				 * gp_dump_agent + options, pipe into compression program,
				 * direct stdout to backup file and stderr to status file
				 */
				pszCmdLine = psprintf("cat %s | %s | %s",
									  pszTempBackupFileName, compPg, gpDDBoostCmdLine);
			}
			else
			{
				pszCmdLine = psprintf("cat %s | %s", pszTempBackupFileName, gpDDBoostCmdLine);
			}
		}
		else
#endif
		if (netbackup_enabled)
		{
			initStringInfo(&cmdBuf);

			if (pszCompressionProgram[0] != '\0')
			{
				appendStringInfo(&cmdBuf, "cat %s | %s -f | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
								 pszTempBackupFileName, compPg, gpNBUDumpPg, netbackup_service_host,
								 netbackup_policy, netbackup_schedule, pszBackupFileName);
			}
			else
			{
				appendStringInfo(&cmdBuf, "cat %s | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
								 pszTempBackupFileName, gpNBUDumpPg, netbackup_service_host,
								 netbackup_policy, netbackup_schedule, pszBackupFileName);
			}
			if (netbackup_block_size)
			{
				appendStringInfo(&cmdBuf, " --netbackup-block-size %s", netbackup_block_size);
			}
			if (netbackup_keyword)
			{
				appendStringInfo(&cmdBuf, " --netbackup-keyword %s", netbackup_keyword);
			}
			pszCmdLine = cmdBuf.data;
		}
		else
		{
			/* if user selected a compression program */
			if (pszCompressionProgram[0] != '\0')
			{
				/*
				 * gp_dump_agent + options, pipe into compression program,
				 * direct stdout to backup file and stderr to status file
				 */
				pszCmdLine = psprintf("cat %s | %s %s", pszTempBackupFileName, compPg, pszThrottleCmd);
			}
			else
			{
				pszCmdLine = psprintf("cat %s %s", pszTempBackupFileName, pszThrottleCmd);
			}
		}

		elog(LOG, "gp_dump_agent command line : %s", pszCmdLine);

#ifdef _WIN32
		exit(1);
#else
		newpid = fork();
#endif
		if (newpid < 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Could not fork a process for backup of database %s", pszDBName)));
		}
		else if (newpid == 0)
		{

			/*
			 * This execs a shell that runs the shell program to cat local
			 * post dump data to dump file
			 */
			execl("/bin/sh", "sh", "-c", pszCmdLine, NULL);

			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Error in gp_backup_launch - execl of %s with Command Line %s failed",
							"/bin/sh", pszCmdLine)));
			_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
		}

		w = waitpid(newpid, &stat, 0);
		if (w == -1)
		{
			elog(ERROR, "Dump agent failure from post schema dump: %m");
		}

		if (remove(pszTempBackupFileName) != 0)
		{
			elog(WARNING, "Failed to remove temporary post schema dump file \"%s\": %m", pszTempBackupFileName);
		}
	}

	/* Restore process interval timers */
	restoreTimers(&savetimers);

	Assert(pszSaveBackupfileName != NULL && pszSaveBackupfileName[0] != '\0');

	return CStringGetTextDatum(pszSaveBackupfileName);
}


/*
 * gp_restore_launch__( TEXT, TEXT, TEXT, TEXT, TEXT, TEXT, INT, bool ) returns TEXT
 *
 * Called by gp_restore.c to launch gp_restore_agent on this host.
 */
PG_FUNCTION_INFO_V1(gp_restore_launch__);
Datum
gp_restore_launch__(PG_FUNCTION_ARGS)
{
	/* Fetch the parameters from the argument list. */
	char	   *pszBackupDirectory = "./";
	char	   *pszBackupKey = "";
	char	   *pszCompressionProgram = "";
	char	   *pszPassThroughParameters = "";
	char	   *pszPassThroughCredentials = "";
	char	   *pszPassThroughTargetInfo = "";
	bool		bOnErrorStop = false;
	int			instid;			/* dispatch node */
	int			segid;
	int			target_dbid = 0;	/* keep compiler quiet. value will get
									 * replaced */
	char	   *pszOnErrorStop;
	char	   *pszBackupFileName;
	char	   *pszStatusFileName;
	struct stat info;
	char	   *pszDBName;
	char	   *pszUserName;
	pid_t		newpid;
	char	   *pszCmdLine;
	int			port;
	char	   *pszKeyParm;
	bool		is_decompress;
	bool		is_file_compressed; /* is the dump file ending with '.gz' */
	bool		postDataSchemaOnly;
	itimers		savetimers;
	char	   *temp;
#ifdef USE_DDBOOST
	char	   *dd_boost_buffer_size = NULL;
	char	   *mkdirStr = NULL;

#endif

	postDataSchemaOnly = false;

	/* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = TextDatumGetCString(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = TextDatumGetCString(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszCompressionProgram = TextDatumGetCString(PG_GETARG_TEXT_P(2));

	if (!PG_ARGISNULL(3))
		pszPassThroughParameters = TextDatumGetCString(PG_GETARG_TEXT_P(3));

	if (!PG_ARGISNULL(4))
		pszPassThroughCredentials = TextDatumGetCString(PG_GETARG_TEXT_P(4));

	if (!PG_ARGISNULL(5))
		pszPassThroughTargetInfo = TextDatumGetCString(PG_GETARG_TEXT_P(5));

	if (!PG_ARGISNULL(6))
		target_dbid = PG_GETARG_INT32(6);

	if (!PG_ARGISNULL(7))
		bOnErrorStop = PG_GETARG_BOOL(7);

	pszOnErrorStop = bOnErrorStop ? "--gp-e" : "";

	/*
	 * dump_prefix must be allocated in TopMemoryContext, because it is looked
	 * at by later gp_read_backup_file() calls in the same backend.
	 */
	if (dump_prefix)
		pfree(dump_prefix);
	temp = parse_prefix_from_params(pszPassThroughParameters);
	if (temp)
		dump_prefix = MemoryContextStrdup(TopMemoryContext, temp);

	statusDirectory = parse_status_from_params(pszPassThroughParameters);
	netbackup_service_host = parse_option_from_params(pszPassThroughParameters, "--netbackup-service-host");
	netbackup_block_size = parse_option_from_params(pszPassThroughParameters, "--netbackup-block-size");

	if (strstr(pszPassThroughParameters, "--old-format") != NULL)
		is_old_format = 1;

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);
	/* if statusDirectory is NULL, it defaults to SEGMENT DATA DIRECTORY */
	statusDirectory = relativeToAbsolute(statusDirectory);

	/* Validate existence of compression program and gp_restore_agent program */
	testCmdsExist(pszCompressionProgram, "gp_restore_agent");

	/* are we going to use a decompression program? */
	is_decompress = (pszCompressionProgram != NULL && *pszCompressionProgram != '\0' ? true : false);

	/* Post data pass? */
	postDataSchemaOnly = strstr(pszPassThroughParameters, "--post-data-schema-only") != NULL;

	/* Form backup file path name */
	pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_decompress, postDataSchemaOnly, NULL);
	pszStatusFileName = formStatusFilePathName(statusDirectory, pszBackupKey, false);

	if (strstr(pszPassThroughParameters, "--netbackup-service-host") != NULL)
	{
		netbackup_enabled = 1;
	}

	if (netbackup_enabled)
	{
		gpNBUQueryPg = testProgramExists("gp_bsa_query_agent");
		if (gpNBUQueryPg == NULL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("%s not found in PGPATH or PATH (PGPATH: %s | PATH:  %s)",
							"gp_bsa_query_agent", getenv("PGPATH"), getenv("PATH")),
					 errhint("Restart the server and try again")));
		}

	}

#ifdef USE_DDBOOST
	if (strstr(pszPassThroughParameters, "--dd_boost_enabled") != NULL)
	{
		dd_boost_enabled = 1;

		/*
		 * When restoring from DDBoost to a fresh cluster, we will not have
		 * the * db_dumps/<date> directory. This is required to store the
		 * status file * Fix is to create the specified backup directory for
		 * every ddboost    * restore. It a directory already exists, then no
		 * side effect is seen  *
		 */
		mkdirStr = psprintf("mkdir -p %s", pszBackupDirectory);
		if (system(mkdirStr))
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Creating backup directory %s failed: %m", pszBackupDirectory)));
		}
	}
	/* Initialise the DDBoost connection and use it to open the file */

	/* Make sure pszBackupFileName exists */
	if (!dd_boost_enabled)
	{
#endif
		if (0 != stat(pszBackupFileName, &info))
		{
			if (netbackup_enabled)
			{
				char	   *queriedNBUBackupFilePathName = queryNBUBackupFilePathName(netbackup_service_host, pszBackupFileName);

				elog(LOG, "Verify backup file path on netbackup server: %s\n", pszBackupFileName);
				elog(LOG, "NetBackup server file path: %s\n", queriedNBUBackupFilePathName);

				if (strncmp(pszBackupFileName, queriedNBUBackupFilePathName, strlen(pszBackupFileName)) != 0)
				{
					ereport(ERROR,
							(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
							 errmsg("Attempt to restore dbid %d failed. Query to NetBackup server for file %s failed."
									"The NetBackup server is not running or the given file was not backed up using NetBackup.",
									target_dbid, pszBackupFileName)));
				}
			}
			else
			{
				pszBackupFileName = findAcceptableBackupFilePathName(pszBackupDirectory, pszBackupKey, instid, segid);
				if (pszBackupFileName == NULL)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("Attempt to restore dbid %d failed. No acceptable dump file name found "
									" in directory %s with backup key %s and source dbid key %d", target_dbid,
									pszBackupDirectory, pszBackupKey, segid)));
			}

			/*
			 * If --gp-c is on make sure our file name has a .gz suffix. If no
			 * decompression requested make sure the file *doesn't* have a .gz
			 * suffix. This fixes the problem with forgetting to use the
			 * compression flags and gp_restore completing silently with no
			 * errors (hard to reproduce, but this check doesn't hurt)
			 */
			{
				int			len_name = strlen(pszBackupFileName);

				is_file_compressed = (strcmp(pszBackupFileName + (len_name - 3), ".gz") == 0 ? true : false);
			}

			if (is_file_compressed && !is_decompress)
				ereport(ERROR,
						(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						 errmsg("dump file appears to be compressed. Pass a decompression "
								"option to gp_restore to decompress it on the fly")));

			if (!is_file_compressed && is_decompress)
				ereport(ERROR,
						(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						 errmsg("a decompression cmd line option is used but the dump "
								"file does not appear to be compressed.")));
		}
#ifdef USE_DDBOOST
	}
#endif

	pszDBName = NULL;
	pszUserName = NULL;
	if (MyProcPort != NULL)
	{
		pszDBName = MyProcPort->database_name;
		pszUserName = MyProcPort->user_name;
	}

	if (pszDBName == NULL)
		pszDBName = "";
	else
		pszDBName = shellEscape(pszDBName, true);

	if (pszUserName == NULL)
		pszUserName = "";

	/* Clear process interval timers */
	resetTimers(&savetimers);

	/* Fork off gp_restore_agent  */
#ifdef _WIN32
	exit(1);
#else
	newpid = fork();
#endif
	if (newpid < 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Could not fork a process for backup of database %s", pszDBName)));
	}
	else if (newpid == 0)
	{
		/* Child Process */
		port = PostPortNumber;

		/* Create the --gp-k parameter string */
		pszKeyParm = psprintf("%s_%d_%d_%s", pszBackupKey, instid, segid, pszPassThroughCredentials);

#ifdef USE_DDBOOST
		dd_boost_buffer_size = psprintf("%d", ddboost_buf_size);
#endif


		/*
		 * if compression was requested with --gp-c
		 */
		if (is_decompress)
		{
			StringInfoData cmdBuf;

			initStringInfo(&cmdBuf);

			/*
			 * de-compress backupfile and pipe into stdin of gp_restore_agent
			 * along with its options. Redirect both stdout and stderr into
			 * the status file.
			 */
#ifdef USE_DDBOOST
			if (dd_boost_enabled)
			{
				appendStringInfo(&cmdBuf, "%s --gp-c %s --gp-k %s --dd_boost_buf_size %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, compPg, pszKeyParm, dd_boost_buffer_size, pszBackupDirectory, pszOnErrorStop, port, pszUserName,
								 pszPassThroughParameters, pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);
			}
			else
#endif
			if (netbackup_enabled)
			{
				appendStringInfo(&cmdBuf, "%s --gp-c %s --gp-k %s --netbackup-service-host %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, compPg, pszKeyParm, netbackup_service_host, pszBackupDirectory, pszOnErrorStop, port, pszUserName,
								 pszPassThroughParameters, pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);

				if (netbackup_block_size)
				{
					appendStringInfo(&cmdBuf, " --netbackup-block-size %s", netbackup_block_size);
				}
			}
			else
			{
				appendStringInfo(&cmdBuf, "%s --gp-c %s --gp-k %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, compPg, pszKeyParm, pszBackupDirectory, pszOnErrorStop, port, pszUserName,
								 pszPassThroughParameters, pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);
			}

			pszCmdLine = cmdBuf.data;
		}
		else
		{
			StringInfoData cmdBuf;

			initStringInfo(&cmdBuf);

			/*
			 * Format gp_restore_agent with options, and Redirect both stdout
			 * and stderr into the status file
			 */
#ifdef USE_DDBOOST
			if (dd_boost_enabled)
			{
				appendStringInfo(&cmdBuf, "%s --gp-k %s --dd_boost_buf_size %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, pszKeyParm, dd_boost_buffer_size, pszBackupDirectory, pszOnErrorStop, port, pszUserName, pszPassThroughParameters,
								 pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);
			}
			else
#endif
			if (netbackup_enabled)
			{
				appendStringInfo(&cmdBuf, "%s --gp-k %s --netbackup-service-host %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, pszKeyParm, netbackup_service_host, pszBackupDirectory, pszOnErrorStop, port, pszUserName, pszPassThroughParameters,
								 pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);
				if (netbackup_block_size)
				{
					appendStringInfo(&cmdBuf, " --netbackup-block-size %s", netbackup_block_size);
				}
			}
			else
			{
				appendStringInfo(&cmdBuf, "%s --gp-k %s --gp-d %s %s -p %d -U %s %s %s -d %s %s %s %s 2>&2",
								 bkPg, pszKeyParm, pszBackupDirectory, pszOnErrorStop, port, pszUserName, pszPassThroughParameters,
								 pszPassThroughTargetInfo, pszDBName, pszBackupFileName,
								 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);
			}
			pszCmdLine = cmdBuf.data;
		}

		elog(LOG, "gp_restore_agent command line: %s\n", pszCmdLine);

		/* This execs a shell that runs the gp_restore_agent program  */
		execl("/bin/sh", "sh", "-c", pszCmdLine, NULL);

		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error in gp_restore_launch - execl of %s with Command Line %s failed",
						"/bin/sh", pszCmdLine)));
		_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
	}

	/* Restore process interval timers */
	restoreTimers(&savetimers);

	Assert(pszBackupFileName != NULL && pszBackupFileName[0] != '\0');

	return CStringGetTextDatum(pszBackupFileName);
}

/*
 * gp_read_backup_file__( TEXT, TEXT, int ) returns TEXT
 *
 */
PG_FUNCTION_INFO_V1(gp_read_backup_file__);
Datum
gp_read_backup_file__(PG_FUNCTION_ARGS)
{
	/* Fetch the parameters from the argument list. */
	char	   *pszBackupDirectory = "./";
	char	   *pszBackupKey = "";
	int			fileType = 0;
	char	   *pszFileName;
	struct stat info;
	char	   *pszFullStatus = NULL;
	FILE	   *f;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = TextDatumGetCString(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = TextDatumGetCString(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		fileType = PG_GETARG_INT32(2);

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);

	/* Form backup file path name */
	pszFileName = NULL;
	switch (fileType)
	{
		case BFT_BACKUP:

			/*
			 * pszFileName = formBackupFilePathName(pszBackupDirectory,
			 * pszBackupKey);
			 */

			/*
			 * we can't support this without making a change to this function
			 * arguments, which will require an initdb, which we want to
			 * avoid. This is due to the fact that in this call to
			 * formBackupFilePathName we don't know if the file is compressed
			 * or not and formBackupFilePathName will not know if to create a
			 * file with a compression suffix or not
			 */
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Only status files are currently supported in gp_read_backup_file")));
			break;
		case BFT_BACKUP_STATUS:
		case BFT_RESTORE_STATUS:
			pszFileName = formStatusFilePathName(pszBackupDirectory, pszBackupKey, (fileType == BFT_BACKUP_STATUS));
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Invalid filetype %d passed to gp_read_backup_file", fileType)));
			break;
	}

	/* Make sure pszFileName exists */
	if (0 != stat(pszFileName, &info))
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Backup File %s Type %d could not be found", pszFileName, fileType)));
	}

	/* Read file */
	if (info.st_size > INT_MAX)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Backup File %s Type %d too large to read", pszFileName, fileType)));
	}

	/* Allocate enough memory for entire file, and read it in. */
	pszFullStatus = (char *) palloc(info.st_size + 1);

	f = AllocateFile(pszFileName, "r");
	if (f == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Backup File %s Type %d cannot be opened", pszFileName, fileType)));
	}

	if (info.st_size != fread(pszFullStatus, 1, info.st_size, f))
	{
		fclose(f);
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error reading Backup File %s Type %d", pszFileName, fileType)));
	}

	FreeFile(f);
	f = NULL;
	pszFullStatus[info.st_size] = '\0';

	return CStringGetTextDatum(positionToError(pszFullStatus));
}

/*
 * gp_write_backup_file__( TEXT, TEXT, TEXT ) returns TEXT
 *
 */

/* NOT IN USE ANYMORE! */

/*
 * gp_write_backup_file__( TEXT, TEXT, TEXT ) returns TEXT
 *
 */
/*
 * Greenplum TODO: This function assumes the file is not compressed.
 * This is not always the case. Therefore it should be used carefully
 * by the caller, until we decide to make the change and pass in an
 * isCompressed boolean, which unfortunately will require an initdb for
 * the change to take effect.
 */
PG_FUNCTION_INFO_V1(gp_write_backup_file__);
Datum
gp_write_backup_file__(PG_FUNCTION_ARGS)
{
	/* Fetch the parameters from the argument list. */
	char	   *pszBackupDirectory = "./";
	char	   *pszBackupKey = "";
	char	   *pszBackup = "";
	char	   *pszFileName;
	FILE	   *f;
	int			nBytes;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = TextDatumGetCString(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = TextDatumGetCString(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszBackup = TextDatumGetCString(PG_GETARG_TEXT_P(2));

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);

	/*
	 * See whether the directory exists. If not, try to create it. If so, make
	 * sure it's a directory
	 */
	validateBackupDirectory(pszBackupDirectory);

	/* Form backup file path name */
	pszFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, false, false, NULL);

	/* Write file */
	f = AllocateFile(pszFileName, "w");
	if (f == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Backup File %s cannot be opened", pszFileName)));
	}

	nBytes = strlen(pszBackup);
	if (nBytes != fwrite(pszBackup, 1, nBytes, f))
	{
		fclose(f);
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error writing Backup File %s", pszFileName)));
	}

	FreeFile(f);
	f = NULL;

	Assert(pszFileName != NULL && pszFileName[0] != '\0');

	return CStringGetTextDatum(pszFileName);
}

/*
 * createBackupDirectory( char* pszPathName ) return bool
 *
 * This will create a directory if it doesn't exist.
 *
 * This routine first attempts to create the directory using the full path (in case the
 * mkdir function is able to create the full directory).  If this mkdir fails with EEXIST,
 * filesystem object is checked to ensure it is a directory and fails with ENOTDIR if it
 * is not.
 *
 * If the "full" mkdir fails with a code other than ENOENT, this routine fails.  For the
 * ENOENT code, the routine attempts to create the directory tree from the top, one level
 * at a time, failing if unable to create a directory with a code other than EEXIST and
 * the EEXIST level is not a directory.  Being tolerant of EEXIST failures allows for
 * multiple segments to attempt to create the directory simultaneously.
 */
static bool
createBackupDirectory(char *pszPathName)
{
	int			rc;
	struct stat info;
	char	   *pSlash;

	/* Can we make it in its entirety?	If so, return */
	if (0 == mkdir(pszPathName, S_IRWXU))
		return true;

	if (errno == ENOENT)
	{
		/*
		 * Try to create each level of the hierarchy that doesn't already
		 * exist
		 */
		pSlash = pszPathName + 1;

		while (pSlash != NULL && *pSlash != '\0')
		{
			/* Find next subdirectory delimiter */
			pSlash = strchr(pSlash, '/');
			if (pSlash == NULL)
			{
				/*
				 * If no more slashes we're at the last level. Attempt to
				 * create it; if it fails for any reason other than EEXIST,
				 * fail the call.
				 */
				rc = mkdir(pszPathName, S_IRWXU);
				if (rc != 0 && errno != EEXIST)
				{
					return false;
				}
			}
			else
			{
				/* Temporarily end the string at the / we just found. */
				*pSlash = '\0';

				/* Attempt to create the level; return code checked below. */
				rc = mkdir(pszPathName, S_IRWXU);

				/*
				 * If failed and the directory level does not exist, fail the
				 * call. Is it possible for a directory level to exist and the
				 * mkdir() call fail with a code such as ENOSYS (and possibly
				 * others) instead of EEXIST.  So, if mkdir() fails, the
				 * directory level's existence is checked.  If the level
				 * does't exist or isn't a directory, the call is failed.
				 */
				if (rc != 0)
				{
					struct stat statbuf;
					int			errsave = errno;	/* Save mkdir error code
													 * over stat() call. */

					if (stat(pszPathName, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
					{
						errno = errsave;	/* Restore mkdir error code. */
						*pSlash = '/';	/* Put back the slash we overwrote
										 * above. */
						return false;
					}
				}

				/* Put back the slash we overwrote above. */
				*pSlash = '/';

				/*
				 * At this point, this level exists -- either because it was
				 * created or it already exists.  If it already exists, it
				 * *could* be a non-directory.  This will be caught when
				 * attempting to create the next level.
				 */

				/* Advance past the slash. */
				pSlash++;
			}
		}
	}
	else if (errno != EEXIST)
	{
		return false;
	}

	/*
	 * At this point, the path has been created or otherwise exists.  Ensure
	 * it's a directory.
	 */
	if (0 != stat(pszPathName, &info))
	{
		/* An unexpected failure obtaining the file info. errno is set. */
		return false;
	}
	else if (!S_ISDIR(info.st_mode))
	{
		/* The pathname exists but is not a directory! */
		errno = ENOTDIR;
		return false;
	}

	return true;
}

/* findAcceptableBackupFilePathName( char* pszBackupDirectory, char* pszBackupKey, int instid, int segid ) returns char*
 *
 * This function takes the directory and timestamp key and finds an existing file that matches
 * the naming convention for a backup file with the proper key and either the proper instid with
 * any segid, or the proper segid with any instid, to accommodate both filename formats in use.
 * In case it was backed up from a redundant instance of the same segment.
 */
static char *
findAcceptableBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, int instid, int segid)
{
	/* Make sure that pszBackupDirectory exists and is in fact a directory. */
	struct stat buf;
	struct dirent *dirp = NULL;
	DIR		   *dp = NULL;
	char	   *pszBackupFilePathName = NULL;

	/* "Old" and "New" refer  to old and new dump file name formats */
	static regex_t rFindFileNameNew,
				rFindFileNameOld,
				rFindFileNameMaster;
	static bool bFirstTime = true;
	char	   *pszFileName;
	char	   *pszSep;
	struct stat info;

	if (bFirstTime)
	{
		int			wmasklenNew,
					wmasklenOld,
					wmasklenMaster,
					masklenNew,
					masklenOld,
					masklenMaster;
		pg_wchar   *maskNew,
				   *maskOld,
				   *maskMaster;

		if (segid == 1)
		{
			/*
			 * Matches all master filenames. These are in the format
			 * gp_dump_1_1 or gp_dump_-1_1
			 */
			char	   *pszRegexMaster = psprintf("^%sgp_dump_-?1_1_%s(.gz)?$", DUMP_PREFIX, pszBackupKey);

			masklenMaster = strlen(pszRegexMaster);
			maskMaster = (pg_wchar *) palloc((masklenMaster + 1) * sizeof(pg_wchar));
			wmasklenMaster = pg_mb2wchar_with_len(pszRegexMaster, maskMaster, masklenMaster);

			if (0 != pg_regcomp(&rFindFileNameMaster, maskMaster, wmasklenMaster, REG_EXTENDED))
			{
				ereport(ERROR,
						(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						 errmsg("Could not compile regular expression for backup filename matching Master")));
			}
		}
		else
		{
			/*
			 * Matches segment filenames. These are in the format
			 * gp_dump_0_<dbid> or gp_dump_<contentid>_<dbid>
			 */
			/*
			 * This regex matches dump files with dbid of any number except 1
			 * to avoid matching master dumps
			 */
			char	   *pszRegexNew = psprintf("^%sgp_dump_%d_([02-9][0-9]*|1[0-9]+)_%s(.gz)?$", DUMP_PREFIX, instid, pszBackupKey);
			char	   *pszRegexOld = psprintf("^%sgp_dump_0_%d_%s(.gz)?$", DUMP_PREFIX, segid, pszBackupKey);

			masklenNew = strlen(pszRegexNew);
			maskNew = (pg_wchar *) palloc((masklenNew + 1) * sizeof(pg_wchar));
			wmasklenNew = pg_mb2wchar_with_len(pszRegexNew, maskNew, masklenNew);

			masklenOld = strlen(pszRegexOld);
			maskOld = (pg_wchar *) palloc((masklenOld + 1) * sizeof(pg_wchar));
			wmasklenOld = pg_mb2wchar_with_len(pszRegexOld, maskOld, masklenOld);

			if (0 != pg_regcomp(&rFindFileNameNew, maskNew, wmasklenNew, REG_EXTENDED))
			{
				ereport(ERROR,
						(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						 errmsg("Could not compile regular expression for backup filename matching New")));
			}

			if (0 != pg_regcomp(&rFindFileNameOld, maskOld, wmasklenOld, REG_EXTENDED))
			{
				ereport(ERROR,
						(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						 errmsg("Could not compile regular expression for backup filename matching Old")));
			}
		}
		bFirstTime = false;
	}

	if (lstat(pszBackupDirectory, &buf) < 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup Directory %s does not exist.", pszBackupDirectory)));
	}

	if (S_ISDIR(buf.st_mode) == 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup Location %s is not a directory.", pszBackupDirectory)));
	}

	dp = opendir(pszBackupDirectory);
	if (dp == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup Directory %s cannot be opened for enumerating files.", pszBackupDirectory)));
	}

	while (NULL != (dirp = readdir(dp)))
	{
		pg_wchar   *data;
		size_t		data_len;
		int			newfile_len;
		int			backup_file_path_len;

		pszFileName = dirp->d_name;

		/* Convert data string to wide characters */
		newfile_len = strlen(pszFileName);
		data = (pg_wchar *) palloc((newfile_len + 1) * sizeof(pg_wchar));
		data_len = pg_mb2wchar_with_len(pszFileName, data, newfile_len);

		if (0 == pg_regexec(&rFindFileNameNew, data, data_len, 0, NULL, 0, NULL, 0) ||
			0 == pg_regexec(&rFindFileNameOld, data, data_len, 0, NULL, 0, NULL, 0) ||
			0 == pg_regexec(&rFindFileNameMaster, data, data_len, 0, NULL, 0, NULL, 0))
		{
			pszSep = "/";
			if (strlen(pszBackupDirectory) >= 1 && pszBackupDirectory[strlen(pszBackupDirectory) - 1] == '/')
				pszSep = "";

			backup_file_path_len = strlen(pszBackupDirectory) + strlen(pszFileName) + strlen(pszSep) + 1;
			pszBackupFilePathName = (char *) palloc(backup_file_path_len);
			snprintf(pszBackupFilePathName, backup_file_path_len, "%s%s%s", pszBackupDirectory, pszSep, pszFileName);

			/* Make sure that this is a regular file */
			if (0 == stat(pszBackupFilePathName, &info) && S_ISREG(info.st_mode))
			{
				break;
			}
		}
	}
	closedir(dp);

	return pszBackupFilePathName;
}

/* formBackupFilePathName( char* pszBackupDirectory, char* pszBackupKey ) returns char*
 *
 * This function takes the directory and timestamp key and creates
 * the path and filename of the backup output file
 * based on the naming convention for this.
 */
static char *
formBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool is_compress, bool isPostData, const char *suffix)
{
	int			instid;			/* dispatch node */
	int			segid;
	StringInfoData buf;

	/* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
		instid = (instid == -1) ? 1 : 0;	/* dispatch node */

	initStringInfo(&buf);

	appendStringInfoString(&buf, pszBackupDirectory);
	if (pszBackupDirectory[strlen(pszBackupDirectory) - 1] != '/')
		appendStringInfoChar(&buf, '/');

	/* prefix */
	appendStringInfo(&buf, "%sgp_dump_%d_%d_", DUMP_PREFIX, instid, segid);

	appendStringInfoString(&buf, pszBackupKey);

	if (isPostData)
		appendStringInfoString(&buf, "_post_data");

	/* if gzip/gunzip is used, the suffix is gz */
	if (is_compress)
		appendStringInfoString(&buf, ".gz");

	if (suffix)
		appendStringInfoString(&buf, suffix);

	if (buf.len > PATH_MAX)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup FileName based on path %s and key %s too long", pszBackupDirectory, pszBackupKey)));
	}

	return buf.data;
}

/* formStatusFilePathName( char* pszBackupDirectory, char* pszBackupKey, bool bIsBackup ) returns char*
 *
 * This function takes the directory and timestamp key and IsBackup flag and creates
 * the path and filename of the backup or restore status file
 * based on the naming convention for this.
 */
static char *
formStatusFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool bIsBackup)
{
	int			instid;			/* dispatch node */
	int			segid;
	StringInfoData fileName;

	Assert(strlen(pszBackupDirectory) > 0);

	/* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
		instid = (instid == -1) ? 1 : 0;

	initStringInfo(&fileName);

	appendStringInfoString(&fileName, pszBackupDirectory);
	if (pszBackupDirectory[strlen(pszBackupDirectory) - 1] != '/')
		appendStringInfoChar(&fileName, '/');

	/* prefix */
	appendStringInfo(&fileName, "%sgp_%s_status_%d_%d_",
					 DUMP_PREFIX, (bIsBackup ? "dump" : "restore"),
					 instid, segid);

	appendStringInfoString(&fileName, pszBackupKey);

	if (fileName.len > PATH_MAX)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Status FileName based on path %s and key %s too long",
						pszBackupDirectory, pszBackupKey)));
	}

	return fileName.data;
}

/* This function will query NetBackup server to find out if a given filename has been
 * backed up using NetBackup. If the query returns NULL, it implies that the given file
 * was never backed up using that NetBackup server. If the query returns the file path,
 * it implies that the given file has been backed up using the given NetBackup server and
 * it can be restored from that server.
 */
static char *
queryNBUBackupFilePathName(char *netbackupServiceHost, char *netbackupRestoreFileName)
{
	FILE	   *fp;
	char		resBuff[512];
	char	   *cmd;
	StringInfoData queryResultFileName;

	cmd = psprintf("%s --netbackup-service-host %s --netbackup-filename %s\n",
				   gpNBUQueryPg, netbackupServiceHost, netbackupRestoreFileName);
	elog(LOG, "NetBackup query restore filename command line: %s", cmd);

	fp = popen(cmd, "r");
	if (fp == NULL)
	{
		ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						errmsg("Error querying restore filename to NetBackup server"),
						errhint("NetBackup server not running or restore file: %s has not been backed up to the NetBackup server", netbackupRestoreFileName)));
		return NULL;
	}

	initStringInfo(&queryResultFileName);
	while (fgets(resBuff, sizeof(resBuff), fp) != NULL)
	{
		appendStringInfoString(&queryResultFileName, resBuff);
	}

	if (pclose(fp) != 0)
	{
		ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						errmsg("Error querying restore filename to NetBackup server"),
						errhint("NetBackup server not running or restore file: %s has not been backed up to the NetBackup server", netbackupRestoreFileName)));
		return NULL;
	}

	return queryResultFileName.data;
}

/* formThrottleCmd(char *pszBackupFileName, int directIO_read_chunk_mb, bool bIsThrottlingEnabled) returns char*
 *
 * This function takes the backup file name and creates
 * the throttlin command
 */
static char *
formThrottleCmd(char *pszBackupFileName, int directIO_read_chunk_mb, bool bIsThrottlingEnabled)
{
	if (bIsThrottlingEnabled)
	{
		/* Create the throttlingD.py string */
		return psprintf("| throttlingD.py %d %s", directIO_read_chunk_mb, pszBackupFileName);
	}
	else
	{
		/*
		 * when directIO is disabled --> need to pipe the dump to backupFile
		 * (No throttling will be done)
		 */
		/* Create the empty throttling string */
		return psprintf(" > %s", pszBackupFileName);
	}
}

/*
 * relativeToAbsolute( char* pszRelativeDirectory ) returns char*
 *
 * This will turn a relative path into an absolute one, based off of the Data Directory
 * for this database.
 *
 * If the path isn't absolute, it is prefixed with DataDir/.
 */
static char *
relativeToAbsolute(char *pszRelativeDirectory)
{
	if (pszRelativeDirectory != NULL && pszRelativeDirectory[0] == '/')
		return pszRelativeDirectory;

	if (pszRelativeDirectory != NULL)
	{
		return psprintf("%s/%s", DataDir, pszRelativeDirectory);
	}
	else
	{
		return DataDir;
	}
}

/* testCmdsExist( char* pszCompressionProgram ) returns void
 *
 * This function tests whether pszBackupProgram and the pszCompressionProgram
 * (if non-empty) are in the path.	If not, it calls ereport.
 */
static void
testCmdsExist(char *pszCompressionProgram, char *pszBackupProgram)
{
	/* Does pszBackupProgram exist? */
	if (pszBackupProgram != NULL &&
		*pszBackupProgram != '\0' &&
		(bkPg = testProgramExists(pszBackupProgram)) == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("%s not found in PGPATH or PATH (PGPATH: %s | PATH:  %s)",
						pszBackupProgram, getenv("PGPATH"), getenv("PATH")),
				 errhint("Restart the server and try again")));
	}

	/* Does compression utility exist? */
	if (pszCompressionProgram != NULL &&
		*pszCompressionProgram != '\0' &&
		(compPg = testProgramExists(pszCompressionProgram)) == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("%s (compression utility) not found in in PGPATH or "
						"PATH (PGPATH: %s | PATH:  %s)",
						pszCompressionProgram, getenv("PGPATH"), getenv("PATH"))));
	}
}

/* testProgramExists(char* pszProgramName) returns bool
 * This runs a shell with which pszProgramName > tempfile
 * piping the output to a temp file.
 * If the file is empty, then pszProgramName didn't exist
 * on the path.
 */
static char *
testProgramExists(char *pszProgramName)
{
	char	   *pszProgramNameLocal;
	char	   *p;

	/* Now see what the length of the file is  - 0 means not there */
	struct stat buf;
	char	   *pszEnvPath;
	char	   *pszPath;
	char	   *pColon;
	char	   *pColonNext;
	char	   *pszTestPath;

	if (pszProgramName == NULL || *pszProgramName == '\0')
		return NULL;

	/*
	 * The pszProgramName might have command line arguments, so we need to
	 * stop at the first whitespace
	 */
	pszProgramNameLocal = pstrdup(pszProgramName);

	p = pszProgramNameLocal;
	while (*p != '\0')
	{
		if (*p == ' ' || *p == '\t' || *p == '\n')
			break;

		p++;
	}

	*p = '\0';

	pszEnvPath = getenv("PGPATH");
	if (pszEnvPath != NULL)
	{
		pszTestPath = psprintf("%s/%s", pszEnvPath, pszProgramNameLocal);
		if (stat(pszTestPath, &buf) >= 0)
		{
			return pszTestPath;
		}
	}
	pszEnvPath = getenv("PATH");
	if (pszEnvPath == NULL)
		return NULL;
	pszPath = pstrdup(pszEnvPath);
	/* Try to create each level of the hierarchy that doesn't already exist */
	pColon = pszPath;

	while (pColon != NULL && *pColon != '\0')
	{
		/* Find next delimiter */
		pColonNext = strchr(pColon, ':');

		if (pColonNext == pColon)
		{
			pColon++;
			continue;
		}

		if (pColonNext == NULL)
		{
			/* See whether pszProgramName exists in subdirectory pColon */
			pszTestPath = psprintf("%s/%s", pColon, pszProgramNameLocal);

			if (stat(pszTestPath, &buf) >= 0)
				return pszTestPath;
			else
				return NULL;
		}

		/* Temporarily end the string at the : we just found. */
		*pColonNext = '\0';

		/* See whether pszProgramName exists in subdirectory pColon */
		pszTestPath = psprintf("%s/%s", pColon, pszProgramNameLocal);
		if (stat(pszTestPath, &buf) >= 0)
		{
			*pColonNext = ':';
			return pszTestPath;
		}

		/* Put back the colon we overwrote above. */
		*pColonNext = ':';

		/* Advance past the colon. */
		pColon = pColonNext + 1;

		/* free */
		pfree(pszTestPath);
	}

	return NULL;
}

/* validateBackupDirectory( char* pszBackupDirectory ) returns void
 * This sees whether the backupDirectory exists
 * If so, it makes sure its a directory and not a file.
 * If not, it tries to create it.  If it cannot it calls ereport
 */
static void
validateBackupDirectory(char *pszBackupDirectory)
{
	struct stat info;

	if (stat(pszBackupDirectory, &info) < 0)
	{
		elog(DEBUG1, "Backup Directory %s does not exist", pszBackupDirectory);

		/* Attempt to create it. */
		if (!createBackupDirectory(pszBackupDirectory))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not create backup directory \"%s\": %m",
							pszBackupDirectory)));
		}
		else
		{
			elog(DEBUG1, "Successfully created Backup Directory %s", pszBackupDirectory);
		}
	}
	else
	{
		if (!S_ISDIR(info.st_mode))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("BackupDirectory %s exists but is not a directory.", pszBackupDirectory)));
		}
	}
}

/*
 * positionToError - given a char buffer, position to the first error msg in it.
 *
 * we are only interested in ERRORs. therefore, search for the first ERROR
 * and return the rest of the status file starting from that point. if no error
 * found, return a "no error found" string. We are only interested in occurrences
 * of "ERROR:" (from the backend), or [ERROR] (from dump/restore log). others,
 * like "ON_ERROR_STOP" should be ignored.
 */
static char *
positionToError(char *source)
{
	char	   *sourceCopy = source;
	char	   *firstErr = NULL;
	char	   *defaultNoErr = "No extra error information available. Please examine status file.";

	while (true)
	{
		firstErr = strstr((const char *) sourceCopy, "ERROR");

		if (!firstErr)
		{
			break;
		}
		else
		{
			/* found "ERROR". only done if it's "ERROR:" or "[ERROR]" */
			if (firstErr[5] == ']' || firstErr[5] == ':')
			{
				break;			/* done */
			}
			else
			{
				sourceCopy = ++firstErr;	/* start from stopping point */
				firstErr = NULL;
			}
		}
	}

	if (!firstErr)
	{
		/* no [ERROR] found */
		firstErr = defaultNoErr;
	}
	else
	{
		/* found [ERROR]. go back to beginning of the line */
		char	   *p;

		for (p = firstErr; p > source; p--)
		{
			if (*p == '\n')
			{
				p++;			/* skip the LF */
				break;
			}
		}

		firstErr = p;
	}

	return firstErr;
}

#ifdef USE_DDBOOST
static char *
formDDBoostFileName(char *pszBackupKey, bool isPostData, bool isCompress)
{
	/* First form the prefix */
	int			instid;			/* dispatch node */
	int			segid;
	StringInfoData backupFileName;

	/* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
		instid = (instid == -1) ? 1 : 0;	/* dispatch node */

	initStringInfo(&backupFileName);

	/* prefix */
	appendStringInfo(&backupFileName, "%sgp_dump_%d_%d_", DUMP_PREFIX, instid, segid);

	appendStringInfo(&backupFileName, "%s", pszBackupKey);

	if (isPostData)
		appendStringInfoString(&backupFileName, "_post_data");

	if (isCompress)
		appendStringInfoString(&backupFileName, ".gz");

	if (backupFileName.len > PATH_MAX)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup FileName based on key %s too long", pszBackupKey)));

	return backupFileName.data;
}
#endif


/*
 * shellEscape: Returns a string in which the shell-significant quoted-string characters are
 * escaped.
 *
 * This function escapes the following characters: '"', '$', '`', '\', '!', '''.
 */
static char *
shellEscape(const char *shellArg, bool addQuote)
{
	const char *s = shellArg;
	const char	escape = '\\';
	StringInfoData escapeBuf;

	initStringInfo(&escapeBuf);

	if (addQuote)
		appendStringInfoChar(&escapeBuf, '\"');

	/*
	 * Copy the shellArg into the escapeBuf prepending any characters
	 * requiring an escape with the escape character.
	 */
	while (*s != '\0')
	{
		switch (*s)
		{
			case '"':
			case '$':
			case '\\':
			case '`':
			case '!':
				appendStringInfoChar(&escapeBuf, escape);
		}
		appendStringInfoChar(&escapeBuf, *s);
		s++;
	}

	if (addQuote)
		appendStringInfoChar(&escapeBuf, '\"');
	return escapeBuf.data;
}
