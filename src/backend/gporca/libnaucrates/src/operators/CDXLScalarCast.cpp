//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarCast.cpp
//
//	@doc:
//		Implementation of DXL RelabelType
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarCast.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::CDXLScalarCast
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarCast::CDXLScalarCast(CMemoryPool *mp, IMDId *mdid_type,
							   IMDId *func_mdid)
	: CDXLScalar(mp), m_mdid_type(mdid_type), m_func_mdid(func_mdid)
{
	GPOS_ASSERT(NULL != m_func_mdid);
	GPOS_ASSERT(m_mdid_type->IsValid());
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::~CDXLScalarCast
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarCast::~CDXLScalarCast()
{
	m_mdid_type->Release();
	m_func_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarCast::GetDXLOperator() const
{
	return EdxlopScalarCast;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarCast::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarCast);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::MdidType
//
//	@doc:
//		Return the oid of the type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarCast::MdidType() const
{
	return m_mdid_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::FuncMdId
//
//	@doc:
//		Casting function id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarCast::FuncMdId() const
{
	return m_func_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarCast::SerializeToDXL(CXMLSerializer *xml_serializer,
							   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenFuncId));

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarCast::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_mdid_type)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCast::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarCast::AssertValid(const CDXLNode *dxlnode,
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
