/*-------------------------------------------------------------------------
 *
 * bitmap.h
 *	header file for on-disk bitmap index access method implementation.
 *
 * Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	$PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */

#ifndef BITMAP_H
#define BITMAP_H

#include "access/htup.h"
#include "access/itup.h"
#include "access/relscan.h"
#include "access/sdir.h"
#include "access/xlogutils.h"
#include "storage/lock.h"
#include "miscadmin.h"

#define BM_READ		BUFFER_LOCK_SHARE
#define BM_WRITE	BUFFER_LOCK_EXCLUSIVE
#define BM_NOLOCK	(-1)

/*
 * The size in bits of a hybrid run-length(HRL) word.
 * If you change this, you should change the type for
 * a HRL word as well.
 */
#define BM_HRL_WORD_SIZE		64

/* the type for a HRL word */
typedef uint64			BM_HRL_WORD;

#define BM_HRL_WORD_LEFTMOST	(BM_HRL_WORD_SIZE-1)

#define BITMAP_VERSION 2
#define BITMAP_MAGIC 0x4249544D

/*
 * Metapage, always the first page (page 0) in the index.
 *
 * This page stores some meta-data information about this index.
 */
typedef struct BMMetaPageData 
{
	/*
	 * The magic and version of the bitmap index. Using the Oid type is to
	 * overcome the problem that the version is not stored in
	 * the index before GPDB 3.4.
	 */
	Oid         bm_magic;
	Oid         bm_version;         /* the version of the index */

	/*
	 * The relation ids for a heap and a btree on this heap. They are
	 * used to speed up finding the bitmap vector for given attribute
	 * value(s), see the comments for LOV pages below for more
	 * information. We consider these as the metadata for LOV pages.
	 */
	Oid			bm_lov_heapId;		/* the relation id for the heap */
	Oid			bm_lov_indexId;		/* the relation id for the index */

	/* the block number for the last LOV pages. */
	BlockNumber	bm_lov_lastpage;
} BMMetaPageData;

typedef BMMetaPageData *BMMetaPage;

/*
 * The meta page is always the first block of the index
 */

#define BM_METAPAGE 	0

/*
 * The maximum number of heap tuples in one page that is considered
 * in the bitmap index. We set this number to be a multiplication
 * of BM_HRL_WORD_SIZE because we can then bits for heap
 * tuples in different heap pages are stored in different words.
 * This makes it easier during the search.
 *
 * Because the tid value for AO tables can be more than MaxHeapTuplesPerPage,
 * we use the maximum possible offset number here.
 */
#define BM_MAX_TUPLES_PER_PAGE \
	(((((1 << (8 * sizeof(OffsetNumber))) - 1) / BM_HRL_WORD_SIZE) + 1) * \
	BM_HRL_WORD_SIZE)

/*
 * LOV (List Of Values) page -- pages to store a list of distinct
 * values for attribute(s) to be indexed, some metadata related to
 * their corresponding bitmap vectors, and the pointers to their
 * bitmap vectors. For each distinct value, there is a BMLOVItemData
 * associated with it. A LOV page maintains an array of BMLOVItemData
 * instances, called lov items.
 *
 * To speed up finding the lov item for a given value, we
 * create a heap to maintain all distinct values along with the
 * block numbers and offset numbers for their lov items in LOV pages.
 * That is, there are total "<number_of_attributes> + 2" attributes
 * in this new heap. Along with this heap, we also create a new btree
 * index on this heap using attribute(s) as btree keys. In this way,
 * for any given value, we search this btree to find
 * the block number and offset number for its corresponding lov item.
 */

/*
 * The first LOV page is reserved for NULL keys
 */
#define		BM_LOV_STARTPAGE	1 

/*
 * Items in a LOV page.
 *
 * Each item is corresponding to a distinct value for attribute(s)
 * to be indexed. For multi-column indexes on (a_1,a_2,...,a_n), we say
 * two values (l_1,l_2,...,l_n) and (k_1,k_2,...,k_n) for (a_1,a_2,...,a_n)
 * are the same if and only if for all i, l_i=k_i.
 * 
 */
typedef struct BMLOVItemData 
{
	/* the first page and last page of the bitmap vector. */
	BlockNumber		bm_lov_head;
	BlockNumber 	bm_lov_tail;

	/* 
	 * Additional information to be used to append new bits into
	 * existing bitmap vector that this distinct value is associated with. 
	 * The following two words do not store in the regular bitmap page,
	 * defined below. 
	 */

	/* the last complete word in its bitmap vector. */
	BM_HRL_WORD		bm_last_compword;

	/*
	 * the last word in its bitmap vector. This word is not
	 * a complete word. If a new appending bit makes this word
	 * to be complete, this word will merge with bm_last_compword.
	 */
	BM_HRL_WORD		bm_last_word;

	/*
	 * the tid location for the last bit stored in bm_last_compword.
	 * A tid location represents the index position for a bit in a
	 * bitmap vector, which is conceptualized as an array
	 * of bits. This value -- the index position starts from 1, and
     * is calculated through (block#)*BM_MAX_TUPLES_PER_PAGE + (offset#),
	 * where (block#) and (offset#) are from the heap tuple ctid.
	 * This value is used while updating a bit in the middle of
	 * its bitmap vector. When moving the last complete word to
	 * the bitmap page, this value will also be written to that page.
	 * Each bitmap page maintains a similar value -- the tid location
	 * for the last bit stored in that page. This will help us
	 * know the range of tid locations for bits in a bitmap page
	 * without decompressing all bits.
	 */
	uint64			bm_last_tid_location;

	/*
	 * the tid location of the last bit whose value is 1 (a set bit).
	 * Each bitmap vector will be visited only when there is a new
	 * set bit to be appended/updated. In the appending case, a new
	 * tid location is presented. With this value, we can calculate
	 * how many bits are 0s between this new set bit and the previous
	 * set bit.
	 */
	uint64			bm_last_setbit;

	/*
	 * Only two least-significant bits in this byte are used.
	 *
	 * If the first least-significant bit is 1, then it represents
	 * that bm_last_word is a fill word. If the second least-significant
	 * bit is 1, it represents that bm_last_compword is a fill word.
	 */
	uint8			lov_words_header;
	 
} BMLOVItemData;
typedef BMLOVItemData *BMLOVItem;

#define BM_MAX_LOVITEMS_PER_PAGE	\
	((BLCKSZ - sizeof(PageHeaderData)) / sizeof(BMLOVItemData))

#define BM_LAST_WORD_BIT 1
#define BM_LAST_COMPWORD_BIT 2

#define BM_LASTWORD_IS_FILL(lov) \
	(bool) (lov->lov_words_header & BM_LAST_WORD_BIT ? true : false)

#define BM_LAST_COMPWORD_IS_FILL(lov) \
	(bool) (lov->lov_words_header & BM_LAST_COMPWORD_BIT ? true : false)

#define BM_BOTH_LOV_WORDS_FILL(lov) \
	(BM_LASTWORD_IS_FILL(lov) && BM_LAST_COMPWORD_IS_FILL(lov))

/*
 * Bitmap page -- pages to store bits in a bitmap vector.
 *
 * Each bitmap page stores two parts of information: header words and
 * content words. Each bit in the header words is corresponding to
 * a word in the content words. If a bit in the header words is 1,
 * then its corresponding content word is a compressed word. Otherwise,
 * it is a literal word.
 *
 * If a content word is a fill word, it means that there is a sequence
 * of 0 bits or 1 bits. The most significant bit in this content word
 * represents the bits in this sequence are 0s or 1s. The rest of bits
 * stores the value of "the number of bits / BM_HRL_WORD_SIZE".
 */

/*
 * Opaque data for a bitmap page.
 */
typedef struct BMBitmapOpaqueData 
{
	uint32		bm_hrl_words_used;	/* the number of words used */
	BlockNumber	bm_bitmap_next;		/* the next page for this bitmap */

	/*
	 * the tid location for the last bit in this page.
     */
	uint64		bm_last_tid_location;
} BMBitmapOpaqueData;
typedef BMBitmapOpaqueData *BMBitmapOpaque;

/*
 * Approximately 4078 words per 8K page
 */
#define BM_MAX_NUM_OF_HRL_WORDS_PER_PAGE \
	((BLCKSZ - \
	MAXALIGN(sizeof(PageHeaderData)) - \
	MAXALIGN(sizeof(BMBitmapOpaqueData)))/sizeof(BM_HRL_WORD))

/* approx 255 */
#define BM_MAX_NUM_OF_HEADER_WORDS \
	(((BM_MAX_NUM_OF_HRL_WORDS_PER_PAGE-1)/BM_HRL_WORD_SIZE) + 1)

/*
 * To make the last header word a complete word, we limit this number to
 * the multiplication of the word size.
 */
#define BM_NUM_OF_HRL_WORDS_PER_PAGE \
	(((BM_MAX_NUM_OF_HRL_WORDS_PER_PAGE - \
	  BM_MAX_NUM_OF_HEADER_WORDS)/BM_HRL_WORD_SIZE) * BM_HRL_WORD_SIZE)

#define BM_NUM_OF_HEADER_WORDS \
	(((BM_NUM_OF_HRL_WORDS_PER_PAGE-1)/BM_HRL_WORD_SIZE) + 1)

/*
 * A page of a compressed bitmap
 */
typedef struct BMBitmapData
{
	BM_HRL_WORD hwords[BM_NUM_OF_HEADER_WORDS];
	BM_HRL_WORD cwords[BM_NUM_OF_HRL_WORDS_PER_PAGE];
} BMBitmapData;
typedef BMBitmapData *BMBitmap;

/*
 * Data structure for used to buffer index creation during bmbuild().
 * Buffering provides three benefits: firstly, it makes for many fewer
 * calls to the lower-level bitmap insert functions; secondly, it means that
 * we reduce the amount of unnecessary compression and decompression we do;
 * thirdly, in some cases pages for a given bitmap vector will be contiguous
 * on disk.
 *
 * byte_size counts how many bytes we've consumed in the buffer.
 * max_lov_block is a hint as to whether we'll find a LOV block in lov_blocks
 * or not (we take advantage of the fact that LOV block numbers will be
 * increasing).
 * lov_blocks is a list of LOV block buffers. The structures put in
 * this list are defined in bitmapinsert.c.
 */

typedef struct BMTidBuildBuf
{
	uint32 byte_size; /* The size in bytes of the buffer's data */
	BlockNumber max_lov_block; /* highest lov block we're seen */
	List *lov_blocks;	/* list of lov blocks we're buffering */
} BMTidBuildBuf;


/*
 * The number of tid locations to be found at once during query processing.
 */
#define BM_BATCH_TIDS  16*1024

/*
 * the maximum number of words to be retrieved during BitmapIndexScan.
 */
#define BM_MAX_WORDS BM_NUM_OF_HRL_WORDS_PER_PAGE*4

/* Some macros for manipulating a bitmap word. */
#define LITERAL_ALL_ZERO	0
#define LITERAL_ALL_ONE		((BM_HRL_WORD)(~((BM_HRL_WORD)0)))

#define FILL_MASK			~(((BM_HRL_WORD)1) << (BM_HRL_WORD_SIZE - 1))

#define BM_MAKE_FILL_WORD(bit, length) \
	((((BM_HRL_WORD)bit) << (BM_HRL_WORD_SIZE-1)) | (length))

#define FILL_LENGTH(w)        (((BM_HRL_WORD)(w)) & FILL_MASK)

#define MAX_FILL_LENGTH		((((BM_HRL_WORD)1)<<(BM_HRL_WORD_SIZE-1))-1)

/* get the left most bit of the word */
#define GET_FILL_BIT(w)		(((BM_HRL_WORD)(w))>>BM_HRL_WORD_LEFTMOST)

/*
 * Given a word number, determine the bit position it that holds in its
 * header word.
 */
#define WORDNO_GET_HEADER_BIT(cw_no) \
	((BM_HRL_WORD)1 << (BM_HRL_WORD_SIZE - 1 - ((cw_no) % BM_HRL_WORD_SIZE)))

/*
 * To see if the content word at n is a compressed word or not we must look
 * look in the header words h_words. Each bit in the header words corresponds
 * to a word amongst the content words. If the bit is 1, the word is compressed
 * (i.e., it is a fill word) otherwise it is uncompressed.
 *
 * See src/backend/access/bitmap/README for more details
 */

#define IS_FILL_WORD(h, n) \
	(bool) ((((h)[(n)/BM_HRL_WORD_SIZE]) & (WORDNO_GET_HEADER_BIT(n))) > 0 ? \
			true : false)

/* A simplified interface to IS_FILL_WORD */

#define CUR_WORD_IS_FILL(b) \
	IS_FILL_WORD(b->hwords, b->startNo)

/*
 * Calculate the number of header words we need given the number of
 * content words
 */
#define BM_CALC_H_WORDS(c_words) \
	(c_words == 0 ? c_words : (((c_words - 1)/BM_HRL_WORD_SIZE) + 1))

/*
 * Convert an ItemPointer to and from an integer representation
 */

#define BM_IPTR_TO_INT(iptr) \
	((uint64)ItemPointerGetBlockNumber(iptr) * BM_MAX_TUPLES_PER_PAGE + \
		(uint64)ItemPointerGetOffsetNumber(iptr))

#define BM_INT_GET_BLOCKNO(i) \
	((i - 1)/BM_MAX_TUPLES_PER_PAGE)

#define BM_INT_GET_OFFSET(i) \
	(((i - 1) % BM_MAX_TUPLES_PER_PAGE) + 1)


/*
 * BMTIDBuffer represents TIDs we've buffered for a given bitmap vector --
 * i.e., TIDs for a distinct value in the underlying table. We take advantage
 * of the fact that since we are reading the table from beginning to end
 * TIDs will be ordered.
 */

typedef struct BMTIDBuffer
{
	/* The last two bitmap words */
	BM_HRL_WORD last_compword;
	BM_HRL_WORD last_word;
	bool		is_last_compword_fill;

	uint64          start_tid;  /* starting TID for this buffer */
	uint64			last_tid;	/* most recent tid added */
	int16			curword; /* index into content */
	int16			num_cwords;	/* number of allocated words in content */

	/* the starting array index that contains useful content words */
	int16           start_wordno;

	/* the last tids, one for each actual data words */
	uint64         *last_tids;

	/* the header and content words */
	BM_HRL_WORD 	hwords[BM_NUM_OF_HEADER_WORDS];
	BM_HRL_WORD    *cwords;
} BMTIDBuffer;

typedef struct BMBuildLovData
{
	BlockNumber 	lov_block;
	OffsetNumber	lov_off;
} BMBuildLovData;


/*
 * the state for index build 
 */
typedef struct BMBuildState
{
	TupleDesc		bm_tupDesc;
	Relation		bm_lov_heap;
	Relation		bm_lov_index;
	/*
	 * We use this hash to cache lookups of lov blocks for different keys
	 * When one of attribute types can not be hashed, we set this hash
	 * to NULL.
	 */
	HTAB		   *lovitem_hash;

	/**
	 * when lovitem_hash is non-NULL then this will correspond to the
	 *   size of the keys in the hash
	 */
	int lovitem_hashKeySize;

	/*
	 * When the attributes to be indexed can not be hashed, we can not use
	 * the hash for the lov blocks. We have to search through the
	 * btree.
	 */
	ScanKey			bm_lov_scanKeys;
	IndexScanDesc	bm_lov_scanDesc;

	/*
	 * the buffer to store last several tid locations for each distinct
	 * value.
	 */
	BMTidBuildBuf	*bm_tidLocsBuffer;

	double 			ituples;	/* the number of index tuples */
	bool			use_wal;	/* whether or not we write WAL records */
} BMBuildState;

/**
 * The key used inside BMBuildState's lovitem_hash hashtable
 *
 * The caller should assign attributeValueArr and isNullArr to point at the values and isnull array to be hashed
 *
 * When inside the hashtable, attributeValueArr and isNullArr will point to aligned memory within the key block itself
 */
typedef struct BMBuildHashKey
{
    Datum *attributeValueArr;
    bool *isNullArr;
} BMBuildHashKey;

/*
 * Define an iteration result while scanning an BMBatchWords.
 *
 * This result includes the last scan position in an BMBatchWords,
 * and all tids that are generated from previous scan.
 */
typedef struct BMIterateResult
{
	uint64	nextTid; /* the first tid for the next iteration */
	uint32	lastScanPos; /* position in the bitmap word we're looking at */
	uint32	lastScanWordNo;	/* offset in BWBatchWords */
	uint64	nextTids[BM_BATCH_TIDS]; /* array of matching TIDs */
	uint32	numOfTids; /* number of TIDs matched */
	uint32	nextTidLoc; /* the next position in 'nextTids' to be read. */
} BMIterateResult;

/*
 * Stores a batch of consecutive bitmap words from a bitmap vector.
 *
 * These bitmap words come from a bitmap vector stored in this bitmap
 * index, or a bitmap vector that is generated by ANDing/ORing several
 * bitmap vectors.
 *
 * This struct also contains information to compute the tid locations
 * for the set bits in these bitmap words.
 */
typedef struct BMBatchWords
{
	uint32	maxNumOfWords;		/* maximum number of words in this list */

	/* Number of uncompressed words that have been read already */
	uint64	nwordsread;			
	uint64	nextread;			/* next word to read */
	uint64	firstTid;			/* the TID we're up to */
	uint32	startNo;			/* position we're at in cwords */
	uint32	nwords;				/* the number of bitmap words */
	BM_HRL_WORD *hwords; 		/* the header words */
	BM_HRL_WORD *cwords;		/* the actual bitmap words */	
} BMBatchWords;

/*
 * Scan opaque data for one bitmap vector.
 *
 * This structure stores a batch of consecutive bitmap words for a
 * bitmap vector that have been read from the disk, and remembers
 * the next reading position for the next batch of consecutive
 * bitmap words.
 */
typedef struct BMVectorData
{
	Buffer			bm_lovBuffer;/* the buffer that contains the LOV item. */
	OffsetNumber	bm_lovOffset;	/* the offset of the LOV item */
	BlockNumber		bm_nextBlockNo; /* the next bitmap page block */

	/* indicate if the last two words in the bitmap has been read. 
	 * These two words are stored inside a BMLovItem. If this value
	 * is true, it means this bitmap vector has no more words.
	 */
	bool			bm_readLastWords;
	BMBatchWords   *bm_batchWords; /* actual bitmap words */

} BMVectorData;
typedef BMVectorData *BMVector;

/*
 * Defines the current position of a scan.
 *
 * For each scan, all related bitmap vectors are read from the bitmap
 * index, and ORed together into a final bitmap vector. The words
 * in each bitmap vector are read in batches. This structure stores
 * the following:
 * (1) words for a final bitmap vector after ORing words from
 *     related bitmap vectors. 
 * (2) tid locations that satisfy the query.
 * (3) One BMVectorData for each related bitmap vector.
 */
typedef struct BMScanPositionData
{
	bool			done;	/* indicate if this scan is over */
	int				nvec;	/* the number of related bitmap vectors */
	/* the words in the final bitmap vector that satisfies the query. */
	BMBatchWords   *bm_batchWords;

	/*
	 * The BMIterateResult instance that contains the final 
	 * tid locations for tuples that satisfy the query.
	 */
	BMIterateResult bm_result;
	BMVector	posvecs;	/* one or more bitmap vectors */
} BMScanPositionData;

typedef BMScanPositionData *BMScanPosition;

typedef struct BMScanOpaqueData
{
	BMScanPosition		bm_currPos;
	bool				cur_pos_valid;
	/* XXX: should we pull out mark pos? */
	BMScanPosition		bm_markPos;
	bool				mark_pos_valid;
} BMScanOpaqueData;

typedef BMScanOpaqueData *BMScanOpaque;

/*
 * XLOG records for bitmap index operations
 *
 * Some information in high 4 bits of log record xl_info field.
 */
#define XLOG_BITMAP_INSERT_NEWLOV	0x10 /* add a new LOV page */
#define XLOG_BITMAP_INSERT_LOVITEM	0x20 /* add a new entry into a LOV page */
#define XLOG_BITMAP_INSERT_META		0x30 /* update the metapage */
#define XLOG_BITMAP_INSERT_BITMAP_LASTWORDS	0x40 /* update the last 2 words
													in a bitmap */
/* insert bitmap words into a bitmap page which is not the last one. */
#define XLOG_BITMAP_INSERT_WORDS		0x50
/* insert bitmap words to the last bitmap page and the lov buffer */
#define XLOG_BITMAP_INSERT_LASTWORDS	0x60
#define XLOG_BITMAP_UPDATEWORD			0x70
#define XLOG_BITMAP_UPDATEWORDS			0x80

/*
 * The information about writing bitmap words to last bitmap page
 * and lov page.
 */
typedef struct xl_bm_bitmapwords
{
	RelFileNode 	bm_node;
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

	/* The block number for the bitmap page */
	BlockNumber		bm_blkno;
	/* The next block number for this bitmap page */
	BlockNumber		bm_next_blkno;
	/* The last tid location for this bitmap page */
	uint64			bm_last_tid;
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

	/*
	 * Indicate if these bitmap words are stored in the last bitmap
	 * page and the lov buffer.
	 */
	bool			bm_is_last;

	/*
	 * Indicate if this is the first time to insert into a bitmap
	 * page.
	 */
	bool			bm_is_first;

	/*
	 * The words stored in the following array to be written to this
	 * bitmap page.
	 */
	uint64			bm_start_wordno;
	uint64			bm_words_written;

	/*
	 * Total number of new bitmap words. We need to log all new words
	 * to be able to do recovery.
	 */
	uint64			bm_num_cwords;

	/*
	 * The following are arrays of last tids, content words, and header
	 * words. They are located one after the other. There are bm_num_cwords
	 * of last tids and content words, and BM_CALC_H_WORDS(bm_num_cwords)
	 * header words.
	 */
} xl_bm_bitmapwords;

typedef struct xl_bm_updatewords
{
	RelFileNode		bm_node;
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

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
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

	BlockNumber		bm_blkno;
	int				bm_word_no;
	BM_HRL_WORD		bm_cword;
	BM_HRL_WORD		bm_hword;
} xl_bm_updateword;

/* The information about inserting a new lovitem into the LOV list. */
typedef struct xl_bm_lovitem
{
	RelFileNode 	bm_node;
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

	BlockNumber		bm_lov_blkno;
	OffsetNumber	bm_lov_offset;
	BMLOVItemData	bm_lovItem;
	bool			bm_is_new_lov_blkno;
} xl_bm_lovitem;

/* The information about adding a new page */
typedef struct xl_bm_newpage
{
	RelFileNode 	bm_node;
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

	BlockNumber		bm_new_blkno;
} xl_bm_newpage;

/*
 * The information about changes on a bitmap page. 
 * If bm_isOpaque is true, then bm_next_blkno is set.
 */
typedef struct xl_bm_bitmappage
{
	RelFileNode 	bm_node;
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

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
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

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
	ItemPointerData bm_persistentTid;
	int64 			bm_persistentSerialNum;

	Oid				bm_lov_heapId;		/* the relation id for the heap */
	Oid				bm_lov_indexId;		/* the relation id for the index */
	/* the block number for the last LOV pages. */
	BlockNumber		bm_lov_lastpage;
} xl_bm_metapage;

/* public routines */
extern Datum bmbuild(PG_FUNCTION_ARGS);
extern Datum bminsert(PG_FUNCTION_ARGS);
extern Datum bmbeginscan(PG_FUNCTION_ARGS);
extern Datum bmgettuple(PG_FUNCTION_ARGS);
extern Datum bmgetbitmap(PG_FUNCTION_ARGS);
extern Datum bmrescan(PG_FUNCTION_ARGS);
extern Datum bmendscan(PG_FUNCTION_ARGS);
extern Datum bmmarkpos(PG_FUNCTION_ARGS);
extern Datum bmrestrpos(PG_FUNCTION_ARGS);
extern Datum bmbulkdelete(PG_FUNCTION_ARGS);
extern Datum bmvacuumcleanup(PG_FUNCTION_ARGS);
extern Datum bmoptions(PG_FUNCTION_ARGS);

extern void GetBitmapIndexAuxOids(Relation index, Oid *heapId, Oid *indexId);

/* bitmappages.c */
extern Buffer _bitmap_getbuf(Relation rel, BlockNumber blkno, int access);
extern void _bitmap_wrtbuf(Buffer buf);
extern void _bitmap_relbuf(Buffer buf);
extern void _bitmap_init_lovpage(Relation rel, Buffer buf);
extern void _bitmap_init_bitmappage(Relation rel, Buffer buf);
extern void _bitmap_init_buildstate(Relation index, BMBuildState* bmstate);
extern void _bitmap_cleanup_buildstate(Relation index, BMBuildState* bmstate);
extern void _bitmap_init(Relation indexrel, bool use_wal);

/* bitmapinsert.c */
extern void _bitmap_buildinsert(Relation rel, ItemPointerData ht_ctid, 
								Datum *attdata, bool *nulls,
							 	BMBuildState *state);
extern void _bitmap_doinsert(Relation rel, ItemPointerData ht_ctid, 
							 Datum *attdata, bool *nulls);
extern void _bitmap_write_alltids(Relation rel, BMTidBuildBuf *tids,
						  		  bool use_wal);
extern uint64 _bitmap_write_bitmapwords(Buffer bitmapBuffer,
								BMTIDBuffer* buf);
extern void _bitmap_write_new_bitmapwords(
	Relation rel,
	Buffer lovBuffer, OffsetNumber lovOffset,
	BMTIDBuffer* buf, bool use_wal);
extern uint16 _bitmap_free_tidbuf(BMTIDBuffer* buf);

/* bitmaputil.c */
extern BMLOVItem _bitmap_formitem(uint64 currTidNumber);
extern BMMetaPage _bitmap_get_metapage_data(Relation rel, Buffer metabuf);
extern void _bitmap_init_batchwords(BMBatchWords* words,
									uint32	maxNumOfWords,
									MemoryContext mcxt);
extern void _bitmap_copy_batchwords(BMBatchWords *words, BMBatchWords *copyWords);
extern void _bitmap_reset_batchwords(BMBatchWords* words);
extern void _bitmap_cleanup_batchwords(BMBatchWords* words);
extern void _bitmap_cleanup_scanpos(BMVector bmScanPos,
									uint32 numBitmapVectors);
extern uint64 _bitmap_findnexttid(BMBatchWords *words,
								  BMIterateResult *result);
extern void _bitmap_findnexttids(BMBatchWords *words,
								 BMIterateResult *result, uint32 maxTids);
#ifdef NOT_USED /* we might use this later */
extern void _bitmap_intersect(BMBatchWords **batches, uint32 numBatches,
						   BMBatchWords *result);
#endif
extern void _bitmap_union(BMBatchWords **batches, uint32 numBatches,
					   BMBatchWords *result);
extern void _bitmap_begin_iterate(BMBatchWords *words, BMIterateResult *result);
extern void _bitmap_log_newpage(Relation rel, uint8 info, Buffer buf);
extern void _bitmap_log_metapage(Relation rel, Page page);
extern void _bitmap_log_bitmap_lastwords(Relation rel, Buffer lovBuffer,
									 OffsetNumber lovOffset, BMLOVItem lovItem);
extern void _bitmap_log_lovitem	(Relation rel, Buffer lovBuffer,
								 OffsetNumber offset, BMLOVItem lovItem,
								 Buffer metabuf,  bool is_new_lov_blkno);
extern void _bitmap_log_bitmapwords(Relation rel, Buffer bitmapBuffer, Buffer lovBuffer,
						OffsetNumber lovOffset, BMTIDBuffer* buf,
						uint64 words_written, uint64 tidnum, BlockNumber nextBlkno,
						bool isLast, bool isFirst);
extern void _bitmap_log_updatewords(Relation rel,
						Buffer lovBuffer, OffsetNumber lovOffset,
						Buffer firstBuffer, Buffer secondBuffer,
						bool new_lastpage);
extern void _bitmap_log_updateword(Relation rel, Buffer bitmapBuffer, int word_no);

/* bitmapsearch.c */
extern bool _bitmap_first(IndexScanDesc scan, ScanDirection dir);
extern bool _bitmap_next(IndexScanDesc scan, ScanDirection dir);
extern bool _bitmap_firstbatchwords(IndexScanDesc scan, ScanDirection dir);
extern bool _bitmap_nextbatchwords(IndexScanDesc scan, ScanDirection dir);
extern void _bitmap_findbitmaps(IndexScanDesc scan, ScanDirection dir);



/* bitmapattutil.c */
extern void _bitmap_create_lov_heapandindex(Relation rel,
											Oid *lovHeapOid,
											Oid *lovIndexOid);
extern void _bitmap_open_lov_heapandindex(Relation rel, BMMetaPage metapage,
						 Relation *lovHeapP, Relation *lovIndexP,
						 LOCKMODE lockMode);
extern void _bitmap_insert_lov(Relation lovHeap, Relation lovIndex,
							   Datum *datum, bool *nulls, bool use_wal);
extern void _bitmap_close_lov_heapandindex(Relation lovHeap, 
										Relation lovIndex, LOCKMODE lockMode);
extern bool _bitmap_findvalue(Relation lovHeap, Relation lovIndex,
							 ScanKey scanKey, IndexScanDesc scanDesc,
							 BlockNumber *lovBlock, bool *blockNull,
							 OffsetNumber *lovOffset, bool *offsetNull);

/*
 * prototypes for functions in bitmapxlog.c
 */
extern void bitmap_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record);
extern void bitmap_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);
extern void bitmap_xlog_startup(void);
extern void bitmap_xlog_cleanup(void);
extern bool bitmap_safe_restartpoint(void);

/* reloptions.c */
#define BITMAP_MIN_FILLFACTOR		10
#define BITMAP_DEFAULT_FILLFACTOR	100

#endif
