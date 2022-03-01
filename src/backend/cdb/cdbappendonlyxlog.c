/*-------------------------------------------------------------------------
 *
 * cdbappendonlyxlog.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlyxlog.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>

#include "access/aomd.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "cdb/cdbappendonlyxlog.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "utils/faultinjector.h"
#include "utils/faultinjector_lists.h"

/*
 * Insert an AO XLOG/AOCO record.
 *
 * This is also used with 0 length, to mark creation of a new segfile.
 */
void
xlog_ao_insert(RelFileNode relFileNode, int32 segmentFileNum,
			   int64 offset, void *buffer, int32 bufferLen)
{
	xl_ao_insert	xlaoinsert;

	xlaoinsert.target.node = relFileNode;
	xlaoinsert.target.segment_filenum = segmentFileNum;
	xlaoinsert.target.offset = offset;

	XLogBeginInsert();
	XLogRegisterData((char*) &xlaoinsert, SizeOfAOInsert);

	if (bufferLen != 0)
		XLogRegisterData((char*) buffer, bufferLen);

	SIMPLE_FAULT_INJECTOR("xlog_ao_insert");

	XLogInsert(RM_APPEND_ONLY_ID, XLOG_APPENDONLY_INSERT);

	wait_to_avoid_large_repl_lag();
}

static void
ao_insert_replay(XLogReaderState *record)
{
	char	   *dbPath;
	char		path[MAXPGPATH];
	int			written_len;
	File		file;
	int			fileFlags;
	xl_ao_insert *xlrec = (xl_ao_insert *) XLogRecGetData(record);
	char	   *buffer = (char *) xlrec + SizeOfAOInsert;
	uint32		len = XLogRecGetDataLen(record) - SizeOfAOInsert;

	dbPath = GetDatabasePath(xlrec->target.node.dbNode,
							 xlrec->target.node.spcNode);

	if (xlrec->target.segment_filenum == 0)
		snprintf(path, MAXPGPATH, "%s/%u", dbPath, xlrec->target.node.relNode);
	else
		snprintf(path, MAXPGPATH, "%s/%u.%u", dbPath, xlrec->target.node.relNode, xlrec->target.segment_filenum);
	pfree(dbPath);

	fileFlags = O_RDWR | PG_BINARY;

	/* When writing from the beginning of the file, it might not exist yet. Create it. */
	if (xlrec->target.offset == 0)
		fileFlags |= O_CREAT;
	file = PathNameOpenFile(path, fileFlags);
	if (file < 0)
	{
		XLogAOSegmentFile(xlrec->target.node, xlrec->target.segment_filenum);
		return;
	}

	written_len = FileWrite(file, buffer, len, xlrec->target.offset,
							WAIT_EVENT_COPY_FILE_WRITE);
	if (written_len < 0 || written_len != len)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("failed to write %d bytes in file \"%s\": %m",
						len,
						path)));
	}

	register_dirty_segment_ao(xlrec->target.node,
							  xlrec->target.segment_filenum,
							  file);

	FileClose(file);
}

/*
 * AO/CO truncate xlog record insertion.
 */
void xlog_ao_truncate(RelFileNode relFileNode, int32 segmentFileNum, int64 offset)
{
	xl_ao_truncate	xlaotruncate;

	xlaotruncate.target.node = relFileNode;
	xlaotruncate.target.segment_filenum = segmentFileNum;
	xlaotruncate.target.offset = offset;

	XLogBeginInsert();
	XLogRegisterData((char*) &xlaotruncate, sizeof(xl_ao_truncate));

	XLogInsert(RM_APPEND_ONLY_ID, XLOG_APPENDONLY_TRUNCATE);
}

static void
ao_truncate_replay(XLogReaderState *record)
{
	char	   *dbPath;
	char		path[MAXPGPATH];
	File		file;

	xl_ao_truncate *xlrec = (xl_ao_truncate*) XLogRecGetData(record);

	dbPath = GetDatabasePath(xlrec->target.node.dbNode,
							 xlrec->target.node.spcNode);

	if (xlrec->target.segment_filenum == 0)
		snprintf(path, MAXPGPATH, "%s/%u", dbPath, xlrec->target.node.relNode);
	else
		snprintf(path, MAXPGPATH, "%s/%u.%u", dbPath, xlrec->target.node.relNode, xlrec->target.segment_filenum);
	pfree(dbPath);
	dbPath = NULL;

	file = PathNameOpenFile(path, O_RDWR | PG_BINARY);
	if (file < 0)
	{
		/*
		 * Primary creates the file first and then writes the xlog record for
		 * the creation for AO tables similar to heap.  Hence, file can get
		 * created on primary without writing xlog record if failure happens
		 * on primary just after creating the file. This creates situation
		 * where VACUUM can generate truncate record based on aoseg entry with
		 * eof 0 and file present on primary. Then during replay mirror may
		 * not have the file, as was never created on mirror. So, avoid adding
		 * the entry to invalid hash table for truncate at offset zero
		 * (EOF=0).  This avoids mirror PANIC, as anyways truncate to zero is
		 * same as file not present.
		 */
		if (xlrec->target.offset != 0)
			XLogAOSegmentFile(xlrec->target.node, xlrec->target.segment_filenum);
		return;
	}

	if (FileTruncate(file, xlrec->target.offset, WAIT_EVENT_DATA_FILE_TRUNCATE) != 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("failed to truncate file \"%s\" to offset:" INT64_FORMAT " : %m",
						path, xlrec->target.offset)));
	}

	FileClose(file);
}

void
appendonly_redo(XLogReaderState *record)
{
	uint8         info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	/*
	 * Do not perform redo of AO XLOG records for crash recovery mode. We do
	 * not need to replay AO XLOG records in this case because fsync
	 * is performed on file close.
	 */
	if (IsCrashRecoveryOnly())
		return;

	switch (info)
	{
		case XLOG_APPENDONLY_INSERT:
			ao_insert_replay(record);
			break;
		case XLOG_APPENDONLY_TRUNCATE:
			ao_truncate_replay(record);
			break;
		default:
			elog(PANIC, "appendonly_redo: unknown code %u", info);
	}
}
