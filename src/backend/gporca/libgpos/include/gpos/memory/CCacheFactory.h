//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC CORP.
//
//	@filename:
//		CCache.h
//
//	@doc:
//		Definition of cache factory class.
//.
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef CCACHEFACTORY_H_
#define CCACHEFACTORY_H_

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/memory/CCache.h"

// default initial value of the gclock counter during insertion of an entry
#define CCACHE_GCLOCK_INIT_COUNTER 3

using namespace gpos;

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CCacheFactory
//
//	@doc:
//		Definition of cache factory;
//
//		This class is responsible of creating cache objects and allocating the
//		memory pools they need
//
//---------------------------------------------------------------------------
class CCacheFactory
{
private:
	// global instance
	static CCacheFactory *m_factory;

	// memory pool allocated to caches
	CMemoryPool *m_mp;

	// private ctor
	CCacheFactory(CMemoryPool *mp);



public:
	CCacheFactory(const CCacheFactory &) = delete;

	// private dtor
	~CCacheFactory()
	{
		GPOS_ASSERT(nullptr == m_factory &&
					"Cache factory has not been shut down");
	}

	// initialize global memory pool
	static void Init();

	// destroy global instance
	static void Shutdown();

	// global accessor
	inline static CCacheFactory *
	GetFactory()
	{
		return m_factory;
	}

	// create a cache instance
	template <class T, class K>
	static CCache<T, K> *
	CreateCache(BOOL unique, ULLONG cache_quota,
				typename CCache<T, K>::HashFuncPtr hash_func,
				typename CCache<T, K>::EqualFuncPtr equal_func)
	{
		GPOS_ASSERT(nullptr != m_factory &&
					"Cache factory has not been initialized");

		CMemoryPool *mp = m_factory->Pmp();
		CCache<T, K> *cache = GPOS_NEW(mp)
			CCache<T, K>(mp, unique, cache_quota, CCACHE_GCLOCK_INIT_COUNTER,
						 hash_func, equal_func);

		return cache;
	}

	CMemoryPool *Pmp() const;

};	// CCacheFactory
}  // namespace gpos


#endif	// CCACHEFACTORY_H_

// EOF
