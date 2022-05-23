//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalDynamicBitmapTableScan.cpp
//
//	@doc:
//		Class for representing DXL bitmap table scan operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalDynamicBitmapTableScan.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/md/IMDCacheObject.h"

using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicBitmapTableScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDynamicBitmapTableScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDynamicBitmapTableScan);
}

IMdIdArray *
CDXLPhysicalDynamicBitmapTableScan::GetParts() const
{
	return m_part_mdids;
}

CDXLPhysicalDynamicBitmapTableScan::~CDXLPhysicalDynamicBitmapTableScan()
{
	m_part_mdids->Release();
	CRefCount::SafeRelease(m_selector_ids);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicBitmapTableScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalDynamicBitmapTableScan::SerializeToDXL(
	CXMLSerializer *xml_serializer, const CDXLNode *node) const
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

// EOF
