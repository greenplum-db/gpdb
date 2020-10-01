//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarIndexCondList.cpp
//
//	@doc:
//		Implementation of DXL index condition lists for DXL index scan operator
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalarIndexCondList.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIndexCondList::CDXLScalarIndexCondList
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CDXLScalarIndexCondList::CDXLScalarIndexCondList(CMemoryPool *mp)
	: CDXLScalar(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIndexCondList::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarIndexCondList::GetDXLOperator() const
{
	return EdxlopScalarIndexCondList;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIndexCondList::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarIndexCondList::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarIndexCondList);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIndexCondList::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarIndexCondList::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarIndexCondList::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarIndexCondList::AssertValid(const CDXLNode *node,
									 BOOL validate_children) const
{
	GPOS_ASSERT(NULL != node);

	if (validate_children)
	{
		const ULONG arity = node->Arity();
		for (ULONG ul = 0; ul < arity; ul++)
		{
			CDXLNode *child_dxlnode = (*node)[ul];
			GPOS_ASSERT(EdxloptypeScalar ==
						child_dxlnode->GetOperator()->GetDXLOperatorType());
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
