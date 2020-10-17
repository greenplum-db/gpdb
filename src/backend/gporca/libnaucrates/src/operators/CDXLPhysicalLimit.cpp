//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalLimit.cpp
//
//	@doc:
//		Implementation of DXL physical LIMIT operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalLimit.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalLimit::CDXLPhysicalLimit
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalLimit::CDXLPhysicalLimit(CMemoryPool *mp) : CDXLPhysical(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalLimit::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalLimit::GetDXLOperator() const
{
	return EdxlopPhysicalLimit;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalLimit::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalLimit::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalLimit);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalLimit::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalLimit::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize properties
	node->SerializePropertiesToDXL(xml_serializer);

	// serialize children nodes

	const CDXLNodeArray *dxl_array = node->GetChildDXLNodeArray();

	GPOS_ASSERT(4 == node->Arity());
	// serialize the first two children: target-list and plan
	for (ULONG i = 0; i < 4; i++)
	{
		CDXLNode *child_dxlnode = (*dxl_array)[i];
		child_dxlnode->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}



#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalLimit::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalLimit::AssertValid(const CDXLNode *node,
							   BOOL validate_children) const
{
	GPOS_ASSERT(4 == node->Arity());

	// Assert proj list is valid
	CDXLNode *proj_list_dxlnode = (*node)[EdxllimitIndexProjList];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list_dxlnode->GetOperator()->GetDXLOperator());

	// assert child plan is a physical plan and is valid

	CDXLNode *child_dxlnode = (*node)[EdxllimitIndexChildPlan];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	// Assert the validity of Count and Offset

	CDXLNode *count_dxlnode = (*node)[EdxllimitIndexLimitCount];
	GPOS_ASSERT(EdxlopScalarLimitCount ==
				count_dxlnode->GetOperator()->GetDXLOperator());

	CDXLNode *offset_dxlnode = (*node)[EdxllimitIndexLimitOffset];
	GPOS_ASSERT(EdxlopScalarLimitOffset ==
				offset_dxlnode->GetOperator()->GetDXLOperator());

	if (validate_children)
	{
		proj_list_dxlnode->GetOperator()->AssertValid(proj_list_dxlnode,
													  validate_children);
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
		count_dxlnode->GetOperator()->AssertValid(count_dxlnode,
												  validate_children);
		offset_dxlnode->GetOperator()->AssertValid(offset_dxlnode,
												   validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
