//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDRequest.cpp
//
//	@doc:
//		Implementation of the class for metadata requests
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDRequest.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDRequest::CMDRequest
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDRequest::CMDRequest(CMemoryPool *mp, IMdIdArray *mdid_array,
					   SMDTypeRequestArray *mdtype_request_array)
	: m_mp(mp),
	  m_mdid_array(mdid_array),
	  m_mdtype_request_array(mdtype_request_array)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != mdid_array);
	GPOS_ASSERT(NULL != mdtype_request_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRequest::CMDRequest
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDRequest::CMDRequest(CMemoryPool *mp, SMDTypeRequest *md_type_request)
	: m_mp(mp), m_mdid_array(NULL), m_mdtype_request_array(NULL)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != md_type_request);

	m_mdid_array = GPOS_NEW(m_mp) IMdIdArray(m_mp);
	m_mdtype_request_array = GPOS_NEW(m_mp) SMDTypeRequestArray(m_mp);

	m_mdtype_request_array->Append(md_type_request);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRequest::~CMDRequest
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDRequest::~CMDRequest()
{
	m_mdid_array->Release();
	m_mdtype_request_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRequest::GetMDName
//
//	@doc:
//		Serialize system id
//
//---------------------------------------------------------------------------
CWStringDynamic *
CMDRequest::GetStrRepr(CSystemId sysid)
{
	CWStringDynamic *str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);
	str->AppendFormat(GPOS_WSZ_LIT("%d.%ls"), sysid.MdidType(),
					  sysid.GetBuffer());
	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRequest::Serialize
//
//	@doc:
//		Serialize relation metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDRequest::Serialize(CXMLSerializer *xml_serializer)
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMDRequest));

	const ULONG ulMdids = m_mdid_array->Size();
	for (ULONG ul = 0; ul < ulMdids; ul++)
	{
		IMDId *mdid = (*m_mdid_array)[ul];
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
		mdid->Serialize(xml_serializer,
						CDXLTokens::GetDXLTokenStr(EdxltokenValue));
		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	}

	const ULONG requests = m_mdtype_request_array->Size();
	for (ULONG ul = 0; ul < requests; ul++)
	{
		SMDTypeRequest *md_type_request = (*m_mdtype_request_array)[ul];
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeRequest));

		CWStringDynamic *str = GetStrRepr(md_type_request->m_sysid);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenSysid),
									 str);
		GPOS_DELETE(str);

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeInfo),
			md_type_request->m_type_info);

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMDTypeRequest));
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenMDRequest));
}



// EOF
