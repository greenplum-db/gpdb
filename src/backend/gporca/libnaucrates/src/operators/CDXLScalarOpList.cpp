//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLScalarOpList.cpp
//
//	@doc:
//		Implementation of DXL list of scalar expressions
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarOpList.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpList::CDXLScalarOpList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarOpList::CDXLScalarOpList(CMemoryPool *mp,
								   EdxlOpListType dxl_op_list_type)
	: CDXLScalar(mp), m_dxl_op_list_type(dxl_op_list_type)
{
	GPOS_ASSERT(EdxloplistSentinel > dxl_op_list_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpList::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarOpList::GetDXLOperator() const
{
	return EdxlopScalarOpList;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpList::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarOpList::GetOpNameStr() const
{
	Edxltoken dxl_token = EdxltokenSentinel;
	switch (m_dxl_op_list_type)
	{
		case EdxloplistEqFilterList:
			dxl_token = EdxltokenPartLevelEqFilterList;
			break;

		case EdxloplistFilterList:
			dxl_token = EdxltokenPartLevelFilterList;
			break;

		case EdxloplistGeneral:
			dxl_token = EdxltokenScalarOpList;
			break;

		default:
			GPOS_ASSERT(!"Invalid op list type");
			break;
	}

	return CDXLTokens::GetDXLTokenStr(dxl_token);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpList::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarOpList::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarOpList::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarOpList::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	for (ULONG idx = 0; idx < arity; ++idx)
	{
		CDXLNode *child_dxlnode = (*dxlnode)[idx];
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
