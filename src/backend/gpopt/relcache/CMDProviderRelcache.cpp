//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDProviderRelcache.cpp
//
//	@doc:
//		Implementation of a relcache-based metadata provider, which uses GPDB's
//		relcache to lookup objects given their ids.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "gpopt/relcache/CMDProviderRelcache.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "naucrates/exception.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderRelcache::CMDProviderRelcache
//
//	@doc:
//		Constructs a file-based metadata provider
//
//---------------------------------------------------------------------------
CMDProviderRelcache::CMDProviderRelcache
	(
	IMemoryPool *memory_pool
	)
	:
	m_memory_pool(memory_pool)
{
	GPOS_ASSERT(NULL != m_memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderRelcache::GetMDObjDXLStr
//
//	@doc:
//		Returns the DXL of the requested object in the provided memory pool
//
//---------------------------------------------------------------------------
CWStringBase *
CMDProviderRelcache::GetMDObjDXLStr
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *md_id
	)
	const
{
	IMDCacheObject *imdobj = CTranslatorRelcacheToDXL::RetrieveObject(memory_pool, md_accessor, md_id);

	GPOS_ASSERT(NULL != imdobj);

	CWStringDynamic *str = CDXLUtils::SerializeMDObj(m_memory_pool, imdobj, true /*fSerializeHeaders*/, false /*findent*/);

	// cleanup DXL object
	imdobj->Release();

	return str;
}

// EOF
