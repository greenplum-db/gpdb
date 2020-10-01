//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalJoin.cpp
//
//	@doc:
//		Implementation of DXL logical Join operator
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalJoin.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::CDXLLogicalJoin
//
//	@doc:
//		Construct a DXL Logical Join node
//
//---------------------------------------------------------------------------
CDXLLogicalJoin::CDXLLogicalJoin(CMemoryPool *mp, EdxlJoinType join_type)
	: CDXLLogical(mp), m_join_type(join_type)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalJoin::GetDXLOperator() const
{
	return EdxlopLogicalJoin;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::GetJoinType
//
//	@doc:
//		Join type
//
//---------------------------------------------------------------------------
EdxlJoinType
CDXLLogicalJoin::GetJoinType() const
{
	return m_join_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalJoin::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalJoin);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalJoin::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenJoinType),
								 GetJoinTypeNameStr());

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::GetJoinTypeNameStr
//
//	@doc:
//		Join type name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalJoin::GetJoinTypeNameStr() const
{
	return CDXLOperator::GetJoinTypeNameStr(m_join_type);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalJoin::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalJoin::AssertValid(const CDXLNode *node, BOOL validate_children) const
{
	const ULONG num_of_child = node->Arity();
	GPOS_ASSERT(2 < num_of_child);

	for (ULONG idx = 0; idx < num_of_child - 1; ++idx)
	{
		CDXLNode *child_dxlnode = (*node)[idx];
		GPOS_ASSERT(EdxloptypeLogical ==
					child_dxlnode->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}

	CDXLNode *node_last_child = (*node)[num_of_child - 1];
	GPOS_ASSERT(NULL != node_last_child);

	//The last child is a CDXLScalar operator representing the join qual
	GPOS_ASSERT(EdxloptypeScalar ==
				node_last_child->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		node_last_child->GetOperator()->AssertValid(node_last_child,
													validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
