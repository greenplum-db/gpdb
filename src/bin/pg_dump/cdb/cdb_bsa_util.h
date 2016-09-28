#include "pg_config.h"

#ifdef USE_NETBACKUP

#include "xbsa.h"
#include "nbbsa.h"

#define NBU_MAX_LABEL_LEN 20
#define NBU_MAX_PARAM_LEN 127
/* We add 2 to account for an '=' and the null terminator */
#define NBU_BUFFER_SIZE (1 + NBU_MAX_LABEL_LEN + 1 + NBU_MAX_PARAM_LEN)

/* Dump related functions to backup data to NetBackup */
extern int initBSADumpSession(char *bsaServiceHost, char *nbbsaPolicy, char *nbbsaSchedule, char *nbbsaKeyword);
extern int createBSADumpObject(char *BackupFilePathName);
extern int sendBSADumpData(int NetBackupBlockSize);
extern int endBSADumpSession(void);

/* Restore related functions to restore data from NetBackup */
extern int initBSARestoreSession(char *bsaServiceHost);
extern int getBSARestoreObject(char *BackupFilePathName);
extern int getBSARestoreData(int NetBackupBlockSize);
extern int endBSARestoreSession(void);

/* Query related functions to query objects to NetBackup */
extern char *queryBSAObject(char *BackupFilePathName);
extern int searchBSAObject(char *NetBackupQueryObject);

/* Cleanup related function to delete objects on NetBackup server */
extern int deleteBSAObjects(char *NetBackupDeleteObject);

/* Helper function to validate parameters */
extern int setNetbackupParam(char **envxPtr, char *label, char *nbbsaParam);


#endif
