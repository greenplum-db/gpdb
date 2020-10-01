//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalSort.cpp
//
//	@doc:
//		Implementation of DXL physical sort operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalSort.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSort::CDXLPhysicalSort
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalSort::CDXLPhysicalSort(CMemoryPool *mp, BOOL discard_duplicates)
	: CDXLPhysical(mp), m_discard_duplicates(discard_duplicates)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSort::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalSort::GetDXLOperator() const
{
	return EdxlopPhysicalSort;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSort::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalSort::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalSort);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSort::FDiscardDuplicates
//
//	@doc:
//		Whether sort operator discards duplicated tuples.
//
//---------------------------------------------------------------------------
BOOL
CDXLPhysicalSort::FDiscardDuplicates() const
{
	return m_discard_duplicates;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSort::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSort::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSortDiscardDuplicates),
		m_discard_duplicates);

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
//		CDXLPhysicalSort::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSort::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(EdxlsortIndexSentinel == dxlnode->Arity());

	CDXLNode *sort_col_list_dxlnode = (*dxlnode)[EdxlsortIndexSortColList];
	CDXLNode *child_dxlnode = (*dxlnode)[EdxlsortIndexChild];
	CDXLNode *limit_count_dxlnode = (*dxlnode)[EdxlsortIndexLimitCount];
	CDXLNode *limit_offset_dxlnode = (*dxlnode)[EdxlsortIndexLimitOffset];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxloptypeScalar ==
				sort_col_list_dxlnode->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxlopScalarLimitCount ==
				limit_count_dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopScalarLimitOffset ==
				limit_offset_dxlnode->GetOperator()->GetDXLOperator());

	// there must be at least one sorting column
	GPOS_ASSERT(sort_col_list_dxlnode->Arity() > 0);

	if (validate_children)
	{
		sort_col_list_dxlnode->GetOperator()->AssertValid(sort_col_list_dxlnode,
														  validate_children);
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
