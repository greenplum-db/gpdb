//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowSpec.cpp
//
//	@doc:
//		Implementation of DXL window specification in the DXL
//		representation of the logical query tree
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLWindowSpec.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowSpec::CDXLWindowSpec
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLWindowSpec::CDXLWindowSpec(CMemoryPool *mp,
							   ULongPtrArray *partition_by_colid_array,
							   CMDName *mdname, CDXLNode *sort_col_list_dxlnode,
							   CDXLWindowFrame *window_frame)
	: m_mp(mp),
	  m_partition_by_colid_array(partition_by_colid_array),
	  m_mdname(mdname),
	  m_sort_col_list_dxlnode(sort_col_list_dxlnode),
	  m_window_frame(window_frame)
{
	GPOS_ASSERT(NULL != m_mp);
	GPOS_ASSERT(NULL != m_partition_by_colid_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowSpec::~CDXLWindowSpec
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLWindowSpec::~CDXLWindowSpec()
{
	m_partition_by_colid_array->Release();
	CRefCount::SafeRelease(m_window_frame);
	CRefCount::SafeRelease(m_sort_col_list_dxlnode);
	GPOS_DELETE(m_mdname);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowSpec::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLWindowSpec::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	const CWStringConst *element_name =
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowSpec);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	GPOS_ASSERT(NULL != m_partition_by_colid_array);

	// serialize partition keys
	CWStringDynamic *partition_by_colid_string =
		CDXLUtils::Serialize(m_mp, m_partition_by_colid_array);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenPartKeys),
								 partition_by_colid_string);
	GPOS_DELETE(partition_by_colid_string);

	if (NULL != m_mdname)
	{
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAlias),
									 m_mdname->GetMDName());
	}

	// serialize sorting columns
	if (NULL != m_sort_col_list_dxlnode)
	{
		m_sort_col_list_dxlnode->SerializeToDXL(xml_serializer);
	}

	// serialize window frames
	if (NULL != m_window_frame)
	{
		m_window_frame->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

// EOF
