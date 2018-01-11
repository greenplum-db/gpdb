/*-------------------------------------------------------------------------
 *
 * syncrefhashtable.c
 *  A synchronized, ref-counted hashtable in shared memory,
 *  built on top of dynahash.c
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/cache/syncrefhashtable.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/syncrefhashtable.h"
#include "port/atomics.h"
#include "storage/shmem.h"

/*
 * A synchronized ref-counted hashtable descriptor
 */
struct SyncHT
{
	HTAB *ht; /* The underlying unsynchronized hashtable */
	long numPartitions; /* number of partitions the hashtable is divided into */
	LWLockId baseLWLockId; /* LockId of the first LW Lock to be used for locking partitions */
	ptrdiff_t keyOffset; /* offset in the payload where the key is located */
	ptrdiff_t pinCountOffset; /* offset in the payload where the pincount is located */
	SyncHTEntryIsEmptyFunc isEmptyEntry; /* callback to determine if an entry can be deleted */
	SyncHTEntryInitFunc initEntry; /* callback to initialize an empty entry before returning it to caller */
};

static LWLockId SyncHTPartLockId(SyncHT *syncHT, void *key);
static int32 SyncHTAddRef(SyncHT *syncHT, void *entry);
static int32 SyncHTDecRef(SyncHT *syncHT, void *entry);

/*
 * Creates a new synchronized refcounted hashtable object. The new SyncHT
 * instance is palloc-ed in the current memory context.
 *
 * All fields in syncHTCtl are required.
 *
 * Returns NULL if it could not create or attach to a hashtable.
 */
SyncHT *
SyncHTCreate(SyncHTCtl *syncHTCtl)
{
	Assert(NULL != syncHTCtl);
	Assert(NULL != syncHTCtl->isEmptyEntry);
	Assert(NULL != syncHTCtl->initEntry);
	Assert(NullLock != syncHTCtl->baseLWLockId);
	Assert(0 != syncHTCtl->numPartitions);

	SyncHT *syncHT = (SyncHT *) palloc(sizeof(SyncHT));

	/* Initialize underlying hashtable control structure with provided values */
	HASHCTL hctl;
	hctl.keysize = syncHTCtl->keySize;
	hctl.entrysize = syncHTCtl->entrySize;
	hctl.hash = syncHTCtl->hash;
	hctl.keycopy = syncHTCtl->keyCopy;
	hctl.match = syncHTCtl->match;
	hctl.num_partitions = syncHTCtl->numPartitions;

	int hashFlags = HASH_ELEM | HASH_FUNCTION | HASH_PARTITION | HASH_COMPARE | HASH_KEYCOPY;

	/* Create underlying hashtable in shared memory (or attach to an existing one) */
	syncHT->ht = ShmemInitHash(syncHTCtl->tabName,
			syncHTCtl->numElements, /* init_size */
			syncHTCtl->numElements, /* max_size */
			&hctl, hashFlags);

	if (syncHT->ht == NULL)
	{
		/* Could not initialize the underlying hashtable */
		pfree(syncHT);
		return NULL;
	}

	syncHT->numPartitions = syncHTCtl->numPartitions;
	syncHT->baseLWLockId = syncHTCtl->baseLWLockId;
	syncHT->pinCountOffset = syncHTCtl->pinCountOffset;
	syncHT->keyOffset = syncHTCtl->keyOffset;
	syncHT->isEmptyEntry = syncHTCtl->isEmptyEntry;
	syncHT->initEntry = syncHTCtl->initEntry;

	return syncHT;
}

/*
 * Free up memory used by the hashtable.
 *
 * Only structures allocated in the client's memory context are freed up,
 * actual entries and hashtable structure remains valid in shared memory.
 */
void
SyncHTDestroy(SyncHT *syncHT)
{
	Assert(NULL != syncHT);
	Assert(NULL != syncHT->ht);

	pfree(syncHT->ht);
	pfree(syncHT);
}

/*
 * Returns the number of elements in the hashtable.
 *
 * This function is not synchronized.
 */
long
SyncHTNumEntries(SyncHT *syncHT)
{
	Assert(NULL != syncHT);
	Assert(NULL != syncHT->ht);

	return hash_get_num_entries(syncHT->ht);
}

/*
 * For a given key, compute the hash value and return the id of the lock
 * protecting the corresponding partition.
 */
static LWLockId
SyncHTPartLockId(SyncHT *syncHT, void *key)
{
	Assert(NULL != syncHT);
	Assert(NULL != syncHT->ht);
	Assert(NULL != key);

	uint32 hashValue = get_hash_value(syncHT->ht, key);
	return (syncHT->baseLWLockId + (hashValue % syncHT->numPartitions));
}

/*
 * Inserts a new entry with a given key in the hashtable.
 *
 * Returns pointer to newly added entry when successful.
 *
 * If entry with given key already existed in the hashtable, set existing flag
 * to true and return existing entry.
 *
 * If hashtable is full, return NULL.
 *
 * This function is synchronized. Returned entry is AddRef'ed and needs to be
 * released.
 */
void *
SyncHTInsert(SyncHT *syncHT, void *key, bool *existing)
{
	Assert(NULL != syncHT);
	Assert(NULL != syncHT->ht);
	Assert(NULL != key);

	LWLockId partitionLock = SyncHTPartLockId(syncHT, key);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	*existing = false;
	void *entry = hash_search(syncHT->ht, key, HASH_ENTER_NULL, existing);

	if (entry == NULL)
	{
		/* Hashtable ran out of memory and could not insert */
		LWLockRelease(partitionLock);
		return NULL;
	}

	if (! *existing)
	{
		/* Newly inserted entry, initialize the contents before touching */
		syncHT->initEntry(entry);
	}

	/* AddRef the entry, even if an existing one was found */
	SyncHTAddRef(syncHT, entry);

	LWLockRelease(partitionLock);

	return entry;
}

/*
 * Looks up an entry with a given key in the hashtable.
 * Returns pointer to the entry if found, NULL otherwise.
 *
 * This function is synchronized. Returned entry is AddRef'ed and needs to
 * be released.
 */
void *
SyncHTLookup(SyncHT *syncHT, void *key)
{
	Assert(NULL != syncHT);
	Assert(NULL != key);

	LWLockId partitionLock = SyncHTPartLockId(syncHT, key);

	LWLockAcquire(partitionLock, LW_SHARED);

	bool existing = false;
	void *entry = hash_search(syncHT->ht, key, HASH_FIND, &existing);

	AssertImply(entry != NULL, existing);

	/* AddRef the entry if found */
	if (entry != NULL)
	{
		SyncHTAddRef(syncHT, entry);
	}

	LWLockRelease(partitionLock);

	return entry;
}

/*
 * Releases a previously looked-up or inserted element.
 *
 * Returns true if the element is actually removed from the hashtable, false otherwise.
 */
bool
SyncHTRelease(SyncHT *syncHT, void *entry)
{
	Assert(NULL != syncHT);
	Assert(NULL != entry);

	/* Point to key in the entry */
	void *key = (void *) ((char *) entry + syncHT->keyOffset);

	LWLockId partitionLock = SyncHTPartLockId(syncHT, key);

	LWLockAcquire(partitionLock, LW_EXCLUSIVE);

	int32 pinCount = SyncHTDecRef(syncHT, entry);

	Assert(pinCount >= 0 && "Attempting to release hashtable entry not owned");

	bool deleted = false;
	if (pinCount == 0 && syncHT->isEmptyEntry(entry))
	{
		/* This element is safe for deleting from the hashtable */

#ifdef USE_ASSERT_CHECKING
		void *deletedEntry =
#endif
		hash_search(syncHT->ht, key, HASH_REMOVE, NULL /* foundPtr */);

		Assert(deletedEntry == entry);

		deleted = true;
	}

	LWLockRelease(partitionLock);

	return deleted;
}

/*
 * Atomically increments the pincount on a given entry.
 * Returns the value of the pincount after the increment.
 */
static int32
SyncHTAddRef(SyncHT *syncHT, void *entry)
{
	int32 *pinCountPtr = (int32 *) ((char *) entry + syncHT->pinCountOffset);
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *)pinCountPtr,1);
	return *pinCountPtr;
}

/*
 * Atomically decrements the pincount on a given entry.
 * Returns the value of the pincount after the decrement.
 */
static int32
SyncHTDecRef(SyncHT *syncHT, void *entry)
{
	int32 *pinCountPtr = (int32 *) ((char *) entry + syncHT->pinCountOffset);
	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)pinCountPtr,1);
	return *pinCountPtr;
}
