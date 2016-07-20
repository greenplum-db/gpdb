/*-------------------------------------------------------------------------
 *
 * cdb_dump_util.h
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDB_DUMP_UTIL_H
#define CDB_DUMP_UTIL_H

#include <regex.h>
#include "cdb_seginst.h"
#include "libpq-int.h"

#define CDB_BACKUP_KEY_LEN 14

#ifdef USE_DDBOOST
#include <limits.h>
#include "ddp_api.h"
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define DDBOOST_CONFIG_FILE ".ddconfig"
#define DDBOOST_USERNAME_MAXLENGTH 30
/*
 * NOTE: If you increase this, you must also increase OBFUSCATE_PAYLOAD_LENGTH
 * in cdb_lockbox.c!
 */
#define DDBOOST_PASSWORD_MAXLENGTH 40
#define DDBOOST_LOG_NUM_OF_FILES 10
#ifndef DDBOOST_POOL_SIZE
#define DDBOOST_POOL_SIZE (32 * 1024 * 2048)
#endif

extern int getDDBoostCredential(char** hostname, char** user, char** password, char** log_level ,char** log_size, char **default_backup_directory, char **ddboost_storage_unit, bool remote);
extern int  setDDBoostCredential(char *hostname, char *user, char *password, char *log_level ,char *log_size, char *default_backup_directory, char *ddboost_storage_unit,  bool remote);
extern void rotate_dd_logs(const char *file_name, unsigned int num_of_files, unsigned int log_size);
extern void _ddp_test_log(const void *session_ptr, const ddp_char_t *log_msg, ddp_severity_t severity);
extern int initDDSystem(ddp_inst_desc_t *ddp_inst, ddp_conn_desc_t *ddp_conn, ddp_client_info_t *cl_info,
                        char **ddboost_storage_unit, bool createStorageUnit, char **default_backup_directory, bool remote);
#endif

/* --------------------------------------------------------------------------------------------------
 * This needs to be the same as that in cdbbackup.h in src/backend/cdb/cdbbackup.c
 */
typedef enum backup_file_type
{
	BFT_BACKUP = 0,
	BFT_BACKUP_STATUS = 1,
	BFT_RESTORE_STATUS = 2
} BackupFileType;

/* --------------------------------------------------------------------------------------------------
 * Structure for holding the command line options for both backup and restore
 */
typedef struct input_options
{
	char	   *pszDBName;
	char	   *pszPGHost;
	char	   *pszPGPort;
	char	   *pszUserName;
	char	   *pszBackupDirectory;
	char	   *pszReportDirectory;
	char	   *pszCompressionProgram;
	char	   *pszPassThroughParms;
	char	   *pszCmdLineParms;
	char	   *pszKey;
	char	   *pszMasterDBName;
	char	   *pszRawDumpSet;
	char	   *pszTimestampKey;
	ActorSet	actors;
	BackupLoc	backupLocation;
	bool		bOnErrorStop;
}	InputOptions;

extern bool shouldDumpSchemaOnly(int g_role, bool incrementalBackup, void *list);

/* issues either a listen or notify command on connection pConn */
extern void DoCancelNotifyListen(PGconn *pConn, bool bListen,
					 const char *pszCDBDumpKey,
					 int CDBInstID,
					 int CDBSegID,
					 int target_db_id,
					 const char *pszSuffix);

/* Checks if we should filter out tables when using incremental mode */
extern bool isFilteringAllowedNow(int role, bool incrementalBackup, char *incrementalFilter);

/* frees data allocated inside an InputOptions struct */
extern void FreeInputOptions(InputOptions * pInputOpts);

/* Generates a 14 digit timestamp key */
extern char *GenerateTimestampKey(void);

/* Returns the 14 digit timestamp key if provided using gp-k or generates it using GenerateTimestampKey method */
extern char *GetTimestampKey(char*);

/* Validates a 14 digit timestamp key */
extern bool ValidateTimestampKey(char*);

/* Determine if we should expand a parent partition to its children */
extern bool shouldExpandChildren(bool, bool);

/* gets an int from a regex match */
extern int	GetMatchInt(regmatch_t *pMatch, char *pszInput);

/* gets an stringt from a regex match */
extern char *GetMatchString(regmatch_t *pMatch, char *pszInput);

/* creates a formatted time string with the current time */
extern char *GetTimeNow(char *szTimeNow);

/* creates a connection where the password may be prompted for */
extern PGconn *GetMasterConnection(const char *progName, const char *pszDBName, const char *pszPGHost,
					const char *pszPGPort, const char *pszUserName,
					int reqPwd, int ignoreVersion, bool bDispatch);

/* creates a connection based on SegmentInstance* parameter */
extern PGconn *MakeDBConnection(const SegmentDatabase *pSegDB, bool bDispatch);

/* returns a formatted char * */
extern char *MakeString(const char *fmt,...);

/* breaks the input parameter associated with --cdb-k into its components for cdb_dump and cdb_restore*/
extern bool ParseCDBDumpInfo(const char *progName, char *pszCDBDumpInfo, char **ppCDBDumpKey, int *pCDBInstID, int *pCDBSegID, char **ppCDBPassThroughCredentials);

/* reads the contents for "ERROR:" and "[ERROR]" out of the appropriate file on the database server */
extern int ReadBackendBackupFileError(PGconn *pConn, const char *pszBackupDirectory, const char *pszKey,
		                                        BackupFileType fileType, const char *progName, PQExpBuffer buf);

/* returns strdup if not NULL, NULL otherwise */
extern char *Safe_strdup(const char *s);

/* returns the string if not null, otherwise the default */
extern const char *StringNotNull(const char *pszInput, const char *pszDefault);

/* writes a formatted message to stderr */
extern void mpp_err_msg(const char *loglevel, const char *prog, const char *fmt,...);

/* writes a formatted message to stderr and caches it in a static var for later use */
extern void mpp_err_msg_cache(const char *loglevel, const char *prog, const char *fmt,...);

/* writes a formatted message to stdout */
extern void mpp_msg(const char *loglevel, const char *prog, const char *fmt,...);

/* writes the contents to the appropriate file on the database server */
/*extern char* WriteBackendBackupFile( PGconn* pConn, const char* pszBackupDirectory, const char* pszKey, const char* pszBackupScript );*/
/* writes a status message to a status file with naming convention based on instid, segid, and Key */
/* extern bool WriteStatusToFile( const char* pszMessage, const char* pszDirectory, const char* pszKey, int instid, int segid, bool bIsBackup ); */

extern char *get_early_error(void);

/* return report text for type of backup */
extern const char* getBackupTypeString(bool incremental);


/* Base64 Encoding and Decoding Routines */
/* Base64 Data is assumed to be in a NULL terminated string */
/* Data is just assumed to be an array of chars, with a length */
extern char *DataToBase64(const char *pszIn, unsigned int InLen);
extern char *Base64ToData(const char *pszIn, unsigned int *pOutLen);
extern char *nextToken(register char **stringp, register const char *delim);
extern int	parseDbidSet(int *dbidset, char *dump_set);
extern char* formCompressionProgramString(char* compPg);
extern void formDDBoostPsqlCommandLine(char** retVal, bool compUsed, const char* ddboostPg, const char* compProg,
							const char* ddp_file_name, const char* dd_boost_buf_size,
							const char* filter_script, const char* table_filter_file,
							int role, const char* psqlPg, bool postSchemaOnly,
							const char* change_schema_file, const char *schema_level_file,
							const char* ddboost_storage_unit);

extern void formSegmentPsqlCommandLine(char** retVal, const char* inputFileSpec,
						bool compUsed, const char* compProg, const char* filter_script,
						const char* table_filter_file, int role, const char* psqlPg, const char* catPg,
                        const char* gpNBURestorePg, const char* netbackupServiceHost, const char* netbackupBlockSize,
						const char* change_schema, const char* schema_level_file);

extern void formPostDataSchemaOnlyPsqlCommandLine(char** retVal, const char* inputFileSpec,
						bool compUsed, const char* compProg, const char* post_data_filter_script,
                        const char* table_filter_file, const char* psqlPg, const char* catPg,
                        const char* gpNBURestorePg, const char* netbackupServiceHost, const char* netbackupBlockSize,
			const char* change_schema_file, const char *schema_level_file);

extern void formFilterCommandLine(char** retVal, const char* filter_script, const char* table_filter_file,
				int role, const char* change_schema_file, const char *schema_level_file);

extern void formPostDataFilterCommandLine(char** retVal, const char* post_data_filter_script, const char* table_filter_file,
					const char* change_schema_file, const char *schema_level_file);

/* prototypes for hash table */

void cleanUpTable(void);

int initializeHashTable(int num_elems);

int insertIntoHashTable(Oid o, char t);

int hashFunc(Oid k);

char getTypstorage(Oid o);

int removeNode(Oid o);

extern char* shellEscape(const char *shellArg, PQExpBuffer escapeBuf);
#endif   /* CDB_DUMP_UTIL_H */
