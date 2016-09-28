#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "pg_config.h"
#include "fe-auth.h"
#include "cdb_dump_util.h"

#ifdef USE_NETBACKUP

#include "cdb_bsa_util.h"

/************* BSA variables ************/
BSA_Handle				BsaHandle;
BSA_ObjectOwner			BsaObjectOwner;
BSA_SecurityToken		*security_tokenPtr;
BSA_UInt32              EnvBufSz = 512;
BSA_DataBlock32			*data_block;
BSA_ObjectDescriptor	*object_desc;
BSA_QueryDescriptor     *query_desc;
BSA_UInt32				ErrStrSize;
char					*envx[6];
char					*gpBsaBuf = NULL;
char                    EnvBuf[512];
char					ErrorString[512];
char					msg[1024];
int						status;
char					*restore_location;
int						total_bytes = 0;
int 					i = 0;

/* Dump related functions used to backup data to NetBackup */

int setNetbackupParam(char **envxPtr, char *label, char *nbbsaParam)
{
	if(nbbsaParam)
	{
		if (strlen(nbbsaParam) > NBU_MAX_PARAM_LEN)
		{
			mpp_err_msg("ERROR", "gp_bsa_dump_agent", "%s provided has more than max limit (%d) characters. Cannot proceed with backup. \n", label, NBU_MAX_PARAM_LEN);
			return -1;
		}
		int retVal = snprintf(*envxPtr, NBU_BUFFER_SIZE, "%s=%s", label, nbbsaParam);
		if (retVal > NBU_BUFFER_SIZE || retVal < 0)
		{
			return -1;
		}
	}
	else
	{
		*envxPtr = NULL;
	}
	return 0;
}

int initBSADumpSession(char *bsaServiceHost, char *nbbsaPolicy, char *nbbsaSchedule, char *nbbsaKeyword)
{

	/* Allocate memory for the XBSA environment variable array. */
	for(i = 0; i<5; i++){
		envx[i] = malloc(NBU_BUFFER_SIZE);
		if(envx[i] == NULL){
			mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to allocate memory for NetBackup BSA enviroment variables\n");
			return -1;
		}
		memset(envx[i], 0x00, NBU_BUFFER_SIZE);
	}

	/* Populate the XBSA environment variables for this session. */
	if (setNetbackupParam(&envx[1], "BSA_SERVICE_HOST", bsaServiceHost) == -1
		|| setNetbackupParam(&envx[2], "NBBSA_POLICY", nbbsaPolicy) == -1
		|| setNetbackupParam(&envx[3], "NBBSA_SCHEDULE", nbbsaSchedule) == -1
		|| setNetbackupParam(&envx[4], "NBBSA_KEYWORD",  nbbsaKeyword) == -1)
	{
		return -1;
	}

	strncpy(envx[0], "BSA_API_VERSION=1.1.0", (1 + strlen("BSA_API_VERSION=1.1.0")));

	envx[5] = NULL;

	/* The NetBackup XBSA Interface does not use the security token. */
	security_tokenPtr = NULL;

	/* Populate the object owner structure. */
	strncpy(BsaObjectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(BsaObjectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));

	/* Initialize an XBSA session. */
	status = BSAInit(&BsaHandle, security_tokenPtr, &BsaObjectOwner, envx);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSAInit() failed with error: %s\n", ErrorString);
		return -1;
	}

	/* Begin a backup transaction.  If the transaction cannot be opened, *
	 * terminate the session.                                            */
	status = BSABeginTxn(BsaHandle);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSABeginTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSABeginTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Backup");
		BSATerminate(BsaHandle);
		return -1;
	}

	/* Initialize the BSA_DataBlock32 structure.  */
	data_block = (BSA_DataBlock32 *)malloc(sizeof(BSA_DataBlock32));
	if(data_block == NULL){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to allocate memory for NetBackup BSA data block to perform Backup\n");
		return -1;
	}
	memset(data_block, 0x00, sizeof(BSA_DataBlock32));

	return 0;
}

int createBSADumpObject(char *BackupFilePathName)
{
	/* Populate the object descriptor of the first object to be backed up. */
	object_desc = (BSA_ObjectDescriptor *)malloc(sizeof(BSA_ObjectDescriptor));
	if(object_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to allocate memory for NetBackup BSA object descriptor to perform Backup\n");
		return -1;
	}
	memset(object_desc, 0x00, sizeof(BSA_ObjectDescriptor));
	strncpy(object_desc->objectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(object_desc->objectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));
	strncpy(object_desc->objectName.pathName, BackupFilePathName, (1 + strlen(BackupFilePathName)));
	strncpy(object_desc->objectName.objectSpaceName, "", (1 + strlen("")));
	strncpy(object_desc->resourceType, "Dump Object", (1 + strlen("Dump Object")));
	strncpy(object_desc->objectDescription,"A dump object contains a fixed sized portion of the dump data", (1 + strlen("A dump object contains a fixed sized portion of the dump data")));
	object_desc->copyType = BSA_CopyType_BACKUP;
	object_desc->estimatedSize.left = 0;
	object_desc->estimatedSize.right = 100;
	object_desc->objectType = BSA_ObjectType_FILE;

	/* Create the sample object. If the object  cannot be created,    *
	 * terminate the session.                                         */
	status = BSACreateObject(BsaHandle, object_desc, data_block);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSACreateObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSACreateObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Backup");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}
	return 0;
}

int sendBSADumpData(int NetBackupBlockSize)
{
	int read_res  = 0;
	gpBsaBuf = (char *)malloc(NetBackupBlockSize * sizeof(char));
	if(gpBsaBuf == NULL){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to allocate memory for NetBackup data transfer buffer to perform Backup\n");
		return -1;
	}
	memset(gpBsaBuf, 0x00, NetBackupBlockSize);

	/* We try to read the data to be dumped in chunks of 'NetBackupBlockSize' size 
	and send each chunk to the NetBackup server to be backed up */
	while((read_res = fread(gpBsaBuf, 1, NetBackupBlockSize, stdin)) > 0)
	{
		data_block->bufferLen = NetBackupBlockSize;
		data_block->bufferPtr = gpBsaBuf;
		data_block->numBytes  = read_res;

		status = BSASendData(BsaHandle, data_block);
		if (status != BSA_RC_SUCCESS) {
			ErrStrSize = 512;
			NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
			snprintf(msg, sizeof(msg), "ERROR: BSASendData() failed with error: %s\n", ErrorString);
			mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSASendData() failed with error: %s\n", ErrorString);
			NBBSALogMsg(BsaHandle, MSERROR, msg, "Backup");
			BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
			BSATerminate(BsaHandle);
			return -1;
		}

		if(read_res < NetBackupBlockSize){
			if(feof(stdin)){
				break;
			}
			else if(ferror(stdin)){
				mpp_err_msg("ERROR", "gp_bsa_dump_agent", "The following error occurred while reading data to be backed up by NetBackup\n");
				perror("gp_bsa_dump_agent");
				return -1;
			}
		}

	}

	return 0;
}

int endBSADumpSession()
{
	status = BSAEndData(BsaHandle);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndData() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSAEndData() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Backup");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}


	/* End the backup transaction and commit the object.  */
	status = BSAEndTxn(BsaHandle, BSA_Vote_COMMIT);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "BSAEndTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Backup");
		BSATerminate(BsaHandle);
		return -1;
	}

	/* End the XBSA session  */
	BSATerminate(BsaHandle);

	free(gpBsaBuf);

	return 0;
}

/* Restore related BSA functions used to restore data from NetBackup */

int initBSARestoreSession(char *bsaServiceHost)
{
	/* Allocate memory for the XBSA environment variable array. */
	for(i=0; i<2; i++){
		envx[i] = malloc(NBU_BUFFER_SIZE);
		if(envx[i] == NULL){
			mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA environment variables\n");
			return -1;
		}
		memset(envx[i], 0x00, NBU_BUFFER_SIZE);
	}

	/* Populate the XBSA environment variables for this session. */
	if (setNetbackupParam(&envx[1], "BSA_SERVICE_HOST", bsaServiceHost) == -1)
	{
		return -1;
	}

	strncpy(envx[0], "BSA_API_VERSION=1.1.0", (1 + strlen("BSA_API_VERSION=1.1.0")));
	envx[2] = NULL;

	/* The NetBackup XBSA Interface does not use the security token. */
	security_tokenPtr = NULL;

	/* Populate the object owner structure. */
	strncpy(BsaObjectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(BsaObjectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));

	/* Initialize an XBSA session. */
	status = BSAInit(&BsaHandle, security_tokenPtr, &BsaObjectOwner, envx);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAInit() failed with error: %s\n", ErrorString);
		return -1;
	}

	/* Begin a restore transaction.  If the transaction cannot be opened, *
	 * terminate the session.                                            */
	status = BSABeginTxn(BsaHandle);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSABeginTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSABeginTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSATerminate(BsaHandle);
		return -1;
	}

	return 0;
}

int getBSARestoreObject(char *BackupFilePathName)
{
	/* Populate the query descriptor of the object to be restored.  The query step  *
	 * can be skipped if the XBSA application keeps a catalog of the copyId's of   *
	 * the objects which have been backed up.                                      */

	query_desc = (BSA_QueryDescriptor *)malloc(sizeof(BSA_QueryDescriptor));
	if(query_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA Query Descriptor to perform Restore\n");
		return -1;
	}
	memset(query_desc, 0x00, sizeof(BSA_QueryDescriptor));

	query_desc->copyType = BSA_CopyType_BACKUP;
	query_desc->objectType = BSA_ObjectType_FILE;
	query_desc->objectStatus = BSA_ObjectStatus_MOST_RECENT;
	strncpy(query_desc->objectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(query_desc->objectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));
	strncpy(query_desc->objectName.pathName, BackupFilePathName, (1 + strlen(BackupFilePathName)));
	strncpy(query_desc->objectName.objectSpaceName, "", (1 + strlen("")));

	object_desc = (BSA_ObjectDescriptor *)malloc(sizeof(BSA_ObjectDescriptor));
	if(object_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA Object Descriptor to perform Restore\n");
		return -1;
	}
	memset(object_desc, 0x00, sizeof(BSA_ObjectDescriptor));

	/* Find the object to be restored.  If the copyId and other object information needed  *
	 * to restore the object are known, this step can be skipped. */
	status = BSAQueryObject(BsaHandle, query_desc, object_desc);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	/* Get the object.  */
	data_block = (BSA_DataBlock32 *)malloc(sizeof(BSA_DataBlock32));
	if(data_block == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA data block to perform Restore\n");
		return -1;
	}
	memset(data_block, 0x00, sizeof(BSA_DataBlock32));
	status = BSAGetObject(BsaHandle, object_desc, data_block);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	return 0;
}

int getBSARestoreData(int NetBackupBlockSize)
{
	gpBsaBuf = (char *)malloc(NetBackupBlockSize * sizeof(char));
	if(gpBsaBuf == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup data transfer buffer to perform Backup\n");
		return -1;
	}
	memset(gpBsaBuf, 0x00, NetBackupBlockSize);

	/* Initialize the data_block structure.  */
	data_block->bufferLen = NetBackupBlockSize;
	data_block->bufferPtr = gpBsaBuf;
	memset(data_block->bufferPtr, 0x00, NetBackupBlockSize);

	/* Read data in chunks of size 'NetBackupBlockSize' until the end of data. 
	 * Each chunk is sent to stdout which gets piped to the psql program
	 * in order to restore the database */
	while ((status = BSAGetData(BsaHandle, data_block)) == BSA_RC_SUCCESS) {

		write(1, data_block->bufferPtr, data_block->numBytes);
		total_bytes += data_block->numBytes;

	}
	if (status == BSA_RC_NO_MORE_DATA) {

		/* The last BSAGetData() that returns BSA_RC_NO_MORE_DATA may have data  *
		 * in the buffer.                                                        */
		write(1, data_block->bufferPtr, data_block->numBytes);
		total_bytes += data_block->numBytes;

	} else {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAGetData() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAGetData() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	return 0;
}

int endBSARestoreSession()
{
	status = BSAEndData(BsaHandle);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndData() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAEndData() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	/* End the backup transaction and commit the object.  */
	status = BSAEndTxn(BsaHandle, BSA_Vote_COMMIT);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAEndTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSATerminate(BsaHandle);
		return -1;
	}

	/* End the XBSA session  */
	BSATerminate(BsaHandle);

	free(gpBsaBuf);

	return 0;
}

/* This function will query the NetBackup server for the given object  *
 * (backup file path here). If the Query to NetBackup server succeeds, *
 * it implies that the object has been backed up using NetBackup.      *
 * If the Query to NetBackup  server fails, it implies that the object *
 * has not been backed up using NBU.                                   */

char *queryBSAObject(char *BackupFilePathName)
{
	/* Populate query descriptor */

	query_desc = (BSA_QueryDescriptor *)malloc(sizeof(BSA_QueryDescriptor));
	if(query_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA Query Descriptor to perform Restore\n");
		return NULL;
	}
	memset(query_desc, 0x00, sizeof(BSA_QueryDescriptor));

	query_desc->copyType = BSA_CopyType_BACKUP;
	query_desc->objectType = BSA_ObjectType_FILE;
	query_desc->objectStatus = BSA_ObjectStatus_MOST_RECENT;
	strncpy(query_desc->objectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(query_desc->objectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));
	strncpy(query_desc->objectName.pathName, BackupFilePathName, (1 + strlen(BackupFilePathName)));
	strncpy(query_desc->objectName.objectSpaceName, "", (1 + strlen("")));

	object_desc = (BSA_ObjectDescriptor *)malloc(sizeof(BSA_ObjectDescriptor));
	if(object_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to allocate memory for NetBackup BSA Object Descriptor to perform Restore\n");
		return NULL;
	}
	memset(object_desc, 0x00, sizeof(BSA_ObjectDescriptor));

	/* Query BSA object to check if it has been backed up */
	status = BSAQueryObject(BsaHandle, query_desc, object_desc);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return NULL;
	}

	return BackupFilePathName;
}


/* This function will query the NetBackup server for the given object *
 * (query object expresssion here). If the Query to the NetBackup     *
 * succeeds, it implies that one or more objects of the given format  *
 * were backed up to the NetBackup server. If the Query to the server *
 * fails, it implies that no objects of the given format were dumped  *
 * to the NetBackup server.                                           */

int searchBSAObject(char *NetBackupQueryObject)
{
	/* Populate query descriptor */

	query_desc = (BSA_QueryDescriptor *)malloc(sizeof(BSA_QueryDescriptor));
	if(query_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Failed to allocate memory for NetBackup BSA Query Descriptor to Query Dumped Objects\n");
		return -1;
	}
	memset(query_desc, 0x00, sizeof(BSA_QueryDescriptor));

	query_desc->copyType = BSA_CopyType_BACKUP;
	query_desc->objectType = BSA_ObjectType_FILE;
	query_desc->objectStatus = BSA_ObjectStatus_MOST_RECENT;
	strncpy(query_desc->objectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(query_desc->objectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));
	strncpy(query_desc->objectName.pathName, NetBackupQueryObject, (1 + strlen(NetBackupQueryObject)));
	strncpy(query_desc->objectName.objectSpaceName, "", (1 + strlen("")));

	object_desc = (BSA_ObjectDescriptor *)malloc(sizeof(BSA_ObjectDescriptor));
	if(object_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Failed to allocate memory for NetBackup BSA Object Descriptor to Query Dumped Objects\n");
		return -1;
	}
	memset(object_desc, 0x00, sizeof(BSA_ObjectDescriptor));

	/* Query BSA object to check if it has been backed up */
	status = BSAQueryObject(BsaHandle, query_desc, object_desc);
	if (status == BSA_RC_SUCCESS) {
		printf("%s\n", (char *)object_desc->objectName.pathName);
	}
	else if (status == BSA_RC_NO_MATCH) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "INFO: BSAQueryObject() did not find any object matching the query: %s\n", ErrorString);
		mpp_err_msg("INFO", "gp_bsa_query_agent", "BSAQueryObject() did not find any object matching the query: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}
	else {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	while ((status = BSAGetNextQueryObject(BsaHandle, object_desc)) == BSA_RC_SUCCESS) {
		printf("%s\n", object_desc->objectName.pathName);
	}

	if (status != BSA_RC_NO_MORE_DATA) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	status = BSAEndTxn(BsaHandle, BSA_Vote_COMMIT);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "BSAEndTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	return 0;
}

/* Function to delete BSA objects on NetBackup server. This function  *
 * will query the NetBackup server for the given object. If the Query *
 * to the NetBackup server succeeds, it will delete all objects of the*
 * given format which were backed up to the NetBackup server. If the  *
 * Query to the server fails, it implies that no objects of the given *
 * format were dumped to the NetBackup server. So, no objects will be *
 * deleted.                                                           */

int deleteBSAObjects(char *NetBackupDeleteObject)
{
	int del_status;
	/* Populate query descriptor */

	query_desc = (BSA_QueryDescriptor *)malloc(sizeof(BSA_QueryDescriptor));
	if(query_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "Failed to allocate memory for NetBackup BSA Query Descriptor to Query Objects to be deleted\n");
		return -1;
	}
	memset(query_desc, 0x00, sizeof(BSA_QueryDescriptor));

	query_desc->copyType = BSA_CopyType_BACKUP;
	query_desc->objectType = BSA_ObjectType_FILE;
	query_desc->objectStatus = BSA_ObjectStatus_MOST_RECENT;
	strncpy(query_desc->objectOwner.bsa_ObjectOwner, "XBSA Client", (1 + strlen("XBSA Client")));
	strncpy(query_desc->objectOwner.app_ObjectOwner, "XBSA App", (1 + strlen("XBSA App")));
	strncpy(query_desc->objectName.pathName, NetBackupDeleteObject, (1 + strlen(NetBackupDeleteObject)));
	strncpy(query_desc->objectName.objectSpaceName, "", (1 + strlen("")));

	object_desc = (BSA_ObjectDescriptor *)malloc(sizeof(BSA_ObjectDescriptor));
	if(object_desc == NULL){
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "Failed to allocate memory for NetBackup BSA Object Descriptor to Query Objects to be deleted\n");
		return -1;
	}
	memset(object_desc, 0x00, sizeof(BSA_ObjectDescriptor));

	/* Query BSA object to check if it has been backed up */
	status = BSAQueryObject(BsaHandle, query_desc, object_desc);
	if (status == BSA_RC_SUCCESS) {
		printf("Deleting BSA object from NBU server: %s\n", (char *)object_desc->objectName.pathName);
		del_status = BSADeleteObject(BsaHandle, object_desc->copyId);
		if (del_status != BSA_RC_SUCCESS) {
			ErrStrSize = 512;
			NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
			snprintf(msg, sizeof(msg), "INFO: BSADeleteObject() failed to delete given BSA objects: %s\n", ErrorString);
			mpp_err_msg("INFO", "gp_bsa_delete_agent", "BSADeleteObject() did not find any object matching the given format: %s\n", ErrorString);
			NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
			BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
			BSATerminate(BsaHandle);
			return -1;
		}
	}
	else if (status == BSA_RC_NO_MATCH) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "INFO: BSAQueryObject() did not find any object matching the query to delete objects: %s\n", ErrorString);
		mpp_err_msg("INFO", "gp_bsa_delete_agent", "BSAQueryObject() did not find any object matching the query to delete objects: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}
	else {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	while ((status = BSAGetNextQueryObject(BsaHandle, object_desc)) == BSA_RC_SUCCESS) {
		printf("Deleting BSA object from NBU server: %s\n", (char *)object_desc->objectName.pathName);
		del_status = BSADeleteObject(BsaHandle, object_desc->copyId);
		if (del_status != BSA_RC_SUCCESS) {
			ErrStrSize = 512;
			NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
			snprintf(msg, sizeof(msg), "INFO: BSADeleteObject() failed to delete given BSA objects: %s\n", ErrorString);
			mpp_err_msg("INFO", "gp_bsa_delete_agent", "BSADeleteObject() did not find any object matching the given format: %s\n", ErrorString);
			NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
			BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
			BSATerminate(BsaHandle);
			return -1;
		}
	}

	if (status != BSA_RC_NO_MORE_DATA) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAQueryObject() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "BSAQueryObject() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	status = BSAEndTxn(BsaHandle, BSA_Vote_COMMIT);
	if (status != BSA_RC_SUCCESS) {
		ErrStrSize = 512;
		NBBSAGetErrorString(status, &ErrStrSize, ErrorString);
		snprintf(msg, sizeof(msg), "ERROR: BSAEndTxn() failed with error: %s\n", ErrorString);
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "BSAEndTxn() failed with error: %s\n", ErrorString);
		NBBSALogMsg(BsaHandle, MSERROR, msg, "Restore");
		BSAEndTxn(BsaHandle, BSA_Vote_ABORT);
		BSATerminate(BsaHandle);
		return -1;
	}

	return 0;
}

#endif
