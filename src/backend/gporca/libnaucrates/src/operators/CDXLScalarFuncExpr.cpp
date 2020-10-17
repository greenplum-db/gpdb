//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarFuncExpr.cpp
//
//	@doc:
//		Implementation of DXL Scalar FuncExpr
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarFuncExpr.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::CDXLScalarFuncExpr
//
//	@doc:
//		Constructs a scalar FuncExpr node
//
//---------------------------------------------------------------------------
CDXLScalarFuncExpr::CDXLScalarFuncExpr(CMemoryPool *mp, IMDId *mdid_func,
									   IMDId *mdid_return_type,
									   INT return_type_modifier, BOOL fRetSet)
	: CDXLScalar(mp),
	  m_func_mdid(mdid_func),
	  m_return_type_mdid(mdid_return_type),
	  m_return_type_modifier(return_type_modifier),
	  m_returns_set(fRetSet)
{
	GPOS_ASSERT(m_func_mdid->IsValid());
	GPOS_ASSERT(m_return_type_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::~CDXLScalarFuncExpr
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarFuncExpr::~CDXLScalarFuncExpr()
{
	m_func_mdid->Release();
	m_return_type_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarFuncExpr::GetDXLOperator() const
{
	return EdxlopScalarFuncExpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarFuncExpr::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarFuncExpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::FuncMdId
//
//	@doc:
//		Returns function id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarFuncExpr::FuncMdId() const
{
	return m_func_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::ReturnTypeMdId
//
//	@doc:
//		Return type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarFuncExpr::ReturnTypeMdId() const
{
	return m_return_type_mdid;
}

INT
CDXLScalarFuncExpr::TypeModifier() const
{
	return m_return_type_modifier;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::ReturnsSet
//
//	@doc:
//		Returns whether the function returns a set
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarFuncExpr::ReturnsSet() const
{
	return m_returns_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarFuncExpr::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenFuncId));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenFuncRetSet), m_returns_set);
	m_return_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::HasBoolResult
//
//	@doc:
//		Does the operator return boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarFuncExpr::HasBoolResult(CMDAccessor *md_accessor) const
{
	IMDId *mdid = md_accessor->RetrieveFunc(m_func_mdid)->GetResultTypeMdid();
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(mdid)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarFuncExpr::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarFuncExpr::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	for (ULONG ul = 0; ul < dxlnode->Arity(); ++ul)
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
