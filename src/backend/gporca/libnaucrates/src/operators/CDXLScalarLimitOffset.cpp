//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarLimitOffset.cpp
//
//	@doc:
//		Implementation of DXL Scalar Limit Offset
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarLimitOffset.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitOffset::CDXLScalarLimitOffset
//
//	@doc:
//		Constructs a scalar Limit Offset node
//
//---------------------------------------------------------------------------
CDXLScalarLimitOffset::CDXLScalarLimitOffset(CMemoryPool *mp) : CDXLScalar(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitOffset::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarLimitOffset::GetDXLOperator() const
{
	return EdxlopScalarLimitOffset;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitOffset::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarLimitOffset::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarLimitOffset);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitOffset::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarLimitOffset::SerializeToDXL(CXMLSerializer *xml_serializer,
									  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitOffset::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarLimitOffset::AssertValid(const CDXLNode *node,
								   BOOL validate_children) const
{
	const ULONG arity = node->Arity();
	GPOS_ASSERT(1 >= arity);

	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_arg = (*node)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_arg->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg,
													validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
