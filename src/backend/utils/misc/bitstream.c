/*------------------------------------------------------------------------------
 *
 * bitstream.c
 *	  A in-memory bitstream implementation.
 *
 * Useful for reading and writing individual bits from a char buffer
 * 
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/bitstream.c
 *
 *------------------------------------------------------------------------------
*/
#include "postgres.h"
#include "utils/bitstream.h"

static bool Bitstream_PutBit(Bitstream* bitstream, uint32 v);
static bool Bitstream_GetBit(Bitstream* bitstream, uint32 *v);

static bool 
Bitstream_CheckForError(Bitstream *bitstream)
{
	if (bitstream->error)
	{
		return false;
	}
	if (bitstream->byteOffset >= bitstream->dataSize)
	{
		bitstream->error = true;
		bitstream->byteOffset = 0;
		return false;
	}
	return true;
}

/*
 * Initializes a new bitstream.
 *
 * The bitstream assumes that the data is zero-initialized. 
 * The bitstream does not allocate memory and does not need a cleanup operation.
 */ 
void
Bitstream_Init(Bitstream *bitstream, unsigned char* data, int dataSize)
{
	bitstream->bitOffset = 0;
	bitstream->byteOffset = 0;
	bitstream->data = data;
	bitstream->dataSize = dataSize;
	bitstream->error = false;
}

static bool
Bitstream_GetBit(Bitstream* bitstream, uint32 *v)
{
	unsigned char c;
	uint32 mask;

	Assert(v);

	if (!Bitstream_CheckForError(bitstream))
		return false;
	
	c = bitstream->data[bitstream->byteOffset];
	mask = 1 << (7 - bitstream->bitOffset);
	(*v) = (c & mask) != 0;
	bitstream->bitOffset++;
	if (bitstream->bitOffset == 8)
	{
		bitstream->bitOffset = 0;
		bitstream->byteOffset++;
	}
	return true;
}

/*
 * Skips the given number of bits in the bitstream.
 * Advances the bitstream cursor.
 *
 * Returns false iff the function call could not succeed.
 */
bool 
Bitstream_Skip(Bitstream* bitstream, int skipBitCount)
{
	uint32 newBitOffset;

	if (!Bitstream_CheckForError(bitstream))
	{
		return false;
	}
	newBitOffset = bitstream->bitOffset + skipBitCount;
	bitstream->bitOffset = newBitOffset % 8;
	bitstream->byteOffset += newBitOffset / 8;
	return true;
}

/*
 * Aligns the bitstream at the given alignment.
 * The alignment needs to be a multiple of 8.
 * Advances the bitstream cursor.
 */
bool 
Bitstream_Align(Bitstream *bitstream, int alignment)
{
	int forAlignment;

	Assert(alignment % 8 == 0);

	forAlignment = Bitstream_GetOffset(bitstream) % alignment;
	if (forAlignment > 0)
	{
		if (!Bitstream_Skip(bitstream, alignment - forAlignment))
			return false;
	}
	return true;
}

/*
 * Returns a pointer to the data aligned to the next given alignment.
 * The pointer can be used to put raw data into the bit stream.
 * However, the caller is then required to ensure that the
 * bitstream has a sufficient size.
 *
 * Advances the bitstream cursor.
 */ 
unsigned char* 
Bitstream_GetAlignedData(Bitstream *bitstream, int alignment)
{
	Bitstream_Align(bitstream, alignment);
	return bitstream->data + bitstream->byteOffset;
}

static bool 
Bitstream_PutBit(Bitstream* bitstream, uint32 v)
{
	uint32 mask;

	if (!Bitstream_CheckForError(bitstream))
	{
		return false;
	}
	if (v)
	{
		mask = 1 << (7 - bitstream->bitOffset);
		bitstream->data[bitstream->byteOffset] |= mask;
	}
	bitstream->bitOffset++;
	if (bitstream->bitOffset == 8)
	{
		bitstream->bitOffset = 0;
		bitstream->byteOffset++;
	}
	return true;
}	

/*
 * returns n bits from the bitstream.
 * Advances the bitstream cursor.
 *
 * Conents of value is undefined if bitstream error flag is true.
 */ 
bool
Bitstream_Get(Bitstream *bitstream, int n, uint32 *value)
{
	uint32 v, tmp;
	int i;

	Assert(bitstream);
	Assert(n >= 1 && n <= 32);
	Assert(value);

	v = 0U;
	for (i = 0; i < n; i++)
	{
		v <<= 1;
		tmp = 0;
		if (!Bitstream_GetBit(bitstream, &tmp))
			return false;
		v |= tmp;
	}
	*value = v;
	return true;
}

/*
 * Writes n bits to the bitstream.
 * Advances the bitstream cursor.
 */ 
bool
Bitstream_Put(Bitstream *bitstream, uint32 v, int bitCount)
{
	uint32 mask, bitValue;
	int i;

	Assert(bitCount >= 1 && bitCount <= 32);
	mask = 1 << (bitCount - 1);

	for (i = 0; i < bitCount; i++)
	{
		bitValue = 0;
		if (v & mask) {
			bitValue = 1;
		}

		if (!Bitstream_PutBit(bitstream, bitValue))
			return false;
		mask = mask >> 1;
	}
	return true;
}

/*
 * returns true iff an error (usually an out of buffer space condition) occurred.
 * If true the content of the bitstream is undefined. However, the bitstream
 * ensures that no read/write outside the provided buffer occurs.
 */ 
bool 
Bitstream_HasError(Bitstream *bitstream)
{
	return bitstream->error;
}

/*
 * Returns the current offset in the bitstream in bits
 */ 
int
Bitstream_GetOffset(Bitstream *bitstream)
{
	return (bitstream->byteOffset * 8) + bitstream->bitOffset;
}

/*
 * Returns the remaining number of bits in the bitstream.
 */ 
int 
Bitstream_GetRemaining(Bitstream *bitstream)
{
	return (bitstream->dataSize * 8) - Bitstream_GetOffset(bitstream);
}

/*
 * returns the number of bytes the bitstream has written data to.
 * The bitstream length can be used to copy the correct part of 
 * the bitstream data.
 *
 * Doesn't advance the bitstream cursor.
 */ 
int 
Bitstream_GetLength(Bitstream *bitstream)
{
	if (bitstream->bitOffset == 0)
	{
		return bitstream->byteOffset;
	}
	else
	{
		return bitstream->byteOffset + 1;
	}
}
