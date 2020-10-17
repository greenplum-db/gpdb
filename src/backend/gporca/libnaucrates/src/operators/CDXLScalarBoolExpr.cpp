//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarBoolExpr.cpp
//
//	@doc:
//		Implementation of DXL BoolExpr
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarBoolExpr.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::CDXLScalarBoolExpr
//
//	@doc:
//		Constructs a BoolExpr node
//
//---------------------------------------------------------------------------
CDXLScalarBoolExpr::CDXLScalarBoolExpr(CMemoryPool *mp,
									   const EdxlBoolExprType bool_type)
	: CDXLScalar(mp), m_bool_type(bool_type)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarBoolExpr::GetDXLOperator() const
{
	return EdxlopScalarBoolExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::GetDxlBoolTypeStr
//
//	@doc:
//		Boolean expression type
//
//---------------------------------------------------------------------------
EdxlBoolExprType
CDXLScalarBoolExpr::GetDxlBoolTypeStr() const
{
	return m_bool_type;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarBoolExpr::GetOpNameStr() const
{
	switch (m_bool_type)
	{
		case Edxland:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolAnd);
		case Edxlor:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolOr);
		case Edxlnot:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolNot);
		default:
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarBoolExpr::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	GPOS_CHECK_ABORT;

	const CWStringConst *element_name = GetOpNameStr();

	GPOS_ASSERT(NULL != element_name);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	GPOS_CHECK_ABORT;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBoolExpr::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarBoolExpr::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	EdxlBoolExprType dxl_bool_type =
		((CDXLScalarBoolExpr *) dxlnode->GetOperator())->GetDxlBoolTypeStr();

	GPOS_ASSERT((dxl_bool_type == Edxlnot) || (dxl_bool_type == Edxlor) ||
				(dxl_bool_type == Edxland));

	const ULONG arity = dxlnode->Arity();
	if (dxl_bool_type == Edxlnot)
	{
		GPOS_ASSERT(1 == arity);
	}
	else
	{
		GPOS_ASSERT(2 <= arity);
	}

	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[ul];
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
