/*-------------------------------------------------------------------------
 *
 * cdbvarblock.c
 *	  A general data package that has variable-length item array that can be
 *    efficiently indexed once complete.
 *
 * (See .h file for usage comments)
 *
 * Portions Copyright (c) 2007-2009, greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbvarblock.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbvarblock.h"

static VarBlockByteLen VarBlockGetItemLen(
    VarBlockReader      *varBlockReader,
    int                 itemIndex);
static void VarBlockGetItemPtrAndLen(
    VarBlockReader      *varBlockReader,
    int                 itemIndex,
    uint8				**itemPtr,
    VarBlockByteLen		*itemLen);
static VarBlockByteOffset VarBlockGetOffset(
	VarBlockHeader 		*header,
	VarBlockByteOffset  offsetToOffsetArray,
    int					itemIndex);


/*
 * Initialize the VarBlock maker.
 *
 * Since we are going to pack the item offset array
 * right after the variable-length item array when
 * finished, we need a temporary buffer for the
 * offsets while we are making the block.  The caller
 * supplies the tempScratchSpace parameter for this
 * purpose.  
 *
 * Note that tempScratchSpaceLen in effect
 * serves as the maximum number of items (UNDONE: more on 2-byte vs. 3-byte).
 */
void VarBlockMakerInit(
    VarBlockMaker        *varBlockMaker,
    uint8                *buffer,
    VarBlockByteLen      maxBufferLen,
    uint8                *tempScratchSpace,
    int                  tempScratchSpaceLen)
{
	Assert(varBlockMaker != NULL);
	Assert(buffer != NULL);								// UNDONE: And ALIGN(4)
	Assert(maxBufferLen >= VARBLOCK_HEADER_LEN);		// UNDONE: And even.
	Assert(tempScratchSpace != NULL);					// UNDONE: And ALIGN(4).
	Assert(tempScratchSpaceLen > 0);					// UNDONE: And even.
	
	varBlockMaker->header = (VarBlockHeader*)buffer;
    varBlockMaker->maxBufferLen = maxBufferLen;
    varBlockMaker->currentItemLenSum = 0;

    varBlockMaker->tempScratchSpace = tempScratchSpace;
    varBlockMaker->tempScratchSpaceLen = tempScratchSpaceLen;

    varBlockMaker->nextItemPtr = &buffer[VARBLOCK_HEADER_LEN];
	
	// UNDONE: Fix the constant to be the $MIN of approx. 16k (or 32k)
	// UNDONE: and maxBufferLen...
    varBlockMaker->last2ByteOffsetPtr = &buffer[63 * 1024];
    varBlockMaker->currentItemCount = 0;
    varBlockMaker->maxItemCount = tempScratchSpaceLen / 2;

	memset(buffer, 0, VARBLOCK_HEADER_LEN);
	VarBlockSet_version(varBlockMaker->header,InitialVersion);
	VarBlockSet_offsetsAreSmall(varBlockMaker->header,true);
}

/*
 * Get a pointer to the next variable-length item so it can
 * be filled in.
 *
 * Returns NULL when there is no more space left in the VarBlock.
 */
uint8* VarBlockMakerGetNextItemPtr(
    VarBlockMaker        *varBlockMaker,
    VarBlockByteLen      itemLen)
{
	int currentItemCount;
    uint8 *buffer;
	uint8 *nextItemPtr;
	VarBlockByteOffset nextItemOffset;
	VarBlockByteLen newItemLenSum;
	VarBlockByteLen offsetArrayLenRounded;
	VarBlockByteLen newTotalLen;
	uint8 *tempScratchSpace;
	VarBlockByteOffset16 *tempOffsetArray16;
	VarBlockByteOffset24 tempOffset24;

	Assert(varBlockMaker != NULL);
	Assert(itemLen >= 0);

	currentItemCount = varBlockMaker->currentItemCount;
	if (currentItemCount >= varBlockMaker->maxItemCount)
	{
		/*
		 * Reached a limit on the number of items based on the
		 * scratch space...
		 */
		return NULL;	
	}

    buffer = (uint8*)varBlockMaker->header;
	nextItemPtr = varBlockMaker->nextItemPtr;
	nextItemOffset = (VarBlockByteOffset)(nextItemPtr - buffer);
	newItemLenSum = varBlockMaker->currentItemLenSum + itemLen;
	tempScratchSpace = 	varBlockMaker->tempScratchSpace;

	/*
	 * We require VarBlocks to given to us on 4 byte boundaries...
	 */
	if (VarBlockGet_offsetsAreSmall(varBlockMaker->header))
	{
		int newMaxItemCount;
		
		/*
		 * Will small 2 byte offsets still work?
		 */
		if (nextItemPtr > varBlockMaker->last2ByteOffsetPtr)
		{		
			int r;
			
			/*
			 * Try out large 3-byte offsets.
			 */
			 
			/*
			 * Round-up to even length.
			 */
			offsetArrayLenRounded = (currentItemCount + 1) *
									VARBLOCK_BYTE_OFFSET_24_LEN;
			offsetArrayLenRounded = ((offsetArrayLenRounded + 1)/2)*2;
			
			newTotalLen = VARBLOCK_HEADER_LEN +
					      ((newItemLenSum + 1)/2)*2 +
						  offsetArrayLenRounded;
			
			if (newTotalLen > varBlockMaker->maxBufferLen)
				return NULL;	// Out of space.

    		newMaxItemCount = varBlockMaker->tempScratchSpaceLen / 
												VARBLOCK_BYTE_OFFSET_24_LEN;
			if (currentItemCount >= newMaxItemCount)
			{
				/*
				 * Reached a limit on the number of items based on the
				 * scratch space with large offsets...
				 */
				return NULL;	
			}

			VarBlockSet_offsetsAreSmall(varBlockMaker->header,false);
			varBlockMaker->maxItemCount = newMaxItemCount;

			/*
			 * Reverse move to use 3-byte offsets.
			 */
			tempOffsetArray16 = (VarBlockByteOffset16*)tempScratchSpace;
			for (r = currentItemCount; r >= 0; r--)
			{
				tempOffset24.byteOffset24 = tempOffsetArray16[r];
				
				memcpy(tempScratchSpace + (r * VARBLOCK_BYTE_OFFSET_24_LEN),
					   &tempOffset24, 
					   VARBLOCK_BYTE_OFFSET_24_LEN);
			}

			tempOffset24.byteOffset24 = nextItemOffset;
			memcpy(tempScratchSpace + 
							(currentItemCount * VARBLOCK_BYTE_OFFSET_24_LEN),
				   &tempOffset24, 
				   VARBLOCK_BYTE_OFFSET_24_LEN);
		}
		else
		{
			/*
			 * Normal calculation.
			 */
			newTotalLen = VARBLOCK_HEADER_LEN +
			              ((newItemLenSum + 1)/2)* 2 +
						  currentItemCount * 2 + 2;
			if (newTotalLen > varBlockMaker->maxBufferLen)
				return NULL;	// Out of space.
				
			tempOffsetArray16 = (VarBlockByteOffset16*)tempScratchSpace;
			tempOffsetArray16[currentItemCount] = 
										(VarBlockByteOffset16)nextItemOffset;
		}
	}
	else
	{
		/*
		 * Large 3 byte ofsets.
		 */
		 
		/*
		 * Round-up to even length.
		 */
		offsetArrayLenRounded = (currentItemCount + 1) *
								VARBLOCK_BYTE_OFFSET_24_LEN;
		offsetArrayLenRounded = ((offsetArrayLenRounded + 1)/2)*2;
		
		newTotalLen = VARBLOCK_HEADER_LEN +
				      ((newItemLenSum + 1)/2)*2 +
					  offsetArrayLenRounded;
			
		if (newTotalLen > varBlockMaker->maxBufferLen)
			return NULL;	// Out of space.

		tempOffset24.byteOffset24 = nextItemOffset;
		memcpy(tempScratchSpace + 
						(currentItemCount * VARBLOCK_BYTE_OFFSET_24_LEN),
			   &tempOffset24, 
			   VARBLOCK_BYTE_OFFSET_24_LEN);
	}

	varBlockMaker->nextItemPtr += itemLen;
	varBlockMaker->currentItemCount++;
	varBlockMaker->currentItemLenSum = newItemLenSum;

	return nextItemPtr;
}

/*
 * Get the variable-length item count.
 */
int VarBlockMakerItemCount(
    VarBlockMaker *varBlockMaker)
{
	Assert(varBlockMaker != NULL);
	
	return varBlockMaker->currentItemCount;
}

/*
 * Finish making the VarBlock.
 *
 * The item-offsets array will be added to the end.
 */
VarBlockByteLen VarBlockMakerFinish(
    VarBlockMaker *varBlockMaker)
{
    uint8 *buffer;
	int itemCount;
	VarBlockByteLen itemLenSum; 
	uint8 *tempScratchSpace;
	VarBlockByteOffset offsetToOffsetArray;
	int z;
	int multiplier;
	VarBlockByteLen offsetArrayLen;
	VarBlockByteLen offsetArrayLenRounded;
	VarBlockByteLen bufferLen;
	
	Assert(varBlockMaker != NULL);

    buffer = (uint8*)varBlockMaker->header;
	itemCount = varBlockMaker->currentItemCount;
	itemLenSum = varBlockMaker->currentItemLenSum;
	
	tempScratchSpace = varBlockMaker->tempScratchSpace;

	/* 
	 * offsetArrays start on even boundary.
	 */
	offsetToOffsetArray = VARBLOCK_HEADER_LEN +
			              ((itemLenSum + 1)/2)* 2;
	
	if (VarBlockGet_offsetsAreSmall(varBlockMaker->header))
	{
		memcpy(&buffer[offsetToOffsetArray], tempScratchSpace, itemCount * 2);

		multiplier = 2;
	}
	else
	{
		memcpy(&buffer[offsetToOffsetArray], 
			   tempScratchSpace, 
			   itemCount * VARBLOCK_BYTE_OFFSET_24_LEN);

		// UNDONE: Zero out odd byte.

		multiplier = VARBLOCK_BYTE_OFFSET_24_LEN;
	}

	/*
	 * Zero the pad between the last item and the offset array for data
	 * security.
	 */
	for (z = VARBLOCK_HEADER_LEN + itemLenSum; z < offsetToOffsetArray; z++)
	{
		buffer[z] = 0;
	}

	VarBlockSet_itemLenSum(varBlockMaker->header,itemLenSum);
	VarBlockSet_itemCount(varBlockMaker->header,itemCount);

	/*
	 * Round-up to even length.
	 */
	offsetArrayLen = itemCount * multiplier;
	offsetArrayLenRounded = ((offsetArrayLen + 1)/2)*2;

	/*
	 * Zero the pad between the last offset and the rounded-up end
	 * of VarBlock for data security.
	 */
	for (z = offsetToOffsetArray + offsetArrayLen;
	     z < offsetToOffsetArray + offsetArrayLenRounded;
		 z++)
	{
		buffer[z] = 0;
	}
	
	bufferLen = offsetToOffsetArray + offsetArrayLenRounded;

//#ifdef DEBUG
//	if(VarBlockIsValid(buffer, bufferLen) != VarBlockCheckOk)
//	{
//		// UNDONE: Use elog.
//		fprintf(stderr, "VarBlockIsValid not ok (detail = '%s')",
//			    VarBlockCheckErrorStr);
//		exit(1);
//	}
//#endif

	return bufferLen;
}

/*
 * Reset the VarBlock maker so it can make another one
 * using the same inputs as given to VarBlockMakerInit.
 */
void VarBlockMakerReset(
    VarBlockMaker *varBlockMaker)
{
    uint8 *buffer;

	Assert(varBlockMaker != NULL);
	
    buffer = (uint8*)varBlockMaker->header;
	
	memset(buffer, 0, VARBLOCK_HEADER_LEN);
	VarBlockSet_version(varBlockMaker->header,InitialVersion);
	VarBlockSet_offsetsAreSmall(varBlockMaker->header,true);
	
    varBlockMaker->currentItemLenSum = 0;

    varBlockMaker->nextItemPtr = &buffer[VARBLOCK_HEADER_LEN];
    varBlockMaker->currentItemCount = 0;
    varBlockMaker->maxItemCount = varBlockMaker->tempScratchSpaceLen / 2;

}

static VarBlockByteOffset VarBlockGetOffset(
	VarBlockHeader 		*header,
	VarBlockByteOffset  offsetToOffsetArray,
    int					itemIndex)
{
	uint8 *offsetArray;

	Assert(header != NULL);
	Assert(offsetToOffsetArray >= VARBLOCK_HEADER_LEN);
	Assert(itemIndex >= 0);
	Assert(itemIndex < VarBlockGet_itemCount(header));

	offsetArray = ((uint8*)header) + offsetToOffsetArray;
	
	if (VarBlockGet_offsetsAreSmall(header))
	{
		return ((VarBlockByteOffset16*)offsetArray)[itemIndex];
	}
	else
	{
		VarBlockByteOffset24 offset24;

		memcpy(&offset24,
			   offsetArray + (itemIndex * VARBLOCK_BYTE_OFFSET_24_LEN),
			   VARBLOCK_BYTE_OFFSET_24_LEN);

		return offset24.byteOffset24;
	}
}

#define MAX_VARBLOCK_CHECK_ERROR_STR 200
static char VarBlockCheckErrorStr[MAX_VARBLOCK_CHECK_ERROR_STR] = "\0";

/*
 * Return a string message for the last check error.
 */
char *VarBlockGetCheckErrorStr(void)
{
	return VarBlockCheckErrorStr;
}

/*
 * Determine if the header looks valid.
 *
 * peekLen must be at least VARBLOCK_HEADER_LEN bytes.
 */
VarBlockCheckError VarBlockHeaderIsValid(
    uint8               *buffer,
    VarBlockByteLen     peekLen)
{
	VarBlockHeader 		*header;
	
	Assert(buffer != NULL);
	Assert(peekLen >= VARBLOCK_HEADER_LEN);

	header = (VarBlockHeader*)buffer;

	if (VarBlockGet_version(header) != InitialVersion)
	{
		sprintf(VarBlockCheckErrorStr,
			    "Invalid initial version %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
			    VarBlockGet_version(header), 
			    header->bytes_0_3, header->bytes_4_7);
		return VarBlockCheckBadVersion;
	}

	if (VarBlockGet_reserved(header) != 0)
	{
		sprintf(VarBlockCheckErrorStr,
			    "Reserved not 0 (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
			    header->bytes_0_3, header->bytes_4_7);
		return VarBlockCheckReservedNot0;
	}

	if (VarBlockGet_moreReserved(header) != 0)
	{
		sprintf(VarBlockCheckErrorStr,
			    "More reserved not 0 (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
			    header->bytes_0_3, header->bytes_4_7);
		return VarBlockCheckMoreReservedNot0;
	}

	/*
	 * Can't do checking of itemSumLen and itemCount without access to whole
	 * VarBlock.
	 */

	return VarBlockCheckOk;
}

/*
 * Determine if the whole VarBlock looks valid.
 *
 * bufferLen must be VarBlock length.
 */
VarBlockCheckError VarBlockIsValid(
    uint8               *buffer,
    VarBlockByteLen     bufferLen)
{
	VarBlockHeader 		*header;
	int headerLen = VARBLOCK_HEADER_LEN;
	VarBlockCheckError	varBlockCheckError;
	int itemLenSum;
	int itemCount;
	VarBlockByteOffset  offsetToOffsetArray;
	int z;
	
	Assert(buffer != NULL);
	Assert(bufferLen >= VARBLOCK_HEADER_LEN);

	header = (VarBlockHeader*)buffer;

	varBlockCheckError = VarBlockHeaderIsValid(buffer, bufferLen);
	if (varBlockCheckError != VarBlockCheckOk)
		return varBlockCheckError;
	
	/*
	 * Now do checking of itemSumLen and itemCount.
	 */
	itemLenSum = VarBlockGet_itemLenSum(header);
	if (itemLenSum >= bufferLen)
	{
		sprintf(VarBlockCheckErrorStr,
				"itemLenSum %d greater than or equal to bufferLen %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
			    itemLenSum, 
			    bufferLen, 
			    header->bytes_0_3, header->bytes_4_7);
		return VarBlockCheckItemSumLenBad1;
	}

	offsetToOffsetArray = VARBLOCK_HEADER_LEN +
			              ((itemLenSum + 1)/2)* 2;
	if (offsetToOffsetArray > bufferLen)
	{
		sprintf(VarBlockCheckErrorStr,
				"offsetToOffsetArray %d greater than bufferLen %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
			    offsetToOffsetArray, 
			    bufferLen, 
			    header->bytes_0_3, header->bytes_4_7);
		return VarBlockCheckItemSumLenBad2;
	}
	
	/*
	 * Verify the data security zero pad between the last item and
	 * the the offset array.
	 */
	for (z = VARBLOCK_HEADER_LEN + itemLenSum; z < offsetToOffsetArray; z++)
	{
		if (buffer[z] != 0)
		{
			sprintf(VarBlockCheckErrorStr,
					"Bad zero pad at offset %d between items and offset array (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
				    z, 
				    header->bytes_0_3, header->bytes_4_7);
			return VarBlockCheckZeroPadBad1;
		}
	}
	
	itemCount = VarBlockGet_itemCount(header);
	if (itemCount == 0)
	{
		if (offsetToOffsetArray != bufferLen)
		{
			sprintf(VarBlockCheckErrorStr,
					"offsetToOffsetArray %d should equal bufferLen %d for itemCount 0 (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
				    offsetToOffsetArray, 
				    bufferLen, 
				    header->bytes_0_3, header->bytes_4_7);
			return VarBlockCheckItemCountBad1;
		}
	}
	else
	{
		int					multiplier;
		VarBlockByteLen		actualOffsetArrayLenRounded;
		VarBlockByteLen		calculatedOffsetArrayLen;
		VarBlockByteLen		calculatedOffsetArrayLenRounded;
		VarBlockByteOffset  offset;
		int					i;
		
		if (VarBlockGet_offsetsAreSmall(header))
		{
			multiplier = 2;
		}
		else
		{
			multiplier = VARBLOCK_BYTE_OFFSET_24_LEN;
		}
		
		actualOffsetArrayLenRounded = bufferLen - offsetToOffsetArray;

		// Since itemCount was from 24-bits and the multipler is small,
		// this multiply should be safe.
		calculatedOffsetArrayLen = itemCount * multiplier;
		calculatedOffsetArrayLenRounded = 
								((calculatedOffsetArrayLen + 1)/2)*2;
		if (actualOffsetArrayLenRounded != calculatedOffsetArrayLenRounded)
		{
			sprintf(VarBlockCheckErrorStr,
					"actual OffsetArray length %d should equal calculated OffsetArray length %d for itemCount %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
				    actualOffsetArrayLenRounded, 
				    calculatedOffsetArrayLenRounded, 
				    itemCount,
				    header->bytes_0_3, header->bytes_4_7);
			return VarBlockCheckItemCountBad2;
		}

		/*
		 * The first offset is right after the header.
		 */
		offset = VarBlockGetOffset(header, offsetToOffsetArray, 0);
		if (offset != VARBLOCK_HEADER_LEN)
		{
			sprintf(VarBlockCheckErrorStr,
					"offset %d at index 0 is bad -- must equal %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
				    offset,
				    headerLen,
				    header->bytes_0_3, header->bytes_4_7);
			return VarBlockCheckOffsetBad1;
		}
		
		for (i = 1; i < itemCount; i++)
		{
			VarBlockByteOffset prevOffset = offset;

			if (offset < prevOffset)
			{
				sprintf(VarBlockCheckErrorStr,
						"offset %d at index %d is bad -- less than previous offset %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
					    offset,
					    i,
					    prevOffset,
					    header->bytes_0_3, header->bytes_4_7);
				return VarBlockCheckOffsetBad2;
			}
			if (offset > itemLenSum + VARBLOCK_HEADER_LEN)
			{
				sprintf(VarBlockCheckErrorStr,
						"offset %d at index %d is bad -- greater than itemLenSum and header %d (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
					    offset,
					    i,
					    itemLenSum + headerLen,
					    header->bytes_0_3, header->bytes_4_7);
				return VarBlockCheckOffsetBad3;
			}
		}
		
		/*
		 * Verify the data security zero pad between the last offset and
		 * the rounded-up end of the VarBlock.
		 */
		for (z = offsetToOffsetArray + calculatedOffsetArrayLen; 
		     z < offsetToOffsetArray + calculatedOffsetArrayLenRounded; 
			 z++)
		{
			if (buffer[z] != 0)
			{
				sprintf(VarBlockCheckErrorStr,
						"Bad zero pad at offset %d between last offset and the rounded-up end of buffer (bytes_0_3 0x%08x, bytes_4_7 0x%08x)",
					    z, 
					    header->bytes_0_3, header->bytes_4_7);
				return VarBlockCheckZeroPadBad2;
			}
		}
	
	}

	return VarBlockCheckOk;
}

/*
 * Given a pointer to a VarBlock with at least VARBLOCK_HEADER_LEN bytes
 * present, return the length of the whole block.
 */
VarBlockByteLen VarBlockLenFromHeader(
    uint8               *buffer,
    VarBlockByteLen     peekLen)
{
	VarBlockHeader 		*header;
    VarBlockByteLen     itemLenSum;
	VarBlockByteOffset  offsetToOffsetArray;
	int 				multiplier;
	VarBlockByteLen     bufferLen;
	int headerLen = VARBLOCK_HEADER_LEN;
	VarBlockByteLen     offsetArrayLen;
	
	Assert(buffer != NULL);
	if(peekLen < headerLen)
	{
		fprintf(stderr,"bufferLen %d minimum %d",
			    peekLen, headerLen);
		exit(1);
	}
	Assert(peekLen >= VARBLOCK_HEADER_LEN);

	header = (VarBlockHeader*)buffer;
	
	itemLenSum = VarBlockGet_itemLenSum(header);

	/* 
	 * Start offsetArrays on even boundary.
	 */
	offsetToOffsetArray = VARBLOCK_HEADER_LEN +
			              ((itemLenSum + 1)/2)* 2;

	if (VarBlockGet_offsetsAreSmall(header))
	{
		multiplier = 2;
	}
	else
	{
		multiplier = VARBLOCK_BYTE_OFFSET_24_LEN;
	}

	/*
	 * Round-up to even length.
	 */
	offsetArrayLen = VarBlockGet_itemCount(header) * multiplier;
	offsetArrayLen = ((offsetArrayLen + 1)/2)*2;
	
	bufferLen = offsetToOffsetArray + offsetArrayLen;

	return bufferLen;
}

/*
 * Initialize the VarBlock reader.
 */
void VarBlockReaderInit(
    VarBlockReader      *varBlockReader,
    uint8               *buffer,
    VarBlockByteLen     bufferLen)
{
	VarBlockHeader *header;
	VarBlockByteLen itemLenSum;
	VarBlockByteOffset offsetToOffsetArray;
	int divisor;
	int calculatedItemCount;
	
	Assert(varBlockReader != NULL);
	Assert(buffer != NULL);								// UNDONE: And ALIGN(4)
	Assert(bufferLen >= VARBLOCK_HEADER_LEN);			// UNDONE: And even.
	
	header = (VarBlockHeader*)buffer;
	
	varBlockReader->header = header;
    varBlockReader->bufferLen = bufferLen;

	itemLenSum = VarBlockGet_itemLenSum(header);
	
	/* 
	 * offsetArrays start on even boundary.
	 */
	offsetToOffsetArray = VARBLOCK_HEADER_LEN +
			              ((itemLenSum + 1)/2)* 2;
	if (VarBlockGet_offsetsAreSmall(header))
	{
		divisor = 2;
	}
	else
	{
		divisor = VARBLOCK_BYTE_OFFSET_24_LEN;
	}
    calculatedItemCount = (bufferLen - offsetToOffsetArray) / divisor;
	Assert(calculatedItemCount == VarBlockGet_itemCount(header));
	
    varBlockReader->offsetToOffsetArray = offsetToOffsetArray;
    varBlockReader->nextIndex = 0;
    varBlockReader->nextItemPtr = &buffer[VARBLOCK_HEADER_LEN];
}

static VarBlockByteLen VarBlockGetItemLen(
    VarBlockReader      *varBlockReader,
    int                 itemIndex)
{
	VarBlockHeader *header;
	VarBlockByteLen itemLen;
	VarBlockByteOffset offset;
	
	Assert(varBlockReader != NULL);
	Assert(itemIndex >= 0);

	header = varBlockReader->header;

	offset = VarBlockGetOffset(
						header, 
						varBlockReader->offsetToOffsetArray, 
						itemIndex);
	if (itemIndex < VarBlockGet_itemCount(header) - 1)
	{
		VarBlockByteOffset nextOffset;

		nextOffset = VarBlockGetOffset(
								header, 
								varBlockReader->offsetToOffsetArray, 
								itemIndex + 1);
		
		itemLen = nextOffset - offset; 
		Assert(itemLen >= 0);
	}
	else
	{
		Assert(itemIndex == VarBlockGet_itemCount(header) - 1);
		
		itemLen = 
			VARBLOCK_HEADER_LEN +
			VarBlockGet_itemLenSum(header) -
			offset;
		Assert(itemLen >= 0);
	}

	return itemLen;
}

static void VarBlockGetItemPtrAndLen(
    VarBlockReader      *varBlockReader,
    int                 itemIndex,
    uint8				**itemPtr,
    VarBlockByteLen		*itemLen)
{
	VarBlockHeader *header;
	uint8 *buffer;
	VarBlockByteOffset offset;
	
	Assert(varBlockReader != NULL);

	header = varBlockReader->header;
	buffer = (uint8*)header;

	offset = VarBlockGetOffset(
						header, 
						varBlockReader->offsetToOffsetArray, 
						itemIndex);
	if (itemIndex < VarBlockGet_itemCount(header) - 1)
	{
		VarBlockByteOffset nextOffset;

		nextOffset = VarBlockGetOffset(
								header, 
								varBlockReader->offsetToOffsetArray, 
								itemIndex + 1);
		
		*itemLen = nextOffset - offset; 
		Assert(*itemLen >= 0);
	}
	else
	{
		Assert(itemIndex == VarBlockGet_itemCount(header) - 1);
		
		*itemLen = 
			VARBLOCK_HEADER_LEN +
			VarBlockGet_itemLenSum(header) -
			offset;
		Assert(*itemLen >= 0);
	}

	*itemPtr = &buffer[offset];
}


/*
 * Get a pointer to the next variable-length item.
 *
 * Returns NULL when there are no more items.
 */
uint8* VarBlockReaderGetNextItemPtr(
    VarBlockReader      *varBlockReader,
    VarBlockByteLen     *itemLen)
{
	uint8 *nextItemPtr;
	
	Assert(varBlockReader != NULL);
	Assert(itemLen != NULL);
	
	if (varBlockReader->nextIndex >= 
			VarBlockGet_itemCount(varBlockReader->header))
	{
		return NULL;
	}
	
	*itemLen = VarBlockGetItemLen(varBlockReader, varBlockReader->nextIndex);
	nextItemPtr = varBlockReader->nextItemPtr;

	varBlockReader->nextItemPtr += *itemLen;
	varBlockReader->nextIndex++;
	
	return nextItemPtr;
}

/*
 * Get the variable-length item count.
 */
int VarBlockReaderItemCount(
    VarBlockReader *varBlockReader)
{
	Assert(varBlockReader != NULL);
	
	return VarBlockGet_itemCount(varBlockReader->header);
}

/*
 * Get a pointer to a variable-length item.
 */
uint8* VarBlockReaderGetItemPtr(
    VarBlockReader      *varBlockReader,
    int                 itemIndex,
    VarBlockByteLen     *itemLen)
{
	uint8 *nextItemPtr;
	
	Assert(varBlockReader != NULL);
	Assert(itemIndex >= 0);
	Assert(itemIndex < VarBlockGet_itemCount(varBlockReader->header));
	
	VarBlockGetItemPtrAndLen(varBlockReader, itemIndex, &nextItemPtr, itemLen);
	
	return nextItemPtr;
}

VarBlockByteLen VarBlockCollapseToSingleItem(
	uint8			*target,
	uint8			*source,
	int32			sourceLen)
{
	VarBlockReader	varBlockReader;
	uint8			*itemPtr;
	VarBlockByteLen	itemLen;
	
	VarBlockReaderInit(
			    &varBlockReader,
			    source,
			    sourceLen);

	Assert(VarBlockReaderItemCount(&varBlockReader) == 1);

	/*
	 * Copy out item, even if it destroys the VarBlock.
	 */
	itemPtr = VarBlockReaderGetItemPtr(
							&varBlockReader,
							/* itemIndex */ 0,
							&itemLen);
	/* 
	 * Slide data back. Since we have overlapping data,
	 * we use memmove which knows how to do that instead of
	 * memcpy which isn't guaranteed to do that right.
	 */
	memmove(target, 
	        itemPtr,
	        itemLen);

	return itemLen;
}

