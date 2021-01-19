/*-------------------------------------------------------------------------
 *
 * bitmap.h
 *	header file for on-disk bitmap index access method implementation.
 *
 * Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	src/include/access/bitmap.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BITMAP_H
#define BITMAP_H

#include "storage/bufmgr.h"
#include "storage/bufpage.h"

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
 * To see if the content word at wordno is a compressed word or not we must look
 * in the header words. Each bit in the header words corresponds to a word
 * amongst the content words. If the bit is 1, the word is compressed (i.e., it
 * is a fill word) otherwise it is uncompressed.
 *
 * See src/backend/access/bitmap/README for more details
 */
static inline bool
IS_FILL_WORD(const BM_HRL_WORD *words, int16 wordno)
{
	return (words[wordno / BM_HRL_WORD_SIZE] & WORDNO_GET_HEADER_BIT(wordno)) > 0;
}

/*
 * GET_NUM_BITS() -- return the number of bits included in the given
 * bitmap words.
 */
static inline uint64
GET_NUM_BITS(const BM_HRL_WORD *contentWords,
			 const BM_HRL_WORD *headerWords,
			 uint32 nwords)
{
	uint64	nbits = 0;
	uint32	i;

	for (i = 0; i < nwords; i++)
	{
		if (IS_FILL_WORD(headerWords, i))
			nbits += FILL_LENGTH(contentWords[i]) * BM_HRL_WORD_SIZE;
		else
			nbits += BM_HRL_WORD_SIZE;
	}

	return nbits;
}

/* reloptions.c */
#define BITMAP_MIN_FILLFACTOR		10
#define BITMAP_DEFAULT_FILLFACTOR	100

#endif
