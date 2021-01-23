//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarBooleanTest.cpp
//
//	@doc:
//		Implementation of DXL BooleanTest
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarBooleanTest.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::CDXLScalarBooleanTest
//
//	@doc:
//		Constructs a BooleanTest node
//
//---------------------------------------------------------------------------
CDXLScalarBooleanTest::CDXLScalarBooleanTest(
	CMemoryPool *mp, const EdxlBooleanTestType dxl_bool_test_type)
	: CDXLScalar(mp), m_dxl_bool_test_type(dxl_bool_test_type)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarBooleanTest::GetDXLOperator() const
{
	return EdxlopScalarBooleanTest;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::GetDxlBoolTypeStr
//
//	@doc:
//		Boolean Test type
//
//---------------------------------------------------------------------------
EdxlBooleanTestType
CDXLScalarBooleanTest::GetDxlBoolTypeStr() const
{
	return m_dxl_bool_test_type;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarBooleanTest::GetOpNameStr() const
{
	switch (m_dxl_bool_test_type)
	{
		case EdxlbooleantestIsTrue:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolTestIsTrue);
		case EdxlbooleantestIsNotTrue:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolTestIsNotTrue);
		case EdxlbooleantestIsFalse:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolTestIsFalse);
		case EdxlbooleantestIsNotFalse:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenScalarBoolTestIsNotFalse);
		case EdxlbooleantestIsUnknown:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBoolTestIsUnknown);
		case EdxlbooleantestIsNotUnknown:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenScalarBoolTestIsNotUnknown);
		default:
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarBooleanTest::SerializeToDXL(CXMLSerializer *xml_serializer,
									  const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	GPOS_ASSERT(NULL != element_name);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBooleanTest::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarBooleanTest::AssertValid(const CDXLNode *dxlnode,
								   BOOL validate_children) const
{
	EdxlBooleanTestType dxl_bool_type =
		((CDXLScalarBooleanTest *) dxlnode->GetOperator())->GetDxlBoolTypeStr();

	GPOS_ASSERT((EdxlbooleantestIsTrue == dxl_bool_type) ||
				(EdxlbooleantestIsNotTrue == dxl_bool_type) ||
				(EdxlbooleantestIsFalse == dxl_bool_type) ||
				(EdxlbooleantestIsNotFalse == dxl_bool_type) ||
				(EdxlbooleantestIsUnknown == dxl_bool_type) ||
				(EdxlbooleantestIsNotUnknown == dxl_bool_type));

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
