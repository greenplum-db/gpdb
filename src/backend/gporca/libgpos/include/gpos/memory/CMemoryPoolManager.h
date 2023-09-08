//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2004-2015 VMware, Inc. or its affiliates.
//
//
//
//	@filename:
//		CMemoryPoolManager.h
//
//	@doc:
//		Central memory pool manager;
//		provides factory method to generate memory pools;
//---------------------------------------------------------------------------
#ifndef GPOS_CMemoryPoolManager_H
#define GPOS_CMemoryPoolManager_H

#include "gpos/common/CSyncHashtable.h"
#include "gpos/common/CSyncHashtableAccessByIter.h"
#include "gpos/common/CSyncHashtableAccessByKey.h"
#include "gpos/common/CSyncHashtableIter.h"
#include "gpos/memory/CMemoryPool.h"



#define GPOS_MEMORY_POOL_HT_SIZE (1024)	 // number of hash table buckets

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMemoryPoolManager
//
//	@doc:
//		Global instance of memory pool management; singleton;
//
//---------------------------------------------------------------------------
class CMemoryPoolManager
{
private:
	using MemoryPoolKeyAccessor =
		CSyncHashtableAccessByKey<CMemoryPool, ULONG_PTR>;

	using MemoryPoolIter = CSyncHashtableIter<CMemoryPool, ULONG_PTR>;

	using MemoryPoolIterAccessor =
		CSyncHashtableAccessByIter<CMemoryPool, ULONG_PTR>;

	// memory pool in which all objects created by the manager itself
	// are allocated - must be thread-safe
	CMemoryPool *m_internal_memory_pool;

	// memory pool in which all objects created using global new operator
	// are allocated
	CMemoryPool *m_global_memory_pool;

	// hash table to maintain created pools
	CSyncHashtable<CMemoryPool, ULONG_PTR> *m_ht_all_pools;

	// global instance
	static CMemoryPoolManager *m_memory_pool_mgr;

	// create new pool of given type
	virtual CMemoryPool *NewMemoryPool();

	// clean-up memory pools
	static void Cleanup();

	// destroy a memory pool at shutdown
	static void DestroyMemoryPoolAtShutdown(CMemoryPool *mp);

	// Set up CMemoryPoolManager's internals
	void Setup();

protected:
	// Used for debugging. Indicates what type of memory pool the manager handles .
	// EMemoryPoolTracker indicates the manager handles CTrackerMemoryPools.
	// EMemoryPoolExternal indicates the manager handles memory pools with logic outside
	// the gporca framework (e.g.: CPallocMemoryPool which is declared in GPDB)
	enum EMemoryPoolType
	{
		EMemoryPoolTracker = 0,
		EMemoryPoolExternal,
		EMemoryPoolSentinel
	};

	EMemoryPoolType m_memory_pool_type;

	// ctor
	CMemoryPoolManager(CMemoryPool *internal, EMemoryPoolType memory_pool_type);

	CMemoryPool *
	GetInternalMemoryPool()
	{
		return m_internal_memory_pool;
	}

	// Initialize global memory pool manager using given types
	template <typename ManagerType, typename PoolType>
	static void
	SetupGlobalMemoryPoolManager()
	{
		// raw allocation of memory for internal memory pools
		void *alloc_internal = gpos::clib::Malloc(sizeof(PoolType));

		GPOS_OOM_CHECK(alloc_internal);

		// create internal memory pool
		CMemoryPool *internal = ::new (alloc_internal) PoolType();

		// instantiate manager
		m_memory_pool_mgr = ::new ManagerType(internal, EMemoryPoolTracker);
		m_memory_pool_mgr->Setup();
	}

public:
	CMemoryPoolManager(const CMemoryPoolManager &) = delete;

	// create new memory pool
	static CMemoryPool *CreateMemoryPool();

	// release memory pool
	static void Destroy(CMemoryPool *);

#ifdef GPOS_DEBUG
	// print internal contents of allocated memory pools
	IOstream &OsPrint(IOstream &os);

	// print memory pools whose allocated size above the given threshold
	void PrintOverSizedPools(CMemoryPool *trace, ULLONG size_threshold);
#endif	// GPOS_DEBUG

	// delete memory pools and release manager
	static void Shutdown();

	// accessor of memory pool used in global new allocations
	static CMemoryPool *
	GetGlobalMemoryPool()
	{
		GPOS_ASSERT(nullptr != m_memory_pool_mgr);
		return m_memory_pool_mgr->m_global_memory_pool;
	}

	virtual ~CMemoryPoolManager() = default;

	// return total allocated size in bytes
	ULLONG TotalAllocatedSize();

	// free memory allocation
	virtual void DeleteImpl(void *ptr, CMemoryPool::EAllocationType eat);

	// get user requested size of allocation
	virtual ULONG UserSizeOfAlloc(const void *ptr);

	// initialize global instance
	static void Init();

	// global accessor
	static CMemoryPoolManager *
	GetMemoryPoolMgr()
	{
		return m_memory_pool_mgr;
	}

};	// class CMemoryPoolManager
}  // namespace gpos

#endif	// !GPOS_CMemoryPoolManager_H

// EOF
