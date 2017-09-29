/*-------------------------------------------------------------------------
 *
 * cdbsharedoidsearch.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsharedoidsearch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "cdb/cdbshareddoublylinked.h"
#include "cdb/cdbsharedoidsearch.h"

/* ----------------------------------------------------------------------------- */
/*  Free Pool  */
/* ----------------------------------------------------------------------------- */

static void
SharedOidSearch_MakeFreeObjPool(
								SharedOidSearchFreeObjPool *sharedFreeObjPool,
 /* The shared free object pool to initialize. */

								void *freeObjectArray,
 /* The shared-memory to use. */

								int32 freeObjectCount,
 /* The byte length of the shared-memory. */

								int32 objectLen)

 /*
  * The total length of the objects that includes the embedded header
  * SharedOidSearchObjHeader.
  */
{
	int32		i;

	SharedListBase_Init(
						&sharedFreeObjPool->listBase,
						freeObjectArray,
						objectLen,
						offsetof(SharedOidSearchObjHeader, private.links));

	SharedDoublyLinkedHead_Init(&sharedFreeObjPool->freeList);

	/*
	 * Initalize the free objects.
	 */
	for (i = 0; i < freeObjectCount; i++)
	{
		SharedOidSearchObjHeader *ele;

		ele = (SharedOidSearchObjHeader *)
			SharedListBase_ToElement(
									 &sharedFreeObjPool->listBase, i);

		SharedDoubleLinks_Init(&ele->private.links, i);

		SharedDoublyLinkedHead_AddLast(
									   &sharedFreeObjPool->listBase,
									   &sharedFreeObjPool->freeList,
									   ele);
	}
}

/* ----------------------------------------------------------------------------- */
/*  Initialize */
/* ----------------------------------------------------------------------------- */

int32
SharedOidSearch_TableLen(
						 int32 hashSize,
 /* The hash array size */

						 int32 freeObjectCount,

						 int32 objectLen)

 /*
  * The total length of the objects that includes the embedded header
  * SharedOidSearchObjHeader.
  */
{
	return MAXALIGN(offsetof(SharedOidSearchTable, private.buckets)) +
		MAXALIGN(hashSize * sizeof(SharedOidSearchHashBucket)) +
		MAXALIGN(freeObjectCount * objectLen);
}

void
SharedOidSearch_InitTable(
						  SharedOidSearchTable *table,
 /* The shared search tables to initialize. */

						  int32 hashSize,
 /* The hash array size */

						  int32 freeObjectCount,

						  int32 objectLen)

 /*
  * The total length of the objects that includes the embedded header
  * SharedOidSearchObjHeader.
  */
{
	SharedOidSearchHashBucket *bucketArray;
	int32		freePoolObjectArrayOffset;
	void	   *freePoolObjectArray;

	int32		i;

	table->private.hashSize = hashSize;

	bucketArray = &table->private.buckets[0];

	for (i = 0; i < hashSize; i++)
		SharedDoublyLinkedHead_Init(&bucketArray[i].bucketListHead);

	freePoolObjectArrayOffset =
		MAXALIGN(
				 (int32)
				 ((uint8 *) &table->private.buckets[hashSize] - (uint8 *) table));

	freePoolObjectArray =
		(void *)
		(((uint8 *) table) + freePoolObjectArrayOffset);

	SharedOidSearch_MakeFreeObjPool(
									&table->private.freePool,
									freePoolObjectArray,
									freeObjectCount,
									objectLen);

}


/* ----------------------------------------------------------------------------- */
/*  Helpers */
/* ----------------------------------------------------------------------------- */

static SharedOidSearchHashBucket *
SharedOidSearch_GetBucket(
						  SharedOidSearchTable *table,
						  Oid oid1)
{
	int32		bucketNum;

	bucketNum = oid1 % table->private.hashSize;
	return &table->private.buckets[bucketNum];
}

static SharedOidSearchObjHeader *
SharedOidSearch_FindInBucket(
							 SharedOidSearchTable *table,
							 SharedOidSearchHashBucket *bucket,
							 Oid oid1,
							 Oid oid2)
{
	SharedListBase *listBase = &table->private.freePool.listBase;
	SharedDoublyLinkedHead *listHead;
	SharedOidSearchObjHeader *ele;

	listHead = &bucket->bucketListHead;
	ele = (SharedOidSearchObjHeader *)
		SharedDoublyLinkedHead_First(listBase, listHead);
	while (ele != NULL)
	{
		if (ele->oid1 == oid1 && ele->oid2 == oid2)
			return ele;

		ele = (SharedOidSearchObjHeader *)
			SharedDoubleLinks_Next(listBase, listHead, ele);
	}

	return NULL;
}

static void
SharedOidSearch_RemoveFromBucket(
								 SharedOidSearchTable *table,
								 SharedOidSearchHashBucket *bucket,
								 SharedOidSearchObjHeader *ele)
{
	SharedListBase *listBase = &table->private.freePool.listBase;
	SharedDoublyLinkedHead *listHead;

	Assert(ele->private.isDeleted);
	Assert(ele->private.pinCount == 0);

	listHead = &bucket->bucketListHead;

	SharedDoubleLinks_Remove(listBase, listHead, ele);

	SharedDoublyLinkedHead_AddLast(
								   &table->private.freePool.listBase,
								   &table->private.freePool.freeList,
								   ele);
}


/* ----------------------------------------------------------------------------- */
/*  Add, Find, Probe, and Delete */
/* ----------------------------------------------------------------------------- */

SharedOidSearchAddResult
SharedOidSearch_Add(
					SharedOidSearchTable *table,
					Oid oid1,
					Oid oid2,
					SharedOidSearchObjHeader **header)
{
	SharedOidSearchHashBucket *bucket;
	SharedOidSearchObjHeader *ele;

	bucket = SharedOidSearch_GetBucket(table, oid1);

	ele = SharedOidSearch_FindInBucket(table, bucket, oid1, oid2);
	if (ele != NULL)
		return SharedOidSearchAddResult_Exists;

	ele =
		(SharedOidSearchObjHeader *)
		SharedDoublyLinkedHead_RemoveFirst(
										   &table->private.freePool.listBase,
										   &table->private.freePool.freeList);
	if (ele == NULL)
		return SharedOidSearchAddResult_NoMemory;

	ele->private.pinCount = 0;
	ele->private.isDeleted = false;
	ele->oid1 = oid1;
	ele->oid2 = oid2;

	SharedDoublyLinkedHead_AddLast(
								   &table->private.freePool.listBase,
								   &bucket->bucketListHead,
								   ele);

	*header = ele;
	return SharedOidSearchAddResult_Ok;
}

SharedOidSearchObjHeader *
SharedOidSearch_Find(
					 SharedOidSearchTable *table,
					 Oid oid1,
					 Oid oid2)
{
	SharedOidSearchHashBucket *bucket;
	SharedOidSearchObjHeader *ele;

	bucket = SharedOidSearch_GetBucket(table, oid1);

	ele = SharedOidSearch_FindInBucket(table, bucket, oid1, oid2);
	if (ele == NULL)
		return NULL;

	return ele;
}

static bool
SharedOidSearch_NextBucket(
						   SharedOidSearchTable *table,
						   SharedOidSearchHashBucket **bucket)
{
	int32		hashSize = table->private.hashSize;

	if (*bucket == &table->private.buckets[hashSize - 1])
		return false;

	(*bucket)++;
	return true;
}

static bool
SharedOidSearch_FindNonDeletedInBucket(
									   SharedListBase *listBase,
									   SharedDoublyLinkedHead *listHead,
									   SharedOidSearchObjHeader **header)
{
	while (true)
	{
		if (!(*header)->private.isDeleted)
			return true;

		*header =
			(SharedOidSearchObjHeader *)
			SharedDoubleLinks_Next(
								   listBase,
								   listHead,
								   *header);
		if (*header == NULL)
			return false;
	}
}


void
SharedOidSearch_Iterate(
						SharedOidSearchTable *table,
						SharedOidSearchObjHeader **header)
{
	SharedListBase *listBase = &table->private.freePool.listBase;
	SharedOidSearchHashBucket *bucket;
	SharedOidSearchObjHeader *current = *header;

	if (current != NULL)
	{
		/*
		 * Try to get next element in current bucket.
		 */
		bucket = SharedOidSearch_GetBucket(table, current->oid1);

		Assert(current->private.pinCount > 0);
		current->private.pinCount--;

		*header =
			(SharedOidSearchObjHeader *)
			SharedDoubleLinks_Next(
								   listBase,
								   &bucket->bucketListHead,
								   current);

		if (current->private.isDeleted &&
			current->private.pinCount == 0)
			SharedOidSearch_RemoveFromBucket(table, bucket, current);

		if (*header != NULL &&
			SharedOidSearch_FindNonDeletedInBucket(
												   listBase,
												   &bucket->bucketListHead,
												   header))
		{
			(*header)->private.pinCount++;
			return;
		}

		Assert(*header == NULL);

		if (!SharedOidSearch_NextBucket(
										table,
										&bucket))
			return;
		//No more buckets.
	}
	else
		bucket = &table->private.buckets[0];

	/*
	 * Find next non-empty bucket.
	 */
	while (true)
	{
		*header =
			(SharedOidSearchObjHeader *)
			SharedDoublyLinkedHead_First(
										 listBase,
										 &bucket->bucketListHead);
		if (*header != NULL &&
			SharedOidSearch_FindNonDeletedInBucket(
												   listBase,
												   &bucket->bucketListHead,
												   header))
		{
			(*header)->private.pinCount++;
			return;
		}

		Assert(*header == NULL);

		if (!SharedOidSearch_NextBucket(
										table,
										&bucket))
			return;
		//No more buckets.
	}
}

void
SharedOidSearch_ReleaseIterator(
								SharedOidSearchTable *table,
								SharedOidSearchObjHeader **header)
{
	SharedOidSearchObjHeader *current = *header;
	SharedOidSearchHashBucket *bucket;

	Assert(current->private.pinCount > 0);
	current->private.pinCount--;

	if (current->private.isDeleted &&
		current->private.pinCount == 0)
	{
		bucket = SharedOidSearch_GetBucket(table, current->oid1);
		SharedOidSearch_RemoveFromBucket(table, bucket, current);
	}

	*header = NULL;
}

void
SharedOidSearch_Delete(
					   SharedOidSearchTable *table,
					   SharedOidSearchObjHeader *header)
{
	SharedOidSearchHashBucket *bucket;

	Assert(!header->private.isDeleted);
	header->private.isDeleted = true;

	if (header->private.pinCount > 0)
		return;
	//Let the last iterator turn out the light.

		bucket = SharedOidSearch_GetBucket(table, header->oid1);

	SharedOidSearch_RemoveFromBucket(table, bucket, header);
}
