//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CMDProviderMemory.cpp
//
//	@doc:
//		Implementation of a memory-based metadata provider, which loads all
//		objects in memory and provides a function for looking them up by id.
//---------------------------------------------------------------------------

#include "gpos/io/COstreamString.h"
#include "gpos/memory/CMemoryPool.h"
#include "gpos/task/CWorker.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/error/CAutoTrace.h"

#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/CMDTypeInt4GPDB.h"
#include "naucrates/md/CMDTypeInt8GPDB.h"
#include "naucrates/md/CMDTypeBoolGPDB.h"
#include "naucrates/md/CDXLRelStats.h"
#include "naucrates/md/CDXLColStats.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/exception.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::CMDProviderMemory
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDProviderMemory::CMDProviderMemory(CMemoryPool *mp, const CHAR *file_name)
	: m_mdmap(NULL)
{
	GPOS_ASSERT(NULL != file_name);

	// read DXL file
	CAutoRg<CHAR> dxl_file;
	dxl_file = CDXLUtils::Read(mp, file_name);

	CAutoRef<IMDCacheObjectArray> mdcache_obj_array;
	mdcache_obj_array = CDXLUtils::ParseDXLToIMDObjectArray(
		mp, dxl_file.Rgt(), NULL /*xsd_file_path*/);

	LoadMetadataObjectsFromArray(mp, mdcache_obj_array.Value());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::CMDProviderMemory
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDProviderMemory::CMDProviderMemory(CMemoryPool *mp,
									 IMDCacheObjectArray *mdcache_obj_array)
	: m_mdmap(NULL)
{
	LoadMetadataObjectsFromArray(mp, mdcache_obj_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::LoadMetadataObjectsFromArray
//
//	@doc:
//		Loads the metadata objects from the given file
//
//---------------------------------------------------------------------------
void
CMDProviderMemory::LoadMetadataObjectsFromArray(
	CMemoryPool *mp, IMDCacheObjectArray *mdcache_obj_array)
{
	GPOS_ASSERT(NULL != mdcache_obj_array);

	// load metadata objects from the file
	CAutoRef<MDIdToSerializedMDIdMap> md_map;
	m_mdmap = GPOS_NEW(mp) MDIdToSerializedMDIdMap(mp);
	md_map = m_mdmap;

	const ULONG size = mdcache_obj_array->Size();

	// load objects into the hash map
	for (ULONG ul = 0; ul < size; ul++)
	{
		GPOS_CHECK_ABORT;

		IMDCacheObject *mdcache_obj = (*mdcache_obj_array)[ul];
		IMDId *mdid_key = mdcache_obj->MDId();
		mdid_key->AddRef();
		CAutoRef<IMDId> mdid_key_autoref;
		mdid_key_autoref = mdid_key;

		CAutoP<CWStringDynamic> str;
		str = CDXLUtils::SerializeMDObj(
			mp, mdcache_obj, true /*fSerializeHeaders*/, false /*findent*/);

		GPOS_CHECK_ABORT;
		BOOL fInserted = m_mdmap->Insert(mdid_key, str.Value());
		if (!fInserted)
		{
			GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryDuplicate,
					   mdid_key->GetBuffer());
		}
		(void) mdid_key_autoref.Reset();
		(void) str.Reset();
	}

	// safely completed loading
	(void) md_map.Reset();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::~CMDProviderMemory
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDProviderMemory::~CMDProviderMemory()
{
	CRefCount::SafeRelease(m_mdmap);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::GetMDObjDXLStr
//
//	@doc:
//		Returns the DXL of the requested object in the provided memory pool
//
//---------------------------------------------------------------------------
CWStringBase *
CMDProviderMemory::GetMDObjDXLStr(CMemoryPool *mp,
								  CMDAccessor *,  //md_accessor
								  IMDId *mdid) const
{
	GPOS_ASSERT(NULL != m_mdmap);

	const CWStringDynamic *pstrObj = m_mdmap->Find(mdid);

	// result string
	CAutoP<CWStringDynamic> a_pstrResult;

	a_pstrResult = NULL;

	if (NULL == pstrObj)
	{
		// Relstats and colstats are special as they may not
		// exist in the metadata file. Provider must return dummy objects
		// in this case.
		switch (mdid->MdidType())
		{
			case IMDId::EmdidRelStats:
			{
				mdid->AddRef();
				CAutoRef<CDXLRelStats> a_pdxlrelstats;
				a_pdxlrelstats = CDXLRelStats::CreateDXLDummyRelStats(mp, mdid);
				a_pstrResult = CDXLUtils::SerializeMDObj(
					mp, a_pdxlrelstats.Value(), true /*fSerializeHeaders*/,
					false /*findent*/);
				break;
			}
			case IMDId::EmdidColStats:
			{
				CAutoP<CWStringDynamic> a_pstr;
				a_pstr = GPOS_NEW(mp) CWStringDynamic(mp, mdid->GetBuffer());
				CAutoP<CMDName> a_pmdname;
				a_pmdname = GPOS_NEW(mp) CMDName(mp, a_pstr.Value());
				mdid->AddRef();
				CAutoRef<CDXLColStats> a_pdxlcolstats;
				a_pdxlcolstats = CDXLColStats::CreateDXLDummyColStats(
					mp, mdid, a_pmdname.Value(),
					CStatistics::DefaultColumnWidth /* width */);
				a_pmdname.Reset();
				a_pstrResult = CDXLUtils::SerializeMDObj(
					mp, a_pdxlcolstats.Value(), true /*fSerializeHeaders*/,
					false /*findent*/);
				break;
			}
			default:
			{
				GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDCacheEntryNotFound,
						   mdid->GetBuffer());
			}
		}
	}
	else
	{
		// copy string into result
		a_pstrResult = GPOS_NEW(mp) CWStringDynamic(mp, pstrObj->GetBuffer());
	}

	GPOS_ASSERT(NULL != a_pstrResult.Value());

	return a_pstrResult.Reset();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderMemory::MDId
//
//	@doc:
//		Returns the mdid for the requested system and type info.
//		The caller takes ownership over the object.
//
//---------------------------------------------------------------------------
IMDId *
CMDProviderMemory::MDId(CMemoryPool *mp, CSystemId sysid,
						IMDType::ETypeInfo type_info) const
{
	return GetGPDBTypeMdid(mp, sysid, type_info);
}

// EOF
