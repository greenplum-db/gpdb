//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Inc.
//
//	@filename:
//		CDXLScalarCoerceBase.cpp
//
//	@doc:
//		Implementation of DXL scalar coerce base class
//
//	@owner:
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarCoerceBase.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceBase::CDXLScalarCoerceBase
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarCoerceBase::CDXLScalarCoerceBase(CMemoryPool *mp, IMDId *mdid_type,
										   INT type_modifier,
										   EdxlCoercionForm dxl_coerce_format,
										   INT location)
	: CDXLScalar(mp),
	  m_result_type_mdid(mdid_type),
	  m_type_modifier(type_modifier),
	  m_dxl_coerce_format(dxl_coerce_format),
	  m_location(location)
{
	GPOS_ASSERT(NULL != mdid_type);
	GPOS_ASSERT(mdid_type->IsValid());
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceBase::~CDXLScalarCoerceBase
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarCoerceBase::~CDXLScalarCoerceBase()
{
	m_result_type_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceBase::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarCoerceBase::SerializeToDXL(CXMLSerializer *xml_serializer,
									 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	m_result_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenCoercionForm),
		(ULONG) m_dxl_coerce_format);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenLocation),
								 m_location);

	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceBase::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarCoerceBase::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_result_type_mdid)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceBase::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarCoerceBase::AssertValid(const CDXLNode *node,
								  BOOL validate_children) const
{
	GPOS_ASSERT(1 == node->Arity());

	CDXLNode *child_dxlnode = (*node)[0];
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
