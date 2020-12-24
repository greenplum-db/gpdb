//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLDatumStatsLintMappable.cpp
//
//	@doc:
//		Implementation of DXL datum of types having LINT mapping
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumStatsLintMappable::CDXLDatumStatsLintMappable
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDatumStatsLintMappable::CDXLDatumStatsLintMappable(
	CMemoryPool *mp, IMDId *mdid_type, INT type_modifier, BOOL is_null,
	BYTE *byte_array, ULONG length, LINT value)
	: CDXLDatumGeneric(mp, mdid_type, type_modifier, is_null, byte_array,
					   length),
	  m_val(value)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumStatsLintMappable::Serialize
//
//	@doc:
//		Serialize datum in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDatumStatsLintMappable::Serialize(CXMLSerializer *xml_serializer)
{
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}
	if (!m_is_null)
	{
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenValue),
									 m_is_null, GetByteArray(), Length());
	}
	else
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIsNull), true);
	}
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenLintValue),
								 GetLINTMapping());
}


// EOF
