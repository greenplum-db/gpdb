//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalRedistributeMotion.cpp
//
//	@doc:
//		Implementation of DXL physical redistribute motion operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalRedistributeMotion.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRedistributeMotion::CDXLPhysicalRedistributeMotion
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalRedistributeMotion::CDXLPhysicalRedistributeMotion(
	CMemoryPool *mp, BOOL is_duplicate_sensitive)
	: CDXLPhysicalMotion(mp), m_is_duplicate_sensitive(is_duplicate_sensitive)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRedistributeMotion::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalRedistributeMotion::GetDXLOperator() const
{
	return EdxlopPhysicalMotionRedistribute;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRedistributeMotion::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalRedistributeMotion::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalRedistributeMotion);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRedistributeMotion::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRedistributeMotion::SerializeToDXL(CXMLSerializer *xml_serializer,
											   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	SerializeSegmentInfoToDXL(xml_serializer);

	if (m_is_duplicate_sensitive)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDuplicateSensitive), true);
	}

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
//		CDXLPhysicalRedistributeMotion::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRedistributeMotion::AssertValid(const CDXLNode *dxlnode,
											BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(m_input_segids_array != NULL);
	GPOS_ASSERT(0 < m_input_segids_array->Size());
	GPOS_ASSERT(m_output_segids_array != NULL);
	GPOS_ASSERT(0 < m_output_segids_array->Size());

	GPOS_ASSERT(EdxlrmIndexSentinel == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[EdxlrmIndexChild];
	CDXLNode *hash_expr_list = (*dxlnode)[EdxlrmIndexHashExprList];

	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
		hash_expr_list->GetOperator()->AssertValid(hash_expr_list,
												   validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
