/*-------------------------------------------------------------------------
 *
 * brin_xlog.h
 *	  POSTGRES BRIN access XLOG definitions.
 *
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/brin_xlog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BRIN_XLOG_H
#define BRIN_XLOG_H

#include "access/xlogreader.h"
#include "lib/stringinfo.h"
#include "storage/bufpage.h"
#include "storage/itemptr.h"
#include "storage/relfilenode.h"
#include "utils/relcache.h"


/*
 * WAL record definitions for BRIN's WAL operations
 *
 * XLOG allows to store some information in high 4 bits of log
 * record xl_info field.
 */
#define XLOG_BRIN_CREATE_INDEX		0x00
#define XLOG_BRIN_INSERT			0x10
#define XLOG_BRIN_UPDATE			0x20
#define XLOG_BRIN_SAMEPAGE_UPDATE	0x30
#define XLOG_BRIN_REVMAP_EXTEND		0x40
#define XLOG_BRIN_REVMAP_VACUUM		0x50
#define XLOG_BRIN_REVMAP_INIT_UPPER_BLK	0x60
#define XLOG_BRIN_REVMAP_EXTEND_UPPER	0x70
#define XLOG_BRIN_OPMASK			0x70
/*
 * When we insert the first item on a new page, we restore the entire page in
 * redo.
 */
#define XLOG_BRIN_INIT_PAGE		0x80

/*
 * This is what we need to know about a BRIN index create.
 *
 * Backup block 0: metapage
 */
typedef struct xl_brin_createidx
{
	BlockNumber pagesPerRange;
	uint16		version;
	bool 		isAo;
} xl_brin_createidx;
#define SizeOfBrinCreateIdx (offsetof(xl_brin_createidx, isAo) + sizeof(bool))


typedef struct xl_brin_createupperblk
{
	BlockNumber targetBlk;
} xl_brin_createupperblk;
#define SizeOfBrinCreateUpperBlk (offsetof(xl_brin_createupperblk, targetBlk) \
								  + sizeof(BlockNumber))

/*
 * This is what we need to know about a BRIN tuple insert
 *
 * Backup block 0: main page, block data is the new BrinTuple.
 * Backup block 1: revmap page
 */
typedef struct xl_brin_insert
{
	BlockNumber heapBlk;

	/* extra information needed to update the revmap */
	BlockNumber pagesPerRange;

	/* offset number in the main page to insert the tuple to. */
	OffsetNumber offnum;
} xl_brin_insert;

#define SizeOfBrinInsert	(offsetof(xl_brin_insert, offnum) + sizeof(OffsetNumber))

/*
 * A cross-page update is the same as an insert, but also stores information
 * about the old tuple.
 *
 * Like in xlog_brin_update:
 * Backup block 0: new page, block data includes the new BrinTuple.
 * Backup block 1: revmap page
 *
 * And in addition:
 * Backup block 2: old page
 */
typedef struct xl_brin_update
{
	/* offset number of old tuple on old page */
	OffsetNumber oldOffnum;

	xl_brin_insert insert;
} xl_brin_update;

#define SizeOfBrinUpdate	(offsetof(xl_brin_update, insert) + SizeOfBrinInsert)

/*
 * This is what we need to know about a BRIN tuple samepage update
 *
 * Backup block 0: updated page, with new BrinTuple as block data
 */
typedef struct xl_brin_samepage_update
{
	OffsetNumber offnum;
} xl_brin_samepage_update;

#define SizeOfBrinSamepageUpdate		(sizeof(OffsetNumber))

/*
 * This is what we need to know about a revmap extension
 *
 * Backup block 0: metapage
 * Backup block 1: new revmap page
 */
typedef struct xl_brin_revmap_extend
{
	/*
	 * XXX: This is actually redundant - the block number is stored as part of
	 * backup block 1.
	 */
	BlockNumber targetBlk;
} xl_brin_revmap_extend;
#define SizeOfBrinRevmapExtend	(offsetof(xl_brin_revmap_extend, targetBlk) + \
								 sizeof(BlockNumber))


typedef struct xl_brin_revmap_extend_upper
{
	BlockNumber heapBlk;

	/* extra information needed to update the revmap */
	BlockNumber pagesPerRange;
	BlockNumber revmapBlk;
} xl_brin_revmap_extend_upper;
#define SizeOfBrinRevmapExtendUpper	(offsetof(xl_brin_revmap_extend_upper, pagesPerRange) + \
									 sizeof(BlockNumber))



extern void brin_redo(XLogReaderState *record);
extern void brin_desc(StringInfo buf, XLogReaderState *record);
extern const char *brin_identify(uint8 info);

#endif   /* BRIN_XLOG_H */
