/*-------------------------------------------------------------------------
 *
 * cdbbackup.c
 *	  utilities for invoking backup/restore at segments
 *
 * Copyright (c) 2017 Pivotal Software, Inc
 *
 * IDENTIFICATION
 *			src/backend/cdb/cdbbackup.c
 *-------------------------------------------------------------------------
 */

#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <sys/wait.h>

#include "postgres.h"

#include "regex/regex.h"
#include "libpq/libpq-be.h"
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
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

#define EMPTY_STR '\0'

#define DUMP_PREFIX (dump_prefix == NULL ? "" : dump_prefix)

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define COMMAND_MAX 65536
#define KEY_MAX 64
#define SCHEMA_MAX 1024

#define EXIT_CODE_BACKUP_RESTORE_ERROR 127

/* general utility copied from cdblink.c */
#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

/* static helper functions */
static bool createBackupDirectory(char *pszPathName);
static char *findAcceptableBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, int instid, int segid);
static char *formBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool is_compress, bool isPostData, const char *suffix);
static char *formStatusFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool bIsBackup);
static char *formThrottleCmd(char *pszBackupFileName, int directIO_read_chunk_mb, bool bIsThrottlingEnabled);
static char *relativeToAbsolute(char *pszRelativeDirectory);
static char *testProgramExists(char *pszProgramName, bool allowMissing);
static void validateBackupDirectory(char *pszBackupDirectory);
static char *positionToError(char *source);
static char *parse_prefix_from_params(char *params);
static char *parse_status_from_params(char *params);
static char *parse_option_from_params(char *params, char *option);
static char *queryNBUBackupFilePathName(char *netbackupServiceHost, char *netbackupRestoreFileName);
static char *shellEscape(const char *shellArg, PQExpBuffer escapeBuf, bool addQuote);

#ifdef USE_DDBOOST

static char *formDDBoostFileName(char *pszBackupKey, bool isPostData, bool isCompress);

static int dd_boost_enabled = 0;
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
static int netbackup_enabled = 0;
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
	char *temp = strdup(params);
	if (temp == NULL)
		ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
	char *pch = strstr(temp, "--prefix");
	if (pch)
	{
		pch = pch + strlen("--prefix");
		char *ptr = strtok(pch," ");
		if (ptr == NULL)
			return NULL;
		return ptr;
	}
	return NULL;
}

static char *
parse_status_from_params(char *params)
{
	char *temp;
	char *pch;

	if (params == NULL)
		return NULL;

	temp = pstrdup(params);
	pch = strstr(temp, "--status");
	if (pch)
	{
		pch = pch + strlen("--status");
		char *ptr = strtok(pch, " ");
		if (ptr == NULL)
			return NULL;
		return ptr;
	}
	return NULL;
}

static char *
parse_option_from_params(char *params, char *option)
{
	char *temp;
	char *pch;

	if ((params == NULL) || (option == NULL))
		return NULL;
	
	temp = pstrdup(params);
	pch = strstr(temp, option);

	if (pch)
	{
		pch = pch + strlen(option);
		char *ptr = strtok(pch," ");
		if (ptr == NULL) {
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
	char	   *pszBackupFileName;
	char	   *pszStatusFileName;
	char       *pszSaveBackupfileName;
	char	   *pszDBName,
	           *pszUserName;
	pid_t		newpid;
	StringInfoData pszCmdLine;
	int			port;
	int			segid;
	int			instid;			/* dispatch node */
	bool		is_compress;
	itimers		savetimers;

	char       *pszThrottleCmd;
	char        emptyStr = EMPTY_STR;

#ifdef USE_DDBOOST
	char	   *pszDDBoostFileName = NULL;
	char	   *pszDDBoostDirName = "db_dumps";		/* Default directory */
	char	   *pszDDBoostStorageUnitName = NULL;
	char	   *temp = NULL, *pch = NULL, *pchs = NULL, *pStu = NULL, *pStus = NULL;
#endif

       /* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = GET_STR(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = GET_STR(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszCompressionProgram = GET_STR(PG_GETARG_TEXT_P(2));

	if (!PG_ARGISNULL(3))
		pszPassThroughParameters = GET_STR(PG_GETARG_TEXT_P(3));

	if (!PG_ARGISNULL(4))
		pszPassThroughCredentials = GET_STR(PG_GETARG_TEXT_P(4));

	dump_prefix = parse_prefix_from_params(pszPassThroughParameters);

	netbackup_service_host = parse_option_from_params(pszPassThroughParameters, "--netbackup-service-host");
	netbackup_policy = parse_option_from_params(pszPassThroughParameters, "--netbackup-policy");
	netbackup_schedule = parse_option_from_params(pszPassThroughParameters, "--netbackup-schedule");
	netbackup_block_size = parse_option_from_params(pszPassThroughParameters, "--netbackup-block-size");
	netbackup_keyword = parse_option_from_params(pszPassThroughParameters, "--netbackup-keyword");

	if (strstr(pszPassThroughParameters,"--old-format") != NULL)
		is_old_format = 1;

	/*
	 * If BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);

	/*
	 * See whether the directory exists.  If not, try to create it, else make
	 * sure it's a directory.
	 */
	validateBackupDirectory(pszBackupDirectory);

	/*
	 * Validate existence of gp_dump_agent and compression program, if
	 * possible
	 */
	testProgramExists("gp_dump_agent", false);
	if (pszCompressionProgram)
		testProgramExists(pszCompressionProgram, false);

	/* Are we going to use a compression program? */
	is_compress = (pszCompressionProgram != NULL && *pszCompressionProgram != '\0' ? true : false);

	/* Form backup file path name */
	pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_compress, false, NULL);
	pszStatusFileName = formStatusFilePathName(pszBackupDirectory, pszBackupKey, true);
	pszSaveBackupfileName = pstrdup(pszBackupFileName);

	/* Initialise the DDBoost connection and use it to open the file */
#ifdef USE_DDBOOST
	if (strstr(pszPassThroughParameters,"--dd_boost_enabled") != NULL)
	{
		dd_boost_enabled = 1;
		pszDDBoostFileName = formDDBoostFileName(pszBackupKey, false, is_compress);
		gpDDBoostPg = testProgramExists("gpddboost", false);
	}
#endif

	if (strstr(pszPassThroughParameters,"--netbackup-service-host") != NULL)
	{
		netbackup_enabled = 1;
		gpNBUDumpPg = testProgramExists("gp_bsa_dump_agent", false);
	}

	PQExpBuffer escapeBuf = NULL;

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
		/*
		 * pszDBName will be pointing to the data portion of the PQExpBuffer
		 * once escpaped (escapeBuf->data), so we can't safely destroy the
		 * escapeBuf buffer until the end of the function.
		 */
		escapeBuf = createPQExpBuffer();
		pszDBName = shellEscape(pszDBName, escapeBuf, true);
	}

	if (pszUserName == NULL)
		pszUserName = "";


	if (strstr(pszPassThroughParameters,"--rsyncable") != NULL)
	{
		elog(DEBUG1,"--rsyncable found, ptp %s",pszPassThroughParameters);
		/* Remove from gp_dump_agent parameters, because this parameter is for gzip */
		strncpy(strstr(pszPassThroughParameters,"--rsyncable"),"            ",strlen("--rsyncable"));
		elog(DEBUG1,"modified to ptp %s",pszPassThroughParameters);
		/* If a compression program is set, and doesn't already include the parameter */
		if (strlen(pszCompressionProgram) > 1 && strstr(pszCompressionProgram, "--rsyncable") == NULL)
		{
			/* add the parameter */
			/*
			 * You would think we'd add this to pszCompressionProgram, but actually
			 * testCommandExists() moved it to compPg, where we use it later.
			 *
			 * But, much of the code assumes that if pszCompressionProgram is set, we WILL have a compression program,
			 * and the code will fail if we don't, so why we do this is a mystery.
			 */
			char * newComp = palloc(strlen(compPg) + strlen(" --rsyncable") + 1);
			strcpy(newComp,compPg);
			strcat(newComp," --rsyncable");
			compPg = newComp;
			elog(DEBUG1,"compPG %s",compPg);
		}
	}


	/* If a compression program is set, and doesn't already include the parameter */
	if (strlen(pszCompressionProgram) > 1 && strstr(pszCompressionProgram, " -1") == NULL &&
			pszCompressionProgram[0] == 'g')
	{
		/* add the -1 parameter to gzip */
		/*
		 * You would think we'd add this to pszCompressionProgram, but actually
		 * testCommandExists() moved it to compPg, where we use it later.
		 *
		 * But, much of the code assumes that if pszCompressionProgram is set, we WILL have a compression program,
		 * and the code will fail if we don't, so why we do this is a mystery.
		 */
		char * newComp = palloc(strlen(compPg) + strlen(" -1") + 1);
		strcpy(newComp,compPg);
		strcat(newComp," -1");
		compPg = newComp;
		elog(DEBUG1,"new compPG %s",compPg);
	}


	/* Clear process interval timers */
	resetTimers(&savetimers);

	/* Child Process */
	port = PostPortNumber;

	pszThrottleCmd = &emptyStr;       /* We do this to point to prevent memory leak*/
	if (gp_backup_directIO)
	{
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

	initStringInfo(&pszCmdLine);

	appendStringInfo(&pszCmdLine, "%s", bkPg);
	
	if (is_old_format)
		appendStringInfo(&pszCmdLine, "--gp-k %s_%d_%d_%s",
						 pszBackupKey, (instid == -1) ? 1 : 0, segid, pszPassThroughCredentials);
	else
		appendStringInfo(&pszCmdLine, "--gp-k %s_%d_%d_%s",
						 pszBackupKey, instid, segid, pszPassThroughCredentials);

	appendStringInfo(&pszCmdLine, " --gp-d %s", pszBackupDirectory);

#ifdef USE_DDBOOST
	if (dd_boost_enabled)
		appendStringInfo(&pszCmdLine, " --dd_boost_file %s --dd_boost_dir %s --dd_boost_buf_size=%d",
						 pszDDBoostFileName, pszDDBoostDirName, ddboost_buf_size);
#endif

	appendStringInfo(&pszCmdLine, " -p %d -U %s %s",
					 port, pszUserName, pszPassThroughParameters);

	/*
	 * Dump schema and data for entry db, and data only for segment db.  Why
	 * dump data for entry db? Because that will include statements to update
	 * the sequence tables (last values). As for actual data - there isn't any,
	 * so we will just dump empty COPY statements...
	 *
	 * XXX: Should instid be checked for is_old_format here?
	 */
	if (instid == MASTER_CONTENT_ID)
		appendStringInfo(&pszCmdLine, " --pre-and-post-data-schema-only");
	else
		appendStringInfo(&pszCmdLine, " -a");

	appendStringInfo(&pszCmdLine, " %s 2> %s",
					 pszDBName, pszStatusFileName);

#ifdef USE_DDBOOST
	if (pszCompressionProgram[0] != '\0')
		appendStringInfo(&pszCmdLine, " | %s", compPg);
	if (dd_boost_enabled)
	{
		if (pszDDBoostFileName == NULL)
			elog(ERROR, "DDboost filename is NULL");

		appendStringInfo(&pszCmdLine, " | %s --write-file-from-stdin",
						 gpDDBoostPg);

		temp = pstrdup(pszPassThroughParameters);
		if ((pch = strstr(temp, "--dd_boost_dir")) != NULL)
		{
			pch += strlen("--dd_boost_dir");
			pchs = strtok(pch, " ");
			if (pchs)
				appendStringInfo(&pszCmdLine, " --to-file=%s/%s", pchs, pszDDBoostFileName);
			else
				appendStringInfo(&pszCmdLine, " --to-file=db_dumps/%s", pszDDBoostFileName);
		}

		appendStringInfo(&pszCmdLine, " --dd_boost_buf_size=%d", ddboost_buf_size);

		if ((pStu = strstr(temp, "--ddboost-storage-unit")) != NULL)
		{
			pStu += strlen("--ddboost-storage-unit");
			pStus = strtok(pStu, " ");
			if (pStus)
				appendStringInfo(&pszCmdLine, "--ddboost-storage-unit=%s", pStu);
		}

		pfree(temp);
	}
#endif

	if (netbackup_enabled)
	{
		if (pszCompressionProgram[0] != '\0')
			appendStringInfo(&pszCmdLine, " | %s -f", compPg);
	
		appendStringInfo(&pszCmdLine, " | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
						 gpNBUDumpPg, netbackup_service_host,
						 netbackup_policy, netbackup_schedule, pszBackupFileName);
		if (netbackup_block_size)
			appendStringInfo(&pszCmdLine, " --netbackup-block-size %s", netbackup_block_size);
		if (netbackup_keyword)
			appendStringInfo(&pszCmdLine, " --netbackup-keyword %s", netbackup_keyword);
	}
	else
	{
		if (pszCompressionProgram[0] != '\0')
			appendStringInfo(&pszCmdLine, " | %s", compPg);

		appendStringInfo(&pszCmdLine, "%s", pszThrottleCmd);
	}

	elog(LOG, "gp_dump_agent command line: %s", pszCmdLine.data);

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
				 errmsg("Could not fork a process for backup of database %s",
				 		pszDBName)));
	}
	else if (newpid == 0)
	{
	    /* This execs a shell that runs the gp_dump_agent program */
		execl("/bin/sh", "sh", "-c", pszCmdLine.data, NULL);

		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error in gp_backup_launch - execl of /bin/sh with Command Line \"%s\" failed",
						pszCmdLine.data)));
		_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
	}

	/*
	 * If we are the master, we do both the pre and post data schema dump
	 */
	if (instid == MASTER_CONTENT_ID)
	{
		int stat;

		pid_t w = waitpid(newpid, &stat, 0);
		if (w == -1)
		{
			elog(ERROR, "Dump agent failure: %m");
		}

		/* Delete the old command */
		pfree(pszThrottleCmd);
		pfree(pszBackupFileName);
	
		resetStringInfo(&pszCmdLine);

#ifdef USE_DDBOOST
		if (pszDDBoostFileName != NULL)
			pfree(pszDDBoostFileName);
#endif

		/* re-format dump file name and create temp file */
		pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_compress, true, NULL);
		char *pszTempBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, false, true, "_temp");

		if (gp_backup_directIO)
			pszThrottleCmd = formThrottleCmd(pszBackupFileName, gp_backup_directIO_read_chunk_mb, true);
		else
			pszThrottleCmd = formThrottleCmd(pszBackupFileName, 0, false);

		appendStringInfo(&pszCmdLine, "cat %s", pszTempBackupFileName);

		if (pszCompressionProgram[0] != '\0')
			appendStringInfo(&pszCmdLine, " | %s", compPg);

#ifdef USE_DDBOOST
		if (dd_boost_enabled)
		{
			pszDDBoostFileName = formDDBoostFileName(pszBackupKey, true, is_compress);
			appendStringInfo(&pszCmdLine, " | %s --write-file-from-stdin --to-file=%s/%s --dd_boost_buf_size=%d",
							 gpDDBoostPg, pszDDBoostDirName, pszDDBoostFileName, ddboost_buf_size);

			if (pszDDBoostStorageUnitName)
				appendStringInfo(&pszCmdLine, " --ddboost-storage-unit=%s", pszDDBoostStorageUnitName);
		}
#endif

		if (netbackup_enabled)
		{
			if (pszCompressionProgram[0] != '\0')
				appendStringInfo(&pszCmdLine, " -f");

			appendStringInfo(&pszCmdLine, " | %s --netbackup-service-host %s --netbackup-policy %s --netbackup-schedule %s --netbackup-filename %s",
							 gpNBUDumpPg, netbackup_service_host, netbackup_policy, netbackup_schedule, pszBackupFileName);

			if (netbackup_block_size)
				appendStringInfo(&pszCmdLine, " --netbackup-block-size %s", netbackup_block_size);

			if (netbackup_keyword)
				appendStringInfo(&pszCmdLine, " --netbackup-keyword %s", netbackup_keyword);
		}
		else
			appendStringInfo(&pszCmdLine, " %s", pszThrottleCmd);

		elog(LOG, "gp_dump_agent command line : %s", pszCmdLine.data);

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
			/* This execs a shell that runs the shell program to cat local post dump data to dump file*/
			execl("/bin/sh", "sh", "-c", pszCmdLine.data, NULL);

			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("Error in gp_backup_launch - execl of /bin/sh with Command Line \"%s\" failed",
							pszCmdLine.data)));
			_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
		}

		w = waitpid(newpid, &stat, 0);
		if (w == -1)
			elog(ERROR, "Dump agent failure from post schema dump: %m");

		if (remove(pszTempBackupFileName) != 0)
			elog(WARNING,
				 "Failed to remove temporary post schema dump file \"%s\": %m",
				 pszTempBackupFileName);
	}

	/* Restore process interval timers */
	restoreTimers(&savetimers);

	assert(pszSaveBackupfileName != NULL && pszSaveBackupfileName[0] != '\0');

	destroyPQExpBuffer(escapeBuf);

	return DirectFunctionCall1(textin, CStringGetDatum(pszSaveBackupfileName));
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
	int			target_dbid = 0; /* keep compiler quiet. value will get replaced */
	char	   *pszOnErrorStop;
	char	   *pszBackupFileName;
	char	   *pszStatusFileName;
	struct stat info;
	char	   *pszDBName;
	char	   *pszUserName;
	pid_t		newpid;
	StringInfoData pszCmdLine;
	int			port;
	int			len_name;
	bool		is_decompress;
	bool		is_file_compressed; /* is the dump file ending with '.gz'*/
	bool		postDataSchemaOnly;
	itimers 	savetimers;

	postDataSchemaOnly = false;

	initStringInfo(&pszCmdLine);

    /* TODO: refactor this piece of code, can easily make bugs with this */
	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (!PG_ARGISNULL(0))
	{
		pszBackupDirectory = GET_STR(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = GET_STR(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszCompressionProgram = GET_STR(PG_GETARG_TEXT_P(2));

	if (!PG_ARGISNULL(3))
		pszPassThroughParameters = GET_STR(PG_GETARG_TEXT_P(3));

	if (!PG_ARGISNULL(4))
		pszPassThroughCredentials = GET_STR(PG_GETARG_TEXT_P(4));

	if (!PG_ARGISNULL(5))
		pszPassThroughTargetInfo = GET_STR(PG_GETARG_TEXT_P(5));

	if (!PG_ARGISNULL(6))
		target_dbid = PG_GETARG_INT32(6);

	if (!PG_ARGISNULL(7))
		bOnErrorStop = PG_GETARG_BOOL(7);

	pszOnErrorStop = bOnErrorStop ? "--gp-e" : "";

    dump_prefix = parse_prefix_from_params(pszPassThroughParameters);
    statusDirectory = parse_status_from_params(pszPassThroughParameters);
	netbackup_service_host = parse_option_from_params(pszPassThroughParameters, "--netbackup-service-host");
	netbackup_block_size = parse_option_from_params(pszPassThroughParameters, "--netbackup-block-size");

	if (strstr(pszPassThroughParameters,"--old-format") != NULL)
		is_old_format = 1;

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);
    /* if statusDirectory is NULL, it defaults to SEGMENT DATA DIRECTORY */
	statusDirectory = relativeToAbsolute(statusDirectory);

	/* Validate existence of compression program and gp_restore_agent program */
	testProgramExists("gp_restore_agent", false);
	if (pszCompressionProgram)
		testProgramExists(pszCompressionProgram, false);

	/* are we going to use a decompression program? */
	is_decompress = (pszCompressionProgram != NULL && *pszCompressionProgram != '\0' ? true : false);

	/* Post data pass? */
	postDataSchemaOnly = strstr(pszPassThroughParameters, "--post-data-schema-only") != NULL;

	/* Form backup file path name */
	pszBackupFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, is_decompress, postDataSchemaOnly, NULL);
	pszStatusFileName = formStatusFilePathName(statusDirectory, pszBackupKey, false);

	if (strstr(pszPassThroughParameters,"--netbackup-service-host") != NULL)
	{
		netbackup_enabled = 1;
		gpNBUQueryPg = testProgramExists("gp_bsa_query_agent", false);
	}

#ifdef USE_DDBOOST
	if (strstr(pszPassThroughParameters,"--dd_boost_enabled") != NULL)
	{
		dd_boost_enabled = 1;

		/*
		 * When restoring from DDBoost to a fresh cluster, we will not
		 * have the db_dumps/<date> directory. This is required to
		 * store the status file. The fix is to create the specified
		 * backup directory for every ddboost restore. If a directory
		 * already exists, then no side effect is seen
		 */
		if (!createBackupDirectory(pszBackupDirectory))
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
			 		 errmsg("Creating backup directory \"%s\" failed",
					 pszBackupDirectory)));
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
				char *queriedNBUBackupFilePathName = queryNBUBackupFilePathName(netbackup_service_host, pszBackupFileName);

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
		 	 * If --gp-c is on make sure our file name has a .gz suffix.
		 	 * If no decompression requested make sure the file *doesn't* have
		 	 * a .gz suffix. This fixes the problem with forgetting to use
		 	 * the compression flags and gp_restore completing silently with
		 	 * no errors (hard to reproduce, but this check doesn't hurt)
		 	 */
			len_name = strlen(pszBackupFileName);
			is_file_compressed = (strcmp(pszBackupFileName + (len_name - 3), ".gz") == 0 ? true : false);

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

	len_name = strlen(pszBackupFileName);

	PQExpBuffer escapeBuf = NULL;
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
	{
		/*
		 * pszDBName will be pointing to the data portion of the PQExpBuffer
		 * once escpaped (escapeBuf->data), so we can't safely destroy the
		 * escapeBuf buffer until the end of the function.
		 */
		escapeBuf = createPQExpBuffer();
		pszDBName = shellEscape(pszDBName, escapeBuf, true);
	}

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
				 errmsg("Could not fork a process for backup of database %s",
				 		pszDBName)));
	}
	else if (newpid == 0)
	{
		/* Child Process */
		port = PostPortNumber;

		appendStringInfo(&pszCmdLine, "%s", bkPg);
		if (is_decompress)
			appendStringInfo(&pszCmdLine, " --gp-c %s", compPg);
	
		appendStringInfo(&pszCmdLine, " --gp-k %s_%d_%d_%s",
						 pszBackupKey, instid, segid, pszPassThroughCredentials);

#ifdef USE_DDBOOST
		if (dd_boost_enabled)
			appendStringInfo(&pszCmdLine, " --dd_boost_buf_size=%d", ddboost_buf_size);
#endif
		if (netbackup_enabled)
			appendStringInfo(&pszCmdLine, " --netbackup-service-host %s", netbackup_service_host);
	
		appendStringInfo(&pszCmdLine, " --gp-d %s %s -p %d -U %s",
						 pszBackupDirectory, pszOnErrorStop, port, pszUserName);
		appendStringInfo(&pszCmdLine, " %s %s",
						 pszPassThroughParameters, pszPassThroughTargetInfo);
		appendStringInfo(&pszCmdLine, " -d %s %s %s %s 2>&2",
						 pszDBName, pszBackupFileName,
						 postDataSchemaOnly ? "2>>" : "2>", pszStatusFileName);

		if (netbackup_enabled && netbackup_block_size)
			appendStringInfo(&pszCmdLine, " --netbackup-block-size %s", netbackup_block_size);

		elog(LOG, "gp_restore_agent command line: %s\n", pszCmdLine.data);

		/* This executes a shell that runs the gp_restore_agent program */
		execl("/bin/sh", "sh", "-c", pszCmdLine.data, NULL);

		/*
		 * If execl() returns it's considered a failure, on successful
		 * invocation it will never return thus we can unconditionally error
		 * our with ereport(ERROR..) here.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error in gp_restore_launch - execl of \"/bin/sh\" with Command Line %s failed",
						pszCmdLine.data)));
			_exit(EXIT_CODE_BACKUP_RESTORE_ERROR);
	}

	/* Restore process interval timers */
	restoreTimers(&savetimers);

	assert(pszBackupFileName != NULL && pszBackupFileName[0] != '\0');

	destroyPQExpBuffer(escapeBuf);

	return DirectFunctionCall1(textin, CStringGetDatum(pszBackupFileName));
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
		pszBackupDirectory = GET_STR(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = GET_STR(PG_GETARG_TEXT_P(1));

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
			 * We can't support this without making a change to this function
			 * arguments, which will require an initdb, which we want to avoid.
			 * This is due to the fact that in this formBackupFilePathName call
			 * we don't know if the file is compressed or not and
			 * formBackupFilePathName will not know if to create a file with a
			 * compression suffix or not
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
					 errmsg("Invalid filetype %d passed to gp_read_backup_file",
					 		fileType)));
			break;
	}

	/* Make sure pszFileName exists */
	if (0 != stat(pszFileName, &info))
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Backup File %s Type %d could not be found",
				 		pszFileName, fileType)));
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

	f = fopen(pszFileName, "r");
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

	fclose(f);
	f = NULL;
	pszFullStatus[info.st_size] = '\0';

	return DirectFunctionCall1(textin, CStringGetDatum(positionToError(pszFullStatus)));
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
		pszBackupDirectory = GET_STR(PG_GETARG_TEXT_P(0));
		if (*pszBackupDirectory == '\0')
			pszBackupDirectory = "./";
	}

	if (!PG_ARGISNULL(1))
		pszBackupKey = GET_STR(PG_GETARG_TEXT_P(1));

	if (!PG_ARGISNULL(2))
		pszBackup = GET_STR(PG_GETARG_TEXT_P(2));

	/*
	 * if BackupDirectory is relative, make it absolute based on the directory
	 * where the database resides
	 */
	pszBackupDirectory = relativeToAbsolute(pszBackupDirectory);

	/*
	 * See whether the directory exists.
	 * If not, try to create it.
	 * If so, make sure it's a directory
	 */
	validateBackupDirectory(pszBackupDirectory);

	/* Form backup file path name */
	pszFileName = formBackupFilePathName(pszBackupDirectory, pszBackupKey, false, false, NULL);

	/* Write file */
	f = fopen(pszFileName, "w");
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

	fclose(f);
	f = NULL;

	assert(pszFileName != NULL && pszFileName[0] != '\0');

	return DirectFunctionCall1(textin, CStringGetDatum(pszFileName));
}

/*
 * createBackupDirectory
 *
 * This will create a directory if it doesn't exist.
 *
 * This routine first attempts to create the directory using the full path (in
 * case the mkdir function is able to create the full directory).  If this
 * mkdir fails with EEXIST, filesystem object is checked to ensure it is a
 * directory and fails with ENOTDIR if it is not.
 *
 * If the "full" mkdir fails with a code other than ENOENT, this routine fails.
 * For the ENOENT code, the routine attempts to create the directory tree from
 * the top, one level at a time, failing if unable to create a directory with a
 * code other than EEXIST and the EEXIST level is not a directory.  Being
 * tolerant of EEXIST failures allows for multiple segments to attempt to
 * create the directory simultaneously.
 */
bool
createBackupDirectory(char *pszPathName)
{
	int			rc;
	struct stat info;
	char	   *pSlash;

	/* Can we make it in its entirety?	If so, return */
	if (mkdir(pszPathName, S_IRWXU) == 0)
		return true;

	if (errno == ENOENT)
	{
		/* Try to create each level of the hierarchy that doesn't already exist */
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
				 * If failed and the directory level does not exist, fail the call.
				 * Is it possible for a directory level to exist and the mkdir() call
				 * fail with a code such as ENOSYS (and possibly others) instead of
				 * EEXIST.  So, if mkdir() fails, the directory level's existence is
				 * checked.  If the level does't exist or isn't a directory, the
				 * call is failed.
				 */
				if (rc != 0)
				{
					struct stat statbuf;
					int errsave = errno;	/* Save mkdir error code over stat() call. */

					if ( stat(pszPathName, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode) )
					{
						errno = errsave;		/* Restore mkdir error code. */
						*pSlash = '/';			/* Put back the slash we overwrote above. */
						return false;
					}
				}

				/* Put back the slash we overwrote above. */
				*pSlash = '/';

				/*
				 * At this point, this level exists -- either because it was created
				 * or it already exists.  If it already exists, it *could* be a
				 * non-directory.  This will be caught when attempting to create the
				 * next level.
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

	/* At this point, the path has been created or otherwise exists.  Ensure it's a directory. */
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

/*
 * findAcceptableBackupFilePathName
 *
 * This function takes the directory and timestamp key and finds an existing
 * file that matches the naming convention for a backup file with the proper
 * key and either the proper instid with any segid, or the proper segid with
 * any instid, to accommodate both filename formats in use.  In case it was
 * backed up from a redundant instance of the same segment.
 */
char *
findAcceptableBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, int instid, int segid)
{
	/* Make sure that pszBackupDirectory exists and is in fact a directory. */
	struct stat buf;
	struct dirent *dirp = NULL;
	DIR		   *dp = NULL;
	char	   *pszBackupFilePathName = NULL;
	/* "Old" and "New" refer  to old and new dump file name formats */
	static regex_t rFindFileNameNew, rFindFileNameOld, rFindFileNameMaster;
	static bool bFirstTime = true;
	char	   *pszRegexNew = NULL, *pszRegexOld = NULL, *pszRegexMaster = NULL;
	char	   *pszFileName;
	char	   *pszSep;
	struct stat info;

	if (bFirstTime)
	{
		int base_len = 64;
		int	wmasklenNew, wmasklenOld, wmasklenMaster,
			masklenNew, masklenOld, masklenMaster,
			regex_len = base_len + strlen(DUMP_PREFIX) + strlen(pszBackupKey);
		pg_wchar   *maskNew, *maskOld, *maskMaster;

		pszRegexNew = (char *) palloc(regex_len + 2);
		pszRegexOld = (char *) palloc(regex_len);
		pszRegexMaster = (char *) palloc(regex_len + 2);

	if (segid == 1)
	{
		/* 
		 * Matches all master filenames. These are in the format gp_dump_1_1 or
		 * gp_dump_-1_1
		 */
		snprintf(pszRegexMaster, regex_len, "^%sgp_dump_-?1_1_%s(.gz)?$", DUMP_PREFIX, pszBackupKey);

		masklenMaster = strlen(pszRegexMaster);
		maskMaster = (pg_wchar *) palloc((masklenMaster+ 1) * sizeof(pg_wchar));
		wmasklenMaster= pg_mb2wchar_with_len(pszRegexMaster, maskMaster, masklenMaster);

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
		 * Matches segment filenames. These are in the format gp_dump_0_<dbid>
		 * or gp_dump_<contentid>_<dbid> This regex matches dump files with
		 * dbid of any number except 1 to avoid matching master dumps
		 */
		snprintf(pszRegexNew, regex_len, "^%sgp_dump_%d_([02-9][0-9]*|1[0-9]+)_%s(.gz)?$", DUMP_PREFIX, instid, pszBackupKey);
		snprintf(pszRegexOld, regex_len, "^%sgp_dump_0_%d_%s(.gz)?$", DUMP_PREFIX, segid, pszBackupKey);

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
	pfree(pszRegexNew);
	pfree(pszRegexOld);
	pfree(pszRegexMaster);
	closedir(dp);

	return pszBackupFilePathName;
}

/*
 * formBackupFilePathName
 *
 * This function takes the directory and timestamp key and creates the path and
 * filename of the backup output file based on the naming convention for this.
 */
char *
formBackupFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool is_compress, bool isPostData, const char *suffix)
{
	int				instid;			/* dispatch node */
	int				segid;
	StringInfoData	pszBackupFilename;

	initStringInfo(&pszBackupFilename);

	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
		instid = (instid == -1) ? 1 : 0;

	appendStringInfo(&pszBackupFilename, "%s/",  pszBackupDirectory);
	appendStringInfo(&pszBackupFilename, "%sgp_dump_%d_%d_", DUMP_PREFIX, instid, segid);
	appendStringInfo(&pszBackupFilename, "%s", pszBackupKey);

	if (isPostData)
		appendStringInfo(&pszBackupFilename, "_post_data");

	if (is_compress)
		appendStringInfo(&pszBackupFilename, ".gz");
	
	if (suffix)
		appendStringInfo(&pszBackupFilename, "%s", suffix);

	if (strlen(pszBackupFilename.data) > MAXPGPATH)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Backup FileName based on path %s and key %s too long",
				 		pszBackupDirectory, pszBackupKey)));

	return pszBackupFilename.data;
}

/*
 * formStatusFilePathName
 *
 * This function takes the directory and timestamp key and IsBackup flag and
 * creates the path and filename of the backup or restore status file based on
 * the naming convention for this.
 */
char *
formStatusFilePathName(char *pszBackupDirectory, char *pszBackupKey, bool bIsBackup)
{
	int				instid;			/* dispatch node */
	int				segid;
	StringInfoData	pszFileName;

	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
		instid = (instid == -1) ? 1 : 0;

	appendStringInfo(&pszFileName, "%s/", pszBackupDirectory);
	appendStringInfo(&pszFileName, "%sgp_%s_status_%d_%d_",
					 DUMP_PREFIX, (bIsBackup ? "dump" : "restore"), instid, segid);
	appendStringInfo(&pszFileName, "%s", pszBackupKey);

	if (strlen(pszFileName.data) > MAXPGPATH)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Status FileName based on path %s and key %s too long",
				 		pszBackupDirectory, pszBackupKey)));

	return pszFileName.data;
}

/*
 * This function will query a NetBackup server to find out if a given filename
 * has been backed up using NetBackup. If the query returns NULL, it implies
 * that the given file was never backed up using that NetBackup server. If the
 * query returns the file path, it implies that the given file has been backed
 * up using the given NetBackup server and it can be restored from that server.
 */
static char *
queryNBUBackupFilePathName(char *netbackupServiceHost, char *netbackupRestoreFileName)
{
	FILE		   *fp;
	char			resBuff[MAXPGPATH];
	StringInfoData	cmd;

	memset(resBuff, 0, MAXPGPATH);
	initStringInfo(&cmd);
	
	appendStringInfo(&cmd, "%s ", gpNBUQueryPg);
	appendStringInfo(&cmd, " --netbackup-service-host %s ", netbackupServiceHost);
	appendStringInfo(&cmd, " --netbackup-filename %s\n", netbackupRestoreFileName);

	elog(LOG, "NetBackup query restore filename command line: %s", cmd.data);

	fp = popen(cmd.data, "r");
	pfree(cmd.data);

	if (fp == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error querying restore filename to NetBackup server"),
				 errhint("NetBackup server not running or restore file: "
				 		 "\"%s\" has not been backed up to the NetBackup server",
						 netbackupRestoreFileName)));

	fgets(resBuff, MAXPGPATH, fp);

	if (pclose(fp) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("Error querying restore filename to NetBackup server"),
				 errhint("NetBackup server not running or restore file: "
				 		 "\"%s\" has not been backed up to the NetBackup server",
						 netbackupRestoreFileName)));

	if (resBuff)
		return pstrdup(resBuff);

	return NULL;
}

/*
 * formThrottleCmd
 *
 * This function takes the backup file name and creates the throttling command
 */
char *
formThrottleCmd(char *pszBackupFileName, int directIO_read_chunk_mb, bool bIsThrottlingEnabled)
{
	StringInfoData	pszThrottleCmd;
	char		   *prog;

	initStringInfo(&pszThrottleCmd);

	if (bIsThrottlingEnabled)
	{
		prog = testProgramExists("throttlingD.py", false);
		appendStringInfo(&pszThrottleCmd, "| %s %d %s",
						 prog, directIO_read_chunk_mb, pszBackupFileName);
		pfree(prog);
	}
	else
		appendStringInfo(&pszThrottleCmd, "> %s", pszBackupFileName);
	
	return pszThrottleCmd.data;
}

/*
 * relativeToAbsolute
 *
 * This will turn a relative path into an absolute one, based off of the Data
 * Directory for this database.
 *
 * If the path isn't absolute, it is prefixed with DataDir/.
 */
char *
relativeToAbsolute(char *pszRelativeDirectory)
{
	char	   *pszAbsolutePath;

	if (pszRelativeDirectory != NULL && pszRelativeDirectory[0] == '/')
		return pszRelativeDirectory;

    if (pszRelativeDirectory != NULL)
    {
	    pszAbsolutePath = (char *) palloc(strlen(DataDir) + 1 + strlen(pszRelativeDirectory) + 1);
	    sprintf(pszAbsolutePath, "%s/%s", DataDir, pszRelativeDirectory);
    }
    else
    {
        return DataDir;
    }

	return pszAbsolutePath;
}

static char *
testProgramExists(char *pszProgramName, bool allowMissing)
{
	char	temp[MAXPGPATH];
	char	out[MAXPGPATH] = {'\0'};
	char   *p;
	char   *sep;

	if (pszProgramName)
	{
		strlcpy(temp, pszProgramName, MAXPGPATH);
		p = temp;

		while (*p != '\0')
		{
			if (*p == ' ' || *p == '\t' || *p == '\n')
			{
				*p = '\0';
				break;
			}
			p++;
		}

		sep = last_dir_separator(pszProgramName);
	
		if (find_other_exec(p, (sep ? sep : p), NULL, out) == 0)
			return pstrdup(out);
	}

	if (!allowMissing)
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("\"%s\" not found in in PGPATH or PATH",
				 sep ? sep : p)));

	return NULL;
}

/*
 * validateBackupDirectory
 *
 * This sees whether the backupDirectory exists.  If so, it makes sure its a
 * directory and not a file.  If not, it tries to create it.  If it cannot it
 * calls ereport
 */
void
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
 * positionToError
 *		Given a char buffer, position to the first error msg in it.
 *
 * We are only interested in ERROR, therefore, search for the first ERROR and
 * return the rest of the status file starting from that point. If no error
 * found, return a "no error found" string. We are only interested in
 * occurrences of "ERROR:" (from the backend), or [ERROR] (from dump/restore
 * log). others, like "ON_ERROR_STOP" should be ignored.
 */
static char *
positionToError(char *source)
{
	char	*sourceCopy = source;
	char	*firstErr = NULL;
	char	*defaultNoErr = "No extra error information available. Please examine status file.";

	while(true)
	{
		firstErr = strstr((const char*)sourceCopy, "ERROR");

		if (!firstErr)
		{
			break;
		}
		else
		{
			/* found "ERROR". only done if it's "ERROR:" or "[ERROR]" */
			if (firstErr[5] == ']' || firstErr[5] == ':')
			{
				break; /* done */
			}
			else
			{
				sourceCopy = ++firstErr; /* start from stopping point */
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
		char *p;

		for (p = firstErr ; p > source ; p--)
		{
			if (*p == '\n')
			{
				p++; /* skip the LF */
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
	int				instid; /* dispatch node */
	int				segid;
	StringInfoData	pszBackupFileName;

	verifyGpIdentityIsSet();
	instid = GpIdentity.segindex;
	segid = GpIdentity.dbid;

	if (is_old_format)
	   instid = (instid == -1) ? 1 : 0;   /* dispatch node */

	appendStringInfo(&pszBackupFileName, "%sgp_dump_%d_%d_%s",
					 DUMP_PREFIX, instid, segid, pszBackupKey);
	if (isPostData)
		appendStringInfo(&pszBackupFileName, "_post_data");

	if (isCompress)
		appendStringInfo(&pszBackupFileName, ".gz");

	return pszBackupFileName.data;
}
#endif


/*
 * shellEscape: Returns a string in which the shell-significant quoted-string characters are
 * escaped.
 *
 * This function escapes the following characters: '"', '$', '`', '\', '!', '''.
 *
 * The StringInfo escapeBuf is used for assembling the escaped string and is reset at the
 * start of this function.
 */
char *
shellEscape(const char *shellArg, PQExpBuffer escapeBuf, bool addQuote)
{
	const char *s = shellArg;
	const char	escape = '\\';

	resetPQExpBuffer(escapeBuf);

	if (addQuote)
		appendPQExpBufferChar(escapeBuf, '\"');
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
                                appendPQExpBufferChar(escapeBuf, escape);
                }
                appendPQExpBufferChar(escapeBuf, *s);
                s++;
        }

	if (addQuote)
		appendPQExpBufferChar(escapeBuf, '\"');

	return escapeBuf->data;
}
