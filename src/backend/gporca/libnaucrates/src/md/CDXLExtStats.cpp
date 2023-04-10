//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CDXLExtStats.cpp
//
//	@doc:
//		Implementation of the class for representing extended stats in DXL
//---------------------------------------------------------------------------


#include "naucrates/md/CDXLExtStats.h"

#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

CDXLExtStats::CDXLExtStats(CMemoryPool *mp, IMDId *rel_stats_mdid,
						   CMDName *mdname,
						   CMDDependencyArray *extstats_dependency_array,
						   CMDNDistinctArray *ndistinct_array)
	: m_mp(mp),
	  m_rel_stats_mdid(rel_stats_mdid),
	  m_mdname(mdname),
	  m_dependency_array(extstats_dependency_array),
	  m_ndistinct_array(ndistinct_array)
{
	GPOS_ASSERT(rel_stats_mdid->IsValid());
	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

CDXLExtStats::~CDXLExtStats()
{
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_rel_stats_mdid->Release();
	m_dependency_array->Release();
	m_ndistinct_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLExtStats::MDId
//
//	@doc:
//		Returns the metadata id of this extended stats object
//
//---------------------------------------------------------------------------
IMDId *
CDXLExtStats::MDId() const
{
	return m_rel_stats_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLExtStats::Mdname
//
//	@doc:
//		Returns the name of this relation
//
//---------------------------------------------------------------------------
CMDName
CDXLExtStats::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLExtStats::GetMDName
//
//	@doc:
//		Returns the DXL string for this object
//
//---------------------------------------------------------------------------
const CWStringDynamic *
CDXLExtStats::GetStrRepr() const
{
	return m_dxl_str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLExtStats::Serialize
//
//	@doc:
//		Serialize extended stats in DXL format
//
//---------------------------------------------------------------------------
void
CDXLExtStats::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenExtendedStats));

	m_rel_stats_mdid->Serialize(xml_serializer,
								CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());

	// serialize dependencies
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMVDependencyList));
	for (ULONG i = 0; i < m_dependency_array->Size(); i++)
	{
		(*m_dependency_array)[i]->Serialize(xml_serializer);
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMVDependencyList));

	// serialize ndistincts
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMVNDistinctList));
	for (ULONG i = 0; i < m_ndistinct_array->Size(); i++)
	{
		(*m_ndistinct_array)[i]->Serialize(xml_serializer);
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMVNDistinctList));

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenExtendedStats));

	GPOS_CHECK_ABORT;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLExtStats::CreateDXLDummyExtStats
//
//	@doc:
//		Dummy extended stats
//
//---------------------------------------------------------------------------
CDXLExtStats *
CDXLExtStats::CreateDXLDummyExtStats(CMemoryPool *mp, IMDId *mdid)
{
	CAutoP<CWStringDynamic> str;
	str = GPOS_NEW(mp) CWStringDynamic(mp, mdid->GetBuffer());
	CAutoP<CMDName> mdname;
	mdname = GPOS_NEW(mp) CMDName(mp, str.Value());
	CAutoRef<CDXLExtStats> ext_stats_dxl;

	CMDDependencyArray *extstats_dependency_array =
		GPOS_NEW(mp) CMDDependencyArray(mp);

	CMDNDistinctArray *extstats_ndistinct_array =
		GPOS_NEW(mp) CMDNDistinctArray(mp);

	ext_stats_dxl = GPOS_NEW(mp)
		CDXLExtStats(mp, mdid, mdname.Value(), extstats_dependency_array,
					 extstats_ndistinct_array);
	mdname.Reset();
	return ext_stats_dxl.Reset();
}

// EOF
