//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalInsert.cpp
//
//	@doc:
//		Implementation of DXL logical insert operator
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLLogicalInsert.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::CDXLLogicalInsert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalInsert::CDXLLogicalInsert(CMemoryPool *mp,
									 CDXLTableDescr *table_descr,
									 ULongPtrArray *src_colids_array)
	: CDXLLogical(mp),
	  m_dxl_table_descr(table_descr),
	  m_src_colids_array(src_colids_array)
{
	GPOS_ASSERT(NULL != table_descr);
	GPOS_ASSERT(NULL != src_colids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::~CDXLLogicalInsert
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLLogicalInsert::~CDXLLogicalInsert()
{
	m_dxl_table_descr->Release();
	m_src_colids_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalInsert::GetDXLOperator() const
{
	return EdxlopLogicalInsert;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalInsert::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalInsert);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalInsert::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	CWStringDynamic *src_colids =
		CDXLUtils::Serialize(m_mp, m_src_colids_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInsertCols), src_colids);
	GPOS_DELETE(src_colids);

	// serialize table descriptor
	m_dxl_table_descr->SerializeToDXL(xml_serializer);

	// serialize arguments
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalInsert::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalInsert::AssertValid(const CDXLNode *node,
							   BOOL validate_children) const
{
	GPOS_ASSERT(1 == node->Arity());

	CDXLNode *child_dxlnode = (*node)[0];
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG


// EOF
