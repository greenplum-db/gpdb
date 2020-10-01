//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarLimitCount.cpp
//
//	@doc:
//		Implementation of DXL Scalar Limit Count
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarLimitCount.h"

#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitCount::CDXLScalarLimitCount
//
//	@doc:
//		Constructs a scalar Limit Count node
//
//---------------------------------------------------------------------------
CDXLScalarLimitCount::CDXLScalarLimitCount(CMemoryPool *mp) : CDXLScalar(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitCount::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarLimitCount::GetDXLOperator() const
{
	return EdxlopScalarLimitCount;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitCount::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarLimitCount::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarLimitCount);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarLimitCount::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarLimitCount::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarLimitCount::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarLimitCount::AssertValid(const CDXLNode *node,
								  BOOL validate_children) const
{
	const ULONG arity = node->Arity();
	GPOS_ASSERT(1 >= arity);

	for (ULONG idx = 0; idx < arity; ++idx)
	{
		CDXLNode *dxlnode_arg = (*node)[idx];
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
