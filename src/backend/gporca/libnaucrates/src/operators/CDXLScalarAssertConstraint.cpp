//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CDXLScalarAssertConstraint.cpp
//
//	@doc:
//		Implementation of DXL scalar assert predicate
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarAssertConstraint.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::CDXLScalarAssertConstraint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarAssertConstraint::CDXLScalarAssertConstraint(CMemoryPool *mp,
													   CWStringBase *error_msg)
	: CDXLScalar(mp), m_error_msg(error_msg)
{
	GPOS_ASSERT(NULL != error_msg);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::~CDXLScalarAssertConstraint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarAssertConstraint::~CDXLScalarAssertConstraint()
{
	GPOS_DELETE(m_error_msg);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarAssertConstraint::GetDXLOperator() const
{
	return EdxlopScalarAssertConstraint;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarAssertConstraint::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarAssertConstraint);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::PstrErrorMsg
//
//	@doc:
//		Error message
//
//---------------------------------------------------------------------------
CWStringBase *
CDXLScalarAssertConstraint::GetErrorMsgStr() const
{
	return m_error_msg;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarAssertConstraint::SerializeToDXL(CXMLSerializer *xml_serializer,
										   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenErrorMessage), m_error_msg);

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAssertConstraint::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarAssertConstraint::AssertValid(const CDXLNode *dxlnode,
										BOOL validate_children) const
{
	GPOS_ASSERT(1 == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[0];
	GPOS_ASSERT(EdxloptypeScalar ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG

// EOF
