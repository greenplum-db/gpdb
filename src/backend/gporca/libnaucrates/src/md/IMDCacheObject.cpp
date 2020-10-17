//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IMDCacheObject.cpp
//
//	@doc:
//		Implementation of common methods for MD cache objects
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/IMDCacheObject.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		IMDCacheObject::SerializeMDIdAsElem
//
//	@doc:
//		Serialize MD operator info in DXL format
//
//---------------------------------------------------------------------------
void
IMDCacheObject::SerializeMDIdAsElem(CXMLSerializer *xml_serializer,
									const CWStringConst *element_name,
									const IMDId *mdid) const
{
	if (NULL == mdid)
	{
		return;
	}

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	mdid->Serialize(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}


//---------------------------------------------------------------------------
//	@function:
//		IMDCacheObject::SerializeMDIdList
//
//	@doc:
//		Serialize a list of metadata ids into DXL
//
//---------------------------------------------------------------------------
void
IMDCacheObject::SerializeMDIdList(CXMLSerializer *xml_serializer,
								  const IMdIdArray *mdid_array,
								  const CWStringConst *strTokenList,
								  const CWStringConst *strTokenListItem)
{
	// serialize list of metadata ids
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), strTokenList);
	const ULONG length = mdid_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			strTokenListItem);

		IMDId *mdid = (*mdid_array)[ul];
		mdid->Serialize(xml_serializer,
						CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			strTokenListItem);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), strTokenList);
}

// EOF
