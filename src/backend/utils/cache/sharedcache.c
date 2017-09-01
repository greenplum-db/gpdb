/*-------------------------------------------------------------------------
 *
 * sharedcache.c
 *	 A generic synchronized cache implementation that lives in the
 *	 shared memory.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/cache/sharedcache.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "port/atomics.h"
#include "utils/memutils.h"
#include "utils/sharedcache.h"

/* Suffix used to generate shared memory hashtable name from cache name */
#define CACHE_HASHTABLE_SUFFIX "_HASHTABLE"

static uint32 Cache_EntryAddRef(Cache *cache, CacheEntry *entry);
static uint32 Cache_EntryDecRef(Cache *cache, CacheEntry *entry);
static CacheEntry *Cache_GetFreeElement(Cache *cache);
static void Cache_ReleaseAcquired(Cache *cache, CacheEntry *entry, bool unregisterCleanup);
static void Cache_ReleaseCached(Cache *cache, CacheEntry *entry, bool unregisterCleanup);

/*
 * Global timestamp used for timing cache operations
 *
 * Used by Cache_TimedOperationStart and Cache_TimedOperationRecord
 */
static instr_time timedOpStart;

/*
 * Callback function to test if an anchor in the hashtable is "empty".
 *
 * This function is not synchronized. We assume the caller is holding a lock
 * on the entry.
 *
 */
static bool
Cache_IsAnchorEmpty(const void *entry)
{
	Assert(NULL != entry);

	CacheAnchor *anchor = (CacheAnchor *) entry;
	return anchor->firstEntry == NULL;
}

/*
 * Callback function to clear an anchor after inserting it into the hashtable,
 * before returning it to the client.
 *
 * This function is not synchronized. The caller must hold a lock on the anchor
 */
static void
Cache_InitAnchor(void *entry)
{
	Assert(NULL != entry);

	CacheAnchor *anchor = (CacheAnchor *) entry;

	SpinLockInit(&anchor->spinlock);
	anchor->firstEntry = NULL;
	anchor->lastEntry = NULL;
	anchor->pinCount = 0;
}

/*
 * Initialize the anchor hashtable component of the cache. Allocates
 * or attaches to an existing hashtable in shared memory.
 */
static void
Cache_InitHashtable(CacheCtl *cacheCtl, Cache *cache)
{
	Assert(NULL != cache);
	Assert(NULL != cacheCtl);
	Assert(NULL != cacheCtl->cacheName);

	SyncHTCtl syncHTCtl;
	MemSet(&syncHTCtl, 0, sizeof(syncHTCtl));

	/* Initialize fields for the anchor hashtable */

	/* Storing only hash codes in hashtable */
	syncHTCtl.keySize = sizeof(uint32);
	syncHTCtl.hash = int32_hash;
	syncHTCtl.keyCopy = memcpy;
	syncHTCtl.match = memcmp;

	/* Entries in the hashtable are CacheAnchors */
	syncHTCtl.entrySize = sizeof(CacheAnchor);

	/* Partitions and associated locks */
	syncHTCtl.numPartitions = cacheCtl->numPartitions;
	syncHTCtl.baseLWLockId = cacheCtl->baseLWLockId;

	/* Name of the hashtable is the name of the cache + CACHE_HASHTABLE_SUFFIX */
	uint32 nameLength = strlen(cacheCtl->cacheName) + strlen(CACHE_HASHTABLE_SUFFIX) + 1;
	char *hashName = (char *) palloc(nameLength);
	snprintf(hashName, nameLength, "%s%s", cacheCtl->cacheName, CACHE_HASHTABLE_SUFFIX);
	syncHTCtl.tabName = hashName;

	/* Create anchor hashtable with the same number of elements as the cache */
	syncHTCtl.numElements = cacheCtl->maxSize;

	/* Offsets to fields in the entry required by the SyncHashtable */
	syncHTCtl.keyOffset = GPDB_OFFSET(CacheAnchor, hashvalue);
	syncHTCtl.pinCountOffset = GPDB_OFFSET(CacheAnchor, pinCount);

	/* Callbacks for SyncHashtable to manipulate anchor entries */
	syncHTCtl.isEmptyEntry = Cache_IsAnchorEmpty;
	syncHTCtl.initEntry = Cache_InitAnchor;

	/* Create or attach to the anchor hashtable */
	cache->syncHashtable = SyncHTCreate(&syncHTCtl);
	Assert(cache->syncHashtable != NULL);

	pfree(hashName);
}

/*
 * Initialize a new CacheEntry structure to initial values
 */
static void
Cache_InitCacheEntry(Cache *cache, CacheEntry *entry)
{
	SpinLockInit(&entry->spinlock);
	entry->state = CACHE_ENTRY_FREE;
	entry->pinCount = 0;
	entry->size = 0L;

#ifdef USE_ASSERT_CHECKING
			Cache_MemsetPayload(cache, entry);
			MemSet(&entry->hashvalue, CACHE_MEMSET_BYTE_PATTERN, sizeof(entry->hashvalue));
#endif
}

/*
 * Reset the statistics data structure.
 */
static void
Cache_ResetStats(Cache_Stats *stats)
{
	MemSet(stats, 0, sizeof(Cache_Stats));
}

/*
 * Allocates the shared control for the cache, or attach to
 * an existing one in the memory. This includes the freelist.
 */
static void
Cache_InitSharedMem(CacheCtl *cacheCtl, Cache *cache)
{
	Assert(NULL != cache);

	Size entrySize = CACHE_ENTRY_HEADER_SIZE + MAXALIGN(cacheCtl->entrySize);
	Size cacheTotalSize = MAXALIGN(sizeof(CacheHdr)) + cacheCtl->maxSize * entrySize;
	bool attach = false;

	/* Allocate or attach to existing cache header */
	cache->cacheHdr = (CacheHdr *) ShmemInitStruct(cache->cacheName, cacheTotalSize, &attach);

	if (!attach)
	{
		/*
		 * If freelist was never allocated, allocate it here.
		 * Set up links.
		 * Identify the HEAD.
		 */

		cache->cacheHdr->entryArray = (void *) (((char *) cache->cacheHdr) + MAXALIGN(sizeof(CacheHdr)));
		cache->cacheHdr->nEntries = cacheCtl->maxSize;
		cache->cacheHdr->keySize = cacheCtl->keySize;
		cache->cacheHdr->keyOffset = cacheCtl->keyOffset;
		cache->cacheHdr->entrySize = cacheCtl->entrySize;
		SpinLockInit(&cache->cacheHdr->spinlock);

		Cache_ResetStats(&cache->cacheHdr->cacheStats);
		cache->cacheHdr->cacheStats.noFreeEntries = cacheCtl->maxSize;

		/* Initialize freeList linked list */
		CacheEntry *firstEntry = (CacheEntry *) cache->cacheHdr->entryArray;
		CacheEntry *prevEntry = NULL;
		CacheEntry *tmpEntry = firstEntry;
		int i=0;
		for (i=0;i<cacheCtl->maxSize;i++)
		{
			Cache_InitCacheEntry(cache, tmpEntry);

			tmpEntry->nextEntry = prevEntry;
			prevEntry = tmpEntry;
			tmpEntry = (CacheEntry *) (((char *) tmpEntry) + entrySize);
		}
		cache->cacheHdr->freeList = prevEntry;
	}
}

/*
 * Computes shared memory size needed for this cache
 */
Size
Cache_SharedMemSize(uint32 nEntries, uint32 entryPayloadSize)
{
	Size size = 0;

	/* Size of anchor hashtable. It has the same number of entries as the cache */
	size = add_size(size, hash_estimate_size(nEntries, sizeof(CacheAnchor)));

	/* Size of cache shared control header */
	size = add_size(size, MAXALIGN(sizeof(CacheHdr)));

	Size entrySize = add_size(CACHE_ENTRY_HEADER_SIZE, MAXALIGN(entryPayloadSize));
	size = add_size(size, mul_size(nEntries, entrySize));

	return size;
}

/*
 * Releases/Surrenders all entries held by this client
 *
 * During normal functionality, client should release all the owned entries
 * and this is a no-op. In case of a client error, this callback makes sure all
 * entries are returned to the cache.
 *
 * This function operates on client data and does not need to be synchronized.
 */
void
Cache_SurrenderClientEntries(Cache *cache)
{
	Assert(NULL != cache);

	uint32 nAcquiredEntries = 0;
	uint32 nCachedEntries = 0;
	Dlelem *elt = NULL;

	/* Surrender all owned entries */
	while (NULL != (elt = DLRemHead(cache->ownedEntries)))
	{
		CacheEntry *entry = DLE_VAL(elt);

		switch(entry->state)
		{
			case CACHE_ENTRY_ACQUIRED:
				Cache_ReleaseAcquired(cache, entry, false /* unregisterCleanup */);
				nAcquiredEntries++;
				break;
			case CACHE_ENTRY_CACHED:
			case CACHE_ENTRY_DELETED:
				Cache_ReleaseCached(cache, entry, false /* unregisterCleanup */);
				nCachedEntries++;
				break;
			default:
				Assert(false && "unexpected cache entry state");
		}

		/* Free linked list element */
		DLFreeElem(elt);
	}

	if (nAcquiredEntries > 0 || nCachedEntries > 0)
	{
		elog(gp_workfile_caching_loglevel, "Cleanup released %u acquired and %u cached entries from client",
			nAcquiredEntries, nCachedEntries);
	}
}

/*
 * Register entry for cleanup. When the clients ends, all the entries still
 * in the lists are released to the cache
 *
 * This is done to ensure all entries are released to the cache even if the
 * client throws an exception
 *
 */
static void
Cache_RegisterCleanup(Cache *cache, CacheEntry *entry, bool isCachedEntry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	MemoryContext oldcxt = MemoryContextSwitchTo(TopMemoryContext);
	DLAddHead(cache->ownedEntries, DLNewElem(entry));
	MemoryContextSwitchTo(oldcxt);
}

/*
 * Unregister entry from cleanup list of the cache
 *
 */
static void
Cache_UnregisterCleanup(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	Dlelem *crtElem = DLGetHead(cache->ownedEntries);
	while (NULL != crtElem && DLE_VAL(crtElem) != entry)
	{
		crtElem = DLGetSucc(crtElem);
	}

	Assert(NULL != crtElem && "could not locate element");

	/* Found matching element. Remove and free. Note that entry is untouched */
	DLRemove(crtElem);
	DLFreeElem(crtElem);

}

/*
 * Create a new cache instance, or attach to an existing one in shared
 * memory.
 *
 * The cache descriptor is allocated in the top memory context since
 * we need it for entry cleanup in case of error
 */
Cache *
Cache_Create(CacheCtl *cacheCtl)
{
	Assert(NULL != cacheCtl);
	Assert(0 != cacheCtl->maxSize);
	Assert(0 != cacheCtl->keySize);
	Assert(0 != cacheCtl->entrySize);
	Assert(NULL != cacheCtl->keyCopy);
	Assert(NULL != cacheCtl->hash);
	Assert(NULL != cacheCtl->match);
	Assert(NULL != cacheCtl->cleanupEntry);

	MemoryContext oldcxt;
	/*
	 * Create Cache in the TopMemoryContext since this memory context
	 * is still available when calling the transaction callback at the
	 * time when the transaction aborts
	 */
	oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	Cache *cache = (Cache *) palloc0(sizeof(Cache));

	cache->keyCopy = cacheCtl->keyCopy;
	cache->hash = cacheCtl->hash;
	cache->match = cacheCtl->match;
	cache->cleanupEntry = cacheCtl->cleanupEntry;
	cache->populateEntry = cacheCtl->populateEntry;
	/* Create new linked lists in top memory context for cleanup */
	cache->ownedEntries = DLNewList();
	cache->cacheName = pstrdup(cacheCtl->cacheName);
	Cache_InitHashtable(cacheCtl, cache);
	Cache_InitSharedMem(cacheCtl, cache);

	INSTR_TIME_SET_ZERO(timedOpStart);

	oldcxt = MemoryContextSwitchTo(oldcxt);

	return cache;
}

void
Cache_Free(Cache *cache)
{
	Assert(false);
	/* FIXME We need to implement this, especially since we're allocating
	 * in the top memory context, can't have memory leaks!
	 */
}

/*
 * Retrieve a new cache entry from the pre-allocated freelist.
 * The client has to either insert the entry in the cache or surrender it.
 *
 * This function calls the populateEntry callback function to populate the
 * entry before returning it to the client.
 *
 * populate_param is the opaque parameter to be passed to the populateEntry function.
 *
 * Return NULL if freelist is empty.
 *
 */
CacheEntry *
Cache_AcquireEntry(Cache *cache, void *populate_param)
{
	Assert(NULL != cache);

	CacheEntry *newEntry = Cache_GetFreeElement(cache);
	if (NULL == newEntry)
	{
		return NULL;
	}

	CACHE_ASSERT_WIPED(newEntry);

	uint32 expected = CACHE_ENTRY_FREE;
#ifdef USE_ASSERT_CHECKING
	int32 casResult =
#endif
	pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&newEntry->state, &expected, CACHE_ENTRY_RESERVED);
	Assert(1 == casResult);

	/*
	 * In RESERVED state nobody else will try to read this entry, not even
	 * the views. No need to lock the entry while populating.
	 */

	if (cache->populateEntry)
	{
		cache->populateEntry(CACHE_ENTRY_PAYLOAD(newEntry), populate_param);
	}

	expected = CACHE_ENTRY_RESERVED;
#ifdef USE_ASSERT_CHECKING
	casResult =
#endif
	pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&newEntry->state, &expected, CACHE_ENTRY_ACQUIRED);
	Assert(1 == casResult);

	Cache_RegisterCleanup(cache, newEntry, false /* isCachedEntry */ );

	return newEntry;
}


#ifdef USE_ASSERT_CHECKING
/*
 * MemSet the payload of an entry with a pattern to prevent a client from
 * accidentally using a surrendered entry's payload.
 */
void
Cache_MemsetPayload(Cache *cache, CacheEntry *entry)
{
	void *payload = CACHE_ENTRY_PAYLOAD(entry);
	MemSet(payload, CACHE_MEMSET_BYTE_PATTERN, cache->cacheHdr->entrySize);
}
#endif

/*
 * Return a previously acquired entry to the cache freelist.
 * Calls the client-specific cleanup before returning to the freelist.
 *
 * Unregisters the entry from the cleanup list if requested.
 */
static void
Cache_ReleaseAcquired(Cache *cache, CacheEntry *entry, bool unregisterCleanup)
{
	Assert(NULL != cache);
	Assert(NULL != entry);
	Assert(CACHE_ENTRY_ACQUIRED == entry->state);

	/* Unregister entry from the cleanup list if requested */
	if (unregisterCleanup)
	{
		Cache_UnregisterCleanup(cache, entry);
	}

	PG_TRY();
	{
		/* Call client-specific cleanup function before removing entry from cache */
		cache->cleanupEntry(CACHE_ENTRY_PAYLOAD(entry));
	}
	PG_CATCH();
	{

		/* Grab entry lock to ensure exclusive access to it while we're touching it */
		Cache_LockEntry(cache, entry);

		/* No need for atomic operations as long as we hold the entry lock */
		entry->state = CACHE_ENTRY_FREE;

#ifdef USE_ASSERT_CHECKING
		Cache_MemsetPayload(cache, entry);
#endif

		Cache_UnlockEntry(cache, entry);

		/* Link entry back in the freelist */
		Cache_AddToFreelist(cache, entry);

		PG_RE_THROW();
	}
	PG_END_TRY();

	/* Grab entry lock to ensure exclusive access to it while we're touching it */
	Cache_LockEntry(cache, entry);

	/* No need for atomic operations as long as we hold the entry lock */
	entry->state = CACHE_ENTRY_FREE;

#ifdef USE_ASSERT_CHECKING
	Cache_MemsetPayload(cache, entry);
#endif

	Cache_UnlockEntry(cache, entry);

	Cache_AddToFreelist(cache, entry);

	Cache_DecPerfCounter(&cache->cacheHdr->cacheStats.noAcquiredEntries, 1 /* delta */ );
}

/*
 * Retrieve a new entry from the freelist
 *
 * Returns NULL if no free entries remain
 */
static CacheEntry *
Cache_GetFreeElement(Cache *cache)
{
	CacheHdr *cacheHdr = cache->cacheHdr;

	/* Must lock to touch freeList */
	SpinLockAcquire(&cacheHdr->spinlock);

	if (cacheHdr->freeList == NULL)
	{
		SpinLockRelease(&cacheHdr->spinlock);
		return NULL;
	}

	CacheEntry *newEntry = cacheHdr->freeList;
	Assert(newEntry->state == CACHE_ENTRY_FREE);

	cacheHdr->freeList = cacheHdr->freeList->nextEntry;

	Cache_DecPerfCounter(&cacheHdr->cacheStats.noFreeEntries, 1 /* delta */ );
	Cache_AddPerfCounter(&cacheHdr->cacheStats.noAcquiredEntries, 1 /* delta */);

	SpinLockRelease(&cacheHdr->spinlock);

	return newEntry;
}

/*
 * Link an entry back in the cache freelist
 *
 * The entry must be already marked as free by the caller.
 */
void
Cache_AddToFreelist(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);
	CACHE_ASSERT_WIPED(entry);
	Assert(entry->state == CACHE_ENTRY_FREE);

	CacheHdr *cacheHdr = cache->cacheHdr;

	/* Must lock to touch freeList */
	SpinLockAcquire(&cacheHdr->spinlock);

	entry->nextEntry = cacheHdr->freeList;
	cacheHdr->freeList = entry;
	Cache_AddPerfCounter(&cacheHdr->cacheStats.noFreeEntries, 1 /* delta */);

	SpinLockRelease(&cacheHdr->spinlock);
}

/*
 * Compute the hashcode for a given cache entry.
 * Uses the hash function specified in the cache.
 */
static void
Cache_ComputeEntryHashcode(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	void *payload = CACHE_ENTRY_PAYLOAD(entry);
	void *key = (void *) ((char *) payload + cache->cacheHdr->keyOffset);
	entry->hashvalue = cache->hash(key, cache->cacheHdr->keySize);
}

/*
 * Inserts a previously acquired entry in the cache.
 *
 * This function should never fail.
 */
void
Cache_Insert(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	Cache_Stats *cacheStats = &cache->cacheHdr->cacheStats;
	Cache_TimedOperationStart();
	Cache_AddPerfCounter(&cacheStats->noInserts, 1 /* delta */);
	Cache_AddPerfCounter(&cacheStats->noCachedEntries, 1 /* delta */);
	Cache_DecPerfCounter(&cacheStats->noAcquiredEntries, 1 /* delta */);
	Cache_AddPerfCounter64(&cacheStats->totalEntrySize, entry->size);

	Cache_ComputeEntryHashcode(cache, entry);

	/* Look up or insert anchor element for this entry */
	bool existing = false;
	volatile CacheAnchor *anchor = SyncHTInsert(cache->syncHashtable, &entry->hashvalue, &existing);
	/*
	 * This should never happen since the SyncHT has as many entries as the SharedCache,
	 * and we'll run out of SharedCache entries before we fill up the SyncHT
	 */
	insist_log(NULL != anchor, "Could not insert in the cache: SyncHT full");

	/* Acquire anchor lock to touch the chain */
	SpinLockAcquire(&anchor->spinlock);

	if (NULL == anchor->firstEntry)
	{
		Assert(NULL == anchor->lastEntry);
		anchor->firstEntry = anchor->lastEntry = entry;
	}
	else
	{
		Assert(NULL != anchor->lastEntry);
		anchor->lastEntry->nextEntry = entry;
		anchor->lastEntry = entry;
	}
	entry->nextEntry = NULL;

	Cache_EntryAddRef(cache, entry);

	uint32 expected = CACHE_ENTRY_ACQUIRED;
#ifdef USE_ASSERT_CHECKING
	int32 casResult =
#endif
	pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&entry->state, &expected, CACHE_ENTRY_CACHED);
	Assert(1 == casResult);
	Assert(NULL != anchor->firstEntry && NULL != anchor->lastEntry);

	SpinLockRelease(&anchor->spinlock);

#ifdef USE_ASSERT_CHECKING
	bool deleted = 
#endif
	SyncHTRelease(cache->syncHashtable, (void *) anchor);
	Assert(!deleted);

	Cache_TimedOperationRecord(&cacheStats->timeInserts,
			&cacheStats->maxTimeInsert);
}

/*
 * Unlink a cache entry from the chain anchored at a CacheAnchor.
 *
 * This function is not synchronized. The caller must hold the spinlock at
 * the anchor.
 */
void
Cache_UnlinkEntry(Cache *cache, CacheAnchor *anchor, CacheEntry *entry)
{
	Assert(NULL != entry);
	Assert(NULL != anchor);
	Assert(NULL != anchor->firstEntry);

	Cache_DecPerfCounter64(&cache->cacheHdr->cacheStats.totalEntrySize, entry->size);

	/* Easy case: Remove first element */
	if (anchor->firstEntry == entry)
	{
		anchor->firstEntry = anchor->firstEntry->nextEntry;
		if (NULL == anchor->firstEntry)
		{
			anchor->lastEntry = NULL;
		}
		return;
	}

	CacheEntry *crtEntry = anchor->firstEntry;
	CacheEntry *prevEntry = NULL;

	while (entry != crtEntry && NULL != crtEntry)
	{
		prevEntry = crtEntry;
		crtEntry = crtEntry->nextEntry;
	}
	Assert(crtEntry == entry);
	Assert(NULL != prevEntry);
	Assert(prevEntry->nextEntry == crtEntry);

	/* Unlink crtEntry */
	prevEntry->nextEntry = crtEntry->nextEntry;
	if (NULL == prevEntry->nextEntry)
	{
		/* Just unlinked lastEntry. Update it */
		anchor->lastEntry = prevEntry;
	}
}

/*
 * Indicates that a client does not use the entry anymore
 *
 * For cached entries, the pin-count of the entry is decremented.
 *    If the pincount is 0 and entry is marked to be deleted, remove the entry
 *    from the cache. If entry is removed, then client clean-up is performed
 *
 * For acquired entries, perform client-cleanup and return entry to the freelist
 */
void
Cache_Release(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	switch(entry->state)
	{
		case CACHE_ENTRY_ACQUIRED:
			Cache_ReleaseAcquired(cache, entry, true /* unregisterCleanup */ );
			break;
		case CACHE_ENTRY_CACHED:
		case CACHE_ENTRY_DELETED:
			Cache_ReleaseCached(cache, entry, true /* unregisterCleanup */ );
			break;
		default:
			Assert(false && "unexpected cache entry state");
	}
}

/*
 * Internal version of the CacheRelease function
 *
 * Unregisters the entry from the cleanup list if requested.
 */
static void
Cache_ReleaseCached(Cache *cache, CacheEntry *entry, bool unregisterCleanup)
{
	Assert(NULL != cache);
	Assert(NULL != entry);

	Cache_ComputeEntryHashcode(cache, entry);

	volatile CacheAnchor *anchor = SyncHTLookup(cache->syncHashtable, &entry->hashvalue);
	Assert(anchor != NULL);

	/* Acquire anchor lock to touch the entry */
	SpinLockAcquire(&anchor->spinlock);

	uint32 pinCount = Cache_EntryDecRef(cache, entry);
	bool deleteEntry = false;

	if (pinCount == 0 && entry->state == CACHE_ENTRY_DELETED)
	{
		/* Delete the cache entry if pin-count = 0 and it is marked for deletion */
		Cache_UnlinkEntry(cache, (CacheAnchor *) anchor, entry);
		deleteEntry = true;

		Cache_DecPerfCounter(&cache->cacheHdr->cacheStats.noDeletedEntries, 1 /* delta */);
	}

	SpinLockRelease(&anchor->spinlock);

	/*
	 * Releasing anchor to hashtable.
	 * Ignoring 'removed' return value, both values are valid
	 */
	SyncHTRelease(cache->syncHashtable, (void *) anchor);

	/* If requested, unregister entry from the cleanup list */
	if (unregisterCleanup)
	{
		Cache_UnregisterCleanup(cache, entry);
	}

	if (deleteEntry)
	{

		PG_TRY();
		{
			/* Call client-specific cleanup function before removing entry from cache */
			cache->cleanupEntry(CACHE_ENTRY_PAYLOAD(entry));
		}
		PG_CATCH();
		{

			/* Grab entry lock to ensure exclusive access to it while we're touching it */
			Cache_LockEntry(cache, entry);

			entry->state = CACHE_ENTRY_FREE;

#ifdef USE_ASSERT_CHECKING
			Cache_MemsetPayload(cache, entry);
#endif

			Cache_UnlockEntry(cache, entry);

			/* Link entry back in the freelist */
			Cache_AddToFreelist(cache, entry);

			PG_RE_THROW();
		}
		PG_END_TRY();

		/* Grab entry lock to ensure exclusive access to it while we're touching it */
		Cache_LockEntry(cache, entry);

		entry->state = CACHE_ENTRY_FREE;

#ifdef USE_ASSERT_CHECKING
		Cache_MemsetPayload(cache, entry);
#endif

		Cache_UnlockEntry(cache, entry);

		/* Link entry back in the freelist */
		Cache_AddToFreelist(cache, entry);
	}
}

/*
 * Mark an entry for removal from the cache.
 * The entry is not immediately deleted, as there is at least one client
 * using it.
 * The entry will not be found using look-up operations after this step.
 * The entry will physically be removed once all using clients release it.
 *
 * This function is not synchronized. Multiple clients can mark an entry
 * deleted.
 */
void
Cache_Remove(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != entry);

	uint32 expected = CACHE_ENTRY_CACHED;
#ifdef USE_ASSERT_CHECKING
	int32 casResult =
#endif
	pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&entry->state, &expected, CACHE_ENTRY_DELETED);
	Assert(casResult == 1);

	Cache_DecPerfCounter(&cache->cacheHdr->cacheStats.noCachedEntries, 1 /* delta */);
	Cache_AddPerfCounter(&cache->cacheHdr->cacheStats.noDeletedEntries, 1 /* delta */);
}

/*
 * Returns true if the entry is in the cache.
 *
 * The state of the entry can be DELETED if the entry is in use but marked for
 * removal.
 *
 * This function is not synchronized.
 */
bool
Cache_IsCached(CacheEntry *entry)
{
	return (entry->state == CACHE_ENTRY_CACHED || entry->state == CACHE_ENTRY_DELETED);
}

/*
 * Acquire the lock protecting an entry. Required to do before touching
 * the state of the entry or its payload
 */
void
Cache_LockEntry(Cache *cache, CacheEntry *entry)
{
	SpinLockAcquire(&entry->spinlock);
}

/*
 * Release the lock protecting an entry
 */
void
Cache_UnlockEntry(Cache *cache, CacheEntry *entry)
{
	SpinLockRelease(&entry->spinlock);
}

/*
 * Increment the pinCount of a CacheEntry.
 * This function is not synchronized, the caller must ensure it is holding
 * a lock covering the entry.
 */
static uint32
Cache_EntryAddRef(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != entry);
	Assert(entry->pinCount < UINT32_MAX);
	CACHE_ASSERT_VALID(entry);

	entry->pinCount++;

	if (1 == entry->pinCount)
	{
		Cache_AddPerfCounter(&cache->cacheHdr->cacheStats.noPinnedEntries, 1 /* delta */);
	}

	return entry->pinCount;
}

/*
 * Decrement the pinCount of a CacheEntry.
 * This function is not synchronized, the caller must ensure it is holding
 * a lock covering the entry.
 */
static uint32
Cache_EntryDecRef(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != entry);
	Assert(entry->pinCount >= 1);
	CACHE_ASSERT_VALID(entry);

	entry->pinCount--;

	if (0 == entry->pinCount)
	{
		Cache_DecPerfCounter(&cache->cacheHdr->cacheStats.noPinnedEntries, 1 /* delta */);
	}

	return entry->pinCount;
}

/*
 * Gets an entry in the preallocated cache entry array by index.
 */
CacheEntry *
Cache_GetEntryByIndex(CacheHdr *cacheHdr, int32 idx)
{
	Assert(NULL != cacheHdr);
	Assert(idx < cacheHdr->nEntries);

	Size entrySize = MAXALIGN(sizeof(CacheEntry)) + MAXALIGN(cacheHdr->entrySize);
	return (CacheEntry *) ( (char *) cacheHdr->entryArray + idx * entrySize);
}

/*
 * Atomically increments the performance counter
 *
 * delta must be positive
 */
void
Cache_AddPerfCounter(uint32 *counter, int delta)
{
	Assert(counter + delta >= 0);
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) counter,delta);
}

void
Cache_AddPerfCounter64(int64 *counter, int64 delta)
{
	Assert(counter + delta >= 0);
	pg_atomic_add_fetch_u64((pg_atomic_uint64 *) counter,delta);
}

/*
 * Atomically decrements the performance counter
 *
 * delta must be positive
 */
void
Cache_DecPerfCounter(uint32 *counter, int delta)
{
	Assert(counter - delta >= 0);
	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) counter,delta);
}

void
Cache_DecPerfCounter64(int64 *counter, int64 delta)
{
	Assert(counter - delta >= 0);
	pg_atomic_sub_fetch_u64((pg_atomic_uint64 *) counter,delta);
}

/*
 * Record current timestamp at beginning of a cache operation.
 * Used in conjunction with Cache_TimedOperationRecord
 */
void
Cache_TimedOperationStart(void)
{
	INSTR_TIME_SET_CURRENT(timedOpStart);
}

/*
 * Records elapsed since the last Cache_TimedOperationStart.
 *
 * Accumulated total time goes in totalTime, maximum time goes into maxTime.
 *
 * Used in conjunction with Cache_TimedOperationStart
 */
void
Cache_TimedOperationRecord(instr_time *totalTime, instr_time *maxTime)
{
	Assert(!INSTR_TIME_IS_ZERO(timedOpStart));

	instr_time elapsedTime;
	INSTR_TIME_SET_CURRENT(elapsedTime);
	INSTR_TIME_SUBTRACT(elapsedTime, timedOpStart);

	/* Add difference to totalTime */
	/* FIXME This is not atomic */
	INSTR_TIME_ADD(*totalTime, elapsedTime);

	/* Compare elapsed time with current maximum and record if new maximum */
	if (INSTR_TIME_GET_DOUBLE(elapsedTime) > INSTR_TIME_GET_DOUBLE(*maxTime))
	{
		/* FIXME This is not atomic */
		INSTR_TIME_ASSIGN(*maxTime, elapsedTime);
	}

#ifdef USE_ASSERT_CHECKING
	INSTR_TIME_SET_ZERO(timedOpStart);
#endif

}

/* EOF */
