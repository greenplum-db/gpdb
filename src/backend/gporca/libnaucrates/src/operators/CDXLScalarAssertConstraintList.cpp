//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CDXLScalarAssertConstraintList.cpp
//
//	@doc:
//		Implementation of DXL scalar assert constraint lists
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarAssertConstraintList.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraintList::CDXLScalarAssertConstraintList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarAssertConstraintList::CDXLScalarAssertConstraintList(CMemoryPool *mp)
	: CDXLScalar(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraintList::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarAssertConstraintList::GetDXLOperator() const
{
	return EdxlopScalarAssertConstraintList;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraintList::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarAssertConstraintList::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarAssertConstraintList);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraintList::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarAssertConstraintList::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarAssertConstraintList::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarAssertConstraintList::AssertValid(const CDXLNode *dxlnode,
											BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(0 < arity);

	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *child_dxlnode = (*dxlnode)[ul];
		GPOS_ASSERT(EdxlopScalarAssertConstraint ==
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
