/*-------------------------------------------------------------------------
 *
 * cdbsharedoidsearch.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbsharedoidsearch.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBSHAREDOIDSEARCH_H
#define CDBSHAREDOIDSEARCH_H

#include "cdb/cdbshareddoublylinked.h"


typedef struct SharedOidSearchFreeObjPool
{
	SharedListBase		listBase;

	SharedDoublyLinkedHead		freeList;
} SharedOidSearchFreeObjPool;

typedef struct SharedOidSearchHashBucket
{
	SharedDoublyLinkedHead	bucketListHead;
} SharedOidSearchHashBucket;

typedef struct SharedOidSearchObjHeader
{
	struct
	{
		SharedDoubleLinks	links;

		int16	pinCount;

		bool	isDeleted;
	} private;

	Oid	oid1;
	Oid oid2;

} SharedOidSearchObjHeader;

typedef struct SharedOidSearchTable
{
	struct
	{
		int32		hashSize;

		SharedOidSearchFreeObjPool		freePool;

		SharedOidSearchHashBucket	buckets[1];
	} private;
} SharedOidSearchTable;

typedef enum SharedOidSearchAddResult
{
	SharedOidSearchAddResult_None = 0,
	SharedOidSearchAddResult_Ok = 1,
	SharedOidSearchAddResult_Exists = 2,
	SharedOidSearchAddResult_NoMemory = 3,
	MaxSharedOidSearchAddResult /* must always be last */
} SharedOidSearchAddResult;


// -----------------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------------

extern int32 SharedOidSearch_TableLen(
	int32 hashSize,
				/* The hash array size */

	int32 freeObjectCount,

	int32 objectLen);
				/* 
				 * The total length of the objects that includes the embedded header
				 * SharedOidSearchObjHeader.
				 */
extern void SharedOidSearch_InitTable(
	SharedOidSearchTable *table,
				/* The shared search tables to initialize. */
	
	int32 hashSize,
				/* The hash array size */
	
	int32 freeObjectCount,
	
	int32 objectLen);
				/* 
				 * The total length of the objects that includes the embedded header
				 * SharedOidSearchObjHeader.
				 */

// -----------------------------------------------------------------------------
// Add, Find, Probe, Iterate, and Delete
// -----------------------------------------------------------------------------

extern SharedOidSearchAddResult SharedOidSearch_Add(
	SharedOidSearchTable 		*table,
	Oid 						oid1,
	Oid 						oid2,
	SharedOidSearchObjHeader	**header);

extern SharedOidSearchObjHeader *SharedOidSearch_Find(
	SharedOidSearchTable 	*table,
	Oid 					oid1,
	Oid 					oid2);

extern void SharedOidSearch_Iterate(
	SharedOidSearchTable 		*table,
	SharedOidSearchObjHeader	**header);

extern void SharedOidSearch_ReleaseIterator(
		SharedOidSearchTable		*table,
		SharedOidSearchObjHeader	**header);

extern void SharedOidSearch_Delete(
	SharedOidSearchTable 		*table,
	SharedOidSearchObjHeader	*header);
																																																																
#endif   /* CDBSHAREDOIDSEARCH_H */
