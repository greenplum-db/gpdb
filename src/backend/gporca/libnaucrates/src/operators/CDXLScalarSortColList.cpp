//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarSortColList.cpp
//
//	@doc:
//		Implementation of DXL sorting column lists for sort and motion operator nodes.
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalarSortColList.h"

#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortColList::CDXLScalarSortColList
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarSortColList::CDXLScalarSortColList(CMemoryPool *mp) : CDXLScalar(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortColList::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarSortColList::GetDXLOperator() const
{
	return EdxlopScalarSortColList;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortColList::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarSortColList::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSortColList);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortColList::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarSortColList::SerializeToDXL(CXMLSerializer *xml_serializer,
									  const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortColList::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarSortColList::AssertValid(const CDXLNode *dxlnode,
								   BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	for (ULONG idx = 0; idx < arity; idx++)
	{
		CDXLNode *child_dxlnode = (*dxlnode)[idx];
		GPOS_ASSERT(EdxlopScalarSortCol ==
					child_dxlnode->GetOperator()->GetDXLOperator());

		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
