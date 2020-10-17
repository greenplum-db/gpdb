//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalGatherMotion.cpp
//
//	@doc:
//		Implementation of DXL physical gather motion operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalGatherMotion.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalGatherMotion::CDXLPhysicalGatherMotion
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalGatherMotion::CDXLPhysicalGatherMotion(CMemoryPool *mp)
	: CDXLPhysicalMotion(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalGatherMotion::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalGatherMotion::GetDXLOperator() const
{
	return EdxlopPhysicalMotionGather;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalGatherMotion::IOutputSegIdx
//
//	@doc:
//		Output segment index
//
//---------------------------------------------------------------------------
INT
CDXLPhysicalGatherMotion::IOutputSegIdx() const
{
	GPOS_ASSERT(NULL != m_output_segids_array);
	GPOS_ASSERT(1 == m_output_segids_array->Size());
	return *((*m_output_segids_array)[0]);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalGatherMotion::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalGatherMotion::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalGatherMotion);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalGatherMotion::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalGatherMotion::SerializeToDXL(CXMLSerializer *xml_serializer,
										 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	SerializeSegmentInfoToDXL(xml_serializer);

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
//		CDXLPhysicalGatherMotion::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalGatherMotion::AssertValid(const CDXLNode *node,
									  BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(node, validate_children);
	GPOS_ASSERT(m_input_segids_array != NULL);
	GPOS_ASSERT(0 < m_input_segids_array->Size());
	GPOS_ASSERT(m_output_segids_array != NULL);
	GPOS_ASSERT(1 == m_output_segids_array->Size());

	GPOS_ASSERT(EdxlgmIndexSentinel == node->Arity());

	CDXLNode *child_dxlnode = (*node)[EdxlgmIndexChild];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
