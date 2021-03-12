//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryQuantified.cpp
//
//	@doc:
//		Implementation of quantified subquery operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarSubqueryQuantified.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryQuantified::CDXLScalarSubqueryQuantified
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarSubqueryQuantified::CDXLScalarSubqueryQuantified(
	CMemoryPool *mp, ULongPtrArray *colids)
	: CDXLScalar(mp), m_colids(colids)
{
	GPOS_ASSERT(nullptr != m_colids);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryQuantified::~CDXLScalarSubqueryQuantified
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarSubqueryQuantified::~CDXLScalarSubqueryQuantified()
{
	m_colids->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryQuantified::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarSubqueryQuantified::SerializeToDXL(CXMLSerializer *xml_serializer,
											 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize computed column id
	CWStringDynamic *str_colids = CDXLUtils::Serialize(m_mp, m_colids);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
								 str_colids);
	GPOS_DELETE(str_colids);

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryQuantified::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarSubqueryQuantified::AssertValid(const CDXLNode *dxlnode,
										  BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());

	CDXLNode *pdxlnScalarChild = (*dxlnode)[EdxlsqquantifiedIndexScalar];
	CDXLNode *pdxlnRelationalChild =
		(*dxlnode)[EdxlsqquantifiedIndexRelational];

	GPOS_ASSERT(EdxloptypeScalar ==
				pdxlnScalarChild->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypeLogical ==
				pdxlnRelationalChild->GetOperator()->GetDXLOperatorType());

	dxlnode->AssertValid(validate_children);
}
#endif	// GPOS_DEBUG

// EOF
