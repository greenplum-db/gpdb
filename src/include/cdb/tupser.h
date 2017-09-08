/*-------------------------------------------------------------------------
 * tupser.h
 *	   Functions for serializing and deserializing heap tuples.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/tupser.h
 *-------------------------------------------------------------------------
 */
#ifndef TUPSER_H
#define TUPSER_H


#include "access/heapam.h"
#include "cdb/tupchunklist.h"
#include "lib/stringinfo.h"
#include "utils/lsyscache.h"
#include "cdb/tupleremap.h"


typedef struct MotionConn MotionConn;

/*
 * The next two structures are for cached tuple serialization and
 * deserialization information.  This information is cached since there will
 * typically be a LOT of tuples being moved around, and we don't want to look
 * up these details all the time.
 *
 * The cached information itself is typically kept within the Motion Layer's
 * per-motion-node storage, where it is going to be used.
 */

/* Attribute information for sending and receiving.
 *
 * All values are for the binary input and output functions.
 */
typedef struct SerAttrInfo
{
	Oid			atttypid;		/* Oid of the attribute's data-type. */
	int16		typlen;
	bool		typbyval;
}	SerAttrInfo;

/* The information for sending and receiving tuples that match a particular
 * description.
 */
typedef struct SerTupInfo
{
	TupleChunkListCache chunkCache;

	TupleDesc	tupdesc;		/* The attr info we are set up for */

	SerAttrInfo *myinfo;		/* Cached info about each attr */

	/* Preallocated space for deformtuple and formtuple. */
	Datum	   *values;
	bool	   *nulls;

	/* true if tupdesc contains record types */
	bool		has_record_types;
}	SerTupInfo;

/*
 * forward declaration to avoid #including cdbmotion.h here, which would create a circular
 * dependency
 */
struct directTransportBuffer;

/* Populate a SerTupInfo struct with information looked up from the specified
 * tuple-descriptor.
 */
extern void InitSerTupInfo(TupleDesc tupdesc, SerTupInfo *pSerInfo);

/* Free up storage in a previously initialized SerTupInfo struct. */
extern void CleanupSerTupInfo(SerTupInfo *pSerInfo);

/* Convert RecordCache into chunks ready to send out, in one pass */
extern void SerializeRecordCacheIntoChunks(SerTupInfo *pSerInfo,
										   TupleChunkList tcList,
										   MotionConn *conn);

/* Convert a HeapTuple into chunks ready to send out, in one pass */
extern void SerializeTupleIntoChunks(HeapTuple tuple, SerTupInfo *pSerInfo, TupleChunkList tcList);

/* Convert a HeapTuple into chunks directly in a set of transport buffers */
extern int SerializeTupleDirect(HeapTuple tuple, SerTupInfo *pSerInfo, struct directTransportBuffer *b);

/* Deserialize a HeapTuple's data from a byte-array. */
extern HeapTuple DeserializeTuple(SerTupInfo * pSerInfo, StringInfo serialTup);

/* Convert a sequence of chunks containing serialized tuple data into a
 * HeapTuple.
 */
extern HeapTuple CvtChunksToHeapTup(TupleChunkList tclist, SerTupInfo * pSerInfo, TupleRemapper *remapper);

#endif   /* TUPSER_H */
