//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalDynamicTableScan.cpp
//
//	@doc:
//		Implementation of DXL physical dynamic table scan operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalDynamicTableScan.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDCacheObject.h"

using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::CDXLPhysicalDynamicTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicTableScan::CDXLPhysicalDynamicTableScan(
	CMemoryPool *mp, CDXLTableDescr *table_descr, IMdIdArray *part_mdids,
	ULongPtrArray *selector_ids)
	: CDXLPhysical(mp),
	  m_dxl_table_descr(table_descr),
	  m_part_mdids(part_mdids),
	  m_selector_ids(selector_ids)

{
	GPOS_ASSERT(nullptr != table_descr);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::~CDXLPhysicalDynamicTableScan
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicTableScan::~CDXLPhysicalDynamicTableScan()
{
	m_dxl_table_descr->Release();
	m_part_mdids->Release();
	CRefCount::SafeRelease(m_selector_ids);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalDynamicTableScan::GetDXLOperator() const
{
	return EdxlopPhysicalDynamicTableScan;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDynamicTableScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDynamicTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::GetDXLTableDescr
//
//	@doc:
//		Table descriptor for the table scan
//
//---------------------------------------------------------------------------
const CDXLTableDescr *
CDXLPhysicalDynamicTableScan::GetDXLTableDescr() const
{
	return m_dxl_table_descr;
}

IMdIdArray *
CDXLPhysicalDynamicTableScan::GetParts() const
{
	return m_part_mdids;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicTableScan::SerializeToDXL(CXMLSerializer *xml_serializer,
											 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	CWStringDynamic *serialized_selector_ids =
		CDXLUtils::Serialize(m_mp, m_selector_ids);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSelectorIds),
		serialized_selector_ids);
	GPOS_DELETE(serialized_selector_ids);
	node->SerializePropertiesToDXL(xml_serializer);
	node->SerializeChildrenToDXL(xml_serializer);
	IMDCacheObject::SerializeMDIdList(
		xml_serializer, m_part_mdids,
		CDXLTokens::GetDXLTokenStr(EdxltokenPartitions),
		CDXLTokens::GetDXLTokenStr(EdxltokenPartition));
	m_dxl_table_descr->SerializeToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicTableScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicTableScan::AssertValid(const CDXLNode *node,
										  BOOL	// validate_children
) const
{
	GPOS_ASSERT(2 == node->Arity());

	// assert validity of table descriptor
	GPOS_ASSERT(nullptr != m_dxl_table_descr);
	GPOS_ASSERT(nullptr != m_dxl_table_descr->MdName());
	GPOS_ASSERT(m_dxl_table_descr->MdName()->GetMDName()->IsValid());
}
#endif	// GPOS_DEBUG

// EOF
