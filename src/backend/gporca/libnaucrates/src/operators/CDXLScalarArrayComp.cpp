//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarArrayComp.cpp
//
//	@doc:
//		Implementation of DXL scalar array comparison
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarArrayComp.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::CDXLScalarArrayComp
//
//	@doc:
//		Constructs a ScalarArrayComp node
//
//---------------------------------------------------------------------------
CDXLScalarArrayComp::CDXLScalarArrayComp(CMemoryPool *mp, IMDId *mdid_op,
										 const CWStringConst *str_opname,
										 EdxlArrayCompType comparison_type)
	: CDXLScalarComp(mp, mdid_op, str_opname),
	  m_comparison_type(comparison_type)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarArrayComp::GetDXLOperator() const
{
	return EdxlopScalarArrayComp;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::GetDXLArrayCmpType
//
//	@doc:
//	 	Returns the array comparison operation type (ALL/ANY)
//
//---------------------------------------------------------------------------
EdxlArrayCompType
CDXLScalarArrayComp::GetDXLArrayCmpType() const
{
	return m_comparison_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::PstrArrayCompType
//
//	@doc:
//		AggRef AggStage
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarArrayComp::GetDXLStrArrayCmpType() const
{
	switch (m_comparison_type)
	{
		case Edxlarraycomptypeany:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOpTypeAny);
		case Edxlarraycomptypeall:
			return CDXLTokens::GetDXLTokenStr(EdxltokenOpTypeAll);
		default:
			GPOS_ASSERT(!"Unrecognized array operation type");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarArrayComp::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarArrayComp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarArrayComp::SerializeToDXL(CXMLSerializer *xml_serializer,
									const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOpName),
								 m_comparison_operator_name);
	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenOpNo));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOpType),
								 GetDXLStrArrayCmpType());

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayComp::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarArrayComp::AssertValid(const CDXLNode *dxlnode,
								 BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(2 == arity);

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
