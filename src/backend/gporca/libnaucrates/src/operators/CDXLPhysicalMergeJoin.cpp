//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMergeJoin.cpp
//
//	@doc:
//		Implementation of DXL physical merge join operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalMergeJoin.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMergeJoin::CDXLPhysicalMergeJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalMergeJoin::CDXLPhysicalMergeJoin(CMemoryPool *mp,
											 EdxlJoinType join_type,
											 BOOL is_unique_outer)
	: CDXLPhysicalJoin(mp, join_type), m_is_unique_outer(is_unique_outer)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMergeJoin::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalMergeJoin::GetDXLOperator() const
{
	return EdxlopPhysicalMergeJoin;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMergeJoin::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalMergeJoin::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalMergeJoin);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMergeJoin::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMergeJoin::SerializeToDXL(CXMLSerializer *xml_serializer,
									  const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenJoinType),
								 GetJoinTypeNameStr());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenMergeJoinUniqueOuter),
		m_is_unique_outer);

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMergeJoin::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMergeJoin::AssertValid(const CDXLNode *dxlnode,
								   BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(EdxlmjIndexSentinel == dxlnode->Arity());
	GPOS_ASSERT(EdxljtSentinel > GetJoinType());

	CDXLNode *dxlnode_join_filter = (*dxlnode)[EdxlmjIndexJoinFilter];
	CDXLNode *dxlnode_merge_clauses = (*dxlnode)[EdxlmjIndexMergeCondList];
	CDXLNode *dxlnode_left = (*dxlnode)[EdxlmjIndexLeftChild];
	CDXLNode *dxlnode_right = (*dxlnode)[EdxlmjIndexRightChild];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarJoinFilter ==
				dxlnode_join_filter->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopScalarMergeCondList ==
				dxlnode_merge_clauses->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxloptypePhysical ==
				dxlnode_left->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypePhysical ==
				dxlnode_right->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		dxlnode_join_filter->GetOperator()->AssertValid(dxlnode_join_filter,
														validate_children);
		dxlnode_merge_clauses->GetOperator()->AssertValid(dxlnode_merge_clauses,
														  validate_children);
		dxlnode_left->GetOperator()->AssertValid(dxlnode_left,
												 validate_children);
		dxlnode_right->GetOperator()->AssertValid(dxlnode_right,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
