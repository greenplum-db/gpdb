//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarNullTest.cpp
//
//	@doc:
//		Implementation of DXL NullTest
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarNullTest.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarNullTest::CDXLScalarNullTest
//
//	@doc:
//		Constructs a NullTest node
//
//---------------------------------------------------------------------------
CDXLScalarNullTest::CDXLScalarNullTest(CMemoryPool *mp, BOOL is_null)
	: CDXLScalar(mp), m_is_null(is_null)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarNullTest::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarNullTest::GetDXLOperator() const
{
	return EdxlopScalarNullTest;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarNullTest::IsNullTest
//
//	@doc:
//		Null Test type (is null or is not null)
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarNullTest::IsNullTest() const
{
	return m_is_null;
}



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarNullTest::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarNullTest::GetOpNameStr() const
{
	if (m_is_null)
	{
		return CDXLTokens::GetDXLTokenStr(EdxltokenScalarIsNull);
	}
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarIsNotNull);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarNullTest::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarNullTest::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarNullTest::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarNullTest::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	GPOS_ASSERT(1 == dxlnode->Arity());

	CDXLNode *dxlnode_arg = (*dxlnode)[0];
	GPOS_ASSERT(EdxloptypeScalar ==
				dxlnode_arg->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg, validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
