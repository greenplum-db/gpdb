/*
 *  cdbfilerepverify.c
 *
 *
 *  Copyright 2009-2011 Greenplum Inc. All rights reserved. *
 */
#include "postgres.h"

#include <signal.h>

#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "access/nbtree.h"
#include "cdb/cdbfilerepconnclient.h"
#include "cdb/cdbfilerepservice.h"
#include "cdb/cdbfilerepverify.h"
#include "cdb/cdbfilerepprimaryack.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistenttablespace.h"
#include "storage/bufpage.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/relfilenode.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "storage/itemid.h"
#include "utils/faultinjector.h"

/*
 * TODO list
 *			1) Signal verification process through SIGUSR1 when --suspend or --resume is issued
 *			2) handle errors for OpenOutputFiles and CloseOutputFiles
 *			DONE 3) check CRC algorithm
 *			4) shutdown
 *			5) transition to change tracking
 *			6) replace fread, fopen, ...
 *			7) block by block hash check file size mirror > fileOffset + data length
 *			DONE 8) send directory path from mirror to primary
 *			9) multiple entries for the same relation in .fix file
 *			10) external table file has to report hash from primary and hash from mirror when block by block diff is found
 */

/*
 * LEVELS OF VERIFICATION
 *
 *  1) Compute one hash value for an entire directory subtree-
 *  any chance this will ever pass?
 *  Right now this is isolated in a routine on its own rather than integrated
 *  into the directory walk
 *
 *  2) Compute a hash value over a file's contents or a directories immediate
 *  contents ( not the whole sub tree) - no locking,no exceptions
 *
 *  Could have another intermediate option -locking of whole file and apply
 *  exceptions but still just one hash over whole file
 * or just apply exceptions, no locking
 *
 *  3) Drill down into the detailed differences of a file or directory
 *  For a file, apply any exceptions based on file type, lock the file, compute
 *  hash over each sub block (locking the buffer) For a directory, identify
 *  which items are missing/added between primary and mirror
 *
 *  4) Could actually send the contents of a file over - block by block - do we
 *  every really want to do this level of compare?
 *  Could be useful if we wanted to do a repair of differences discovered
 *
 */

/*
 *  HIGH TODO
 *
 *  - Needs a few more diff file writes? ( review all writeFixFileRecord to see
 *  if have/need corresponding writeDiffTableRecord?) calls - Rajesh
 *
 *  - write a fix and diff file recprd when heap files are bigger than RELSEG_SIZE*BLCKSZ
 *
 *  - add a timestamp line to the fix file
 *
 *  MEDIUM TODO
 *   - replace all fopen/fread/fwrite/fclose/fseek with
 *   PathNameOpenFile/FileRead/FileWrite/FileClose/FileSeek
 *   - Sync before close required? or close imply sync?
 *
 *  SMALL TODO
 *
 *  - check default ignore lists in gp.py ; remove the hardcoded directories
 *  here
 *
 *  PERFORMANCE
 *
 *   - the EnhancedBlockCompares are not batched-	 that should change
 *  BATCH_BLOCK_HASHES
 *  First batch blocks of one large file - then perhaps also batch blocks of
 *  multiple files?
 *
 *  - one inbetween cheap and per-block - in medium lock everything but still
 *  just send one hash to mirror for the whole thing
 *  or data block full of hashes for many pages?
 *
 *  - Change FileRepVerifySummaryItem_s to be smaller
 *
 *  - minimize the size of transmited objects - FileRepIdentifier_u is big, we
 *  could save space with another representation
 *
 *  - pipelining of compute next hash while waiting for mirror to respond -
 *
 *  - Search for capping at 15 vs 31 and recheck
 *
 *  POSSIBLE TODO/CHECK
 *
 *  - fix checksum failures, timeouts, etc. - GO INTO CHANGETRACKING FROM
 *  VERIFICATION MESSAGES TOO?
 *
 *  - if high level directories don't exist report an error
 *
 *  - Get rid of compare options to mirror that we don;t need
 *
 *  - confused when primary process runs on mirror machines?
 *  PersistentFileSysObj_FilespaceScan returns primaryFilespaceLocation and
 *  mirrorFilespaceLocation the same regardless
 *  in my testing when primary process runs on mirror machine that is fine
 *  because they are the same machine but generally ok?
 *
 *  VerifyAllDiretories starts with getcwd - that changes whether on mirror or
 *  primary after resynch stop verification - verification only runs when it
 *  is on the primary
 *  failover to mirror - transactions are aborted ( put in design spec)
 *  what about restart?
 *  - better way that getcwd to determine base primary directory?
 *
 *  - maybe better to go with adding a slash than taking it off when comparing
 *  directory names
 *  to avoid matching directories that begin with the same name but are longer
 *  (eg. ~/gp_data/segment/primary/filerep0/base/10890 vs
 *  ~/gp_data/segment/primary/filerep0/base/1)
 *
 *  - What if rel node is deleted after the OnlineVerifyScan is initiated?
 *
 *  - asynchronous sends from primary for performance - send a bunch to mirror
 *  - mirror acks them all?
 *
 *  - MAXPGPATH+1+20 if can allow segment file num?
 *
 *  DONE (TEST MORE/CHECK MORE)
 *  - restart from checkpoint -	DONE - test with filespaces
 *
 *  - VERIFICATION_ERROR_INTERNAL: Error in FileRepPrimary_VerifyAllRelFileNode
 * - mirrorDataSynchronizationState 0",,,,,,,0,,"cdbfilerepverify.c",432,
 *
 *  - Review how errors are returned/responded to - make sure the verification
 *  process does not stop for errors that should just be reported!
 *  VERIFICATION_ERROR_REPORT - this is definitely a mismatch between primary
 *  and mirror
 *  VERIFICATION_ERROR_WARNING - this could indicate a mismatch between primary
 *  and mirror but we there are legitimate reasons this could be different
 *
 *  do we want to distinguish between WARNINGs for which have a lower level
 *  compare routine we can try to see for sure and
 *  WARNINGS where all we can do is retry?
 *  VERIFICATION_ERROR_INTERNAL - should only happen if our code is wrong
 *  VERIFICATION_ERROR_FATAL- we cannot continue verification
 *
 *  - Search STATUS_, ERROR, status
 *
 *  - Review anywhere that is return STATUS_ERROR or return status
 *  ereport, errcode, errmsg, FileRep_errcontext, errcode_for_file_access,
 *  FileRep_serrdetail, FileRep_errdetail_Shmem,
 *  FileRep_SetSegmentState(SegmentStateFault),
 *
 *  - Review logging and turn off verbose debugging output
 *  - Clean up (TODO, JNM, XXX, #if 0)
 *  - Double check every malloc/alloc is freed, every open has a close, etc. -
 *  DONE
 *  - pfree after all FileRep_GetFileName  - DONE
 *  - audit total amount of memory used per request/ check for no memory leak
 *
 * right now we have only one outstanding request at a time
 * TODO: allow more - have a hashtable of requests (via token)
 */

#define VERIFYDIR				"pg_verify"
#define INPROGRESS_TOKEN_FILE	"inprogress_token"

char gMirrorBaseDir[MAXPGPATH+1];
FileRepVerifyMirrorRequestParams_s gMirrorRequestParams;

typedef struct FileRepVerifyShmem_s
{
	slock_t					lock;

	volatile bool			isVerificationInProgress;
	volatile bool			isNewRequestWaiting;
	/* SYNCHRONOUSABORTSUSPEND*/
	volatile bool           signalSynchronousRequests;

	FileRepVerifyRequest_s	request;

	bool					availableForReuse;
	bool					resume;

} FileRepVerifyShmem_s;

FileRepVerifyShmem_s *gFileRepVerifyShmem = NULL;

Size FileRepVerifyShmemSize(void);
void FileRepVerifyShmemInit(void);

void FileRepVerify_LockAcquire(void);
void FileRepVerify_LockRelease(void);

void FileRepPrimary_InitVerificationRequest(
											FileRepVerifyRequest_s *request);

int FileRepPrimary_EstimateVerificationRequest(
											   FileRepVerifyRequest_s *request);

int FileRepPrimary_ExecuteVerificationRequest(
											  FileRepVerifyRequest_s	*request,
											  bool						resume);

int FileRepPrimary_ExchangeRequestParameters(
										 FileRepVerifyRequest_s *request);

void FileRepPrimary_ClearCheckpoint(
									FileRepVerifyRequest_s *request);

void FileRepPrimary_SetLogicalCheckpoint(
										 FileRepVerifyRequest_s		*request,
										 int64						persistentSerialNum);

void FileRepPrimary_SetPhysicalCheckpoint(
										  FileRepVerifyRequest_s	*request,
										  char						*nextParent,
										  char						*nextChild);

bool FileRepPrimary_CheckpointFileIsPresent(
											char	*token,
											char	*errorMessage,
											int		errMsgLen);

int FileRepPrimary_RestoreVerificationRequest(
											  FileRepVerifyRequest_s	*request,
											  char						*token);

int FileRepPrimary_CheckpointVerificationRequest(
	FileRepVerifyRequest_s *request,
	bool force);

bool stillInSync(void);

int FileRepPrimary_PeriodicTasks(FileRepVerifyRequest_s *request);

bool FileRepPrimary_ContinueVerificationRequest(FileRepVerifyRequest_s *request);

bool
isRelBufKindOkForHeapException(PersistentFileSysRelBufpoolKind kind);

/*
 *  High level request execution functions on primary, primary drives or
 *  controls the request options
 *  Below this level requestOptions not passed down, just logControl and
 *  other lower level parameters
 */
int FileRepPrimary_VerifyAllRelFileNodes(
										 FileRepVerifyRequest_s *request,
										 bool					resume,
										 bool justCount
	);

int FileRepPrimary_VerifyAllRelFileNodes_SPI(
											 FileRepVerifyRequest_s *request);

int FileRepPrimary_VerifySystemFilespace(
										FileRepVerifyRequest_s	*request,
										bool					resume);

int FileRepPrimary_VerifyUserFilespaces(
									   FileRepVerifyRequest_s	*request,
									   bool						resume);

int
FileRepPrimary_EstimateSystemFilespace(
									  FileRepVerifyRequest_s * request);

int
FileRepPrimary_EstimateUserFilespaces(
									 FileRepVerifyRequest_s * request);

int
FileRepPrimary_EstimateDirectory(
								 FileRepVerifyRequest_s *request,
								 char *primaryBaseDir);

int
FileRepPrimary_VerifyDirectory(
							   FileRepVerifyRequest_s *request,
							   char *primaryBaseDir,
							   char *mirrorBaseDir,
							   bool resume);

/* TODO - symmetry  char * vs FileName, int vs bool */
int isExcludedFile(
				   FileName fileName,
				   int		numExcluded,
				   char		**excludeList);

int isExcludedDirectory(
						FileName	directoryName,
						int			numExcluded,
						char		**excludeList);

bool looksLikeRelFileName(
						  char *directory,
						  char *file);

int
FileRepVerify_NumHeapSegments( RelFileNode relFileNode);

int FileRepVerify_ComputeFileHash(
								  char *filename,
								  pg_crc32 *hashcode,
								  FileRepVerify_CompareException_e exception,
								  uint64 AO_logicalEof, /* if exception is AO */
								  uint64 whichSegment,  /* if exception is Heap */
								  FileRepVerifyLogControl_s logControl );

int FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing(
																   FileRepVerifyRequest_s *request,
																   char *primaryBaseDir,
																   char *mirrorBaseDir,
																   char *listingBuffer,
																   uint32 bufferSizeBytes,
																   uint32 *numEntries,
																   bool justCount,
																   bool *complete,
																   char *whereNextParent,
																   char *whereNextChild );

int FillBufferWithDirectoryTreeListing(
									   FileRepVerifyRequest_s *request,
									   char *priBaseDirectory,
									   char *mirrBaseDirectory,
									   char *fromBasePath,
									   char *buf,
									   uint32 *used_len,
									   uint32 bufferSizeBytes,
									   uint32 *num_entries,
									   bool justCount,
									   bool *complete,
									   char *whereNextParent,
									   char *whereNextChild);

bool wasHandledAlready(
					   char *thisDirectory,
					   char *thisChild,
					   char *nextDirectory,
					   char *nextChild);

bool keepItem(
			  char *toMatch,
			  bool isFile,
			  char *toKeep,
			  FileRepVerifyRequestType_e requestType);

int FileRepPrimary_SendEnhancedBlockHashes(
										   FileRepRelFileNodeInfo_s relinfo,
										   bool *matched,
										   FileRepVerifyLogControl_s logControl );


int FileRepPrimary_CompareDirectoryListing(
										FileRepVerifyRequest_s	*request,
										char					*primaryDirectoryName,
										char					*mirrorDirectoryName,
										FileRepIdentifier_u		fileRepIdentifier,
										FileRepRelationType_e	fileRepRelationType,
										int						*numChildrenAddedOnMirror,
										FileRepVerifyLogControl_s logControl);

int FileRepMirror_ExchangeRequestParameters(
										  FileRepMessageHeader_s		*fileRepMessageHeader,
										  char *fileRepMessageBody,
										  void **response,
										  uint32 *responseSizeBytes,
										  FileRepVerifyLogControl_s logControl  );


int FileRepMirror_CompareDirectoryContents(
										   FileRepMessageHeader_s		*fileRepMessageHeader,
										   char							*fileRepMessageBody,
										   void							**response,
										   uint32						*responseSizeBytes,
										   FileRepVerifyLogControl_s	logControl);

int FileRepMirror_CompareEnhancedBlockHash(
										   FileRepMessageHeader_s		*fileRepMessageHeader,
										   char							*fileRepMessageBody,
										   void							**response,
										   uint32						*responseSizeBytes,
										   FileRepVerifyLogControl_s	logControl);

int FileRepMirror_CompareAllSummaryItems(
										 FileRepMessageHeader_s		*fileRepMessageHeader,
										 char *fileRepMessageBody,
										 void **response,
										 uint32 *responseSizeBytes,
										 FileRepVerifyLogControl_s logControl  );

int FileRepMirror_CompareSummaryItem(
									 FileRepVerifySummaryItem_s *pItem,
									 bool doHash,
									 bool includeFileSizeInfo,
									 bool *mismatch,
									 FileRepVerifyLogControl_s logControl);

int FileRepVerify_GetRelFileNodeInfoById(
										 RelFileNode relFileNode,
										 FileRepRelFileNodeInfo_s **relinfo,
										 int *numReturned);

int FileRepVerify_GetRelFileNodeInfoTable(
										  FileRepRelFileNodeInfo_s **relfilenodeTable,
										  int *rowsReturned);

void FileRepVerify_PrintRelFileNodeInfoTable(
											 FileRepRelFileNodeInfo_s *relfilenodeTable,
											 int numRows);

int FileRepPrimary_ComputeSummaryItemsFromRelFileNodeInfoTable(
															   FileRepRelFileNodeInfo_s *relfilenodeTable,
															   int numRows,
															   FileRepVerifySummaryItem_s *summaryItemArray,
															   bool doHash,
															   bool includeFileSizeInfo,
															   FileRepVerifyLogControl_s logControl);

static bool FileRepVerify_ProcessHeapPage(
										  Page page,
										  bool *madeChange,
										  FileRepVerifyLogControl_s logControl);
//	                                      pg_crc32 *hashcode);

static bool FileRepVerify_ProcessBtreeIndexPage(
												Page page,
												bool *madeChange,
												FileRepVerifyLogControl_s logControl);
//												pg_crc32 *hashcode);

void
writeFailedResultFile(char *token);

bool FileRepPrimary_StartRequest(
								 FileRepVerifyRequest_s *request,
								 bool resume,
								 bool autoResume);

int FileRepPrimary_BuildRequest(
								FileRepVerifyRequest_s *request);

void FileRepPrimary_CleanRequest(
								 FileRepVerifyRequest_s *request);

void printFileRepVerifyRequest(
							   FileRepVerifyRequest_s request,
							   int printLevel);

void printFileRepVerifyArguments(
								 FileRepVerifyArguments args,
								 int printLevel);
void clearFileRepVerifyArguments(
								 FileRepVerifyArguments *args);

void FileRepVerify_CreateInProgressTokenFile(char * token);

void FileRepVerify_EmptyInProgressTokenFile(void);

bool FileRepVerify_GetInProgressTokenFromFile(char *token);

int FileRep_OpenOutputFiles(
							FileRepVerifyLogControl_s *logControl,
							bool isPrimary);

void FileRep_CloseOutputFiles(
							  FileRepVerifyLogControl_s *logControl,
							  bool isPrimary);

void writeFixFileRecord(
						FileRepVerifyLogControl_s logControl,
						char * type,
						char * goodSegHost,
						char * goodSegFilePath,
						char * badSegHost,
						char * badSegFilePath);

bool isPageAllZeros(Page page);

static void writeDiffTableRecord(
								 FileRepVerifyLogControl_s logControl,
								 char *token,
								 char * diffType,
								 char * primarySegHost,
								 char * primarySegFilePath,
								 char * mirrorSegHost,
								 char * mirrorSegFilePath,
								 int	num_files_primary,
								 int	num_files_mirror,
								 int	blockNumber,
								 int	size_on_primary,
								 int	size_on_mirror,
								 RelFileNode	*relfilenode
								 );

bool
extractDir(char *file, char *dir, int maxLen);

bool
extractDirPath(char *file, char *dir, int maxLen);

bool
checkDirectoryExists(char *dir);

bool
checkFileExists(char *file);

bool
checkDirectoryInDB(char *dir);

bool
checkFileInDB(char *file);

bool
isPageAllZeros(Page page)
{

	char *buf = (char *) page;
	int i = 0;

	for ( i=0; i< BLCKSZ; i++)
	{
		if (buf[i] != 0)
		{
			return false;
		}
	}
	return true;
}

void
writeFixFileRecord(
				   FileRepVerifyLogControl_s logControl,
				   char *type,
				   char *goodSegHost,
				   char *goodSegFilePath,
				   char *badSegHost,
				   char *badSegFilePath)
{

	/*
     *
	 *  categoryAction['ao:']			  = COPY
	 *  categoryAction['heap:']			  = COPY
	 *  categoryAction['btree:']			  = COPY
	 *  categoryAction['extra_p:']		  = COPY
	 *  categoryAction['extra_m:']		  = DELETE
	 *  categoryAction['missing_topdir_p:'] = COPY
	 *  categoryAction['missing_topdir_m:'] = COPY
	 *  categoryAction['unknown:']		  = NO_ACTION?
     */
	errno = 0;
	if (logControl.filesAreOpen)
	{
#define MAX_FIX_MESSAGE_LEN (MAXPGPATH*5)
		char fixMessage[MAX_FIX_MESSAGE_LEN];

#define FIX_MESSAGE_FORMAT "%s: %s:%s %s:%s\n"
		snprintf(fixMessage, MAX_FIX_MESSAGE_LEN, FIX_MESSAGE_FORMAT, type,
				 goodSegHost, goodSegFilePath, badSegHost, badSegFilePath);

		if ((int) FileWrite(logControl.fixFile, fixMessage, strlen(fixMessage)) != strlen(fixMessage))
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not write to file '%s' data '%s' length '%lu' verification token '%s' : %m "
							"verification aborted",
							logControl.logOptions.fixPath,
							fixMessage,
							(long unsigned int) strlen(fixMessage),
							logControl.logOptions.token),
					 errhint("address file system failure and run gpverify")));
			return;
		}
		if (FileSync(logControl.fixFile) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not flush (fsync) to file '%s' verification token '%s' : %m "
							"verification aborted",
							logControl.logOptions.fixPath,
							logControl.logOptions.token),
					 errhint("address file system failure and run gpverify")));

			return;
		}
	}
}

void
FileRepVerify_CreateInProgressTokenFile(char *token)
{
	struct	stat		st;
	char	fileName[MAXPGPATH+1];
	File	f;
	int		len = 0;

	if (strlen(token) > FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN+1)
	{
		return;
	}
	errno = 0;
	if (stat(VERIFYDIR, &st) < 0)
	{
		errno = 0;
		if (mkdir(VERIFYDIR, 0700) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not create directory '%s' verification token '%s' : %m "
							"verification aborted",
							VERIFYDIR,
							token),
					 errhint("address file system failure and run gpverify")));
			return;
		}
	}

	snprintf(fileName,
			 MAXPGPATH+1,
			 "%s/%s",
			 VERIFYDIR,
			 INPROGRESS_TOKEN_FILE);

	f =	PathNameOpenFile(fileName,
						 O_RDWR | O_CREAT | O_TRUNC| PG_BINARY,
						 S_IRUSR | S_IWUSR);

	if (f < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' verification token '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName,
						token),
				 errhint("run gpverify")));
		return;
	}

	len = strlen(token) + 1;

	if ((int) FileWrite(f, (char *) &len, sizeof(int)) != sizeof(int))
	{
		FileClose(f);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not write to file '%s' verification token '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName,
						token),
				 errhint("run gpverify")));
		return;
	}

	if ((int) FileWrite(f, token, len * sizeof(char)) != (len *sizeof(char)))
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not write to file '%s' verification token '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName,
						token),
				 errhint("run gpverify")));
	}

	FileClose(f);
}

bool
FileRepVerify_GetInProgressTokenFromFile(char *token)
{
	File	fd;
	char	fileName[MAXPGPATH+1];
	int		len = 0;

	snprintf(fileName,
			 MAXPGPATH+1,
			 "%s/%s",
			 VERIFYDIR,
			 INPROGRESS_TOKEN_FILE);

	errno = 0;
	fd = PathNameOpenFile(fileName,
						  O_RDONLY | PG_BINARY,
						  S_IRUSR| S_IWUSR);

	if (fd < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName),
				 errhint("run gpverify")));
		return false;
	}

	if (FileSeek(fd, 0L, SEEK_END) < 0)
	{
		FileClose(fd);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"in progress file empty '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName),
				 errhint("run gpverify")));

		return false;
	}

	if (FileSeek(fd, 0L, SEEK_SET) < 0)
	{
		FileClose(fd);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not seek to begin of file '%s' : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName),
				 errhint("run gpverify")));

		return false;
	}

	if (FileRead(fd, (char *) &len, sizeof(int)) != sizeof(int))
	{
		FileClose(fd);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not read from file '%s' verification token : %m "
						"verification aborted",
						(fileName == NULL) ? "<null>" : fileName),
				 errhint("run gpverify")));

		return false;
	}

	if (len == 0 || len > FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN)
	{
		FileClose(fd);

		ereport(LOG,
				(errmsg("no in progress verification file '%s' ",
						(fileName == NULL) ? "<null>" : fileName)));

		return false;
	}
	else
	{
		if (FileRead(fd, (char *) token, len * sizeof(char)) != (len * sizeof(char)))
		{
			FileClose(fd);

			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not read from file '%s' verification token : %m "
							"verification aborted",
							(fileName == NULL) ? "<null>" : fileName),
					 errhint("run gpverify")));

			return false;
		}

		FileClose(fd);

		return true;
	}
}

void
FileRepVerify_EmptyInProgressTokenFile(void)
{
	struct	stat		st;
	char	fileName[MAXPGPATH+1];
	File	f;
	int		len = 0;

	errno = 0;
	if (stat(VERIFYDIR, &st) < 0)
	{
		if (mkdir(VERIFYDIR, 0700) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not create directory '%s' : %m "
							"verification aborted",
							VERIFYDIR),
					 errhint("address file system failure and run gpverify")));
			return;
		}
	}

	snprintf(fileName,
			 MAXPGPATH+1,
			 "%s/%s",
			 VERIFYDIR,
			 INPROGRESS_TOKEN_FILE);

	f =	PathNameOpenFile(fileName,
						 O_RDWR | O_CREAT | O_TRUNC| PG_BINARY,
						 S_IRUSR | S_IWUSR);

	if (f < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' : %m "
						"verification aborted",
						fileName),
				 errhint("address file system failure and run gpverify")));

		return;
	}

	len = 0;

	if ((int) FileWrite(f, (char *) &len, sizeof(int)) != sizeof(int))
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not write to file '%s' : %m "
						"verification aborted",
						fileName),
				 errhint("address file system failure and run gpverify")));
	}

	FileClose(f);
}

int
FileRep_OpenOutputFiles(
						FileRepVerifyLogControl_s *logControl,
						bool isPrimary)
{

	struct	stat		st;

	errno = 0;

	logControl->filesAreOpen = false;

	if (stat(VERIFYDIR, &st) < 0)
	{
		if (mkdir(VERIFYDIR, 0700) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not create directory '%s' : %m "
							"verification aborted ",
							VERIFYDIR),
					 errhint("address file system failure and run gpverify")));

			return STATUS_ERROR;
		}
	}

	/* for each token we need a token.log, token.fix and token.chkpnt */
	snprintf(logControl->logOptions.logPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.log",
			 VERIFYDIR, logControl->logOptions.token);

	snprintf(logControl->logOptions.fixPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.fix",
			 VERIFYDIR, logControl->logOptions.token);

	snprintf(logControl->logOptions.chkpntPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.chkpnt",
			 VERIFYDIR, logControl->logOptions.token);

	snprintf(logControl->logOptions.externalTablePath,
			 MAXPGPATH+1,
			 "%s/verification_%s.tbl",
			 VERIFYDIR, logControl->logOptions.token);

	snprintf(logControl->logOptions.resultsPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.result",
			 VERIFYDIR, logControl->logOptions.token);

	logControl->logFile = PathNameOpenFile(logControl->logOptions.logPath,
										   O_RDWR | O_CREAT | PG_BINARY,
										   S_IRUSR | S_IWUSR);

	if (logControl->logFile < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' : %m "
						"verification aborted ",
						logControl->logOptions.logPath),
				 errhint("address file system failure and run gpverify")));

		return STATUS_ERROR;

	}

	if (FileSeek(logControl->logFile, 0, SEEK_END) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not seek to end of file '%s' : %m "
						"verification aborted",
						logControl->logOptions.logPath),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}


	/* fix file */
	logControl->fixFile = PathNameOpenFile(logControl->logOptions.fixPath,
										   O_RDWR | O_CREAT | PG_BINARY,
										   S_IRUSR | S_IWUSR);

	if (logControl->fixFile <= 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' : %m "
						"verification aborted ",
						logControl->logOptions.fixPath),
				 errhint("address file system failure and run gpverify")));

		return STATUS_ERROR;
	}

	if (FileSeek(logControl->fixFile, 0, SEEK_END) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not seek to end of file '%s' : %m "
						"verification aborted",
						logControl->logOptions.fixPath),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	if (isPrimary)
	{
		/* checkpoint file */
		logControl->chkpntFile = PathNameOpenFile(
												  logControl->logOptions.chkpntPath,
												  O_RDWR | O_CREAT | PG_BINARY,
												  S_IRUSR | S_IWUSR);

		if (logControl->chkpntFile < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not open file '%s' : %m "
							"verification aborted ",
							logControl->logOptions.chkpntPath),
					 errhint("address file system failure and run gpverify")));

			return STATUS_ERROR;
		}

		if (FileSeek(logControl->chkpntFile, 0, SEEK_END) < 0)
		{
			ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not seek to end of file '%s' : %m "
						"verification aborted",
						logControl->logOptions.chkpntPath),
				 errhint("run gpverify")));

			return STATUS_ERROR;
		}

		/*  External Table file */
		logControl->externalTableFile = PathNameOpenFile(
														 logControl->logOptions.externalTablePath,
														 O_RDWR | O_CREAT | PG_BINARY,
														 S_IRUSR | S_IWUSR );

		if (logControl->externalTableFile < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not open file '%s' : %m "
							"verification aborted ",
							logControl->logOptions.externalTablePath),
					 errhint("address file system failure and run gpverify")));

			return STATUS_ERROR;
		}

		logControl->resultsFile = PathNameOpenFile(
												   logControl->logOptions.resultsPath,
												   O_RDWR | O_CREAT | O_TRUNC | PG_BINARY,
												   S_IRUSR | S_IWUSR );

		if (logControl->resultsFile < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not open file '%s' : %m "
							"verification aborted ",
							logControl->logOptions.resultsPath),
					 errhint("address file system failure and run gpverify")));

			return STATUS_ERROR;
		}

	}

	logControl->filesAreOpen = true;
	return STATUS_OK;
}

void
FileRep_CloseOutputFiles(
						 FileRepVerifyLogControl_s *logControl,
						 bool isPrimary)
{
	if (logControl->filesAreOpen)
	{
		FileClose(logControl->logFile);
		FileClose(logControl->fixFile);

		if (isPrimary)
		{
			FileClose(logControl->chkpntFile);
			FileClose(logControl->externalTableFile);
			FileClose(logControl->resultsFile);
		}
	}
	logControl->filesAreOpen = false;
}

void printVerificationStatus(
	int messageLevel,
	FileRepVerifyRequest_s *request);
void verifyLog(
	FileRepVerifyLogControl_s logControl,
	int messageLevel,
	char * format, ...);

#define MAX_LOG_MESSAGE 1000
char logMessage[MAX_LOG_MESSAGE+1];


Size FileRepVerifyShmemSize(void)
{
	Size		size;

	size = sizeof(FileRepVerifyShmem_s);
	return size;
}

void FileRepVerifyShmemInit(void)
{
	gFileRepVerifyShmem =
		(FileRepVerifyShmem_s*)ShmemAlloc(sizeof(FileRepVerifyShmem_s));
	SpinLockInit(&gFileRepVerifyShmem->lock);

	memset(gFileRepVerifyShmem, 0, sizeof(FileRepVerifyShmem_s));

	gFileRepVerifyShmem->availableForReuse = true;

	/* aren't these unnecessary after memset? doesn't hurt*/
	gFileRepVerifyShmem->request.compareStats.totalBytesVerified = 0;
	gFileRepVerifyShmem->request.compareStats.totalBytesToVerify = 0;
	gFileRepVerifyShmem->isNewRequestWaiting = false;
	gFileRepVerifyShmem->isVerificationInProgress = false;
	/* SYNCHRONOUSABORTSUSPEND */
	gFileRepVerifyShmem->signalSynchronousRequests = false;
	gFileRepVerifyShmem->resume = false;
}

void
FileRepVerify_LockAcquire(void)
{
	SpinLockAcquire(&gFileRepVerifyShmem->lock);
}

void
FileRepVerify_LockRelease(void)
{
	SpinLockRelease(&gFileRepVerifyShmem->lock);
}


void
printFileRepVerifyRequest(
	FileRepVerifyRequest_s request,
	int printLevel)
{

	elog(printLevel, "estimate only %d",	 (int) request.estimateOnly);
	elog(printLevel, "all logical %d",  (int)request.allLogical);
	elog(printLevel, "all physical %d",	(int)request.allPhysical);
	elog(printLevel, "do hash %d", (int) request.doHash);
	elog(printLevel, "skip rel files %d", (int) request.skipRelFiles);
	elog(printLevel, "include file size info %d",
		 (int) request.includeFileSizeInfo);
	elog(printLevel, "do cheap compare first %d",
		 (int) request.doCheapCompareFirst);
	elog(printLevel, "state %d", (int) request.state);
	elog(printLevel, "request type %d", (int) request.requestType);
	elog(printLevel, "verify item %s", request.verifyItem);


}
void
printFileRepVerifyArguments(FileRepVerifyArguments args, int printLevel)
{
	elog(printLevel, "verification details request arguments");
	elog(printLevel, "full verify %d",  (int) args.fullVerify);
	elog(printLevel, "estimate only %d", (int) args.estimateOnly);
	elog(printLevel, "print history %d", (int) args.printHistory);
	elog(printLevel, "abort %d", (int) args.abort);
	elog(printLevel, "suspend %d", (int)	args.suspend);
	elog(printLevel, "resume %d", (int)	args.resume);
	elog(printLevel, "resport state %d", (int)	args.reportState);
	elog(printLevel, "token %s",	args.token);
	elog(printLevel, "compare file %s",	args.compareFile);
	elog(printLevel, "log level %s", args.logLevel);
	elog(printLevel, "content number %s",	args.contentNumber);
	elog(printLevel, "directory tree %s",	args.directoryTree);
	elog(printLevel, "ignore pattern %s",  args.ignorePattern);
	elog(printLevel, "dir ignore path %s",	args.dirIgnorePath);
}

void
clearFileRepVerifyArguments(FileRepVerifyArguments *args)
{
	/*
	   isn't this enough? safer to do it at least
	   to make sure no newly added fields are missed!
	*/
	memset(args, 0, sizeof(FileRepVerifyArguments));

	args->fullVerify = false;
	args->estimateOnly = false;
	args->printHistory = false;
	args->abort = false;
	args->suspend = false;
	args->resume = false;
	args->reportState = false;
	args->token[0] = '\0';
	args->compareFile[0] = '\0';
	args->logLevel[0]= '\0';
	args->contentNumber[0] = '\0';
	args->directoryTree[0] = '\0';
	args->ignorePattern[0] = '\0';
	args->dirIgnorePath[0] = '\0';

}

char*
FileRepVerify_GetCurrentToken(void)
{
	return ((gFileRepVerifyShmem != NULL) ?
			(gFileRepVerifyShmem->request.fileRepVerifyArguments.token) : "");

}

float
FileRepVerify_GetTotalGBsVerified(void)
{
	return((gFileRepVerifyShmem != NULL)?
		   (((float)gFileRepVerifyShmem->request.compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0) ): 0);
}

float
FileRepVerify_GetTotalGBsToVerify(void)
{
	return((gFileRepVerifyShmem != NULL)?
		   (((float)gFileRepVerifyShmem->request.compareStats.totalBytesToVerify)/(1024.0*1024.0*1024.0) ): 0);
}

void
FileRepVerify_SetTotalBlocksToVerify(int64 totalBytesToVerify)
{
	gFileRepVerifyShmem->request.compareStats.totalBytesToVerify = totalBytesToVerify;
}

void
FileRepVerify_AddToTotalBlocksToVerify(int64 moreBlocksToVerify)
{
	gFileRepVerifyShmem->request.compareStats.totalBytesToVerify += moreBlocksToVerify;
}

void
FileRepVerify_AddToTotalBlocksVerified(int64 moreBlocksVerified)
{
	gFileRepVerifyShmem->request.compareStats.totalBytesVerified += moreBlocksVerified;
}

/*
 * TODO
 * Update the counters before querying for the gpstate with
 * verification details.
 */
struct timeval
FileRepVerify_GetEstimateVerifyCompletionTime(void)
{
	char			temp[128];
	pg_time_t		tt;
	struct timeval	currentVerificationTime = {0, 0};
	struct timeval	estimateVerifyCompletionTime = {0, 0};

	if (gFileRepVerifyShmem == NULL)
	{
		return estimateVerifyCompletionTime;
	}
	/* pull values out of shared memory into local variables so we
	 * have consistent values for calculation here
	 */
	int64 totalBytesToVerify =
		gFileRepVerifyShmem == NULL ? 0L :
		gFileRepVerifyShmem->request.compareStats.totalBytesToVerify;
	int64 totalBytesVerified =
		gFileRepVerifyShmem == NULL ? 0L :
		gFileRepVerifyShmem->request.compareStats.totalBytesVerified;

	if (totalBytesToVerify == 0L || totalBytesVerified == 0L)
	{
		return estimateVerifyCompletionTime;
	}
	struct timeval startVerificationTime =
		gFileRepVerifyShmem->request.startVerificationTime;

	gettimeofday(&currentVerificationTime, NULL);

	if ((gFileRepVerifyShmem->request.state == FileRepVerifyRequestState_RUNNING ) &&
		(totalBytesToVerify > totalBytesVerified) &&
		(totalBytesToVerify!= 0))
	{
		estimateVerifyCompletionTime.tv_sec =
			(((currentVerificationTime.tv_sec - startVerificationTime.tv_sec) *
			  (totalBytesToVerify - totalBytesVerified)) /
			 totalBytesVerified) + currentVerificationTime.tv_sec;
	}
	else
	{
		estimateVerifyCompletionTime = currentVerificationTime;
	}

	if (Debug_filerep_print)
	{
		tt = (pg_time_t) estimateVerifyCompletionTime.tv_sec;
		pg_strftime(temp, sizeof(temp), "%a %b %d %H:%M:%S.%%06d %Y %Z",
					pg_localtime(&tt, session_timezone));

		elog(LOG,
			 "verification info: "
			 "total blocks to verify " INT64_FORMAT " "
			 "blocks verified "	 INT64_FORMAT  " "
			 "estimate verification completion time '%s' ",
			 totalBytesToVerify,
			 totalBytesVerified,
			 temp);
	}

	return estimateVerifyCompletionTime;
}

/*
 * Get request start time
 */
struct timeval
FileRepVerify_GetVerifyRequestStartTime(void)
{
	struct timeval	startVerificationTime = {0, 0};

	if (gFileRepVerifyShmem != NULL)
	{
		return gFileRepVerifyShmem->request.startVerificationTime;
	}
	else
	{
		return startVerificationTime;
	}

}

char*
FileRepVerify_GetVerifyRequestMode(void)
{

	if (gFileRepVerifyShmem == NULL)
	{
		return "Unknown";
	}


	if (gFileRepVerifyShmem->request.requestType ==

		FileRepVerifyRequestType_FULL)
	{
		return "Full";
	}
	else	 if (gFileRepVerifyShmem->request.requestType ==
				 FileRepVerifyRequestType_DIRECTORY)
	{
		return "Directory";
	}
	else	 if (gFileRepVerifyShmem->request.requestType ==
				 FileRepVerifyRequestType_FILE)
	{
		return "File";
	}
	else
	{
		return "Unknown";
	}
}


void printVerificationStatus(
	int messageLevel,
	FileRepVerifyRequest_s *request)
{

	if (request == NULL)
	{
		return;
	}
	verifyLog(request->logControl,
			  messageLevel,
			  "VERIFICATION_STATUS: %d of %d items COMPLETE (%0.2f); %0.2f of %0.2f GB (%0.2f); %d mismatches (%0.2f), %d required enhanced compare (%0.2f)" ,
			  request->compareStats.comparesComplete,
			  request->compareStats.estimateTotalCompares,
			  ((float)request->compareStats.comparesComplete/
			   (float)request->compareStats.estimateTotalCompares)*100.0,
			  (float) request->compareStats.totalBytesVerified/(1024.0*1024.0*1024.0) ,
			  (float) request->compareStats.totalBytesToVerify/(1024.0*1024.0*1024.0) ,
			  ((float)request->compareStats.totalBytesVerified/
			   (float)request->compareStats.totalBytesToVerify)*100.0,
			  request->compareStats.totalMismatches-
			  request->compareStats.totalMismatchesResolved,
			  ((float)( request->compareStats.totalMismatches-
						request->compareStats.totalMismatchesResolved)/
			   (float)request->compareStats.comparesComplete)*100.0,
			  request->compareStats.numEnhancedCompares,
			  ((float)request->compareStats.numEnhancedCompares/
			   (float)request->compareStats.comparesComplete)*100.0);

}
void verifyLog(
	FileRepVerifyLogControl_s logControl,
	int messageLevel,
	char * format, ...)
{
	/* I'd like to avoid allocating this on the stack each time
	 *  so I made it global rather than local
	 * //char logMessage[MAX_LOG_MESSAGE+1];
	 */
	va_list args;
	va_start (args, format);

	errno = 0;
	vsnprintf(logMessage, (MAX_LOG_MESSAGE)*sizeof(char), format, args);
	if (messageLevel >= logControl.logOptions.level)
	{

		/* for debugging can put all verifyLog messages into the log */
#if 1
		/* is LOG enough to guarantee that this log record will
		 *  be generated? gpverify will rely on this
		 */
		elog(LOG, "verification token %s %s",
			 logControl.logOptions.token,
			 logMessage);
#endif
		if (logControl.filesAreOpen)
		{
			if ((int) FileWrite(logControl.logFile,
								logMessage,
								strlen(logMessage))
				!= strlen(logMessage))
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not write to file '%s' data '%p' dataLength '%lu' : %m",
								logControl.logOptions.logPath,
								logMessage,
								(long unsigned int) strlen(logMessage))));

			}
			if ((int) FileWrite(logControl.logFile, "\n", strlen("\n"))
				!= strlen("\n"))
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not write to file '%s' data '%p' dataLength '%u' : %m",
								logControl.logOptions.logPath,
								logMessage,
								(int) strlen(logMessage))));

			}
		}

	}
	va_end (args);

}

/* Allocate memory and initialise the global request structure.*/
void FileRepPrimary_InitVerificationRequest(
	FileRepVerifyRequest_s * request)
{
}

/****************************************************************
 * FILEREP SUB-PROCESS (FileRep Primary VERIFICATION Process)
 ****************************************************************/
/*
 * FileRepPrimary_StartVerification
 * This will loop processing new requests for the life of the primary verification process
 */
void
FileRepPrimary_StartVerification(void)
{
	FileRepVerifyRequest_s	*request = &gFileRepVerifyShmem->request;

	/* we start out with stateTransition true because we are starting up the
	 * the process, that equals a state transition and as with other 
	 * state transitions it is possible that we have an old request that
	 * was suspended on shutdown that should be automatically resumed
	 */
	bool					stateTransition = true;   
	bool					started = false;
	uint32					count = 0;
	char					token[FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN];

	GpMonotonicTime			beginTime;

	FileRep_InsertConfigLogEntry("start verification");

	Insist(fileRepRole == FileRepPrimaryRole);

	while (1)
	{
		FileRepSubProcess_ProcessSignals();

		while (FileRepSubProcess_GetState() != FileRepStateShutdown &&
			   FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
			   ! (FileRepSubProcess_GetState() == FileRepStateReady &&
				  dataState == DataStateInSync))
		{

			/* During the running of the primary verificaion process we
			 * experienced a state transition that could have aborted an 
			 * in process request; Regardless after a state transition 
			 * there is no verification request currently in progress
			 */
			stateTransition = true;

			FileRepVerify_LockAcquire();
			if (gFileRepVerifyShmem->isVerificationInProgress)
			{
				ereport(LOG,
						(errmsg("verification failure, "
								"unexpected state during state transition verification token '%s' "
								"verification aborted",
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				writeFailedResultFile(request->logControl.logOptions.token);
				gFileRepVerifyShmem->isVerificationInProgress = false;

				clearFileRepVerifyArguments(&(request->fileRepVerifyArguments));

				gFileRepVerifyShmem->availableForReuse = true;

				request->state = FileRepVerifyRequestState_NONE;
			}
			FileRepVerify_LockRelease();

			FileRepSubProcess_ProcessSignals();
			pg_usleep(50000L); /* 50 ms */
		}

		/* if we are in the process of shutting down then get out of this
		 * infinite loop and let the primary verification process shutdown
		 */
		if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
			FileRepSubProcess_GetState() == FileRepStateShutdownBackends)
		{
			break;
		}

		/* Give the database time to get up and running */
		while (!isDatabaseRunning())
		{
			pg_usleep(50000L); /* 50 ms */
		}

		/* If we just had a state transition ( either starting up for the first
		 * time or a state transition while we are running) then we need check 
		 * if we have an inprogress token file indicating there was
		 * an inprocess token that was suspended (but not by a user, just because
		 * of the state transition) - if so that request should be automatically 
		 * resumed
		 */
		if (stateTransition)
		{
			if (FileRepVerify_GetInProgressTokenFromFile(token))
			{
				FileRepVerify_LockAcquire();

				/* if this fires then we have a request in progress
				 * and one that needs to be automatically
				 * resumed
				 */
				Assert(! gFileRepVerifyShmem->isVerificationInProgress);

				/* if any new requests have arrived abort those
				 * because the suspended request gets first 
				 * priority and we don't allow multiple in progress
				 */
				if (gFileRepVerifyShmem->isNewRequestWaiting)
				{
					ereport(WARNING,
							(errmsg("verification failure, "
									"request '%s' aborted because inprogress request '%s' has priority "
									"verification aborted",
									request->logControl.logOptions.token, token),
							 errhint("rerun new request when inprogress request is complete")));

					writeFailedResultFile(request->fileRepVerifyArguments.token);

					gFileRepVerifyShmem->isNewRequestWaiting = false;

					clearFileRepVerifyArguments(&(request->fileRepVerifyArguments));

					/* why wouldn't we leave availableForReuse == false
					 * and then immediately restart the inprogres request?
					 * i.e. no "continue" and no LockRelease below
					 */
					gFileRepVerifyShmem->availableForReuse = true;

					request->state = FileRepVerifyRequestState_NONE;

					FileRepVerify_LockRelease();

					continue;
				}

				gFileRepVerifyShmem->isVerificationInProgress = true;

				gFileRepVerifyShmem->availableForReuse = false;

				FileRepVerify_LockRelease();

				gp_set_monotonic_begin_time(&beginTime);

				strcpy(request->fileRepVerifyArguments.token, token);

				started = FileRepPrimary_StartRequest(
													  request,
													  true,
													  true);

				ereport(LOG,
						(errmsg("verification state '%s' result '%s' token '%s' "
								"elapsed time ' " INT64_FORMAT " ' miliseconds  "
								"data to verify '%0.2f' GB data verified '%0.2f' GB ",
								FileRepVerify_GetVerifyRequestState(),
								(started) ? "succeeded" : "failed",
								request->fileRepVerifyArguments.token,
								gp_get_elapsed_ms(&beginTime),
								FileRepVerify_GetTotalGBsToVerify(),
								FileRepVerify_GetTotalGBsVerified())));

				FileRepVerify_LockAcquire();

				gFileRepVerifyShmem->isVerificationInProgress = false;

				if (started)
				{
					count++;
					request->numAutoRestarts++;
				}

				switch (request->state)
				{
					/* Ok to have FileRepVerifyRequestState_FAILED here?
					 *  will automatic suspend work if it is here?
					 */
					case FileRepVerifyRequestState_ABORTED:
						/* SYNCHRONOUSABORTSUSPEND
						 * signal the waiter for the current request to finish
						 * synchronously
						 */
						gFileRepVerifyShmem->signalSynchronousRequests = true;
				



					case FileRepVerifyRequestState_FAILED:
					case FileRepVerifyRequestState_COMPLETE:

						clearFileRepVerifyArguments(&(request->fileRepVerifyArguments));

						gFileRepVerifyShmem->availableForReuse = true;

						request->state = FileRepVerifyRequestState_NONE;
						break;

					case FileRepVerifyRequestState_SUSPENDED:
						/* SYNCHRONOUSABORTSUSPEND
						 * signal the waiter for the current request to finish
						 * synchronously
						 */
						gFileRepVerifyShmem->signalSynchronousRequests = true;
						break;

					default:
						Assert(0);
						break;
				}

				FileRepVerify_EmptyInProgressTokenFile();

				FileRepVerify_LockRelease();

				stateTransition = false;
			}
			else
			{
				FileRepVerify_LockAcquire();

				clearFileRepVerifyArguments(&(request->fileRepVerifyArguments));

				gFileRepVerifyShmem->availableForReuse = true;

				request->state = FileRepVerifyRequestState_NONE;

				FileRepVerify_EmptyInProgressTokenFile();

				FileRepVerify_LockRelease();

				stateTransition = false;
			}
		}

		/* Check if a new gpverify request has arrived */
		FileRepVerify_LockAcquire();

		if (! gFileRepVerifyShmem->isVerificationInProgress &&
		     gFileRepVerifyShmem->isNewRequestWaiting)
		{
			gFileRepVerifyShmem->isVerificationInProgress = true;
			gFileRepVerifyShmem->isNewRequestWaiting = false;
			gFileRepVerifyShmem->availableForReuse = false;

			FileRepVerify_LockRelease();

			gp_set_monotonic_begin_time(&beginTime);

			started = FileRepPrimary_StartRequest(
												  request,
												  gFileRepVerifyShmem->resume,
												  false);

			ereport(LOG,
					(errmsg("verification state '%s' result '%s' token '%s' "
							"elapsed time ' " INT64_FORMAT " ' miliseconds  "
							"data to verify '%0.2f' GB data verified '%0.2f' GB ",
							FileRepVerify_GetVerifyRequestState(),
							(started) ? "succeeded" : "failed",
							request->fileRepVerifyArguments.token,
							gp_get_elapsed_ms(&beginTime),
							FileRepVerify_GetTotalGBsToVerify(),
							FileRepVerify_GetTotalGBsVerified())));

			FileRepVerify_LockAcquire();

			gFileRepVerifyShmem->isVerificationInProgress = false;

			if (started)
			{
				count++;
			}

			switch (request->state)
			{
				case FileRepVerifyRequestState_ABORTED:
				case FileRepVerifyRequestState_FAILED:
				case FileRepVerifyRequestState_COMPLETE:

					clearFileRepVerifyArguments(&(request->fileRepVerifyArguments));

					gFileRepVerifyShmem->availableForReuse = true;

					request->state = FileRepVerifyRequestState_NONE;
					break;

				case FileRepVerifyRequestState_SUSPENDED:
					break;

				default:
					Assert(0);
					break;
			}
		}

		FileRepVerify_LockRelease();

		pg_usleep(1000L*1000L*1L); /* 1 second */

	} // while(1)
}

void
writeFailedResultFile(char *token)
{
	char		filename[MAXPGPATH+1];
	struct stat	st;
	File		f;
#define MAX_RESULT 500
	char	result[MAX_RESULT+1];

	errno = 0;
	if (stat(VERIFYDIR, &st) < 0)
	{
		if (mkdir(VERIFYDIR, 0700) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not create directory '%s' verification token '%s' : %m "
							"verification aborted, "
							"results not reported",
							VERIFYDIR,
							token),
					 errhint("address file system failure and run gpverify")));
		}
	}

	snprintf(filename,
			 MAXPGPATH+1,
			 "%s/verification_%s.result",
			 VERIFYDIR,
			 token);

	f = PathNameOpenFile(
						 filename,
						 O_RDWR | O_CREAT | O_TRUNC | PG_BINARY,
						 S_IRUSR | S_IWUSR );

	if (f < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' verification token '%s' : %m "
						"verification aborted, "
						"results not reported",
						(filename == NULL) ? "<null>" : filename,
						token),
				 errhint("run gpverify")));

		return;
	}

	snprintf(result, 
			 MAX_RESULT,
			 "FAILED\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
			 timestamptz_to_str(GetCurrentTimestamp()),
			 0.0
		);


	if ((int) FileWrite(f, result, strlen(result)) != strlen(result))
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not write to file '%s' verification token '%s' : %m "
						"verification aborted, "
						"results not reported",
						(filename == NULL) ? "<null>" : filename,
						token),
				 errhint("run gpverify")));
	}
	FileClose(f);
}

bool
FileRepPrimary_StartRequest(
							FileRepVerifyRequest_s	*request,
							bool					resume,
							bool					autoResume)
{
	long		secs = 0;
	int			microsecs = 0;
	int			status = STATUS_OK;
	TimestampTz resumeTime;
#define MAX_RESULT 500
	char	result[MAX_RESULT+1];
	bool	success = false;

	if (resume)
	{

		status = FileRepPrimary_RestoreVerificationRequest(
														   request,
														   request->fileRepVerifyArguments.token);

		if (status != STATUS_OK)
		{
			verifyLog(request->logControl,
					  LOG,
					  "RestoreVerificationRequest failed %d",
					  status);

		
			goto FileRepPrimary_StartRequest_Return;
		}

		if (autoResume &&
			(request->numAutoRestarts >= MAX_AUTO_RESTARTS))
		{
			status = STATUS_ERROR;

			verifyLog(request->logControl,
					  LOG,
					  "numAutoRestarts exceeded; retry number %d",
					  request->numAutoRestarts);
			
			ereport(WARNING,
					(errmsg("verification failure, "
							"could not resume verification token '%s' retry number '%d' "
							"verification aborted",
							request->fileRepVerifyArguments.token,
							request->numAutoRestarts),
					 errhint("run gpverify")));

			goto FileRepPrimary_StartRequest_Return;
		}

		status = FileRep_OpenOutputFiles(&(request->logControl), true);
		
		if (status != STATUS_OK)
		{
			return status;
		}
	}
	else
	{
		request->startTime = GetCurrentTimestamp();
		gettimeofday(&(gFileRepVerifyShmem->request.startVerificationTime), NULL);

		printFileRepVerifyArguments(request->fileRepVerifyArguments, LOG);
		gFileRepVerifyShmem->request.compareStats.totalBytesToVerify = 0;
		gFileRepVerifyShmem->request.compareStats.totalBytesVerified = 0;
		request->logControl.filesAreOpen = false;

		status = FileRepPrimary_BuildRequest(request);

		if (status != STATUS_OK)
		{
			verifyLog(request->logControl,
					  LOG,
					  "FAILED_TO_ISSUE: BuildRequest failed %d",
					  status);
			goto FileRepPrimary_StartRequest_Return;
		}
		printFileRepVerifyRequest(*request, LOG);
	}

	Assert(request->logControl.filesAreOpen);

	if (request->fileRepVerifyArguments.abort ||
		request->fileRepVerifyArguments.suspend)
	{
		status = STATUS_ERROR;
		/* request to abort or suspend doesn't do anything when
		 * nothing in progress
		 */
		ereport(WARNING,
				(errmsg("verification failure, "
						"no verification in progress")));

		verifyLog(request->logControl,
				  WARNING,
				  "nothing to abort or suspend!!; no request in progress");
		goto FileRepPrimary_StartRequest_Return;
	}

	if (resume)
	{

		resumeTime = GetCurrentTimestamp();
		verifyLog(request->logControl,
				  LOG,
				  "RESUME_TIME %s",
				  timestamptz_to_str(resumeTime));
	}
	else
	{
		verifyLog(request->logControl,
				  LOG,
				  "START_TIME %s",
				  timestamptz_to_str(request->startTime));
		FileRepVerify_CreateInProgressTokenFile(request->logControl.logOptions.token);
	}

	if (!resume)
	{
		/* this will count up the number of compares/amount of data to
		 * cover in this request
		 */
		status = FileRepPrimary_EstimateVerificationRequest(request);

		if (status != STATUS_OK)
		{
			verifyLog(request->logControl,
					  LOG,
					  "FAILURE UNABLE TO ESTIMATE WORK FOR REQUEST %d",
					  status);
			/* should we drop down to execute the request anyway?
			 * set totalToCompare to -1 = UNKNOWN
			 * this avoids divide by 0
			 */
			request->compareStats.estimateTotalCompares = -1;
			goto FileRepPrimary_StartRequest_Return;
		}

		verifyLog(request->logControl,
				  LOG,
				  "ESTIMATE COMPLETE total compares TODO %d",
				  request->compareStats.estimateTotalCompares);
		printVerificationStatus(LOG, request);

	}
	/* Execute the requested verification operation*/
	if ((!request->estimateOnly) &&
		(request->compareStats.estimateTotalCompares > 0))
	{
		status = FileRepPrimary_ExecuteVerificationRequest(
														   request,
														   resume);
		if (status != STATUS_OK)
		{
			bool continueRequest = TRUE;

			verifyLog(request->logControl,
					  LOG,
					  "ExeucteVerificationRequest returned error status %d",
					  status);

			/*
			 * The following events are not treated as failure
			 *		a) transition to Change Tracking
			 *		b) suspend request
			 *		c) abort request
			 */
			FileRepPrimary_PeriodicTasks(request);

			continueRequest = FileRepPrimary_ContinueVerificationRequest(request);

			if (! continueRequest)
			{

				verifyLog(request->logControl,
						  LOG,
						  "ContinueVerificationRequest false");

				status = STATUS_OK;
			}
		}
	}
	else
	{
		verifyLog(request->logControl,
				  LOG,
				  "verification estimate only - no execution required");
	}

	if (status != STATUS_OK)
	{

		/* could we return one code from ExecuteVerificationRequest for
		 * problem don't bother to try again and another for worth
		 * trying again
		 * or maybe test the primary/mirror state here
		 * if it is in sync then this is a real failure???
		 */
		request->endTime = GetCurrentTimestamp();
		TimestampDifference(
			request->startTime,
			request->endTime,
			&secs,
			&microsecs);
		verifyLog(request->logControl,
				  LOG,
				  "FAILED AFTER DURATION %ld secs %d microsecs",
				  secs,
				  microsecs);
		verifyLog(request->logControl,
				  LOG,
				  "FAILURE_TIME %s",
				  timestamptz_to_str(request->endTime));
	}

FileRepPrimary_StartRequest_Return:

	if (status != STATUS_OK)
	{
		request->state = FileRepVerifyRequestState_FAILED;

		verifyLog(request->logControl,
				  LOG,
				  "Request failed with status %d",
				  status);

		request->endTime = GetCurrentTimestamp();

		snprintf(result, 
				 MAX_RESULT,
				 "FAILED\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
				 timestamptz_to_str(request->endTime),
				 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
			);

		success = false;
	} else {


		if (request->state == FileRepVerifyRequestState_RUNNING ||
			request->compareStats.estimateTotalCompares == 0)
		{
			
			verifyLog(request->logControl,
					  LOG,
					  "REQUEST_COMPLETE");
			
			request->endTime = GetCurrentTimestamp();
			TimestampDifference(
				request->startTime,
				request->endTime,
				&secs,
				&microsecs);
			verifyLog(request->logControl,
					  LOG,
					  "DURATION %ld secs %d microsecs",
					  secs,
					  microsecs);
			verifyLog(request->logControl,
					  LOG,
					  "END_TIME %s",
					  timestamptz_to_str(request->endTime));
			
			verifyLog(request->logControl,
					  LOG,
					  "DATA_VERIFIED: %0.2f GB",
					  (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0));
			request->state = FileRepVerifyRequestState_COMPLETE;

			if (request->isFailed)
			{
				
				snprintf(result, 
						 MAX_RESULT,
						 "FAILED\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
						 timestamptz_to_str(request->endTime),
						 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
					);

				success = false;
			}
			else
			{

				snprintf(result, 
						 MAX_RESULT,
						 "SUCCESS\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
						 timestamptz_to_str(request->endTime),
						 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
					);

				success = true;
			}

		FileRepVerify_EmptyInProgressTokenFile();

		}
		else if (request->state == FileRepVerifyRequestState_ABORTED)
		{
			verifyLog(request->logControl,
					  LOG,
					  "REQUEST_ABORTED");
			
			request->endTime = GetCurrentTimestamp();
			TimestampDifference(
				request->startTime,
				request->endTime,
				&secs,
				&microsecs);
			verifyLog(request->logControl,
					  LOG,
					  "ABORTED AFTER DURATION %ld secs %d microsecs",
					  secs,
					  microsecs);
			verifyLog(request->logControl,
					  LOG,
					  "ABORT_TIME %s",
					  timestamptz_to_str(request->endTime));
			verifyLog(request->logControl,
					  LOG,
					  "DATA_VERIFIED: %0.2f GB",
					  (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0));
			
			verifyLog(request->logControl,
					  LOG,
					  "DATA_TO_VERIFY: %0.2f GB",
					  (float)(request->compareStats.totalBytesToVerify)/(1024.0*1024.0*1024.0));
			
			snprintf(result, 
					 MAX_RESULT,
					 "ABORT\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
					 timestamptz_to_str(request->endTime),
					 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
				);
			
			FileRepVerify_EmptyInProgressTokenFile();
			/* SYNCHRONOUSABORTSUSPEND
			 * signal the waiter for the current request to finish
			 * synchronously
			 */
			gFileRepVerifyShmem->signalSynchronousRequests = true;
			
		}
		else if (request->state == FileRepVerifyRequestState_SUSPENDED)
		{
			
			verifyLog(request->logControl,
					  LOG,
					  "REQUEST_SUSPENDED");
			verifyLog(request->logControl,
					  LOG,
					  "DATA_VERIFIED: %0.2f GB",
					  (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0));
			
			verifyLog(request->logControl,
					  LOG,
					  "DATA_TO_VERIFY: %0.2f GB",
					  (float)(request->compareStats.totalBytesToVerify)/(1024.0*1024.0*1024.0));
			

			snprintf(result, 
					 MAX_RESULT,
					 "SUSPEND\nSUSPEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
					 timestamptz_to_str(request->endTime),
					 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
				);

			
			/* SYNCHRONOUSABORTSUSPEND
			 * signal the waiter for the current request to finish
			 * synchronously
			 */
			gFileRepVerifyShmem->signalSynchronousRequests = true;
			
		}
		else if (request->state == FileRepVerifyRequestState_FAILED)
		{
			verifyLog(request->logControl,
					  LOG,
					  "REQUEST_FAILED");
			verifyLog(request->logControl,
					  LOG,
					  "DATA_VERIFIED: %0.2f GB",
					  (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0));
			
			verifyLog(request->logControl,
					  LOG,
					  "DATA_TO_VERIFY: %0.2f GB",
					  (float)(request->compareStats.totalBytesToVerify)/(1024.0*1024.0*1024.0));
			
			snprintf(result, 
					 MAX_RESULT,		
					 "FAILED\nEND_TIME: %s\nDATA_VERIFIED: %0.2f GB",
					 timestamptz_to_str(request->endTime),
					 (float)(request->compareStats.totalBytesVerified)/(1024.0*1024.0*1024.0)
				);

		}
	}
	
	errno = 0;
	
	if (!request->logControl.filesAreOpen || (request->logControl.resultsFile <= 0))
	{
		success = FALSE;
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' : %m "
						"verification aborted ",
						request->logControl.logOptions.resultsPath),
				 errhint("address file system failure and run gpverify")));

		return success;
	}

	if ((int) FileWrite(request->logControl.resultsFile, result, strlen(result)) != strlen(result))
	{
		success = FALSE;

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not write to file '%s' data '%s' verification token '%s' : %m "
						"verification aborted",
						request->logControl.logOptions.resultsPath,
						result,
						request->fileRepVerifyArguments.token),
				 errhint("run gpverify")));
	}
	else
	{
		if (FileSync(request->logControl.resultsFile) < 0)
		{
			success = FALSE;

			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not write to file '%s' data '%s' verification token '%s' : %m "
							"verification aborted",
							request->logControl.logOptions.resultsPath,
							result,
							request->fileRepVerifyArguments.token),
					 errhint("run gpverify")));
		}
	}
	FileRepPrimary_CleanRequest(request);
	FileRep_CloseOutputFiles(&(request->logControl), true);

	return success;
}

void
FileRepPrimary_CleanRequest(FileRepVerifyRequest_s * request)
{
	int i = 0;

	if (request->numIgnoreFilePatterns)
	{

		for (i=0; i< request->numIgnoreFilePatterns; i++)
		{
			if (request->ignoreFiles[i] != NULL)
			{
				pfree(request->ignoreFiles[i]);
			}
		}
		pfree(request->ignoreFiles);
	}
	request->numIgnoreFilePatterns =0;

	if (request->numIgnoreDirs)
	{

		for (i=0; i< request->numIgnoreFilePatterns; i++)
		{
			if (request->ignoreDirs[i] != NULL)
			{
				pfree(request->ignoreDirs[i]);
			}
		}
		pfree(request->ignoreDirs);
	}
	request->numIgnoreDirs =0;
}

/*
 * To build the request structure from the specified arguments
 */
int
FileRepPrimary_BuildRequest(FileRepVerifyRequest_s *request)
{
	FileRepVerifyArguments *args = &(request->fileRepVerifyArguments);
	int i = 0;
	int logLevel = 0;
	int status = 0;

	request->allLogical = true;
	request->allPhysical = true;

	request->checkpointVersion = CHECKPOINT_VERSION_ID;
	request->lastCheckpointTime = 0;
	request->numAutoRestarts = 0;
	request->doHash = true;
	request->skipRelFiles = true;
	request->isFailed = false;
	request->state = FileRepVerifyRequestState_PENDING;
	FileRepPrimary_ClearCheckpoint(request);
	request->abortRequested = false;
	request->suspendRequested = false;

	memset(&(request->compareStats), 0, sizeof(FileRepVerify_CompareStats_s));

	request->estimateOnly = args->estimateOnly;

	request->logControl.filesAreOpen = false;
	logLevel = atoi(args->logLevel);
	if ((logLevel>= 0) && (logLevel <=10))
	{
		request->logControl.logOptions.level = ERROR-logLevel;
	}
	else
	{
		request->logControl.logOptions.level = ERROR;
	}
	elog(DEBUG3,
		 "logControl.logOptions.level %d",
		 request->logControl.logOptions.level);

	if (strlen(args->token) != 0)
	{
		/* if a token is passed in use it*/
		strncpy(request->logControl.logOptions.token,
				args->token,
				FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN+1);
		/* should we add on a timestamp to any token passed in? */
	}
	else
	{
		//requests without a token shouldn't even get this far
		return STATUS_ERROR;
	}

	status = FileRep_OpenOutputFiles(&(request->logControl), true);

	if (status != STATUS_OK)
	{
		return status;
	}



	/* set a default for testing? */
	request->requestType = FileRepVerifyRequestType_FULL;

	if (args->fullVerify)
	{
		request->requestType = FileRepVerifyRequestType_FULL;
		verifyLog(request->logControl, LOG, "Full Verify");
	}
	else
	{
		verifyLog(request->logControl, LOG, "Partial Verify");
		if (strlen(args->directoryTree) != 0)
		{
			if (!checkDirectoryInDB(args->directoryTree))
			{
				verifyLog(request->logControl,
						  WARNING,
						  "This directory %s does not exist in the DB - invalid verify request",
						  args->directoryTree);

				ereport(WARNING,
						(errmsg("invalid verification request, "
								"could not find directory '%s' verification token '%s' "
								"verification aborted",
								args->directoryTree,
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			request->requestType = FileRepVerifyRequestType_DIRECTORY;
			strncpy(request->verifyItem,
					args->directoryTree,
					strlen(args->directoryTree)+1);
			verifyLog(request->logControl,
					  LOG, "IncludeDirectory %s",
					  request->verifyItem);
		}

		if (strlen(args->compareFile) != 0)
		{

			if (!checkFileInDB(args->compareFile))
			{
				verifyLog(request->logControl,
						  WARNING,
						  "This file %s does not exist in the DB - invalid verify request",
						  args->compareFile);

				ereport(WARNING,
						(errmsg("invalid verification request, "
								"could not find file '%s' verification token '%s' "
								"verification aborted",
								args->compareFile,
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			request->requestType = FileRepVerifyRequestType_FILE;
			strncpy(request->verifyItem,
					args->compareFile,
					strlen(args->compareFile)+1);
			verifyLog(request->logControl,
					  LOG,
					  "IncludeFile %s", request->verifyItem);
		}
	}

	/* NOTE: pallocs of the file and dir ignore patterns
	 *  are freed when the request is complete in
	 *  FileRepPrimary_CleanRequest
	 */
	request->numIgnoreFilePatterns =0;
	if (strlen(args->ignorePattern) != 0)
	{
		int thisStart, numChars, whichPattern;
		request->numIgnoreFilePatterns =1;
		for (i=0; i< strlen(args->ignorePattern); i++)
		{
			if (args->ignorePattern[i] == ',')
			{
				request->numIgnoreFilePatterns++;
			}
		}

		if (request->numIgnoreFilePatterns)
		{
			request->ignoreFiles = (char **) palloc(request->numIgnoreFilePatterns * sizeof(char *));

			if (request->ignoreFiles == NULL)
			{
				request->numIgnoreFilePatterns = 0;

				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to start verification token '%s' "
								"verification aborted",
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}
			else
			{
				for (i=0; i< request->numIgnoreFilePatterns; i++)
				{
					request->ignoreFiles[i] = NULL;
				}
			}
			thisStart =0;
			numChars = 0;
			whichPattern = 0;
			for (i=0; i< strlen(args->ignorePattern); i++)
			{

				if (args->ignorePattern[i] == ',')
				{

					request->ignoreFiles[whichPattern] = (char *) palloc((numChars+1) * sizeof(char));
					if (request->ignoreFiles[whichPattern] == NULL)
					{
						ereport(WARNING,
								(errmsg("verification failure, "
										"could not allocate memory to start verification token '%s' "
										"verification aborted",
										request->logControl.logOptions.token),
								 errhint("run gpverify")));

						return STATUS_ERROR;
					}
					strncpy(request->ignoreFiles[whichPattern],
							&(args->ignorePattern[thisStart]),
							numChars);
					request->ignoreFiles[whichPattern][numChars] = '\0';
					thisStart = i+1;
					whichPattern++;
					numChars = 0;
				}
				else if (i == strlen(args->ignorePattern) -1)
				{
					request->ignoreFiles[whichPattern] = (char *) palloc((numChars+2) * sizeof(char));
					if (request->ignoreFiles[whichPattern] == NULL)
					{
						ereport(WARNING,
								(errmsg("verification failure, "
										"could not allocate memory to start verification token '%s' "
										"verification aborted",
										request->logControl.logOptions.token),
								 errhint("run gpverify")));

						return STATUS_ERROR;
					}
					strncpy(request->ignoreFiles[whichPattern],
							&(args->ignorePattern[thisStart]),
							numChars+1);
					request->ignoreFiles[whichPattern][numChars+1] = '\0';
				}
				else
				{
					numChars++;
				}
			}
		}
	}
	verifyLog(request->logControl, LOG,
			  "numIgnoreFilePatterns %d",
			  request->numIgnoreFilePatterns);
	for (i=0; i< request->numIgnoreFilePatterns; i++)
	{
		verifyLog(request->logControl, LOG,
				  "numIgnoreFilePattern %d %s",
				  i,
				  request->ignoreFiles[i]);
	}

	request->numIgnoreDirs =0;
	if (strlen(args->dirIgnorePath) != 0)
	{
		int thisStart, numChars, whichDir;
		request->numIgnoreDirs =1;
		for (i=0; i< strlen(args->dirIgnorePath); i++)
		{
			if (args->dirIgnorePath[i] == ',')
			{
				request->numIgnoreDirs++;
			}
		}

		if (request->numIgnoreDirs)
		{
			request->ignoreDirs = (char **) palloc(request->numIgnoreDirs * sizeof(char *));
			if (request->ignoreDirs == NULL)
			{
				request->numIgnoreDirs = 0;

				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to start verification token '%s' "
								"verification aborted",
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}
			else
			{
				for (i=0; i< request->numIgnoreDirs; i++)
				{
					request->ignoreDirs[i] = NULL;
				}
			}
			thisStart =0;
			numChars = 0;
			whichDir = 0;
			for (i=0; i< strlen(args->dirIgnorePath); i++)
			{

				if (args->dirIgnorePath[i] == ',')
				{
					request->ignoreDirs[whichDir] = (char *) palloc((numChars+1) * sizeof(char));
					if (request->ignoreDirs[whichDir] == NULL)
					{
						ereport(WARNING,
								(errmsg("verification failure, "
										"could not allocate memory to start verification token '%s' "
										"verification aborted",
										request->logControl.logOptions.token),
								 errhint("run gpverify")));

						return STATUS_ERROR;
					}

					strncpy(request->ignoreDirs[whichDir],
							&(args->dirIgnorePath[thisStart]),
							numChars);
					request->ignoreDirs[whichDir][numChars] = '\0';
					thisStart = i+1;
					numChars = 0;
					whichDir++;
				}
				else if (i == strlen(args->dirIgnorePath) -1)
				{
					request->ignoreDirs[whichDir] = (char *) palloc((numChars+2)* sizeof(char));
					if (request->ignoreDirs[whichDir] == NULL)
					{
						ereport(WARNING,
								(errmsg("verification failure, "
										"could not allocate memory to start verification token '%s' "
										"verification aborted",
										request->logControl.logOptions.token),
								 errhint("run gpverify")));

						return STATUS_ERROR;
					}
					strncpy(request->ignoreDirs[whichDir],
							&(args->dirIgnorePath[thisStart]),
							numChars+1);
					request->ignoreDirs[whichDir][numChars+1] = '\0';
				}
				else
				{
					numChars++;
				}
			}
		}
	}
	verifyLog(request->logControl, LOG,
			  "numIgnoreDirs %d",
			  request->numIgnoreDirs);

	for (i=0; i< request->numIgnoreDirs; i++)
	{
		verifyLog(request->logControl, LOG,
				  "numIgnoreDir %d %s", i,
				  request->ignoreDirs[i]);
	}
	return STATUS_OK;
}

int
FileRepPrimary_VerifyUserFilespaces(
								   FileRepVerifyRequest_s	*request,
								   bool						resume)
{

	int		status = STATUS_OK;
	bool	found = true;
	FilespaceScanToken	  token;
	char	primaryFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char	mirrorFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char	*primaryFilespaceLocation = NULL;
	char	*mirrorFilespaceLocation = NULL;
	int16	primary_dbId = 0;
	int16	mirror_dbId = 0;
	PersistentFileSysState	persistentState;
	MirroredObjectExistenceState mirrorExistenceState;
	ItemPointerData	 persistentTid;
	int64	persistentSerialNum = 0;
	Oid		filespaceOid;
	int		numFilespaces = 0;
	FileRepVerifyLogControl_s logControl = request->logControl;
	bool	keepFilespace = false;

	request->restartInfo.filespacesComplete = false;
	request->restartInfo.physicalCheckpoint_persistentSerialNum = -1;
	request->restartInfo.haveWrittenIgnoreInfo = false;

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepPrimary_VerifyUserFilespaces START ");

	numFilespaces = 0;
	FilespaceScanToken_Init(&token);

	do
	{
		/*
		 *  Walk and compare the filespace directories
		 *  between primary and mirror segment
		 */
		found = PersistentFileSysObj_FilespaceScan(
												   &token,
												   &filespaceOid,
												   &primary_dbId,
												   primaryFilespaceLocationBlankPadded,
												   &mirror_dbId,
												   mirrorFilespaceLocationBlankPadded,
												   &persistentState,
												   &mirrorExistenceState,
												   &persistentTid,
												   &persistentSerialNum);

		if (found)
		{
			if (resume &&
				!(request->restartInfo.useLogical) &&
				(persistentSerialNum <=
				 request->restartInfo.physicalCheckpoint_persistentSerialNum))
			{
				/* we've already done this one - skip over it*/
				continue;
			}

			numFilespaces++;

			PersistentFilespace_ConvertBlankPaddedLocation(
														   &primaryFilespaceLocation,
														   primaryFilespaceLocationBlankPadded,
														   /* isPrimary */ true);
			PersistentFilespace_ConvertBlankPaddedLocation(
														   &mirrorFilespaceLocation,
														   mirrorFilespaceLocationBlankPadded,
														   /* isPrimary */ false);

			keepFilespace = keepItem(
									 primaryFilespaceLocation,
									 false,request->verifyItem,
									 request->requestType);

			if (keepFilespace)
			{
				verifyLog(logControl, DEBUG1,
						  "Keeping FILESPACE %d oid %d primary id %d |%s| strlen %d mirror id %d |%s| strlen %d",
						  numFilespaces,
						  filespaceOid,
						  primary_dbId,
						  primaryFilespaceLocation,
						  (int)strlen(primaryFilespaceLocation),
						  mirror_dbId,
						  mirrorFilespaceLocation,
						  (int) strlen(mirrorFilespaceLocation));

				status = FileRepPrimary_VerifyDirectory(
														request,
														primaryFilespaceLocation,
														mirrorFilespaceLocation,
														resume);
			}
			else
			{

				verifyLog(logControl, DEBUG1,
						  "Skipping FILESPACE %d oid %d primary id %d |%s| strlen %d mirror id %d |%s| strlen %d",
						  numFilespaces,
						  filespaceOid,
						  primary_dbId,
						  primaryFilespaceLocation,
						  (int)strlen(primaryFilespaceLocation),
						  mirror_dbId,
						  mirrorFilespaceLocation,
						  (int) strlen(mirrorFilespaceLocation));
			}

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);
			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);

			if (status != STATUS_OK)
			{
				/* should we abandon the cursor?
				 * should we continue to any other filespaces?
				 */
				verifyLog(logControl, LOG,
						  "ERROR in FileRepPrimary_VerifyDirectory for directory %s status %d\n",
						  primaryFilespaceLocation,
						  status);
				return status;
			}

			request->restartInfo.physicalCheckpoint_persistentSerialNum = persistentSerialNum;

			/* JEANNA how the state will get changed, Periodic check was not issued */
			if (request->state != FileRepVerifyRequestState_RUNNING)
			{
				verifyLog(logControl, LOG,
						  "FileRepPrimary_VerifyUserFilespaces: Request is being stopped %d\n");

				return status;
			}
		}
	} while (found);

	verifyLog(logControl, DEBUG1,
			  "FileRepPrimary_VerifyUserFilespaces numFilespaces %d END ",
			  numFilespaces);
	request->restartInfo.filespacesComplete = true;

	return STATUS_OK;
}

int
FileRepPrimary_EstimateDirectory(
	FileRepVerifyRequest_s * request,
	char * primaryBaseDir)
{
	int status = STATUS_OK;
	char  whereNextParent[MAXPGPATH+1];
	char  whereNextChild[MAXPGPATH+1];

	bool justCount = true;

	bool complete = false;

	uint32 toCompareThisDirectory = 0;



	whereNextParent[0] = '\0';
	whereNextChild[0] = '\0';



	/*
	 *  TODO - just like count num_entries could split into
	 *  num_files and num_directories - that would be a nice statistic
	 *  TODO- FOR PHYSICAL COMPARE IF NOT LOOKING AT FILE SIZE OR
	 *  HASH OF CONTENTS THEN CAN WE JUST SEND DIRECTORIES -
	 *  NO FILE SUMMARIES AT ALL?
	 */
	status = FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing(
		request,
		primaryBaseDir,
		NULL,
		NULL,
		0,
		&toCompareThisDirectory,
		justCount,
		&complete,
		whereNextParent,
		whereNextChild);
	if (status != STATUS_OK)
	{
		verifyLog(request->logControl,
				  WARNING,
				  "VERIFICATION_ERROR_INTERNAL: Unable to count up the number we need to process");
		return status;

	}
	else
	{
		request->compareStats.estimateTotalCompares += toCompareThisDirectory;
	}
	/*
	 * should be able to just count in one pass -
	 *  no limited buffer size to make us return early
	 */
	Assert( complete == true);

	return STATUS_OK;
}


int
FileRepPrimary_VerifyDirectory(
	FileRepVerifyRequest_s * request,
	char *	primaryBaseDir,
	char *	mirrorBaseDir,
	bool resume)
{
	bool justCount = false;

	char  whereNextParent[MAXPGPATH+1];
	char  whereNextChild[MAXPGPATH+1];
	bool complete = false;
	int status = STATUS_OK;
	char  listingBuffer[FILEREP_MESSAGEBODY_LEN];
	uint32 bufferSizeBytes= FILEREP_MESSAGEBODY_LEN;

	FileRepOperationDescription_u desc;
	uint32						responseDataLength = 0;
	void *						responseData = NULL;
	FileRepVerifySummaryItem_s * pItem = NULL;

	uint32 numLeftToSend = 0;
	uint32 num_entries = 0;
	uint32 whichBuf=0;

	uint32 maxSummaryItemsPerSend = 0;

	int numMismatches = 0;

	FileRepIdentifier_u		fileRepIdentifier;
	FileRepRelationType_e	fileRepRelationType;
	bool continueRequest = true;

	memset(&fileRepIdentifier, 0, sizeof(FileRepIdentifier_u));

	if (resume)
	{
		strncpy( whereNextParent,
				 request->restartInfo.physicalCheckpoint_whereNextParent,
				 MAXPGPATH+1);
		strncpy( whereNextChild,
				 request->restartInfo.physicalCheckpoint_whereNextChild,
				 MAXPGPATH+1);
		verifyLog(request->logControl, LOG,
				  "Resuming from parent %s child %s",
				  whereNextParent, whereNextChild);

	}
	else
	{
		whereNextParent[0] = '\0';
		whereNextChild[0] = '\0';
	}
	maxSummaryItemsPerSend =
		FILEREP_MESSAGEBODY_LEN/sizeof(FileRepVerifySummaryItem_s);
	verifyLog(request->logControl,
			  DEBUG5,
			  "Max summary items per page of directory listing %d",
			  maxSummaryItemsPerSend);
	verifyLog(request->logControl,
			  DEBUG5, "Space left over: %lu bytes ",
			  FILEREP_MESSAGEBODY_LEN%sizeof(FileRepVerifySummaryItem_s));

	do
	{

		/* here we really want to do it not just count */
		justCount = false;

		/*
		 *  TODO -
		 *  just like count num_entries could split into num_files
		 *  and num_directories - that would be a nice statistic
		 *  //TODO- FOR PHYSICAL COMPARE IF NOT LOOKING AT FILE SIZE
		 *  OR HASH OF CONTENTS THEN CAN WE JUST SEND DIRECTORIES -
		 *  NO FILE SUMMARIES AT ALL?
		 *  //THAT WOULD MEAN LOGICAL JUST DOES FILES AND PHYSICAL
		 *  JUST DOES DIRECTORIES?  CATALOG CAN DO ALL FLAT FILES
		 *  WE CARE ABOUT?
		 *  //IF CHANGE FillBufferWithDirectoryTreeListing to just
		 *  have summary items for diretories
		 */
		status = FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing(
			request,
			primaryBaseDir,
			mirrorBaseDir,
			listingBuffer,
			bufferSizeBytes,
			&num_entries,
			justCount,
			&complete,
			whereNextParent,
			whereNextChild);
		if (status != STATUS_OK)
		{
			/*
			 * right way to respond in case of error?
			 * FileRepSubProcess_SetState(FileRepStateFault);
			 * should we do anything respond that could not
			 * complete verification?
			 */
			verifyLog(request->logControl,
					  WARNING,
					  "VERIFICATION_ERROR_INTERNAL: Failure of FileRepVerify_ComputeSummaryItemsFromRecursuveDirectoryListing %d",
					  status);
			return status;

		}

		verifyLog(request->logControl,
				  DEBUG5,
				  "listingBuffer %p, address of listingBuffer %p bufferSizeBytes %d",
				  listingBuffer,
				  &listingBuffer,
				  bufferSizeBytes);
		verifyLog(request->logControl,
				  DEBUG5,
				  "listingBuffer[0] %c, listingBuffer[bufferSizeBytes-1] %c",
				  listingBuffer[0],listingBuffer[bufferSizeBytes-1]);


		numLeftToSend = num_entries;
		request->compareStats.comparesComplete += num_entries;
		whichBuf = 0;

		/*
		 *  TODO XXX - can take out this loop -
		 *  now will only ever go through it once
		 */
		Assert( num_entries <= maxSummaryItemsPerSend);


		while ( numLeftToSend >0)
		{

			uint32 numThisSend;
			char * dataToMirror = NULL;

			/* send the computed hash to the mirror along with a
			   message type that means HASH_WHOLE_DIRECTORY,
			   fileRepidentifier that indicates base directory
			*/
			if (numLeftToSend > maxSummaryItemsPerSend)
			{
				numThisSend = maxSummaryItemsPerSend;
			}
			else
			{
				numThisSend = numLeftToSend;
			}

			/* point send into the listingBuffer at the correct offset*/
			Assert(whichBuf == 0);
			dataToMirror = ((char *)listingBuffer)+
				(whichBuf*FILEREP_MESSAGEBODY_LEN);
/* 					Assert(dataToMirror >= listingBuffer); */

			desc.verify.type = FileRepOpVerifyType_SummaryItems;
			desc.verify.logOptions = request->logControl.logOptions;
			desc.verify.desc.summaryItems.hashcodesValid = request->doHash;
			desc.verify.desc.summaryItems.sizesValid =
				request->includeFileSizeInfo;
			desc.verify.desc.summaryItems.num_entries = numThisSend;

			verifyLog(request->logControl,
					  DEBUG3,
					  "FileRepPrimary_VerifySystemFilespace numThisSend %d of %d; whichBuf %d bufAddress %p",
					  numThisSend,
					  num_entries,
					  whichBuf,
					  dataToMirror);


			/*  what do I use as an fileRepIdentifier for
			 *  FileRepAckPrimary_NewHashEntry
			 *  fileRepIdentifier is currently set to the top level
			 *  directory will that work ok? No it will lock up on
			 *  the second send
			 * use id and type of first item in the buffer
			*/

			pItem = (FileRepVerifySummaryItem_s *)dataToMirror;

			snprintf(
					 fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
					 MAXPGPATH+1,
					 "%s/%s",
					 mirrorBaseDir,
					 pItem->fromBasePath);

			fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName[0] = '\0';

			fileRepIdentifier.fileRepFlatFileIdentifier.blockId = FILEREPFLATFILEID_WHOLE_FILE;

			fileRepRelationType = FileRepRelationTypeFlatFile;

			verifyLog(request->logControl,
					  DEBUG3,
					  "SEND SUMMARY ITEMS FileRepPrimary_VerifyDirectory Sending a batch (%d) of summary items to mirror", numThisSend);

			status = FileRepPrimary_MirrorVerify(
												 fileRepIdentifier,
												 fileRepRelationType,
												 desc,
												 dataToMirror,
												 FILEREP_MESSAGEBODY_LEN,
												 &responseData,
												 &responseDataLength,
												 &desc);

			if (status != STATUS_OK)
			{
				verifyLog(request->logControl,
							  WARNING,
							  "VERIFICATION_ERROR_INTERNAL: FileRepPrimary_MirrorVerify failed len %d dataToMirror %p",
							  FILEREP_MESSAGEBODY_LEN,
							  dataToMirror);

				return status;
			}

			numMismatches = desc.verify.desc.summaryItems.num_entries;
			request->compareStats.totalMismatches+=numMismatches;

			if (numMismatches == 0)
			{
				verifyLog(request->logControl,
						  DEBUG3,
						  "MIRROR SAYS NO MISMATCHES THIS BATCH");
			}
			else
			{

#if DO_ENHANCED_COMPARES
				int i = 0;
				FileRepVerifySummaryItem_s * itemArray
					= (FileRepVerifySummaryItem_s *)responseData;
				int numFiles =0, numDirs = 0;
				int numMismatchesResolved = 0;
#endif

				verifyLog(request->logControl,
						  DEBUG3,
						  "MIRROR SAYS %d of %d MISMATCHES THIS BATCH!!",
						  numMismatches,
						  numThisSend);
				Assert(responseData != NULL);
				Assert(responseDataLength > 0);
				Assert(responseDataLength == sizeof(FileRepVerifySummaryItem_s) * numMismatches);

#if DO_ENHANCED_COMPARES
				for (i=0; i < numMismatches; i++)
				{
					FileRepVerifySummaryItem_s * pItem = NULL;


					pItem = &(itemArray[i]);

					if(pItem->isFile)
					{
						FileRepRelFileNodeInfo_s relinfo;
						bool matched = false;

						char primaryFilename[MAXPGPATH+1];
						char mirrorFilename[MAXPGPATH+1];

						numFiles++;
						request->compareStats.totalFileMismatches++;

						strncpy(relinfo.primaryBaseDir,
								primaryBaseDir,
								MAXPGPATH+1);
						strncpy(relinfo.mirrorBaseDir,
								mirrorBaseDir,
								MAXPGPATH+1);
						strncpy(relinfo.fromBasePath,
								pItem->fromBasePath,
								MAXPGPATH+1);
						strncpy(relinfo.filename,
								pItem->filename,
								MAXPGPATH+1);


						snprintf(primaryFilename,
								 MAXPGPATH+1,
								 "%s/%s/%s",
								 relinfo.primaryBaseDir,
								 relinfo.fromBasePath,
								 relinfo.filename);

						snprintf(mirrorFilename,
								 MAXPGPATH+1,
								 "%s/%s/%s",
								 relinfo.mirrorBaseDir,
								 relinfo.fromBasePath,
								 relinfo.filename);



						writeDiffTableRecord(
							request->logControl,
							request->logControl.logOptions.token,
							"FileRepVerifyEnhancedFileCompareRequired",
							fileRepPrimaryHostAddress,
							primaryFilename, 
							fileRepMirrorHostAddress,
							mirrorFilename,
							-1,	-1,		-1, -1, -1,
							&(relinfo.relFileNode));

						/* relinfo.type =  FileRepRelationTypeFlatFile; */
						relinfo.relFileNodeValid = false;

						relinfo.exception = FileRepVerifyCompareException_None;

						request->compareStats.numEnhancedCompares++;

						status = FileRepPrimary_SendEnhancedBlockHashes(
																		relinfo,
																		&matched,
																		request->logControl);
						if (status != STATUS_OK)
						{
							if (responseData != NULL)
							{
								pfree(responseData);
								responseData = NULL;
							}

							FileRepPrimary_PeriodicTasks(request);

							continueRequest = FileRepPrimary_ContinueVerificationRequest(request);

							if (! continueRequest)
							{
								verifyLog(
										  request->logControl,
										  LOG,
										  "FileRepPrimary_VerifyAllRelFileNodes: Request %s is being aborted or suspended",
										  request->logControl.logOptions.token);

								return STATUS_OK;
							}
							else
							{
								ereport(WARNING,
										(errmsg("verification failure, "
												"unable to complete verification request with token '%s'"
												"verification aborted, ",
												request->logControl.logOptions.token),
										 errhint("run gpverify")));

								return status;
							}
						}

						if (!matched)
						{



							verifyLog(request->logControl,
									  LOG,
									  "FAILED ENHANCED COMPARE %s (%d of %d) - relnodeValid %d exception %d",
									  pItem->filename,
									  i,
									  numMismatches,
									  relinfo.relFileNodeValid,
									  relinfo.exception
								);

							gFileRepVerifyShmem->request.isFailed = true;

						}
						else
						{
							numMismatchesResolved++;
							request->compareStats.totalMismatchesResolved++;
							request->compareStats.totalFileMismatchesResolved++;
							verifyLog(request->logControl,
									  LOG,
									  "SUCCEEDED %s (%d of %d) Enhanced Compare (SendEnhancedBlockHashes) succeeded - mismatch resolved",
									  pItem->filename,
									  i,
									  numMismatches);
						}

					}
					else
					{
						char primaryDirectory[MAXPGPATH+1];
						int numChildrenAddedOnMirror = 0;

						memset(&fileRepIdentifier,
							   0,
							   sizeof(FileRepIdentifier_u));

						snprintf(primaryDirectory,
								 MAXPGPATH+1,
								 "%s/%s",
								 primaryBaseDir,
								 pItem->fromBasePath);

						snprintf(
							fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
							MAXPGPATH+1,
							"%s/%s",
							mirrorBaseDir,
							pItem->fromBasePath);

						writeDiffTableRecord(
							request->logControl,
							request->logControl.logOptions.token,
							"FileRepVerifyEnhancedDirectoryCompareRequired",
							fileRepPrimaryHostAddress,
							primaryDirectory, 
							fileRepMirrorHostAddress,
							fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
							-1,	-1,		-1, -1, -1,
							NULL /* Not applicable for directories */);

						fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName[0]
							= '\0';

						fileRepIdentifier.fileRepFlatFileIdentifier.blockId =
							FILEREPFLATFILEID_WHOLE_FILE;

							verifyLog(request->logControl,
								  LOG,
								  "MISMATCH on Directory %s use CompareDirectoryListing to get more details...!",
								  primaryDirectory);

						numDirs++;
						request->compareStats.totalDirMismatches++;


						/*
						 *   call a more detailed verify routine to
						 *  detect missing files, added files etc
						 */
						request->compareStats.numEnhancedCompares++;

						/* So highlevel compare failed
						 *  but low level succeeded
						 *  this is not really an enhanced
						 *  compare that is more likely to
						 *  succeed - just details on the
						 *  mismatch - is there a way to make
						 *  CompareDirectoryListing an enhanced
						 *  compare - lock the directory etc?
						 *  but still this could happen because
						 *  there is no locking - so might
						 *  have been in flux during first
						 *  compare
						 */

						status = FileRepPrimary_CompareDirectoryListing(
							request,
							primaryDirectory,
							fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
							fileRepIdentifier,
							fileRepRelationType,
							&numChildrenAddedOnMirror,
							request->logControl);

						if (status != STATUS_OK)
						{
							/* put on retry list and continue? */
							if (responseData != NULL)
							{
								pfree(responseData);
								responseData = NULL;
							}
							return status;
						}

					

						if (numChildrenAddedOnMirror)
						{
							
							verifyLog(request->logControl,
									  LOG,
									  "!!MISMATCH:	%s (%d of %d) numChildrenAddedOnMirror %d ",
									  primaryDirectory,
									  i,
									  numMismatches,
									  numChildrenAddedOnMirror);

								gFileRepVerifyShmem->request.isFailed = true;
						}

					}

				}
				verifyLog(request->logControl,
						  LOG,
						  "Of the %d mismatches %d files, %d directories %d resolved",
						  numMismatches,
						  numFiles,
						  numDirs,
						  numMismatchesResolved);
#endif
				if (responseData != NULL)
				{
					pfree(responseData);
					responseData = NULL;
				}
			}

			numLeftToSend -= numThisSend;
			//???? whichBuf?????
			//whichBuf++;
		}

		verifyLog(request->logControl,
				  LOG,
				  "FileRepPrimary_VerifyDirectory  %d mismatches  (%d directory mismatch/ %d file mismatch) of %d total compares ",
				  request->compareStats.totalMismatches,
				  request->compareStats.totalDirMismatches,
				  request->compareStats.totalFileMismatches,
				  request->compareStats.comparesComplete);
		verifyLog(request->logControl,
				  LOG,
				  "FileRepPrimary_VerifyDirectory  %d mismatches resolved (%d file mismatched resolved)  ",
				  request->compareStats.totalMismatchesResolved,
				  request->compareStats.totalFileMismatchesResolved);

		printVerificationStatus(LOG, request);

		FileRepPrimary_SetPhysicalCheckpoint(
			request,whereNextParent,
			whereNextChild);

		FileRepPrimary_PeriodicTasks(request);

		continueRequest = FileRepPrimary_ContinueVerificationRequest(request);

	} while (!complete && continueRequest);

	if (!continueRequest)
	{
		verifyLog(request->logControl,
				  LOG,
				  "FileRepPrimary_VerifyDirectory: Request %s is being aborted or suspended",
				  request->logControl.logOptions.token);
		return STATUS_OK;
	}
	return STATUS_OK;
}

bool
keepItem(
	char * toMatch,
	bool isFile,
	char * toKeep,
	FileRepVerifyRequestType_e requestType)
{

	int toMatchLen = 0;
	int toKeepLen = 0;


	if (requestType ==	FileRepVerifyRequestType_FULL)
	{
		return true;
	}

	toMatchLen = strlen(toMatch);
	if (!isFile)
	{
		if (toMatch[toMatchLen-1] == '\\')
		{
			toMatchLen--;
		}
	}

	toKeepLen = strlen(toKeep);
	if (requestType ==	FileRepVerifyRequestType_DIRECTORY )
	{
		if (toKeep[toKeepLen-1] == '\\')
		{
			toKeepLen--;
		}
	}

	if (requestType ==	FileRepVerifyRequestType_DIRECTORY )
	{
		/* which is more deeply nested toMatch or toKeep? */

		if (toKeepLen >= toMatchLen)
		{
			/*
			 *  toKeep is more deeply nested
			 *  see if toKeep is a child of toMatch
			 */
			if (strncmp(toKeep, toMatch,
						toMatchLen) == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			/*
			 * toMatch is more deeply nested
			 * see if toMatch is a child of toKeep
			 */
			if (strncmp(toKeep, toMatch,
						toKeepLen) == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

	}
	else if  (requestType ==	FileRepVerifyRequestType_FILE )
	{
		if (isFile)
		{
			/*  we are matching a file against a file name -
			 *  so they must be exact
			 */
			if ( (toKeepLen == toMatchLen)
				 && (strncmp(toKeep, toMatch,
							 toKeepLen) == 0))
			{
				/* this file begins with this toMatch so it
				 * is in this filespace
				 */
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			/* we are matching a file against a diretory name
			 * want to know if file could be child of this directory
			 *
			 * is the beginning of this toMatch character the same
			 *  as the beginning of toKeep
			 */
			if (strncmp(toKeep, toMatch,toMatchLen) == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;

}
int
FileRepPrimary_EstimateUserFilespaces(
	FileRepVerifyRequest_s * request)
{
	int status = STATUS_OK;
	bool   found = true;
	FilespaceScanToken	  token;
	char   primaryFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char   mirrorFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char * primaryFilespaceLocation;
	//	char * mirrorFilespaceLocation;
	int16	 primary_dbId = 0;
	int16	 mirror_dbId = 0;
	PersistentFileSysState	persistentState;
	MirroredObjectExistenceState mirrorExistenceState;
	ItemPointerData	 persistentTid;
	int64		 persistentSerialNum = 0;
	Oid	   filespaceOid;

	verifyLog(request->logControl, DEBUG1,
			  "FileRepPrimary_EstimateUserFilespaces");

	FilespaceScanToken_Init(&token);
	do
	{

		/*
		 *  Walk and compare the filespace directories
		 *  between primary and mirror segment
		 */
		found = PersistentFileSysObj_FilespaceScan(
			&token,
			&filespaceOid,
			&primary_dbId,
			primaryFilespaceLocationBlankPadded,
			&mirror_dbId,
			mirrorFilespaceLocationBlankPadded,
			&persistentState,
			&mirrorExistenceState,
			&persistentTid,
			&persistentSerialNum);

		if (found)
		{
			PersistentFilespace_ConvertBlankPaddedLocation(
				&primaryFilespaceLocation,
				primaryFilespaceLocationBlankPadded,
				/* isPrimary */ true);

			if (keepItem(
					primaryFilespaceLocation,
					false,
					request->verifyItem,
					request->requestType) )
			{
				/* primary and mirror identifers are the same in these cases */
				verifyLog(request->logControl,
						  DEBUG3,
						  "Keeping filepsace %s",
						  primaryFilespaceLocation);

				status = FileRepPrimary_EstimateDirectory(
					request,
					primaryFilespaceLocation);

			}
			else
			{
				verifyLog(request->logControl,
						  DEBUG3,
						  "Skipping filespace %s",
						  primaryFilespaceLocation);

			}
			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);

			if (status != STATUS_OK)
			{
				/* should we abandon the cursor? */
				return status;
			}

		}
	} while (found);

	verifyLog(request->logControl,
			  LOG,
			  "AFTER EstimateUserFilespaces ESTIMATE total compares TODO %d",
			  request->compareStats.estimateTotalCompares);
	return STATUS_OK;
}

int
FileRepPrimary_EstimateSystemFilespace(
	FileRepVerifyRequest_s * request)
{
	FileRepVerifyLogControl_s logControl = request->logControl;
	bool keepBaseDir;
	int status = STATUS_OK;

	char primaryBaseDirectory[MAXPGPATH+1];

	/*
	 *  start with the parent directory above base and global ( e.g. filerep0)
	 *  start with "." on both primary and mirror
	 */

	verifyLog(request->logControl, DEBUG1,
			  "FileRepPrimary_EstimateSystemFilespace");

	getcwd(primaryBaseDirectory, MAXPGPATH+1);

	keepBaseDir = keepItem(
		primaryBaseDirectory,
		false,
		request->verifyItem,
		request->requestType);
	if (keepBaseDir)
	{
		status = FileRepPrimary_EstimateDirectory(
			request,
			primaryBaseDirectory);

		if (status != STATUS_OK)
		{
			return status;
		}
	}
	else
	{
		verifyLog(logControl, LOG,
				  "Base dir %s not in the set of things to compare",
				  primaryBaseDirectory);
	}
	/* Rewrite as % complete */
	verifyLog(request->logControl,
			  LOG,
			  "AFTER EstimateSystemFilespace ESTIMATE total compares TODO %d",
			  request->compareStats.estimateTotalCompares);

	if (request->compareStats.estimateTotalCompares ==0)
	{
		verifyLog(logControl,
				  WARNING,
				  "VERIFCATION_ERROR_WARNING: No items to compare");
		return STATUS_OK;

	}
	return STATUS_OK;

}

int
FileRepPrimary_VerifySystemFilespace(
	FileRepVerifyRequest_s * request, bool resume)
{

	FileRepVerifyLogControl_s logControl = request->logControl;
	char primaryBaseDirectory[MAXPGPATH+1];
	bool keepBaseDir = true;
	int status = STATUS_OK;


	verifyLog(logControl,
			  DEBUG1,
			  "FileRepPrimary_VerifySystemFilespace START ");



	/*
	 *  start with the parent directory above base and global ( e.g. filerep0)
	 *  i.e. start with "." on both primary and mirror
	 */
	/* TODO: Need to get back from mirror what is the pathname of its system filespace, make_database_relative, appends DatabasePath
	 * Need to send request to mirror and have mirror answer with its DatabasePath or getcwd
	 */
	getcwd(primaryBaseDirectory, MAXPGPATH+1);
	verifyLog(logControl,LOG,
			  "FileRepPrimary_VerifySystemFilespace: primaryBaseDirectory is %s\n",
			  primaryBaseDirectory);
	/*
	 *  parentDirectory holds the primaryBaseDirectory -
	 *  what about mirror? call it "."
	 *  we used to call the primaryBaseDirectory "." also but to compare
	 *  to request->verifyItem we need a full path
	 */
	keepBaseDir = keepItem(
		primaryBaseDirectory,
		false,
		request->verifyItem,
		request->requestType);
	if (keepBaseDir)
	{
		status = FileRepPrimary_VerifyDirectory(
			request,
			primaryBaseDirectory,
			gMirrorBaseDir,
			resume);
		if (status != STATUS_OK)
		{
			verifyLog(logControl, LOG,
					  "ERROR in FileRepPrimary_VerifyDirectory for top level directory status %d\n",
					  status);
			return status;
		}
	}
	else
	{
		verifyLog(logControl, LOG,
				  "Base dir %s not in the set of things to compare",
				  primaryBaseDirectory);
	}

	if(request->state != FileRepVerifyRequestState_RUNNING)
	{
		verifyLog(
			logControl, LOG,
			"FileRepPrimary_VerifySystemFilespace: Request is being stopped %d\n");

		return status;
	}

	verifyLog(
		logControl,
		LOG,
		"FileRepPrimary_VerifySystemFilespace END %d mismatches  (%d directory mismatch/ %d file mismatch) of %d total compares ",
		request->compareStats.totalMismatches,
		request->compareStats.totalDirMismatches,
		request->compareStats.totalFileMismatches,
		request->compareStats.comparesComplete);

	verifyLog(
		logControl,
		LOG,
		"FileRepPrimary_VerifySystemFilespace END %d mismatches resolved (%d file mismatched resolved)  ",
		request->compareStats.totalMismatchesResolved,
		request->compareStats.totalFileMismatchesResolved);

	printVerificationStatus(LOG, request);

	return STATUS_OK;


}

bool
isRelBufKindOkForHeapException(PersistentFileSysRelBufpoolKind kind)
{
	switch (kind) {

		case PersistentFileSysRelBufpoolKind_Heap:
		case PersistentFileSysRelBufpoolKind_AppendOnlySeginfo:
		case PersistentFileSysRelBufpoolKind_AppendOnlyBlockDirectory:
		case PersistentFileSysRelBufpoolKind_AppendOnlyVisibilityMap:
		case PersistentFileSysRelBufpoolKind_BitMap:
		case PersistentFileSysRelBufpoolKind_Sequence:
        case PersistentFileSysRelBufpoolKind_Toast:
		case PersistentFileSysRelBufpoolKind_UncatalogedHeap:
			return TRUE;
	
		case PersistentFileSysRelBufpoolKind_None:
		case PersistentFileSysRelBufpoolKind_Btree:
		case PersistentFileSysRelBufpoolKind_UnknownRelStorage:
		case PersistentFileSysRelBufpoolKind_UnknownIndex:
	    case PersistentFileSysRelBufpoolKind_UnknownRelKind:
			return FALSE;
		
		default:
 
			Assert (0);
			break;

	}

	Assert(0);
	return FALSE;

}

int
FileRepPrimary_VerifyAllRelFileNodes(
	FileRepVerifyRequest_s * request, bool resume, bool justCount)
{
	FileRepVerifyLogControl_s logControl = request->logControl;
	int status = STATUS_OK;
	FileRepRelFileNodeInfo_s *relfilenodeTable = NULL;
	int numRows= 0;
	int numMismatches=0, numThisSend=0;
	int numSends = 0;
	int row = 0;

	char listingBuffer[FILEREP_MESSAGEBODY_LEN];

	uint32 maxSummaryItemsPerSend = 0;
	FileRepOperationDescription_u desc;
	FileRepOperationDescription_u responseDesc;
	uint32						responseDataLength = 0;
	void *						responseData = NULL;
	FileRepVerifySummaryItem_s	*summaryItemArray = NULL;

	OnlineVerifyScanToken onlineVerifyScanToken;
	RelFileNode relNode;
	int32 segmentFileNum = 0;
	PersistentFileSysRelStorageMgr	relStorageMgr;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	int64							mirrorAppendOnlyLossEof = 0;
	int64							mirrorAppendOnlyNewEof = 0;
	ItemPointerData					persistentTid;
	int64							persistentSerialNum = 0;

	bool  foundAnotherRow = false;
	char *primaryFilespaceLocation = NULL;
	char *mirrorFilespaceLocation = NULL;
	FileRepIdentifier_u		fileRepIdentifier;
	FileRepRelationType_e	fileRepRelationType;
	bool keepRow = true;
	//char baseFilePath[MAXPGPATH+2];
	char primaryBaseDirectory[MAXPGPATH+1];
	bool continueRequest = true;

	FileRep_InsertConfigLogEntry("verify all rel file nodes");

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepPrimary_VerifyAllRelFileNodes START resume %d justCount %d",
			  resume, justCount
		);

	getcwd(primaryBaseDirectory, MAXPGPATH+1);


	memset(&fileRepIdentifier, 0, sizeof(FileRepIdentifier_u));
	maxSummaryItemsPerSend = FILEREP_MESSAGEBODY_LEN/sizeof(FileRepVerifySummaryItem_s);
	summaryItemArray = (FileRepVerifySummaryItem_s *) listingBuffer;

	relfilenodeTable = (void *) palloc(maxSummaryItemsPerSend * sizeof(FileRepRelFileNodeInfo_s));

	if (relfilenodeTable == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						request->logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}


	/* Batches of 31 right now but lets cap at 15 to avoid TCP timeouts
	   XXXX
	*/
	/* JEANNA 15 should be #define */
	if (maxSummaryItemsPerSend > 15)
	{
		maxSummaryItemsPerSend = 15;
	}

	verifyLog(
		logControl, DEBUG1,
		"FileRepPrimary_VerifyAllRelFileNodes working with batches of %d rows ",
		maxSummaryItemsPerSend);

	numThisSend = 0;
	numRows = 0;
	numSends = 0;


	OnlineVerifyScanToken_Init(&onlineVerifyScanToken);
	do
	{

		foundAnotherRow = PersistentFileSysObj_OnlineVerifyScan(
			&onlineVerifyScanToken,
			&relNode,
			/* for heap always 0 */
			&segmentFileNum,
			&relStorageMgr,
			&mirrorDataSynchronizationState,
			&relBufpoolKind,
			&mirrorAppendOnlyLossEof,
			&mirrorAppendOnlyNewEof,
			&persistentTid,
			&persistentSerialNum);


		verifyLog(logControl,
				  LOG,
				  "PersistentFileSysObj_OnlineVerifyScan numRows %d foundAnotherRow %d persistentTid %d persistentSerialNum %d",
				  numRows,
				  foundAnotherRow,
				  persistentTid,
				  persistentSerialNum);


		/* send	 relNode -> filespaces info ( call on primary
		 * and get primary and mirror, send mirror the info it needs
		 */

		/*
		 *	for append only - segmentFileNum is 1 or higher -
		 *	always a number
		 *
		 * for heap - segmentFileNum is 0 - 0 is for 0 - 1GB,
		 * 1 is for 1GB to 2GB, etc.
		 * must look at
		 * smgr interfaces do mapping
		 * to mirror send file level operation - we don't do
		 * the mapping
		 * send the segmentFileNum to the mirror
		 * compute segment file (reuse same mechanism) -
		 * implement sgmrwrite**** - see code oaht
		 * smgrwrite, mdwrite, cdb_filerep_mirroredbufferpool_write
		 * (MirroredBufferPool_Write)
		 * FileRepPrimary_MirrorWrite
		 * look at mapping part
		 * ADD smgr_verify, md_verify,
		 * cdb_filerep_mirroredbufferpool_verify
		 *
		 * and for md_write smgr_verify
		 *
		 * help mirror build the filename properly
		 * send segmentFileNum to mirror
		 *
		 * before files were not big enough to test this
		*/

		if (resume	&&
			foundAnotherRow &&
			(request->restartInfo.logicalCheckpoint_persistentSerialNum != 1) &&
			(persistentSerialNum < request->restartInfo.logicalCheckpoint_persistentSerialNum))
		{
			/* we've already done this one - skip over it */
			verifyLog(
				logControl,
				LOG,
				"Resuming and we've already completed this one (%d) checkpoint is %d ",
				persistentSerialNum,
				request->restartInfo.logicalCheckpoint_persistentSerialNum);
			continueRequest = true;
			continue;
		}

		if (foundAnotherRow)
		{
			if (mirrorDataSynchronizationState !=
				MirroredRelDataSynchronizationState_DataSynchronized)
			{
				/* verification should only be called when in sync
				 * VERIFICATION_ERROR_INTERNAL
				 * I saw this called when the TCP connection
				 * timed out - at what level should we
				 * handle/retry
				 */

				/* often I am seeing
				 * MirroredRelDataSynchronizationState_None but
				 * I see no sign of error on miror ?
				 * Ex. 288/504 on one run
				 * The objects seems to compare fine though?
				 * is this for template0 or master?
				 * TRY gprecoverseg -f should fix this ?
				 * gp_persistent_relation_node - get all rows from and
				 * check that col
				 */

				verifyLog(logControl,
						  LOG,
						  "VERIFICATION_ERROR_INTERNAL: Error in FileRepPrimary_VerifyAllRelFileNodes - mirrorDataSynchronizationState %d",
						  mirrorDataSynchronizationState);

				if (mirrorDataSynchronizationState !=
					MirroredRelDataSynchronizationState_None)
				{
					pfree(relfilenodeTable);
					return STATUS_ERROR;
				}
			}

			/* load the info into the relfilenodeTable at numThisSend */

			/* could just pass this in as a parameter - better to avoid copy overhead */
			relfilenodeTable[numThisSend].relFileNode = relNode;
			relfilenodeTable[numThisSend].relFileNodeValid = true;

			if ((relStorageMgr ==
				 PersistentFileSysRelStorageMgr_BufferPool) &&
				isRelBufKindOkForHeapException(relBufpoolKind))
			{
				/* HEAP PAGE */
				relfilenodeTable[numThisSend].exception =
					FileRepVerifyCompareException_HeapRelation;
			}
			else	if ((relStorageMgr ==
						 PersistentFileSysRelStorageMgr_BufferPool) &&
						(relBufpoolKind ==
						 PersistentFileSysRelBufpoolKind_Btree))
			{
				/* BTREE_INDEX */
				relfilenodeTable[numThisSend].exception =
					FileRepVerifyCompareException_BtreeIndex;

			}
			else	if (relStorageMgr ==
						PersistentFileSysRelStorageMgr_AppendOnly)
			{
				/* APPEND ONLY PAGE */
				relfilenodeTable[numThisSend].exception =
					FileRepVerifyCompareException_AORelation;
			}
			else
			{
				relfilenodeTable[numThisSend].exception =
					FileRepVerifyCompareException_None;
			}

			/* verification is only done when in sync - so these should match */
			Assert(mirrorAppendOnlyLossEof == mirrorAppendOnlyNewEof);
			relfilenodeTable[numThisSend].AO_logicalEof =
				mirrorAppendOnlyNewEof;

			relfilenodeTable[numThisSend].segmentFileNum = segmentFileNum;


			if (relfilenodeTable[numThisSend].relFileNode.spcNode
				== GLOBALTABLESPACE_OID)
			{
				snprintf(relfilenodeTable[numThisSend].primaryBaseDir,
						 MAXPGPATH+1,
						 "%s/global",
						 primaryBaseDirectory);
				strcpy(relfilenodeTable[numThisSend].mirrorBaseDir,
					   "global");

				/*  relfilenodeTable[numThisSend].mirrorIdSame = true; */

			}
			else if (relfilenodeTable[numThisSend].relFileNode.spcNode
					   == DEFAULTTABLESPACE_OID)
			{
				snprintf(relfilenodeTable[numThisSend].primaryBaseDir,
						 MAXPGPATH+1,
						 "%s/base/%d",
						 primaryBaseDirectory,
						 (int)relfilenodeTable[numThisSend].relFileNode.dbNode);
				snprintf(relfilenodeTable[numThisSend].mirrorBaseDir,
						 MAXPGPATH+1,
						 "%s/base/%d",
						 gMirrorBaseDir,
						 (int)relfilenodeTable[numThisSend].relFileNode.dbNode);

				/*  relfilenodeTable[numThisSend].mirrorIdSame = true; */

			}
			else
			{

				/* relfilenodeTable[numThisSend].mirrorIdSame = false; */
				PersistentTablespace_GetPrimaryAndMirrorFilespaces(
					relfilenodeTable[numThisSend].relFileNode.spcNode,
					&primaryFilespaceLocation,
					&mirrorFilespaceLocation);

				snprintf(relfilenodeTable[numThisSend].primaryBaseDir,
						 MAXPGPATH+1,
						 "%s/%d/%d",
						 primaryFilespaceLocation,
						 (int)relfilenodeTable[numThisSend].relFileNode.spcNode,
						 (int)relfilenodeTable[numThisSend].relFileNode.dbNode);

				snprintf(
					relfilenodeTable[numThisSend].mirrorBaseDir,
					MAXPGPATH+1,
					"%s/%d/%d",
					mirrorFilespaceLocation,
					(int)relfilenodeTable[numThisSend].relFileNode.spcNode,
					(int)relfilenodeTable[numThisSend].relFileNode.dbNode);

				if (primaryFilespaceLocation != NULL)
					pfree(primaryFilespaceLocation);
				if (mirrorFilespaceLocation != NULL)
					pfree(mirrorFilespaceLocation);
			}

			/* relfilenode says where it is in the file system,
			 *  this starts out matching the relid but can change
			 *  over time e.g if the table is truncated, if the
			 *  distribution is altered or if the index is reindexed
			 */
			if (segmentFileNum == 0)
			{
				snprintf(
						 relfilenodeTable[numThisSend].filename,
						 MAXPGPATH+1,
						 "%d",
						 (int) relfilenodeTable[numThisSend].relFileNode.relNode);
			}
			else
			{
				snprintf(relfilenodeTable[numThisSend].filename,
						 MAXPGPATH+1,
						 "%d.%d",
						 (int)
						 relfilenodeTable[numThisSend].relFileNode.relNode,
						 segmentFileNum);
			}

			relfilenodeTable[numThisSend].fromBasePath[0] = '\0';



			/*
			 * TODO - don't need both relpath and custom built path -
			 * should use relpath but need to split into directoryName
			 * and fileName when needed
			 *
			 * Get path to a given relation
			 * (returns a relative path for "global" and "base",
			 * otherwise full path)
			 * char *relpath(RelFileNode rnode);
			 *
			 * Get the full path to a given database
			 * (returns a relative path for "global" and "base",
			 * otherwise full path)
			 * char *GetDatabasePath(Oid dbNode, Oid spcNode);
			 *
			 */

		    snprintf(relfilenodeTable[numThisSend].fullBaseFilePath, MAXPGPATH+1,
					 "%s/%s",
					 relfilenodeTable[numThisSend].primaryBaseDir,
					 relfilenodeTable[numThisSend].filename);

			keepRow = keepItem(
				//XXXXX?
				//relfilenodeTable[numThisSend].filename,
				//baseFilePath,
				relfilenodeTable[numThisSend].fullBaseFilePath,
				true,
				request->verifyItem,
				request->requestType);
			if (keepRow)
			{
				//consider whether this is in the ignore lists
				if(isExcludedFile(
						  relfilenodeTable[numThisSend].filename,
						  request->numIgnoreFilePatterns,
						  request->ignoreFiles))
				{
					keepRow = false;

				}
				else
				{
					char dir[MAXPGPATH+1];
					char fullFilePath[MAXPGPATH+1];

					snprintf(fullFilePath, MAXPGPATH+1,
							 "%s/%d",
							 relfilenodeTable[numThisSend].primaryBaseDir,
							 (int) relfilenodeTable[numThisSend].relFileNode.relNode);

					extractDir(fullFilePath,
							   dir, MAXPGPATH+1);

					verifyLog(logControl, DEBUG1,
							  "extractDir filename %s dir %s",
							  fullFilePath,
							  dir);

					if (isExcludedDirectory(
							dir,
							request->numIgnoreDirs,
							request->ignoreDirs))
					{

						keepRow = false;
						verifyLog(request->logControl,
								  DEBUG3,
								  "Skipping file %s because in excluded directory",
								  fullFilePath,
								  dir
							);

					}

				}
			}


			if (keepRow)
			{
				verifyLog(logControl, DEBUG1,
						  "Keeping ROW %d (%d of %d, send %d): primary %s/ %s mirror %s/ %s %d %d %d exception %d justCount %d",
						  numRows,
						  numThisSend,
						  maxSummaryItemsPerSend,
						  numSends,
						  relfilenodeTable[numThisSend].primaryBaseDir,
						  relfilenodeTable[numThisSend].filename,
						  relfilenodeTable[numThisSend].mirrorBaseDir,
						  relfilenodeTable[numThisSend].filename,
						  (int) relfilenodeTable[numThisSend].relFileNode.spcNode,
						  (int) relfilenodeTable[numThisSend].relFileNode.dbNode,
						  (int) relfilenodeTable[numThisSend].relFileNode.relNode,
						  relfilenodeTable[numThisSend].exception,
						  justCount);


				if (justCount)
				{
					struct stat	 info;
					int numSegments;
					int seg;
					char filename[MAXPGPATH+2];

					request->compareStats.estimateTotalCompares++;

/*					if	(relStorageMgr ==
						 PersistentFileSysRelStorageMgr_BufferPool) &&
						(relBufpoolKind ==
						 PersistentFileSysRelBufpoolKind_Heap))
*/
					if (relfilenodeTable[numThisSend].exception == FileRepVerifyCompareException_HeapRelation)

					{
						numSegments = FileRepVerify_NumHeapSegments(relfilenodeTable[numThisSend].relFileNode);

					} else {
						numSegments = 1;
					}
					for (seg=0; seg < numSegments; seg++){
						if (seg == 0)
						{

							snprintf(
								filename,
								MAXPGPATH+2,
								"%s/%s",
								relfilenodeTable[numThisSend].primaryBaseDir,
								relfilenodeTable[numThisSend].filename);

						}
						else
						{
							snprintf(
								filename,
								MAXPGPATH+2,
								"%s/%s.%d",
								relfilenodeTable[numThisSend].primaryBaseDir,
								relfilenodeTable[numThisSend].filename,
								seg);


						}


						if(stat(filename, &info) < 0)
						{
							verifyLog(
								request->logControl, DEBUG3,
								"Stat on %s failed", filename);
						}
						else
						{
							FileRepVerify_AddToTotalBlocksToVerify(info.st_size);

						}
					}


				}


				/* we are keeping this row so
				 * update the controlling stats
				 */
				numRows++;
				numThisSend++;

			}
			else
			{
				verifyLog(
					logControl, DEBUG1,
					"Skipping ROW %d (%d of %d, send %d): primary %s/ %s mirror %s/ %s %d %d %d exception %d %d",
					numRows,
					numThisSend,
					maxSummaryItemsPerSend,
					numSends,
					relfilenodeTable[numThisSend].primaryBaseDir,
					relfilenodeTable[numThisSend].filename,
					relfilenodeTable[numThisSend].mirrorBaseDir,
					relfilenodeTable[numThisSend].filename,
					(int) relfilenodeTable[numThisSend].relFileNode.spcNode,
					(int) relfilenodeTable[numThisSend].relFileNode.dbNode,
					(int) relfilenodeTable[numThisSend].relFileNode.relNode,
					relfilenodeTable[numThisSend].exception,
					justCount);

			}



		} /* foundAnotherRow */


/*			 if (!foundAnotherRow){
 *  do I need to actively call
 * PersistentFileSysObj_OnlineVerifyAbandonScan when done
 * I don't think so - don't see other AbandonScan
 * variants called
 *
 */
		if (!justCount &&
			((!foundAnotherRow && (numThisSend > 0)) ||
			 (numThisSend == maxSummaryItemsPerSend)))
		{

			/* TIME TO SEND A BATCH - SEND_BASE_DIRECTORY */
			status =
				FileRepPrimary_ComputeSummaryItemsFromRelFileNodeInfoTable(
					&(relfilenodeTable[0]),
					numThisSend,
					summaryItemArray,
					request->doHash,
					request->includeFileSizeInfo,
					logControl);
			if (STATUS_OK != status)
			{
				pfree(relfilenodeTable);
				return status;
			}

			for (row=0; row< numThisSend; row++)
			{
				summaryItemArray[row].summary.fileSummary.rowHint= row;
			}

			desc.verify.type = FileRepOpVerifyType_SummaryItems;
			desc.verify.logOptions =
				logControl.logOptions;
			desc.verify.desc.summaryItems.hashcodesValid = request->doHash;
			desc.verify.desc.summaryItems.sizesValid =
				request->includeFileSizeInfo ;
			desc.verify.desc.summaryItems.num_entries = numThisSend;

			request->compareStats.comparesComplete += numThisSend;

			verifyLog(logControl,
					  DEBUG4,
					  "Sending hashes on row %d to row %d",
					  numRows-numThisSend+1,
					  numRows);
			/* use id and type of the first summary item
			 * this is just used as an id for this message
			 * does it matter if it is primary or mirror
			 * SEND_BASE_DIRECTORY
			 */

			memset(&fileRepIdentifier, 0, sizeof(FileRepIdentifier_u));
			snprintf(
				fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
				MAXPGPATH+1,
				"%s/%s",
				relfilenodeTable[0].mirrorBaseDir,
				relfilenodeTable[0].fromBasePath);

			strncpy(
				fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName,
				relfilenodeTable[0].filename,
				MAXPGPATH+1);

			fileRepIdentifier.fileRepFlatFileIdentifier.blockId =
				FILEREPFLATFILEID_WHOLE_FILE;

			fileRepRelationType = FileRepRelationTypeFlatFile;

			verifyLog(request->logControl,
					  DEBUG3,
					  "SEND SUMMARY ITEMS FileRepPrimary_VerifyAllRelFileNodes Sending a batch (%d) of summary items to mirror", numThisSend);

			status = FileRepPrimary_MirrorVerify(
												 fileRepIdentifier,
												 fileRepRelationType,
												 desc,
												 listingBuffer,
												 FILEREP_MESSAGEBODY_LEN,
												 &responseData,
												 &responseDataLength,
												 &responseDesc);

			if (status != STATUS_OK)
			{
				pfree(relfilenodeTable);

				return status;
			}

			numMismatches =
				responseDesc.verify.desc.summaryItems.num_entries;
			if (numMismatches == 0)
			{
				verifyLog(logControl,
						  DEBUG3,
						  "MIRROR SAYS NO MISMATCHES THIS BATCH");
			}
			else
			{

#if DO_ENHANCED_COMPARES
				int i =0;
				FileRepVerifySummaryItem_s * itemArray =
					(FileRepVerifySummaryItem_s *)responseData;
				int numFiles =0, numDirs = 0;
				int numMismatchesResolved = 0;
#endif
				request->compareStats.totalMismatches +=numMismatches;
				verifyLog(logControl,
						  DEBUG3,
						  "MIRROR SAYS %d of %d MISMATCHES THIS BATCH!!",
						  numMismatches, numThisSend);
				Assert(responseData != NULL);
				Assert(responseDataLength > 0);
				Assert(responseDataLength == sizeof(FileRepVerifySummaryItem_s) * numMismatches);

#if DO_ENHANCED_COMPARES
				for (i=0; i < numMismatches; i++)
				{
					FileRepVerifySummaryItem_s * pItem = NULL;

					pItem = &(itemArray[i]);

					if (!(pItem->isFile))
					{
						/* shouldn't have any directories
						 * in this response
						 */
						Assert(0);
						numDirs++;

					}
					else
					{

						bool matched = false;

						int rowHint = 0;
						char  primaryFilename[MAXPGPATH+1];
						char mirrorFilename[MAXPGPATH+1];

						rowHint =
							pItem->summary.fileSummary.rowHint;

						Assert (rowHint < numThisSend);

						snprintf(primaryFilename,
								 MAXPGPATH+1,
								 "%s/%s/%s",
								 relfilenodeTable[rowHint].primaryBaseDir,
								 relfilenodeTable[rowHint].fromBasePath,
								 relfilenodeTable[rowHint].filename);

						snprintf(mirrorFilename,
								 MAXPGPATH+1,
								 "%s/%s/%s",
								 relfilenodeTable[rowHint].mirrorBaseDir,
								 relfilenodeTable[rowHint].fromBasePath,
								 relfilenodeTable[rowHint].filename);



						/*  Could have another intermediate option -
						 *  send another whole file hash applying the
						 *  exceptions and perhaps locking the
						 *  relation
						 */
						/* this one sends block by block
						 * send in both primary and mirror identifier
						 * XXX if this is a heap file we need to map each
						 * block to the right .segmentNumFile/offset on
						 * the mirror
						 */
						request->compareStats.numEnhancedCompares++;


						status = FileRepPrimary_SendEnhancedBlockHashes(
																		relfilenodeTable[rowHint],
																		&matched,
																		logControl);
						if (status != STATUS_OK)
						{
							pfree(relfilenodeTable);
							if (responseData != NULL)
							{
								pfree(responseData);
								responseData = NULL;
							}

							FileRepPrimary_PeriodicTasks(request);

							continueRequest = FileRepPrimary_ContinueVerificationRequest(request);

							if (! continueRequest)
							{
								verifyLog(
										  request->logControl,
										  LOG,
										  "FileRepPrimary_VerifyAllRelFileNodes: Request %s is being aborted or suspended",
										  request->logControl.logOptions.token);

								return STATUS_OK;
							}
							else
							{
								ereport(WARNING,
										(errmsg("verification failure, "
												"unable to complete verification request with token '%s'"
												"verification aborted, ",
												request->logControl.logOptions.token),
										 errhint("run gpverify")));

								return status;
							}

						}

						if (!matched)
						{
							verifyLog(request->logControl,
									  LOG,
									  "FAILED ENHANCED COMPARE %s (%d of %d) - relnodeValid %d exception %d",
									  pItem->filename,
									  i,
									  numMismatches,
									  relfilenodeTable[rowHint].relFileNodeValid,
									  relfilenodeTable[rowHint].exception);

							verifyLog(logControl,
									  DEBUG3,
									  "!!MISMATCH:	file %s on primary and %s on mirror (rowHint %d  row %d of %d) some Enhanced Block Hashes mismatched",
									  primaryFilename,
									  mirrorFilename,
									  rowHint,
									  row,
									  numRows);

							gFileRepVerifyShmem->request.isFailed = true;
						}
						else
						{
							numMismatchesResolved++;
							request->compareStats.totalMismatchesResolved++;
							request->compareStats.totalFileMismatchesResolved++;
							verifyLog(request->logControl,
									  LOG,
									  "SUCCEEDED %s (%d of %d) Enhanced Compare (SendEnhancedBlockHashes) succeeded - mismatch resolved",
									  pItem->filename,
									  i,
									  numMismatches);

							verifyLog(
								logControl,
								DEBUG3,
								"SUCCEEDED file %s on primary and %s on mirror  (rowhint %d row %d of %d) All Enhanced Block Hashes Matched",
								primaryFilename,
								mirrorFilename,
								rowHint,
								row,
								numRows);
						}
					}


				}
				verifyLog(logControl,
						  DEBUG3,
						  "Of the %d mismatches %d files, %d directories %d resolved",
						  numMismatches,
						  numFiles,
						  numDirs,
						  numMismatchesResolved);
#endif

				if (responseData != NULL)
				{
					pfree(responseData);
					responseData = NULL;
				}
			}

		}

		if (((!foundAnotherRow && (numThisSend > 0)) ||
			 (numThisSend == maxSummaryItemsPerSend)))
		{

			numThisSend = 0;
			numSends++;
		}

		if (!justCount)
		{
			/* REPORT STATS */
			printVerificationStatus(LOG, request);

			FileRepPrimary_SetLogicalCheckpoint(request,persistentSerialNum );

			status = FileRepPrimary_PeriodicTasks(request);

			if ( status != STATUS_OK)
			{
				verifyLog(request->logControl,
						  LOG,
						  "FileRepPrimary_PeriodicTasks failed)");
			}
			continueRequest = FileRepPrimary_ContinueVerificationRequest(request);

			if (!continueRequest)
			{
				verifyLog(
					request->logControl,
					LOG,
					"FileRepPrimary_VerifyAllRelFileNodes: Request %s is being aborted or suspended",
					request->logControl.logOptions.token);
				return STATUS_OK;

			}
		}
	} while (foundAnotherRow && continueRequest);

	if (!justCount)
	{
		printVerificationStatus(LOG, request);

		verifyLog(
			logControl,
			LOG,
			"FileRepPrimary_VerifyAllRelFileNodes END %d mismatches of %d total compares, %d enhanced compares ",
			request->compareStats.totalMismatches,
			request->compareStats.comparesComplete,
			request->compareStats.numEnhancedCompares);

		verifyLog(
			logControl,
			LOG,
			"FileRepPrimary_VerifyAllRelFileNodes END %d mismatches resolved	",
			request->compareStats.totalMismatchesResolved);
	}
	pfree(relfilenodeTable);
	return STATUS_OK;
}

void
FileRepPrimary_ClearCheckpoint(FileRepVerifyRequest_s *request)
{
	request->restartInfo.valid = false;

}
void
FileRepPrimary_SetLogicalCheckpoint(
									FileRepVerifyRequest_s	*request,
									int64					persistentSerialNum)
{
	request->restartInfo.useLogical = true;
	request->restartInfo.logicalCheckpoint_persistentSerialNum = persistentSerialNum;
	request->restartInfo.valid = true;
}

void
FileRepPrimary_SetPhysicalCheckpoint(
									 FileRepVerifyRequest_s *request,
									 char *nextParent,
									 char *nextChild)
{
	request->restartInfo.useLogical = false;
	strncpy(
			request->restartInfo.physicalCheckpoint_whereNextParent,
			nextParent,
			MAXPGPATH+1);
	strncpy(
			request->restartInfo.physicalCheckpoint_whereNextChild,
			nextChild,
			MAXPGPATH+1);
	request->restartInfo.valid = true;
}

bool
FileRepPrimary_CheckpointFileIsPresent(
									   char *token,
									   char *errorMessage,
									   int	errMsgLen)
{

	FileRepVerifyRequest_s restoreContents;
	char chkpntPath[MAXPGPATH+1];
	FILE * f = NULL;
	snprintf(chkpntPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.chkpnt",
			 VERIFYDIR,
			 token);

	f= fopen(chkpntPath, "rb");
	if (f == NULL)
	{
		snprintf(errorMessage,
				 errMsgLen,
				 "Unable to open file %s",
				 chkpntPath);
		return false;
	}

	if ( fread(&restoreContents, 1, sizeof(FileRepVerifyRequest_s), f) != sizeof(FileRepVerifyRequest_s))
	{
		snprintf(
				 errorMessage,
				 errMsgLen,
				 "Unable to read checkpoint from file file %s",
				 chkpntPath);
		fclose(f);
		return false;
	}

	if (strncmp(
				restoreContents.logControl.logOptions.token,
				token,
				MAXPGPATH+1))
	{
		snprintf(
				 errorMessage,
				 errMsgLen,
				 "Checkpoint file %s contains incorect token %s",
				 chkpntPath,
				 restoreContents.logControl.logOptions.token);
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

int
FileRepPrimary_RestoreVerificationRequest(
										  FileRepVerifyRequest_s	*request,
										  char						*token)
{

	FileRepVerifyRequest_s	restore;
	File					chkpntFile;
	char					chkpntPath[MAXPGPATH+1];

	snprintf(chkpntPath,
			 MAXPGPATH+1,
			 "%s/verification_%s.chkpnt",
			 VERIFYDIR,
			 token);

	errno = 0;
	chkpntFile = PathNameOpenFile(chkpntPath,
								  O_RDONLY | PG_BINARY,
								  S_IRUSR| S_IWUSR);

	if (chkpntFile < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' verification token '%s' : %m "
						"verification aborted",
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						token),
				errhint("run gpverify")));

		return STATUS_ERROR;
	}

	if (FileSeek(chkpntFile, 0L, SEEK_END) < 0)
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"checkpoint file empty '%s' verification token '%s' : %m "
						"verification aborted",
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	if (FileSeek(chkpntFile, 0L, SEEK_SET) < 0)
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not seek to begin of file '%s' verification token '%s' : %m "
						"verification aborted",
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	if (FileRead(
				 chkpntFile,
				 (char *) &restore,
				 sizeof(FileRepVerifyRequest_s)) != sizeof(FileRepVerifyRequest_s))
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not read from file '%s' verification token '%s' : %m "
						"verification aborted",
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	if (strncmp(restore.logControl.logOptions.token, token, MAXPGPATH+1))
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errmsg("verification failure, "
						"could not match verification request between expected '%s' and checkpoint token '%s' file '%s' "
						"verification aborted",
						token,
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						restore.logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	restore.logControl.filesAreOpen = false;

	memcpy(request, &restore, sizeof(FileRepVerifyRequest_s));

	if (request->checkpointVersion != CHECKPOINT_VERSION_ID)
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errmsg("verification failure, "
						"could not match verification checkpoint version expected '%d' actual '%d' file '%s' "
						"verification token '%s' "
						"verification aborted",
						CHECKPOINT_VERSION_ID,
						request->checkpointVersion,
						(chkpntPath == NULL) ? "<null>" : chkpntPath,
						token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	request->lastCheckpointTime = 0;

	if (request->numIgnoreFilePatterns)
	{
		/* allocate space for ignorefile patterns and ignore dir patterns */
		request->ignoreFiles = (char **) palloc(request->numIgnoreFilePatterns * sizeof(char *));

		if (request->ignoreFiles == NULL)
		{
			FileClose(chkpntFile);

			ereport(WARNING,
					(errmsg("verification failure, "
							"could not allocate memory to resume verification token '%s' "
							"verification aborted",
							token),
					 errhint("run gpverify")));

			request->numIgnoreFilePatterns = 0;

			return STATUS_ERROR;
		}

		/* read in ignore file info */
		for (int i=0; i< request->numIgnoreFilePatterns; i++)
		{

			int len;

			/* read in len of pattern */
			if (FileRead(
						 chkpntFile,
						 (char *) &len,
						 sizeof(int)) != sizeof(int) )
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not read from file '%s' verification token '%s' : %m "
								"verification aborted",
								(chkpntPath == NULL) ? "<null>" : chkpntPath,
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			/* allocate space based on len */
			request->ignoreFiles[i] = (char *) palloc((len) * sizeof(char));
			if (request->ignoreFiles[i] == NULL)
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to resume verification token '%s' "
								"verification aborted",
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			/* read in actual pattern */
			if (FileRead(
						 chkpntFile,
						 (char *) request->ignoreFiles[i],
						 len * sizeof(char)) != (len * sizeof(char)) )
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not read from file '%s' verification token '%s' : %m "
								"verification aborted",
								(chkpntPath == NULL) ? "<null>" : chkpntPath,
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

		}
	}

	/* read in ignore dir info */
	if (request->numIgnoreDirs)
	{
		/* allocate space for ignorefile patterns and ignore dir patterns */
		request->ignoreDirs = (char **) palloc(request->numIgnoreDirs * sizeof(char *));
		if (NULL == request->ignoreDirs)
		{
			FileClose(chkpntFile);

			ereport(WARNING,
					(errmsg("verification failure, "
							"could not allocate memory to resume verification token '%s' "
							"verification aborted",
							token),
					 errhint("run gpverify")));

			request->numIgnoreDirs = 0;
			return STATUS_ERROR;
		}

		/* read in ignore dir info */
		for (int i=0; i< request->numIgnoreDirs; i++)
		{

			int len;

			/* read in len of dir */
			if (FileRead(
						 chkpntFile,
						 (char *) &len,
						 sizeof(int)) != sizeof(int) )
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not read from file '%s' verification token '%s' : %m "
								"verification aborted",
								(chkpntPath == NULL) ? "<null>" : chkpntPath,
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			/* allocate based on len */
			request->ignoreDirs[i] = (char *) palloc((len) * sizeof(char));
			if (request->ignoreDirs[i] == NULL)
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to resume verification token '%s' "
								"verification aborted",
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			/* read in actual pattern */
			if (FileRead(
						 chkpntFile,
						 (char *) request->ignoreDirs[i],
						 len * sizeof(char) ) != (len * sizeof(char)) )
			{
				FileClose(chkpntFile);

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not read from file '%s' verification token '%s' : %m "
								"verification aborted",
								(chkpntPath == NULL) ? "<null>" : chkpntPath,
								token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}
		}
	}


	if (request->restartInfo.valid)
	{
		if (request->restartInfo.useLogical)
		{
			verifyLog(
				request->logControl,
				NOTICE,
				"verification checkpoint restored successfully: logical persistent serial num %d ",
				request->restartInfo.logicalCheckpoint_persistentSerialNum);

		}
		else
		{

			verifyLog(
				request->logControl,
				NOTICE,
				"verification checkpoint restored successfully: physical next to compare %s child %s",
				request->restartInfo.physicalCheckpoint_whereNextParent,
				request->restartInfo.physicalCheckpoint_whereNextChild);
		}
	}
	else
	{
		FileClose(chkpntFile);

		ereport(WARNING,
				(errmsg("verification failure, "
						"resume information is not valid "
						"verification aborted"),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	return STATUS_OK;
}

int
FileRepPrimary_CheckpointVerificationRequest(
	FileRepVerifyRequest_s * request,
	bool force)
{
	TimestampTz currentTime;
	long secs = 0;
	int microsecs = 0;


	/* if we are not forcing the check point, consider whether we really want
	 * to do a check point right now based on verify_checkpoint_interval
	 */
	if (!force)
	{

		/* verify_checkpoint_interval 0 means never checkpoint */
		if (verify_checkpoint_interval == 0)
		{
			return STATUS_OK;
		}

		/* if lastCheckpointTime is 0  haven't checkpointed since start/resume */
		if (request->lastCheckpointTime != 0)
		{
			currentTime = GetCurrentTimestamp();
			TimestampDifference(
				request->lastCheckpointTime,
				currentTime,
				&secs,
				&microsecs);

			/* not enough time has passed before ready to checkpoint again */
			if ( secs < verify_checkpoint_interval)
			{
				return STATUS_OK;
			}

		}
	}


	verifyLog(
		request->logControl,
		LOG,
		"CheckpointVerificationRequest restartInfo.valid %d useLogical %d lastCheckpoint %d secs ago verify_checkpoint_interval %d force %d",
		request->restartInfo.valid, request->restartInfo.useLogical,
		secs,
		verify_checkpoint_interval,
		force);

	errno = 0;

	if (request->restartInfo.valid)
	{
		if (request->restartInfo.useLogical)
		{
			verifyLog(
				request->logControl,
				DEBUG1,
				"CHECKPOINT: useLogical	persistentSerialNum %d ",
				request->restartInfo.logicalCheckpoint_persistentSerialNum);

		}
		else
		{
			verifyLog(
				request->logControl,
				DEBUG1,
				"CHECKPOINT: usePhysical	 whereNextParent %s	 whereNextChild %s",
				request->restartInfo.physicalCheckpoint_whereNextParent,
				request->restartInfo.physicalCheckpoint_whereNextChild);
		}

		if (FileSeek(request->logControl.chkpntFile, 0, SEEK_SET) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not seek to begin of file '%s' verification token '%s' : %m "
							"verification aborted",
							request->logControl.logOptions.chkpntPath,
							request->fileRepVerifyArguments.token),
					 errhint("run gpverify")));

			return STATUS_ERROR;
		}

		if ((int) FileWrite(request->logControl.chkpntFile,
							(char *) request,
							sizeof(FileRepVerifyRequest_s)) != sizeof(FileRepVerifyRequest_s))
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not write to file '%s' verification token '%s' : %m "
							"verification aborted",
							request->logControl.logOptions.chkpntPath,
							request->fileRepVerifyArguments.token),
					 errhint("run gpverify")));

			return STATUS_ERROR;

		}

		/* JEANNA why so many writes, why not one write */

		/* Don't really need to checkpoint each time - they won't change */
		if (! request->restartInfo.haveWrittenIgnoreInfo)
		{
			int len = 0;
			/* write out each ignorefile - len then name */
			for (int i=0; i< request->numIgnoreFilePatterns; i++)
			{
				len = strlen(request->ignoreFiles[i]) + 1;
				if ((int) FileWrite(request->logControl.chkpntFile,
									(char *) &len,
									sizeof(int)) != sizeof(int))
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("verification failure, "
									"could not write to file '%s' verification token '%s' : %m "
									"verification aborted",
									request->logControl.logOptions.chkpntPath,
									request->fileRepVerifyArguments.token),
							 errhint("run gpverify")));

					return STATUS_ERROR;

				}

				if ((int) FileWrite(request->logControl.chkpntFile,
									(char *) request->ignoreFiles[i],
									len* sizeof(char)) != (len * sizeof(char)))
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("verification failure, "
									"could not write to file '%s' verification token '%s' : %m "
									"verification aborted",
									request->logControl.logOptions.chkpntPath,
									request->fileRepVerifyArguments.token),
							 errhint("run gpverify")));

					return STATUS_ERROR;
				}
			}

			/* write out each ignoredir - len then name */
			for (int i=0; i< request->numIgnoreDirs; i++)
			{
				len = strlen(request->ignoreDirs[i]) + 1;

				if ((int) FileWrite(request->logControl.chkpntFile,
									(char *) &len,
									sizeof(int)) != sizeof(int))
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("verification failure, "
									"could not write to file '%s' verification token '%s' : %m "
									"verification aborted",
									request->logControl.logOptions.chkpntPath,
									request->fileRepVerifyArguments.token),
							 errhint("run gpverify")));

					return STATUS_ERROR;
				}

				if ((int) FileWrite(request->logControl.chkpntFile,
									(char *) request->ignoreDirs[i],
									len * sizeof(char)) != (len * sizeof(char)))
				{
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("verification failure, "
									"could not write to file '%s' verification token '%s' : %m "
									"verification aborted",
									request->logControl.logOptions.chkpntPath,
									request->fileRepVerifyArguments.token),
							 errhint("run gpverify")));

					return STATUS_ERROR;
				}
			}
			request->restartInfo.haveWrittenIgnoreInfo = true;
		}

		if (FileSync(request->logControl.chkpntFile) < 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not flush (fsync) to file '%s' verification token '%s' : %m "
							"verification aborted",
							request->logControl.logOptions.chkpntPath,
							request->fileRepVerifyArguments.token),
					 errhint("run gpverify")));

			return FALSE;
		}
		request->restartInfo.haveWrittenIgnoreInfo = true;
	}

	request->lastCheckpointTime = GetCurrentTimestamp();
	return STATUS_OK;
}

bool
stillInSync(void)
{
	/* check if we are still in sync with mirror */
	if (FileRepSubProcess_GetState() != FileRepStateShutdown &&
		FileRepSubProcess_GetState() != FileRepStateShutdownBackends &&
		! (FileRepSubProcess_GetState() == FileRepStateReady &&
		   dataState == DataStateInSync))
	{
		return false;
	}
	return true;

}

bool
FileRepPrimary_ContinueVerificationRequest(FileRepVerifyRequest_s *request)
{

	switch (request->state)
	{
		case FileRepVerifyRequestState_SUSPENDED:
		case FileRepVerifyRequestState_ABORTED:
		case FileRepVerifyRequestState_FAILED:
		case FileRepVerifyRequestState_COMPLETE:
		case FileRepVerifyRequestState_NONE:
			return false;

		case FileRepVerifyRequestState_PENDING:
		case FileRepVerifyRequestState_RUNNING:
			return true;
	}

	return true;
}

/*
 * Return TRUE if verification should be interrupted due to
 *		a) suspend requested by user
 *		b) abort requested by user
 *		c) transition to change tracking
 *		d) database shutdown
 */
bool
FileRepVerification_ProcessRequests(void)
{
	bool processRequest = TRUE;

	FileRepPrimary_PeriodicTasks(&gFileRepVerifyShmem->request);

	processRequest = ! FileRepPrimary_ContinueVerificationRequest(&gFileRepVerifyShmem->request);

	return processRequest;

}

int
FileRepPrimary_PeriodicTasks(FileRepVerifyRequest_s *request)
{
	FileRepVerifyLogControl_s logControl = request->logControl;

	/* check for new requests that abort or suspend this one */
	if (request->suspendRequested)
	{
		TimestampTz suspendTime;

		FileRep_InsertConfigLogEntry("verification suspended by user");

		suspendTime = GetCurrentTimestamp();
		request->state = FileRepVerifyRequestState_SUSPENDED;
		request->suspendRequested = false;
		verifyLog(request->logControl,
				  LOG,
				  "SUSPEND_TIME %s",
				  timestamptz_to_str(suspendTime));
		/*
		 * this is a manual suspend so empty the inprogress token file
		 * we don't want this one to resume automatically
		 */
		FileRepVerify_EmptyInProgressTokenFile();

		/* even if checkpoints disabled by verify_checkpoint_interval we
		 * want to check point here */
		FileRepPrimary_CheckpointVerificationRequest(request, true);

		return STATUS_OK;
	}

	if (! stillInSync())
	{

		FileRep_InsertConfigLogEntry("verification suspended by out of sync transition");

		/*
		 * do not update the checkpoint/restart point
		 * we will redo a portion?
		 */
		verifyLog(logControl,
				  LOG,
				  "PRIMARY/MIRROR not in sync; verification request interrupted - please restart when system is back in sync ");
		request->state = FileRepVerifyRequestState_SUSPENDED;

		/*
		 * this is not a manual suspend so leave the in progress
		 * token file alone
		 */

		/* if not still in sync then really shouldn't checkpoint current state
		 * last batch could be messed up
		 */
		return STATUS_OK;

	}

	if (FileRepSubProcess_IsStateTransitionRequested())
	{

		FileRep_InsertConfigLogEntry("verification suspended by change tracking transition");

		verifyLog(
			logControl,
			LOG,
			"StateTransition has occured; verification request interrupted- please restart when system is back in sync ");

		request->state = FileRepVerifyRequestState_SUSPENDED;
		/*
		 * this is not a manual suspend so leave the in progress
		 * token file alone
		 */
		FileRepPrimary_CheckpointVerificationRequest(request, true);

		return STATUS_OK;
	}

	/*
	 * check that no one has requested that this request
	 * be aborted - global table of tokens to abort - when
	 * abort it then delete from table? for now just
	 * a global boolean
	 */
	if (request->abortRequested)
	{

		FileRep_InsertConfigLogEntry("verification aborted by user");

		/* TODO: right now we only have one request in progress
		 *  when we have more this needs to be a lookup in a table
		 */
		request->state = FileRepVerifyRequestState_ABORTED;
		request->abortRequested = false;

		verifyLog(logControl,
				  DEBUG1,
				  "ABORTING REQUEST.... ");
		/* no need for check point */
		return STATUS_OK;
	}


	/* all that's left is to consider doing a periodic
	 * check point
	 */
	FileRepPrimary_CheckpointVerificationRequest(request, false);

	return STATUS_OK;
}

int
FileRepPrimary_EstimateVerificationRequest(FileRepVerifyRequest_s *request)
{
	int status = STATUS_OK;

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_EstimateVerificationRequest BEGIN ESTIMATE allLogical %d allPhysical %d",
		request->allLogical,
		request->allPhysical);
	if (request->requestType == FileRepVerifyRequestType_FILE)
	{
		verifyLog(
			request->logControl, DEBUG1,
			"requestType FILE verifyItem %s",
			request->verifyItem);

	}
	else if (request->requestType == FileRepVerifyRequestType_DIRECTORY)
	{
		verifyLog(
			request->logControl, DEBUG1,
			"requestType DIRECTORY verifyItem %s",
			request->verifyItem);
	} else {
		verifyLog(
			request->logControl,DEBUG1,
			"requestType FULL");

	}
	/* 	if (request->allPhysical){ */
	status = FileRepPrimary_EstimateUserFilespaces(request);
	if (status != STATUS_OK)
	{
		verifyLog(
				  request->logControl,
				  LOG,
				  "FileRepPrimary_EstimateUserFilespaces Unable to estimate work ");
		return status;
	}

	status = FileRepPrimary_EstimateSystemFilespace(request);
	if (status != STATUS_OK)
	{
		verifyLog(
				  request->logControl,
				  LOG,
				  "FileRepPrimary_EstimateSystemFilespace Unable to estimate work ");
		return status;
	}

	/* 	} */

	/* 	if (request->allLogical)  { */
	status = FileRepPrimary_VerifyAllRelFileNodes(request, false, true);
	if (status != STATUS_OK)
	{
		verifyLog(
				  request->logControl,
				  LOG,
				  "FileRepPrimary_EstimateAllRelFileNodes: Unable to estimate work ");
		return status;
	}
	verifyLog(
			  request->logControl,
			  DEBUG1,
			  "FileRepPrimary_EstimateVerificationRequest estimateTotalCompares %d ",
			  request->compareStats.estimateTotalCompares);

	if (request->compareStats.estimateTotalCompares == 0)
	{
		verifyLog(
				  request->logControl,
				  WARNING,
				  "VERIFCATION_ERROR_WARNING: No items to compare");

	}
	/* } */

	return STATUS_OK;
}

int
FileRepPrimary_ExchangeRequestParameters(FileRepVerifyRequest_s *request)
{
	int			status = STATUS_OK;
	FileRepOperationDescription_u fileRepOperationDescription;
	int			i = 0;
	int			sizeIgnoreInfo = 0;
	char		*dataToMirror = NULL;
	char		*thisLoc = NULL;
	void		*responseData = NULL;
	uint32		responseDataLength = 0;

	FileRepOperationDescription_u	responseDesc;
	FileRepIdentifier_u				fileRepIdentifier;
	FileRepRelationType_e			fileRepRelationType;

	verifyLog(
		request->logControl,
		DEBUG5,
		"In FileRepPrimary_ExchangeRequestParameters");

	fileRepOperationDescription.verify.type = FileRepOpVerifyType_RequestParameters;

	fileRepOperationDescription.verify.desc.requestParameters.type =  FileRepOpVerifyParamterType_IgnoreInfo;
	fileRepOperationDescription.verify.desc.requestParameters.numFileIgnore = request->numIgnoreFilePatterns;
	fileRepOperationDescription.verify.desc.requestParameters.numDirIgnore = request->numIgnoreDirs;
	fileRepOperationDescription.verify.logOptions = request->logControl.logOptions;
	sizeIgnoreInfo = 0;

	for (i=0; i< request->numIgnoreFilePatterns; i++)
	{
		sizeIgnoreInfo += (sizeof(int) +
						   (strlen(request->ignoreFiles[i]) + 1) *
						   sizeof(char));
	}
	for (i=0; i< request->numIgnoreDirs; i++)
	{
		sizeIgnoreInfo += (sizeof(int) +
						   (strlen(request->ignoreDirs[i]) + 1) *
						   sizeof(char));
	}

	Assert(sizeIgnoreInfo < FILEREP_MESSAGEBODY_LEN);

	dataToMirror = (char *)palloc(sizeIgnoreInfo);
	if (dataToMirror == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						request->logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	verifyLog(
		request->logControl,
		DEBUG5,
		"In FileRepPrimary_ExchangeRequestParameters sizeIgnoreInfo %d dataToMirror %x end %x %x",
		sizeIgnoreInfo,
		dataToMirror,
		dataToMirror+sizeIgnoreInfo,
		&(dataToMirror[sizeIgnoreInfo]));

	thisLoc = dataToMirror;
	for (i=0; i< request->numIgnoreFilePatterns; i++)
	{
		int * thisLen;
		thisLen = (int *) thisLoc;
		*thisLen = strlen(request->ignoreFiles[i]);
		thisLoc = (char *) (thisLen+1);
		strncpy(thisLoc, request->ignoreFiles[i], *thisLen);
		thisLoc[*thisLen]= '\0';
		verifyLog(
			request->logControl,
			LOG,
			"ignoreFile %d len %d %s address %x",
			i, *thisLen, thisLoc, thisLoc);
		thisLoc += (*thisLen + 1);
	}

	for (i=0; i< request->numIgnoreDirs; i++)
	{
		int * thisLen;
		thisLen = (int *) thisLoc;
		*thisLen = strlen(request->ignoreDirs[i]);
		thisLoc = (char *) (thisLen+1);
		strncpy(thisLoc, request->ignoreDirs[i], *thisLen);
		thisLoc[*thisLen]= '\0';
		verifyLog(
			request->logControl, LOG,
			"ignoreDir %d len %d %s address %x",
			i, *thisLen, thisLoc, thisLoc);
		thisLoc += (*thisLen +1 );

	}

	memset(&fileRepIdentifier, 0, sizeof(FileRepIdentifier_u));

	strcpy(
		   fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
		   "pg_verify");

	fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName[0] = '\0';

	fileRepIdentifier.fileRepFlatFileIdentifier.blockId = FILEREPFLATFILEID_WHOLE_FILE;

	fileRepRelationType = FileRepRelationTypeFlatFile;

	status = FileRepPrimary_MirrorVerify(
										 fileRepIdentifier,
										 fileRepRelationType,
										 fileRepOperationDescription,
										 dataToMirror,
										 sizeIgnoreInfo,
										 &responseData,
										 &responseDataLength,
										 &responseDesc);
	verifyLog(
		request->logControl,
		LOG,
		"In FileRepPrimary_ExchangeRequestParameters ConsumeMirrorResponse status %d responseDataLength %d",
		status,
		responseDataLength);

	if (status != STATUS_OK)
	{
		return status;

	}

	/* This is the first request that is sent after a request is resumed
	 * It is possible that there are old responses from old requets to mirror
	 * left in the pipeline - this while loop will clear those out and get 
	 * us to the reponse to the ExchangeRequestParameters request 
	 */
	while(responseDesc.verify.type != FileRepOpVerifyType_RequestParameters){

		status = FileRepAckPrimary_RunConsumerVerification(
			&responseData, 
			&responseDataLength, 
			&responseDesc);
		
		
		if (status != STATUS_OK && 
			dataState != DataStateInChangeTracking &&
			! primaryMirrorIsIOSuspended())
		{
			ereport(WARNING,
					(errmsg("mirror failure, "
							"could not complete operation on mirror, "
							"failover requested"), 
					 errhint("run gprecoverseg to re-establish mirror connectivity"),
					 FileRep_errdetail(fileRepIdentifier,
									   fileRepRelationType,
									   FileRepOperationVerify,
									   FILEREP_UNDEFINED), 
					 FileRep_errdetail_Shmem(),
					 FileRep_errdetail_ShmemAck(),
					 FileRep_errcontext()));				
		}


	}
	Assert(responseDataLength != 0);
	Assert(responseData != NULL);
	strncpy(gMirrorBaseDir, (char *) responseData, responseDataLength);

	verifyLog(
		request->logControl,
		LOG,
		"FileRepPrimary_ExchangeRequestParameters: gMirrorBaseDir is %s",
		gMirrorBaseDir);

	pfree(dataToMirror);
	return status;
}

int
FileRepPrimary_ExecuteVerificationRequest(
										  FileRepVerifyRequest_s	*request,
										  bool						resume)
{

	int status = STATUS_OK;
	FileRepVerifyLogControl_s logControl = request->logControl;

	bool
		doVerifyUserFilespaces = false,
		doVerifySystemFilespace = false,
		doVerifyAllRelFileNodes = false;

	bool
		resumeFilespaces = false,
		resumeDirectories = false,
		resumeRelFileNodes = false;

	/* decide which phases need to be executed,
	 * and if resuming, which phases being executed need to be resumed
	 * rather then started from the beginning
	 */
	/* JEANNA isn't allPhysical always TRUE */
	if (request->allPhysical)
	{
		doVerifyUserFilespaces = true;
		doVerifySystemFilespace = true;

		if (resume)
		{
			if (request->restartInfo.useLogical)
			{
				/* we are resuming and are past the physical
				 * verification parts
				 */
				doVerifyUserFilespaces = false;
				doVerifySystemFilespace = false;
			}
			else
			{
				if (request->restartInfo.filespacesComplete)
				{
					doVerifyUserFilespaces = false;
					resumeDirectories = true;
					Assert(doVerifySystemFilespace);
				}
				else
				{
					Assert(doVerifyUserFilespaces && doVerifySystemFilespace);
					resumeFilespaces = true;
				}
			}
		}
	}
	else
	{
		doVerifyUserFilespaces = false;
		doVerifySystemFilespace = false;
	}

	if (request->allLogical)
	{
		doVerifyAllRelFileNodes = true;
		if (resume && request->restartInfo.useLogical)
		{
			resumeRelFileNodes = true;
			Assert(!doVerifyUserFilespaces && !doVerifySystemFilespace);
		}
	}
	else
	{
		doVerifyAllRelFileNodes = false;
	}

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_ExecuteVerificationRequest BEGIN EXECUTE allLogical %d allPhysical %d resume %d",
		request->allLogical,
		request->allPhysical,
		resume);

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_ExecuteVerificationRequest BEGIN EXECUTE resumeRelFileNodes %d doVerifyAllRelFileNodes %d",
		resumeRelFileNodes,
		doVerifyAllRelFileNodes);

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_ExecuteVerificationRequest BEGIN EXECUTE resumeDirectories %d doVerifySystemFilespace %d",
		resumeDirectories,
		doVerifySystemFilespace);

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_ExecuteVerificationRequest BEGIN EXECUTE resumeFilespaces %d doVerifyUserFilespaces %d",
		resumeFilespaces,
		doVerifyUserFilespaces);

	request->state = FileRepVerifyRequestState_RUNNING;

	/* Call ExchangeRequestParameters here - this is ok because
	 * estimate phase does not involve the mirror
	 */
	status = FileRepPrimary_ExchangeRequestParameters(request);
	if (status != STATUS_OK)
	{
		verifyLog(
			request->logControl,
			WARNING,
			"FileRepPrimary_ExchangeRequestParameters failed with status %d\n",
			status);

		return status;
	}

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet(
								   FileRepVerification,
								   DDLNotSpecified,
								   "",	// databaseName
								   ""); // tableName
#endif

	if (doVerifyUserFilespaces)
	{
		status = FileRepPrimary_VerifyUserFilespaces(request, resumeFilespaces);

		if (status != STATUS_OK)
		{
			verifyLog(logControl,
					  WARNING,
					  "VerifyUserFilespaces failed status %d\n", status);
			/* when to say FAILED and when SUSPENDED
			 * failed should be not worth trying again
			 */
			request->state = FileRepVerifyRequestState_SUSPENDED;

			return status;
		}

		if (request->state != FileRepVerifyRequestState_RUNNING)
		{
			verifyLog(
				logControl, LOG,
				"FileRepPrimary_ExecuteVerificationRequestRequest is being stopped %d\n");

			return status;
		}
	}

	if (doVerifySystemFilespace)
	{
		/*
		 * BASE DIRECTORY WALK
		 * skip files that look like rel  file nodes we would have
		 * verified during the database walk
		 */
		status = FileRepPrimary_VerifySystemFilespace(
			request,
			resumeDirectories);

		if (status != STATUS_OK)
		{
			verifyLog(logControl,
					  WARNING,
					  "VerifySystemFilespace failed status %d\n", status);

			return status;
		}

		if (request->state != FileRepVerifyRequestState_RUNNING)
		{
			verifyLog(
				logControl, LOG,
				"FileRepPrimary_ExecuteVerificationRequest: Request is being stopped %d\n");

			return status;
		}
	}

	if (doVerifyAllRelFileNodes)
	{
		/* DATABASE WALK  */
		status = FileRepPrimary_VerifyAllRelFileNodes(
			request,
			resumeRelFileNodes,
			false);
		if (status != STATUS_OK)
		{
			verifyLog(logControl,
					  WARNING,
					  "VerifyAllRelFileNodes failed status %d\n",
					  status);

			return status;
		}

		if (request->state != FileRepVerifyRequestState_RUNNING)
		{
			verifyLog(
				logControl,
				LOG,
				"FileRepPrimary_ExecuteVerificationRequest: Request is being stopped %d\n");

			return status;
		}
	}

	return STATUS_OK;
}


void FileRepMirror_StartVerification(void)
{
	gMirrorRequestParams.valid = false;
	gMirrorRequestParams.numIgnoreFilePatterns = 0;
	gMirrorRequestParams.numIgnoreDirs = 0;

}

int FileRepMirror_CompareSummaryItem(
	FileRepVerifySummaryItem_s *pItem,
	bool doHash,
	bool includeFileSizeInfo,
	bool *mismatch,
	FileRepVerifyLogControl_s logControl)
{

	int status = STATUS_OK;
	pg_crc32 hashcode = 0;
	struct stat	 info;

	*mismatch = true;

	verifyLog(logControl, DEBUG1,
			  "FileRepMirror_CompareSummaryItem; baseDirectory %s directorySimpleName %s fileName %s\n",
			  pItem->baseDir,
			  pItem->fromBasePath,
			  pItem->filename);

/* this is a good test (GOOD_TEST)
 * fail all cheap compares to force more expensive compares
 * to be exercised - should never be left on for checkin
*/
#if 0
	*mismatch = true;
	return STATUS_OK;
#endif

	if (pItem->isFile)
	{
		char segmentFileName[MAXPGPATH+2];
		int numSegments = 0;
		int seg =0;
		pg_crc32 thisHashcode = 0;

		if (pItem->summary.fileSummary.exception == FileRepVerifyCompareException_HeapRelation)
		{
			numSegments = pItem->summary.fileSummary.numHeapSegments;
		}
		else
		{
			numSegments = 1;
		}

		for (seg = 0; seg < numSegments; seg++)
		{
			/* if this is a heap file with multiple segments must
			 * pass over all of them
			 */
			if (seg == 0)
			{
				/* the first segment doesn't have a .X after it */
				snprintf(segmentFileName, MAXPGPATH+1, "%s/%s/%s",
						 pItem->baseDir, pItem->fromBasePath,
						 pItem->filename);
				hashcode = 0;
			}
			else
			{
				snprintf(segmentFileName, MAXPGPATH+1, "%s/%s/%s.%d",
						 pItem->baseDir, pItem->fromBasePath,
						 pItem->filename,
						 seg);
			}

			verifyLog(logControl,
					  DEBUG1,
					  "FileRepMirror_CompareSummaryItem Segfile	 %s",
					  segmentFileName);

			if (stat(segmentFileName, &info) < 0)
			{
				/*
				 *  file doesn't even exist - return with mismatch set to true
				 *  TODO we should note in this summary item that it just
				 *  doesn't exist
				 *  then if numMatches + numMissingOnMirror = numOnPrimary
				 *  we wouldn't even have
				 *  to do CompareDirectoryContents
				 */
				verifyLog(logControl,
						  LOG,
						  "VERIFICATION_ERROR_REPORT MIRROR: File %s does not exist on the mirror",
						  segmentFileName);
				*mismatch = true;
				return STATUS_OK;
			}

			/* TODO */
#if 0
			/* this check was moved into CompareEnhanced - it works there but would be better
			 * to have mirror add this into the fix/diff record here and then return an indication
			 * that there is no need to try the enhanced compare in that case
			 */

			if (pItem->summary.fileSummary.exception == FileRepVerifyCompareException_HeapRelation)
			{
				if (info.st_size > RELSEG_SIZE* BLCKSZ)
				{

					/* TODO  it would be good to write fix and diff table records here on the mirror
					 * saying that the file is too long - the error on the primary is not that specific
					 * we would need to send over primaryDirectoryName for enhanced block compares like
					 * we do for directory contents
					 */
					verifyLog(logControl,
							  LOG,
							  "VERIFICATION_ERROR_REPORT MIRROR: Heap file %s is larger than RELSEG_SIZE %d - that should never be true",
							  segmentFileName,
							  RELSEG_SIZE*BLCKSZ);
					*mismatch = true;
					return STATUS_OK;
				}

			}
#endif
			if (includeFileSizeInfo && (seg == numSegments - 1))
			{
				if (info.st_size != pItem->summary.fileSummary.fileSize)
				{
					/* sizes don't match - return with mismatch set to true */
					verifyLog(
							  logControl,
							  LOG,
							  "VERIFICATION_ERROR_WARNING MIRROR: File %s on mirror is different size %d vs %d on primary",
							  segmentFileName,
							  (int) info.st_size,
							  (int) pItem->summary.fileSummary.fileSize);
					*mismatch = true;
					return status;
				}
			}

			if (doHash)
			{
				status = FileRepVerify_ComputeFileHash(
													   /* this is mirror's fileRepIdentifier that was sent over */
													   segmentFileName,
													   &thisHashcode,
													   pItem->summary.fileSummary.exception,
													   pItem->summary.fileSummary.AO_logicalEof,
													   seg,
													   logControl);
				if (status != STATUS_OK)
				{
					verifyLog(
							  logControl,
							  LOG,
							  "VERIFICATION_ERROR_REPORT MIRROR: Could not compute hash on file %s ",
							  segmentFileName);
					*mismatch = true;
					/* Don't let this kill the verification process! */
					return STATUS_OK;
				}

				if (seg ==0)
				{
					hashcode = thisHashcode;
				}
				else
				{
					hashcode += thisHashcode;
				}
			}
		}
		//EQ_CRC32???
		if (doHash && (hashcode != pItem->summary.fileSummary.fileContentsHashcode))
		{
			/* hashcodes don't match -but just a warning -
			 * there are reasons why this could legitimately
			 * mismatch
			 */
			verifyLog(logControl,
					  LOG,
					  "VERIFICATION_ERROR_WARNING MIRROR: Hash for file %s does not match (primary %d mirror %d)",
					  segmentFileName,
					  pItem->summary.fileSummary.fileContentsHashcode,
					  hashcode);
			*mismatch = true;
			return status;
		}
		*mismatch = false;
		return status;
	}
	else
	{
		char directoryName[MAXPGPATH+1];
		DIR	 *dir = NULL;
		struct dirent *de = NULL;
		uint32 numImmediateChildren =0;
		struct stat	 child_st;
		char		 child_path[MAXPGPATH+1];
		pg_crc32 hashcode = 0;

		snprintf(directoryName, MAXPGPATH+1, "%s/%s",
				 pItem->baseDir, pItem->fromBasePath);


		if (doHash)
		{
			//NEW_HASH
			INIT_CRC32C(hashcode);
		}

		verifyLog(logControl,
				  DEBUG1,
				  "FileRepMirror_CompareSummaryItem DIRECTORY %s",
				  directoryName);


		/* open up the directory and see how many immediate children
		 *  and compute hash over their names
		 *  TODO: Factor out this code and call same on primary and mirror
		 */
		dir = AllocateDir(directoryName);
		if (dir == NULL)
		{
			verifyLog(logControl,
					  LOG,
					  "VERIFICATION_ERROR_REPORT MIRROR: cannot read directory %s on mirror (AllocateDir)",
					  directoryName);
			*mismatch = true;
			return STATUS_OK;
		}


		while ((de = ReadDir(dir, directoryName)) != NULL)
		{
			if (strcmp(de->d_name, ".") == 0 ||
				strcmp(de->d_name, "..") == 0)
			{
				continue;
			}

			snprintf(child_path,
					 MAXPGPATH+1,
					 "%s/%s",
					 directoryName, de->d_name);

			if (0 != stat(child_path, &child_st))
			{
				/* this was returned by ReadDir so we should be able
				 *  to do a stat on this
				 */
				verifyLog(logControl,
						  LOG,
						  "VERIFICATION_ERROR_INTERNAL: cannot do a stat on %s",
						  child_path);
				*mismatch = true;

				FreeDir(dir);
				return STATUS_OK;
			}

			if ((S_ISDIR(child_st.st_mode) &&
				 !isExcludedDirectory(
					 de->d_name,
					 gMirrorRequestParams.numIgnoreDirs,
					 gMirrorRequestParams.ignoreDirs))
				||
				(S_ISREG(child_st.st_mode) &&
				 !isExcludedFile(
					 de->d_name,
					 gMirrorRequestParams.numIgnoreFilePatterns,
					 gMirrorRequestParams.ignoreFiles)))
			{
				numImmediateChildren++;
				if (doHash)
				{
					//NEW_HASH
					COMP_CRC32C(
						hashcode,
						de->d_name,
						strlen(de->d_name));
				}

			} else {
				verifyLog(logControl, DEBUG3,
						  "FileRepMirror_CompareDirectoryContents 1: Skipping %s because excluded",
						  de->d_name);

			}
		}

		//NEW_HASH
		FIN_CRC32C(hashcode);

		FreeDir(dir);

		if (numImmediateChildren !=
			pItem->summary.dirSummary.numImmediateChildren)
		{
			/*
			 *  sizes don't match - return with mismatch set to true
			 *  Can report mismatch here - compare directory contents
			 *  will give more info but know its wrong here
			 *  is there a way to lock to prevent changes in flight?
			 */
			verifyLog(
					  logControl,
					  LOG,
					  "VERIFICATION_ERROR_REPORT MIRROR: Directory %s number of children on primary %d does not match number of children on mirror %d",
					  directoryName,
					  pItem->summary.dirSummary.numImmediateChildren,
					  numImmediateChildren);
			*mismatch = true;
			return STATUS_OK;
		}

		if (!doHash)
		{
			/* don't even check the hashcode - set mismatch to false */
			*mismatch = false;
			return status;
		}
		else
		{

			//EQ_CRC32???
			if (hashcode ==
				pItem->summary.dirSummary.childrenListHashcode)
			{
				*mismatch = false;
			}
			else
			{
				verifyLog(
						  logControl,
						  LOG,
						  "VERIFICATION_ERROR REPORT MIRROR: Hash of directory contents for directory %s does not match",
						  directoryName);
				*mismatch = true;
			}
			return STATUS_OK;
		}
	}

	/*  TODO: Update the global count of total blocks to be verified */

	return STATUS_OK;
}

int
FileRepMirror_CompareAllSummaryItems(
									 FileRepMessageHeader_s		*fileRepMessageHeader,
									 char						*fileRepMessageBody,
									 void						**response,
									 uint32						*responseSizeBytes,
									 FileRepVerifyLogControl_s	logControl)
{
	int status = STATUS_OK;
	int numSummaryItems =0;
	int numMismatches=0, numFiles=0, numDirs=0;
	int i=0;
	bool doHash = false;
	bool includeFileSizeInfo = false;

	Assert(fileRepMessageHeader->fileRepOperation ==
		   FileRepOperationVerify);
	Assert(fileRepMessageHeader->fileRepOperationDescription.verify.type ==
		   FileRepOpVerifyType_SummaryItems) ;

	/* REALLY NO NEED TO COPY OUT INTO LOCAL VARIABLES - JUST MORE READABLE */
	numSummaryItems =
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.summaryItems.num_entries;
	doHash =
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.summaryItems.hashcodesValid;
	includeFileSizeInfo =
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.summaryItems.sizesValid;

	*response = NULL;
	*responseSizeBytes = 0;

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepMirror_CompareAllSummaryItems numSummaryItems %d",
			  numSummaryItems);
	numMismatches = 0;
	numFiles = 0;
	numDirs = 0;
	for (i=0; i< numSummaryItems; i++)
	{
		bool mismatch = false;
		FileRepVerifySummaryItem_s *pItem = NULL;
		FileRepVerifySummaryItem_s *itemArray = (FileRepVerifySummaryItem_s *)fileRepMessageBody;

		pItem = &(itemArray[i]);

		if(pItem->isFile)
		{
			numFiles++;
		}
		else
		{
			numDirs++;
		}

		status = FileRepMirror_CompareSummaryItem(
												  pItem,
												  doHash,
												  includeFileSizeInfo,
												  &mismatch,
												  logControl);
		if ( status != STATUS_OK)
		{
			return status;
		}
		if (mismatch)
		{
			char * intoResponse = NULL;

			/* if first mismatch - allocated responseBuffer */
			if (numMismatches == 0)
			{
				/* response gets freed in FileRepMirror_RunConsumer */
				*response = (char *) palloc(FILEREP_MESSAGEBODY_LEN);
				if (*response == NULL)
				{
					ereport(WARNING,
							(errmsg("verification failure, "
									"could not allocate memory to run verification token '%s' "
									"verification aborted",
									logControl.logOptions.token),
							 errhint("run gpverify")));

					return STATUS_ERROR;
				}
				verifyLog(logControl,
						  DEBUG3,
						  "FileRepMirror_CompareAllSummaryItems *response %p",
						  *response);
			} else {
				Assert(FILEREP_MESSAGEBODY_LEN >= numMismatches*sizeof(FileRepVerifySummaryItem_s));
			}
			/* copy the summary item into the response at the proper place */
			*responseSizeBytes += sizeof(FileRepVerifySummaryItem_s);
			intoResponse = (char *)(*response) +
				(numMismatches*sizeof(FileRepVerifySummaryItem_s));


			memcpy(intoResponse,
				   pItem,
				   sizeof(FileRepVerifySummaryItem_s));
			numMismatches++;
		}
	}

	Assert(fileRepMessageHeader->fileRepOperation == FileRepOperationVerify);
	fileRepMessageHeader->fileRepOperationDescription.verify.desc.summaryItems.num_entries =
		numMismatches;

	verifyLog(
		logControl,
		DEBUG1,
		"FileRepMirror_CompareAllSummaryItems numSummaryItems %d of %d mismatches, numFiles %d numDirs %d",
		numMismatches,
		numSummaryItems,
		numFiles,
		numDirs);
	return STATUS_OK;
}

int
FileRepMirror_CompareEnhancedBlockHash(
									   FileRepMessageHeader_s		*fileRepMessageHeader,
									   char							*fileRepMessageBody,
									   void							**response,
									   uint32						*responseSizeBytes,
									   FileRepVerifyLogControl_s	logControl )
{



	FILE  *fd = NULL;
	FileName fileName = NULL;

	struct stat	 info;
	char * buf = NULL;

	FileRepIdentifier_u		fileRepIdentifier;
	FileRepRelationType_e	fileRepRelationType;
	uint64 fileOffset = 0;
	uint64 dataLength = 0;
	FileRepVerify_CompareException_e exception = FileRepVerifyCompareException_None;
	bool success = false;
	pg_crc32 hashcode = 0;
	uint32 bytesRead = 0;
	bool lastBlock = false;


	fileRepRelationType = fileRepMessageHeader->fileRepRelationType;

	Assert(fileRepRelationType == FileRepRelationTypeFlatFile);
	fileRepIdentifier = fileRepMessageHeader->fileRepIdentifier;

	fileOffset = fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.offset;
	dataLength = fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.dataLength;
	exception  =  fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.exception;
	lastBlock =  fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.lastBlock;

	fileName = FileRep_GetFileName(
								   fileRepIdentifier,
								   fileRepRelationType);

	verifyLog(logControl,
			  LOG,
			  "FileRepMirror_CompareEnhancedBlockHash:File %s fileOffset %d \n",
			  fileName,
			  fileOffset);

	errno = 0;
	if (stat(fileName, &info) <	 0)
	{
		/* file does not exist on mirror - return out of here */
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_REPORT MIRROR: File %s exists on the primary but not on the mirror\n",
				  fileName);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not find file '%s' verification token '%s' : %m "
						"verification aborted ",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.isOnMirror = false ;
		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}
	else
	{
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.isOnMirror = true ;
	}

	/* Is it actually a file? */
	if (! S_ISREG(info.st_mode))
	{
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_REPORT MIRROR: %s is on mirror but its not a regular file as it is on the primary\n",
				  fileName);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"not a regular file '%s' verification token '%s' : %m "
						"verification aborted ",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.isOnMirror = false;
		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}


	if (exception == FileRepVerifyCompareException_HeapRelation)
	{
		if (info.st_size > RELSEG_SIZE* BLCKSZ)
		{

			/* TODO  it would be good to write fix and diff table records here on the mirror
			* saying that the file is too long - the error on the primary is not that specific
			* we would need to send over primaryDirectoryName for enhanced block compares like
			* we do for directory contents
			*/
			verifyLog(logControl,
					  LOG,
					  "VERIFICATION_ERROR_REPORT MIRROR: Heap file %s is larger than RELSEG_SIZE %d - that should never be true",
					  fileName,
					  RELSEG_SIZE*BLCKSZ);
			success = false;
			goto FileRepMirror_CompareEnhancedBlockHash_Return;
		}

	}
	if (dataLength > BLCKSZ)
	{
		/* VERIFICATION_ERROR_INTERNAL - should this be handled differently? */
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_INTERNAL: FileRepMirror_CompareEnhancedBlockHash:	Data length (%d) should always be <= BLCKSZ (%d)",
				  dataLength,
				  BLCKSZ);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"length '" INT64_FORMAT "' bigger than '%d' file '%s' verification token '%s' : %m "
						"verification aborted ",
						dataLength,
						BLCKSZ,
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}

	if (info.st_size < fileOffset + dataLength)
	{
		/* the file on the mirror is not long enough to include
		   this whole block
		*/
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_WARNING MIRROR: File on mirror is shorter than on primary; for some files this is ok ");

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"mirror file '%s' is shorter ' " INT64_FORMAT " ' than primary offset ' " INT64_FORMAT " ' length '" INT64_FORMAT "' verification token '%s' : %m "
						"verification aborted ",
						fileName,
						info.st_size,
						fileOffset,
						dataLength,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}

	if (lastBlock &&
		(exception != FileRepVerifyCompareException_AORelation) &&
		(info.st_size > fileOffset + dataLength))
	{
		/* the file on the mirror is longer than the file on the primary
		 * for AO this is ok because it is ok to have data past the
		 * logical end of file
		 */
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_WARNING MIRROR: File on mirror is longer than on primary; for some files this is ok ");

		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"mirror file '%s' is longer ' " INT64_FORMAT " ' than primary offset ' " INT64_FORMAT " ' length '" INT64_FORMAT "' verification token '%s' : %m "
						"verification aborted ",
						fileName,
						info.st_size,
						fileOffset,
						dataLength,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;

	}

	buf = (char *) palloc(dataLength + 1 );
	if (buf == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						logControl.logOptions.token),
				 errhint("run gpverify")));

		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}

	/* TODO
	 * For file operations such as open(), read(), close(), ... is recommended
	 * to use postgres interfaces  File_Read(), File_Close(),
	 * PathNameOpenFile()
	 * ... See routines in ../storage/file/fd.c. The reason is to use the VFD
	 * (virtual file descriptors) abstraction level, which provides protection
	 * against descriptor leaks as well as management of files that need to be
	 *  open for more than a short period of time.
	 */
	errno = 0;
	fd = fopen(fileName, "r");
	if (fd == NULL)
	{
		/* log, put on retry list and continue verification */
		verifyLog(
				  logControl,
				  LOG,
				  "VERIFICATION_ERROR_INTERNAL - we can stat file %s but not open it - why?",
				  fileName);

		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' verification token '%s' : %m "
						"verification aborted ",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		success = false;
		goto FileRepMirror_CompareEnhancedBlockHash_Return;
	}

	fseek(fd, fileOffset, SEEK_SET);

	if (dataLength == 0)
	{
		hashcode = 0;
	}
	else
	{
		/* read the corresponding section on the mirror */
		bytesRead = fread(buf, 1, dataLength, fd);

		if (bytesRead != dataLength)
		{
			/* file on mirror is shorter than the one on primary
			 * VERIFICATION_ERROR_INTERNAL: we checked the file
			 * length above we should be able to read it - why can't we
			 */
			verifyLog(
				logControl,
				LOG,
				"VERIFICATION_ERROR_REPORT MIRROR: file %s should be long enough but read (offset %d len %d) only returns %d bytes ",
				fileName,
				fileOffset,
				dataLength,
				bytesRead );

			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not read file '%s' offset '" INT64_FORMAT "' length requested '" INT64_FORMAT "' "
							"length read '%u' : %m "
							"verification aborted",
							fileName,
							fileOffset,
							dataLength,
							bytesRead),
					 errhint("run gpverify")));

			success = false;

			goto FileRepMirror_CompareEnhancedBlockHash_Return;
		}
		else
		{
			/* compare this chunk of the file on mirror
			 * The logical eof value is handled on the mirror for AO files
			 * But heap relations and btree indices need masking on the
			 * mirror as well
			 */
			if (exception ==
				FileRepVerifyCompareException_HeapRelation)
			{
				Page page = (Page) buf;
				bool madeChange;
				bool validPage;

				validPage = FileRepVerify_ProcessHeapPage(page, &madeChange, logControl);

				if (!validPage)
				{
					verifyLog(
						logControl,
						LOG,
						"VERIFICATION_ERROR_REPORT MIRROR: corrupt heap file detected on mirror file %s offset %d, len %d",
						fileName,
						fileOffset,
						dataLength);
				}
			}
			else if (exception ==
					 FileRepVerifyCompareException_BtreeIndex)
			{
				Page page = (Page) buf;
				bool madeChange = false;
				bool validPage = false;

				validPage = FileRepVerify_ProcessBtreeIndexPage(
																page,
																&madeChange,
																logControl);

				if (!validPage)
				{
					verifyLog(
						logControl,
						LOG,
						"VERIFICATION_ERROR_REPORT MIRROR: corrupt btree index file detected on mirror file %s offset %d, len %d",
						fileName,
						fileOffset,
						dataLength);

				}
			}

			//NEW_HASH
			INIT_CRC32C(hashcode);
			COMP_CRC32C(hashcode, buf, dataLength);
			FIN_CRC32C(hashcode);
		}
	}

	//EQ_CRC32???
	if (hashcode == fileRepMessageHeader->fileRepOperationDescription.verify.desc.enhancedBlockHash.hashcode)
	{
		verifyLog(
			logControl,
			DEBUG1,
			"Verification SUCCEEDED onthe mirror; enhanced block hash shows they match ");

		success = true;
	}
	else
	{
		verifyLog(
			logControl,
			DEBUG1,
			"Verification FAILED onthe mirror; enhanced block hash failed ");

		success = false;
	}

FileRepMirror_CompareEnhancedBlockHash_Return:

	if (fd > 0)
	{
		fclose(fd);
	}

	if (buf != NULL)
	{
		pfree(buf);
	}

	if (fileName)
	{
		pfree(fileName);
	}

	*responseSizeBytes = sizeof (bool);
	/* response gets freed in FileRepMirror_RunConsumer */
	*response = (char *) palloc(*responseSizeBytes);
	if (*response == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	memcpy(*response, &success, *responseSizeBytes);

	return STATUS_OK;
}


int
FileRepMirror_CompareDirectoryContents(
									   FileRepMessageHeader_s		*fileRepMessageHeader,
									   char							*fileRepMessageBody,
									   void							**response,
									   uint32						*responseSizeBytes,
									   FileRepVerifyLogControl_s	logControl)
{

	int						status = STATUS_OK;
	FileRepIdentifier_u		fileRepIdentifier;
	FileRepRelationType_e	fileRepRelationType;
	int currOffset =0;
	FileName   directoryName;
	DIR *dir = NULL;
	struct dirent	*de = NULL;
	struct stat	 mirrorDir_st;
	struct stat	 child_st;
	char		 child_path[MAXPGPATH+1];
	bool foundLastChild = false;
	bool startOver = false;
	uint32 numThisSend =0;
	char * childrenList;
	bool firstBatch = false;
	int lastChildLen = 0;
	char lastChild[MAXPGPATH+1];
	bool bufferFull = false;
	
	/* 
	 * Note: we get extra_p files just fine without this directory contents
	 * comparison  - this is looking for extra_m files
	 * primary asks mirror to send a chunk of its directory listing - until
	 * mirror says it is the finalBatch - 
	 * with this primary can find the extra_m files and mirror doesn't have
	 * to generate fix files at all
	 */


	fileRepRelationType	= fileRepMessageHeader->fileRepRelationType;
	fileRepIdentifier = fileRepMessageHeader->fileRepIdentifier;
	firstBatch =
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContents.firstBatch;

	directoryName = FileRep_GetFileName(fileRepIdentifier, fileRepRelationType);

	verifyLog(logControl, 
			  DEBUG1, 
			  "FileRepMirror_CompareDirectoryContents mirrorDirectory %s firstBatch %d", 
			  directoryName, firstBatch);
	
	
	/* Check that this directory on the mirror exists */
	if (stat(directoryName, &mirrorDir_st) != 0) 
	{
	/* directory does not even exist */
		verifyLog(
			logControl,
			LOG,
			"VERIFICATION_ERROR_REPORT MIRROR: Directory %s exists on the primary but not on the mirror\n",
			directoryName);
		/* FORMAT RESPONSE and RETURN */
		/* if numChildrenMissingOnMirror == numChildrenOnPrimary and endCompares = true => error*/
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.finalBatch = true;
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.numChildrenInBatch = 0;
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.startOver = false;
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.directoryExists = false;

		*responseSizeBytes = 0;

		pfree(directoryName);
		return status;

	}

	childrenList = (char *) palloc(FILEREP_MESSAGEBODY_LEN);
	if (childrenList == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						logControl.logOptions.token),
				 errhint("run gpverify")));
		
		pfree(directoryName);
		return STATUS_ERROR;
	}


	/* Allocate the directory */
	if ((dir = AllocateDir(directoryName)) == NULL)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not read directory '%s' verification token '%s' : %m "
						"verification aborted",
						directoryName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}


	if (!firstBatch)
	{
		/* read the lastChild from the message body */
		lastChildLen = (int) (*fileRepMessageBody);
		
		Assert( lastChildLen < MAXPGPATH);
		memcpy(lastChild,
			   fileRepMessageBody+ sizeof(int),
			   lastChildLen);
		/* make sure there is a '\0' there? not necessary? */
		lastChild[lastChildLen] = '\0';

		verifyLog(logControl,
				  DEBUG5,
				  "lastChild is %s lastChildLen %d\n",
				  lastChild,
				  lastChildLen);

		/* Advance the directory on mirror until we find lastChild */
		foundLastChild = false;
		verifyLog(logControl, DEBUG5, "Looking for lastChild %s in directory %s",
				  lastChild,
				  directoryName);

		/* once we've found the last child importat that we don't read another de 
		 * short circuit evaluation of this condition is important
		 */
		while ( !foundLastChild && ((de = ReadDir(dir, directoryName)) != NULL))
		{

			
			if (strcmp(de->d_name, ".") == 0 ||
				strcmp(de->d_name, "..") == 0)
			{
				continue;
			}

			/* strlen(de->d_name does not include the \0 and lastChildLen does */
			if ((strncmp(lastChild, de->d_name, lastChildLen) == 0)
				&& (strlen(de->d_name)+1 == lastChildLen))
			{

				foundLastChild = true;

			}
			
		}

		/* if we don't find the last child then start back over from the beginning ? */
		if (!foundLastChild){

			verifyLog(
				logControl, 
				LOG, 
				"FileRepMirror_CompareDirectoryContents did not find lastChild %s in dir %s", 
				lastChild,
				directoryName);

			/* Because there is no locking on the directory this is possible but should be rare 
			 * - Primary will only allow MAX_START_OVERS to prevent an infinite loop
			 */
			FreeDir(dir);
			startOver = true;

			if ((dir = AllocateDir(directoryName)) == NULL)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not read directory '%s' verification token '%s' : %m "
								"verification aborted",
								directoryName,
								logControl.logOptions.token),
						 errhint("run gpverify")));
				
				return STATUS_ERROR;
			}
		}
	}

	
	/* Look over the mirror directory - copying each child into the response buffer
	 * while we have room
	 */
	bufferFull = false;
	currOffset = 0;
	while (((de = ReadDir(dir, directoryName)) != NULL) && !bufferFull)
	{

		if (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0)
		{
			continue;
		}

		snprintf(child_path,
				 MAXPGPATH+1,
				 "%s/%s",
				 directoryName,
				 de->d_name);

		if (stat(child_path, &child_st) != 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("verification failure, "
							"could not stat file '%s' verification token '%s' : %m "
							"verification aborted",
							child_path,
							logControl.logOptions.token),
					 errhint("run gpverify")));

			//Why just continue?
			/* FreeDir(dir);
			 * return STATUS_ERROR;
			 */
			continue;
		}

		if ((S_ISDIR(child_st.st_mode) &&
			 !isExcludedDirectory(
				 de->d_name,
				 gMirrorRequestParams.numIgnoreDirs,
				 gMirrorRequestParams.ignoreDirs)) ||
			(S_ISREG(child_st.st_mode) &&
			 !isExcludedFile(
				 de->d_name,
				 gMirrorRequestParams.numIgnoreFilePatterns,
				 gMirrorRequestParams.ignoreFiles)))
		{
			int thisLen = strlen(de->d_name)+1;

			if (currOffset + thisLen + sizeof(int) >= FILEREP_MESSAGEBODY_LEN)
			{

				/* The current message is too full to include this last piece 
				 *  Send the current message and then start fresh with this child
				 */

				bufferFull = true;
			
				
			} else {
				
				/* Copy this item into place */
				verifyLog(
					logControl,
					DEBUG5,
					"Child %d: name %s len %d",
					numThisSend, de->d_name, thisLen);
				memcpy(childrenList+ currOffset, &thisLen, sizeof(int));
				currOffset+=sizeof(int);
				numThisSend++;
				
				memcpy(childrenList+ currOffset, de->d_name, thisLen);
				currOffset+=thisLen;
			}
		} else {
			verifyLog(logControl, DEBUG3,
					  "FileRepMirror_CompareDirectoryContents 4: Skipping %s because excluded",
					  de->d_name);
		}
	
	}

	/* FORMAT RESPONSE */
	fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.finalBatch = !bufferFull;
	fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.numChildrenInBatch = numThisSend;
	fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.startOver = startOver;
	fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.directoryExists = true;



	/* response gets freed in FileRepMirror_RunConsumer */
	*response = childrenList;
	*responseSizeBytes = currOffset;

	pfree(directoryName);
	FreeDir(dir);
	return status;


}

int
FileRepMirror_ExchangeRequestParameters(
									  FileRepMessageHeader_s	*fileRepMessageHeader,
									  char						*fileRepMessageBody,
									  void						**response,
									  uint32					*responseSizeBytes,
									  FileRepVerifyLogControl_s	logControl)
{

	FileRepOperationVerifyParameterType_e type =
		fileRepMessageHeader->fileRepOperationDescription.verify.desc.requestParameters.type;

	/* safer and cheaper to just once at the beginning and not again? */
	getcwd(gMirrorBaseDir, MAXPGPATH+1);
	
	*responseSizeBytes = MAXPGPATH+1;

	/* response gets freed in FileRepMirror_RunConsumer */
	*response = (char *) palloc(*responseSizeBytes);
	if (*response == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification token '%s' "
						"verification aborted",
						logControl.logOptions.token),
				 errhint("run gpverify")));
		
		return STATUS_ERROR;
	}
	strncpy((char*) *response, gMirrorBaseDir, MAXPGPATH+1);
	verifyLog(logControl, LOG, "FileRepMirror_ExchangeRequestParameters START gMirrorBaseDir %s response %s responseSizeBytes %d", 
			  gMirrorBaseDir,
			  (char *) *response,
			  *responseSizeBytes
		);

	verifyLog(logControl, LOG, "FileRepMirror_ExchangeRequestParameters type %d", type);

	if (type ==	 FileRepOpVerifyParamterType_IgnoreInfo)
	{
		int i;
		char * thisLoc;

		if (gMirrorRequestParams.valid)
		{
			/* deallocate what is there */
			for (i=0; i < gMirrorRequestParams.numIgnoreFilePatterns; i++)
			{
				pfree (gMirrorRequestParams.ignoreFiles[i]);
			}
			gMirrorRequestParams.numIgnoreFilePatterns = 0;
			pfree (gMirrorRequestParams.ignoreFiles);

			for (i=0; i < gMirrorRequestParams.numIgnoreDirs; i++)
			{
				pfree (gMirrorRequestParams.ignoreDirs[i]);
			}
			gMirrorRequestParams.numIgnoreDirs = 0;
			pfree (gMirrorRequestParams.ignoreDirs);

			gMirrorRequestParams.valid = false;

		}
		gMirrorRequestParams.numIgnoreFilePatterns =
			fileRepMessageHeader->fileRepOperationDescription.verify.desc.requestParameters.numFileIgnore;
		gMirrorRequestParams.numIgnoreDirs =
			fileRepMessageHeader->fileRepOperationDescription.verify.desc.requestParameters.numDirIgnore;

		verifyLog(
			logControl, DEBUG1,
			"FileRepMirror_ExchangeRequestParameters numFileIgnore %d",
			gMirrorRequestParams.numIgnoreFilePatterns);
		verifyLog(
			logControl, DEBUG1,
			"FileRepMirror_ExchangeRequestParameters numDirIgnore %d",
			gMirrorRequestParams.numIgnoreDirs);

		/* get actual ignore info out of fileRepMessageBody */
		thisLoc = (char *)fileRepMessageBody;

		gMirrorRequestParams.ignoreFiles = (char **) palloc(gMirrorRequestParams.numIgnoreFilePatterns * sizeof(char *));
		if (gMirrorRequestParams.ignoreFiles == NULL)
		{
			ereport(WARNING,
					(errmsg("verification failure, "
							"could not allocate memory to run verification token '%s' "
							"verification aborted",
							logControl.logOptions.token),
					 errhint("run gpverify")));

			return STATUS_ERROR;
		}
		for (i=0; i < gMirrorRequestParams.numIgnoreFilePatterns; i++)
		{
			int * thisLen = (int *) thisLoc;
			char * thisString;

			gMirrorRequestParams.ignoreFiles[i] = (char *) palloc ((*thisLen + 1) * sizeof(char));
			if (gMirrorRequestParams.ignoreFiles[i] == NULL)
			{
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to run verification token '%s' "
								"verification aborted",
								logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}

			thisString = (char *) (thisLen +1);
			strncpy(gMirrorRequestParams.ignoreFiles[i],
					thisString, *thisLen+1);
			verifyLog(
				logControl, DEBUG1,
				"FileRepMirror_ExchangeRequestParameters fileIgnore %d len %d is %s",
				i, *thisLen, gMirrorRequestParams.ignoreFiles[i]);

			thisLoc = thisString + *thisLen +1;
		}

		gMirrorRequestParams.ignoreDirs = (char **) palloc(gMirrorRequestParams.numIgnoreDirs * sizeof (char *));
		if (gMirrorRequestParams.ignoreDirs == NULL)
		{
			ereport(WARNING,
					(errmsg("verification failure, "
							"could not allocate memory to run verification token '%s' "
							"verification aborted",
							logControl.logOptions.token),
					 errhint("run gpverify")));
			return STATUS_ERROR;
		}

		for (i=0; i < gMirrorRequestParams.numIgnoreDirs; i++)
		{

			int * thisLen = (int *) thisLoc;
			char * thisString;
			gMirrorRequestParams.ignoreDirs[i] = (char *) palloc ((*thisLen+1) * sizeof(char));
			if (gMirrorRequestParams.ignoreDirs[i] == NULL)
			{
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to run verification token '%s' "
								"verification aborted",
								logControl.logOptions.token),
						 errhint("run gpverify")));
				return STATUS_ERROR;
			}

			thisString = (char *) (thisLen +1);
			strncpy(gMirrorRequestParams.ignoreDirs[i],
					thisString, *thisLen+1);
			verifyLog(
				logControl, DEBUG1,
				"FileRepMirror_ExchangeRequestParameters dirIgnore %d len %d is %s",
				i, *thisLen, gMirrorRequestParams.ignoreDirs[i]);

			thisLoc = thisString + *thisLen + 1;

		}
		strncpy(
			gMirrorRequestParams.token,
			fileRepMessageHeader->fileRepOperationDescription.verify.desc.requestParameters.token,
			FILEREP_VERIFY_MAX_REQUEST_TOKEN_LEN+1);


		gMirrorRequestParams.valid = true;
	} 


	verifyLog(logControl, LOG, "FileRepMirror_ExchangeRequestParameters END gMirrorBaseDir %s response %s responseSizeBytes %d", 
			  gMirrorBaseDir,
			  (char *) *response,
			  *responseSizeBytes
		);
	return STATUS_OK;
}


/*
 * Page will be altered to mask out the bits that may legitimately
 * differ between the primary and the mirror
 * Make sure changes made to page will just be reflected in this
 * local buffer and not anywhere else
*/
static bool
FileRepVerify_ProcessHeapPage(
							  Page page,
							  bool * madeChange,
							  FileRepVerifyLogControl_s logControl)
{
	int	 maxOffset = PageGetMaxOffsetNumber(page);
	int	 whichItem = 0;


	if (!((maxOffset >=0) && (maxOffset <= BLCKSZ)))
	{
		/* Assert((maxOffset >=0) && (maxOffset <=BLCKSZ)); */
		verifyLog(
				  logControl,
				  LOG,
				  "FileRepVerify_ProcessHeapPage: CORRUPT/INVALID PAGE maxOffset %d MaxHeapTuplesPerPage %d BLCKSZ %d\n",
				  maxOffset, MaxHeapTuplesPerPage, BLCKSZ);
		return false;
	}
	if (!((maxOffset >=0) && (maxOffset <=MaxHeapTuplesPerPage)))
	{
		/* Is MaxHeapTuplesPerPage a better upper bound?
		 * Assert((maxOffset >=0) && (maxOffset <=MaxHeapTuplesPerPage));
		 */
		verifyLog(
				  logControl,
				  LOG,
				  "FileRepVerify_ProcessHeapPage: CORRUPT/INVALID maxOffset %d MaxHeapTuplesPerPage %d BLCKSZ %d\n",
				  maxOffset, MaxHeapTuplesPerPage, BLCKSZ);
		return false;
	}


	*madeChange = false;
	for (whichItem = 1; whichItem < (maxOffset + 1); whichItem++)
	{
		ItemId			itemId;
		HeapTupleHeader htup;

		itemId = PageGetItemId(page, whichItem);

		if (ItemIdIsUsed(itemId))
		{
			htup = (HeapTupleHeader)(PageGetItem((Page)page, itemId));
			/* Jeff says he believes completely clearing both infomask
			 *  and infomask2  is overkill
			 *  look into clearing just
			 *  HEAP_XMIN_COMMITED, HEAP_XMIN_?, HEAP_XMAX_?
			 */
			if (htup->t_infomask != 0)
			{
				htup->t_infomask = 0;
				*madeChange = true;
			}
			if (htup->t_infomask2 != 0)
			{
				htup->t_infomask2 = 0;
				*madeChange = true;
			}

		}

	}
	return true;
}

/*
 *   Page will be altered to mask out the bits that may legitimately
 *   differ between the primary and the mirror
 *   Make sure changes made to page will just be reflected in this
 *   local buffer and not anywhere else
*/
static bool
FileRepVerify_ProcessBtreeIndexPage(
									Page page,
									bool * madeChange,
									FileRepVerifyLogControl_s logControl)
{

	BTPageOpaque opaque;
	int	 maxOffset = PageGetMaxOffsetNumber(page);
	int	 whichItem = 0;

	opaque = (BTPageOpaque) PageGetSpecialPointer(page);

	*madeChange = false;
	if (P_HAS_GARBAGE(opaque))
	{
		/* Page does have garbage - clear that */
		opaque->btpo_flags &= ~BTP_HAS_GARBAGE;

		Assert(!(P_HAS_GARBAGE(opaque)));

		*madeChange=true;
	}

	/* if this is a meta	 page, there there are no itemIds so return
	 * without attempting to process them
	 */
	if (opaque->btpo_flags & BTP_META)
	{
		if (BTPageGetMeta(page)->btm_magic != BTREE_MAGIC)
		{
			/* Assert(BTPageGetMeta(page)->btm_magic == BTREE_MAGIC); */
			verifyLog(
					  logControl,
					  LOG,
					  "FileRepVerify_ProcessBtreeIndexPage: CORRUPT/INVALID PAGE btm_magic %d != BTREE_MAGIC %d\n",
					  BTPageGetMeta(page)->btm_magic, BTREE_MAGIC);
			return false;
		}
		return true;
	}

	if (!((maxOffset >=0) && (maxOffset <= BLCKSZ)))
	{
		/* Assert((maxOffset >=0) && (maxOffset <=BLCKSZ)); */
		verifyLog(
				  logControl,
				  LOG,
				  "FileRepVerify_ProcessBtreeIndexPage: CORRUPT/INVALID PAGE maxOffset %d MaxHeapTuplesPerPage %d BLCKSZ %d\n",
				  maxOffset,
				  MaxHeapTuplesPerPage, BLCKSZ);
		return false;
	}

	/* walk the items and clear the LP_DELETE FLAG */
	for (whichItem = 1; whichItem < (maxOffset + 1); whichItem++)
	{
		ItemId	itemId;
		itemId = PageGetItemId(page, whichItem);
		if (ItemIdDeleted(itemId))
		{
			PageGetItemId(page, whichItem)->lp_flags &= ~LP_DELETE;
			*madeChange = true;
			Assert(!ItemIdDeleted(itemId));
		}
	}
	return true;
}


int FileRepPrimary_CompareDirectoryListing(
										FileRepVerifyRequest_s		*request,
										char						*primaryDirectoryName,
										char                        *mirrorDirectoryName,
										FileRepIdentifier_u			fileRepIdentifier,
										FileRepRelationType_e		fileRepRelationType,
										int							*numChildrenAddedOnMirror,
										FileRepVerifyLogControl_s	logControl)
{

	/* The purpose of this is get a list of files that are on the mirror but not on the primary
	 * We get the extra files on primary but not mirror another way
	 */
	int		status = STATUS_ERROR;
	bool finalBatch = false;
	bool startOver = false;
	bool firstBatch = true;
	char * lastChild = NULL;
	int  lastChildLen =0;
	char		*dataToMirror = NULL;
	int dataLength = 0;

	uint32							responseDataLength = 0;
	void							*responseData = NULL;
	FileRepOperationDescription_u	responseDesc;
	int child = 0;
	uint32 numChildrenInBatch = 0;
	int numStartOvers = 0;
	FileRepOperationDescription_u	fileRepOperationDescription;
	struct stat	 child_st;
	char		 primary_child_path[MAXPGPATH+1];
	char		 mirror_child_path[MAXPGPATH+1];
	int currOffset = 0;
	bool directoryExists = false;

	verifyLog(
		request->logControl,
		DEBUG1,
		"FileRepPrimary_CompareDirectoryListing primaryDir %s mirrorDir %s",
		primaryDirectoryName,
		mirrorDirectoryName);

	
	*numChildrenAddedOnMirror = 0;

	/* we are going to request that mirror send us a chunk of directory listing
	 * until they response that they have sent it all
	 */
	fileRepOperationDescription.verify.type = FileRepOpVerifyType_DirectoryContents;
	fileRepOperationDescription.verify.logOptions = logControl.logOptions;

	do 
	{

		fileRepOperationDescription.verify.desc.dirContents.firstBatch =
			firstBatch;
		
		if (!firstBatch){

			/* copy the lastChildLen and lastChild value into the dataToMirror buffer */
			dataLength = sizeof(int) + lastChildLen;
			
			dataToMirror = (char *)palloc(dataLength);
			if (dataToMirror == NULL)
			{
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory to run verification token '%s' "
								"verification aborted",
								request->logControl.logOptions.token),
						 errhint("run gpverify")));
				
				return STATUS_ERROR;
			}
			Assert(lastChild != NULL);
			Assert(lastChildLen >0);
			memcpy(dataToMirror, &lastChildLen, sizeof(int));
			memcpy((char*)(dataToMirror)+sizeof(int),
				   lastChild, lastChildLen);

			/* lastChild was a pointer into responseData
			 * now we are done with it so free it 
			 */
			if (responseData != NULL)
			{
				pfree(responseData);
				responseData = NULL;
			}
			
		}

		/* Ask Mirror to send us a batch */
		status = FileRepPrimary_MirrorVerify(
			fileRepIdentifier,
			fileRepRelationType,
			fileRepOperationDescription,
			dataToMirror,
			dataLength,
			&responseData,
			&responseDataLength,
			&responseDesc);
		
		if (dataToMirror != NULL)
		{
			pfree(dataToMirror);
		}

		if (status != STATUS_OK)
		{
			return status;
		}

		finalBatch = 	responseDesc.verify.desc.dirContentsMirror.finalBatch;
		numChildrenInBatch = 	responseDesc.verify.desc.dirContentsMirror.numChildrenInBatch;
		startOver = responseDesc.verify.desc.dirContentsMirror.startOver;
		directoryExists = responseDesc.verify.desc.dirContentsMirror.directoryExists;

		if (!directoryExists)
		{
			Assert(numChildrenInBatch ==0);
			writeFixFileRecord(
				logControl,
				"extra_p",
				fileRepPrimaryHostAddress,
				primaryDirectoryName,
				fileRepMirrorHostAddress,
				mirrorDirectoryName);
			
			writeDiffTableRecord(
				logControl,
				logControl.logOptions.token,
				"FileRepVerifyDirectoryMissingOnMirror",
				fileRepPrimaryHostAddress,
				primaryDirectoryName,
				fileRepMirrorHostAddress,
				mirrorDirectoryName,
				-1, -1, -1, -1, -1,
				NULL); /* Not applicable for directories */
		
			gFileRepVerifyShmem->request.isFailed = true;

		}

		if (startOver)
		{
			numStartOvers++;
			
/* if mirror does not find lastChild it will start over at the beginning 
 * only allow that to happen so many times to avoid infinite loop
*/
#define MAX_START_OVERS 5
			if (numStartOvers > MAX_START_OVERS)
			{

				ereport(WARNING,
						(errmsg("verification failure, "
								"maximum restarts exceeded when comparing directory contents for '%s' run verification token '%s' "
								"verification aborted",
								primaryDirectoryName,
								request->logControl.logOptions.token),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}
		}
		firstBatch =    false;
		
		verifyLog(
			request->logControl,
			DEBUG1,
			"FileRepPrimary_CompareDirectoryListing finalBatch %d numChildrenInBatch %d startOver %d numStartOvers %d responseDataLength %d directoryExists %d",
			finalBatch,
			numChildrenInBatch,
			startOver,
			numStartOvers,
			responseDataLength,
			directoryExists);
	  
		if (numChildrenInBatch > 0){
			Assert(responseData != NULL);
		}
		/* Walk through all the children sent by mirror
		 * for each one see if it is on the primary, if not
		 * then generate an extra_m record  and increment
		 * numChildrenAddedOnMirror
		 *
		 * Note the lastChild in this batch so we can send it to 
		 * mirror with the next request - so mirror knows where
		 * to start the next buffer 
		 */
		child = 0;
		currOffset = 0;
		while (child < numChildrenInBatch)
		{
			
			lastChildLen = (int)(*((char *)responseData+currOffset));
			Assert(lastChildLen > 1);
			lastChild = (char *)((char *)responseData+currOffset+sizeof(int));
			verifyLog(logControl,
					  DEBUG3,
					  "Child %d of %d 1: name %s len %d currOffset %d",
					  child,
					  numChildrenInBatch,
					  lastChild,
					  lastChildLen,
					  currOffset);
			
			snprintf(primary_child_path,
					 MAXPGPATH+1,
					 "%s/%s",
					 primaryDirectoryName,
					 lastChild);

			snprintf(mirror_child_path,
					 MAXPGPATH+1,
					 "%s/%s",
					 mirrorDirectoryName,
					 lastChild);
			
			if (stat(primary_child_path, &child_st) != 0)
			{
				(*numChildrenAddedOnMirror)++;
				verifyLog(logControl,
						  LOG,
						  "VERIFICATION_ERROR_REPORT MIRROR NEW1 Directory %s on mirror contains child %s len %d that is not found in the primary directory %s \n",
						  
						  mirrorDirectoryName,
						  lastChild,
						  lastChildLen,
						  primaryDirectoryName);

				writeFixFileRecord(logControl, 
								   "extra_m",
								   fileRepMirrorHostAddress,
								   mirror_child_path,
								   fileRepPrimaryHostAddress,
								   primary_child_path
					);
				
				writeDiffTableRecord(
					logControl,
					logControl.logOptions.token,
					"FileRepFileMissingOnPrimary",
					fileRepPrimaryHostAddress,
					primary_child_path,
					fileRepMirrorHostAddress,
					mirror_child_path,
					-1, -1, -1, -1, -1,
					NULL /* if file isn't on primary then it doesn't have a relnode */);
				
				
			}

			/* advance the child */
			child++;
			currOffset += sizeof(int);
			currOffset += lastChildLen;
		}
		
		
	} while (!finalBatch && directoryExists);

	if (responseData != NULL)
	{
		pfree(responseData);
		responseData = NULL;
	}


	return STATUS_OK;
	
}

/* this should change to sending a data block full of hash values?
 *   or perhaps we should try computing one block hash over the entire
 *  file just wtih locking
 *
 * Locking order is
 *			1) MirroredLock in LW_SHARED mode
 *			   It has to be acquired for
 *						a) Buffer Pool relation
 *						b) Append Only relation
 *						c) Flat files
 *			2) LockRelationForResynchronize in AccessExclusiveLock mode
 *			   It has to be acquired for
 *						a) Buffer Pool relation
 *						b) Append Only relation
*			3) LockBuffer in BUFFER_LOCK_EXCLUSIVE mode
 *			   It has to be acquired for
 *						a) Buffer Pool relation
 *				NOTE:
 *					Buffer has to be locked for reading page, computing hash on page,
 *					sending hash to mirror, reading page on mirror, computing hash on mirror,
 *					comparing hashes and sending ack back to primary.
 *					The reason for holding lock
 *					for so long time is that verification messages goes through different
 *					shared memory and so the messages ordering per file is not applied on mirror.
 */
int
FileRepPrimary_SendEnhancedBlockHashes(
									   FileRepRelFileNodeInfo_s		relinfo,
									   bool							*matched,
									   FileRepVerifyLogControl_s	logControl)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	int		status = STATUS_ERROR;
	struct	stat	 info;
	uint64	dataLength = 0;
	char	*dataToMirror = NULL;
	uint64	dataLengthLocal = 0;
	uint64	fileOffset = 0; //heap can require int64
	uint64	mirrorFileOffset = 0;
	uint64	bytesRead = 0;

	char	primaryFilename[MAXPGPATH+1];
	char	mirrorFilename[MAXPGPATH+1];

	FileRepIdentifier_u				fileRepIdentifier;
	FileRepRelationType_e			fileRepRelationType;
	FileRepOperationDescription_u	fileRepOperationDescription;


	uint32							responseDataLength = 0;
	void							*responseData = NULL;
	FileRepOperationDescription_u	responseDesc;

	SMgrRelation	smgr_relation = NULL;
	Buffer			buffer = 0;
	Page			page;
	BlockNumber		numBlocks = 0;
	BlockNumber		blkno=0;
	pg_crc32		hashcode = 0;
	bool			isBufferPoolRelation = true;
	uint32			nameLength = 0;
	FILE			*fd = NULL;
	char			*buf = NULL;
	char            scratchBuf[BLCKSZ];


	memset(&fileRepIdentifier, 0, sizeof(FileRepIdentifier_u));

	snprintf(fileRepIdentifier.fileRepFlatFileIdentifier.directorySimpleName,
			 MAXPGPATH+1,
			 "%s/%s",
			 relinfo.mirrorBaseDir,
			 relinfo.fromBasePath);

	strncpy(fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName,
			relinfo.filename,
			MAXPGPATH+1);

	nameLength = strlen(fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName);

	fileRepRelationType = FileRepRelationTypeFlatFile;

	snprintf(primaryFilename,
			 MAXPGPATH+1,
			 "%s/%s/%s",
			 relinfo.primaryBaseDir,
			 relinfo.fromBasePath,
			 relinfo.filename);

	snprintf(mirrorFilename,
			 MAXPGPATH+1,
			 "%s/%s/%s",
			 relinfo.mirrorBaseDir,
			 relinfo.fromBasePath,
			 relinfo.filename);


	/* if exceptions are none or AO then flat file */
	if (! relinfo.relFileNodeValid ||
		(relinfo.exception == FileRepVerifyCompareException_AORelation))
	{
		/* It is either Append Only relation or Flat File */
		isBufferPoolRelation = false;
	}
	else
	{
		Assert(relinfo.relFileNodeValid);
	}

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepPrimary_SendEnhancedBlockHashes primary %s mirror %s isBufferPool %d",
			  primaryFilename,
			  mirrorFilename,
			  isBufferPoolRelation
		);



	*matched = true;

#if FILEREP_VERIFY_LOCKING
	/* -------- MirroredLock(LW_SHARED) ---------- */
	MIRROREDLOCK_BUFMGR_LOCK;

	if (relinfo.relFileNodeValid)
	{
		LockRelationForResynchronize(&(relinfo.relFileNode), AccessExclusiveLock);
	}
#endif

	errno = 0;

	/* while loop used not for repetition but for break out to completion handling */
	while (1)
	{
		if (isBufferPoolRelation)
		{
			smgr_relation = smgropen(relinfo.relFileNode);
			if (smgr_relation == NULL)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not open file '%s' verification token '%s' : %m "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}
			else
			{
				smgr_relation->smgr_rnode.relNode = relinfo.relFileNode.relNode;
				smgr_relation->smgr_rnode.spcNode = relinfo.relFileNode.spcNode;
				smgr_relation->smgr_rnode.dbNode = relinfo.relFileNode.dbNode;
			}
			numBlocks = smgrnblocks(smgr_relation);
			dataLength = BLCKSZ * numBlocks;
		}
		else
		{
			/* AO/CO files can be very large - uint64 maybe needed to hold their size */
			uint64 sizeToCompare;

			if (stat(primaryFilename, &info) <	0)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not stat file '%s' verification token '%s' : %m "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}

			if (! S_ISREG(info.st_mode))
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"not a regular file '%s' verification token '%s' : %m "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}

			if (info.st_size > INT_MAX)
			{
				verifyLog(
					logControl,
					LOG,
					"FileRepPrimary_SendEnhancedBlockHashes warning: large file " INT64_FORMAT " bytes",
					info.st_size
					);
			}

/*
 * TODO
 * Memory should be allocated once and then same buffer re-used
 */
			buf = (char *)palloc(BLCKSZ + 1);
			if (buf == NULL)
			{
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not allocate memory for file '%s' to run verification token '%s' "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}

			fd = fopen(primaryFilename, "r");
			if (fd == NULL)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"could not open file '%s' verification token '%s' : %m "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}

			/* this is true for relfiles but there may be other types of files -
			 * certainly are in white box testing
			 * Assert(info.st_size % BLCKSZ == 0);
			 */
			if (relinfo.exception == FileRepVerifyCompareException_AORelation)
			{
				sizeToCompare = relinfo.AO_logicalEof;
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
				if (relinfo.AO_logicalEof!= info.st_size)
				{
					verifyLog(
							  logControl,
							  DEBUG1,
							  "FileRepPrimary_SendEnhancedBlockHashes: AO file comparing only to logical EOF, AO_logicalEof " INT64_FORMAT "  != filesize " INT64_FORMAT "",
							  relinfo.AO_logicalEof,
							  info.st_size);
				}
#endif
			}
			else
			{
				sizeToCompare = info.st_size;
			}

			dataLength = sizeToCompare;
			numBlocks = sizeToCompare/BLCKSZ;

			if (sizeToCompare % BLCKSZ !=0)
			{
				numBlocks++;
			}
		}

		fileOffset = 0;

#if DEBUG_PRINTS_WHILE_LOCKS_HELD
		verifyLog(
				  logControl, DEBUG1,
				  "FileRepPrimary_SendEnhancedBlockHashes: %s numBlocks %d size " INT64_FORMAT " ",
				  primaryFilename,
				  numBlocks,
				  numBlocks*BLCKSZ);
#endif

		if (numBlocks == 0)
		{

			/* file exists but is 0 size */
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
			verifyLog(
					  logControl, DEBUG1,
					  "FileRepPrimary_SendEnhancedBlockHashes: %s is of size 0",
					  primaryFilename);
#endif
			fileRepIdentifier.fileRepFlatFileIdentifier.blockId = 0;
			fileRepOperationDescription.verify.type = FileRepOpVerifyType_EnhancedBlockHash;
			fileRepOperationDescription.verify.logOptions = logControl.logOptions;
			fileRepOperationDescription.verify.desc.enhancedBlockHash.offset = 0;
			fileRepOperationDescription.verify.desc.enhancedBlockHash.dataLength = 0;
			fileRepOperationDescription.verify.desc.enhancedBlockHash.hashcode = 0;
			fileRepOperationDescription.verify.desc.enhancedBlockHash.exception = relinfo.exception;
			fileRepOperationDescription.verify.desc.enhancedBlockHash.lastBlock = true;

			status = FileRepPrimary_MirrorVerify(
												 fileRepIdentifier,
												 fileRepRelationType,
												 fileRepOperationDescription,
												 NULL,	/* data */
												 0,		/* data length */
												 &responseData,
												 &responseDataLength,
												 &responseDesc);

			if (status != STATUS_OK)
			{
				Assert(responseData == NULL);
				break;
			}

			if (! responseDesc.verify.desc.enhancedBlockHash.isOnMirror)
			{
				/*Note this is already reported on the mirror -
				 just leave reporting to the mirror?
				 */
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
				verifyLog(
						  logControl,
						  LOG,
						  "VERIFICATION_ERROR_REPORT PRIMARY: !!MISMATCH!!!: ENHANCED FILE BLOCK HASH FAILED BECAUSE FILE %s NOT ON MIRROR",
						  primaryFilename);
#endif
				writeFixFileRecord(
								   logControl,
								   "extra_p",
								   fileRepPrimaryHostAddress,
								   primaryFilename,
								   fileRepMirrorHostAddress,
								   mirrorFilename);

				writeDiffTableRecord(
									 logControl,
									 logControl.logOptions.token,
									 "FileRepFileMissingOnMirror",
									 fileRepPrimaryHostAddress,
									 primaryFilename,
									 fileRepMirrorHostAddress,
									 mirrorFilename,
									 -1, -1, -1, -1, -1,
									 &(relinfo.relFileNode));

				gFileRepVerifyShmem->request.isFailed = true;

				*matched = false;

				status = STATUS_OK;

				break;
			}

			/* TODO: this could really be done the responseDesc and not
			 * in the responseBody!!
			 */
			if (responseData != NULL)
			{
				bool *pBool = (bool *) responseData;
				bool contentsMatched = *pBool;

				if (! contentsMatched)
				{
					/* TODO: log this mismatch - do we want to send
					 *  back enough info to report how long the file
					 *  on mirror is?
					 *  TODO: In general make sure that if mirror file
					 *  is longer we catch that too!
					 *  Note this is already reported on the mirror -
					 *  just leave reporting to the mirror
					 */
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
					verifyLog(
							  logControl,
							  LOG,
							  "VERIFICATION_ERROR_REPORT PRIMARY: !!MISMATCH!!!: ENHANCED FILE BLOCK HASH DID NOT MATCH - FILE ON PRIMARY is 0 length but FILE ON MIRROR is not");
#endif
					writeFixFileRecord(
									   logControl,
									   getCompareExceptionString(relinfo),
									   fileRepPrimaryHostAddress,
									   primaryFilename,
									   fileRepMirrorHostAddress,
									   mirrorFilename);

					writeDiffTableRecord(
										 logControl,
										 logControl.logOptions.token,
										 "FileRepVerifyFileSizeMismatch",
										 fileRepPrimaryHostAddress,
										 primaryFilename,
										 fileRepMirrorHostAddress,
										 mirrorFilename,
										 -1, -1, -1, 0, -1,
										 &(relinfo.relFileNode));

					gFileRepVerifyShmem->request.isFailed = true;
					*matched = false;
					status = STATUS_OK;
				}
			}
			else
			{
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
				verifyLog(logControl,
						  DEBUG1,
						  "MIRROR RESPONSE response data is NUL");
#endif
				Assert(0);

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("verification failure, "
								"response from mirror is empty file '%s' verification token '%s' : %m "
								"verification aborted",
								primaryFilename,
								logControl.logOptions.token),
						 errhint("run gpverify")));

				break;
			}

		}
		else
		{
			bool heapZeroPageAtEnd = false;
			bool failure = false;

			/* BATCH_BLOCK_HASHES
			 * XXX especially when multiple blocks	 per file really need to
			 * batch them together!!!
			 * should we compute one hash per file - locking all the blocks
			 * and then unlock
			 * them all after we hear back from mirror
			 */
			for (blkno = 0; blkno < numBlocks; blkno++)
			{
				heapZeroPageAtEnd = false;
				fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName[nameLength] = '\0';

				dataToMirror = NULL;
				dataLengthLocal = Min(BLCKSZ, dataLength);

				if (isBufferPoolRelation)
				{
					/* availBufHdr == NULL
					 * isLocalBuf = ? setting to false but is that ok?
					 * isTemp = false (only used if isExtend anyway and
					 * we are not planning to pass in blockNum =P_NEW
					 */
					buffer = ReadBuffer_Ex_SMgr(smgr_relation,
												blkno,
												false,
												false);

#if FILEREP_VERIFY_LOCKING
					LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
#endif

					page = BufferGetPage(buffer);

					if (! PageHeaderIsValid((PageHeader)page))
					{
#if FILEREP_VERIFY_LOCKING
						UnlockReleaseBuffer(buffer);
#endif
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("verification failure, "
										"invalid buffer pool page file '%s' blkno '%d' "
										"verification token '%s' : %m "
										"verification aborted",
										primaryFilename,
										blkno,
										logControl.logOptions.token),
								 errhint("run gpverify")));

						failure = true;
						break;
					}
				}
				else
				{
					bytesRead = fread(buf, 1, dataLengthLocal, fd);
					if (dataLengthLocal != bytesRead)
					{
						ereport(WARNING,
								(errcode_for_file_access(),
								 errmsg("verification failure, "
										"could not read file '%s' offset '" INT64_FORMAT "' length requested '" INT64_FORMAT "' "
										"length read '" INT64_FORMAT "' verification token '%s' : %m "
										"verification aborted",
										primaryFilename,
										fileOffset,
										dataLengthLocal,
										bytesRead,
										logControl.logOptions.token),
								 errhint("run gpverify")));

						failure = true;
						break;
					}
					page = (Page) buf;
				}

				if (relinfo.exception == FileRepVerifyCompareException_HeapRelation)
				{
					bool validPage  = false;
					bool madeChange = false;

					/*
					 * if one of the last two heap blocks is all
					 * zeros this is a special case
					 * no need to compare with the data on the mirror
					 */
					if ((blkno == numBlocks - 1) || (blkno == numBlocks - 2))
					{
						if (isPageAllZeros(page))
						{
							heapZeroPageAtEnd = true;
						}
					}
					memcpy(scratchBuf, (char *) page, BLCKSZ);
					validPage = FileRepVerify_ProcessHeapPage(
						(Page) scratchBuf,
						&madeChange,
						logControl);

					if (madeChange){
						page = (Page) scratchBuf;
					}
					/* whichSegment ? blkno / RELSEG_SIZE whichPage?
					 * blkno % RELSEG_SIZE);
					 */

					if (! validPage && ! heapZeroPageAtEnd)
					{

#if DEBUG_PRINTS_WHILE_LOCKS_HELD
						verifyLog(
								  logControl,
								  LOG,
								  "VERIFICATION_ERROR_REPORT PRIMARY: corrupt heap file detected on primary file %s seg %d, page %d",
								  primaryFilename,
								  blkno/RELSEG_SIZE,
								  blkno%RELSEG_SIZE);
#endif
						writeFixFileRecord(
										   logControl,
										   "heap",
										   fileRepMirrorHostAddress,
										   mirrorFilename,
										   fileRepPrimaryHostAddress,
										   primaryFilename);

						writeDiffTableRecord(
											 logControl,
											 logControl.logOptions.token,
											 "FileRepVerifyCorruptHeapFile",
											 fileRepPrimaryHostAddress,
											 primaryFilename,
											 fileRepMirrorHostAddress,
											 mirrorFilename,
											 -1, -1, -1, -1, -1,
											 &(relinfo.relFileNode));

						gFileRepVerifyShmem->request.isFailed = true;
					}

				}
				else if (relinfo.exception == FileRepVerifyCompareException_BtreeIndex)
				{
					bool validPage = false;
					bool madeChange = false;

					memcpy(scratchBuf, (char *) page, BLCKSZ);

					validPage = FileRepVerify_ProcessBtreeIndexPage(
						(Page) scratchBuf,
						&madeChange,
						logControl);

					if (madeChange){
						page = (Page) scratchBuf;
					}


					if (! validPage)
					{
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
						verifyLog(
								  logControl,
								  LOG,
								  "VERIFICATION_ERROR_REPORT PRIMARY: corrupt btree index file detected on primary file %s seg %d, page %d",
								  primaryFilename,
								  blkno / RELSEG_SIZE,
								  blkno % RELSEG_SIZE);
#endif
						writeFixFileRecord(
										   logControl,
										   "btree",
										   fileRepMirrorHostAddress,
										   mirrorFilename,
										   fileRepPrimaryHostAddress,
										   primaryFilename);

						writeDiffTableRecord(
											 logControl,
											 logControl.logOptions.token,
											 "FileRepVerifyCorruptBTreeIndexFile",
											 fileRepPrimaryHostAddress,
											 primaryFilename,
											 fileRepMirrorHostAddress,
											 mirrorFilename,
											 -1, -1, -1, -1, -1,
											 &(relinfo.relFileNode));

						gFileRepVerifyShmem->request.isFailed = true;
					}
				}

				if (heapZeroPageAtEnd)
				{
#if FILEREP_VERIFY_LOCKING
					if (isBufferPoolRelation)
					{
						UnlockReleaseBuffer(buffer);
					}
#endif
					dataLength -= dataLengthLocal;
					fileOffset += dataLengthLocal;
					continue;
				}
				dataToMirror = (char * ) page;


				//NEW_HASH
				COMP_CRC32C(hashcode, dataToMirror, dataLengthLocal);
				FIN_CRC32C(hashcode);


				/*			   relinfo.primaryIdentifier.fileRepFlatFileIdentifier.blockId =
				 *			   fileOffset;
				 */
				if ((relinfo.exception == FileRepVerifyCompareException_HeapRelation) &&
					(fileOffset >= RELSEG_SIZE * BLCKSZ))
				{
					int64 perSeg = RELSEG_SIZE * BLCKSZ;
					uint32 seg = fileOffset / perSeg;
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
					verifyLog(
							  logControl,
							  DEBUG1,
							  "SendEnhancedBlockHashes: Heap file of multiple segments, seg %u perSeg " INT64_FORMAT " fileOffset " INT64_FORMAT "",
							  seg,
							  perSeg,
							  fileOffset);
#endif
					mirrorFileOffset = fileOffset % (RELSEG_SIZE * BLCKSZ);
					snprintf(
							 &(fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName[nameLength]),
							 MAXPGPATH + 1 - nameLength,
							 ".%d",
							 seg);
				}
				else
				{
					mirrorFileOffset = fileOffset;
				}

#if DEBUG_PRINTS_WHILE_LOCKS_HELD
				verifyLog(
						  logControl,
						  DEBUG1,
						  "SendEnhancedBlockHashes Filename %s mirrorFileOffset %d",
						  fileRepIdentifier.fileRepFlatFileIdentifier.fileSimpleName,
						  mirrorFileOffset);
#endif

				/* ??? blockId = blkno? */
				fileRepIdentifier.fileRepFlatFileIdentifier.blockId = blkno;
				fileRepOperationDescription.verify.type = FileRepOpVerifyType_EnhancedBlockHash;
				fileRepOperationDescription.verify.logOptions = logControl.logOptions;
				fileRepOperationDescription.verify.desc.enhancedBlockHash.offset = mirrorFileOffset;
				fileRepOperationDescription.verify.desc.enhancedBlockHash.dataLength = dataLengthLocal;
				fileRepOperationDescription.verify.desc.enhancedBlockHash.hashcode = hashcode;
				fileRepOperationDescription.verify.desc.enhancedBlockHash.exception = relinfo.exception;
				if (blkno == numBlocks-1){
					fileRepOperationDescription.verify.desc.enhancedBlockHash.lastBlock = true;
				} else {
					fileRepOperationDescription.verify.desc.enhancedBlockHash.lastBlock = false;
				}

				/* could do the mapping of filename and offset here if we wanted */
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
				verifyLog(
						  logControl,
						  DEBUG1,
						  "FileRepPrimary_SendEnhancedBlockHashes before SendToMirror fileOffset " INT64_FORMAT " blk %d of %d; seg %d of %d: %s",
						  fileOffset,
						  blkno,
						  numBlocks,
						  blkno / RELSEG_SIZE,
						  (numBlocks - 1) / RELSEG_SIZE,
						  primaryFilename);
#endif

				status = FileRepPrimary_MirrorVerify(
													 fileRepIdentifier,
													 fileRepRelationType,
													 fileRepOperationDescription,
													 NULL,	/* data */
													 0,		/* data length */
													 &responseData,
													 &responseDataLength,
													 &responseDesc);

#if FILEREP_VERIFY_LOCKING
				if (isBufferPoolRelation)
				{
					UnlockReleaseBuffer(buffer);
				}
#endif

				if (status != STATUS_OK)
				{
					Assert(responseData == NULL);
					failure = true;
					break;
				}

				if (!responseDesc.verify.desc.enhancedBlockHash.isOnMirror)
				{

#if DEBUG_PRINTS_WHILE_LOCKS_HELD
					verifyLog(
							  logControl,
							  LOG,
							  "VERIFICATION_ERROR_REPORT PRIMARY: !!MISMATCH!!!: "
							  "ENHANCED FILE BLOCK HASH FAILED BECAUSE CORRESPONDING FILE for %s NOT ON MIRROR",
							  primaryFilename);
#endif
					writeFixFileRecord(
									   logControl,
									   "extra_p",
									   fileRepPrimaryHostAddress,
									   primaryFilename,
									   fileRepMirrorHostAddress,
									   mirrorFilename);

					writeDiffTableRecord(
										 logControl,
										 logControl.logOptions.token,
										 "FileRepVerifyFileMissingOnMirror",
										 fileRepPrimaryHostAddress,
										 primaryFilename,
										 fileRepMirrorHostAddress,
										 mirrorFilename,
										 -1, -1, -1, -1, -1,
										 &(relinfo.relFileNode));

					gFileRepVerifyShmem->request.isFailed = true;
					*matched = false;

					status = STATUS_OK;

					break;
				}

				/* TODO: this could really be done the responseDesc and
				 * not in the responseBody!!
				 */
				if (responseData != NULL)
				{
					bool *pBool = (bool *)responseData;
					bool contentsMatched = *pBool;

					if (! contentsMatched)
					{
						/* log this mismatch
						 * Note this is already reported on the mirror -
						 * just leave reporting to the mirror
						 */
#if DEBUG_PRINTS_WHILE_LOCKS_HELD
						verifyLog(
								  logControl,
								  LOG,
								  "VERIFICATION_ERROR_REPORT PRIMARY: !!MISMATCH!!!: ENHANCED FILE BLOCK HASH DID NOT MATCH FOR BLOCK OFFSET %d (STOPPING WITH FIRST MISMATCH)",
								  fileOffset);
#endif
						writeFixFileRecord(
										   logControl,
										   getCompareExceptionString(relinfo),
										   fileRepPrimaryHostAddress,
										   primaryFilename,
										   fileRepMirrorHostAddress,
										   mirrorFilename);

						writeDiffTableRecord(
											 logControl,
											 logControl.logOptions.token,
											 "FileRepBlockCompareFailed",
											 fileRepPrimaryHostAddress,
											 primaryFilename,
											 fileRepMirrorHostAddress,
											 mirrorFilename,
											 -1, -1, fileOffset, -1, -1,
											 &(relinfo.relFileNode));

						gFileRepVerifyShmem->request.isFailed = true;
						*matched = false;
						status = STATUS_OK;
						break;
					}

					if (responseData != NULL)
					{
						pfree(responseData);
						responseData = NULL;
					}
				}
				else
				{
#if DEBUG_PRINTS_WHILE_LOCKS_HELD

					verifyLog(logControl,
							  DEBUG3,
							  "MIRROR RESPONSE response data is NUL");
#endif
					Assert(0);

					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("verification failure, "
									"response from mirror is empty file '%s' verification token '%s' : %m "
									"verification aborted",
									primaryFilename,
									logControl.logOptions.token),
							 errhint("run gpverify")));
					failure = true;
					break;
				}

				dataLength -= dataLengthLocal;
				fileOffset += dataLengthLocal;

			} /* for (blkno = 0; blkno < numBlocks; blkno++) */

			if (failure)
			{
				//get out of the while loop
				break;
			}
		}  /* if (numBlocks == 0) else { } */

		status = STATUS_OK;

		break;

	}  // while (1)

	if (responseData != NULL)
	{
		pfree(responseData);
		responseData = NULL;
	}

	if (isBufferPoolRelation)
	{
		if (smgr_relation != NULL)
		{
			smgrclose(smgr_relation);
		}
	}
	else
	{
		if (buf != NULL)
		{
			pfree(buf);
		}
		if (fd != NULL)
		{
			fclose(fd);
		}
	}

#if FILEREP_VERIFY_LOCKING
	if (relinfo.relFileNodeValid)
	{
		UnlockRelationForResynchronize(&(relinfo.relFileNode), AccessExclusiveLock);
	}

	/* -------- MirroredLockRelease (LW_SHARED) ---------- */
	MIRROREDLOCK_BUFMGR_UNLOCK;
#endif

	return status;
}


/*
 *   TODO- should this be FileRepMessageHeader_s not
 *  FileRepMessageHeader_s *
 */
int
FileRepMirror_ExecuteVerificationRequest(
										 FileRepMessageHeader_s	*fileRepMessageHeader,
										 char					*fileRepMessageBody,
										 void					**response,
										 uint32					*responseSizeBytes)
{
	int							status = STATUS_OK;
	FileName					fileName = NULL;
	FileRepVerifyLogControl_s	logControl;

	FileRep_InsertConfigLogEntry("run verification");

	/* we could open separate logging file on mirror as well if we wanted
	 * that might mean passing it into this function as a parameter?
	*/
	logControl.logOptions = fileRepMessageHeader->fileRepOperationDescription.verify.logOptions;

	if (fileRepMessageHeader->fileRepOperationDescription.verify.type != FileRepOpVerifyType_RequestParameters)
	{
		status = FileRep_OpenOutputFiles(&logControl, false);
		if (status != STATUS_OK)
		{
			return status;
		}
	}
	else
	{
		logControl.filesAreOpen = false;
	}

	verifyLog(
		logControl, DEBUG1,
		"FileRepMirror_ExecuteVerificationRequest() ");

	*responseSizeBytes = 0;
	*response = NULL;

	/* fileName is relative path to $PGDATA directory */
	fileName = FileRep_GetFileName(
								   fileRepMessageHeader->fileRepIdentifier,
								   fileRepMessageHeader->fileRepRelationType);

	switch (fileRepMessageHeader->fileRepOperationDescription.verify.type)
	{
		case FileRepOpVerifyType_SummaryItems:

			verifyLog(logControl,
					  DEBUG3,
					  "FileRepMirror_ExecuteVerificationRequest type FileRepOpVerifyType_SummaryItems fileRepMessageBody %p",
					  fileRepMessageBody);

			status = FileRepMirror_CompareAllSummaryItems(
														  fileRepMessageHeader,
														  fileRepMessageBody,
														  response,
														  responseSizeBytes,
														  logControl);
			if (status == STATUS_OK )
			{
				verifyLog(
					logControl,
					DEBUG3,
					"FileRepMirror_CompareAllSummaryItems returned successfully: responseSizeBytes %u numMismatches %u",
					*responseSizeBytes,
					fileRepMessageHeader->fileRepOperationDescription.verify.desc.summaryItems.num_entries);
			}
			break;

		case FileRepOpVerifyType_DirectoryContents:

			verifyLog(
				logControl,
				DEBUG3,
				"FileRepMirror_ExecuteVerificationRequest type FileRepOpVerifyType_DirectoryContents fileRepMessageBody %p",
				fileRepMessageBody);

			status = FileRepMirror_CompareDirectoryContents(
															fileRepMessageHeader,
															fileRepMessageBody,
															response,
															responseSizeBytes,
															logControl);
			if (status == STATUS_OK )
			{
				verifyLog(
					logControl,
					DEBUG3,
					"FileRepMirror_CompareDirectoryContents returned successfully: responseSizeBytes %u numChildrenInBatch %u finalBatch %d startOver %d",
					*responseSizeBytes,
					fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.numChildrenInBatch,
					fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.finalBatch,
					fileRepMessageHeader->fileRepOperationDescription.verify.desc.dirContentsMirror.startOver
					);
			
			}
			break;

		case	FileRepOpVerifyType_EnhancedBlockHash:

			status =  FileRepMirror_CompareEnhancedBlockHash(
															 fileRepMessageHeader,
															 fileRepMessageBody,
															 response,
															 responseSizeBytes,
															 logControl);
			break;

		case	FileRepOpVerifyType_RequestParameters:

			status =  FileRepMirror_ExchangeRequestParameters(
															fileRepMessageHeader,
															fileRepMessageBody,
															response,
															responseSizeBytes,
															logControl);
			break;

		default:
			/* VERIFICATION_ERROR_INTERNAL */
			verifyLog(
					  logControl,
					  WARNING,
					  "VERIFICATION_ERROR_INTERNAL, FileRepMirror_ExecuteVerificationRequest:	FileRepOperationVerificationType_e(%d) not recognized: %s",
					  fileRepMessageHeader->fileRepOperationDescription.verify.type,
					  FileRepOperationVerificationTypeToString[ fileRepMessageHeader->fileRepOperationDescription.verify.type] );
			Assert(0);
			break;
	}

	if (status != STATUS_OK )
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not compare verification type '%s' : %m "
						"verification aborted",
						FileRepOperationVerificationTypeToString[fileRepMessageHeader->fileRepOperationDescription.verify.type]),
				 errhint("run gpverify"),
				 FileRep_errdetail(
								   fileRepMessageHeader->fileRepIdentifier,
								   fileRepMessageHeader->fileRepRelationType,
								   fileRepMessageHeader->fileRepOperation,
								   fileRepMessageHeader->messageCount),
				 FileRep_errcontext()));
	}

	pfree(fileName);
	Assert(fileRepMessageHeader->fileRepOperation == FileRepOperationVerify);

	FileRep_CloseOutputFiles(&logControl, false);

	return status;
}

int
FileRepVerify_NumHeapSegments( RelFileNode relFileNode)
{

	SMgrRelation smgr_relation = NULL;
	BlockNumber numBlocks = 0;
	int numSegs = 0;

	smgr_relation = smgropen(relFileNode);
	if (smgr_relation == NULL)
	{
		return -1;

	}
	else
	{
		/* this is done differently in cdbfilerepresyncworker.c -
		 * is this ok/better?
		 */
		smgr_relation->smgr_rnode.relNode = relFileNode.relNode;
		smgr_relation->smgr_rnode.spcNode = relFileNode.spcNode;
		smgr_relation->smgr_rnode.dbNode = relFileNode.dbNode;

	}

	numBlocks = smgrnblocks(smgr_relation);

	numSegs = numBlocks / RELSEG_SIZE;
	if (numBlocks % RELSEG_SIZE != 0)
	{
		numSegs++;
	}
	if (numSegs == 0)
	{
		/* base segment is empty but there is still a file there */
		numSegs = 1;
	}

	smgrclose(smgr_relation);
	return numSegs;
}


int FileRepPrimary_ComputeSummaryItemsFromRelFileNodeInfoTable(
															   FileRepRelFileNodeInfo_s *relfilenodeTable,
															   int numRows,
															   FileRepVerifySummaryItem_s *summaryItemArray,
															   bool doHash,
															   bool includeFileSizeInfo,
															   FileRepVerifyLogControl_s logControl)
{
	int status = STATUS_OK;
	int row = 0;
	struct stat info;
	int numSegments = 0;
	int seg = 0;
	char segmentFileName[MAXPGPATH+2];

	for (row = 0; row < numRows; row++)
	{

		/* are all things in the rel file nodes list files */
		summaryItemArray[row].isFile = true;

		/* this is what will get sent to mirror */
		strncpy(summaryItemArray[row].baseDir,
				relfilenodeTable[row].mirrorBaseDir,
				MAXPGPATH+1);
		strncpy(summaryItemArray[row].fromBasePath,
				relfilenodeTable[row].fromBasePath,
				MAXPGPATH+1);
		strncpy(summaryItemArray[row].filename,
				relfilenodeTable[row].filename,
				MAXPGPATH+1);

		summaryItemArray[row].summary.fileSummary.exception =
		relfilenodeTable[row].exception;
		summaryItemArray[row].summary.fileSummary.AO_logicalEof =
		relfilenodeTable[row].AO_logicalEof;

		if ( summaryItemArray[row].summary.fileSummary.exception ==
			FileRepVerifyCompareException_HeapRelation)
		{

			summaryItemArray[row].summary.fileSummary.numHeapSegments =
				FileRepVerify_NumHeapSegments(relfilenodeTable[row].relFileNode);

			verifyLog(
					  logControl, DEBUG1, "File %s numHeapSegments %d\n",
					  relpath(relfilenodeTable[row].relFileNode),
					  summaryItemArray[row].summary.fileSummary.numHeapSegments);

			if (summaryItemArray[row].summary.fileSummary.numHeapSegments == -1)
			{
				/* VERIFICATION_ERROR_INTERNAL */
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not compute summary for relation file '%s' "
								"verification aborted",
								relpath(relfilenodeTable[row].relFileNode)),
						 errhint("run gpverify")));

				return STATUS_ERROR;
			}
			numSegments = summaryItemArray[row].summary.fileSummary.numHeapSegments;
		}
		else
		{
			numSegments = 1;
		}

		for (seg=0; seg < numSegments; seg++)
		{
			if (seg ==0)
			{

				if (relfilenodeTable[row].segmentFileNum == 0)
				{
					snprintf(
						segmentFileName,
						MAXPGPATH+1,
						"%s",
						relpath(relfilenodeTable[row].relFileNode));
				}
				else
				{
					snprintf(	
						segmentFileName,
						MAXPGPATH+1,
						"%s.%d",
						relpath(relfilenodeTable[row].relFileNode),
						relfilenodeTable[row].segmentFileNum);
				}

					/*	strncpy(
						segmentFileName,
						relpath(relfilenodeTable[row].relFileNode),
						MAXPGPATH+2);
					*/

				summaryItemArray[row].summary.fileSummary.fileSize = 0;

				/*
				 * if (doHash)
				 * {
				 * //if do this mirror is always 1 bigger than primary
				 * for segment 1
				 * INIT_LEGACY_CRC32(
				 * summaryItemArray[row].summary.fileSummary.fileContentsHashcode);
				 * }
				 * else
				 * {
				 * summaryItemArray[row].summary.fileSummary.fileContentsHashcode = 0;
				 *}
				 */
				summaryItemArray[row].summary.fileSummary.fileContentsHashcode = 0;
			}
			else
			{
				//Assert relfilenodeTable[row].segmentFileNum == 0
				snprintf(segmentFileName, MAXPGPATH+2,
						 "%s.%d",
						 relpath(relfilenodeTable[row].relFileNode),
						 seg);
			}

			verifyLog(logControl, DEBUG1, "Segfile is %s\n",
					  segmentFileName);

			errno = 0;
			/* careful to compute real file name based on segmentFileNum */
			if (stat(segmentFileName, &info) != 0)
			{
				ereport(WARNING,
						(errcode_for_file_access(),
						errmsg("verification failure, "
								"could not stat file '%s' : %m "
								"verification aborted",
								relpath(relfilenodeTable[row].relFileNode)),
						errhint("run gpverify")));

				/* VERIFICATION_ERROR_INTERNAL */
				return STATUS_ERROR;
			}

			if (includeFileSizeInfo && (seg == numSegments-1))
			{
				/*
				 * when we have multiple segments fileSize will be the
				 * size of the last one
				 *  it would be nice to just make fileSize the sum of
				 *  all the sizes
				 *  but that could overflow off_t
				 */
				summaryItemArray[row].summary.fileSummary.fileSize =
				info.st_size;

			}
			verifyLog(logControl, DEBUG1, "File %s is size %d \n",
					  segmentFileName,
					  info.st_size);
			FileRepVerify_AddToTotalBlocksVerified(info.st_size);

			if (doHash)
			{
				pg_crc32 thisSegmentHashcode = 0;

				FileRepVerify_ComputeFileHash(
											  segmentFileName,
											  &thisSegmentHashcode,
											  summaryItemArray[row].summary.fileSummary.exception,
											  summaryItemArray[row].summary.fileSummary.AO_logicalEof,
											  seg,
											  logControl);

				if (seg ==0)
				{
					summaryItemArray[row].summary.fileSummary.fileContentsHashcode
					= thisSegmentHashcode;
				}
				else
				{
					summaryItemArray[row].summary.fileSummary.fileContentsHashcode
					+= thisSegmentHashcode;
				}
			}

		} /* segs */
	} /* rows */

	return status;
}

int
isExcludedFile(
			   FileName fileName,
			   int numExcluded,
			   char ** excludeList)
{
	int i = 0;
	/*
	 * gpcheckmirrorseg
	 *
	 * file/directory names
	 * MPP-8973: ignore changetracking files
	 * MPP-9461: ignore filespace gp_dbid files
	 * pg_stat			 HANDLED EXCLUDED FILE pg_stat.stat,
	 * EXCLUDED DIRECTORY pg_stat_tmp
	 * pg_log			 HANDLED EXCLUDED DIRECTORY pg_log
	 * pg_xlog			 HANDLED EXCLUDED DIRECTORY pg_xlog - Jeff does
	 * not exclude this - go ahead and exclude
	 * pg_internal		 HANDLED EXCLUDED FILE pg_internal.init
	 * pg_fsm.cache		 HANDLED EXCLUDED FILE pg_fsm.cache
	 * pgstat			 HANDLED EXCLUDED FILE pgstat.stat
	 * pgsql_tmp			 HANDLED EXCLUDED DIRECTORY pgsql_tmp
	 * pg_changetracking	 HANDLED EXCLUDED DIRECTORY pg_changetracking
	 * gp_dbid			 HANDLED EXCLUDED FILE gp_dbid
	 * core				 HANDLED EXCLUDED FILE core
	 *
	 * file suffixes -- append a close paren
	 * MPP-8887: postgresql.conf.bak generated by recovery
	 * Note: Jeff excludes anything with these suffixes and I am doing
	 * specific match of a file
	 *  .log				 HANDLED EXCLUDED FILE postmaster.log
	 * .opts				 HANDLED EXCLUDED FILE postmaster.opts
	 * .conf				 HANDLED EXCLUDED FILE	pg_hba.conf, pg_ident.conf,
	 * postgresql.conf
	 * .bak				 HANDLED EXCLUDED FILE postgresql.conf.bak
	 * _args				 HANDLED EXCLUDED FILE gp_pmtransition_args
	 * .pid				 HANDLED EXCLUDED FILE postmaster.pid
	 * core				 HANDLED EXCLUDED FILE core
	 * _segargs			 ???
	 *
	 *
	 * wet_execute.tbl	  EXCLUDED FILE wet_execute.tbl ( I don't believe
	 * Jeff excludes this)
	 *
	 * gp_dump* in filerep0	EXCLUDED FILE gp_dump*
	 *
	 * PG_VERSION - Jeff includes and I will too
	 *
	 * pg_control ( make sure does't get changed during
	 * verification?)
	 * global/pg_control - should we do something special with this?
	 *
	 * Other things Jeff excludes that I don't?????
	 *
	 *
	 * INCLUDED DIRECTORY pg_tblspc - Jeff includes and I will too
	 * INCLUDED DIRECTORY pg_utilitymodedtmred - Jeff includes and I will too
	 * INCLUDED DIRECTORY pg_distributedxidmap - Jeff includes and I will too
	 *
	 *
	 * recovery.done - should be ignored
	 *
	 */


	/*prefix match	if exclude list contains foo it will match any files foo*/
	for (i=0; i< numExcluded; i++)
	{
		if (strncmp(fileName, excludeList[i], strlen(excludeList[i])) == 0)
		{
			return 1;
		}
	}

	/*
	 *TODO: Remove all these hardcodes and make sure list in gpverify is
	 * correct
	 */
	if (strncmp(fileName, "pg", strlen("pg")) == 0)
	{
		if (strcmp(fileName, "pg_internal.init") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "pgstat.stat") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "pg_hba.conf") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "pg_ident.conf") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "pg_fsm.cache") == 0)
		{
			return 1;
		}
	}
	if (strncmp(fileName, "gp", strlen("gp")) == 0)
	{
		if (strcmp(fileName, "gp_dbid") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "gp_pmtransition_args") == 0)
		{
			return 1;
		}
		else if (strncmp(fileName, "gp_dump", strlen("gp_dump")) == 0)
		{
			/* igore files starting with gp_dump* in filerep0
			 * Note: others are exact file match - this is gp_dump*
			 */
			return 1;
		}
	}
	if (strncmp(fileName, "post", strlen("post")) == 0)
	{
		if (strcmp(fileName, "postgresql.conf") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "postmaster.log") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "postmaster.opts") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "postmaster.pid") == 0)
		{
			return 1;
		}
		else if (strcmp(fileName, "postgresql.conf.bak") == 0)
		{
			/* no real need to check this - covered by check
			 * for postgresql.conf
			 */
			return 1;
		}
	}
	if (strcmp(fileName, "core") == 0)
	{
		return 1;
	}

	if (strcmp(fileName, "wet_execute.tbl") == 0)
	{
		return 1;
	}

	if (strcmp(fileName, "recovery.done") == 0)
	{
		return 1;
	}
/*
 *	//Jeff includes - so I will too
 *	if (strcmp(fileName, "PG_VERSION") == 0)
 *	{
 *		return 1;
 *	}
*/

	return 0;

}



int
isExcludedDirectory(
	FileName directoryName,
	int numExcluded,
	char ** excludeList)
{

	int i = 0;

	/* exact match -	 if exclude list contains foo
	 * it will match any directory named exactly foo
	 */
	for (i=0; i< numExcluded; i++)
	{
		if ((strncmp(directoryName,
					 excludeList[i],
					 strlen(excludeList[i])) == 0) &&
			(strlen(directoryName) == strlen(excludeList[i])))
		{
			return 1;
		}
	}
	/*
	 *TODO: Remove all these hardcodes and make sure list in gpverify is
	 * correct
	 */
	if (strcmp(directoryName, "pgsql_tmp") == 0)
	{
		return 1;
	}
	else if (strcmp(directoryName, "pg_xlog") == 0)
	{
		return 1;
	}
	else if (strcmp(directoryName, "pg_log") == 0)
	{
		return 1;
	}
	else if (strcmp(directoryName, "pg_stat_tmp") == 0)
	{
		return 1;
	}
	else if (strcmp(directoryName, "pg_changetracking") == 0)
	{
		return 1;
#if 0
		/*	Things I exclude that Jeff doesn't
		 * 	Jeff includes and I will too
		 *	pg_tblspc
		 *	pg_utilitymodedtmredo
		 *	pg_distributedxidmap
		*/

		/* Jeff includes pg_tblspc */
	}
	else if (strcmp(directoryName, "pg_tblspc") == 0)
	{
		return 1;
		/* Jeff includes pg_utilitymodedtmredo */
	}
	else if (strcmp(directoryName, "pg_utilitymodedtmredo") == 0)
	{
		return 1;
		/* Jeff includes pg_distributedxidmap */
	}
	else if (strcmp(directoryName, "pg_distributedxidmap") == 0)
	{
		return 1;

#endif
	}
	else if (strcmp(directoryName, "pg_verify") == 0)
	{
		return 1;

	}
	else if (strcmp(directoryName, ".") == 0)
	{
		return 1;
	}
	else if (strcmp(directoryName, "..") == 0)
	{
		return 1;
	}
	return 0;

}

bool
looksLikeRelFileName(
	char * dirName,
	char * file)
{
	int i;
	char * directory = NULL;

	/* move past . or / 's
	 * Sometimes we get .//base rather than base - not sure why?
	 */
	directory = dirName;
	while((*directory == '.') || (*directory == '/'))
	{
		directory++;
	}

	/* Used to detrmine if this looks a relfile - if so then don't
	 * reverify during directory walk
	 */
	if (strncmp("base/", directory,5) ==0)
	{
		bool foundDot = false;

		/*  base/NUM/NUM
		 *  base/NUM/NUM.NUM
		 */
		/*
		 * check the NUM subdirectory
		 * past base/ starts at 5
		 */
		for (i=5; i< strlen(directory); i++)
		{

			if ((directory[i] < '0') || (directory[i]>'9'))
			{
				return false;
			}
		}

		/* now check the file
		 * should be at least one number
		 */
		if ((file[0] < '0') || (file[0]>'9'))
		{
			return false;
		}
		/* then go until find a . or the end of the string */
		for (i=1; i< strlen(file); i++)
		{
			if (file[i] == '.')
			{
				if (foundDot)
				{
					/* are two dots allows? NUM.NUM.NUM? */
					return false;
				}
				else
				{
					foundDot = true;
				}
			}
			else if ((file[i] < '0') || (file[i]>'9'))
			{
				return false;
			}
		}

		return true;
	}
	else if	(strncmp("global", directory,6) ==0 &&
				 strlen(directory) == 6)
	{
		/* global/NUM
		 * global/NUM.NUM
		 */
		for (i=0; i< strlen(file);i++)
		{
			if ((file[i] < '0') || (file[i]>'9'))
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		bool foundDot = false;
		bool foundDivider = false;

		/* TABLESPACE/DATABASE/FILE
		 *  NUM/NUM/NUM
		 *  NUM/NUM/NUM.NUM
		 * check the NUM subdirectory
		 */
		for (i=0; i< strlen(directory); i++)
		{

			if(directory[i] == '/')
			{
				if (foundDivider == true)
				{
					return false;
				}
				foundDivider = true;
			}
			else if ((directory[i] < '0') || (directory[i]>'9'))
			{
				return false;
			}
		}

		/* now check the file
		 * should be at least one number
		 */
		if ((file[0] < '0') || (file[0]>'9'))
		{
			return false;
		}
		/* then go until find a . or the end of the string */
		for (i=1; i< strlen(file); i++)
		{
			if (file[i] == '.')
			{
				if (foundDot)
				{
					/* are two dots allows? NUM.NUM.NUM? */
					return false;
				}
				else
				{
					foundDot = true;
				}
			}
			else if ((file[i] < '0') || (file[i]>'9'))
			{
				return false;
			}

		}

		return true;

	}


	/* others too? what about overflow files, indices etc  */
	return false;
}

bool
wasHandledAlready(
	char * thisDirectory,
	char * thisChild,
	char * nextDirectory,
	char * nextChild)
{

	/*	elog(
	 	LOG,
	 	"wasHandledAlready thisDir %s thisChild %s nextDir %s nextChild %s",
		thisDirectory,
		thisChild,
		nextDirectory,
		nextChild);
	*/
	/* compare this directory and thisChild to the nextDiretory and nextChild
	 *if earlier in the predefined order then don't process again
	 */

	if ((nextDirectory[0] == '\0') && (nextChild[0] == '\0'))
	{
		/* NOTHING PROCESSED YET */
		return false;

	}

#if 0
	/* TOOD this works given ordering - better to say
	 * if thisDirectory >= nextDirectory && nextChild is null
	 */
	if (nextChild[0] == '\0')
	{
		/* thisDirectory base/2	nextDirectory base/1
		 * everything in this directory has been processed -
		 * just need to tally up the stats for the directory itself
		 */
		return true;
	}
#endif

	/* SAME DIRECTORY */
	if ((strlen(thisDirectory) == strlen(nextDirectory)) &&
		(strcmp(thisDirectory, nextDirectory) == 0))
	{
		/* 		elog(LOG, "Directories are the same"); */
		/* directories are the same - just compare the child names */
		if (nextChild[0] == '\0')
		{
			if (thisChild[0] == '\0')
			{
				/* all we have left to do is the directory itself
				 * and this is it
				 */
				return false;
			}
			else
			{
				/* all children have been done */
				return true;
			}
		}
		else if (thisChild[0] == '\0')
		{
			/* if thisChild is 0 and nextChild is not -
			 * then we have not handled this yet because thisChild 0
			 * is the last thing to handle in a directory
			 */
			return false;
		}

		/* NEITHER thisChild or nextChild is null so just compare them */
		if (strcmp(thisChild,nextChild) <0)
		{
			/* this child is < next child then its been processed
			 * already
			 */
			return true;
		}
		else
		{
			/* if child is the same too then this is the nextChild
			 * to be processed and
			 * so it was not handled already
			 */
			return false;
		}
	}


	/* DIFFERENT DIRECTORIES
	 * ONE DIRECTORY MAY BE A CHILD OF ANOTHER
	 */
	if ( (strlen(thisDirectory) > strlen(nextDirectory)) &&
		 (strncmp(thisDirectory, nextDirectory, strlen(nextDirectory)) ==0))
	{

		/* THIS IS LONGER but STARTS WITH NEXT -
		 * THIS COULD BE A CHILD OF NEXT
		 * CAREFUL could be this /base/120 and next is /base/1 -
		 * not a child of each other - should get out of here and
		 * do compares at the end
		 */
		int nextDirLen = strlen(nextDirectory);

		if (thisDirectory[nextDirLen] == '/')
		{

			/* thisDirectory is a child of nextDirectory -
			 * but also have to look at the children!
			 */
			 /* elog(LOG,
			  " thisDirectory %s is child of nextDirectory %s",
			  thisDirectory,  nextDirectory); */
            /* compare rest of /this to next's child
			 */
			if (nextChild[0] == '\0')
			{
				/* e.g. this /base/2 100 and next is /base
				 * e.g. this /base/2	 and next is /base
				 * just have the parent left to do -
				 * all the children were handled already
				 */
				return true;

			}
			else if (strncmp(
						 thisDirectory+nextDirLen+1,
						 nextChild,
						 strlen(nextChild)) <0)
			{
				/* e.g. this /base/2 100 and next is /base 3 */
				/* elog(LOG,
				   "wasHandledAlready-2- Skipping thisDirectory %s %s nextDirectory %s %s",
				   thisDirectory, thisChild, nextDirectory, nextChild);
				 */
				return true;
			}
			else
			{
				/* e.g. this /base/2 100 and next is /base 1 */
				/*
				 elog(LOG,
				  "wasHandledAlready-3- Not skipping thisDirectory %s %s nextDirectory %s %s",
				  thisDirectory, thisChild, nextDirectory, nextChild);
				*/
				return false;
			}
		}

	}
	else if ( (strlen(thisDirectory) < strlen(nextDirectory))
				 &&
				 (strncmp(thisDirectory,
						  nextDirectory,
						  strlen(thisDirectory)) ==0))
	{

		/* NEXT IS LONGER but STARTS WITH THIS -
		 * NEXT COULD BE A CHILD OF THIS
		 * CAREFUL could be this /base/1 and next is /base/120 -
		 * not a child of each other - that should get out of
		 * here and do compares at the end
		 */
		int thisDirLen = strlen(thisDirectory);

		if (nextDirectory[thisDirLen] == '/')
		{
			/*
			  elog(LOG, "next %s is child of this %s",
			  nextDirectory,  thisDirectory);
			 */

			/* nextDirectory is a child of thisDirectory -
			 * but also have to look at the children!
			 * compare rest of /next to this's child
			 */
			if (thisChild[0] == '\0')
			{
				/* next is /base/2 100 and this is /base	 NOT DONE */
				/* elog(LOG,
				   "wasHandledAlready-4-Not skipping thisDirectory %s %s nextDirectory %s %s",
				   thisDirectory,
				   thisChild,
				   nextDirectory,
				   nextChild);
				*/
				return false;
			}
			else if (strncmp(nextDirectory+thisDirLen+1,
							 thisChild, strlen(thisChild)) <= 0)
			{
				/* next is /base/2 100 and this is /base 3
				 * NOT DONE
				 * next is /base/2 100 and this is /base 2
				 * NOT DONE
				 */
				/* elog(LOG,
				   "wasHandledAlready-5-Not skipping thisDirectory %s %s nextDirectory %s %s",
				   thisDirectory,
				   thisChild,
				   nextDirectory,
				   nextChild); */
				return false;
			}
			else
			{
				/* ext is /base/2 100 and this is /base 1 DONE*/
				/* elog(LOG,
				   "wasHandledAlready-6-Skipping thisDirectory %s %s nextDirectory %s %s",
				   thisDirectory,
				   thisChild,
				   nextDirectory,
				   nextChild);*/
				return true;
			}

			return false;
		}

	}

	/* DIFFERENT DIRECTORIES AND NEITHER IS A CHILD OF THE OTHER -
	 * JUST COMPARE THE DIRECTORIES
	 */
	Assert(strcmp(thisDirectory, nextDirectory) != 0);

	if (strcmp(thisDirectory, nextDirectory) < 0)
	{
		/* if thisDirectory is < nextDirectory then
		 * it has been processed already
		 */
		return true;
	}
	else
	{
		return false;
	}
}

int
FillBufferWithDirectoryTreeListing(
								   FileRepVerifyRequest_s * request,
	/* base dir should stay the same as we descend the directory tree -
	 * it is the base of the tree
	 */
								   char * priBaseDirectory,
								   char * mirrBaseDirectory,
	/* fromBasePath will start out ""
	 *  but then as we descend the directory tree it will
	 *  gather pieces of the path from
	 *  the base dir - at any point the currentDirectoryName
	 * is basically baseDir/fromBasePath
	 */
								   char * fromBasePath,
								   char * buf,
								   uint32 *used_len,
								   uint32 bufferSizeBytes,
								   uint32 *num_entries,
								   bool justCount,
								   bool *complete,

	/*
	 *  whereNext will be terms of the full path - baseDir/fromBasePath
	 *  it is only used to pick back up where we left off - it is not
	 *  used to put in the buffer
	 */
								   char * whereNextParent,
								   char * whereNextChild)
{

	int	 status = STATUS_OK;
	DIR		*dir = NULL;
	struct stat	 child_st;
	char		 currentDirectoryName[MAXPGPATH+1];
	char		 child_path[MAXPGPATH+1];
	char		 base_path[MAXPGPATH+1];
	struct dirent	*de = NULL;
	FileRepVerifySummaryItem_s summaryItem;
	FileRepVerifySummaryItem_s *pItem = NULL;
	uint32 numImmediateChildren =0;
	int this_len = 0;
	pg_crc32 directoryHashcode = 0;
	bool doFileHash = false;
	bool doDirectoryHash = false;
	bool keepChild = false;
	bool sendDirectory = false;

	verifyLog(
		request->logControl,	DEBUG1,
		"FillBufferWithDirectoryTreeListing: priBaseDirectory %s mirrBaseDirectory %s\n",
		priBaseDirectory,
		mirrBaseDirectory);

	doFileHash = request->doHash;

	/* We don't want to do a hash of a file contents because for
	 *  physical compare that can be wrong but should we always do
	 *  a directory hash?
	 */
#define DO_DIRECTORY_HASH 1
	if (DO_DIRECTORY_HASH)
	{
		doDirectoryHash = true;

	}
	else
	{
		doDirectoryHash = request->doHash;
	}

	if (justCount)
	{
		/* if just counting then no need to compute any hashes */
		doFileHash =false;
		doDirectoryHash = false;
	}

	*complete = false;

	if (doDirectoryHash)
	{
		//NEW_HASH
		INIT_CRC32C(directoryHashcode);
	}

	if (strlen(fromBasePath) == 0)
	{
		snprintf(currentDirectoryName,
				 MAXPGPATH+1,
				 "%s",
				 priBaseDirectory);
	}
	else
	{
		snprintf(currentDirectoryName,
				 MAXPGPATH+1,
				 "%s/%s",
				 priBaseDirectory, fromBasePath);
	}

	verifyLog(request->logControl,
			  DEBUG1,
			  "FillBufferWithDirectoryTreeListing: currentDirectoryName %s whereNext parent %s child %s",
			  currentDirectoryName,
			  whereNextParent,
			  whereNextChild);

	errno = 0;
	/* read the directory into the DIR structure */
	if ((dir = AllocateDir(currentDirectoryName)) == NULL)
	{
		/* VERIFICATION_ERROR_INTERNAL */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not read directory '%s' : %m "
						"verification aborted",
						currentDirectoryName),
				 errhint("run gpverify")));

		verifyLog(request->logControl,
				  DEBUG1,
				  "FillBufferWithDirectoryTreeListing: Unable to AllocateDir %s",
				  currentDirectoryName);

		return STATUS_ERROR;
	}

	while ((de = ReadDir(dir, currentDirectoryName)) != NULL)
	{

		/*
		   verifyLog(
		   request->logControl,
		   DEBUG5,
		   "FillBufferWithDirectoryTreeListing de->d_name: |%s| num_entries %d numImmediateChildren %d",
		   de->d_name,
		   *num_entries,
		   numImmediateChildren);
		   verifyLog(
		   request->logControl,
		   DEBUG5,
		   "strcmp(de->d_name, .) %d", strcmp(de->d_name, "."));
		   verifyLog(
		   request->logControl,
		   DEBUG5, "strcmp(de->d_name, ..) %d", strcmp(de->d_name, ".."));
		*/
		if (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0)
		{
			//verifyLog(request->logControl, DEBUG5, "continuing");
			continue;
		}

		/* TODO inefficient to look at each one and say whether handled or not?
		 * restrucutre to just start at whereNext and
		 * then go to the right, including doing up and to the right as necessary
		 */
		snprintf(child_path,
				 MAXPGPATH+1,
				 "%s/%s",
				 currentDirectoryName, de->d_name);


		/* MOVE THIS if just before the recursive call to FillBuffer */
/*	  if (fromBasePath != NULL)
	  {
	  snprintf(base_path, "%s/%s", fromBasePath, de->d_name);
	  }
	  else
	  {
	  snprintf(base_path, "%s", de->d_name);
	  }
*/

		if (stat(child_path, &child_st) != 0)
		{
			/* VERIFICATION_ERROR_INTERNAL */
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("could not stat file '%s/%s' : %m",
							child_path,
							de->d_name),
					errhint("run gpverify")));

			FreeDir(dir);
			verifyLog(request->logControl,
					  DEBUG1,
					  "FillBufferWithDirectoryTreeListing: Unable to stat %s",
					  child_path);
			return STATUS_ERROR;
		}
		keepChild =
			keepItem(child_path,
					 S_ISREG(child_st.st_mode),
					 request->verifyItem,
					 request->requestType);

		if (!keepChild)
		{
			verifyLog(request->logControl,
					  DEBUG3,
					  "Skipping CHILD %s",
					  child_path);
			continue;
		}
		else
		{
			verifyLog(request->logControl,
					  DEBUG3,
					  "Keeping CHILD %s",
					  child_path);
		}


		if (S_ISDIR(child_st.st_mode) &&
			isExcludedDirectory(
								de->d_name,
								request->numIgnoreDirs,
								request->ignoreDirs ))
		{
			verifyLog(request->logControl,
					  DEBUG3,
					  "FillBufferWithDirectoryTreeListing Skipping excluded directory %s/%s",
					  currentDirectoryName,
					  de->d_name);
		}
		if (S_ISREG(child_st.st_mode) &&
			isExcludedFile(
						   de->d_name,
						   request->numIgnoreFilePatterns,
						   request->ignoreFiles))
		{
			verifyLog(request->logControl,
					  DEBUG3,
					  "FillBufferWithDirectoryTreeListing Skipping excluded file %s/%s",
					  currentDirectoryName,
					  de->d_name);
		}
		if (S_ISDIR(child_st.st_mode) &&
			!isExcludedDirectory(
								 de->d_name,
								 request->numIgnoreDirs,
								 request->ignoreDirs ))
		{
			bool subDirComplete = false;

			numImmediateChildren++;
			if (doDirectoryHash)
			{

				//NEW_HASH
				directoryHashcode =
					COMP_CRC32C(
						directoryHashcode,
						de->d_name,
						strlen(de->d_name));

			}

			if (wasHandledAlready(
								  currentDirectoryName,
								  de->d_name,
								  whereNextParent,
								  whereNextChild))
			{
				verifyLog(
						  request->logControl,
						  DEBUG5,
						  "Skipping %s/%s because handled already",
						  currentDirectoryName,
						  de->d_name);
				continue;
			}

			/* JUST PASS IN '\0' to start out fromBasePath rather than NULL
			 * WOULD GET RID OF THIS SPECIAL CASE?
			 */
			if (strlen(fromBasePath) == 0)
			{
				snprintf(base_path,
						 MAXPGPATH+1,
						 "%s",
						 de->d_name);
			}
			else
			{
				snprintf(base_path,
						 MAXPGPATH+1,
						 "%s/%s",
						 fromBasePath, de->d_name);

			}
			status = FillBufferWithDirectoryTreeListing(
														request,
														priBaseDirectory,
														mirrBaseDirectory,
														base_path,
														buf,
														used_len,
														bufferSizeBytes,
														num_entries,
														justCount,
														&subDirComplete,
														whereNextParent,
														whereNextChild);

			if (status != STATUS_OK)
			{
				ereport(WARNING,
						(errmsg("verification failure, "
								"could not parse file '%s' "
								"verification aborted",
								child_path),
						 errhint("run gpverify")));

				FreeDir(dir);
				return status;
			}

			/* if this sub directory is not complete then no point
			 * continuing here - return all the way back to send this out
			 */
			if (!subDirComplete)
			{
				verifyLog(
						  request->logControl,
						  DEBUG3,
						  "Returning from partial directory %s whereNext parent %s child %s",
						  child_path,
						  whereNextParent,
						  whereNextChild);
				FreeDir(dir);
				return STATUS_OK;
			}

		}
		else if (
				 S_ISREG(child_st.st_mode) &&
				 !isExcludedFile(
								 de->d_name,
								 request->numIgnoreFilePatterns,
								 request->ignoreFiles))
		{

			numImmediateChildren++;
			if (doDirectoryHash)
			{

				//NEW_HASH
				COMP_CRC32C(
						directoryHashcode,
						de->d_name,
						strlen(de->d_name));

			}

			/* TODO -disable this check to see that the database version
			 * is really finding everything it needs to!!!
			 */
			if (request->skipRelFiles &&
/*				  looksLikeRelFileName(currentDirectoryName, de->d_name)) */
				looksLikeRelFileName(fromBasePath, de->d_name))
			{
				verifyLog(
						  request->logControl,
						  DEBUG5,
						  "Skipping compare %s %s %s looksLikeRelFileName",
						  priBaseDirectory,
						  fromBasePath,
						  de->d_name);
				continue;
			}
			else
			{
				verifyLog(
						  request->logControl,
						  DEBUG3,
						  "%s %s %s Do compare - not relfilename or !skipRelFiles (skipRelFiles %d)",
						  priBaseDirectory,
						  fromBasePath,
						  de->d_name,
						  request->skipRelFiles);
			}

			if (wasHandledAlready(
								  currentDirectoryName,
								  de->d_name,
								  whereNextParent,
								  whereNextChild))
			{
				verifyLog(
					request->logControl,
					DEBUG5,
					"Skipping %s/%s because handled already",
					currentDirectoryName, de->d_name);
				continue;
			}

			if (!justCount)
			{
				this_len = sizeof(FileRepVerifySummaryItem_s);

				/* add to bytes verified ? not really verified at this point */
				FileRepVerify_AddToTotalBlocksVerified(child_st.st_size);

				/* buffer is full so just send, set last Item so
				 * we can pick up where we left off
				 */
				if (bufferSizeBytes < *used_len + this_len)
				{

					verifyLog(
							  request->logControl,
							  DEBUG3,
							  "Buffer is full - set whereNextParent %s and whereNextChild %s",
							  currentDirectoryName,
							  de->d_name);

					strcpy(whereNextParent, currentDirectoryName);
					strcpy(whereNextChild, de->d_name);
					FreeDir(dir);

					return STATUS_OK;

				}
			} else {
				FileRepVerify_AddToTotalBlocksToVerify(child_st.st_size);
			}

			verifyLog(
					  request->logControl,
					  DEBUG3,
					  "FillBufferWithDirectoryTreeListing FILE de->d_name: |%s| num_entries %d",
					  de->d_name,
					  *num_entries);

			/* TODO: Is there really any point of sending a
			 * summaryItem.isFile = true entry if we don't compare the size
			 *  or the contents?
			 *  we shouldn't compare size or contents of relFileNodes -
			 *  but we are skipping those anyway	 -
			 *  everything else we find would
			 *  be a flat file - shouldn't we be comparing the size and
			 *  contents of those?
			 */

			(*num_entries)++;

			if (!justCount)
			{
				memset(&summaryItem, 0,
					   sizeof(FileRepVerifySummaryItem_s));
				summaryItem.isFile = true;

				/* WHEN WE PLACE ITEMS in the buffer -
				 * we just leave off the baseDir portion
				 * It has to be put back on on the mirror
				 * when we go to actually open files
				 */
				strncpy(summaryItem.baseDir,
						mirrBaseDirectory,
						MAXPGPATH+1);

				if (strlen(fromBasePath) != 0)
				{
					strncpy(summaryItem.fromBasePath,
							fromBasePath,
							strlen(fromBasePath)+1);
				}
				else
				{
					summaryItem.fromBasePath[0] = '\0';
				}
				strncpy(summaryItem.filename,
						de->d_name,
						strlen(de->d_name)+1);


				/*summaryItem.identifier.fileRepFlatFileIdentifier.blockId =
				  FILEREPFLATFILEID_WHOLE_FILE;*/

				if (request->includeFileSizeInfo)
				{
					summaryItem.summary.fileSummary.fileSize =
						child_st.st_size;

				}
				else
				{
					summaryItem.summary.fileSummary.fileSize = 0;
				}
				if (doFileHash)
				{
					char primaryFileName[MAXPGPATH+1];
					char mirrorFileName[MAXPGPATH+1];

					snprintf(primaryFileName, MAXPGPATH+1,
							 "%s/%s/%s",
							 priBaseDirectory,
							 summaryItem.fromBasePath,
							 summaryItem.filename);
					snprintf(mirrorFileName, MAXPGPATH+1,
							 "%s/%s/%s",
							 mirrBaseDirectory,
							 summaryItem.fromBasePath,
							 summaryItem.filename);

					FileRepVerify_ComputeFileHash(
						primaryFileName,
						&summaryItem.summary.fileSummary.fileContentsHashcode,
						FileRepVerifyCompareException_None,
						0,
						0,
						request->logControl);
				}
				else
				{
					summaryItem.summary.fileSummary.fileContentsHashcode = 0;
				}



				pItem = (FileRepVerifySummaryItem_s *)(buf + *used_len);
				memcpy((void *)pItem, (void *) (&(summaryItem)), this_len);

				*used_len+=this_len;
			}

		}
	}


	//NEW_HASH
	FIN_CRC32C(directoryHashcode);

	if (!justCount)
	{
		this_len = sizeof(FileRepVerifySummaryItem_s);

		if (bufferSizeBytes < *used_len + this_len)
		{

			verifyLog(
				request->logControl,
				DEBUG3,
				"Buffer is full -just have directory %s left",
				currentDirectoryName);
			strcpy(whereNextParent, currentDirectoryName);
			whereNextChild[0] = '\0';
			FreeDir(dir);
			return STATUS_OK;

		}
	}

	Assert(strlen("\0") == 0);


	if (request->requestType ==
		FileRepVerifyRequestType_FILE )
	{
		sendDirectory = false;
	}
	else if (request->requestType ==
			   FileRepVerifyRequestType_DIRECTORY )
	{
		int compareLen;
		compareLen = strlen(request->verifyItem);
		if (request->verifyItem[compareLen-1] == '\\')
		{
			compareLen --;
		}
		if (strncmp(currentDirectoryName,
					request->verifyItem,
					compareLen) ==0)
		{
			sendDirectory = true;
		}
		else
		{
			sendDirectory = false;
		}
	}
	else
	{
		sendDirectory = true;
	}
	if (sendDirectory
		&& ! wasHandledAlready(
			currentDirectoryName,
			"\0",
			whereNextParent,
			whereNextChild))
	{
		/* TODO: This is being done multiple times for the same directory */
		verifyLog(
			request->logControl,
			DEBUG3,
			"FillBufferWithDirectoryTreeListing DIRECTORY currentDirectoryName %s numImmediateChildren %d",
			currentDirectoryName,
			numImmediateChildren);

		if (!justCount)
		{
			memset(&summaryItem, 0, sizeof(FileRepVerifySummaryItem_s));
			summaryItem.isFile = false;

			strncpy(summaryItem.baseDir,
					mirrBaseDirectory,
					MAXPGPATH+1);

			/* WHEN WE PLACE ITEMS in the buffer -
			 * we just leave off the baseDir portion
			 * It has to be put back on on the mirror
			 * when we go to actually open files
			 */
			if (strlen(fromBasePath) != 0)
			{
				strncpy(summaryItem.fromBasePath,
						fromBasePath,
						strlen(fromBasePath)+1);
			}
			else
			{
				summaryItem.fromBasePath[0] = '\0';
			}

			summaryItem.filename[0] = '\0';
/*			summaryItem.identifier.fileRepFlatFileIdentifier.blockId =
					FILEREPFLATFILEID_WHOLE_FILE;
*/
			summaryItem.summary.dirSummary.numImmediateChildren =
				numImmediateChildren;


			if (doDirectoryHash)
			{
				summaryItem.summary.dirSummary.childrenListHashcode =
					directoryHashcode;
			}
			else
			{
				summaryItem.summary.dirSummary.childrenListHashcode =
					0;
			}
			/*
			   memcpy((void *)(*buf+ *used_len),
			   (void *) (&(summaryItem)), this_len);
			   *used_len+=this_len;
			   */
			pItem = (FileRepVerifySummaryItem_s *)(buf + *used_len);
			memcpy((void *)pItem, (void *) (&(summaryItem)), this_len);
			*used_len+=this_len;
		}
		(*num_entries)++;
	}

	*complete = true;
	FreeDir(dir);

	/* TODO: Update the global count of total blocks to be verified*/

	return STATUS_OK;
}



/* TODO -
 *   we should pass in listingBuffer and the size of the buffer -
 *  allocated it outside this function becaue it has to freed
 *  outside this function!
 */
int FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing(
																   FileRepVerifyRequest_s * request,
																   char * primaryBaseDir,
																   char * mirrorBaseDir,
																   char * listingBuffer,
																   uint32 bufferSizeBytes,
																   uint32 * num_entries,
																   bool justCount,
																   bool * complete,
																   char * whereNextParent,
																   char * whereNextChild)
						{
	int status = STATUS_OK;
	struct stat	 dir_st;
	uint32	used_len = 0;

	verifyLog(request->logControl,
			  DEBUG1,
			  "FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing: primary %s mirror %s whereNext parent %s child %s",
			  primaryBaseDir,
			  mirrorBaseDir,
			  whereNextParent,
			  whereNextChild);

	errno = 0;
	if (stat(primaryBaseDir, &dir_st) <	 0)
	{
		/*  - on the primary this would be an error if base or global
		 * or something did not exist?
		 * VERIFICATION_ERROR_INTERNAL
		 */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not stat file '%s' : %m "
						"verification aborted",
						primaryBaseDir),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	/* Is it actually a directory? */
	if (! S_ISDIR(dir_st.st_mode))
	{
		/* VERIFICATION_ERROR_INTERNAL */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"not a directory '%s' : %m "
						"verification aborted",
						primaryBaseDir),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}


	/* pass over every child of this directory -
	 *  does it pass over them in a reliable order?
	 */

	memset(listingBuffer, 0, bufferSizeBytes);
	used_len = 0;

	*num_entries = 0;

	status = FillBufferWithDirectoryTreeListing(
												request,
												primaryBaseDir,
												mirrorBaseDir,
												"",
												listingBuffer,
												&used_len,
												bufferSizeBytes,
												num_entries,
												justCount,
												complete,
												whereNextParent,
												whereNextChild);

	if (status != STATUS_OK)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not parse file '%s' "
						"verification aborted",
						primaryBaseDir),
				 errhint("run gpverify")));

		return status;
	}

	verifyLog(request->logControl,
			  DEBUG1,
			  "FileRepVerify_ComputeSummaryItemsFromRecursiveDirectoryListing: num_entries %d used_len %d bufferSizeBytes %d complete %d whereNextParent %s whereNextChild %s",
			  *num_entries,
			  used_len,
			  bufferSizeBytes,
			  (int) (*complete),
			  whereNextParent,
			  whereNextChild);

	return STATUS_OK;
}


int
FileRepVerify_ComputeFileHash(
							  char * fileName,
							  pg_crc32 *hashcode,
							  FileRepVerify_CompareException_e exception,
							  uint64 AO_logicalEof,  /* if exception is AO */
							  uint64 whichSegment,	  /* if exception is Heap */
							  FileRepVerifyLogControl_s logControl )
{

	struct stat	 info;
	FILE	   *f = NULL;

#define FILEREPVERIFY_COMPUTEFILEHASH_BUFSIZE BLCKSZ
	char buffer[FILEREPVERIFY_COMPUTEFILEHASH_BUFSIZE];
	int dataRead = 0;
	uint64 totalDataRead = 0;
	uint32 whichPage =0;

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepVerify_ComputeFileHash: %s",
			  fileName);

	if (exception == FileRepVerifyCompareException_AORelation)
	{
		verifyLog(
			logControl,
			DEBUG1,
			"FileRepVerify_ComputeFileHash AO Exception, AO_logicalEof " INT64_FORMAT " ",
			AO_logicalEof);
	}

	errno = 0;

	if (stat(fileName, &info) <	 0)
		{
		/* VERIFICATION_ERROR_INTERNAL
		 *  XXX - on the primary this would be an e_Comprror if
		 *  base or global or something did not exist?
		 *  TODO carefully consider what reaction should be -
		 *  what if file existed when we looked in directory or DB query
		 *  but is gone now - that isn't really an error just a legimate
		 *  change in state that we must accomodate
		 *  also what if it isn't on the mirror - we should report a
		 *  MISMATCH but it is not a error in the verification process per se
		 */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not stat file '%s' verification token '%s' : %m "
						"verification aborted",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	verifyLog(logControl,
			  DEBUG1,
			  "FileRepVerify_ComputeFileHash: %s size " INT64_FORMAT " ",
			  fileName,
			  info.st_size
		);
	/* Is it actually a file? */
	if (! S_ISREG(info.st_mode))
	{
		/* VERIFICATION_ERROR_INTERNAL */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"not a regular file '%s' verification token '%s' : %m "
						"verification aborted",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}




	f = fopen(fileName, "r");
	if (f == NULL)
	{
		/* VERIFICATION_ERROR_INTERNAL */
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("verification failure, "
						"could not open file '%s' verification token '%s' : %m "
						"verification aborted",
						fileName,
						logControl.logOptions.token),
				 errhint("run gpverify")));

		return STATUS_ERROR;
	}

	//NEW_HASH
	INIT_CRC32C(*hashcode);

	totalDataRead = 0;

	while ((dataRead =
			fread(buffer, 1, FILEREPVERIFY_COMPUTEFILEHASH_BUFSIZE, f)) != 0)
	{

		int dataToInclude = dataRead;
		whichPage++;

		if (exception ==		FileRepVerifyCompareException_HeapRelation)
		{
			bool validPage = false;
			bool madeChange = false;

			validPage =FileRepVerify_ProcessHeapPage(buffer, &madeChange, logControl);

			if (!validPage)
			{
				verifyLog(
					logControl,
					LOG,
					"VERIFICATION_ERROR_REPORT PRIMARY: corrupt heap file detected file %s whichSegment " INT64_FORMAT " whichPage %d",
					fileName,
					whichSegment,
					whichPage);
				/* TODO */
				/* writeFixFileRecord(
				   logControl,
				   "heap",
				   fileRepMirrorHostAddress,
				   mirrorFilename,
				   fileRepPrimaryHostAddress,
				   primaryFilename);
				 */


			}

		}
		else if (exception ==
				 FileRepVerifyCompareException_BtreeIndex)
		{
			bool validPage = false;
			bool madeChange = false;

			validPage =
				FileRepVerify_ProcessBtreeIndexPage(buffer, &madeChange, logControl);

			if (!validPage)
			{
				verifyLog(logControl,
						  LOG,
						  "VERIFICATION_ERROR_REPORT PRIMARY: corrupt btree index file detected file %s",
						  fileName);
				/* TODO */
				/* writeFixFileRecord(
				   logControl,
				   "btree",
				   fileRepMirrorHostAddress,
				   mirrorFilename,
				   fileRepPrimaryHostAddress,
				   primaryFilename);
				*/
			}
		}
		else if (exception ==
				 FileRepVerifyCompareException_AORelation)
		{
			if (totalDataRead >= AO_logicalEof)
			{
				/* we've read up to or past the end of the Eof */
				verifyLog(logControl,
						  DEBUG3,
						  "Stopping AO file at logical EOF " INT64_FORMAT "!!!",
						  AO_logicalEof);

				dataToInclude = 0;
				break;
			}
			if (totalDataRead + dataRead > AO_logicalEof)
			{
				verifyLog(
					logControl,
					DEBUG3,
					"Stopping AO file at logical EOF " INT64_FORMAT "!!!",
					AO_logicalEof);
				dataToInclude = AO_logicalEof - totalDataRead;
			}

		}


		if (dataToInclude > 0)
		{
			//NEW_HASH
			*hashcode =
				COMP_CRC32C(*hashcode,
					   buffer,
					   dataToInclude);
		}
		totalDataRead += dataRead;

	}

	//NEW_HASH
	FIN_CRC32C(*hashcode);

	fclose(f);
	return STATUS_OK;
}

bool
checkDirectoryInDB(char * dir)
{

	FilespaceScanToken token;
	char   primaryFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char   mirrorFilespaceLocationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen];
	char * primaryFilespaceLocation= NULL;
	char * mirrorFilespaceLocation = NULL;
	int16	 primary_dbId = 0;
	int16	 mirror_dbId = 0;
	PersistentFileSysState	persistentState;
	MirroredObjectExistenceState mirrorExistenceState;
	ItemPointerData	 persistentTid;
	int64		 persistentSerialNum = 0;
	Oid	   filespaceOid;

	bool found = false;
	char primaryBaseDirectory[MAXPGPATH+1];

	struct stat st;
/* 	elog(LOG, "canVerifyDirectory DatabasePath %s", DatabasePath); */

	/* try to stat the directory if not there return false */
	if (stat(dir, &st) < 0)
	{
		return false;
	}
	/* CAN"T DO THE PersistentFileSysObj_FilespaceScan here
	 * because "Unexpected internal error (lwlock.c:434)","FailedAssertion
	 * (""!(!(proc == ((void *)0) && IsUnderPostmaster))""
	*/
	getcwd(primaryBaseDirectory, MAXPGPATH+1);

	elog(LOG, "canVerifyDirectory primaryBaseDirectory %s",
		 primaryBaseDirectory);


	/* even if it is there make sure it is part of DB tree for this primary
	 * especially important if have multiple primary processes on the
	 * same physical machine
	 */
	if (keepItem(primaryBaseDirectory, false, dir,
				 FileRepVerifyRequestType_DIRECTORY))
	{
		elog(LOG, "keepItem dir %s DatabasePath %s YES", dir,
			 primaryBaseDirectory);
		return true;
	}
	else
	{
		elog(LOG, "keepItem dir %s DatabasePath %s NO", dir,
			 primaryBaseDirectory);
	}

	FilespaceScanToken_Init(&token);
	do
	{
		found = PersistentFileSysObj_FilespaceScan(
			&token,
			&filespaceOid,
			&primary_dbId,
			primaryFilespaceLocationBlankPadded,
			&mirror_dbId,
			mirrorFilespaceLocationBlankPadded,
			&persistentState,
			&mirrorExistenceState,
			&persistentTid,
			&persistentSerialNum);

		if (found)
		{
			PersistentFilespace_ConvertBlankPaddedLocation(
				&primaryFilespaceLocation,
				primaryFilespaceLocationBlankPadded,
				/* isPrimary */ true);
			PersistentFilespace_ConvertBlankPaddedLocation(
				&mirrorFilespaceLocation,
				mirrorFilespaceLocationBlankPadded,
				/* isPrimary */ false);

			if (keepItem(primaryFilespaceLocation,
						 false, dir, FileRepVerifyRequestType_DIRECTORY))
			{
				elog(LOG,
					 "keepItem dir %s primaryFilespace %s YES",
					 dir, primaryFilespaceLocation);

				return true;
			}
			else
			{
				elog(LOG,
					 "keepItem dir %s primaryFilespace %s NO",
					 dir, primaryFilespaceLocation);
			}


		}

	} while (found);
	return false;


}

bool
extractDirPath(char * file, char * dir, int maxLen)
{
	int len=0, i=0;

	len = strlen(file);
	strcpy(dir, file);

	if (len > maxLen)
	{
		return false;
	}

	/* grab the directory name out */
	for (i= len; i>=0; i--)
	{
		if (file[i] == '/')
		{
			dir[i] = '\0';
			return true;
		}
	}
	return false;
}

bool
extractDir(char * file, char * dir, int maxLen)
{
	int len =0, i=0;
	int endOfDir =0;
	int startOfDir =0;

	len = strlen(file);

	if (len > maxLen){
		return false;
	}

	for (i= len; i>=0; i--)
	{
		if (file[i] == '/')
		{

			endOfDir = i-1;
			break;
		}
	}

	for (i= endOfDir; i>=0; i--)
	{
		if (file[i] == '/')
		{
			startOfDir = i+1;
			break;
		}
	}

	len = endOfDir-startOfDir +1;
	if (len <=0){
		return false;
	}

	strncpy(dir, &(file[startOfDir]), len);
	dir[len] = '\0';
	return true;
}


bool
checkDirectoryExists(char * dir)
{
	/* Not really worth a procedure but left this
	 * way to contrast with checkDirectoryInDB
	 */

	struct stat st;


	/* try to stat the directory if not there return false */
	if (stat(dir, &st) < 0)
	{
		return false;
	} else {

		return true;
	}
}
bool
checkFileInDB(char * file)
{

	char dir[MAXPGPATH+1];

	struct	stat		st;

	/* try to stat the file if not there return false */
	if (stat(file, &st) < 0)
	{
		return false;
	}

	extractDirPath(file, dir, MAXPGPATH+1);

	elog(LOG, "checkFileInDB: file %s directory %s", file, dir);
	return checkDirectoryInDB(dir);

}
bool
checkFileExists(char * file)
{
	/* Not really worth a procedure but left this
	 * way to contrast with checkFileInDB
	 */

	struct	stat		st;

	/* try to stat the file if not there return false */
	if (stat(file, &st) < 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

int
processVerifyRequest(
	FileRepVerifyArguments *args,
	char* responseMsg,
	int responseMsgLen)
{
	int status = STATUS_OK;
	FileRepVerifyRequest_s *request = NULL;
	bool sameToken = false;
	bool waitForCompletion = false;

	printFileRepVerifyArguments(*args, DEBUG3);

	/* Check if the request can be allowed at this point of time */
	getFileRepRoleAndState(
		&fileRepRole,
		&segmentState,
		&dataState,
		NULL, NULL);

	if (!((fileRepRole == FileRepPrimaryRole) &&
		  (segmentState == SegmentStateReady) &&
		  (dataState == DataStateInSync)))
	{
		strncpy(responseMsg,
				"Failure: Verification request cannot be processed; primary and mirror not in sync\n",
				responseMsgLen);
		elog(LOG, "%s", responseMsg);
		return STATUS_ERROR;
	}

	if (strlen(args->token) == 0)
	{
		strncpy(responseMsg,
				"Failure: token must be specified with each verification request; token is empty\n",
				responseMsgLen);
		elog(LOG, "%s", responseMsg);
		return STATUS_ERROR;

	}

	FileRepVerify_LockAcquire();

	request = &(gFileRepVerifyShmem->request);

	if (strncmp(args->token,
				request->logControl.logOptions.token,
				TOKEN_LENGTH+1) == 0)
	{
		sameToken = true;
		elog(DEBUG5,
			 "verfication: same token %s", request->logControl.logOptions.token);
	}
	else
	{
		sameToken = false;
		elog(DEBUG5,
			 "verfication: different token |%s| |%s|",
			 request->logControl.logOptions.token, args->token);
	}

	if (gFileRepVerifyShmem->isVerificationInProgress)
	{

		/* check if new request is on same token (abort or suspend) */
		elog(DEBUG5,
			 "verification: inprogress token %s new request token %s",
			 request->logControl.logOptions.token,
			 args->token);

		if (sameToken)
		{
			/* this new request refers to the same token -
			 * is it an abort or suspend?
			 */
			elog(DEBUG5,
				 "verification: new request for in-progress token %s",
				 request->logControl.logOptions.token);

			if (args->abort)
			{

				/* SYNCHRONOUSABORTSUSPEND */
				if (gFileRepVerifyShmem->signalSynchronousRequests)
				{
					status = STATUS_ERROR;
					/* there is another abort or suspend that hasn't been picked up
					 * fail this one 
					 */
					strncpy(responseMsg,
							"Failure: another abort/suspend still in progress\n",
							responseMsgLen);
					elog(LOG, "%s", responseMsg);
					
				} 
				else 
				{

					waitForCompletion = true;
					elog(LOG,
						 "Abort requested for in progress request %s",
						 request->logControl.logOptions.token);
					request->abortRequested = true;
				}

			}
			else if (args->suspend)
			{
				/* SYNCHRONOUSABORTSUSPEND */
				if (gFileRepVerifyShmem->signalSynchronousRequests)
				{
					status = STATUS_ERROR;
					/* there is another abort or suspend that hasn't been picked up
					 * fail this one 
					 */
					strncpy(responseMsg,
							"Failure: another abort/suspend still in progress\n",
							responseMsgLen);
					elog(LOG, "%s", responseMsg);
					
				} 
				else 
				{
					waitForCompletion = true;
					elog(LOG,
						 "Suspend requested for in progress request %s",
						 request->logControl.logOptions.token);
					request->suspendRequested = true;
				}
				
				   
			}
			else
			{
				status = STATUS_ERROR;
				snprintf(
					responseMsg,
					responseMsgLen,
					"Failure: Illegal request to inprogress request with token %s\n",
					request->logControl.logOptions.token);
				elog(LOG, "%s", responseMsg);

			}

		}
		else
		{
			status = STATUS_ERROR;
			snprintf(
				responseMsg,
				responseMsgLen,
				"Failure: Request %s in progress; operation on token %s not allowed\n",
				request->logControl.logOptions.token, args->token);
			elog(LOG, "%s", responseMsg);

		}
	}
	else
	{

		elog(
			DEBUG5,
			"verification: no in progress request for last token %s new request token %s",
			request->logControl.logOptions.token,
			args->token);

		if (args->resume)
		{
			elog(LOG, "verification: resume request for token %s", args->token);

			if (!gFileRepVerifyShmem->availableForReuse &&
				(request->state == FileRepVerifyRequestState_SUSPENDED) )
			{
				if (sameToken)
				{
					elog(LOG,
						 "verification: resume request for token %s which is suspended",
						 args->token);
					gFileRepVerifyShmem->resume = true;
					gFileRepVerifyShmem->isNewRequestWaiting = true;
				}
				else
				{
					snprintf(responseMsg,
							 responseMsgLen,
							 "There is a suspended request, token %s, but this request is for a different token %s",
							 request->logControl.logOptions.token,
							 args->token);
					elog(LOG, "%s", responseMsg);
					status = STATUS_ERROR;
				}

			}
			else if (gFileRepVerifyShmem->availableForReuse)
			{
				elog(
					DEBUG5,
					"verification: resume is available for reuse for this resume %s",
					args->token);
				elog(DEBUG5,
					 "verification: can we find verification_%s.chkpnt in pg_verify",
					 args->token);

				/* read into the request structure and restart */
				if (FileRepPrimary_CheckpointFileIsPresent(
						args->token, responseMsg, responseMsgLen))
				{
					gFileRepVerifyShmem->resume = true;
					gFileRepVerifyShmem->isNewRequestWaiting = true;
					memcpy(&(request->fileRepVerifyArguments), args,
						   sizeof(FileRepVerifyArguments));
				}
				else
				{
					elog(LOG, "%s", responseMsg);
					status = STATUS_ERROR;
				}

			}



		}
		else
		{
			gFileRepVerifyShmem->resume = false;

			if (args->abort &&
				sameToken &&
				(request->state == FileRepVerifyRequestState_SUSPENDED) )
			{
				gFileRepVerifyShmem->availableForReuse = true;
				request->state = FileRepVerifyRequestState_ABORTED;
			}
			else if (args->fullVerify ||
					   (strlen(args->directoryTree) != 0) ||
					   (strlen(args->compareFile) !=0))
			{


				/* Can't call checkDirectoryinDB here
				 * because LwLock will fail when called
				 * in transition process unless transition process
				 * calls InitAuxiliaryProcess
				 * Here we will just check if directory exists
				 * then in the verification process we will check if
				 * it is in the DB - would be nice to fail here
				 * but gpverify should do this check on the parameter as well
				 */
				if ((strlen(args->directoryTree) != 0) &&
				    !checkDirectoryExists(args->directoryTree))
				{
					/* check that this is a valid directory for this primary */
					snprintf(
						responseMsg,
						responseMsgLen,
						"Failure: directory %s does not exist on this primary; cannot verify\n",
						args->directoryTree);
					elog(LOG, "%s", responseMsg);
					status = STATUS_ERROR;


				}

				/* Can't call checkFileinDB here
				 * because LwLock will fail when called
				 * in transition process unless transition process
				 * calls InitAuxiliaryProcess
				 * Here we will just check if file exists
				 * then in the verification process we will check if
				 * it is in the DB - would be nice to fail here
				 * but gpverify should do this check on the parameter as well
				 */
				else if ((strlen(args->compareFile) != 0) &&
				    !checkFileExists(args->compareFile))
				{
					/* check that this is a valid file for this primary */
					snprintf(
						responseMsg,
						responseMsgLen,
						"Failure: file %s does not exist on this primary; cannot verify\n",
						args->compareFile);
					elog(LOG, "%s", responseMsg);
					status = STATUS_ERROR;

				}
				else
				{
					elog(LOG,
						"verification request token '%s' ",
						args->token);

					if (gFileRepVerifyShmem->availableForReuse)
					{
						gFileRepVerifyShmem->isNewRequestWaiting = true;
						/* Copy the arguments to the shared memory.*/
						memcpy(&(request->fileRepVerifyArguments),
							   args, sizeof(FileRepVerifyArguments));
					}
					else
					{
						snprintf(
								 responseMsg,
								 responseMsgLen,
								 "Failure: cannot issue a verification request while there is one request suspended; Abort or resume token %s\n",
								 request->logControl.logOptions.token);

						elog(LOG, "%s", responseMsg);
						status = STATUS_ERROR;
					}
				}


			}
			else
			{
				status = STATUS_ERROR;

				if (args->abort)
				{
					strncpy(
						responseMsg,
						"Failure: no request in progress - abort not a legal new request\n",
						responseMsgLen);
					elog(LOG, "%s", responseMsg);
				}		else  if (args->suspend)
				{
					strncpy(
						responseMsg,
						"Failure: no request in progress - suspend not a legal new request\n",
						responseMsgLen);
					elog(LOG, "%s", responseMsg);
				}

			}
		} /* not resuming */
	} /* if(gFileRepVerifyShmem->isVerificationInProgress) */

	FileRepVerify_LockRelease();


	/* SYNCHRONOUSABORTSUSPEND*/
	/* This is the thread of control to which gpverify sends a request
	 * Originally all requests were asynchronous - they would drop
	 * off the request here (e.g. fill in the request struct and
	 * set isNewRequestWaiting or set a flag in the request structure
	 * saying that abort or suspend is requested
	 * The following snippet of code allows some requests (specifically
	 * suspend and abort because those are not long running) to be
	 * synchronous - they will wait here until the primary thread 
	 * sees that a suspend or abort has been requested and sets the
	 * signalSynchronousRequests field to release requests that are
	 * waiting for the request to finish 
	 */
 	if (waitForCompletion)
	{
		while (1)
		{

			/* if the process is shutting down then don't wait for
			 * completion
			 */
			FileRepSubProcess_ProcessSignals();

			if (FileRepSubProcess_GetState() == FileRepStateShutdown ||
				FileRepSubProcess_GetState() == FileRepStateShutdownBackends)
			{
				break;
			}

			FileRepVerify_LockAcquire();
			
			if (gFileRepVerifyShmem->signalSynchronousRequests) 
			{
				gFileRepVerifyShmem->signalSynchronousRequests = false;
				FileRepVerify_LockRelease();
				break;
			}
			FileRepVerify_LockRelease();
			
			pg_usleep(50000L); /* 50 ms */

		}
	} 

	return status;
}

FileRepVerifyArguments*
createNewFileRepVerifyArguments(void)
{
	/* freed in processFilerepVerifyRequest */
	FileRepVerifyArguments* result = palloc(sizeof(FileRepVerifyArguments));
	if (result == NULL)
	{
		ereport(WARNING,
				(errmsg("verification failure, "
						"could not allocate memory to run verification "
						"verification aborted"),
				 errhint("run gpverify")));

		return result;
	}
	clearFileRepVerifyArguments(result);
	return result;
}


char*
FileRepVerify_GetVerifyRequestState(void)
{
	/* Verification shared memory is not initialized on master */
	if (gFileRepVerifyShmem == NULL)
	{
		return "None";
	}

	if (gFileRepVerifyShmem->request.state == FileRepVerifyRequestState_NONE)
	{
		return "None";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_PENDING)
	{
		return "Pending";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_RUNNING)
	{
		return "Running";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_SUSPENDED)
	{
		return "Suspended";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_ABORTED)
	{
		return "Aborted";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_FAILED)
	{
		return "Failed";
	}
	else	 if (gFileRepVerifyShmem->request.state ==
				 FileRepVerifyRequestState_COMPLETE)
	{
		return "Complete";
	}
	else
	{
		return "Unknown";
	}
}

char * getCompareExceptionString( FileRepRelFileNodeInfo_s relinfo)
{

	switch (relinfo.exception)
	{
		case FileRepVerifyCompareException_None:
			return "unknown";
		case FileRepVerifyCompareException_AORelation:
			return "ao";
		case FileRepVerifyCompareException_HeapRelation:
			return "heap";
		case FileRepVerifyCompareException_BtreeIndex:
			return "btree";
	}

	return "unknown";
}

void writeDiffTableRecord(FileRepVerifyLogControl_s logControl,
						  char * token,
						  char * diffType,
						  char * primarySegHost,
						  char * primarySegFilePath,
						  char * mirrorSegHost,
						  char * mirrorSegFilePath,
						  int	 num_files_primary,
						  int	 num_files_mirror,
						  int	 blockNumber,
						  int	 size_on_primary,
						  int	 size_on_mirror,
						  RelFileNode	*relfilenode
	)

{

	if (logControl.filesAreOpen)
	{
#define MAX_DIFF_MESSAGE_LEN (MAXPGPATH*5)
		char diffMessage[MAX_DIFF_MESSAGE_LEN];

#define DIFF_MESSAGE_FORMAT "\n \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%d\", \"%d\", \"%d\", \"%d\", \"%d\", \"%d\", \"%d\", \"%d\" "
		snprintf(diffMessage,
				 MAX_FIX_MESSAGE_LEN,
				 DIFF_MESSAGE_FORMAT,
				 token, diffType,
				 primarySegHost,
				 primarySegFilePath,
				 mirrorSegHost,
				 mirrorSegFilePath,
				 num_files_primary,
				 num_files_mirror,
				 blockNumber,
				 size_on_primary,
				 size_on_mirror,
				 relfilenode ? relfilenode->spcNode : -1,
				 relfilenode ? relfilenode->dbNode	: -1,
				 relfilenode ? relfilenode->relNode : -1);

		if ((int) FileWrite(logControl.externalTableFile,
							diffMessage,
							strlen(diffMessage))!= strlen(diffMessage))
		{
			ereport(
				WARNING,
				(errcode_for_file_access(),
				 errmsg("could not write to file '%s' data '%p' length '%lu' : %m",
						logControl.logOptions.externalTablePath,
						diffMessage,
						(long unsigned int) strlen(diffMessage))));

		}
	}
}
