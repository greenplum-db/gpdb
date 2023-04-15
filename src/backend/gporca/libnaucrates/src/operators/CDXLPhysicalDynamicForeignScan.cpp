//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CDXLPhysicalDynamicForeignScan.cpp
//
//	@doc:
//		Implementation of DXL physical dynamic foreign scan operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalDynamicForeignScan.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDCacheObject.h"

using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::CDXLPhysicalDynamicForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicForeignScan::CDXLPhysicalDynamicForeignScan(
	CMemoryPool *mp, CDXLTableDescr *table_descr, IMdIdArray *part_mdids,
	ULongPtrArray *selector_ids, OID foreign_server_oid)
	: CDXLPhysical(mp),
	  m_dxl_table_descr(table_descr),
	  m_part_mdids(part_mdids),
	  m_selector_ids(selector_ids),
	  m_foreign_server_oid(foreign_server_oid)

{
	GPOS_ASSERT(nullptr != table_descr);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::~CDXLPhysicalDynamicForeignScan
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLPhysicalDynamicForeignScan::~CDXLPhysicalDynamicForeignScan()
{
	m_dxl_table_descr->Release();
	m_part_mdids->Release();
	CRefCount::SafeRelease(m_selector_ids);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalDynamicForeignScan::GetDXLOperator() const
{
	return EdxlopPhysicalDynamicForeignScan;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDynamicForeignScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDynamicForeignScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::GetDXLTableDescr
//
//	@doc:
//		Table descriptor for the foreign scan
//
//---------------------------------------------------------------------------
const CDXLTableDescr *
CDXLPhysicalDynamicForeignScan::GetDXLTableDescr() const
{
	return m_dxl_table_descr;
}

IMdIdArray *
CDXLPhysicalDynamicForeignScan::GetParts() const
{
	return m_part_mdids;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicForeignScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicForeignScan::SerializeToDXL(CXMLSerializer *xml_serializer,
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
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenForeignServerOid),
		m_foreign_server_oid);
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
//		CDXLPhysicalDynamicForeignScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicForeignScan::AssertValid(const CDXLNode *node,
											BOOL  // validate_children
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
