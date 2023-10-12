//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CCacheFactory.cpp
//
//	@doc:
//		 Function implementation of CCacheFactory
//---------------------------------------------------------------------------


#include "gpos/memory/CCacheFactory.h"

#include "gpos/io/ioutils.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/memory/CCache.h"

using namespace gpos;

// global instance of cache factory
CCacheFactory *CCacheFactory::m_factory = NULL;

//---------------------------------------------------------------------------
//	@function:
//		CCacheFactory::CCacheFactory
//
//	@doc:
//		Ctor;
//
//---------------------------------------------------------------------------
CCacheFactory::CCacheFactory(CMemoryPool *mp) : m_mp(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheFactory::Pmp
//
//	@doc:
//		Returns a pointer to allocated memory pool
//
//---------------------------------------------------------------------------
CMemoryPool *
CCacheFactory::Pmp() const
{
	return m_mp;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheFactory::Init
//
//	@doc:
//		Initializes global instance
//
//---------------------------------------------------------------------------
void
CCacheFactory::Init()
{
	GPOS_ASSERT(NULL == GetFactory() &&
				"Cache factory was already initialized");

	// create cache factory memory pool
	CMemoryPool *mp =
		CMemoryPoolManager::GetMemoryPoolMgr()->CreateMemoryPool();

	// create cache factory instance
	m_factory = GPOS_NEW(mp) CCacheFactory(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheFactory::Shutdown
//
//	@doc:
//		Cleans up allocated memory pool
//
//---------------------------------------------------------------------------
void
CCacheFactory::Shutdown()
{
	CCacheFactory *factory = CCacheFactory::GetFactory();

	GPOS_ASSERT(NULL != factory && "Cache factory has not been initialized");

	CMemoryPool *mp = factory->m_mp;

	// destroy cache factory
	CCacheFactory::m_factory = NULL;
	GPOS_DELETE(factory);

	// release allocated memory pool
	CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(mp);
}
// EOF
