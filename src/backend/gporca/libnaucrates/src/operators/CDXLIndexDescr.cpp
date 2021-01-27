//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLIndexDescr.cpp
//
//	@doc:
//		Implementation of DXL index descriptors
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLIndexDescr.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLIndexDescr::CDXLIndexDescr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLIndexDescr::CDXLIndexDescr(CMemoryPool *mp, IMDId *mdid, CMDName *mdname)
	: m_mp(mp), m_mdid(mdid), m_mdname(mdname)
{
	GPOS_ASSERT(m_mdid->IsValid());
	GPOS_ASSERT(NULL != m_mdname);
	GPOS_ASSERT(m_mdname->GetMDName()->IsValid());
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLIndexDescr::~CDXLIndexDescr
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLIndexDescr::~CDXLIndexDescr()
{
	m_mdid->Release();
	GPOS_DELETE(m_mdname);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLIndexDescr::MDId
//
//	@doc:
//		Return the metadata id for the index
//
//---------------------------------------------------------------------------
IMDId *
CDXLIndexDescr::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLIndexDescr::MdName
//
//	@doc:
//		Return index name
//
//---------------------------------------------------------------------------
const CMDName *
CDXLIndexDescr::MdName() const
{
	return m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLIndexDescr::SerializeToDXL
//
//	@doc:
//		Serialize index descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLIndexDescr::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexDescr));
	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIndexName),
								 m_mdname->GetMDName());
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexDescr));
}

// EOF
