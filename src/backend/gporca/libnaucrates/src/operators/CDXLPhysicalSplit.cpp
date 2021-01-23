//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalSplit.cpp
//
//	@doc:
//		Implementation of DXL physical Split operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalSplit.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::CDXLPhysicalSplit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalSplit::CDXLPhysicalSplit(CMemoryPool *mp,
									 ULongPtrArray *delete_colid_array,
									 ULongPtrArray *insert_colid_array,
									 ULONG action_colid, ULONG ctid_colid,
									 ULONG segid_colid, BOOL preserve_oids,
									 ULONG tuple_oid)
	: CDXLPhysical(mp),
	  m_deletion_colid_array(delete_colid_array),
	  m_insert_colid_array(insert_colid_array),
	  m_action_colid(action_colid),
	  m_ctid_colid(ctid_colid),
	  m_segid_colid(segid_colid),
	  m_preserve_oids(preserve_oids),
	  m_tuple_oid(tuple_oid)
{
	GPOS_ASSERT(NULL != delete_colid_array);
	GPOS_ASSERT(NULL != insert_colid_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::~CDXLPhysicalSplit
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalSplit::~CDXLPhysicalSplit()
{
	m_deletion_colid_array->Release();
	m_insert_colid_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalSplit::GetDXLOperator() const
{
	return EdxlopPhysicalSplit;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalSplit::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalSplit);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSplit::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	CWStringDynamic *delete_cols =
		CDXLUtils::Serialize(m_mp, m_deletion_colid_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenDeleteCols), delete_cols);
	GPOS_DELETE(delete_cols);

	CWStringDynamic *insert_cols =
		CDXLUtils::Serialize(m_mp, m_insert_colid_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInsertCols), insert_cols);
	GPOS_DELETE(insert_cols);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenActionColId), m_action_colid);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenCtidColId),
								 m_ctid_colid);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGpSegmentIdColId), m_segid_colid);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenUpdatePreservesOids),
		m_preserve_oids);

	if (m_preserve_oids)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTupleOidColId), m_tuple_oid);
	}

	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize project list
	(*dxlnode)[0]->SerializeToDXL(xml_serializer);

	// serialize physical child
	(*dxlnode)[1]->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSplit::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSplit::AssertValid(const CDXLNode *dxlnode,
							   BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG


// EOF
