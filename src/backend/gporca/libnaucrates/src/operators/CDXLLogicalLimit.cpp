//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLLogicalLimit.cpp
//
//	@doc:
//		Implementation of DXL logical limit operator
//
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLLogicalLimit.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::CDXLLogicalLimit
//
//	@doc:
//		Construct a DXL Logical limit node
//
//---------------------------------------------------------------------------
CDXLLogicalLimit::CDXLLogicalLimit(CMemoryPool *mp, BOOL top_limit_under_dml)
	: CDXLLogical(mp), m_top_limit_under_dml(top_limit_under_dml)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::~CDXLLogicalLimit
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLLogicalLimit::~CDXLLogicalLimit()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalLimit::GetDXLOperator() const
{
	return EdxlopLogicalLimit;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalLimit::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalLimit);
}
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalLimit::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	if (m_top_limit_under_dml)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTopLimitUnderDML),
			m_top_limit_under_dml);
	}

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalLimit::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalLimit::AssertValid(const CDXLNode *node,
							  BOOL validate_children) const
{
	GPOS_ASSERT(4 == node->Arity());

	// Assert the validity of sort column list
	CDXLNode *sort_col_list_dxlnode = (*node)[EdxllogicallimitIndexSortColList];
	GPOS_ASSERT(EdxloptypeScalar ==
				sort_col_list_dxlnode->GetOperator()->GetDXLOperatorType());

	// Assert the validity of Count and Offset

	CDXLNode *limit_count_dxlnode = (*node)[EdxllogicallimitIndexLimitCount];
	GPOS_ASSERT(EdxlopScalarLimitCount ==
				limit_count_dxlnode->GetOperator()->GetDXLOperator());

	CDXLNode *limit_offset_dxlnode = (*node)[EdxllogicallimitIndexLimitOffset];
	GPOS_ASSERT(EdxlopScalarLimitOffset ==
				limit_offset_dxlnode->GetOperator()->GetDXLOperator());

	// Assert child plan is a logical plan and is valid
	CDXLNode *child_dxlnode = (*node)[EdxllogicallimitIndexChildPlan];
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		sort_col_list_dxlnode->GetOperator()->AssertValid(sort_col_list_dxlnode,
														  validate_children);
		limit_offset_dxlnode->GetOperator()->AssertValid(limit_offset_dxlnode,
														 validate_children);
		limit_count_dxlnode->GetOperator()->AssertValid(limit_count_dxlnode,
														validate_children);
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
