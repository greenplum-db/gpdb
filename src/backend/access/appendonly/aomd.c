/*-------------------------------------------------------------------------
 *
 * aomd.c
 *	  This code manages append only relations that reside on magnetic disk.
 *	  It serves the same general purpose as smgr/md.c however we introduce
 *    AO specific file access functions mainly because would like to bypass 
 *	  md.c's and bgwriter's fsyncing. AO relations also use a non constant
 *	  block number to file segment mapping unlike heap relations.
 *
 *	  As of now we still let md.c create and unlink AO relations for us. This
 *	  may need to change if inconsistencies arise.
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/aomd.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "access/aomd.h"
#include "catalog/catalog.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include "utils/guc.h"
#include "cdb/cdbappendonlystorage.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"
#include "cdb/cdbappendonlyxlog.h"

int
AOSegmentFilePathNameLen(Relation rel)
{
	char		*basepath;
	int 		len;
		
	/* Get base path for this relation file */
	basepath = relpathbackend(rel->rd_node, rel->rd_backend, MAIN_FORKNUM);

	/*
	 * The basepath will be the RelFileNode number.  Optional part is dot "." plus 
	 * 6 digit segment file number.
	 */
	len = strlen(basepath) + 8;	// Generous.
	
	pfree(basepath);

	return len;
}

/*
 * Formats an Append Only relation file segment file name.
 *
 * The filepathname parameter assume sufficient space.
 */
void
FormatAOSegmentFileName(char *basepath,
						int segno,
						int col,
						int32 *fileSegNo,
						char *filepathname)
{
	int	pseudoSegNo;
	
	Assert(segno >= 0);
	Assert(segno <= AOTupleId_MaxSegmentFileNum);

	if (col < 0)
	{
		/*
		 * Row oriented Append-Only.
		 */
		pseudoSegNo = segno;		
	}
	else
	{
		/*
		 * Column oriented Append-only.
		 */
		pseudoSegNo = (col*AOTupleId_MultiplierSegmentFileNum) + segno;
	}
	
	*fileSegNo = pseudoSegNo;

	if (pseudoSegNo > 0)
	{
		sprintf(filepathname, "%s.%u", basepath, pseudoSegNo);
	}
	else
		strcpy(filepathname, basepath);
}

/*
 * Make an Append Only relation file segment file name.
 *
 * The filepathname parameter assume sufficient space.
 */
void
MakeAOSegmentFileName(Relation rel,
					  int segno,
					  int col,
					  int32 *fileSegNo,
					  char *filepathname)
{
	char	*basepath;
	int32   fileSegNoLocal;
	
	/* Get base path for this relation file */
	basepath = relpathbackend(rel->rd_node, rel->rd_backend, MAIN_FORKNUM);

	FormatAOSegmentFileName(basepath, segno, col, &fileSegNoLocal, filepathname);
	
	*fileSegNo = fileSegNoLocal;
	
	pfree(basepath);
}

/*
 * Open an Append Only relation file segment
 *
 * The fd module's PathNameOpenFile() is used to open the file, so the
 * the File* routines can be used to read, write, close, etc, the file.
 */
File
OpenAOSegmentFile(Relation rel, 
				  char *filepathname, 
				  int32	segmentFileNum,
				  int64	logicalEof)
{	
	char	   *dbPath;
	char		path[MAXPGPATH];
	int			fileFlags = O_RDWR | PG_BINARY;
	File		fd;

	dbPath = GetDatabasePath(rel->rd_node.dbNode, rel->rd_node.spcNode);

	if (segmentFileNum == 0)
		snprintf(path, MAXPGPATH, "%s/%u", dbPath, rel->rd_node.relNode);
	else
		snprintf(path, MAXPGPATH, "%s/%u.%u", dbPath, rel->rd_node.relNode, segmentFileNum);

	errno = 0;

	fd = PathNameOpenFile(path, fileFlags, 0600);
	if (fd < 0)
	{
		if (logicalEof == 0 && errno == ENOENT)
			return -1;

		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open Append-Only segment file \"%s\": %m",
						filepathname)));
	}
	pfree(dbPath);

	return fd;
}


/*
 * Close an Append Only relation file segment
 */
void
CloseAOSegmentFile(File fd)
{
	FileClose(fd);
}

/*
 * Truncate all bytes from offset to end of file.
 */
void
TruncateAOSegmentFile(File fd, Relation rel, int32 segFileNum, int64 offset)
{
	char *relname = RelationGetRelationName(rel);

	Assert(fd > 0);
	Assert(offset >= 0);

	/*
	 * Call the 'fd' module with a 64-bit length since AO segment files
	 * can be multi-gigabyte to the terabytes...
	 */
	if (FileTruncate(fd, offset) != 0)
		ereport(ERROR,
				(errmsg("\"%s\": failed to truncate data after eof: %m",
					    relname)));
	if (RelationNeedsWAL(rel))
		xlog_ao_truncate(rel->rd_node, segFileNum, offset);
}

/*
 * Delete All segment file extensions, in case it was an AO or AOCS
 * table. Ideally the logic works even for heap tables, but is only used
 * currently for AO and AOCS tables to avoid merge conflicts.
 *
 * There are different rules for the naming of the files, depending on
 * the type of table:
 *
 *   Heap Tables: contiguous extensions, no upper bound
 *   AO Tables: non contiguous extensions [.1 - .127]
 *   CO Tables: non contiguous extensions
 *          [  .1 - .127] for first column
 *          [.129 - .255] for second column
 *          [.257 - .283] for third column
 *          etc
 *
 *  Algorithm is coded with the assumption for CO tables that for a given
 *  concurrency level either all columns have the file or none.
 *
 *  1) Finds for which concurrency levels the table has files. This is
 *     calculated based off the first column. It performs 127
 *     (MAX_AOREL_CONCURRENCY) unlink().
 *  2) Iterates over the single column and deletes all concurrency level files.
 *     For AO tables this will exit fast.
 */
void
mdunlink_ao(const char *path)
{
	int path_size = strlen(path);
	char *segpath = (char *) palloc(path_size + 12);
	int segNumberArray[AOTupleId_MaxSegmentFileNum];
	int segNumberArraySize;
	char *segpath_suffix_position = segpath + path_size;

	strncpy(segpath, path, path_size);

	/*
	 * The 0 based extensions such as .128, .256, ... for CO tables are
	 * created by ALTER table or utility mode insert. These also need to be
	 * deleted; however, they may not exist hence are treated separately
	 * here. Column 0 concurrency level 0 file is always
	 * present. MaxHeapAttributeNumber is used as a sanity check; we expect
	 * the loop to terminate based on unlink return value.
	 */
	for(int colnum = 1; colnum <= MaxHeapAttributeNumber; colnum++)
	{
		sprintf(segpath_suffix_position, ".%u", colnum*AOTupleId_MultiplierSegmentFileNum);
		if (unlink(segpath) != 0)
		{
			/* ENOENT is expected after the end of the extensions */
			if (errno != ENOENT)
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove file \"%s\": %m", segpath)));
			else
				break;
		}
	}

	segNumberArraySize = 0;
	/* Collect all the segmentNumbers in [1..127]. */
	for (int concurrency_index = 1; concurrency_index < MAX_AOREL_CONCURRENCY;
		 concurrency_index++)
	{
		sprintf(segpath_suffix_position, ".%u", concurrency_index);
		if (unlink(segpath) != 0)
		{
			if (errno != ENOENT)
				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("could not remove file \"%s\": %m", segpath)));
			continue;
		}
		segNumberArray[segNumberArraySize] = concurrency_index;
		segNumberArraySize++;
	}

	if (segNumberArraySize == 0)
	{
		pfree(segpath);
		return;
	}

	for (int colnum = 1; colnum <= MaxHeapAttributeNumber; colnum++)
	{
		bool finished = false;
		for (int i = 0; i < segNumberArraySize; i++)
		{
			sprintf(segpath_suffix_position, ".%u",
					colnum*AOTupleId_MultiplierSegmentFileNum + segNumberArray[i]);
			if (unlink(segpath) != 0)
			{
				/* ENOENT is expected after the end of the extensions */
				if (errno != ENOENT)
					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("could not remove file \"%s\": %m", segpath)));
				else
				{
					finished = true;
					break;
				}
			}
		}
		if (finished)
			break;
	}

	pfree(segpath);
}

static void
copy_file(char *srcsegpath, char* dstsegpath,
		  RelFileNode dst, int segfilenum, bool use_wal)
{
	File		srcFile;
	File		dstFile;
	int64		left;
	off_t		offset;
	char       *buffer = palloc(BLCKSZ);
	int dstflags;

	srcFile = PathNameOpenFile(srcsegpath, O_RDONLY | PG_BINARY, 0600);
	if (srcFile < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 (errmsg("could not open file %s: %m", srcsegpath))));

	dstflags = O_WRONLY | O_EXCL | PG_BINARY;
	/*
	 * .0 relfilenode is expected to exist before calling this
	 * function. Caller calls RelationCreateStorage() which creates the base
	 * file for the relation. Hence use different flag for the same.
	 */
	if (segfilenum)
		dstflags |= O_CREAT;

	dstFile = PathNameOpenFile(dstsegpath, dstflags, 0600);
	if (dstFile < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 (errmsg("could not create destination file %s: %m", dstsegpath))));

	left = FileSeek(srcFile, 0, SEEK_END);
	if (left < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 (errmsg("could not seek to end of file %s: %m", srcsegpath))));

	if (FileSeek(srcFile, 0, SEEK_SET) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 (errmsg("could not seek to beginning of file %s: %m", srcsegpath))));

	offset = 0;
	while(left > 0)
	{
		int			len;

		CHECK_FOR_INTERRUPTS();

		len = Min(left, BLCKSZ);
		if (FileRead(srcFile, buffer, len) != len)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read %d bytes from file \"%s\": %m",
							len, srcsegpath)));

		if (FileWrite(dstFile, buffer, len) != len)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write %d bytes to file \"%s\": %m",
							len, dstsegpath)));

		if (use_wal)
			xlog_ao_insert(dst, segfilenum, offset, buffer, len);

		offset += len;
		left -= len;
	}

	if (FileSync(dstFile) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not fsync file \"%s\": %m",
						dstsegpath)));
	FileClose(srcFile);
	FileClose(dstFile);
	pfree(buffer);
}

/*
 * Like copy_relation_data(), but for AO tables.
 *
 * Currently, AO tables don't have any extra forks.
 */
void
copy_append_only_data(RelFileNode src, RelFileNode dst, BackendId backendid, char relpersistence)
{
	char *srcpath;
	char *dstpath;
	char srcsegpath[MAXPGPATH + 12];
	char dstsegpath[MAXPGPATH + 12];
	int segNumberArray[AOTupleId_MaxSegmentFileNum];
	int segNumberArraySize;
	bool use_wal;

	/*
	 * We need to log the copied data in WAL iff WAL archiving/streaming is
	 * enabled AND it's a permanent relation.
	 */
	use_wal = XLogIsNeeded() && relpersistence == RELPERSISTENCE_PERMANENT;

	srcpath = relpathbackend(src, backendid, MAIN_FORKNUM);
	dstpath = relpathbackend(dst, backendid, MAIN_FORKNUM);

	/*
	 * For CO table, ALTER table, or utility mode insert files .128.. .256 0
	 * based extensions are created. These also need to be copied; however,
	 * they may not exist hence are treated separately here. Column 0
	 * concurrency level 0 file is always present. MaxHeapAttributeNumber is
	 * used as a sanity check; we expect the loop to terminate based on
	 * access()'s return value.
	 */
	copy_file(srcpath, dstpath, dst, 0, use_wal);
	for(int colnum = 1; colnum <= MaxHeapAttributeNumber; colnum++)
	{
		int suffix = colnum*AOTupleId_MultiplierSegmentFileNum;
		sprintf(srcsegpath, "%s.%u", srcpath, suffix);
		if (access(srcsegpath, F_OK) != 0)
		{
			/* ENOENT is expected after the end of the extensions */
			if (errno != ENOENT)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("access failed for file \"%s\": %m", srcsegpath)));
			break;
		}
		sprintf(dstsegpath, "%s.%u", dstpath, suffix);
		copy_file(srcsegpath, dstsegpath, dst, suffix, use_wal);
	}

	segNumberArraySize=0;
	/* Collect all the segmentNumbers in [1..127]. */
	for (int concurrency_index = 1; concurrency_index < MAX_AOREL_CONCURRENCY;
		 concurrency_index++)
	{
		sprintf(srcsegpath, "%s.%u", srcpath, concurrency_index);
		if (access(srcsegpath, F_OK) != 0)
		{
			if (errno != ENOENT)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("access failed for file \"%s\": %m", srcsegpath)));
			continue;
		}
		sprintf(dstsegpath, "%s.%u", dstpath, concurrency_index);
		copy_file(srcsegpath, dstsegpath, dst, concurrency_index, use_wal);

		segNumberArray[segNumberArraySize] = concurrency_index;
		segNumberArraySize++;
	}

	if (segNumberArraySize == 0)
		return;

	for (int colnum = 1; colnum <= MaxHeapAttributeNumber; colnum++)
	{
		bool finished = false;
		for (int concurrency_index = 0; concurrency_index < segNumberArraySize;
			 concurrency_index++)
		{
			int suffix =
				colnum*AOTupleId_MultiplierSegmentFileNum + segNumberArray[concurrency_index];
			sprintf(srcsegpath, "%s.%u", srcpath, suffix);
			if (access(srcsegpath, F_OK) != 0)
			{
				/* ENOENT is expected after the end of the extensions */
				if (errno != ENOENT)
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("access failed for file \"%s\": %m", srcsegpath)));
				finished = true;
				break;
			}
			sprintf(dstsegpath, "%s.%u", dstpath, suffix);
			copy_file(srcsegpath, dstsegpath, dst, suffix, use_wal);
		}
		if (finished)
			break;
	}
}
