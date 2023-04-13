//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLRelStats.cpp
//
//	@doc:
//		Implementation of the class for representing relation stats in DXL
//---------------------------------------------------------------------------


#include "naucrates/md/CDXLRelStats.h"

#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::CDXLRelStats
//
//	@doc:
//		Constructs a metadata relation
//
//---------------------------------------------------------------------------
CDXLRelStats::CDXLRelStats(CMemoryPool *mp, CMDIdRelStats *rel_stats_mdid,
						   CMDName *mdname, CDouble rows, BOOL is_empty,
						   ULONG relpages, ULONG relallvisible)
	: m_mp(mp),
	  m_rel_stats_mdid(rel_stats_mdid),
	  m_mdname(mdname),
	  m_rows(rows),
	  m_empty(is_empty),
	  m_relpages(relpages),
	  m_relallvisible(relallvisible)
{
	GPOS_ASSERT(rel_stats_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::~CDXLRelStats
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLRelStats::~CDXLRelStats()
{
	GPOS_DELETE(m_mdname);
	if (nullptr != m_dxl_str)
	{
		GPOS_DELETE(m_dxl_str);
	}
	m_rel_stats_mdid->Release();
}

const CWStringDynamic *
CDXLRelStats::GetStrRepr()
{
	if (nullptr == m_dxl_str)
	{
		m_dxl_str = CDXLUtils::SerializeMDObj(
			m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
	}
	return m_dxl_str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::MDId
//
//	@doc:
//		Returns the metadata id of this relation stats object
//
//---------------------------------------------------------------------------
IMDId *
CDXLRelStats::MDId() const
{
	return m_rel_stats_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::Mdname
//
//	@doc:
//		Returns the name of this relation
//
//---------------------------------------------------------------------------
CMDName
CDXLRelStats::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::Rows
//
//	@doc:
//		Returns the number of rows
//
//---------------------------------------------------------------------------
CDouble
CDXLRelStats::Rows() const
{
	return m_rows;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::Serialize
//
//	@doc:
//		Serialize relation stats in DXL format
//
//---------------------------------------------------------------------------
void
CDXLRelStats::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelationStats));

	m_rel_stats_mdid->Serialize(xml_serializer,
								CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenRows),
								 m_rows);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenRelPages),
								 m_relpages);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelAllVisible), m_relallvisible);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenEmptyRelation), m_empty);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelationStats));

	GPOS_CHECK_ABORT;
}



#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CDXLRelStats::DebugPrint(IOstream &os) const
{
	os << "Relation id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Relation name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;

	os << "Rows: " << Rows() << std::endl;

	os << "RelPages: " << RelPages() << std::endl;

	os << "RelAllVisible: " << RelAllVisible() << std::endl;

	os << "Empty: " << IsEmpty() << std::endl;
}

#endif	// GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CDXLRelStats::CreateDXLDummyRelStats
//
//	@doc:
//		Dummy relation stats
//
//---------------------------------------------------------------------------
CDXLRelStats *
CDXLRelStats::CreateDXLDummyRelStats(CMemoryPool *mp, IMDId *mdid)
{
	CMDIdRelStats *rel_stats_mdid = CMDIdRelStats::CastMdid(mdid);
	CAutoP<CWStringDynamic> str;
	str = GPOS_NEW(mp) CWStringDynamic(mp, rel_stats_mdid->GetBuffer());
	CAutoP<CMDName> mdname;
	mdname = GPOS_NEW(mp) CMDName(mp, str.Value());
	CAutoRef<CDXLRelStats> rel_stats_dxl;
	rel_stats_dxl = GPOS_NEW(mp) CDXLRelStats(
		mp, rel_stats_mdid, mdname.Value(), CStatistics::DefaultColumnWidth,
		false /* is_empty */, 0 /* relpages */, 0 /* relallvisible */);
	mdname.Reset();
	return rel_stats_dxl.Reset();
}

// EOF
