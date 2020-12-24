/*-------------------------------------------------------------------------
 *
 * cdbappendonlyxlog.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlyxlog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYXLOG_H
#define CDBAPPENDONLYXLOG_H


#include "access/xlogreader.h"
#include "lib/stringinfo.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"



#define XLOG_APPENDONLY_INSERT			0x00
#define XLOG_APPENDONLY_TRUNCATE		0x10

typedef struct
{
	RelFileNode node;
	uint32		segment_filenum;
	int64		offset;
} xl_ao_target;

#define SizeOfAOTarget (offsetof(xl_ao_target, offset) + sizeof(int64))

typedef struct
{
	/* meta data about the inserted block of AO data*/
	xl_ao_target target;
	/* BLOCK DATA FOLLOWS AT END OF STRUCT */
} xl_ao_insert;

#define SizeOfAOInsert (offsetof(xl_ao_insert, target) + SizeOfAOTarget)

typedef struct
{
	/* meta data about the truncated AO/CO file*/
	xl_ao_target target;
} xl_ao_truncate;

extern void xlog_ao_insert(RelFileNode relFileNode, int32 segmentFileNum,
			   int64 offset, void *buffer, int32 bufferLen);
extern void xlog_ao_truncate(RelFileNode relFileNode, int32 segmentFileNum, int64 offset);


extern void appendonly_redo(XLogReaderState *record);

/* in appendonlydesc.c */
extern void appendonly_desc(StringInfo buf, XLogReaderState *record);
extern const char *appendonly_identify(uint8 info);

#endif   /* CDBAPPENDONLYXLOG_H */
