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
	/*
	 * Perform redo of AO XLOG records only for standby mode.  We do not need
	 * to replay AO XLOG records in normal mode because fsync is performed on
	 * file close.
	 */
	if (IsStandbyMode())
		ao_xlog_insert(record);
}

void
appendonly_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	xl_ao_insert *xlrec = (xl_ao_insert *) XLogRecGetData(record);
	uint8		xl_info = record->xl_info;
	uint8		info = xl_info & ~XLR_INFO_MASK;

	if (info == XLOG_APPENDONLY_INSERT)
	{
		appendStringInfo(buf, "insert: rel %u/%u/%u seg/offset:%u/" INT64_FORMAT " len:%lu",
						 xlrec->node.spcNode, xlrec->node.dbNode,
						 xlrec->node.relNode, xlrec->segment_filenum,
						 xlrec->offset, record->xl_len - SizeOfAOInsert);
	}
	else
		appendStringInfo(buf, "UNKNOWN");
}
#endif							/* USE_SEGWALREP */
