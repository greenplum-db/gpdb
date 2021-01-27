//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDCache.cpp
//
//	@doc:
//		 Function implementation of CMDCache
//---------------------------------------------------------------------------

#include "gpopt/mdcache/CMDCache.h"

#include "gpos/task/CAutoTraceFlag.h"

using namespace gpos;
using namespace gpmd;
using namespace gpopt;


// global instance of metadata cache
CMDAccessor::MDCache *CMDCache::m_pcache = NULL;

// maximum size of the cache
ULLONG CMDCache::m_ullCacheQuota = UNLIMITED_CACHE_QUOTA;

//---------------------------------------------------------------------------
//	@function:
//		CMDCache::Init
//
//	@doc:
//		Initializes global instance
//
//---------------------------------------------------------------------------
void
CMDCache::Init()
{
	GPOS_ASSERT(NULL == m_pcache && "Metadata cache was already created");

	m_pcache = CCacheFactory::CreateCache<IMDCacheObject *, CMDKey *>(
		true /*fUnique*/, m_ullCacheQuota, CMDKey::UlHashMDKey,
		CMDKey::FEqualMDKey);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDCache::Shutdown
//
//	@doc:
//		Cleans up the underlying cache
//
//---------------------------------------------------------------------------
void
CMDCache::Shutdown()
{
	GPOS_DELETE(m_pcache);
	m_pcache = NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDCache::SetCacheQuota
//
//	@doc:
//		Set the maximum size of the cache
//
//---------------------------------------------------------------------------
void
CMDCache::SetCacheQuota(ULLONG ullCacheQuota)
{
	GPOS_ASSERT(NULL != m_pcache && "Metadata cache was not created");
	m_ullCacheQuota = ullCacheQuota;
	m_pcache->SetCacheQuota(ullCacheQuota);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCache::ULLGetCacheQuota
//
//	@doc:
//		Get the maximum size of the cache
//
//---------------------------------------------------------------------------
ULLONG
CMDCache::ULLGetCacheQuota()
{
	// make sure that the CMDCache's saved quota is reflected in the underlying CCache
	GPOS_ASSERT_IMP(NULL != m_pcache,
					m_pcache->GetCacheQuota() == m_ullCacheQuota);
	return m_ullCacheQuota;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCache::ULLGetCacheEvictionCounter
//
//	@doc:
// 		Get the number of times we evicted entries from this cache
//
//---------------------------------------------------------------------------
ULLONG
CMDCache::ULLGetCacheEvictionCounter()
{
	// make sure that we already initialized our underlying CCache
	GPOS_ASSERT(NULL != m_pcache);

	return m_pcache->GetEvictionCounter();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCache::Reset
//
//	@doc:
//		Reset metadata cache
//
//---------------------------------------------------------------------------
void
CMDCache::Reset()
{
	CAutoTraceFlag atf1(EtraceSimulateOOM, false);
	CAutoTraceFlag atf2(EtraceSimulateAbort, false);
	CAutoTraceFlag atf3(EtraceSimulateIOError, false);
	CAutoTraceFlag atf4(EtraceSimulateNetError, false);

	Shutdown();
	Init();
}

// EOF
