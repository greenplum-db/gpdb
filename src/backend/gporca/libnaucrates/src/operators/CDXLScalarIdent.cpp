//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarIdent.cpp
//
//	@doc:
//		Implementation of DXL scalar identifier operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarIdent.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;



//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::CDXLScalarIdent
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarIdent::CDXLScalarIdent(CMemoryPool *mp, CDXLColRef *dxl_colref)
	: CDXLScalar(mp), m_dxl_colref(dxl_colref)
{
	GPOS_ASSERT(NULL != m_dxl_colref);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::~CDXLScalarIdent
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarIdent::~CDXLScalarIdent()
{
	m_dxl_colref->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarIdent::GetDXLOperator() const
{
	return EdxlopScalarIdent;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarIdent::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarIdent);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::MakeDXLColRef
//
//	@doc:
//		Return column reference of the identifier operator
//
//---------------------------------------------------------------------------
const CDXLColRef *
CDXLScalarIdent::GetDXLColRef() const
{
	return m_dxl_colref;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::MdidType
//
//	@doc:
//		Return the id of the column type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarIdent::MdidType() const
{
	return m_dxl_colref->MdidType();
}

INT
CDXLScalarIdent::TypeModifier() const
{
	return m_dxl_colref->TypeModifier();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarIdent::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// add col name and col id
	const CWStringConst *colname = (m_dxl_colref->MdName())->GetMDName();

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
								 m_dxl_colref->Id());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColName),
								 colname);
	m_dxl_colref->MdidType()->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}

	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::HasBoolResult
//
//	@doc:
//		Does the operator return boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarIdent::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (
		IMDType::EtiBool ==
		md_accessor->RetrieveType(m_dxl_colref->MdidType())->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarIdent::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarIdent::AssertValid(const CDXLNode *node,
							 BOOL  // validate_children
) const
{
	GPOS_ASSERT(0 == node->Arity());
	GPOS_ASSERT(m_dxl_colref->MdidType()->IsValid());
	GPOS_ASSERT(NULL != m_dxl_colref);
}
#endif	// GPOS_DEBUG

// EOF
