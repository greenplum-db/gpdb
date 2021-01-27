//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Inc.
//
//	@filename:
//		CDXLScalarArrayCoerceExpr.cpp
//
//	@doc:
//		Implementation of DXL scalar array coerce expr
//
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarArrayCoerceExpr.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayCoerceExpr::CDXLScalarArrayCoerceExpr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarArrayCoerceExpr::CDXLScalarArrayCoerceExpr(
	CMemoryPool *mp, IMDId *coerce_func_mdid, IMDId *result_type_mdid,
	INT type_modifier, BOOL is_explicit, EdxlCoercionForm coerce_format,
	INT location)
	: CDXLScalarCoerceBase(mp, result_type_mdid, type_modifier, coerce_format,
						   location),
	  m_coerce_func_mdid(coerce_func_mdid),
	  m_explicit(is_explicit)
{
	GPOS_ASSERT(NULL != coerce_func_mdid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayCoerceExpr::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarArrayCoerceExpr::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarArrayCoerceExpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArrayCoerceExpr::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarArrayCoerceExpr::SerializeToDXL(CXMLSerializer *xml_serializer,
										  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	m_coerce_func_mdid->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenElementFunc));
	GetResultTypeMdId()->Serialize(xml_serializer,
								   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIsExplicit), m_explicit);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenCoercionForm),
		(ULONG) GetDXLCoercionForm());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenLocation),
								 GetLocation());

	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

// EOF
