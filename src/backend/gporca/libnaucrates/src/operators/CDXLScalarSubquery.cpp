//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubquery.cpp
//
//	@doc:
//		Implementation of subqueries computing scalar values
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLScalarSubquery.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::CDXLScalarSubquery
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarSubquery::CDXLScalarSubquery(CMemoryPool *mp, ULONG colid)
	: CDXLScalar(mp), m_colid(colid)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::~CDXLScalarSubquery
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarSubquery::~CDXLScalarSubquery()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarSubquery::GetDXLOperator() const
{
	return EdxlopScalarSubquery;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarSubquery::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSubquery);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarSubquery::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize computed column id
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
								 m_colid);

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubquery::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarSubquery::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	GPOS_ASSERT(1 == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[0];
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	dxlnode->AssertValid(validate_children);
}
#endif	// GPOS_DEBUG

// EOF
