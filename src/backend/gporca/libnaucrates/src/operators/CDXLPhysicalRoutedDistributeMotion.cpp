//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalRoutedDistributeMotion.cpp
//
//	@doc:
//		Implementation of DXL physical routed redistribute motion operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalRoutedDistributeMotion.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRoutedDistributeMotion::CDXLPhysicalRoutedDistributeMotion
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalRoutedDistributeMotion::CDXLPhysicalRoutedDistributeMotion(
	CMemoryPool *mp, ULONG segment_id_col)
	: CDXLPhysicalMotion(mp), m_segment_id_col(segment_id_col)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRoutedDistributeMotion::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalRoutedDistributeMotion::GetDXLOperator() const
{
	return EdxlopPhysicalMotionRoutedDistribute;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRoutedDistributeMotion::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalRoutedDistributeMotion::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalRoutedDistributeMotion);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRoutedDistributeMotion::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRoutedDistributeMotion::SerializeToDXL(
	CXMLSerializer *xml_serializer, const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSegmentIdCol), m_segment_id_col);

	SerializeSegmentInfoToDXL(xml_serializer);

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
//		CDXLPhysicalRoutedDistributeMotion::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRoutedDistributeMotion::AssertValid(const CDXLNode *dxlnode,
												BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(m_input_segids_array != NULL);
	GPOS_ASSERT(0 < m_input_segids_array->Size());
	GPOS_ASSERT(m_output_segids_array != NULL);
	GPOS_ASSERT(0 < m_output_segids_array->Size());

	GPOS_ASSERT(EdxlroutedmIndexSentinel == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[EdxlroutedmIndexChild];

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
