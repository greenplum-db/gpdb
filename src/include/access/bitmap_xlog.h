/*-------------------------------------------------------------------------
 *
 * bitmap_xlog.h
 *	  Bitmap index XLOG definitions.
 *
 *
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/bitmap_xlog.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BITMAP_XLOG_H
#define BITMAP_XLOG_H

#include "access/bitmap.h"
#include "access/xlogreader.h"
#include "lib/stringinfo.h"

/*
 * XLOG records for bitmap index operations
 *
 * Some information in high 4 bits of log record xl_info field.
 */
/* 0x10 is unused */
#define XLOG_BITMAP_INSERT_LOVITEM	0x20 /* add a new entry into a LOV page */
#define XLOG_BITMAP_INSERT_META		0x30 /* update the metapage */
#define XLOG_BITMAP_INSERT_BITMAP_LASTWORDS	0x40 /* update the last 2 words
													in a bitmap */
/* insert bitmap words into a bitmap page which is not the last one. */
#define XLOG_BITMAP_INSERT_WORDS		0x50
/* 0x60 is unused */
#define XLOG_BITMAP_UPDATEWORD			0x70
#define XLOG_BITMAP_UPDATEWORDS			0x80

/*
 * Maximum number of inserts that can be done in one WAL-logged operation.
 * This is a conservative estimate, I think the code actually flushes the
 * insert buffer whenever it has more than 1 page's worth of data, so
 * we could get away with 2 here. But better safe than sorry.
 */
#define MAX_BITMAP_PAGES_PER_INSERT	10

/*
 * The information about writing bitmap words to last bitmap page
 * and lov page.
 */
typedef struct xl_bm_bitmapwords
{
	RelFileNode 	bm_node;
	int32			bm_num_pages;

	/*
	 * If true, the first page is initialized from scratch. Otherwise it is
	 * an existing page, stored as backup block #1. (All subsequent pages
	 * are implicitly initialized.)
	 */
	bool			bm_init_first_page;

	/*
	 * The block number and offset for the lov page that is associated
	 * with this bitmap page.
	 */
	BlockNumber		bm_lov_blkno;
	OffsetNumber	bm_lov_offset;

	/* The information for the lov page */
	BM_HRL_WORD		bm_last_compword;
	BM_HRL_WORD		bm_last_word;
	uint8			lov_words_header;
	uint64			bm_last_setbit;

	/*  bm_num_pages xl_bm_bitmapwords_perpage structs follow */

} xl_bm_bitmapwords;

typedef struct xl_bm_bitmapwords_perpage
{
	/* The block number for the bitmap page */
	BlockNumber		bmp_blkno;

	/* The last tid location for this bitmap page */
	uint64			bmp_last_tid;

	/*
	 * The words stored in the following array to be written to this
	 * bitmap page.
	 */
	uint16			bmp_start_hword_no;
	uint16			bmp_num_hwords;
	uint16			bmp_start_cword_no;
	uint16			bmp_num_cwords;

	/*
	 * The following are arrays of content words and header
	 * words, at next MAXALIGN boundary. They are located one after the other.
	 * They are omitted, if a full-page-image of the page is created.
	 */
} xl_bm_bitmapwords_perpage;

typedef struct xl_bm_updatewords
{
	RelFileNode		bm_node;

	BlockNumber		bm_lov_blkno;
	OffsetNumber	bm_lov_offset;

	BlockNumber		bm_first_blkno;
	BM_HRL_WORD		bm_first_cwords[BM_NUM_OF_HRL_WORDS_PER_PAGE];
	BM_HRL_WORD		bm_first_hwords[BM_NUM_OF_HEADER_WORDS];
	uint64			bm_first_last_tid;
	uint64			bm_first_num_cwords;

	BlockNumber		bm_second_blkno;
	BM_HRL_WORD		bm_second_cwords[BM_NUM_OF_HRL_WORDS_PER_PAGE];
	BM_HRL_WORD		bm_second_hwords[BM_NUM_OF_HEADER_WORDS];
	uint64			bm_second_last_tid;
	uint64			bm_second_num_cwords;

	/* Indicate if this update involves two bitmap pages */
	bool			bm_two_pages;

	/* The previous next page number for the first page. */
	BlockNumber		bm_next_blkno;

	/* Indicate if the second page is a new last bitmap page */
	bool			bm_new_lastpage;
} xl_bm_updatewords;

typedef struct xl_bm_updateword
{
	RelFileNode		bm_node;

	BlockNumber		bm_blkno;
	int				bm_word_no;
	BM_HRL_WORD		bm_cword;
	BM_HRL_WORD		bm_hword;
} xl_bm_updateword;

/* The information about inserting a new lovitem into the LOV list. */
typedef struct xl_bm_lovitem
{
	RelFileNode 	bm_node;
	ForkNumber		bm_fork;

	BlockNumber		bm_lov_blkno;
	OffsetNumber	bm_lov_offset;
	BMLOVItemData	bm_lovItem;
	bool			bm_is_new_lov_blkno;
} xl_bm_lovitem;

/* The information about adding a new page */
typedef struct xl_bm_newpage
{
	RelFileNode 	bm_node;
	BlockNumber		bm_new_blkno;
} xl_bm_newpage;

/*
 * The information about changes on a bitmap page. 
 * If bm_isOpaque is true, then bm_next_blkno is set.
 */
typedef struct xl_bm_bitmappage
{
	RelFileNode 	bm_node;

	BlockNumber		bm_bitmap_blkno;

	bool			bm_isOpaque;
	BlockNumber		bm_next_blkno;

	uint32			bm_last_tid_location;
	uint32			bm_hrl_words_used;
	uint32			bm_num_words;
	/* for simplicity, we log the header words each time */
	BM_HRL_WORD 	hwords[BM_NUM_OF_HEADER_WORDS];
	/* followed by the "bm_num_words" content words. */
} xl_bm_bitmappage;

/* The information about changes to the last 2 words in a bitmap vector */
typedef struct xl_bm_bitmap_lastwords
{
	RelFileNode 	bm_node;

	BM_HRL_WORD		bm_last_compword;
	BM_HRL_WORD		bm_last_word;
	uint8			lov_words_header;
	uint64          bm_last_setbit;
	uint64          bm_last_tid_location;

	BlockNumber		bm_lov_blkno;
	OffsetNumber	bm_lov_offset;
} xl_bm_bitmap_lastwords;

/* The information about the changes in the metapage. */
typedef struct xl_bm_metapage
{
	RelFileNode 	bm_node;
	ForkNumber		bm_fork;
	Oid				bm_lov_heapId;		/* the relation id for the heap */
	Oid				bm_lov_indexId;		/* the relation id for the index */
	/* the block number for the last LOV pages. */
	BlockNumber		bm_lov_lastpage;
} xl_bm_metapage;

/*
 * prototypes for functions in bitmapxlog.c
 */
extern void bitmap_redo(XLogReaderState *record);
extern void bitmap_desc(StringInfo buf, XLogReaderState *record);
extern const char *bitmap_identify(uint8 info);

#endif							/* BITMAP_XLOG_H */
