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
	IMemoryPool *pmp
	)
	:
	m_pmp(pmp)
{
	GPOS_ASSERT(NULL != m_pmp);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderRelcache::PstrObject
//
//	@doc:
//		Returns the DXL of the requested object in the provided memory pool
//
//---------------------------------------------------------------------------
CWStringBase *
CMDProviderRelcache::PstrObject
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	IMDId *pmdid
	)
	const
{
	IMDCacheObject *pimdobj = CTranslatorRelcacheToDXL::Pimdobj(pmp, pmda, pmdid);

	GPOS_ASSERT(NULL != pimdobj);

	CWStringDynamic *pstr = CDXLUtils::PstrSerializeMDObj(m_pmp, pimdobj, true /*fSerializeHeaders*/, false /*findent*/);

	// cleanup DXL object
	pimdobj->Release();

	return pstr;
}

// EOF
