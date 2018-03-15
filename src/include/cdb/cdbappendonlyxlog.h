/*-------------------------------------------------------------------------
 *
 * cdbappendonlyxlog.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlyxlog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYXLOG_H
#define CDBAPPENDONLYXLOG_H

#include "storage/fd.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"

extern void ao_create_segfile_replay(XLogRecord *record);
extern void ao_insert_replay(XLogRecord *record);
extern void xlog_ao_insert(RelFileNode relFileNode, int32 segmentFileNum,
			   int64 offset, void *buffer, int32 bufferLen);
extern void xlog_ao_truncate(RelFileNode relFileNode, int32 segmentFileNum, int64 offset);
extern void ao_truncate_replay(XLogRecord *record);

#endif   /* CDBAPPENDONLYXLOG_H */
