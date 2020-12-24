/*-------------------------------------------------------------------------
 *
 * appendonlydesc.c
 *	  rmgr descriptor routines for cdb/cdbappendonlystorage.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/rmgrdesc/appendonlydesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbappendonlystorage_int.h"
#include "cdb/cdbappendonlystorage.h"
#include "cdb/cdbappendonlyxlog.h"

void
appendonly_desc(StringInfo buf, XLogReaderState *record)
{
	uint8		  info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

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
					xlrec->target.offset, XLogRecGetDataLen(record) - SizeOfAOInsert);
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

const char *
appendonly_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case XLOG_APPENDONLY_INSERT:
			id = "APPENDONLY_INSERT";
			break;
		case XLOG_APPENDONLY_TRUNCATE:
			id = "APPENDONLY_TRUNCATE";
			break;
	}

	return id;
}
