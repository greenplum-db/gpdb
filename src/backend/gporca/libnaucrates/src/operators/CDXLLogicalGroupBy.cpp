//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalGroupBy.cpp
//
//	@doc:
//		Implementation of DXL logical group by operator
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalGroupBy.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::CDXLLogicalGroupBy
//
//	@doc:
//		Construct a DXL Logical group by node
//
//---------------------------------------------------------------------------
CDXLLogicalGroupBy::CDXLLogicalGroupBy(CMemoryPool *mp)
	: CDXLLogical(mp), m_grouping_colid_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::CDXLLogicalGroupBy
//
//	@doc:
//		Construct a DXL Logical group by node
//
//---------------------------------------------------------------------------
CDXLLogicalGroupBy::CDXLLogicalGroupBy(CMemoryPool *mp,
									   ULongPtrArray *pdrgpulGrpColIds)
	: CDXLLogical(mp), m_grouping_colid_array(pdrgpulGrpColIds)
{
	GPOS_ASSERT(NULL != pdrgpulGrpColIds);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::~CDXLLogicalGroupBy
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLLogicalGroupBy::~CDXLLogicalGroupBy()
{
	CRefCount::SafeRelease(m_grouping_colid_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalGroupBy::GetDXLOperator() const
{
	return EdxlopLogicalGrpBy;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalGroupBy::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalGrpBy);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::SetGroupingColumns
//
//	@doc:
//		Sets array of grouping columns
//
//---------------------------------------------------------------------------
void
CDXLLogicalGroupBy::SetGroupingColumns(ULongPtrArray *grouping_colid_array)
{
	GPOS_ASSERT(NULL != grouping_colid_array);
	m_grouping_colid_array = grouping_colid_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::GetGroupingColidArray
//
//	@doc:
//		Grouping column indices
//
//---------------------------------------------------------------------------
const ULongPtrArray *
CDXLLogicalGroupBy::GetGroupingColidArray() const
{
	return m_grouping_colid_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::SerializeGrpColsToDXL
//
//	@doc:
//		Serialize grouping column indices in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalGroupBy::SerializeGrpColsToDXL(CXMLSerializer *xml_serializer) const
{
	if (NULL != m_grouping_colid_array)
	{
		const CWStringConst *grouping_cols_str =
			CDXLTokens::GetDXLTokenStr(EdxltokenGroupingCols);
		const CWStringConst *grouping_col_str =
			CDXLTokens::GetDXLTokenStr(EdxltokenGroupingCol);

		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			grouping_cols_str);

		for (ULONG idx = 0; idx < m_grouping_colid_array->Size(); idx++)
		{
			GPOS_ASSERT(NULL != (*m_grouping_colid_array)[idx]);
			ULONG grouping_col = *((*m_grouping_colid_array)[idx]);

			xml_serializer->OpenElement(
				CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
				grouping_col_str);
			xml_serializer->AddAttribute(
				CDXLTokens::GetDXLTokenStr(EdxltokenColId), grouping_col);

			xml_serializer->CloseElement(
				CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
				grouping_col_str);
		}

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			grouping_cols_str);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalGroupBy::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize grouping columns
	SerializeGrpColsToDXL(xml_serializer);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalGroupBy::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalGroupBy::AssertValid(const CDXLNode *node,
								BOOL validate_children) const
{
	// 1 Child node
	// 1 Group By project list

	const ULONG num_of_child = node->Arity();
	GPOS_ASSERT(2 == num_of_child);

	CDXLNode *proj_list = (*node)[0];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list->GetOperator()->GetDXLOperator());

	CDXLNode *dxl_op_type = (*node)[1];
	GPOS_ASSERT(EdxloptypeLogical ==
				dxl_op_type->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		for (ULONG idx = 0; idx < num_of_child; idx++)
		{
			CDXLNode *child_dxlnode = (*node)[idx];
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}

	const ULONG num_of_proj_elem = proj_list->Arity();
	for (ULONG idx = 0; idx < num_of_proj_elem; ++idx)
	{
		CDXLNode *proj_elem = (*proj_list)[idx];
		GPOS_ASSERT(EdxlopScalarIdent !=
					proj_elem->GetOperator()->GetDXLOperator());
	}
}
#endif	// GPOS_DEBUG

// EOF
