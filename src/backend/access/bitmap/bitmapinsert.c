/*-------------------------------------------------------------------------
 *
 * bitmapinsert.c
 *	  Tuple insertion in the on-disk bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmapinsert.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/tupdesc.h"
#include "access/heapam.h"
#include "access/bitmap.h"
#include "access/transam.h"
#include "parser/parse_oper.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"

/*
 * The following structure along with BMTIDBuffer are used to buffer
 * words for tids * during index create -- bmbuild().
 */

/*
 * BMTIDLOVBuffer represents those bitmap vectors whose LOV item would be
 * stored on the specified lov_block. The array bufs stores the TIDs for
 * a distinct vector (see above). The index of the array we're upto tell
 * us the offset number of the LOV item on the lov_block.
 */

typedef struct BMTIDLOVBuffer
{
	BlockNumber lov_block;
	BMTIDBuffer *bufs[BM_MAX_LOVITEMS_PER_PAGE];
} BMTIDLOVBuffer;

static void _bitmap_write_new_bitmapwords(Relation rel,
							  Buffer lovBuffer, OffsetNumber lovOffset,
							  BMTIDBuffer* buf, bool use_wal);
static uint64 _bitmap_write_bitmapwords_on_page(Page bitmapPage,
								  BMTIDBuffer *buf, int startWordNo,
								  xl_bm_bitmapwords_perpage *xlrec_perpage);
static void create_lovitem(Relation rel, Buffer metabuf, uint64 tidnum, 
						   TupleDesc tupDesc, 
						   Datum *attdata, bool *nulls,
						   Relation lovHeap, Relation lovIndex,
						   BlockNumber *lovBlockP, 
						   OffsetNumber *lovOffsetP, bool use_wal);
static void build_inserttuple(Relation rel, uint64 tidnum,
							   ItemPointerData ht_ctid, TupleDesc tupDesc, 
							   Datum *attdata, bool *nulls, BMBuildState *state);
static void inserttuple(Relation rel, Buffer metabuf, 
								uint64 tidnum, ItemPointerData ht_ctid, 
							    TupleDesc tupDesc, Datum* attdata,
							    bool *nulls, Relation lovHeap, 
								Relation lovIndex, ScanKey scanKey, 
								IndexScanDesc scanDesc, bool use_wal);
static void updatesetbit(Relation rel, 
						 Buffer lovBuffer, OffsetNumber lovOffset,
						 uint64 tidnum, bool use_wal);
static void updatesetbit_inword(BM_HRL_WORD word, uint64 updateBitLoc,
								uint64 firstTid, BMTIDBuffer* buf);
static void updatesetbit_inpage(Relation rel, uint64 tidnum,
								Buffer lovBuffer, OffsetNumber lovOffset,
								Buffer bitmapBuffer, uint64 firstTidNumber,
								bool use_wal);
static void insertsetbit(Relation rel, BlockNumber lovBlock, OffsetNumber lovOffset,
			 			 uint64 tidnum, BMTIDBuffer *buf, bool use_wal);
static uint64 getnumbits(BM_HRL_WORD *contentWords, 
					     BM_HRL_WORD *headerWords, uint32 nwords);
static void findbitmappage(Relation rel, BMLOVItem lovitem,
					   uint64 tidnum,
					   Buffer *bitmapBufferP, uint64 *firstTidNumberP);
static void shift_header_bits(BM_HRL_WORD *words, uint32 numOfBits,
						  uint32 maxNumOfWords, uint32 startLoc,
						  uint32 numOfShiftingBits);
static void rshift_header_bits(BM_HRL_WORD *words, uint64 nwords,
							   uint32 bits);
static void lshift_header_bits(BM_HRL_WORD *words, uint64 nwords,
							   uint32 bits);
static void insert_newwords(BMTIDBuffer* words, uint32 insertPos,
							BMTIDBuffer* new_words, BMTIDBuffer* words_left);
static int16 mergewords(BMTIDBuffer* buf, bool lastWordFill);
static void buf_make_space(Relation rel,
					  BMTidBuildBuf *tidLocsBuffer, bool use_wal);
static void verify_bitmappages(Relation rel, BMLOVItem lovitem);
static int16 buf_add_tid_with_fill(Relation rel, BMTIDBuffer *buf,
								   Buffer lovBuffer, OffsetNumber off,
								   uint64 tidnum, bool use_wal);
static uint16 buf_extend(BMTIDBuffer *buf);
static uint16 buf_ensure_head_space(Relation rel, BMTIDBuffer *buf,
								   Buffer lovBuffer, OffsetNumber off,
								   bool use_wal);
static uint16 buf_free_mem_block(Relation rel, BMTIDBuffer *buf,
			  			         Buffer lovBuffer, OffsetNumber off,
						         bool use_wal);
static uint16 buf_free_mem(Relation rel, BMTIDBuffer *buf,
			  			   BlockNumber lov_block, OffsetNumber off, 
						   bool use_wal);
static uint16 _bitmap_free_tidbuf(BMTIDBuffer* buf);

#define BUF_INIT_WORDS 8 /* as good a point as any */


/*
 * getnumbits() -- return the number of bits included in the given
 * 	bitmap words.
 */
static uint64
getnumbits(BM_HRL_WORD *contentWords, BM_HRL_WORD *headerWords, uint32 nwords)
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

/*
 * updatesetbit() -- update a set bit in a bitmap.
 *
 * This function finds the bit in a given bitmap vector whose bit location is
 * equal to tidnum, and changes this bit to 1.
 *
 * If this bit is already 1, then we are done. Otherwise, there are
 * two possibilities:
 * (1) This bit appears in a literal word. In this case, we simply change
 *     it to 1.
 * (2) This bit appears in a fill word with bit 0. In this case, this word
 *     may generate two or three words after changing the corresponding bit
 *     to 1, depending on the position of this bit.
 *
 * Case (2) will make the corresponding bitmap page to grow. The words after
 * this affected word in this bitmap page are shifted right to accommodate
 * the newly generated words. If this bitmap page does not have enough space
 * to hold all these words, the last few words will be shifted out of this
 * page. In this case, the next bitmap page is checked to see if there are
 * enough space for these extra words. If so, these extra words are inserted
 * into the next page. Otherwise, we create a new bitmap page to hold
 * these extra words.
 */
static void
updatesetbit(Relation rel, Buffer lovBuffer, OffsetNumber lovOffset,
			 uint64 tidnum, bool use_wal)
{
	Page		lovPage;
	BMLOVItem	lovItem;
		
	uint64	tidLocation;
	uint16	insertingPos;

	uint64	firstTidNumber = 1;
	Buffer	bitmapBuffer = InvalidBuffer;

	lovPage = BufferGetPage(lovBuffer);
	lovItem = (BMLOVItem) PageGetItem(lovPage, 
		PageGetItemId(lovPage, lovOffset));

	/* Calculate the tid location in the last bitmap page. */
	tidLocation = lovItem->bm_last_tid_location;
	if (BM_LAST_COMPWORD_IS_FILL(lovItem))
		tidLocation -= (FILL_LENGTH(lovItem->bm_last_compword) *
					    BM_HRL_WORD_SIZE);
	else
		tidLocation -= BM_HRL_WORD_SIZE;

	/*
	 * If tidnum is in either bm_last_compword or bm_last_word,
	 * and this does not generate any new words, we simply
	 * need to update the lov item.
	 */
	if ((tidnum > lovItem->bm_last_tid_location) ||
		((tidnum > tidLocation) &&
		 ((lovItem->lov_words_header == 0) ||
		  (FILL_LENGTH(lovItem->bm_last_compword) == 1))))
	{
		START_CRIT_SECTION();

		MarkBufferDirty(lovBuffer);

		if (tidnum > lovItem->bm_last_tid_location)   /* bm_last_word */
		{
			insertingPos = (tidnum-1)%BM_HRL_WORD_SIZE;
			lovItem->bm_last_word |= (((BM_HRL_WORD)1)<<insertingPos);
			
			if (Debug_bitmap_print_insert)
				elog(LOG, "Bitmap Insert: updated a set bit in lovItem->bm_last_word"
					 " pos %d"
					 ", lovBlock=%d, lovOffset=%d"
					 ", tidnum=" INT64_FORMAT,
					 insertingPos,
					 BufferGetBlockNumber(lovBuffer),
					 lovOffset,
					 tidnum);
		}
		else /* bm_last_compword */
		{
			if (BM_LAST_COMPWORD_IS_FILL(lovItem))
			{
				if (GET_FILL_BIT(lovItem->bm_last_compword) == 1)
					lovItem->bm_last_compword = LITERAL_ALL_ONE;
				else
					lovItem->bm_last_compword = 0;
			}

			insertingPos = (tidnum - 1) % BM_HRL_WORD_SIZE;
			lovItem->bm_last_compword |= (((BM_HRL_WORD)1) << insertingPos);
			if (lovItem->bm_last_compword == LITERAL_ALL_ONE)
			{
				lovItem->lov_words_header = 2;
				lovItem->bm_last_compword = BM_MAKE_FILL_WORD(1, 1);
			}
			else
				lovItem->lov_words_header = 0;

			if (Debug_bitmap_print_insert)
				elog(LOG, "Bitmap Insert: updated a set bit in lovItem->bm_last_compword"
					 " pos %d"
					 ", lovBlock=%d, lovOffset=%d"
					 ", tidnum=" INT64_FORMAT,
					 insertingPos,
					 BufferGetBlockNumber(lovBuffer),
					 lovOffset,
					 tidnum);
		}

		if (use_wal)
			_bitmap_log_bitmap_lastwords(rel, lovBuffer, lovOffset, lovItem);

		END_CRIT_SECTION();

		return;
	}

	/*
	 * Here, if tidnum is still in bm_last_compword, we know that
	 * bm_last_compword is a fill zero words with fill length greater
	 * than 1. This update will generate new words, we insert new words
	 * into the last bitmap page and update the lov item.
	 */
	if ((tidnum > tidLocation) && (lovItem->lov_words_header >= 2))
	{
		/*
		 * We know that bm_last_compwords will be split into two
		 * or three words, depending on the splitting position.
		 */
		BMTIDBuffer buf;
		MemSet(&buf, 0, sizeof(buf));
		buf_extend(&buf);

		updatesetbit_inword(lovItem->bm_last_compword,
							tidnum - tidLocation - 1,
							tidLocation + 1, &buf);

		/* set the last_compword and last_word */
		buf.last_compword = buf.cwords[buf.curword-1];
		buf.is_last_compword_fill = IS_FILL_WORD(buf.hwords, buf.curword-1);
		buf.curword--;
		buf.last_word = lovItem->bm_last_word;
		buf.last_tid = lovItem->bm_last_setbit;
		_bitmap_write_new_bitmapwords(rel, lovBuffer, lovOffset,
									  &buf, use_wal);

		if (Debug_bitmap_print_insert)
			verify_bitmappages(rel, lovItem);
		
		_bitmap_free_tidbuf(&buf);

		return;
	}

	/*
	 * Now, tidnum is in the middle of the bitmap vector.
	 * We try to find the bitmap page that contains this bit,
	 * and update the bit.
	 */
	/* find the page that contains this bit. */
	findbitmappage(rel, lovItem, tidnum,
				   &bitmapBuffer, &firstTidNumber);

	updatesetbit_inpage(rel, tidnum, lovBuffer, lovOffset,
						bitmapBuffer, firstTidNumber, use_wal);

	_bitmap_relbuf(bitmapBuffer);

	if (Debug_bitmap_print_insert)
		verify_bitmappages(rel, lovItem);
}

/*
 * updatesetbit_inword() -- update the given bit to 1 in a given
 * 	word.
 *
 * The given word will generate at most three new words, depending on 
 * the position of the given bit to be updated. Make sure that the 
 * array 'words' has the size of 3 when you call this function. All new 
 * words will be put in this array, and the final number of new words is 
 * stored in '*numWordsP'. The bit location 'updateBitLoc' is relative to
 * the beginning of the given word, starting from 0.
 *
 * We assume that word is a fill zero word.
 */
static void
updatesetbit_inword(BM_HRL_WORD word, uint64 updateBitLoc,
					uint64 firstTid, BMTIDBuffer *buf)
{
	uint64	numBits, usedNumBits;
	uint16	insertingPos;

	Assert(updateBitLoc < BM_HRL_WORD_SIZE*FILL_LENGTH(word));

	numBits = FILL_LENGTH(word) * BM_HRL_WORD_SIZE;
	usedNumBits = 0;
	if (updateBitLoc >= BM_HRL_WORD_SIZE)
	{
		firstTid += (updateBitLoc/BM_HRL_WORD_SIZE) * BM_HRL_WORD_SIZE;
		buf->cwords[buf->curword] =
			BM_MAKE_FILL_WORD(0, updateBitLoc/BM_HRL_WORD_SIZE);
		buf->last_tids[buf->curword] = firstTid - 1;
		buf->curword++;
		buf_extend(buf);
		buf->hwords[(buf->curword-1)/BM_HRL_WORD_SIZE] |=
			(((BM_HRL_WORD)1)<<(BM_HRL_WORD_SIZE - buf->curword));
		usedNumBits += (updateBitLoc/BM_HRL_WORD_SIZE) * BM_HRL_WORD_SIZE;
	}

	/* construct the literal word */
	insertingPos = updateBitLoc - usedNumBits;
	firstTid += BM_HRL_WORD_SIZE;
	buf->cwords[buf->curword] =
		((BM_HRL_WORD)0) | (((BM_HRL_WORD)1) << insertingPos);
	buf->last_tids[buf->curword] = firstTid - 1;
	buf->curword++;
	buf_extend(buf);
	usedNumBits += BM_HRL_WORD_SIZE;

	if (numBits > usedNumBits)
	{
		BM_HRL_WORD fill_length;
		Assert((numBits - usedNumBits) % BM_HRL_WORD_SIZE == 0);
		fill_length = (numBits - usedNumBits) / BM_HRL_WORD_SIZE;

		firstTid += fill_length * BM_HRL_WORD_SIZE;
		buf->cwords[buf->curword] = BM_MAKE_FILL_WORD(0, fill_length);
		buf->last_tids[buf->curword] = firstTid -1;
		buf->curword++;
		buf_extend(buf);
		buf->hwords[(buf->curword-1)/BM_HRL_WORD_SIZE] |=
			(((BM_HRL_WORD)1) << (BM_HRL_WORD_SIZE - buf->curword));
	}
}

/*
 * rshift_header_bits() -- 'in-place' right-shift bits in given words
 * 	'bits' bits.
 *
 * Assume that 'bits' is smaller than BM_HRL_WORD_SIZE. The right-most
 * 'bits' bits will be ignored.
 */
static void
rshift_header_bits(BM_HRL_WORD* words, uint64 nwords,
				   uint32 bits)
{
	BM_HRL_WORD shifting_bits = 0;
	uint32 word_no;

	Assert(bits < BM_HRL_WORD_SIZE);

	for (word_no = 0; word_no < nwords; word_no++)
	{
		BM_HRL_WORD new_shifting_bits = 
			((BM_HRL_WORD)words[word_no]) << (BM_HRL_WORD_SIZE - bits);
		words[word_no] = (words[word_no] >> bits) | shifting_bits;

		shifting_bits = new_shifting_bits;
	}
}

/*
 * lshift_header_bits() -- 'in-place' left-shift bits in given words
 * 	'bits' bits.
 *
 * Assume that 'bits' is smaller than BM_HRL_WORD_SIZE. The left-most
 * 'bits' bits will be ignored.
 */
static void
lshift_header_bits(BM_HRL_WORD* words, uint64 nwords,
				   uint32 bits)
{
	uint32 word_no;
	Assert(bits < BM_HRL_WORD_SIZE);

	for (word_no = 0; word_no < nwords; word_no++)
	{
		BM_HRL_WORD shifting_bits = 
			words[word_no] >> (BM_HRL_WORD_SIZE - bits);
		words[word_no] = ((BM_HRL_WORD)words[word_no]) << bits;

		if (word_no != 0)
			words[word_no - 1] |= shifting_bits;
	}
}

/*
 * shift_header_bits() -- right-shift bits after 'startLoc' for
 * 	'numofShiftingBits' bits.
 *
 * These bits are stored in an array of words with the word size of
 * BM_HRL_WORD_SIZE. This shift is done in-place. The maximum number of
 * words in this array is given. If the shifting causes the array not to
 * have enough space for all bits, the right-most overflow bits will be
 * discarded. The value 'startLoc' starts with 0.
 */
static void
shift_header_bits(BM_HRL_WORD* words, uint32 numOfBits,
						  uint32 maxNumOfWords, uint32 startLoc,
						  uint32 numOfShiftingBits)
{
	uint32	startWordNo;
	uint32	endWordNo;
	uint32	wordNo;
	uint32	numOfFinalShiftingBits;
	BM_HRL_WORD tmpWord;

	Assert(startLoc <= numOfBits);
	Assert((numOfBits-1)/BM_HRL_WORD_SIZE < maxNumOfWords);

	startWordNo = startLoc/BM_HRL_WORD_SIZE;
	endWordNo = (numOfBits-1)/BM_HRL_WORD_SIZE;

	for (wordNo = endWordNo; wordNo > startWordNo; wordNo--)
	{
		/*
		 * obtain the last 'numOfShiftingBits' bits in the words[wordNo],
		 * and store them in the high-end of a word.
		 */
		tmpWord = (((BM_HRL_WORD)words[wordNo])<<
					(BM_HRL_WORD_SIZE-numOfShiftingBits));

		/* right-shift the original word 'numOfShiftingBits' bits. */
		words[wordNo] =	(((BM_HRL_WORD)words[wordNo])>>numOfShiftingBits);

		/* OR those shifted bits into the next word in the array. */
		if (wordNo < maxNumOfWords-1)
			words[wordNo + 1] |= tmpWord;
		
	}

	/* obtain bits after 'startLoc'.*/
	tmpWord = ((BM_HRL_WORD)(words[startWordNo]<<
				(startLoc%BM_HRL_WORD_SIZE)))>>(startLoc%BM_HRL_WORD_SIZE);

	/*
	 * When startLoc%BM_HRL_WORD_SIZE is 0, we want to shift all 64 bits.
	 * There is no way to use the bit-shifting to shift all 64 bits out
	 * of a 64-bit integer. So just simply set the word to 0.
	 * Otherwise, use bit-shifting to shift out the bits after 'startLoc'.
	 */
	if (startLoc%BM_HRL_WORD_SIZE > 0)
	{
		words[startWordNo] = ((BM_HRL_WORD)(words[startWordNo]>>
											(BM_HRL_WORD_SIZE-startLoc%BM_HRL_WORD_SIZE)))<<
			(BM_HRL_WORD_SIZE-startLoc%BM_HRL_WORD_SIZE);
	}
	else
	{
		words[startWordNo] = 0;
	}

	numOfFinalShiftingBits = numOfShiftingBits;
	if (BM_HRL_WORD_SIZE - startLoc % BM_HRL_WORD_SIZE < numOfShiftingBits)
		numOfFinalShiftingBits = BM_HRL_WORD_SIZE - startLoc % BM_HRL_WORD_SIZE;

	words[startWordNo] |= (tmpWord>>numOfFinalShiftingBits);

	if (startWordNo < maxNumOfWords-1)
	{
		tmpWord = ((BM_HRL_WORD)(tmpWord << (BM_HRL_WORD_SIZE - numOfFinalShiftingBits)))>>
			(numOfShiftingBits - numOfFinalShiftingBits);
		words[startWordNo+1] |= tmpWord;
	}
}

/*
 * insert_newwords() -- insert a buffer of new words into a given buffer of
 *   words at a specified position.
 *
 * The new words will be inserted into the positions starting from
 * 'insertPos'(>=0). The original words from 'insertPos' will be shifted
 * to the right. If the given array does not have enough space to
 * hold all words, the last '(*numWordsP+numNewWords-maxNumWords)' words
 * will be stored in the buffer 'words_left', for which the caller should set
 * the enough space to hold these left words.
 *
 * All three buffers are specified as BMTIDBuffer objects, in which the following
 * fields are used:
 *   curword -- the number of content words in this buffer.
 *   num_cwords -- the maximum number of content words that are allowed.
 *   hwords -- the header words
 *   cwords -- the content words
 *
 * This function assumes that the number of new words is not greater than BM_HRL_WORD_SIZE.
 */
static void
insert_newwords(BMTIDBuffer* words, uint32 insertPos,
				BMTIDBuffer* new_words, BMTIDBuffer* words_left)
{
	int32	wordNo;
	uint16  bitLoc;

	Assert(new_words->curword <= BM_HRL_WORD_SIZE);
	Assert(insertPos <= words->num_cwords);

	words_left->curword = 0;

	/* if there are no words in the original buffer, we simply copy the new words. */
	if (words->curword == 0)
	{
		memcpy(words->cwords, new_words->cwords, new_words->curword*sizeof(BM_HRL_WORD));
		memcpy(words->hwords, new_words->hwords,
			   BM_CALC_H_WORDS(new_words->curword) * sizeof(BM_HRL_WORD));
		words->curword = new_words->curword;

		return;
	}

	/*
	 * if insertPos is pointing to the position after the maximum position
	 * in this word, we simply copy the new words to leftContentWords.
	 */
	if (insertPos == words->num_cwords)
	{
		memcpy(words_left->cwords, new_words->cwords,
			   new_words->curword * sizeof(BM_HRL_WORD));
		memcpy(words_left->hwords, new_words->hwords,
			   BM_CALC_H_WORDS(new_words->curword) * sizeof(BM_HRL_WORD));
		words_left->curword = new_words->curword;

		return;
	}

	Assert(words->curword > 0);

	/* Calculate how many words left after this insert. */
	if (words->curword + new_words->curword > words->num_cwords)
		words_left->curword = words->curword + new_words->curword - words->num_cwords;
	MemSet(words_left->hwords, 0, BM_NUM_OF_HEADER_WORDS * sizeof(BM_HRL_WORD));

	/*
	 * Walk from the last word in the array back to 'insertPos'.
	 * If the word no + new_words->curword is greater than words->num_cwords,
	 * we store these words in words_left.
	 */
	for (wordNo=words->curword-1; wordNo>=0 && wordNo>=insertPos; wordNo--)
	{
		if (wordNo + new_words->curword >= words->num_cwords)
		{
			words_left->cwords[wordNo+new_words->curword-words->num_cwords] = 
				words->cwords[wordNo];
			if (IS_FILL_WORD(words->hwords, wordNo))
			{
				uint32		o = (int)wordNo/BM_HRL_WORD_SIZE;
				uint32		n = wordNo + new_words->curword - words->num_cwords;

				words_left->hwords[0] |= WORDNO_GET_HEADER_BIT(n);
				words->hwords[o] &= ~(WORDNO_GET_HEADER_BIT(wordNo));
			}
		}
		else
			words->cwords[wordNo + new_words->curword] = words->cwords[wordNo];
	}

	/* insert new words */
	for (wordNo=0; wordNo<new_words->curword; wordNo++)
	{
		if (insertPos+wordNo>= words->num_cwords)
		{
			uint32 	n = insertPos + wordNo - words->num_cwords;

			words_left->cwords[n] = new_words->cwords[wordNo];
			if (IS_FILL_WORD(new_words->hwords, wordNo))
				words_left->hwords[0] |= WORDNO_GET_HEADER_BIT(n);
		}
		else
			words->cwords[insertPos+wordNo] = new_words->cwords[wordNo];
	}

	/* right-shift the bits in the header words */
	shift_header_bits(words->hwords, words->curword, 
					  BM_NUM_OF_HEADER_WORDS, insertPos,
					  new_words->curword);

	/* set the newWords header bits */
	for (bitLoc = insertPos;
		 bitLoc < insertPos + new_words->curword && bitLoc < words->num_cwords;
		 bitLoc++)
	{
		if (IS_FILL_WORD(new_words->hwords, bitLoc-insertPos))
		{
			uint32		off = (uint32)bitLoc/BM_HRL_WORD_SIZE;

			words->hwords[off] |= WORDNO_GET_HEADER_BIT(bitLoc);
		}
	}

	words->curword += (new_words->curword - words_left->curword);	
}

/*
 * updatesetbit_inpage() -- update the given bit to 1 in a given
 *	bitmap page.
 *
 * The argument 'firstTidNumber' indicates the first tid location of
 * the bits stored in this page. This is necessary for locating the bit
 * of 'tidnum'.
 *
 * This update may generate new words that cause this page to overflow.
 * In this case, we will first check the next bitmap page have enough
 * space for these new words. If so, we update these two pages. Otherwise,
 * a new bitmap page is created.
 */
static void
updatesetbit_inpage(Relation rel, uint64 tidnum,
					Buffer lovBuffer, OffsetNumber lovOffset,
					Buffer bitmapBuffer, uint64 firstTidNumber,
					bool use_wal)
{
	Page 			bitmapPage;
	BMBitmapOpaque	bitmapOpaque;
	BMBitmap		bitmap;
	Buffer			nextBuffer;
	Page			nextPage;
	BMBitmapOpaque	nextOpaque;
	BMBitmap		nextBitmap;

	uint64			bitNo = 0;
	uint32			wordNo;
	uint32          free_words;
	BM_HRL_WORD		word = 0;
	bool			found = false;

	BMTIDBuffer     words;
	BMTIDBuffer     new_words;
	BMTIDBuffer     words_left;

	bool			new_page;
	bool			new_lastpage;
	int             word_no;

	bitmapPage = BufferGetPage(bitmapBuffer);
	bitmapOpaque = (BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);

	bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);
	bitNo = 0;

 	if (Debug_bitmap_print_insert)
 		elog(LOG, "Bitmap Insert: updating a set bit in bitmap page %d, "
 			 "lovBlock=%d, lovOffset=%d"
 			 ", firstTidNumber=" INT64_FORMAT
 			 ", bm_last_tid_location=" INT64_FORMAT
 			 ", tidnum=" INT64_FORMAT,
 			 BufferGetBlockNumber(bitmapBuffer),
			 BufferGetBlockNumber(lovBuffer),
 			 lovOffset,
 			 firstTidNumber,
 			 bitmapOpaque->bm_last_tid_location,
 			 tidnum);
	
	/* Find the word that contains the bit of tidnum. */
	for (wordNo = 0; wordNo < bitmapOpaque->bm_hrl_words_used; wordNo++)
	{
		word = bitmap->cwords[wordNo];
		if (IS_FILL_WORD(bitmap->hwords, wordNo))
			bitNo += FILL_LENGTH(word) * BM_HRL_WORD_SIZE;
		else
			bitNo += BM_HRL_WORD_SIZE;

		if (firstTidNumber + bitNo - 1 >= tidnum)
		{
			found = true;
			break;		/* find the word */
		}
	}

	if(!found)
	{
		elog(ERROR, "Found incorrect bitmap words in LOV block %d offset %d. "
			 "Please reindex",
			 BufferGetBlockNumber(lovBuffer), lovOffset);
	}

	Assert (wordNo <= bitmapOpaque->bm_hrl_words_used);

	/*
	 * If the word containing the updating bit is a literal word,
	 * we simply update the word, and return.
	 */
	if (!IS_FILL_WORD(bitmap->hwords, wordNo))
	{
		uint16 insertingPos = (tidnum - 1) % BM_HRL_WORD_SIZE;

		START_CRIT_SECTION();

		MarkBufferDirty(bitmapBuffer);

		bitmap->cwords[wordNo] |= (((BM_HRL_WORD)1)<<insertingPos);

		if (use_wal)
			_bitmap_log_updateword(rel, bitmapBuffer, wordNo);

		END_CRIT_SECTION();
		
		if (Debug_bitmap_print_insert)
			elog(LOG, "Bitmap Insert: updated a set bit in bitmap page %d, "
				 "lovBlock=%d, lovOffset=%d"
				 ", firstTidNumber=" INT64_FORMAT
				 ", bm_last_tid_location=" INT64_FORMAT
				 ", tidnum=" INT64_FORMAT
				 ", wordNo=%d",
				 BufferGetBlockNumber(bitmapBuffer),
				 BufferGetBlockNumber(lovBuffer),
				 lovOffset,
				 firstTidNumber,
				 bitmapOpaque->bm_last_tid_location,
				 tidnum,
				 wordNo);

		return;
	}

	/* If this bit is already 1, then simply return. */
	if (GET_FILL_BIT(word) == 1)
	{
		if (Debug_bitmap_print_insert)
			elog(LOG, "Bitmap Insert: updated a set bit in bitmap page %d, "
				 "lovBlock=%d, lovOffset=%d"
				 ", firstTidNumber=" INT64_FORMAT
				 ", bm_last_tid_location=" INT64_FORMAT
				 ", tidnum=" INT64_FORMAT
				 ", wordNo=%d, no update is needed",
				 BufferGetBlockNumber(bitmapBuffer),
				 BufferGetBlockNumber(lovBuffer),
				 lovOffset,
				 firstTidNumber,
				 bitmapOpaque->bm_last_tid_location,
				 tidnum,
				 wordNo);
		
		return;
	}

	firstTidNumber = firstTidNumber + bitNo -
					 FILL_LENGTH(word) * BM_HRL_WORD_SIZE;
		
	Assert(tidnum >= firstTidNumber);

	MemSet(&new_words, 0, sizeof(new_words));
	buf_extend(&new_words);
	updatesetbit_inword(word, tidnum - firstTidNumber, firstTidNumber,
						&new_words);

	/* Make sure that there are at most 3 new words. */
	Assert(new_words.curword <= 3);

	if (new_words.curword == 1)
	{
		uint32		off = wordNo/BM_HRL_WORD_SIZE;

		START_CRIT_SECTION();

		MarkBufferDirty(bitmapBuffer);

		bitmap->cwords[wordNo] = new_words.cwords[0];
		bitmap->hwords[off] &= (BM_HRL_WORD)(~WORDNO_GET_HEADER_BIT(wordNo));

		if (IS_FILL_WORD(new_words.hwords, 0))
			elog(ERROR, "The header bit should be 1.");

		Assert(IS_FILL_WORD(new_words.hwords, 0) ==
			   IS_FILL_WORD(bitmap->hwords, wordNo));

		if (use_wal)
			_bitmap_log_updateword(rel, bitmapBuffer, wordNo);

		END_CRIT_SECTION();

		if (Debug_bitmap_print_insert)
			elog(LOG, "Bitmap Insert: updated a set bit in bitmap page %d, "
				 "lovBlock=%d, lovOffset=%d"
				 ", firstTidNumber=" INT64_FORMAT
				 ", bm_last_tid_location=" INT64_FORMAT
				 ", tidnum=" INT64_FORMAT
				 ", wordNo=%d, header bit=%d",
				 BufferGetBlockNumber(bitmapBuffer),
				 BufferGetBlockNumber(lovBuffer),
				 lovOffset,
				 firstTidNumber,
				 bitmapOpaque->bm_last_tid_location,
				 tidnum,
				 wordNo,
				 IS_FILL_WORD(bitmap->hwords, off));
		return;		
	}

	/*
	 * Check if this page has enough space for all new words. If so,
	 * replace this word with new words. Otherwise,
	 * we first check if the next page has enough space for all new words.
	 * If so, insert new words to the next page, otherwise,
	 * create a new page.
	 */
	free_words = BM_NUM_OF_HRL_WORDS_PER_PAGE -
			bitmapOpaque->bm_hrl_words_used;

	new_page = false;
	new_lastpage = false;
	nextBuffer = InvalidBuffer;

	if (free_words < new_words.curword - 1)
	{
		if (bitmapOpaque->bm_bitmap_next != InvalidBlockNumber)
		{
			nextBuffer = _bitmap_getbuf(rel, bitmapOpaque->bm_bitmap_next,
										BM_WRITE);
			nextPage = BufferGetPage(nextBuffer);
			nextOpaque = (BMBitmapOpaque)PageGetSpecialPointer(nextPage);
			free_words = BM_NUM_OF_HRL_WORDS_PER_PAGE -
				nextOpaque->bm_hrl_words_used;
		} else
		{
			new_lastpage = true;
		}
	}

	if (free_words < new_words.curword - 1)
	{
		if (BufferIsValid(nextBuffer))
			_bitmap_relbuf(nextBuffer);

		nextBuffer = _bitmap_getbuf(rel, P_NEW, BM_WRITE);
		_bitmap_init_bitmappage(BufferGetPage(nextBuffer));
		new_page = true;
		free_words = BM_NUM_OF_HRL_WORDS_PER_PAGE;
	}

	START_CRIT_SECTION();

	MarkBufferDirty(bitmapBuffer);
	if (BufferIsValid(nextBuffer))
		MarkBufferDirty(nextBuffer);

	if (new_lastpage)
	{
		Page 		lovPage;
		BMLOVItem	lovItem;
		MarkBufferDirty(lovBuffer);

		lovPage = BufferGetPage(lovBuffer);
		lovItem = (BMLOVItem) PageGetItem(lovPage, 
			PageGetItemId(lovPage, lovOffset));
		lovItem->bm_lov_tail = BufferGetBlockNumber(nextBuffer);
	}

	if (Debug_bitmap_print_insert)
		elog(LOG, "Bitmap Insert: updated a set bit in bitmap page %d, "
			 "lovBlock=%d, lovOffset=%d"
			 ", firstTidNumber=" INT64_FORMAT
			 ", bm_last_tid_location=" INT64_FORMAT
			 ", tidnum=" INT64_FORMAT
			 ", generate %d new words, %d new bitmap page",
			 BufferGetBlockNumber(bitmapBuffer),
			 BufferGetBlockNumber(lovBuffer),
			 lovOffset,
			 firstTidNumber,
			 bitmapOpaque->bm_last_tid_location,
			 tidnum,
			 new_words.curword,
			 new_page
			 );

	bitmap->cwords[wordNo] = new_words.cwords[0];
	if (tidnum - firstTidNumber + 1 <= BM_HRL_WORD_SIZE)
	{
		uint32		off = wordNo/BM_HRL_WORD_SIZE;

		bitmap->hwords[off] &= (BM_HRL_WORD)(~WORDNO_GET_HEADER_BIT(wordNo));
		if (IS_FILL_WORD(new_words.hwords, 0))
			elog(ERROR, "the header bit should be 1.");
	}

	/* ignore the first word in new_words.cwords. */
	new_words.hwords[0] = ((BM_HRL_WORD)new_words.hwords[0]) << 1;
	for (word_no = 0; word_no < new_words.curword - 1; word_no++)
		new_words.cwords[word_no] = new_words.cwords[word_no+1];
	new_words.curword--;

	/* Create the buffer for the original words */
	MemSet(&words, 0, sizeof(words));
	words.cwords = bitmap->cwords;
	memcpy(words.hwords, bitmap->hwords,
		   BM_CALC_H_WORDS(bitmapOpaque->bm_hrl_words_used) * sizeof(BM_HRL_WORD));
	words.num_cwords = BM_NUM_OF_HRL_WORDS_PER_PAGE;
	words.curword = bitmapOpaque->bm_hrl_words_used;

	MemSet(&words_left, 0, sizeof(words_left));
	buf_extend(&words_left);

	insert_newwords(&words, wordNo + 1, &new_words, &words_left);

	/*
	 * We have to copy header words back to the page, and set the correct
	 * number of words in the page.
	 */
	bitmapOpaque->bm_hrl_words_used = words.curword;
	memcpy(bitmap->hwords, words.hwords,
		   BM_CALC_H_WORDS(bitmapOpaque->bm_hrl_words_used) * sizeof(BM_HRL_WORD));

	if (new_page)
	{
		nextPage = BufferGetPage(nextBuffer);
		nextOpaque = (BMBitmapOpaque)PageGetSpecialPointer(nextPage);
		nextBitmap = (BMBitmap)PageGetContentsMaxAligned(nextPage);

		nextOpaque->bm_last_tid_location = bitmapOpaque->bm_last_tid_location;
		nextOpaque->bm_bitmap_next = bitmapOpaque->bm_bitmap_next;
		bitmapOpaque->bm_bitmap_next = BufferGetBlockNumber(nextBuffer);
	}

	bitmapOpaque->bm_last_tid_location -=
		getnumbits(words_left.cwords, words_left.hwords, words_left.curword);

	if (words_left.curword > 0)
	{
		nextPage = BufferGetPage(nextBuffer);
		nextOpaque = (BMBitmapOpaque)PageGetSpecialPointer(nextPage);
		nextBitmap = (BMBitmap)PageGetContentsMaxAligned(nextPage);

		/* Create the buffer for the original words */
		MemSet(&words, 0, sizeof(words));
		words.cwords = nextBitmap->cwords;
		memcpy(words.hwords, nextBitmap->hwords,
			   BM_CALC_H_WORDS(nextOpaque->bm_hrl_words_used) * sizeof(BM_HRL_WORD));
		words.num_cwords = BM_NUM_OF_HRL_WORDS_PER_PAGE;
		words.curword = nextOpaque->bm_hrl_words_used;

		MemSet(&new_words, 0, sizeof(new_words));

		insert_newwords(&words, 0, &words_left, &new_words);

		/*
		 * We have to copy header words back to the page, and set the correct
		 * number of words in the page.
		 */
		nextOpaque->bm_hrl_words_used = words.curword;
		memcpy(nextBitmap->hwords, words.hwords,
			   BM_CALC_H_WORDS(nextOpaque->bm_hrl_words_used) * sizeof(BM_HRL_WORD));

		Assert(new_words.curword == 0);
	}

	if (use_wal)
		_bitmap_log_updatewords(rel, lovBuffer, lovOffset,
								bitmapBuffer, nextBuffer, new_lastpage);

	END_CRIT_SECTION();

	if (BufferIsValid(nextBuffer))
		_bitmap_relbuf(nextBuffer);
	_bitmap_free_tidbuf(&new_words);
	_bitmap_free_tidbuf(&words_left);
}

/*
 * findbitmappage() -- find the bitmap page that contains
 *	the given tid location, and obtain the first tid location
 * 	in this page.
 *
 * We assume that this tid location is not in bm_last_compword or
 * bm_last_word of its LOVItem.
 *
 * We will have write lock on the bitmap page we find.
 */
static void
findbitmappage(Relation rel, BMLOVItem lovitem,
					   uint64 tidnum,
					   Buffer* bitmapBufferP, uint64* firstTidNumberP)
{
	BlockNumber nextBlockNo = lovitem->bm_lov_head;

	*firstTidNumberP = 1;

	while (BlockNumberIsValid(nextBlockNo))
	{
		Page bitmapPage;
		BMBitmapOpaque bitmapOpaque;

		CHECK_FOR_INTERRUPTS();

		*bitmapBufferP = _bitmap_getbuf(rel, nextBlockNo, BM_WRITE);
		bitmapPage = BufferGetPage(*bitmapBufferP);
		bitmapOpaque = (BMBitmapOpaque)
			PageGetSpecialPointer(bitmapPage);

		if (bitmapOpaque->bm_last_tid_location >= tidnum)
			return;   		/* find the page */

		(*firstTidNumberP) = bitmapOpaque->bm_last_tid_location + 1;
		nextBlockNo = bitmapOpaque->bm_bitmap_next;

		_bitmap_relbuf(*bitmapBufferP);
	}

	/*
	 * We can't find such a page. This should not happen.
	 * So we error out.
	 */
 	elog(ERROR, "Can not find the bitmap page containing tid=" INT64_FORMAT
 		 ", nextBlockNo=%d"
 		 ", lov_head=%d, lov_tail=%d"
 		 ", firstTidNumber=" INT64_FORMAT
 		 ", bm_last_tid_location=" INT64_FORMAT
 		 ", bm_last_setbit=" INT64_FORMAT
 		 ", last_compword=" INT64_FORMAT 
		 ", last_word=" INT64_FORMAT 
		 ", headerbits=%d. Please reindex",
		 tidnum, nextBlockNo, lovitem->bm_lov_head, lovitem->bm_lov_tail,
		 *firstTidNumberP, lovitem->bm_last_tid_location, lovitem->bm_last_setbit,
		 lovitem->bm_last_compword, lovitem->bm_last_word, lovitem->lov_words_header);
}

/*
 * verify_bitmappages() -- verify if the bm_last_tid_location values
 * 	are valid in all bitmap pages. Only used during debugging.
 */
static void
verify_bitmappages(Relation rel, BMLOVItem lovitem)
{
	BlockNumber nextBlockNo = lovitem->bm_lov_head;
	uint64 tidnum = 0;

	while (BlockNumberIsValid(nextBlockNo))
	{
		Page bitmapPage;
		BMBitmapOpaque bitmapOpaque;
		Buffer bitmapBuffer;
		uint32 wordNo;
		BMBitmap bitmap;

		bitmapBuffer = _bitmap_getbuf(rel, nextBlockNo, BM_READ);
		bitmapPage = BufferGetPage(bitmapBuffer);
		bitmapOpaque = (BMBitmapOpaque)
			PageGetSpecialPointer(bitmapPage);
		bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);

		for (wordNo = 0; wordNo < bitmapOpaque->bm_hrl_words_used; wordNo++)
		{
			BM_HRL_WORD word = bitmap->cwords[wordNo];
			if (IS_FILL_WORD(bitmap->hwords, wordNo))
				tidnum += FILL_LENGTH(word) * BM_HRL_WORD_SIZE;
			else
				tidnum += BM_HRL_WORD_SIZE;

		}

		if (bitmapOpaque->bm_last_tid_location != tidnum)
		{
			elog(ERROR, "bm_last_tid_location=" INT64_FORMAT ", tidnum=" INT64_FORMAT,
                 bitmapOpaque->bm_last_tid_location, tidnum);
		}

		nextBlockNo = bitmapOpaque->bm_bitmap_next;

		_bitmap_relbuf(bitmapBuffer);
	}
}

/*
 * mergewords() -- merge last two bitmap words based on the HRL compression
 * 	scheme. If these two words can not be merged, the last complete
 * 	word will be appended into the word array in the buffer.
 *
 * If the buffer is extended, this function returns the number
 * of bytes used.
 */
static int16
mergewords(BMTIDBuffer *buf, bool lastWordFill)
{
	int16 bytes_used = 0;

	/* the last_tid in the complete word */
	uint64 last_tid = buf->last_tid - buf->last_tid%BM_HRL_WORD_SIZE;

	/*
	 * If last_compword is LITERAL_ALL_ONE, it is not set yet.
	 * We move last_word to it.
	 */
	if (buf->last_compword == LITERAL_ALL_ONE)
	{
		buf->last_compword = buf->last_word;
		buf->is_last_compword_fill = lastWordFill;
		if (lastWordFill)
			last_tid = FILL_LENGTH(buf->last_word) * BM_HRL_WORD_SIZE;
		else
			last_tid = BM_HRL_WORD_SIZE;

		buf->last_word = 0;
		buf->last_tid = last_tid;

		return bytes_used;
	}
	/*
	 * If both words are fill words, and have the same fill bit,
	 * we increment the fill length of the last complete word by
	 * the fill length stored in the last word.
	 */
	if (buf->is_last_compword_fill && lastWordFill &&
		(GET_FILL_BIT(buf->last_compword) ==
		 GET_FILL_BIT(buf->last_word)))
	{
		BM_HRL_WORD lengthMerged;
			
		if (FILL_LENGTH(buf->last_compword) + FILL_LENGTH(buf->last_word) <=
			MAX_FILL_LENGTH)
		{
			last_tid += FILL_LENGTH(buf->last_word)*BM_HRL_WORD_SIZE;
			buf->last_compword += FILL_LENGTH(buf->last_word);
			buf->last_word = LITERAL_ALL_ZERO;
			buf->last_tid = last_tid;

			return bytes_used;
		}

		lengthMerged =
			MAX_FILL_LENGTH - FILL_LENGTH(buf->last_compword);
		buf->last_word -= lengthMerged;
		last_tid += lengthMerged * BM_HRL_WORD_SIZE;
		buf->last_compword += lengthMerged;
	}

	/*
	 * Here, these two words can not be merged together. We
	 * move the last complete word to the array, and set it to be the
	 * last word.
	 */

	/*
	 * When there are not enough space in the array of new words,
	 * we re-allocate a bigger space.
	 */
	bytes_used += buf_extend(buf);

	buf->cwords[buf->curword] = buf->last_compword;
	buf->last_tids[buf->curword] = last_tid;

	if (buf->is_last_compword_fill)
		buf->hwords[buf->curword/BM_HRL_WORD_SIZE] |=
			((BM_HRL_WORD)1) << (BM_HRL_WORD_SIZE - 1 -
								 buf->curword % BM_HRL_WORD_SIZE);

	buf->curword ++;

	buf->last_compword = buf->last_word;
	buf->is_last_compword_fill = lastWordFill;
	if (buf->is_last_compword_fill)
		last_tid += FILL_LENGTH(buf->last_compword) * BM_HRL_WORD_SIZE;
	else
		last_tid += BM_HRL_WORD_SIZE;

	buf->last_word = 0;
	buf->last_tid = last_tid;

	return bytes_used;
}

/*
 * _bitmap_write_new_bitmapwords() -- write a given buffer of new bitmap words
 * 	into the end of bitmap page(s).
 *
 * If the last bitmap page does not have enough space for all these new
 * words, new pages will be allocated here.
 *
 * We consider a write to one bitmap page as one atomic-action WAL
 * record. The WAL record for the write to the last bitmap page also
 * includes updates on the lov item. Writes to the non-last
 * bitmap page are not self-consistent. We need to do some fix-up
 * during WAL logic replay.
 */
static void
_bitmap_write_new_bitmapwords(Relation rel,
							  Buffer lovBuffer, OffsetNumber lovOffset,
							  BMTIDBuffer* buf, bool use_wal)
{
	Page		lovPage;
	BMLOVItem	lovItem;
	bool		first_page_needs_init = false;
	List	   *perpage_buffers = NIL;
	List	   *perpage_xlrecs = NIL;
	ListCell   *lcb;

	lovPage = BufferGetPage(lovBuffer);
	lovItem = (BMLOVItem) PageGetItem(lovPage,
									  PageGetItemId(lovPage, lovOffset));

	/*
	 * Write changes to bitmap pages, if needed. (We might get away by
	 * updating just the last words stored on the LOV item.)
	 */
	if (buf->curword > 0)
	{
		ListCell   *lcp;
		BlockNumber first_blkno;
		BlockNumber last_blkno;
		List	   *perpage_tmppages = NIL;
		bool		is_first;
		ListCell   *buffer_cell;
		int			start_wordno;

		/*
		 * Write bitmap words, one page at a time, allocating new pages as
		 * required.
		 */
		is_first = true;
		start_wordno = 0;
		do
		{
			Buffer		bitmapBuffer;
			bool		bitmapBufferNeedsInit = false;
			Page		bitmapPage;
			BMBitmapOpaque	bitmapPageOpaque;
			uint32		numFreeWords;
			uint32		words_written;
			xl_bm_bitmapwords_perpage *xlrec_perpage;
			Page		tmppage = NULL;

			if (is_first && lovItem->bm_lov_head != InvalidBlockNumber)
			{
				bitmapBuffer = _bitmap_getbuf(rel, lovItem->bm_lov_tail, BM_WRITE);

				/* Append to an existing LOV page as much as fits */
				bitmapPage = BufferGetPage(bitmapBuffer);
				bitmapPageOpaque =
					(BMBitmapOpaque) PageGetSpecialPointer(bitmapPage);

				numFreeWords = BM_NUM_OF_HRL_WORDS_PER_PAGE -
					bitmapPageOpaque->bm_hrl_words_used;
			}
			else
			{
				/* Allocate new page */
				bitmapBuffer = _bitmap_getbuf(rel, P_NEW, BM_WRITE);
				bitmapBufferNeedsInit = true;
				numFreeWords = BM_NUM_OF_HRL_WORDS_PER_PAGE;
			}

			/*
			 * Remember information about the first page, needed
			 * for updating the LOV and for the WAL record.
			 */
			if (is_first)
			{
				first_blkno = BufferGetBlockNumber(bitmapBuffer);
				first_page_needs_init = bitmapBufferNeedsInit;
			}

			if (use_wal)
			{
				xlrec_perpage = palloc0(sizeof(xl_bm_bitmapwords_perpage));
				xlrec_perpage->bmp_blkno = BufferGetBlockNumber(bitmapBuffer);
			}
			else
				xlrec_perpage = NULL;
			perpage_buffers = lappend_int(perpage_buffers, bitmapBuffer);
			perpage_xlrecs = lappend(perpage_xlrecs, xlrec_perpage);

			if (list_length(perpage_buffers) > MAX_BITMAP_PAGES_PER_INSERT)
				elog(ERROR, "too many bitmap pages in one insert batch");

			/*
			 * Allocate a new temporary page to operate on, in case we fail
			 * half-way through the updates (because of running out of memory
			 * or disk space, most likely). If this is the last bitmap page,
			 * i.e. we can fit all the remaining words on this bitmap page,
			 * though, we can skip that, and modify the page directly.
			 *
			 * If this is not the last page, we will need to allocate more
			 * pages. That in turn might fail, so we must not modify the
			 * existing pages yet.
			 */
			if (numFreeWords < buf->curword - start_wordno)
			{
				/*
				 * Does not fit, we will need to expand.
				 *
				 * Note: we don't write to the page until we're sure we get
				 * all of them. We do all the action on temp copies.
				 */
				if (bitmapBufferNeedsInit)
				{
					tmppage = palloc(BLCKSZ);
					_bitmap_init_bitmappage(tmppage);
				}
				else
					tmppage = PageGetTempPageCopy(BufferGetPage(bitmapBuffer));

				bitmapPage = tmppage;

				perpage_tmppages = lappend(perpage_tmppages, tmppage);
			}
			else
			{
				/*
				 * This is the last page. Now that we have successfully
				 * fetched/allocated it, none of the things we do should
				 * ereport(), so we can make the changes directly to the
				 * buffer.
				 */
				bitmapPage = BufferGetPage(bitmapBuffer);
				START_CRIT_SECTION();
				if (bitmapBufferNeedsInit)
					_bitmap_init_bitmappage(bitmapPage);
			}

			words_written =
				_bitmap_write_bitmapwords_on_page(bitmapPage, buf,
												  start_wordno, xlrec_perpage);
			Assert(is_first || words_written > 0);
			start_wordno += words_written;

			if (bitmapPage != tmppage)
				MarkBufferDirty(bitmapBuffer);

			last_blkno = BufferGetBlockNumber(bitmapBuffer);
			is_first = false;
		} while (buf->curword - start_wordno > 0);
		Assert(start_wordno == buf->curword);

		/*
		 * Ok, we have locked all the pages we need. Apply any changes we had made on
		 * temporary pages.
		 *
		 * NOTE: there is one fewer temppage.
		 */
		buffer_cell = list_head(perpage_buffers);
		foreach(lcp, perpage_tmppages)
		{
			Page		tmppage = (Page) lfirst(lcp);
			Buffer		buffer = (Buffer) lfirst_int(buffer_cell);
			Page		page = BufferGetPage(buffer);
			BlockNumber nextBlkNo;
			BMBitmapOpaque	bitmapPageOpaque;

			PageRestoreTempPage(tmppage, page);
			MarkBufferDirty(buffer);

			/* Update the 'next' pointer on this page, before moving on */
			buffer_cell = lnext(buffer_cell);
			Assert(buffer_cell);
			nextBlkNo = BufferGetBlockNumber((Buffer) lfirst_int(buffer_cell));

			bitmapPageOpaque =
				(BMBitmapOpaque) PageGetSpecialPointer(page);

			bitmapPageOpaque->bm_bitmap_next = nextBlkNo;
		}
		list_free(perpage_tmppages);

		/* Update the bitmap page pointers in the LOV item */
		if (first_page_needs_init)
			lovItem->bm_lov_head = first_blkno;
		lovItem->bm_lov_tail = last_blkno;
	}
	else
	{
		START_CRIT_SECTION();
	}

	/* Update LOV item (lov_head/tail were updated above already) */
	lovItem->bm_last_compword = buf->last_compword;
	lovItem->bm_last_word = buf->last_word;
	lovItem->lov_words_header = (buf->is_last_compword_fill) ? 2 : 0;
	lovItem->bm_last_setbit = buf->last_tid;
	lovItem->bm_last_tid_location = buf->last_tid - buf->last_tid % BM_HRL_WORD_SIZE;
	MarkBufferDirty(lovBuffer);

	/* Write WAL record */
	if (use_wal)
	{
		if (buf->curword > 0)
			_bitmap_log_bitmapwords(rel, buf,
									first_page_needs_init, perpage_xlrecs, perpage_buffers,
									lovBuffer, lovOffset, buf->last_tid);
		else
			_bitmap_log_bitmap_lastwords(rel, lovBuffer, lovOffset, lovItem);
	}

	END_CRIT_SECTION();

	if (Debug_bitmap_print_insert)
		elog(LOG, "Bitmap Insert: write bitmapwords: numwords=%d"
			 ", last_tid=" INT64_FORMAT
			 ", lov_blkno=%d, lov_offset=%d, lovItem->bm_last_setbit=" INT64_FORMAT
			 ", lovItem->bm_last_tid_location=" INT64_FORMAT,
			 (int) buf->curword, buf->last_tid,
			 BufferGetBlockNumber(lovBuffer), lovOffset,
			 lovItem->bm_last_setbit, lovItem->bm_last_tid_location);

	/* release all bitmap buffers. */
	foreach(lcb, perpage_buffers)
	{
		UnlockReleaseBuffer((Buffer) lfirst_int(lcb));
	}
}


/*
 * _bitmap_write_bitmapwords_on_page() -- Write an array of bitmap words into
 * the given bitmap page. Returns the number of words that have been written
 * in this call.
 *
 * The number of bitmap words writing to a given bitmap page is the maximum
 * number of words that can be appended into the page.
 */
static uint64
_bitmap_write_bitmapwords_on_page(Page bitmapPage, BMTIDBuffer *buf, int startWordNo,
								  xl_bm_bitmapwords_perpage *xlrec_perpage)
{
	BMBitmapOpaque	bitmapPageOpaque = (BMBitmapOpaque)PageGetSpecialPointer(bitmapPage);
	BMBitmap		bitmap = (BMBitmap) PageGetContentsMaxAligned(bitmapPage);
	uint64			cwords;
	uint64 			words_written;
	uint64			start_hword_no, end_hword_no;
	uint64			final_start_hword_no, final_end_hword_no;
	BM_HRL_WORD		hwords[BM_MAX_NUM_OF_HEADER_WORDS + 1];
	uint64			num_hwords;
	uint32			start_hword_bit, end_hword_bit, final_start_hword_bit;

	cwords = bitmapPageOpaque->bm_hrl_words_used;

	words_written = buf->curword - startWordNo;
	if (words_written > BM_NUM_OF_HRL_WORDS_PER_PAGE - cwords)
		words_written = BM_NUM_OF_HRL_WORDS_PER_PAGE - cwords;

	if (words_written == 0)
	{
		/*
		 * This page is full. We still include it in the WAL record, to keep this
		 * case the same as the general case.
		 */
		if (xlrec_perpage)
		{
			xlrec_perpage->bmp_last_tid = bitmapPageOpaque->bm_last_tid_location;
			xlrec_perpage->bmp_start_hword_no = 0;
			xlrec_perpage->bmp_num_hwords = 0;
			xlrec_perpage->bmp_start_cword_no = cwords;
			xlrec_perpage->bmp_num_cwords = 0;
		}
		return 0;
	}

	/* Copy the content words */
	memcpy(bitmap->cwords + cwords,
		   buf->cwords + startWordNo,
		   words_written * sizeof(BM_HRL_WORD));
	/* and make note of what we changed for the WAL record */
	if (xlrec_perpage)
	{
		xlrec_perpage->bmp_start_cword_no = cwords;
		xlrec_perpage->bmp_num_cwords = words_written;
	}

	/*
	 * Shift the header words in 'words' to match with the bit positions
	 * in the header words in this page, and then copy them.
	 */
	start_hword_no = startWordNo/BM_HRL_WORD_SIZE;
	end_hword_no = (startWordNo + words_written - 1) / BM_HRL_WORD_SIZE;
	num_hwords = end_hword_no - start_hword_no + 1;

	Assert(num_hwords < BM_MAX_NUM_OF_HEADER_WORDS);
	memcpy(hwords, buf->hwords + start_hword_no,
			num_hwords * sizeof(BM_HRL_WORD));
	hwords[num_hwords] = 0;

	/* clean up the first and last header words */
	start_hword_bit = startWordNo % BM_HRL_WORD_SIZE;
	end_hword_bit = (startWordNo + words_written - 1) % BM_HRL_WORD_SIZE;

	hwords[0] = ((BM_HRL_WORD)(hwords[0] << start_hword_bit)) >>
				start_hword_bit;
	hwords[num_hwords - 1] = (hwords[num_hwords - 1] >>
			(BM_HRL_WORD_SIZE - end_hword_bit - 1)) <<
			(BM_HRL_WORD_SIZE - end_hword_bit - 1);

	final_start_hword_bit = cwords % BM_HRL_WORD_SIZE;

	if (final_start_hword_bit > start_hword_bit)
	{
		/* right-shift 'final-start_hword_bit - start_hword_bit' */
		rshift_header_bits(hwords, num_hwords + 1,
						   final_start_hword_bit - start_hword_bit);
	}
		
	else if (final_start_hword_bit < start_hword_bit)
	{
		/* left-shift 'start_hword_bit - final_start_hword_bit' */
		lshift_header_bits(hwords, num_hwords,
						   start_hword_bit - final_start_hword_bit);
	}

	/* copy the header bits */
	final_start_hword_no = cwords / BM_HRL_WORD_SIZE;
	final_end_hword_no = (cwords + words_written - 1) / BM_HRL_WORD_SIZE;

	bitmap->hwords[final_start_hword_no] |= hwords[0];
	memcpy(bitmap->hwords + (final_start_hword_no + 1),
		   hwords + 1,
		   (final_end_hword_no - final_start_hword_no) *
			sizeof(BM_HRL_WORD));
	/* and make note of what we changed for the WAL record */
	if (xlrec_perpage)
	{
		xlrec_perpage->bmp_start_hword_no = final_start_hword_no;
		xlrec_perpage->bmp_num_hwords = (final_end_hword_no + 1) - final_start_hword_no;
	}

	bitmapPageOpaque->bm_hrl_words_used += words_written;

 	if (Debug_bitmap_print_insert)
  	{
 		elog(LOG, "Bitmap Insert: insert bitmapwords: "
 			 ", old bm_last_tid_location=" INT64_FORMAT
 			 ", new bm_last_tid_location=" INT64_FORMAT
 			 ", first last_tid=" INT64_FORMAT
 			 ", last_compword=" INT64_FORMAT
 			 ", is_last_compword_fill=%s"
 			 ", last_word=" INT64_FORMAT
 			 ", last_setbit=" INT64_FORMAT,
			 bitmapPageOpaque->bm_last_tid_location,
			 buf->last_tids[startWordNo + words_written-1],
			 buf->last_tids[startWordNo],
			 buf->last_compword,
			 (buf->is_last_compword_fill ? "yes" : "no"),
			 buf->last_word,
			 buf->last_tid);
	}

	Assert(bitmapPageOpaque->bm_last_tid_location <=
		   buf->last_tids[startWordNo + words_written-1]);

	bitmapPageOpaque->bm_last_tid_location =
		buf->last_tids[startWordNo + words_written-1];
	if (xlrec_perpage)
		xlrec_perpage->bmp_last_tid = bitmapPageOpaque->bm_last_tid_location;

	return words_written;
}

/*
 * create_lovitem() -- create a new LOV item.
 *
 * Create a new LOV item and append this item into the last LOV page.
 * Each LOV item is associated with one distinct value for attributes
 * to be indexed. This function also inserts this distinct value along
 * with this new LOV item's block number and offsetnumber into the
 * auxiliary heap and its b-tree of this bitmap index.
 *
 * This function returns the block number and offset number of this
 * new LOV item.
 *
 * The caller should have an exclusive lock on metabuf.
 */
static void
create_lovitem(Relation rel, Buffer metabuf, uint64 tidnum,
			   TupleDesc tupDesc, Datum *attdata, bool *nulls,
			   Relation lovHeap, Relation lovIndex, BlockNumber *lovBlockP, 
			   OffsetNumber *lovOffsetP, bool use_wal)
{
	BMMetaPage		metapage;
	Buffer			currLovBuffer;
	Page			currLovPage;
	Datum*			lovDatum;
	bool*			lovNulls;
	OffsetNumber	itemSize;
	BMLOVItem		lovitem;
	int				numOfAttrs;
	bool			is_new_lov_blkno = false;

	numOfAttrs = tupDesc->natts;

	/* Get the last LOV page. Meta page should be locked. */
	metapage = _bitmap_get_metapage_data(rel, metabuf);
	*lovBlockP = metapage->bm_lov_lastpage;

	currLovBuffer = _bitmap_getbuf(rel, *lovBlockP, BM_WRITE);
	currLovPage = BufferGetPage(currLovBuffer);

	lovitem = _bitmap_formitem(tidnum);

	*lovOffsetP = OffsetNumberNext(PageGetMaxOffsetNumber(currLovPage));
	itemSize = sizeof(BMLOVItemData);

	/*
	 * If there is not enough space in the last LOV page for
	 * a new item, create a new LOV page, and update the metapage.
	 */
	if (itemSize > PageGetFreeSpace(currLovPage))
	{
		Buffer		newLovBuffer;

		/* create a new LOV page */
		newLovBuffer = _bitmap_getbuf(rel, P_NEW, BM_WRITE);
		_bitmap_init_lovpage(rel, newLovBuffer);

		_bitmap_relbuf(currLovBuffer);

		currLovBuffer = newLovBuffer;
		currLovPage = BufferGetPage(currLovBuffer);

		is_new_lov_blkno = true;
	}

	/* First create the LOV item. */
	START_CRIT_SECTION();

	if (is_new_lov_blkno)
	{
		MarkBufferDirty(metabuf);

		metapage->bm_lov_lastpage = BufferGetBlockNumber(currLovBuffer);

		*lovOffsetP = OffsetNumberNext(PageGetMaxOffsetNumber(currLovPage));
		*lovBlockP = BufferGetBlockNumber(currLovBuffer);
	}

	MarkBufferDirty(currLovBuffer);

	if (PageAddItem(currLovPage, (Item)lovitem, itemSize, *lovOffsetP,
					false, false) == InvalidOffsetNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("failed to add LOV item to \"%s\"",
				RelationGetRelationName(rel))));

	if(use_wal)
		_bitmap_log_lovitem(rel, MAIN_FORKNUM, currLovBuffer, *lovOffsetP, lovitem,
							metabuf, is_new_lov_blkno);

	END_CRIT_SECTION();

	/*
	 * .. and then create the entry in the auxiliary LOV heap and index for it.
	 *
	 * This could still fail for various reasons, e.g. if you run out of disk
	 * space. In that case, we'll leave behind an "orphan" LOV item, with no
	 * corresponding item in the LOV heap. That's a bit sloppy and leaky, but
	 * harmless; the orphaned LOV item won't be encountered by any scans.
	 */
	lovDatum = palloc0((numOfAttrs + 2) * sizeof(Datum));
	lovNulls = palloc0((numOfAttrs + 2) * sizeof(bool));
	memcpy(lovDatum, attdata, numOfAttrs * sizeof(Datum));
	memcpy(lovNulls, nulls, numOfAttrs * sizeof(bool));
	lovDatum[numOfAttrs] = Int32GetDatum(*lovBlockP);
	lovNulls[numOfAttrs] = false;
	lovDatum[numOfAttrs + 1] = Int16GetDatum(*lovOffsetP);
	lovNulls[numOfAttrs + 1] = false;

	_bitmap_insert_lov(lovHeap, lovIndex, lovDatum, lovNulls, use_wal);

	_bitmap_relbuf(currLovBuffer);

	if (Debug_bitmap_print_insert)
		elog(LOG, "Bitmap Insert: create a lov item: "
			 "lovBlock=%d, lovOffset=%d, is_new_lovblock=%d",
			 *lovBlockP, *lovOffsetP, is_new_lov_blkno);

	pfree(lovitem);
	pfree(lovDatum);
	pfree(lovNulls);
}

/*
 * When building an index we try and buffer calls to write tids to disk
 * as it will result in lots of I/Os.
 */

static void
buf_add_tid(Relation rel, BMTidBuildBuf *tids, uint64 tidnum, 
			BMBuildState *state, BlockNumber lov_block, OffsetNumber off)
{
	BMTIDBuffer *buf;
	BMTIDLOVBuffer *lov_buf = NULL;

	/* If we surpass maintenance_work_mem, free some space from the buffer */
	if (tids->byte_size >= maintenance_work_mem * 1024L)
		buf_make_space(rel, tids, state->use_wal);

	/*
	 * tids is lazily initialized. If we do not have a current LOV block 
	 * buffer, initialize one.
	 */
	if (!BlockNumberIsValid(tids->max_lov_block) || 
		tids->max_lov_block < lov_block)
	{
		/*
		 * XXX: We're currently not including the size of this data structure
		 * in out byte_size count... should we?
		 */
		lov_buf = palloc(sizeof(BMTIDLOVBuffer));
		lov_buf->lov_block = lov_block;
		MemSet(lov_buf->bufs, 0, BM_MAX_LOVITEMS_PER_PAGE * sizeof(BMTIDBuffer *));
		tids->max_lov_block = lov_block;
		
		/*
		 * Add the new LOV buffer to the list head. It seems reasonable that
		 * future calls to this function will want this lov_block rather than
		 * older lov_blocks.
		 */
		tids->lov_blocks = lcons(lov_buf, tids->lov_blocks);
	}
	else
	{
		ListCell *cell;
		
		foreach(cell, tids->lov_blocks)
		{
			BMTIDLOVBuffer *tmp = lfirst(cell);
			if(tmp->lov_block == lov_block)
			{
				lov_buf = tmp;
				break;
			}
		}
	}
	
	Assert(lov_buf);
	Assert(off - 1 < BM_MAX_LOVITEMS_PER_PAGE);

	if (lov_buf->bufs[off - 1])
	{

		buf = lov_buf->bufs[off - 1];

		Buffer lovbuf = _bitmap_getbuf(rel, lov_block, BM_WRITE);
		buf_add_tid_with_fill(rel, buf, lovbuf, off,
							  tidnum, state->use_wal);
		_bitmap_relbuf(lovbuf);
	}
	else
	{
		/* no pre-existing buffer found, create a new one */
		Buffer lovbuf;
		Page page;
		BMLOVItem lovitem;
		uint16 bytes_added;
		
		buf = (BMTIDBuffer *)palloc0(sizeof(BMTIDBuffer));
		
		lovbuf = _bitmap_getbuf(rel, lov_block, BM_WRITE);
		page = BufferGetPage(lovbuf);
		lovitem = (BMLOVItem)PageGetItem(page, PageGetItemId(page, off));

		buf->last_tid = lovitem->bm_last_setbit;
		buf->last_compword = lovitem->bm_last_compword;
		buf->last_word = lovitem->bm_last_word;
		buf->is_last_compword_fill = (lovitem->lov_words_header == 2);

		MemSet(buf->hwords, 0, BM_NUM_OF_HEADER_WORDS * sizeof(BM_HRL_WORD));

		bytes_added = buf_extend(buf);

		buf->curword = 0;

		buf_add_tid_with_fill(rel, buf, lovbuf, off, tidnum,
							  state->use_wal);

		_bitmap_relbuf(lovbuf);

		lov_buf->bufs[off - 1] = buf;
		tids->byte_size += bytes_added;
	}
}

/*
 * buf_add_tid_with_fill() -- Worker for buf_add_tid().
 *
 * Return how many bytes are used. Since we move words to disk when
 * there is no space left for new header words, this returning number
 * can be negative.
 */
static int16
buf_add_tid_with_fill(Relation rel, BMTIDBuffer *buf,
					  Buffer lovBuffer, OffsetNumber off,
					  uint64 tidnum, bool use_wal)
{
	int64 zeros;
	uint16 inserting_pos;
	int16 bytes_used = 0;

	/*
	 * Compute how many zeros between this set bit and the last inserted
	 * set bit.
	 */
	zeros = tidnum - buf->last_tid - 1;

	/*
	 * If zeros is less than 0, the incoming tids are not
	 * sorted. Currently, this is not allowed.
	 */
	if (zeros < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("tids are not in order when building the bitmap index: "
						"new tidnum " INT64_FORMAT ", last tidnum " INT64_FORMAT,
						tidnum, buf->last_tid)));

	if (zeros > 0)
	{
		uint64 zerosNeeded;
		uint64 numOfTotalFillWords;

		/* 
		 * Calculate how many bits are needed to fill up the existing last
		 * bitmap word.
		 */
		zerosNeeded =
			BM_HRL_WORD_SIZE - ((buf->last_tid - 1) % BM_HRL_WORD_SIZE) - 1;

		if (zerosNeeded > 0 && zeros >= zerosNeeded)
		{
			/*
			 * The last bitmap word is complete now. We merge it with the
			 * last bitmap complete word.
			 */
			bytes_used -=
				buf_ensure_head_space(rel, buf, lovBuffer, off, use_wal);

			bytes_used += mergewords(buf, false);
			zeros -= zerosNeeded;
		}

		/*
		 * If the remaining zeros are more than BM_HRL_WORD_SIZE,
		 * We construct the last bitmap word to be a fill word, and merge it
		 * with the last complete bitmap word.
		 */
		numOfTotalFillWords = zeros/BM_HRL_WORD_SIZE;

		while (numOfTotalFillWords > 0)
		{
			BM_HRL_WORD 	numOfFillWords;

			CHECK_FOR_INTERRUPTS();

			if (numOfTotalFillWords >= MAX_FILL_LENGTH)
				numOfFillWords = MAX_FILL_LENGTH;
			else
				numOfFillWords = numOfTotalFillWords;

			buf->last_word = BM_MAKE_FILL_WORD(0, numOfFillWords);

			bytes_used -= 
				buf_ensure_head_space(rel, buf, lovBuffer, off, use_wal);
			bytes_used += mergewords(buf, true);

			numOfTotalFillWords -= numOfFillWords;
			zeros -= ((uint64)numOfFillWords * BM_HRL_WORD_SIZE);
		}
	}

	Assert((zeros >= 0) && (zeros<BM_HRL_WORD_SIZE));
	
	inserting_pos = (tidnum-1)%BM_HRL_WORD_SIZE;
	buf->last_word |= (((BM_HRL_WORD)1) << inserting_pos);

	if (tidnum % BM_HRL_WORD_SIZE == 0)
	{
		bool lastWordFill = false;

		if (buf->last_word == LITERAL_ALL_ZERO)
		{
			buf->last_word = BM_MAKE_FILL_WORD(0, 1);
			lastWordFill = true;
		}

		else if (buf->last_word == LITERAL_ALL_ONE)
		{
			buf->last_word = BM_MAKE_FILL_WORD(1, 1);
			lastWordFill = true;
		}

		bytes_used -=
			buf_ensure_head_space(rel, buf, lovBuffer, off, use_wal);
		bytes_used += mergewords(buf, lastWordFill);
	}

	buf->last_tid = tidnum;

	return bytes_used;
}

/*
 * buf_ensure_head_space() -- If there is no space in the header words,
 * move words in the given buffer to disk and free the existing space,
 * and then allocate new space for future new words.
 *
 * The number of bytes freed are returned.
 */
static uint16
buf_ensure_head_space(Relation rel, BMTIDBuffer *buf, 
					  Buffer lovBuffer, OffsetNumber off, bool use_wal)
{
	uint16 bytes_freed = 0;

	if (buf->curword >= (BM_NUM_OF_HEADER_WORDS * BM_HRL_WORD_SIZE))
	{
		bytes_freed = buf_free_mem_block(rel, buf, lovBuffer, off, use_wal);
		bytes_freed -= buf_extend(buf);
	}

	return bytes_freed;
}

/*
 * buf_extend() -- Enlarge the memory allocated to a buffer.
 * Return how many bytes are added to the buffer.
 */
static uint16
buf_extend(BMTIDBuffer *buf)
{
	uint16 bytes;
	uint16 size;
	
	if (buf->num_cwords > 0 && buf->curword < buf->num_cwords - 1)
		return 0; /* already large enough */

	if(buf->num_cwords == 0)
	{
		size = BUF_INIT_WORDS;
		buf->cwords = (BM_HRL_WORD *)
			palloc0(BUF_INIT_WORDS * sizeof(BM_HRL_WORD));
		buf->last_tids = (uint64 *)palloc0(BUF_INIT_WORDS * sizeof(uint64));
		bytes = BUF_INIT_WORDS * sizeof(BM_HRL_WORD) +
			BUF_INIT_WORDS * sizeof(uint64);
	}
	else
	{
		size = buf->num_cwords;
		buf->cwords = repalloc(buf->cwords, 2 * size * sizeof(BM_HRL_WORD));
		MemSet(buf->cwords + size, 0, size * sizeof(BM_HRL_WORD));
		buf->last_tids = repalloc(buf->last_tids, 2 * size * sizeof(uint64));
		MemSet(buf->last_tids + size, 0, size * sizeof(uint64));
		bytes = 2 * size * sizeof(BM_HRL_WORD) +
			2 * size * sizeof(uint64);
	}
	buf->num_cwords += size;
	return bytes;
}

/*
 * Spill some HRL compressed tids to disk
 *
 * Use buf_free_mem_block if the caller already holds a BM_WRITE lock
 * on lovbuf (Buffer item). Else, buf_free_mem will get and lock lovBuffer
 * by its block number, internally call buf_free_mem_block and finally
 * release lovBuffer.
 */

static uint16
buf_free_mem(Relation rel, BMTIDBuffer *buf, BlockNumber lov_block,
			 OffsetNumber off, bool use_wal)
{
	Buffer lovbuf;
	uint16 bytes_freed;

	/* already done */
	if (buf->num_cwords == 0)
		return 0;

	lovbuf = _bitmap_getbuf(rel, lov_block, BM_WRITE);

	bytes_freed = buf_free_mem_block(rel, buf, lovbuf, off, use_wal);

	_bitmap_relbuf(lovbuf);

	return bytes_freed;
}

static uint16
buf_free_mem_block(Relation rel, BMTIDBuffer *buf, Buffer lovbuf,
			 OffsetNumber off, bool use_wal)
{
	/* already done */
	if (buf->num_cwords == 0)
		return 0;

	_bitmap_write_new_bitmapwords(rel, lovbuf, off, buf, use_wal);

	return _bitmap_free_tidbuf(buf);
}

/*
 * Spill some data out of the buffer to free up space.
 */
static void
buf_make_space(Relation rel, BMTidBuildBuf *locbuf, bool use_wal)
{
	ListCell *cell;

	/*
	 * Now, we could just pull the head of lov_blocks but there'd be no
	 * guarantee that we'd free up enough space.
	 */
	foreach(cell, locbuf->lov_blocks)
	{
		int i;
		BMTIDLOVBuffer *lov_buf = (BMTIDLOVBuffer *)lfirst(cell);
		BlockNumber lov_block = lov_buf->lov_block;

		for (i = 0; i < BM_MAX_LOVITEMS_PER_PAGE; i++)
		{
			BMTIDBuffer *buf = (BMTIDBuffer *)lov_buf->bufs[i];
			OffsetNumber off;

			/* return if we've freed enough space */
#ifdef NOT_USED
			if (locbuf->byte_size < (maintenance_work_mem * 1024L))
				return;
#endif
			if (!buf || buf->num_cwords == 0)
				continue;

			off = i + 1;
			locbuf->byte_size -= buf_free_mem(rel, buf, lov_block, off, use_wal);

			if (QueryCancelPending || ProcDiePending)
				return;
		}
#ifdef NOT_USED
		if (locbuf->byte_size < (maintenance_work_mem * 1024L))
			return;
#endif
	}
}

/*
 * _bitmap_free_tidbuf() -- release the space.
 */
static uint16
_bitmap_free_tidbuf(BMTIDBuffer* buf)
{
	uint16 bytes_freed = 0;

	if (buf->last_tids)
		pfree(buf->last_tids);
	if (buf->cwords)
		pfree(buf->cwords);

	bytes_freed = buf->num_cwords * sizeof(BM_HRL_WORD) +
		buf->num_cwords * sizeof(uint64);

	buf->num_cwords = 0;
	buf->curword = 0;
	/* Paranoia */
	MemSet(buf->hwords, 0, sizeof(BM_HRL_WORD) * BM_NUM_OF_HEADER_WORDS);

	return bytes_freed;
}

/*
 * insertsetbit() -- insert a given set bit into a bitmap
 * 	specified by lovBlock and lovOffset.
 */
static void
insertsetbit(Relation rel, BlockNumber lovBlock, OffsetNumber lovOffset,
			 uint64 tidnum, BMTIDBuffer *buf, bool use_wal)
{
	Buffer lovBuffer;
	Page		lovPage;
	BMLOVItem	lovItem;

	lovBuffer = _bitmap_getbuf(rel, lovBlock, BM_WRITE);
	lovPage = BufferGetPage(lovBuffer);
	lovItem = (BMLOVItem) PageGetItem(lovPage, 
									  PageGetItemId(lovPage, lovOffset));

	buf->last_compword = lovItem->bm_last_compword;
	buf->last_word = lovItem->bm_last_word;
	buf->is_last_compword_fill = (lovItem->lov_words_header >= 2);
	buf->last_tid = lovItem->bm_last_setbit;
	if (buf->cwords)
	{
		MemSet(buf->cwords, 0,
				buf->num_cwords * sizeof(BM_HRL_WORD));
	}
	MemSet(buf->hwords, 0,
		   BM_CALC_H_WORDS(buf->num_cwords) * sizeof(BM_HRL_WORD));
	if (buf->last_tids)
		MemSet(buf->last_tids, 0,
				buf->num_cwords * sizeof(uint64));
	buf->curword = 0;

	/*
	 * Usually, tidnum is greater than lovItem->bm_last_setbit.
	 * However, if this is not the case, this should be called while
	 * doing 'vacuum full' or doing insertion after 'vacuum'. In this
	 * case, we try to update this bit in the corresponding bitmap
	 * vector.
	 */
	if (tidnum <= lovItem->bm_last_setbit)
	{
		/*
		 * Scan through the bitmap vector, and update the bit in
		 * tidnum.
		 */
		updatesetbit(rel, lovBuffer, lovOffset, tidnum, use_wal);

		_bitmap_relbuf(lovBuffer);
		return;
	}

	/*
	 * To insert this new set bit, we also need to add all zeros between
	 * this set bit and last set bit. We construct all new words here.
	 */
	buf_add_tid_with_fill(rel, buf, lovBuffer, lovOffset, tidnum, use_wal);
	
	/*
	 * If there are only updates to the last bitmap complete word and
	 * last bitmp word, we simply needs to update the lov buffer.
	 */
	if (buf->num_cwords == 0)
	{
		START_CRIT_SECTION();

		MarkBufferDirty(lovBuffer);

		lovItem->bm_last_compword = buf->last_compword;
		lovItem->bm_last_word = buf->last_word;
		lovItem->lov_words_header =
			(buf->is_last_compword_fill) ? 2 : 0;
		lovItem->bm_last_setbit = tidnum;
		lovItem->bm_last_tid_location = tidnum - tidnum % BM_HRL_WORD_SIZE;

		if (use_wal)
			_bitmap_log_bitmap_lastwords
				(rel, lovBuffer, lovOffset, lovItem);
		
		END_CRIT_SECTION();

 		if (Debug_bitmap_print_insert)
 			elog(LOG, "Bitmap Insert: insert to last two words"
 				 ", bm_last_setbit=" INT64_FORMAT
 				 ", bm_last_tid_location=" INT64_FORMAT,
 				 lovItem->bm_last_setbit,
 				 lovItem->bm_last_tid_location);
 		
		_bitmap_relbuf(lovBuffer);
		
		return;
	}

	/*
	 * Write bitmap words to bitmap pages. When there are no enough
	 * space for all these bitmap words, new bitmap pages are created.
	 */
	_bitmap_write_new_bitmapwords(rel, lovBuffer, lovOffset,
								  buf, use_wal);
	_bitmap_relbuf(lovBuffer);
}

/*
 * _bitmap_write_alltids() -- write all tids in the given buffer into disk.
 */
void
_bitmap_write_alltids(Relation rel, BMTidBuildBuf *tids, 
					  bool use_wal)
{
	ListCell *cell;

	foreach(cell, tids->lov_blocks)
	{
		int i;
		BMTIDLOVBuffer *lov_buf = (BMTIDLOVBuffer *)lfirst(cell);
		BlockNumber lov_block = lov_buf->lov_block;

		for (i = 0; i < BM_MAX_LOVITEMS_PER_PAGE; i++)
		{
			BMTIDBuffer *buf = (BMTIDBuffer *)lov_buf->bufs[i];
			OffsetNumber off;

			CHECK_FOR_INTERRUPTS();

			if (!buf || buf->num_cwords == 0)
				continue;

			off = i + 1;

			buf_free_mem(rel, buf, lov_block, off, use_wal);

			pfree(buf);

			lov_buf->bufs[i] = NULL;
		}
	}
	list_free_deep(tids->lov_blocks);
	tids->lov_blocks = NIL;
	tids->byte_size = 0;
}

/*
 * build_inserttuple() -- insert a new tuple into the bitmap index
 *	during the bitmap index construction.
 *
 * Each new tuple has an assigned number -- tidnum, called a
 * tid location, which represents the bit location for this tuple in
 * a bitmap vector. To speed up the construction, this function does not
 * write this tid location into its bitmap vector immediately. We maintain
 * a buffer -- BMTidBuildBuf to keep an array of tid locations
 * for each distinct attribute value.
 *
 * If this insertion causes the buffer to overflow, we write tid locations
 * for enough distinct values to disk to accommodate this new tuple.
 */
static void
build_inserttuple(Relation rel, uint64 tidnum,
				  ItemPointerData ht_ctid  __attribute__((unused)), TupleDesc tupDesc,
				  Datum *attdata, bool *nulls, BMBuildState *state)
{
	Buffer 			metabuf;
	BlockNumber		lovBlock;
	OffsetNumber	lovOffset;
	bool			blockNull;
	bool			offsetNull;
	BMTidBuildBuf *tidLocsBuffer;
	int				attno;
	bool			allNulls = true;
	BMBuildHashKey  *entry;

	CHECK_FOR_INTERRUPTS();

	tidLocsBuffer = state->bm_tidLocsBuffer;

	/* Check if all attributes have value of NULL. */
	for (attno = 0; attno < tupDesc->natts; attno++)
	{
		if (!nulls[attno])
		{
			allNulls = false;
			break;
		}
	}

	metabuf = _bitmap_getbuf(rel, BM_METAPAGE, BM_WRITE);
	
	/*
	 * if the inserting tuple has the value of NULL, then
	 * the corresponding tid array is the first.
	 */
	if (allNulls)
	{
		lovBlock = BM_LOV_STARTPAGE;
		lovOffset = 1;
	}
	else
	{
		bool found;
		BMBuildLovData *lov;

		if (state->lovitem_hash)
		{
		    BMBuildHashKey toLookup;
		    toLookup.attributeValueArr = attdata;
		    toLookup.isNullArr = nulls;

			/* look up the hash to see if we can find the lov data that way */
			entry = (BMBuildHashKey *)hash_search(state->lovitem_hash,
												  (void*)&toLookup,
												  HASH_ENTER, &found);
			if (!found)
			{
				/* Copy the key values in case someone modifies them */
				for(attno = 0; attno < tupDesc->natts; attno++)
				{
					Form_pg_attribute at = tupDesc->attrs[attno];

					entry->attributeValueArr[attno] = datumCopy(entry->attributeValueArr[attno], at->attbyval,
																at->attlen);
				}

				/*
				 * If the inserting tuple has a new value, then we create a new
				 * LOV item.
				 */
				create_lovitem(rel, metabuf, tidnum, tupDesc, attdata, 
							   nulls, state->bm_lov_heap, state->bm_lov_index,
							   &lovBlock, &lovOffset, state->use_wal);

				lov = (BMBuildLovData *) (((char*)entry) + state->lovitem_hashKeySize );
				lov->lov_block = lovBlock;
				lov->lov_off = lovOffset;
			}

			else
			{
				lov = (BMBuildLovData *) (((char*)entry) + state->lovitem_hashKeySize );
				lovBlock = lov->lov_block;
				lovOffset = lov->lov_off;
			}
		}

		else {
			/*
			 * Search the btree to find the right bitmap vector to append
			 * this bit. Here, we reset the scan key and call index_rescan.
			 */
			for (attno = 0; attno<tupDesc->natts; attno++)
			{
				ScanKey theScanKey = (ScanKey)(((char*)state->bm_lov_scanKeys) +
											   attno * sizeof(ScanKeyData));
				if (nulls[attno])
				{
					theScanKey->sk_flags = SK_ISNULL;
					theScanKey->sk_argument = attdata[attno];
				}
				else
				{
					theScanKey->sk_flags = 0;
					theScanKey->sk_argument = attdata[attno];
				}
			}

			index_rescan(state->bm_lov_scanDesc, state->bm_lov_scanKeys, tupDesc->natts, NULL, 0);

			found = _bitmap_findvalue(state->bm_lov_heap, state->bm_lov_index,
									  state->bm_lov_scanKeys, state->bm_lov_scanDesc,
									  &lovBlock, &blockNull, &lovOffset, &offsetNull);

			if (!found)
			{
				/*
				 * If the inserting tuple has a new value, then we create a new
				 * LOV item.
				 */
				create_lovitem(rel, metabuf, tidnum, tupDesc, attdata, 
							   nulls, state->bm_lov_heap, state->bm_lov_index,
							   &lovBlock, &lovOffset, state->use_wal);
			}
		}
	}

	buf_add_tid(rel, tidLocsBuffer, tidnum, state, lovBlock, lovOffset);
	_bitmap_wrtbuf(metabuf);

	CHECK_FOR_INTERRUPTS();
}

/*
 * inserttuple() -- insert a new tuple into the bitmap index.
 *
 * This function finds the corresponding bitmap vector associated with
 * the given attribute value, and inserts a set bit into this bitmap
 * vector. Each distinct attribute value is stored as a LOV item, which
 * is stored in a list of LOV pages.
 *
 * If there is no LOV item associated with the given attribute value,
 * a new LOV item is created and appended into the last LOV page.
 *
 * For support the high-cardinality case for attributes to be indexed,
 * we also maintain an auxiliary heap and a btree structure for all
 * the distinct attribute values so that the search for the
 * corresponding bitmap vector can be done faster. The heap
 * contains all attributes to be indexed and 2 more attributes --
 * the block number of the offset number of the block that stores
 * the corresponding LOV item. The b-tree index is on this new heap
 * and the key contains all attributes to be indexed.
 */
static void
inserttuple(Relation rel, Buffer metabuf, uint64 tidnum, 
			ItemPointerData ht_ctid __attribute__((unused)), TupleDesc tupDesc, Datum *attdata,
			bool *nulls, Relation lovHeap, Relation lovIndex, ScanKey scanKey,
		   	IndexScanDesc scanDesc, bool use_wal)
{
	BlockNumber		lovBlock;
	OffsetNumber	lovOffset;
	bool			blockNull, offsetNull;
	bool			allNulls = true;
	int 			attno;

	BMTIDBuffer     buf;
	MemSet(&buf, 0, sizeof(buf));

	/* Check if the values of given attributes are all NULL. */
	for (attno = 0; attno < tupDesc->natts; attno++)
	{
		if (!nulls[attno])
		{
			allNulls = false;
			break;
		}
	}

	/*
	 * if the inserting tuple has the value NULL, then the LOV item is
	 * the first item in the lovBuffer.
	 */
	if (allNulls)
	{
		lovBlock = BM_LOV_STARTPAGE;
		lovOffset = 1;
	}
	else
	{
 		bool res = false;
		
		/*
		 * Search through the lov heap and index to find the LOV item which
		 * has the same value as the inserting tuple. If such an item is
		 * not found, then we create a new LOV item, and insert it into the
		 * lov heap and index.
		 */

		/*
		 * XXX: We lock the meta page to guarantee that only one writer
		 * will create a new lovItem at once. However, this does not
		 * guard against a race condition where a concurrent writer is
		 * inserting the same key as us. So we do another search with
		 * SnapshotDirty. If such a key is found, we have to wait for
		 * the other guy, and try again. However, this may cause
		 * distributed deadlock (see MPP-3155). The fix is to use
		 * FrozenTransactionId for tuples in the LOV heap so that
		 * all tuples are always visible to any transactions.
		 *
		 * The problem is, locking the metapage is pretty heavy handed 
		 * because the read routines need a read lock on it. There are a
		 * few other things we could do instead: use a BM insert lock or
		 * wrap the code below in a PG_TRY and try and catch the unique
		 * constraint violation from the btree code.
		 */

		LockBuffer(metabuf, BM_WRITE);
		
		res = _bitmap_findvalue(lovHeap, lovIndex, scanKey, scanDesc, &lovBlock,
								&blockNull, &lovOffset, &offsetNull);

		if(!res)
		{
			create_lovitem(rel, metabuf, tidnum, tupDesc,
						   attdata, nulls, lovHeap, lovIndex,
						   &lovBlock, &lovOffset, use_wal);
		}
		LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
	}

	/*
	 * Here, we have found the block number and offset number of the
	 * LOV item that points to the bitmap page, to which we will
	 * append the set bit.
	 */

	insertsetbit(rel, lovBlock, lovOffset, tidnum, &buf, use_wal);

	_bitmap_free_tidbuf(&buf);
}

/*
 * _bitmap_buildinsert() -- insert an index tuple during index creation.
 */
void
_bitmap_buildinsert(Relation rel, ItemPointerData ht_ctid, Datum *attdata, 
					bool *nulls, BMBuildState *state)
{
	TupleDesc	tupDesc;
	uint64		tidOffset;

	tidOffset = BM_IPTR_TO_INT(&ht_ctid); 

	tupDesc = RelationGetDescr(rel);

	/* insert a new bit into the corresponding bitmap */
	build_inserttuple(rel, tidOffset, ht_ctid,
							  tupDesc, attdata, nulls, state);
}

/*
 * _bitmap_doinsert() -- insert an index tuple for a given tuple.
 */
void
_bitmap_doinsert(Relation rel, ItemPointerData ht_ctid, Datum *attdata, 
				 bool *nulls)
{
	uint64			tidOffset;
	TupleDesc		tupDesc;
	Buffer			metabuf;
	BMMetaPage		metapage;
	Relation		lovHeap, lovIndex;
	ScanKey			scanKeys;
	IndexScanDesc	scanDesc;
	int				attno;

	tupDesc = RelationGetDescr(rel);
	if (tupDesc->natts <= 0)
		return ;

	tidOffset = BM_IPTR_TO_INT(&ht_ctid);

	/* insert a new bit into the corresponding bitmap using the HRL scheme */
	metabuf = _bitmap_getbuf(rel, BM_METAPAGE, BM_READ);
	metapage = _bitmap_get_metapage_data(rel, metabuf);
	_bitmap_open_lov_heapandindex(rel, metapage, &lovHeap, &lovIndex, 
								  RowExclusiveLock);

	LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);

	scanKeys = (ScanKey) palloc0(tupDesc->natts * sizeof(ScanKeyData));

	for (attno = 0; attno < tupDesc->natts; attno++)
	{
		Oid			eq_opr;
		RegProcedure opfuncid;
		ScanKey		scanKey;

		get_sort_group_operators(tupDesc->attrs[attno]->atttypid,
								 false, true, false,
								 NULL, &eq_opr, NULL, NULL);
		opfuncid = get_opcode(eq_opr);

		scanKey = (ScanKey) (((char *)scanKeys) + attno * sizeof(ScanKeyData));

		ScanKeyEntryInitialize(scanKey,
							   nulls[attno] ? SK_ISNULL : 0,
							   attno + 1,
							   BTEqualStrategyNumber,
							   InvalidOid,
							   lovIndex->rd_indcollation[attno],
							   opfuncid,
							   attdata[attno]);
	}

	scanDesc = index_beginscan(lovHeap, lovIndex, GetActiveSnapshot(),
							   tupDesc->natts, 0);
	index_rescan(scanDesc, scanKeys, tupDesc->natts, NULL, 0);

	/* insert this new tuple into the bitmap index. */
	inserttuple(rel, metabuf, tidOffset, ht_ctid, tupDesc, attdata, nulls, 
				lovHeap, lovIndex, scanKeys, scanDesc, RelationNeedsWAL(rel));

	index_endscan(scanDesc);
	_bitmap_close_lov_heapandindex(lovHeap, lovIndex, RowExclusiveLock);

	ReleaseBuffer(metabuf);
	pfree(scanKeys);
}
