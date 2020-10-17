//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarDistinctComp.cpp
//
//	@doc:
//		Implementation of DXL "is distinct from" comparison operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLScalarDistinctComp.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarDistinctComp::CDXLScalarDistinctComp
//
//	@doc:
//		Constructs a scalar distinct comparison node
//
//---------------------------------------------------------------------------
CDXLScalarDistinctComp::CDXLScalarDistinctComp(CMemoryPool *mp, IMDId *mdid_op)
	: CDXLScalarComp(
		  mp, mdid_op,
		  GPOS_NEW(mp) CWStringConst(
			  mp, CDXLTokens::GetDXLTokenStr(EdxltokenEq)->GetBuffer()))
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarDistinctComp::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarDistinctComp::GetDXLOperator() const
{
	return EdxlopScalarDistinct;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarDistinctComp::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarDistinctComp::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarDistinctComp);
	;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarDistinctComp::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarDistinctComp::SerializeToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenOpNo));

	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}



#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarDistinctComp::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarDistinctComp::AssertValid(const CDXLNode *node,
									BOOL validate_children) const
{
	GPOS_ASSERT(EdxlscdistcmpSentinel == node->Arity());

	CDXLNode *dxlnode_left = (*node)[EdxlscdistcmpIndexLeft];
	CDXLNode *dxlnode_right = (*node)[EdxlscdistcmpIndexRight];

	// assert children are of right type (scalar)
	GPOS_ASSERT(EdxloptypeScalar ==
				dxlnode_left->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypeScalar ==
				dxlnode_right->GetOperator()->GetDXLOperatorType());

	GPOS_ASSERT(
		GetComparisonOpName()->Equals(CDXLTokens::GetDXLTokenStr(EdxltokenEq)));

	if (validate_children)
	{
		dxlnode_left->GetOperator()->AssertValid(dxlnode_left,
												 validate_children);
		dxlnode_right->GetOperator()->AssertValid(dxlnode_right,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
