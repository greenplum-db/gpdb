//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarIfStmt.cpp
//
//	@doc:
//		Implementation of DXL If Statement
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarIfStmt.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::CDXLScalarIfStmt
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarIfStmt::CDXLScalarIfStmt(CMemoryPool *mp, IMDId *result_type_mdid)
	: CDXLScalar(mp), m_result_type_mdid(result_type_mdid)
{
	GPOS_ASSERT(m_result_type_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::~CDXLScalarIfStmt
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarIfStmt::~CDXLScalarIfStmt()
{
	m_result_type_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarIfStmt::GetDXLOperator() const
{
	return EdxlopScalarIfStmt;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarIfStmt::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarIfStmt);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::GetResultTypeMdId
//
//	@doc:
//		Return type id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarIfStmt::GetResultTypeMdId() const
{
	return m_result_type_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarIfStmt::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_result_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarIfStmt::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_result_type_mdid)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIfStmt::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarIfStmt::AssertValid(const CDXLNode *node,
							  BOOL validate_children) const
{
	const ULONG arity = node->Arity();
	GPOS_ASSERT(3 == arity);

	for (ULONG idx = 0; idx < arity; ++idx)
	{
		CDXLNode *dxlnode_arg = (*node)[idx];
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
