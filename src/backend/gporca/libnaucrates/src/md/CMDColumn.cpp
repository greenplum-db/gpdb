//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDColumn.cpp
//
//	@doc:
//		Implementation of the class for representing metadata about relation's
//		columns
//---------------------------------------------------------------------------


#include "naucrates/md/CMDColumn.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::CMDColumn
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDColumn::CMDColumn(CMDName *mdname, INT attrnum, IMDId *mdid_type,
					 INT type_modifier, BOOL is_nullable, BOOL is_dropped,
					 CDXLNode *dxl_dafault_value, ULONG length)
	: m_mdname(mdname),
	  m_attno(attrnum),
	  m_mdid_type(mdid_type),
	  m_type_modifier(type_modifier),
	  m_is_nullable(is_nullable),
	  m_is_dropped(is_dropped),
	  m_length(length),
	  m_dxl_default_val(dxl_dafault_value)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::~CMDColumn
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDColumn::~CMDColumn()
{
	GPOS_DELETE(m_mdname);
	m_mdid_type->Release();
	CRefCount::SafeRelease(m_dxl_default_val);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::Mdname
//
//	@doc:
//		Returns the column name
//
//---------------------------------------------------------------------------
CMDName
CMDColumn::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::AttrNum
//
//	@doc:
//		Attribute number
//
//---------------------------------------------------------------------------
INT
CMDColumn::AttrNum() const
{
	return m_attno;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::MdidType
//
//	@doc:
//		Attribute type id
//
//---------------------------------------------------------------------------
IMDId *
CMDColumn::MdidType() const
{
	return m_mdid_type;
}

INT
CMDColumn::TypeModifier() const
{
	return m_type_modifier;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::IsNullable
//
//	@doc:
//		Returns whether NULLs are allowed for this column
//
//---------------------------------------------------------------------------
BOOL
CMDColumn::IsNullable() const
{
	return m_is_nullable;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::IsDropped
//
//	@doc:
//		Returns whether column is dropped
//
//---------------------------------------------------------------------------
BOOL
CMDColumn::IsDropped() const
{
	return m_is_dropped;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::Serialize
//
//	@doc:
//		Serialize column metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDColumn::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumn));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAttno),
								 m_attno);

	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnNullable), m_is_nullable);
	if (gpos::ulong_max != m_length)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenColWidth), m_length);
	}

	if (m_is_dropped)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenColDropped), m_is_dropped);
	}

	// serialize default value
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnDefaultValue));

	if (NULL != m_dxl_default_val)
	{
		m_dxl_default_val->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnDefaultValue));

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumn));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDColumn::DebugPrint
//
//	@doc:
//		Debug print of the column in the provided stream
//
//---------------------------------------------------------------------------
void
CMDColumn::DebugPrint(IOstream &os) const
{
	os << "Attno: " << AttrNum() << std::endl;

	os << "Column name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;
	os << "Column type: ";
	MdidType()->OsPrint(os);
	os << std::endl;

	const CWStringConst *pstrNullsAllowed =
		IsNullable() ? CDXLTokens::GetDXLTokenStr(EdxltokenTrue)
					 : CDXLTokens::GetDXLTokenStr(EdxltokenFalse);

	os << "Nulls allowed: " << pstrNullsAllowed->GetBuffer() << std::endl;
}

#endif	// GPOS_DEBUG

// EOF
