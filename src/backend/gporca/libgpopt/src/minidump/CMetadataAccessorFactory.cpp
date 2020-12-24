//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpos/common/CAutoRef.h"
#include "naucrates/md/CMDProviderMemory.h"

namespace gpopt
{
CMetadataAccessorFactory::CMetadataAccessorFactory(CMemoryPool *mp,
												   CDXLMinidump *pdxlmd,
												   const CHAR *file_name)
{
	// set up MD providers
	CAutoRef<CMDProviderMemory> apmdp(GPOS_NEW(mp)
										  CMDProviderMemory(mp, file_name));
	const CSystemIdArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
	CAutoRef<CMDProviderArray> apdrgpmdp(GPOS_NEW(mp) CMDProviderArray(mp));

	// ensure there is at least ONE system id
	apmdp->AddRef();
	apdrgpmdp->Append(apmdp.Value());

	for (ULONG ul = 1; ul < pdrgpsysid->Size(); ul++)
	{
		apmdp->AddRef();
		apdrgpmdp->Append(apmdp.Value());
	}

	m_apmda = GPOS_NEW(mp) CMDAccessor(
		mp, CMDCache::Pcache(), pdxlmd->GetSysidPtrArray(), apdrgpmdp.Value());
}

CMDAccessor *
CMetadataAccessorFactory::Pmda()
{
	return m_apmda.Value();
}
}  // namespace gpopt
