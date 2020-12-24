/*-------------------------------------------------------------------------
 *
 * cdbvarblock.h
 *	  A general data package that has variable-length item array that can be
 *    efficiently indexed once complete.
 *
 *  The variable-length items are byte sized.
 *
 *  Alignment:
 *      The first item will be aligned on an eight-byte (64 bit) boundary.
 *      If all items are a multiple or 8, 4, 2 in length then they will
 *      be aligned on those multiples.  If the items are any length, then
 *      the alignment is 1 byte.
 *
 *  For efficient VarBlock making, the client is given a pointer directly
 *  into the VarBlock buffer and the client fills their items directly.
 *
 *  Examples:
 *
 *      #define MY_VARBLOCK_TEMP_SCRATCH_LEN 2000
 *
 *      // Make a VarBlock in the buffer from some records.
 *		VarBlockByteLen MakerExample(
 *		        byte *buffer, VarBlockByteLen maxBufferLen,
 *		        Records *records, int numRecords)
 *		{
 *		    VarBlockMaker  myVarBlockMaker;
 *		    uint8 tempScratch [MY_VARBLOCK_TEMP_SCRATCH_LEN];
 *		    byte *item;
 *
 *		    VarBlockMakerInit(
 *		        &myVarBlockMaker,
 *		        buffer,
 *		        maxBufferLen,
 *		        tempScratch,
 *		        MY_VARBLOCK_TEMP_SCRATCH_LEN);
 *
 *		    for (int i = 0; i < numRecords; i++)
 *		    {
 *		        item = VarBlockMakerGetNextItemPtr(
 *		                       &myVarBlockMaker,
 *		                       records[i].length);
 *		        Assert(item != NULL);   // Real code would deal with NULL
 *		                                // indicating VarBlock full...
 *		                                
 *		        MemCpy(item,records[i].data,records[i].length);
 *		    }
 *
 *		    return VarBlockMakerFinish(&myVarBlockMaker);
 *		}
 *
 *		// Extract records out of VarBlock.
 *		void ReaderExample(
 *		        byte *buffer, VarBlockByteLen bufferLen,
 *		        Records *records, int *numRecords)
 *		{
 *		    VarBlockReader  myVarBlockReader;
 *		    byte *item;
 *
 *		    VarBlockReaderInit(
 *		        &myVarBlockReader,
 *		        buffer,
 *		        bufferLen);
 *
 *		    *numRecords = VarBlockReaderItemCount(&myVarBlockReader);
 *
 *		    for (int i = 0; i < *numRecords; i++)
 *		    {
 *		        item = VarBlockMakerGetNextItemPtr(
 *		                       &myVarBlockMaker,
 *		                       &records[i].length);
 *		        Assert(item != NULL);
 *
 *		        //
 *		        // Keep a pointer to each item.
 *		        //
 *		        records[i].data = item;
 *		    }
 *       }
 *        
 * Portions Copyright (c) 2007, greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbvarblock.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBVARBLOCK_H
#define CDBVARBLOCK_H

typedef int32 VarBlockByteLen;
typedef int32 VarBlockByteOffset;

// ----------------------------------------------------------
typedef enum VarBlockVersion
{
	InitialVersion = 1
} VarBlockVersion;

typedef uint16 VarBlockByteOffset16;

/*
 * The VarBlockByteOffset24struct is designed to be 3 bytes long
 * (it contains a 24-bit field) but a few compilers will pad it to
 * four bytes.  Rather than deal with the ambiguity of unpersuadable compilers,
 * we try to use "VARBLOCK_BYTE_OFFSET_24_LEN" rather than 
 * "sizeof(VarBlockByteOffset24)" when computing on-disk sizes AND 
 * we DO NOT index arrays as the struct but calculate addesses and do byte
 * moves.
 */
typedef struct VarBlockByteOffset24
{
    unsigned  		byteOffset24:24;
                           /* Medium style offsets. */
						   
} VarBlockByteOffset24;

#define VARBLOCK_BYTE_OFFSET_24_LEN 3

typedef struct VarBlockByteLen24
{
    unsigned  		byteLen24:24;
                           /* Medium style lengths. */
} VarBlockByteLen24;


typedef struct VarBlockHeader
{
/*
 * Can't seem to get this to pack nicely to 64 bits so the header is aligned
 * on an 8 byte boundary, so go to bit shifting...
 */
 
//  unsigned  		offsetsAreSmall:1;  
//                        /* True if the offsets are small and occupy 2 bytes.
//                         * Otherwise, the block is larger 3-byte offsets are
//                         * used.
//                         */
//  unsigned  		reserved:5;
//						   /* Reserved for future. */
//  VarBlockVersion version:2;  
//						   /* Version number */
//
//  VarBlockByteLen24 itemLenSum;
//                         /* Offset to the item offset array. */
//
// -----------------------------------------------------------------------------
//  unsigned  		moreReserved:8;
//						   /* Reserved for future. */
//  unsigned  		itemCount:24;
//                         /* Total number of items. 24-bits to we bit-pad 
//						    * to a total of 64-bits. 
//						    */

	uint32			bytes_0_3;
	uint32			bytes_4_7;

} VarBlockHeader;

#define VarBlockGet_offsetsAreSmall(h) 	((h)->bytes_0_3>>31)
#define VarBlockGet_reserved(h) 		(((h)->bytes_0_3&0x7C000000)>>26)
#define VarBlockGet_version(h) 		    (((h)->bytes_0_3&0x03000000)>>24)
#define VarBlockGet_itemLenSum(h) 		((h)->bytes_0_3&0x00FFFFFF)

#define VarBlockGet_moreReserved(h) 	(((h)->bytes_4_7&0xFF000000)>>24)
#define VarBlockGet_itemCount(h) 		((h)->bytes_4_7&0x00FFFFFF)

// For single bits, set or clear directly.  Otherwise, use AND to clear field then OR to set.
#define VarBlockSet_offsetsAreSmall(h,e) 	{if(e)(h)->bytes_0_3|=0x80000000;else(h)->bytes_0_3&=0x7FFFFFFF;} 
#define VarBlockSet_version(h,e) 	        {(h)->bytes_0_3&=0xFCFFFFFF;(h)->bytes_0_3|=(0x03000000&((e)<<24));} 
#define VarBlockSet_itemLenSum(h,e) 	    {(h)->bytes_0_3&=0xFF000000;(h)->bytes_0_3|=(0x00FFFFFF&(e));}

#define VarBlockSet_itemCount(h,e) 	        {(h)->bytes_4_7&=0xFF000000;(h)->bytes_4_7|=(0x00FFFFFF&(e));}

// Assume field is initially zero.
#define VarBlock_Init(h)          {(h)->bytes_0_3=0;(h)->bytes_4_7=0;} 

#define VarBlockInit_offsetsAreSmall(h,e) 	{if(e)(h)->bytes_0_3|=0x80000000;} 
#define VarBlockInit_version(h,e) 	        {(h)->bytes_0_3|=(0x03000000&((e)<<24));} 
#define VarBlockInit_itemLenSum(h,e) 	    {(h)->bytes_0_3|=(0x00FFFFFF&(e));}

#define VarBlockInit_itemCount(h,e) 	    {(h)->bytes_4_7|=(0x00FFFFFF&(e));}

#define VARBLOCK_HEADER_LEN sizeof(VarBlockHeader)

// ----------------------------------------------------------

typedef struct VarBlockMaker
{
	VarBlockHeader       *header;
							/* The buffer with the header at the beginning. */
							
    VarBlockByteLen      maxBufferLen;
							/* The maximum amount of space that can be used
							 * for the VarBlock.
							 */
	
    VarBlockByteLen      currentItemLenSum;
							/* The running sum of the item data lengths. */

    uint8                *tempScratchSpace;
    int                  tempScratchSpaceLen;
							/* The scratch space where we will temporarily
							 * keep the item offsets while making the
							 * VarBlock. 
							 */

    uint8                *nextItemPtr;
							/* Pointer in the buffer to the beginning of
							 * the next item.
							 */
							 
	uint8                *last2ByteOffsetPtr;
							/* The boundary point where we need to switch
							 * from 2-byte offsets to 3-byte offsets.
							 */
							 
    int                  currentItemCount;
							/* The running count of items. */
							
	int                  maxItemCount;
							/* The maximum number of items for the VarBlock.
							 * Based on the length of the scratch area.
							 */
} VarBlockMaker;

// ----------------------------------------------------------

typedef struct VarBlockReader
{
	VarBlockHeader       *header;
							/* The buffer with the header at the beginning. */
							
    VarBlockByteLen      bufferLen;
							/* The exact byte length of this VarBlock. */

    int                  nextIndex;
    uint8                *nextItemPtr;
							/* The index and pointer when doing get-next
							 * scanning of the VarBlock.
							 */
							 
    VarBlockByteOffset   offsetToOffsetArray;
							/* Offset to the beginning of the offset array. */
							
} VarBlockReader;


typedef enum VarBlockCheckError
{
	VarBlockCheckOk = 0,
	VarBlockCheckBadVersion,
	VarBlockCheckReservedNot0,
	VarBlockCheckMoreReservedNot0,
	VarBlockCheckItemSumLenBad1,
	VarBlockCheckItemSumLenBad2,
	VarBlockCheckZeroPadBad1,
	VarBlockCheckZeroPadBad2,
	VarBlockCheckItemCountBad1,
	VarBlockCheckItemCountBad2,
	VarBlockCheckOffsetBad1,
	VarBlockCheckOffsetBad2,
	VarBlockCheckOffsetBad3,
} VarBlockCheckError;


/*
 * Initialize the VarBlock maker.
 *
 * Since we are going to pack the item offset array
 * right after the variable-length item array when
 * finished, we need a place to buffer the
 * offsets while we are making the block.  The caller
 * supplies the tempItemOffsets parameter for this
 * purpose.  Note that maxTempItemOffsets in effect
 * serves as the maximum number of items.
 */
extern void VarBlockMakerInit(
    VarBlockMaker        *varBlockMaker,
    uint8                *buffer,
    VarBlockByteLen      maxBufferLen,
    uint8                *tempScratchSpace,
    int                  tempScratchSpaceLen);

/*
 * Get a pointer to the next variable-length item so it can
 * be filled in.
 *
 * Returns NULL when there is no more space left in the VarBlock.
 */
extern uint8* VarBlockMakerGetNextItemPtr(
    VarBlockMaker        *varBlockMaker,
    VarBlockByteLen      itemLen);

/*
 * Get the variable-length item count.
 */
extern int VarBlockMakerItemCount(
    VarBlockMaker *varBlockMaker);

/*
 * Finish making the VarBlock.
 *
 * The item-offsets array will be added to the end.
 */
extern VarBlockByteLen VarBlockMakerFinish(
    VarBlockMaker *varBlockMaker);

// -----------------------------------------------------------------------------

/*
 * Determine if the header looks valid.
 *
 * peekLen must be at least VARBLOCK_HEADER_LEN bytes.
 */
extern VarBlockCheckError VarBlockHeaderIsValid(
    uint8               *buffer,
    VarBlockByteLen     peekLen);

/*
 * Determine if the whole VarBlock looks valid.
 *
 * bufferLen must be VarBlock length.
 */
extern VarBlockCheckError VarBlockIsValid(
    uint8               *buffer,
    VarBlockByteLen     bufferLen);

/*
 * Return a string message for the last check error.
 */
char *VarBlockGetCheckErrorStr(void);

// -----------------------------------------------------------------------------

/*
 * Initialize the VarBlock reader.
 */
extern void VarBlockReaderInit(
    VarBlockReader      *varBlockReader,
    uint8               *buffer,
    VarBlockByteLen     bufferLen);

/*
 * Get a pointer to the next variable-length item.
 *
 * Returns NULL when there are no more items.
 */
extern uint8* VarBlockReaderGetNextItemPtr(
    VarBlockReader      *varBlockReader,
    VarBlockByteLen     *itemLen);

/*
 * Get the variable-length item count.
 */
extern int VarBlockReaderItemCount(
    VarBlockReader *varBlockReader);

/*
 * Get a pointer to a variable-length item.
 */
extern uint8* VarBlockReaderGetItemPtr(
    VarBlockReader      *varBlockReader,
    int                 itemIndex,
    VarBlockByteLen     *itemLen);

extern VarBlockByteLen VarBlockCollapseToSingleItem(
	uint8			*target,
	uint8			*source,
	int32			sourceLen);

#endif   /* CDBVARBLOCK_H */

