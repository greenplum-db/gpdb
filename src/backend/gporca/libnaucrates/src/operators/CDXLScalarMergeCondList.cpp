//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarMergeCondList.cpp
//
//	@doc:
//		Implementation of DXL merge condition lists for merge join operators
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalarMergeCondList.h"

#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMergeCondList::CDXLScalarMergeCondList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarMergeCondList::CDXLScalarMergeCondList(CMemoryPool *mp)
	: CDXLScalar(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMergeCondList::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarMergeCondList::GetDXLOperator() const
{
	return EdxlopScalarMergeCondList;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMergeCondList::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarMergeCondList::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarMergeCondList);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMergeCondList::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarMergeCondList::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarMergeCondList::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarMergeCondList::AssertValid(const CDXLNode *node,
									 BOOL validate_children) const
{
	GPOS_ASSERT(NULL != node);

	const ULONG arity = node->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *child_dxlnode = (*node)[ul];
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
