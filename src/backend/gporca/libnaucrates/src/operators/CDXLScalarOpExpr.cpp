//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarOpExpr.cpp
//
//	@doc:
//		Implementation of DXL Scalar OpExpr
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarOpExpr.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDScalarOp.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::CDXLScalarOpExpr
//
//	@doc:
//		Constructs a scalar OpExpr node
//
//---------------------------------------------------------------------------
CDXLScalarOpExpr::CDXLScalarOpExpr(CMemoryPool *mp, IMDId *mdid_op,
								   IMDId *return_type_mdid,
								   const CWStringConst *str_opname)
	: CDXLScalar(mp),
	  m_mdid(mdid_op),
	  m_return_type_mdid(return_type_mdid),
	  m_str_opname(str_opname)
{
	GPOS_ASSERT(m_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::~CDXLScalarOpExpr
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarOpExpr::~CDXLScalarOpExpr()
{
	m_mdid->Release();
	CRefCount::SafeRelease(m_return_type_mdid);
	GPOS_DELETE(m_str_opname);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::GetScalarOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarOpExpr::GetScalarOpNameStr() const
{
	return m_str_opname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarOpExpr::GetDXLOperator() const
{
	return EdxlopScalarOpExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarOpExpr::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarOpExpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::MDId
//
//	@doc:
//		Operator id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarOpExpr::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::GetReturnTypeMdId
//
//	@doc:
//		Operator return type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarOpExpr::GetReturnTypeMdId() const
{
	return m_return_type_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::HasBoolResult
//
//	@doc:
//		Does the operator return boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarOpExpr::HasBoolResult(CMDAccessor *md_accessor) const
{
	const IMDScalarOp *md_scalar_op = md_accessor->RetrieveScOp(m_mdid);
	IMDId *mdid = md_accessor->RetrieveFunc(md_scalar_op->FuncMdId())
					  ->GetResultTypeMdid();
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(mdid)->GetDatumType());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarOpExpr::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *dxlnode) const
{
	GPOS_CHECK_ABORT;

	const CWStringConst *element_name = GetOpNameStr();
	const CWStringConst *str_opname = GetScalarOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOpName),
								 str_opname);
	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenOpNo));

	if (NULL != m_return_type_mdid)
	{
		m_return_type_mdid->Serialize(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenOpType));
	}

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	GPOS_CHECK_ABORT;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarOpExpr::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarOpExpr::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(1 == arity || 2 == arity);

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
