//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatum.cpp
//
//	@doc:
//		Implementation of DXL datum with type information
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDatum.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatum::CDXLDatum
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDatum::CDXLDatum(CMemoryPool *mp, IMDId *mdid_type, INT type_modifier,
					 BOOL is_null, ULONG length)
	: m_mp(mp),
	  m_mdid_type(mdid_type),
	  m_type_modifier(type_modifier),
	  m_is_null(is_null),
	  m_length(length)
{
	GPOS_ASSERT(m_mdid_type->IsValid());
}

INT
CDXLDatum::TypeModifier() const
{
	return m_type_modifier;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatum::IsNull
//
//	@doc:
//		Is the datum NULL
//
//---------------------------------------------------------------------------
BOOL
CDXLDatum::IsNull() const
{
	return m_is_null;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatum::Length
//
//	@doc:
//		Returns the size of the byte array
//
//---------------------------------------------------------------------------
ULONG
CDXLDatum::Length() const
{
	return m_length;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatum::Serialize
//
//	@doc:
//		Serialize datum in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDatum::Serialize(CXMLSerializer *xml_serializer,
					 const CWStringConst *datum_string)
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), datum_string);
	Serialize(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), datum_string);
}

// EOF
