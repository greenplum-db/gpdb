//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumBool.cpp
//
//	@doc:
//		Implementation of DXL datum of type boolean
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDatumBool.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumBool::CDXLDatumBool
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDatumBool::CDXLDatumBool(CMemoryPool *mp, IMDId *mdid_type, BOOL is_null,
							 BOOL value)
	: CDXLDatum(mp, mdid_type, default_type_modifier, is_null, 1 /*length*/),
	  m_value(value)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumBool::Serialize
//
//	@doc:
//		Serialize datum in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDatumBool::Serialize(CXMLSerializer *xml_serializer)
{
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (!m_is_null)
	{
		if (m_value)
		{
			xml_serializer->AddAttribute(
				CDXLTokens::GetDXLTokenStr(EdxltokenValue),
				CDXLTokens::GetDXLTokenStr(EdxltokenTrue));
		}
		else
		{
			xml_serializer->AddAttribute(
				CDXLTokens::GetDXLTokenStr(EdxltokenValue),
				CDXLTokens::GetDXLTokenStr(EdxltokenFalse));
		}
	}
	else
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIsNull), true);
	}
}

// EOF
