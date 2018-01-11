/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorage.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlystorage.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "storage/gp_compress.h"
#include "cdb/cdbappendonlystorage_int.h"
#include "cdb/cdbappendonlystorage.h"
#include "utils/guc.h"

#ifdef USE_SEGWALREP
#include "cdb/cdbappendonlyam.h"
#endif							/* USE_SEGWALREP */


int32
AppendOnlyStorage_GetUsableBlockSize(int32 configBlockSize)
{
	int32		result;

	if (configBlockSize > AOSmallContentHeader_MaxLength)
		result = AOSmallContentHeader_MaxLength;
	else
		result = configBlockSize;

	/*
	 * Round down to 32-bit boundary.
	 */
	result = (result / sizeof(uint32)) * sizeof(uint32);

	return result;
}

#ifdef USE_SEGWALREP
void
appendonly_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8         xl_info = record->xl_info;
	uint8         info = xl_info & ~XLR_INFO_MASK;

	/*
	 * Perform redo of AO XLOG records only for standby mode. We do
	 * not need to replay AO XLOG records in normal mode because fsync
	 * is performed on file close.
	 */
	if (!IsStandbyMode())
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

void
appendonly_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8		  xl_info = record->xl_info;
	uint8		  info = xl_info & ~XLR_INFO_MASK;

	switch (info)
	{
		case XLOG_APPENDONLY_INSERT:
			{
				xl_ao_insert *xlrec = (xl_ao_insert *)XLogRecGetData(record);

				appendStringInfo(
					buf,
					"insert: rel %u/%u/%u seg/offset:%u/" INT64_FORMAT " len:%lu",
					xlrec->target.node.spcNode, xlrec->target.node.dbNode,
					xlrec->target.node.relNode, xlrec->target.segment_filenum,
					xlrec->target.offset, record->xl_len - SizeOfAOInsert);
			}
			break;
		case XLOG_APPENDONLY_TRUNCATE:
			{
				xl_ao_truncate *xlrec = (xl_ao_truncate *)XLogRecGetData(record);

				appendStringInfo(
					buf,
					"truncate: rel %u/%u/%u seg/offset:%u/" INT64_FORMAT,
					xlrec->target.node.spcNode, xlrec->target.node.dbNode,
					xlrec->target.node.relNode, xlrec->target.segment_filenum,
					xlrec->target.offset);
			}
			break;
		default:
			appendStringInfo(buf, "UNKNOWN");
	}
}
#endif
