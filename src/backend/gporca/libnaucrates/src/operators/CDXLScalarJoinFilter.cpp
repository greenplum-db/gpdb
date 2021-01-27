//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarJoinFilter.cpp
//
//	@doc:
//		Implementation of DXL join filter operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLScalarJoinFilter.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarJoinFilter::CDXLScalarJoinFilter
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarJoinFilter::CDXLScalarJoinFilter(CMemoryPool *mp)
	: CDXLScalarFilter(mp)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarJoinFilter::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarJoinFilter::GetDXLOperator() const
{
	return EdxlopScalarJoinFilter;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarJoinFilter::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarJoinFilter::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarJoinFilter);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarJoinFilter::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarJoinFilter::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarJoinFilter::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarJoinFilter::AssertValid(const CDXLNode *node,
								  BOOL validate_children) const
{
	GPOS_ASSERT(1 >= node->Arity());

	if (1 == node->Arity())
	{
		CDXLNode *dxlnode_condition = (*node)[0];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_condition->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_condition->GetOperator()->AssertValid(dxlnode_condition,
														  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG


// EOF
