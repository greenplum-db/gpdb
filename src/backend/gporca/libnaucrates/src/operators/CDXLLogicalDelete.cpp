//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalDelete.cpp
//
//	@doc:
//		Implementation of DXL logical delete operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalDelete.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::CDXLLogicalDelete
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalDelete::CDXLLogicalDelete(CMemoryPool *mp,
									 CDXLTableDescr *table_descr,
									 ULONG ctid_colid, ULONG segid_colid,
									 ULongPtrArray *delete_colid_array)
	: CDXLLogical(mp),
	  m_dxl_table_descr(table_descr),
	  m_ctid_colid(ctid_colid),
	  m_segid_colid(segid_colid),
	  m_deletion_colid_array(delete_colid_array)
{
	GPOS_ASSERT(NULL != table_descr);
	GPOS_ASSERT(NULL != delete_colid_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::~CDXLLogicalDelete
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLLogicalDelete::~CDXLLogicalDelete()
{
	m_dxl_table_descr->Release();
	m_deletion_colid_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalDelete::GetDXLOperator() const
{
	return EdxlopLogicalDelete;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalDelete::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalDelete);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalDelete::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	CWStringDynamic *deletion_colids =
		CDXLUtils::Serialize(m_mp, m_deletion_colid_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenDeleteCols), deletion_colids);
	GPOS_DELETE(deletion_colids);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenCtidColId),
								 m_ctid_colid);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGpSegmentIdColId), m_segid_colid);

	m_dxl_table_descr->SerializeToDXL(xml_serializer);
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalDelete::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalDelete::AssertValid(const CDXLNode *node,
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
