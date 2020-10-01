//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarFilter.cpp
//
//	@doc:
//		Implementation of DXL physical filter operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLScalarFilter.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFilter::CDXLScalarFilter
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarFilter::CDXLScalarFilter(CMemoryPool *mp) : CDXLScalar(mp)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFilter::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarFilter::GetDXLOperator() const
{
	return EdxlopScalarFilter;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFilter::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarFilter::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarFilter);
	;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFilter::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarFilter::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serilize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFilter::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarFilter::AssertValid(const CDXLNode *node,
							  BOOL validate_children) const
{
	GPOS_ASSERT(1 >= node->Arity());

	if (1 == node->Arity())
	{
		CDXLNode *child_dxlnode = (*node)[0];

		GPOS_ASSERT(EdxloptypeScalar ==
					child_dxlnode->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}

#endif	// GPOS_DEBUG


// EOF
