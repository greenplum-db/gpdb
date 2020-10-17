//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CDXLPhysicalDynamicIndexScan.cpp
//
//	@doc:
//		Implementation of DXL physical dynamic index scan operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalDynamicIndexScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::CDXLPhysicalDynamicIndexScan
//
//	@doc:
//		Construct an index scan node given its table descriptor,
//		index descriptor and filter conditions on the index
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicIndexScan::CDXLPhysicalDynamicIndexScan(
	CMemoryPool *mp, CDXLTableDescr *table_descr, ULONG part_idx_id,
	ULONG part_idx_id_printable, CDXLIndexDescr *dxl_index_descr,
	EdxlIndexScanDirection idx_scan_direction)
	: CDXLPhysical(mp),
	  m_dxl_table_descr(table_descr),
	  m_part_index_id(part_idx_id),
	  m_part_index_id_printable(part_idx_id_printable),
	  m_dxl_index_descr(dxl_index_descr),
	  m_index_scan_dir(idx_scan_direction)
{
	GPOS_ASSERT(NULL != m_dxl_table_descr);
	GPOS_ASSERT(NULL != m_dxl_index_descr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::~CDXLPhysicalDynamicIndexScan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicIndexScan::~CDXLPhysicalDynamicIndexScan()
{
	m_dxl_index_descr->Release();
	m_dxl_table_descr->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalDynamicIndexScan::GetDXLOperator() const
{
	return EdxlopPhysicalDynamicIndexScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDynamicIndexScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDynamicIndexScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::MakeDXLIndexDescr
//
//	@doc:
//		Index descriptor for the index scan
//
//---------------------------------------------------------------------------
const CDXLIndexDescr *
CDXLPhysicalDynamicIndexScan::GetDXLIndexDescr() const
{
	return m_dxl_index_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetIndexScanDir
//
//	@doc:
//		Return the scan direction of the index
//
//---------------------------------------------------------------------------
EdxlIndexScanDirection
CDXLPhysicalDynamicIndexScan::GetIndexScanDir() const
{
	return m_index_scan_dir;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetDXLTableDescr
//
//	@doc:
//		Return the associated table descriptor
//
//---------------------------------------------------------------------------
const CDXLTableDescr *
CDXLPhysicalDynamicIndexScan::GetDXLTableDescr() const
{
	return m_dxl_table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetPartIndexId
//
//	@doc:
//		Part index id
//
//---------------------------------------------------------------------------
ULONG
CDXLPhysicalDynamicIndexScan::GetPartIndexId() const
{
	return m_part_index_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::GetPartIndexIdPrintable
//
//	@doc:
//		Printable partition index id
//
//---------------------------------------------------------------------------
ULONG
CDXLPhysicalDynamicIndexScan::GetPartIndexIdPrintable() const
{
	return m_part_index_id_printable;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicIndexScan::SerializeToDXL(CXMLSerializer *xml_serializer,
											 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexScanDirection),
		CDXLOperator::GetIdxScanDirectionStr(m_index_scan_dir));

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenPartIndexId), m_part_index_id);
	if (m_part_index_id_printable != m_part_index_id)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenPartIndexIdPrintable),
			m_part_index_id_printable);
	}

	// serialize properties
	node->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	// serialize index descriptor
	m_dxl_index_descr->SerializeToDXL(xml_serializer);

	// serialize table descriptor
	m_dxl_table_descr->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicIndexScan::AssertValid(const CDXLNode *node,
										  BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(node, validate_children);

	// index scan has only 3 children
	GPOS_ASSERT(3 == node->Arity());

	// assert validity of the index descriptor
	GPOS_ASSERT(NULL != m_dxl_index_descr);
	GPOS_ASSERT(NULL != m_dxl_index_descr->MdName());
	GPOS_ASSERT(m_dxl_index_descr->MdName()->GetMDName()->IsValid());

	// assert validity of the table descriptor
	GPOS_ASSERT(NULL != m_dxl_table_descr);
	GPOS_ASSERT(NULL != m_dxl_table_descr->MdName());
	GPOS_ASSERT(m_dxl_table_descr->MdName()->GetMDName()->IsValid());

	CDXLNode *index_filter_dxlnode = (*node)[EdxldisIndexFilter];
	CDXLNode *index_cond_dxlnode = (*node)[EdxldisIndexCondition];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarIndexCondList ==
				index_cond_dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxlopScalarFilter ==
				index_filter_dxlnode->GetOperator()->GetDXLOperator());

	if (validate_children)
	{
		index_cond_dxlnode->GetOperator()->AssertValid(index_cond_dxlnode,
													   validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
