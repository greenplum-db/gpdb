/*-------------------------------------------------------------------------
 * tupchunk.h
 *	   The data-structures and functions for dealing with tuple chunks.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/tupchunk.h
 *-------------------------------------------------------------------------
 */
#ifndef TUPCHUNK_H
#define TUPCHUNK_H

/*--------------*/
/* Tuple Chunks */
/*--------------*/

typedef uint32 TupleChunkType;

#define TC_WHOLE 			((uint32)1 << 20)	/* Contains a whole tuple. */
#define TC_PARTIAL_START 	((uint32)1 << 21)	/* Contains the starting portion of a tuple. */
#define TC_PARTIAL_MID 		((uint32)1 << 22)	/* Contains a middle part of a tuple. */
#define TC_PARTIAL_END 		((uint32)1 << 23)	/* Contains the final portion of a tuple. */
#define TC_END_OF_STREAM 	((uint32)1 << 24)	/* Indicates "end of tuples" from this source. */
#define TC_EMPTY 			((uint32)1 << 25)	/* Empty tuple */
#define TC_MAXVAL 			((uint32)1 << 26)	/* For range checks on type values. */

#define TC_TYPE_BITS		0xFFF00000
#define TC_SIZE_BITS		0x000FFFFF


/* This is the size of a tuple-chunk header, as it appears in the packet that
 * comes from the network.	Thus, some values are packed into 2 bytes or 1
 * byte in this header.  The break-down is as follows:
 *
 *             31           20                          0
 *             ┌────────────┬───────────────────────────┐
 *             └────────────┴───────────────────────────┘
 *               chunk type           chunk size
 *
 *	  Bits Offset	  Description			   Size
 *		00~19	    Tuple Chunk Size		  20 bits
 *		20~31	    Tuple Type				  12 bits
 *	 ------------------------------------------------------
 *							  				TOTAL:  4 BYTES
 *
 * Yes, we could make this smaller. But we're doing lots of memcpy()s
 * of data immediately following these headers. Let's align the data on
 * 32-bit boundaries!
 */

#define TUPLE_CHUNK_HEADER_SIZE 4

/* see MPP-2099, let's not run into this one again! NOTE: the
 * definition of BROADCAST_SEGIDX is *key*.
 *
 * We don't support hash-motion to the QD, so any value that will not
 * appear in our hash-mapping (see nodeMotion.c) will work 
 * 
 * Before changing this value make sure that you look for all uses of
 * BROADCAST_SEGIDX */
#define BROADCAST_SEGIDX		-2 /* to avoid confusion with a QD-content-id, I'll avoid -1 */


#define ANY_ROUTE -100

#define GetSegIdx(x)			(x)
#define IsBroadcastSegIdx(x)	(GetSegIdx(x) == BROADCAST_SEGIDX)

/* Simple macros for accessing tuple-chunk headers; NOTE: we no longer
 * use network byte order */

/* add support for "inplace" chunk items */
#define GetChunkDataPtr(tcItem) \
	(((tcItem)->inplace != NULL) ? ((char *)((tcItem)->inplace)) : ((char *)((tcItem)->chunk_data)))

#define InitializeChunkHeader(/* uint8 * */tc_data) \
	do { uint32 val = 0; memcpy((tc_data), &val, sizeof(uint32)); } while (0)

#define GetChunkHeader(/* uint8 * */tc_data, /* uint32 * */value) \
	do { uint32 val; memcpy(&val, (tc_data), sizeof(uint32)); *(value) = val; } while (0)

#define SetChunkHeader(/* uint8 * */tc_data, /* uint32 */value) \
	do { uint32 val = (value); memcpy((tc_data), &val, sizeof(uint32)); } while (0)

#define GetChunkType(/* uint 8 * */tc_item, /* uint32 * */tc_type) \
	do { uint32 current; GetChunkHeader(GetChunkDataPtr(tc_item), &current); *(tc_type) = current & TC_TYPE_BITS; } while (0)

#define SetChunkType(/* uint8 * */tc_data, /* uint32 */value) \
	do { uint32 current; GetChunkHeader((tc_data), &current); SetChunkHeader((tc_data), current | (value)); } while (0)

#define ClearChunkType(/* uint8 * */tc_data) \
	do { uint32 current; GetChunkHeader((tc_data), &current); SetChunkHeader((tc_data), current & TC_SIZE_BITS); } while (0)

#define GetChunkDataSize(/* uint8 * */tc_data, /* uint32 * */tc_size) \
	do { uint32 current; GetChunkHeader((tc_data), &current); *(tc_size) = current & TC_SIZE_BITS; } while (0)

#define SetChunkDataSize(/* uint8 * */tc_data, /* uint32 */value) \
	do { uint32 current; GetChunkHeader((tc_data), &current); SetChunkHeader((tc_data), current | (value)); } while (0)

#define SetChunkTupleSize(/* uint8 * */tc_data, /* uint32 */value) \
	do { uint32 current = (value); memcpy((tc_data) + TUPLE_CHUNK_HEADER_SIZE, &current, sizeof(uint32)); } while (0)

#define SetChunkTupleContent(/* uint8 * */tc_data, /* uint8 * */tuple, /* uint32 */length) \
	do { memcpy((tc_data) + TUPLE_CHUNK_HEADER_SIZE + sizeof(uint32), (tuple), (length)); } while (0)

#endif   /* TUPCHUNK_H */
