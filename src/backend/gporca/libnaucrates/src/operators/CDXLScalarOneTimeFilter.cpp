//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarOneTimeFilter.cpp
//
//	@doc:
//		Implementation of DXL physical one-time filter operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLScalarOneTimeFilter.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOneTimeFilter::CDXLScalarOneTimeFilter
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarOneTimeFilter::CDXLScalarOneTimeFilter(CMemoryPool *mp)
	: CDXLScalarFilter(mp)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOneTimeFilter::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarOneTimeFilter::GetDXLOperator() const
{
	return EdxlopScalarOneTimeFilter;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOneTimeFilter::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarOneTimeFilter::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarOneTimeFilter);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOneTimeFilter::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarOneTimeFilter::SerializeToDXL(CXMLSerializer *xml_serializer,
										const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serilize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}



// EOF
