/*-------------------------------------------------------------------------
 *
 * sharedcache_gclock.c
 *	 A generalized clock replacement policy for the shared cache
 *
 * Copyright (c) 2012, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/sharedcache.h"
#include "utils/syncrefhashtable.h"
#include "utils/gp_atomic.h"

/* Replacement policy default parameters */

/* Number of clock loops before giving up on finding a victim */
#define CACHE_DEFAULT_CLOCK_MAX_LOOPS 100

/* Number of entries to advance clock on look-up */
#define CACHE_DEFAULT_ENTRIES_ADVANCE 50

/* Amount to decrement utility of an entry at clock advance */
#define CACHE_DEFAULT_UTILITY_DECREMENT 7

/* Amount to increment utility of an entry when used */
#define CACHE_DEFAULT_UTILITY_INCREMENT 500

/*
 * Initialize parameters required for the replacement policy to run
 */
void
Cache_InitReplacementPolicy(Cache *cache)
{
	cache->cacheHdr->policyContext.clockPointer = 0;
	cache->cacheHdr->policyContext.maxClockLoops = CACHE_DEFAULT_CLOCK_MAX_LOOPS;
	cache->cacheHdr->policyContext.entriesAdvance = CACHE_DEFAULT_ENTRIES_ADVANCE;
	cache->cacheHdr->policyContext.utilityDecrement = CACHE_DEFAULT_UTILITY_DECREMENT;
	cache->cacheHdr->policyContext.utilityIncrement = CACHE_DEFAULT_UTILITY_INCREMENT;
}

/*
 * Atomically advance clock hand by one position, wrapping around
 * when reaching end of the array.
 *
 * Sets wraparound to true if clock hand wraps around
 *
 * Returns the new clock hand index
 */
static int32
Cache_NextClockHand(Cache *cache, bool *wraparound)
{
	Assert(NULL != cache);
	Assert(NULL != wraparound);

	int32 oldIndex = cache->cacheHdr->policyContext.clockPointer;
	int32 newIndex = gp_atomic_incmod_32(&cache->cacheHdr->policyContext.clockPointer, cache->cacheHdr->nEntries);
	Assert(newIndex < cache->cacheHdr->nEntries);
	*wraparound = (newIndex < oldIndex);

	return newIndex;
}

/*
 * Advance cache clock by a set number of entries and decrement each entry's
 * utility by decAmount.
 *
 * This function doesn't do any look-ups or locking, it's supposed to be fast.
 */
void
Cache_AdvanceClock(Cache *cache)
{
	Assert(NULL != cache);

	long entriesTouched = 0;
	while (entriesTouched++ < cache->cacheHdr->policyContext.entriesAdvance)
	{
		bool wraparound = false;
		int crtIndex = Cache_NextClockHand(cache, &wraparound);
		CacheEntry *crtEntry = Cache_GetEntryByIndex(cache->cacheHdr, crtIndex);
		gp_atomic_dec_positive_32(&crtEntry->utility,
				cache->cacheHdr->policyContext.utilityDecrement);
	}
}

/*
 * Run cache eviction algorithm
 *
 * It will try to evict enough entries to add up to evictSize. Returns the
 * actual accumulated size of the entries evicted
 */
int64
Cache_Evict(Cache *cache, int64 evictRequestSize)
{
	Assert(NULL != cache);
	Assert(evictRequestSize > 0);

	Cache_TimedOperationStart();

	int64 evictedSize = 0;
	uint32 unsuccessfulLoops = 0;
	bool foundVictim = false;
	uint32 decAmount = cache->cacheHdr->policyContext.utilityDecrement;
	Cache_Stats *cacheStats = &cache->cacheHdr->cacheStats;

	while (true)
	{

		bool wraparound = false;
		int32 entryIdx = Cache_NextClockHand(cache, &wraparound);
		Assert(entryIdx < cache->cacheHdr->nEntries);

		Cache_AddPerfCounter(&cacheStats->noEntriesScanned,1 /* delta */);

		if (wraparound)
		{
			unsuccessfulLoops++;

			Cache_AddPerfCounter(&cacheStats->noWraparound, 1 /* delta */);

			if (!foundVictim)
			{
				/*
				 * We looped around and did not manage to evict any entries.
				 * Double the amount we decrement eviction candidate's utility by.
				 * This makes the eviction algorithm look for a victim more aggressively
				 */
				if (decAmount <= CACHE_MAX_UTILITY / 2)
				{
					decAmount = 2 * decAmount;
				}
				else
				{
					decAmount = CACHE_MAX_UTILITY;
				}
			}
			foundVictim = false;

			if (unsuccessfulLoops > cache->cacheHdr->policyContext.maxClockLoops)
			{
				/* Can't find any cached and unused entries candidates for evictions, even after looping around
				 * maxClockLoops times. Give up looking for victims. */
				Cache_TimedOperationRecord(&cacheStats->timeEvictions, &cacheStats->maxTimeEvict);
				break;
			}
		}

		CacheEntry *crtEntry = Cache_GetEntryByIndex(cache->cacheHdr, entryIdx);
		if (crtEntry->state != CACHE_ENTRY_CACHED)
		{
			/* Not interested in free/acquired/deleted entries. Go back and advance clock hand */
			continue;
		}

		CacheAnchor *anchor = (CacheAnchor *) SyncHTLookup(cache->syncHashtable, &crtEntry->hashvalue);
		if (NULL == anchor)
		{
			/* There's no anchor for this entry, someone might have snatched it in the meantime */
			continue;
		}

		SpinLockAcquire(&anchor->spinlock);

		if (crtEntry->state != CACHE_ENTRY_CACHED)
		{
			/* Someone freed this entry in the meantime, before we got a chance to acquire the anchor lock */
			SpinLockRelease(&anchor->spinlock);
			SyncHTRelease(cache->syncHashtable, (void *) anchor);
			continue;
		}

		/* Ok, did all the checks, this entry must be valid now */
		CACHE_ASSERT_VALID(crtEntry);

		if (crtEntry->pinCount > 0)
		{
			/* Entry is in use and can't be evicted. Go back and advance clock hand */
			SpinLockRelease(&anchor->spinlock);
			SyncHTRelease(cache->syncHashtable, (void *) anchor);
			continue;
		}

		/* Decrement utility */
		gp_atomic_dec_positive_32(&crtEntry->utility, decAmount);
		/* Just decremented someone's utility. Reset our unsuccessful loops counter */
		unsuccessfulLoops = 0;

		if (crtEntry->utility > 0)
		{
			/* Entry has non-zero utility, we shouldn't evict it. Go back and advance clock hand */
			SpinLockRelease(&anchor->spinlock);
			SyncHTRelease(cache->syncHashtable, (void *) anchor);
			continue;
		}

		/* Found our victim */
		Assert(0 == crtEntry->pinCount);
		CACHE_ASSERT_VALID(crtEntry);
		Assert(crtEntry->utility == 0);

		uint32 expected = CACHE_ENTRY_CACHED;
#if USE_ASSERT_CHECKING
		int32 casResult =
#endif
		pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)&crtEntry->state, &expected, CACHE_ENTRY_DELETED);
		Assert(1 == casResult);

		SpinLockRelease(&anchor->spinlock);
		foundVictim = true;
		evictedSize += crtEntry->size;

		/* Don't update noFreeEntries yet. It will be done in Cache_AddToFreelist */
		Cache_DecPerfCounter(&cacheStats->noCachedEntries, 1 /* delta */);

		/* Unlink entry from the anchor chain */
		SpinLockAcquire(&anchor->spinlock);
		Cache_UnlinkEntry(cache, anchor, crtEntry);
		SpinLockRelease(&anchor->spinlock);

		SyncHTRelease(cache->syncHashtable, (void *) anchor);

		/* Call client-side cleanup for entry */
		cache->cleanupEntry(CACHE_ENTRY_PAYLOAD(crtEntry));

		Cache_LockEntry(cache, crtEntry);

		Assert(crtEntry->state == CACHE_ENTRY_DELETED);
		crtEntry->state = CACHE_ENTRY_FREE;

#if USE_ASSERT_CHECKING
		Cache_MemsetPayload(cache, crtEntry);
#endif

		Cache_UnlockEntry(cache, crtEntry);

		Cache_AddToFreelist(cache, crtEntry);

		Cache_AddPerfCounter(&cacheStats->noEvicts, 1 /* delta */);
		Cache_TimedOperationRecord(&cacheStats->timeEvictions, &cacheStats->maxTimeEvict);

		if (evictedSize >= evictRequestSize)
		{
			/* We evicted as much as requested */
			break;
		}

		Cache_TimedOperationStart();

	}

	return evictedSize;
}

/*
 * Marks an entry as recently used when it is accessed. Atomically
 * increments the  utility of the entry.
 */
void
Cache_TouchEntry(Cache *cache, CacheEntry *entry)
{
	Assert(NULL != cache);
	Assert(NULL != entry);
	Assert(entry->pinCount > 0);

	gp_atomic_inc_ceiling_32(&entry->utility,
			cache->cacheHdr->policyContext.utilityIncrement,
			CACHE_MAX_UTILITY);
}
