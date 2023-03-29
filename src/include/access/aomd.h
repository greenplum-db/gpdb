/*-------------------------------------------------------------------------
 *
 * aomd.h
 *	  Declarations and functions for supporting aomd.c
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/aomd.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOMD_H
#define AOMD_H

#include "htup_details.h"
#include "storage/fd.h"
#include "utils/rel.h"

struct AOVacuumRelStats;
extern int AOSegmentFilePathNameLen(Relation rel);

extern void FormatAOSegmentFileName(
						char *basepath,
						int segno,
						int filenum,
						int32 *fileSegNo,
						char *filepathname);

extern void MakeAOSegmentFileName(
					  Relation rel,
					  int segno,
					  int filenum,
					  int32 *fileSegNo,
					  char *filepathname);

extern File OpenAOSegmentFile(char *filepathname, int64	logicalEof);

extern void CloseAOSegmentFile(File fd);

extern void
TruncateAOSegmentFile(File fd,
					  Relation rel,
					  int32 segmentFileNum,
					  int64 offset,
					  struct AOVacuumRelStats *vacrelstats);

extern void ao_truncate_one_rel(Relation rel);

extern void
mdunlink_ao(RelFileNodeBackend rnode, ForkNumber forkNumber, bool isRedo);

extern void
copy_append_only_data(RelFileNode src, RelFileNode dst, BackendId backendid, char relpersistence);

/*
 * return value should be true if the callback was able to find the given
 * segment number on disk and false otherwise. Failures during operation should
 * be handled out of band, either with a PG_THROW/elog/etc., or through the
 * passed user context.
 */
typedef bool (*ao_extent_callback)(int segno, void *ctx);

extern void ao_foreach_extent_file(ao_extent_callback callback, void *ctx);

extern void register_dirty_segment_ao(RelFileNode rnode, int segno, File vfd);

extern uint64 ao_rel_get_physical_size(Relation aorel);
#endif							/* AOMD_H */
