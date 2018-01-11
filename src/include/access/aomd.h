/*-------------------------------------------------------------------------
 *
 * aomd.h
 *	  Declarations and functions for supporting aomd.c
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/aomd.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOMD_H
#define AOMD_H

#include "storage/fd.h"
#include "utils/rel.h"

struct MirroredAppendOnlyOpen;	/* Defined in cdb/cdbmirroredappendonly.h */

extern int
			AOSegmentFilePathNameLen(Relation rel);

extern void FormatAOSegmentFileName(
						char *basepath,
						int segno,
						int col,
						int32 *fileSegNo,
						char *filepathname);

extern void MakeAOSegmentFileName(
					  Relation rel,
					  int segno,
					  int col,
					  int32 *fileSegNo,
					  char *filepathname);

extern bool OpenAOSegmentFile(
				  Relation rel,
				  char *filepathname,
				  int32 segmentFileNum,
				  int64 logicalEof,
				  struct MirroredAppendOnlyOpen *mirroredOpen);

extern void CloseAOSegmentFile(
				   struct MirroredAppendOnlyOpen *mirroredOpen);

extern void
TruncateAOSegmentFile(
					  struct MirroredAppendOnlyOpen *mirroredOpen, 
					  Relation rel, 
					  int64 offset);

#endif							/* AOMD_H */
