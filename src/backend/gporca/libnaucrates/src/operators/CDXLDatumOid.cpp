//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumOid.cpp
//
//	@doc:
//		Implementation of DXL datum of type oid
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDatumOid.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumOid::CDXLDatumOid
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDatumOid::CDXLDatumOid(CMemoryPool *mp, IMDId *mdid_type, BOOL is_null,
						   OID oid_val)
	: CDXLDatum(mp, mdid_type, default_type_modifier, is_null, 4 /*length*/),
	  m_oid_val(oid_val)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumOid::OidValue
//
//	@doc:
//		Return the oid value
//
//---------------------------------------------------------------------------
OID
CDXLDatumOid::OidValue() const
{
	return m_oid_val;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumOid::Serialize
//
//	@doc:
//		Serialize datum in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDatumOid::Serialize(CXMLSerializer *xml_serializer)
{
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (!m_is_null)
	{
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenValue),
									 m_oid_val);
	}
	else
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIsNull), true);
	}
}

// EOF
