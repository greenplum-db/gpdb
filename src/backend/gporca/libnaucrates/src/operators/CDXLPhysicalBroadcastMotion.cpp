//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalBroadcastMotion.cpp
//
//	@doc:
//		Implementation of DXL physical broadcast motion operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalBroadcastMotion.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalBroadcastMotion::CDXLPhysicalBroadcastMotion
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalBroadcastMotion::CDXLPhysicalBroadcastMotion(CMemoryPool *mp)
	: CDXLPhysicalMotion(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalBroadcastMotion::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalBroadcastMotion::GetDXLOperator() const
{
	return EdxlopPhysicalMotionBroadcast;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalBroadcastMotion::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalBroadcastMotion::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalBroadcastMotion);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalBroadcastMotion::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalBroadcastMotion::SerializeToDXL(CXMLSerializer *xml_serializer,
											const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

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
//		CDXLPhysicalBroadcastMotion::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalBroadcastMotion::AssertValid(const CDXLNode *dxlnode,
										 BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(m_input_segids_array != NULL);
	GPOS_ASSERT(0 < m_input_segids_array->Size());
	GPOS_ASSERT(m_output_segids_array != NULL);
	GPOS_ASSERT(0 < m_output_segids_array->Size());

	GPOS_ASSERT(EdxlbmIndexSentinel == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[EdxlbmIndexChild];
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
