//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		ICache.h
//
//	@doc:
//		Interface for cache implementations.
//
//	@owner:
//		cwhipkey
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOS_ICache_H
#define GPOS_ICache_H

#include "gpos/base.h"

namespace gpos
{
typedef ULONG (*HashCacheKey)(void *key);
typedef BOOL (*EqualsCacheKey)(void *left_key, void *right_key);

class ICacheEntry
{
protected:
	ICacheEntry()
	{
	}

public:
	virtual ~ICacheEntry()
	{
	}

	/**
             * Get the assigned value
             */
	virtual void *GetValue() = 0;

	/**
             * Get the memory pool for this cache value.
             */
	virtual CMemoryPool *Pmp() = 0;
};

class ICache
{
protected:
	ICache()
	{
	}

public:
	virtual ~ICache()
	{
	}

	//
	// Get a handle to the value for the given key, incrementing the pin count on the value.
	//
	// Returns NULL if the key is not in the cache
	//
	virtual ICacheEntry *Get(void *key) = 0;

	//
	// Insert the given cache value.
	//
	// key and value should be allocated in the memory pool of the cache_entry object
	//
	// Return true if the object was successfully inserted, false otherwise.
	//
	virtual BOOL Insert(ICacheEntry *cache_entry, void *key, void *value) = 0;

	//
	// Create a new cache value that can be later passed to Insert
	//
	// Note that even if Insert fails, or Insert is never called, the returned value
	//  must be released with a call to Release.  Calling CreateEntry returns a value
	//  with a read lock of 1.
	//
	// Returns NULL if the cache value cannot be created because the cache is full or
	//   no memory is available
	//
	virtual ICacheEntry *CreateEntry() = 0;

	//
	// Decrement the pin counter, potentially triggering the delete of this cache value
	//
	virtual void Release(ICacheEntry *cache_entry) = 0;

	//
	// Remove the given key from the cache.
	//
	// Callers who still hold valid ICacheEntry handles will not be affected.
	//
	virtual void Delete(void *key) = 0;
};
}  // namespace gpos

#endif	// !GPOS_ICache_H

// EOF
