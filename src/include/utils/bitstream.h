/*------------------------------------------------------------------------------
 *
 * bitstream
 *
 * A in-memory bitstream implementation.
 * Useful for reading and writing individual bits from a char buffer
 * 
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/bitstream.h
 *
 *------------------------------------------------------------------------------
*/
#ifndef CDB_BITSTREAM_H
#define CDB_BITSTREAM_H

/*
 * Data structure implementing a bitstream abstraction.
 *
 * A bitstream is used to read and write individual bits from a data buffer
 * conveniently. 
 */
typedef struct Bitstream
{
	/*
	 * Offset within the current byte
	 */ 
	int bitOffset;

	/*
	 * Offset to the start of the data pointer
	 */
	int byteOffset;

	/*
	 * Data buffer to read from or write to.
	 */
	unsigned char* data;

	/*
	 * Size of the data buffer.
	 * The bitstream should never read or write beyond the data size.
	 */ 
	int dataSize;

	/*
	 * Error flag.
	 */ 
	bool error;
} Bitstream;


void Bitstream_Init(Bitstream *bitstream, unsigned char* data, int dataSize);

int Bitstream_GetOffset(Bitstream *bitstream);

bool Bitstream_Get(Bitstream *bitstream, int n, uint32 *value);

bool Bitstream_Put(Bitstream *bitstream, uint32_t v, int bitCount);

bool Bitstream_HasError(Bitstream *bitstream);

bool Bitstream_Skip(Bitstream* bitstream, int skipBitCount);

bool Bitstream_Align(Bitstream *bitstream, int alignment);

unsigned char* Bitstream_GetAlignedData(Bitstream *bitstream, int alignment);

int Bitstream_GetRemaining(Bitstream *bitstream);

int Bitstream_GetLength(Bitstream *bitstream);
#endif
