//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalSelect.cpp
//
//	@doc:
//		Implementation of DXL logical select operator
//
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLLogicalSelect.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSelect::CDXLLogicalSelect
//
//	@doc:
//		Construct a DXL Logical select node
//
//---------------------------------------------------------------------------
CDXLLogicalSelect::CDXLLogicalSelect(CMemoryPool *mp) : CDXLLogical(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSelect::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalSelect::GetDXLOperator() const
{
	return EdxlopLogicalSelect;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSelect::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalSelect::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalSelect);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSelect::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalSelect::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSelect::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalSelect::AssertValid(const CDXLNode *node,
							   BOOL validate_children) const
{
	GPOS_ASSERT(2 == node->Arity());

	CDXLNode *condition_dxl = (*node)[0];
	CDXLNode *child_dxlnode = (*node)[1];

	GPOS_ASSERT(EdxloptypeScalar ==
				condition_dxl->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		condition_dxl->GetOperator()->AssertValid(condition_dxl,
												  validate_children);
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
