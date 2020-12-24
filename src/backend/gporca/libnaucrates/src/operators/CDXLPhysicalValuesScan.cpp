//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalValuesScan.cpp
//
//	@doc:
//		Implementation of DXL physical values scan operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalValuesScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

// ctor
CDXLPhysicalValuesScan::CDXLPhysicalValuesScan(CMemoryPool *mp)
	: CDXLPhysical(mp)
{
}

// dtor
CDXLPhysicalValuesScan::~CDXLPhysicalValuesScan() = default;

// operator type
Edxlopid
CDXLPhysicalValuesScan::GetDXLOperator() const
{
	return EdxlopPhysicalValuesScan;
}

// operator name
const CWStringConst *
CDXLPhysicalValuesScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalValuesScan);
}

CDXLPhysicalValuesScan *
CDXLPhysicalValuesScan::Cast(CDXLOperator *dxl_op)
{
	GPOS_ASSERT(NULL != dxl_op);
	GPOS_ASSERT(EdxlopPhysicalValuesScan == dxl_op->GetDXLOperator());

	return dynamic_cast<CDXLPhysicalValuesScan *>(dxl_op);
}
// serialize operator in DXL format
void
CDXLPhysicalValuesScan::SerializeToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG

// checks whether operator node is well-structured
void
CDXLPhysicalValuesScan::AssertValid(const CDXLNode *dxlnode,
									BOOL validate_children) const
{
	GPOS_ASSERT(EdxloptypePhysical ==
				dxlnode->GetOperator()->GetDXLOperatorType());

	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(EdxlValIndexSentinel <= arity);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *child_dxlnode = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					child_dxlnode->GetOperator()->GetDXLOperatorType());
		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
