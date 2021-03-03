//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalHashJoin.cpp
//
//	@doc:
//		Implementation of DXL physical hash join operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalHashJoin.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalHashJoin::CDXLPhysicalHashJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalHashJoin::CDXLPhysicalHashJoin(CMemoryPool *mp,
										   EdxlJoinType join_type)
	: CDXLPhysicalJoin(mp, join_type)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalHashJoin::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalHashJoin::GetDXLOperator() const
{
	return EdxlopPhysicalHashJoin;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalHashJoin::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalHashJoin::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalHashJoin);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalHashJoin::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalHashJoin::SerializeToDXL(CXMLSerializer *xml_serializer,
									 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenJoinType),
								 GetJoinTypeNameStr());

	// serialize properties
	node->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalHashJoin::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalHashJoin::AssertValid(const CDXLNode *node,
								  BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(node, validate_children);

	GPOS_ASSERT(EdxlhjIndexSentinel == node->Arity());
	GPOS_ASSERT(EdxljtSentinel > GetJoinType());

	CDXLNode *join_filter = (*node)[EdxlhjIndexJoinFilter];
	CDXLNode *hash_clauses = (*node)[EdxlhjIndexHashCondList];
	CDXLNode *left = (*node)[EdxlhjIndexHashLeft];
	CDXLNode *right = (*node)[EdxlhjIndexHashRight];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarJoinFilter ==
				join_filter->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopScalarHashCondList ==
				hash_clauses->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxloptypePhysical ==
				left->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypePhysical ==
				right->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		join_filter->GetOperator()->AssertValid(join_filter, validate_children);
		hash_clauses->GetOperator()->AssertValid(hash_clauses,
												 validate_children);
		left->GetOperator()->AssertValid(left, validate_children);
		right->GetOperator()->AssertValid(right, validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
