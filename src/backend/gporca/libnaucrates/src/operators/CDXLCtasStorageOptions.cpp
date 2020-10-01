//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLCtasStorageOptions.cpp
//
//	@doc:
//		Implementation of DXL CTAS storage options
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::CDXLCtasStorageOptions
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::CDXLCtasStorageOptions(
	CMDName *mdname_tablespace, ECtasOnCommitAction ctas_on_commit_action,
	CDXLCtasOptionArray *ctas_storage_option_array)
	: m_mdname_tablespace(mdname_tablespace),
	  m_ctas_on_commit_action(ctas_on_commit_action),
	  m_ctas_storage_option_array(ctas_storage_option_array)
{
	GPOS_ASSERT(EctascommitSentinel > ctas_on_commit_action);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::~CDXLCtasStorageOptions
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::~CDXLCtasStorageOptions()
{
	GPOS_DELETE(m_mdname_tablespace);
	CRefCount::SafeRelease(m_ctas_storage_option_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::GetMdNameTableSpace
//
//	@doc:
//		Returns the tablespace name
//
//---------------------------------------------------------------------------
CMDName *
CDXLCtasStorageOptions::GetMdNameTableSpace() const
{
	return m_mdname_tablespace;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::GetOnCommitAction
//
//	@doc:
//		Returns the OnCommit ctas spec
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::ECtasOnCommitAction
CDXLCtasStorageOptions::GetOnCommitAction() const
{
	return m_ctas_on_commit_action;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::CDXLCtasOptionArray
//
//	@doc:
//		Returns array of storage options
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions::CDXLCtasOptionArray *
CDXLCtasStorageOptions::GetDXLCtasOptionArray() const
{
	return m_ctas_storage_option_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::Serialize
//
//	@doc:
//		Serialize options in DXL format
//
//---------------------------------------------------------------------------
void
CDXLCtasStorageOptions::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCTASOptions));
	if (NULL != m_mdname_tablespace)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTablespace),
			m_mdname_tablespace->GetMDName());
	}

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitAction),
		GetOnCommitActionStr(m_ctas_on_commit_action));

	const ULONG ulOptions = (m_ctas_storage_option_array == NULL)
								? 0
								: m_ctas_storage_option_array->Size();
	for (ULONG ul = 0; ul < ulOptions; ul++)
	{
		CDXLCtasOption *dxl_ctas_storage_options =
			(*m_ctas_storage_option_array)[ul];
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenCTASOption));
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenCtasOptionType),
			dxl_ctas_storage_options->m_type);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
									 dxl_ctas_storage_options->m_str_name);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenValue),
									 dxl_ctas_storage_options->m_str_value);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIsNull),
			dxl_ctas_storage_options->m_is_null);
		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenCTASOption));
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCTASOptions));
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLCtasStorageOptions::GetOnCommitActionStr
//
//	@doc:
//		String representation of OnCommit action spec
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLCtasStorageOptions::GetOnCommitActionStr(
	CDXLCtasStorageOptions::ECtasOnCommitAction ctas_on_commit_action)
{
	switch (ctas_on_commit_action)
	{
		case EctascommitNOOP:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitNOOP);

		case EctascommitPreserve:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitPreserve);

		case EctascommitDelete:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitDelete);

		case EctascommitDrop:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOnCommitDrop);

		default:
			GPOS_ASSERT("Invalid on commit option");
			return NULL;
	}
}

// EOF
