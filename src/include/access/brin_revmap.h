/*
 * brin_revmap.h
 *		Prototypes for BRIN reverse range maps
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/include/access/brin_revmap.h
 */

#ifndef BRIN_REVMAP_H
#define BRIN_REVMAP_H

#include "access/brin_tuple.h"
#include "storage/block.h"
#include "storage/buf.h"
#include "storage/itemptr.h"
#include "storage/off.h"
#include "utils/relcache.h"
#include "utils/snapshot.h"


/*
 * In revmap pages, each item stores an ItemPointerData.  These defines let one
 * find the logical revmap page number and index number of the revmap item for
 * the given heap block number.
 */
#define HEAPBLK_TO_REVMAP_BLK(pagesPerRange, heapBlk) \
	((heapBlk / pagesPerRange) / REVMAP_PAGE_MAXITEMS)
#define HEAPBLK_TO_REVMAP_INDEX(pagesPerRange, heapBlk) \
	((heapBlk / pagesPerRange) % REVMAP_PAGE_MAXITEMS)
#define HEAPBLK_TO_REVMAP_UPPER_BLK(pagesPerRange, heapBlk) \
	(HEAPBLK_TO_REVMAP_BLK(pagesPerRange, heapBlk) / REVMAP_UPPER_PAGE_MAXITEMS)
#define HEAPBLK_TO_REVMAP_UPPER_IDX(pagesPerRange, heapBlk) \
	(HEAPBLK_TO_REVMAP_BLK(pagesPerRange, heapBlk) % REVMAP_UPPER_PAGE_MAXITEMS)
#define REVMAP_UPPER_PAGE_TOTAL_NUM(pagesPerRange) \
	(HEAPBLK_TO_REVMAP_UPPER_BLK(pagesPerRange, MaxBlockNumber) + 1)

/* struct definition lives in brin_revmap.c */
typedef struct BrinRevmap BrinRevmap;

extern BrinRevmap *brinRevmapInitialize(Relation idxrel,
					 BlockNumber *pagesPerRange, Snapshot snapshot);
extern void brinRevmapTerminate(BrinRevmap *revmap);

extern void brinRevmapExtend(BrinRevmap *revmap,
				 BlockNumber heapBlk);
extern Buffer brinLockRevmapPageForUpdate(BrinRevmap *revmap,
							BlockNumber heapBlk);
extern void brinSetHeapBlockItemptr(Buffer rmbuf, BlockNumber pagesPerRange,
						BlockNumber heapBlk, ItemPointerData tid);
extern BrinTuple *brinGetTupleForHeapBlock(BrinRevmap *revmap,
						 BlockNumber heapBlk, Buffer *buf, OffsetNumber *off,
						 Size *size, int mode, Snapshot snapshot);
extern void brin_init_upper_pages(Relation index, BlockNumber pagesPerRange);
extern BlockNumber heapBlockGetCurrentAosegStart(BlockNumber heapBlk);
extern BlockNumber segnoGetCurrentAosegStart(int segno);
#endif   /* BRIN_REVMAP_H */
