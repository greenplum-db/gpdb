/*-------------------------------------------------------------------------
 *
 * sharedcache.h
 *	  Interface for a generic shared cache.
 *
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/sharedcache.h
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef SHAREDCACHE_H
#define SHAREDCACHE_H

#include "utils/hsearch.h"
#include "storage/lwlock.h"
#include "storage/spin.h"
#include "utils/syncrefhashtable.h"
#include "lib/dllist.h"
#include "portability/instr_time.h"


/* Retrieve pointer to the payload given a CacheEntry pointer */

#define CACHE_ENTRY_HEADER_SIZE (MAXALIGN(sizeof(CacheEntry)))
/* Get the payload of a cache entry */
#define CACHE_ENTRY_PAYLOAD(E) ((void *) ((char *) E + CACHE_ENTRY_HEADER_SIZE))
/* Get the cache entry header for an existing entry payload */
#define CACHE_ENTRY_HEADER(P) ((CacheEntry *)((char *)P - CACHE_ENTRY_HEADER_SIZE))

/* Pattern for memset-ing entry payloads in freelist */
#define CACHE_MEMSET_BYTE_PATTERN 0xCc
#define CACHE_MEMSET_4BYTE_PATTERN 0xCcCcCcCc
#define CACHE_ASSERT_WIPED(E) Assert(CACHE_MEMSET_4BYTE_PATTERN == *((uint32 *) CACHE_ENTRY_PAYLOAD(E)));
#define CACHE_ASSERT_VALID(E) Assert(CACHE_MEMSET_4BYTE_PATTERN != *((uint32 *) CACHE_ENTRY_PAYLOAD(E)));

/* Max allowed utility value for an entry. Set to int32 / 2 to avoid overflows */
#define CACHE_MAX_UTILITY 0x3FFFFFFF

typedef enum Cache_EntryState
{
	CACHE_ENTRY_FREE = 0, /* Entry is in the freelist and not in use */
	CACHE_ENTRY_ACQUIRED, /* Entry is owned by a client and populated, but not visible to other clients */
	CACHE_ENTRY_CACHED,   /* Entry is in the cache, visible to all clients through lookups */
	CACHE_ENTRY_DELETED,  /* Entry is in the cache and marked for deletion. Not visible to new clients through lookups */
	CACHE_ENTRY_RESERVED  /* Entry is in the process of being acquired, but it is not populated yet */
} Cache_EntryState;

/* Signature for function to clean up a resource before removing from cache */
typedef void (*Cache_ClientCleanupFunc) (const void *resource);

/* Signature for a function to populate a new entry after acquiring */
typedef void (*Cache_ClientPopulateFunc) (const void *resource, const void *param);


/*
 * An entry in the Cache. The caller's data
 * follows the CacheEntry structure (on a MAXALIGN'd boundary).
 */
typedef struct CacheEntry
{
	/* Lock to synchronize changes to the payload of the CacheEntry */
	slock_t	spinlock;

	/* Hash function result for this entry */
	uint32 hashvalue;

	/* Pointers to the next element in chain */
	struct CacheEntry *nextEntry;

	/* The number of clients holding pins on this entry */
	uint32 pinCount;

	/* State of this entry. Valid values:
	 *   CACHE_ENTRY_FREE
	 *   CACHE_ENTRY_ACQUIRED
	 *   CACHE_ENTRY_CACHED
	 *   CACHE_ENTRY_DELETED
	 * Use atomic operations (test&set) to change this.
	 */
	Cache_EntryState state;

	/* Abstract size of this cache entry */
	int64 size;

} CacheEntry;

/* Parameter data structure for used for to create a cache */
typedef struct CacheCtl
{
	/* Maximum number of elements to be stored in the cache */
	long maxSize;

	/*
	 * Number of partitions the buckets are placed in. Each partition is
	 * protected by a lock. More partitions mean lower contention.
	 */
	long numPartitions;

	/*
	 * Each partition requires a lightweight lock.
	 * baseLWLockId is the first LW lock of a preallocated numPartitions size
	 * lock array.
	 */
	LWLockId baseLWLockId;

	/* A name for the cache used to attach to it from all its clients */
	const char *cacheName;

	/* hash key length in bytes */
	Size keySize;

	/* Offset in the entry where the key is located */
	Size keyOffset;

	/* Total user element size in bytes */
	Size entrySize;

	/* Hash function */
	HashValueFunc hash;

	/* Key comparison function */
	HashCompareFunc match;

	/* Key copying function */
	HashCopyFunc keyCopy;

	/* Client cleanup function */
	Cache_ClientCleanupFunc cleanupEntry;

	/* Client populate entry function */
	Cache_ClientPopulateFunc populateEntry;

} CacheCtl;

/*
 * Record structure holding the to be exposed per-segment cache stats
 */
typedef struct Cache_StatsRec
{

	/* Cache activity stats */
	uint32 noLookups;
	uint32 noInserts;
	uint32 noEvicts;

	/* Cache performance stats */
	uint32 noCacheHits;
	uint32 noCompares;

	/* Cache state stats */
	uint32 noPinnedEntries;
	uint32 noCachedEntries;
	uint32 noDeletedEntries;
	uint32 noAcquiredEntries;
	uint32 noFreeEntries;

	/* Total size of entries in the cache */
	int64 totalEntrySize;

	/* Timing statistics */
	instr_time timeInserts;
	instr_time maxTimeInsert;

}	Cache_Stats;


/*
 * Cache header structure stored in shared memory
 */
typedef struct CacheHdr
{
	/* Lock to touch nentries or freeList */
	slock_t		spinlock;


	/* Maximum number of entries for this cache */
	uint32 nEntries;

	/* hash key length in bytes */
	Size keySize;

	/* Offset in the entry where the key is located */
	Size keyOffset;

	/* Total user element size in bytes */
	Size entrySize;

	/* linked list of free elements */
	CacheEntry *freeList;

	/* Pointer to the pre-allocated array of entries in shared memory. Used for evictions */
	void *entryArray;

	/* number of entries in the freelist*/
	long		nFreeEntries;

	/* Statistics about the cache */
	Cache_Stats cacheStats;

} CacheHdr;

/*
 * The Cache descriptor stored in the client memory
 */
typedef struct Cache
{
	/* Shared control information */
	CacheHdr *cacheHdr;

	/* Hashtable used as directory for anchor elements */
	SyncHT *syncHashtable;

	/* A name for the cache used to attach to it from all its clients */
	char *cacheName;

	/* Hash function */
	HashValueFunc hash;

	/* Key comparison function */
	HashCompareFunc match;

	/* Key copying function */
	HashCopyFunc keyCopy;

	/* Client cleanup function */
	Cache_ClientCleanupFunc cleanupEntry;

	/* Client populate entry function */
	Cache_ClientPopulateFunc populateEntry;

	/* List of entries owned by client. Used for cleanup */
	Dllist *ownedEntries;

} Cache;

/*
 * Anchor element stored in the hashtable. All cache elements with equal
 * hashvalues are linked to one such element.
 */
typedef struct CacheAnchor
{
	/* Hashvalue common for all cache entries anchored here. Required for SyncHT elements */
	uint32 hashvalue;
	/* PinCount counting number of clients holding this anchor. Required for SyncHT elements */
	int32 pinCount;

	/* List abstraction for cache entries anchored here */
	struct CacheEntry *firstEntry, *lastEntry;

	/* Mutex to synchronize access to the list */
	slock_t spinlock;
} CacheAnchor;

/* Public Cache API */
Cache *Cache_Create(CacheCtl *cacheCtl);
Size Cache_SharedMemSize(uint32 nEntries, uint32 cacheEntrySize);
void Cache_Free(Cache *cache);
void Cache_Insert(Cache *cache, CacheEntry *entry);
void Cache_Remove(Cache *cache, CacheEntry *entry);
void Cache_Release(Cache *cache, CacheEntry *entry);
CacheEntry *Cache_AcquireEntry(Cache *cache, void *populate_param);
bool Cache_IsCached(CacheEntry *entry);
void Cache_SurrenderClientEntries(Cache *cache);

/* Internal cache utility functions */
void Cache_UnlinkEntry(Cache *cache, CacheAnchor *anchor, CacheEntry *entry);
void Cache_AddToFreelist(Cache *cache, CacheEntry *entry);
CacheEntry *Cache_GetEntryByIndex(CacheHdr *cacheHdr, int32 idx);
void Cache_LockEntry(Cache *cache, CacheEntry *entry);
void Cache_UnlockEntry(Cache *cache, CacheEntry *entry);


#if USE_ASSERT_CHECKING
void Cache_MemsetPayload(Cache *cache, CacheEntry *entry);
#endif


/* Statistics related functions */
void Cache_AddPerfCounter(uint32 *counter, int delta);
void Cache_AddPerfCounter64(int64 *counter, int64 delta);
void Cache_DecPerfCounter(uint32 *counter, int delta);
void Cache_DecPerfCounter64(int64 *counter, int64 delta);
void Cache_TimedOperationStart(void);
void Cache_TimedOperationRecord(instr_time *totalTime, instr_time *maxTime);

#endif /* SHAREDCACHE_H */

/* EOF */
