//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalDML.cpp
//
//	@doc:
//		Implementation of DXL physical DML operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"
#include "naucrates/dxl/operators/CDXLPhysicalDML.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::CDXLPhysicalDML
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalDML::CDXLPhysicalDML(
	CMemoryPool *mp, const EdxlDmlType dxl_dml_type,
	CDXLTableDescr *table_descr, ULongPtrArray *src_colids_array,
	ULONG action_colid, ULONG oid_colid, ULONG ctid_colid, ULONG segid_colid,
	BOOL preserve_oids, ULONG tuple_oid,
	CDXLDirectDispatchInfo *dxl_direct_dispatch_info, BOOL input_sort_req)
	: CDXLPhysical(mp),
	  m_dxl_dml_type(dxl_dml_type),
	  m_dxl_table_descr(table_descr),
	  m_src_colids_array(src_colids_array),
	  m_action_colid(action_colid),
	  m_oid_colid(oid_colid),
	  m_ctid_colid(ctid_colid),
	  m_segid_colid(segid_colid),
	  m_preserve_oids(preserve_oids),
	  m_tuple_oid(tuple_oid),
	  m_direct_dispatch_info(dxl_direct_dispatch_info),
	  m_input_sort_req(input_sort_req)
{
	GPOS_ASSERT(EdxldmlSentinel > dxl_dml_type);
	GPOS_ASSERT(NULL != table_descr);
	GPOS_ASSERT(NULL != src_colids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::~CDXLPhysicalDML
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalDML::~CDXLPhysicalDML()
{
	m_dxl_table_descr->Release();
	m_src_colids_array->Release();
	CRefCount::SafeRelease(m_direct_dispatch_info);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalDML::GetDXLOperator() const
{
	return EdxlopPhysicalDML;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDML::GetOpNameStr() const
{
	switch (m_dxl_dml_type)
	{
		case Edxldmlinsert:
			return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDMLInsert);
		case Edxldmldelete:
			return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDMLDelete);
		case Edxldmlupdate:
			return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDMLUpdate);
		default:
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDML::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	CWStringDynamic *pstrCols = CDXLUtils::Serialize(m_mp, m_src_colids_array);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColumns),
								 pstrCols);
	GPOS_DELETE(pstrCols);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenActionColId), m_action_colid);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOidColId),
								 m_oid_colid);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenCtidColId),
								 m_ctid_colid);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGpSegmentIdColId), m_segid_colid);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInputSorted), m_input_sort_req);

	if (Edxldmlupdate == m_dxl_dml_type)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenUpdatePreservesOids),
			m_preserve_oids);
	}

	if (m_preserve_oids)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTupleOidColId), m_tuple_oid);
	}

	node->SerializePropertiesToDXL(xml_serializer);

	if (NULL != m_direct_dispatch_info)
	{
		m_direct_dispatch_info->Serialize(xml_serializer);
	}
	else
	{
		// TODO:  - Oct 22, 2014; clean this code once the direct dispatch code for DML and SELECT is unified
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchInfo));
		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchInfo));
	}

	// serialize project list
	(*node)[0]->SerializeToDXL(xml_serializer);

	// serialize table descriptor
	m_dxl_table_descr->SerializeToDXL(xml_serializer);

	// serialize physical child
	(*node)[1]->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDML::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDML::AssertValid(const CDXLNode *node, BOOL validate_children) const
{
	GPOS_ASSERT(2 == node->Arity());
	CDXLNode *child_dxlnode = (*node)[1];
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
