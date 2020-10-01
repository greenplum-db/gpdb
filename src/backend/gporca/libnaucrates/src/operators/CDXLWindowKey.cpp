//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowKey.cpp
//
//	@doc:
//		Implementation of DXL window key
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLWindowKey.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowKey::CDXLWindowKey
//
//	@doc:
//		Constructs a scalar window key node
//
//---------------------------------------------------------------------------
CDXLWindowKey::CDXLWindowKey(CMemoryPool *mp)
	: m_mp(mp), m_window_frame_dxl(NULL), m_sort_col_list_dxlnode(NULL)
{
	GPOS_ASSERT(NULL != m_mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowKey::~CDXLWindowKey
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLWindowKey::~CDXLWindowKey()
{
	CRefCount::SafeRelease(m_window_frame_dxl);
	CRefCount::SafeRelease(m_sort_col_list_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowKey::SetWindowFrame
//
//	@doc:
//		Set window frame
//
//---------------------------------------------------------------------------
void
CDXLWindowKey::SetWindowFrame(CDXLWindowFrame *window_frame)
{
	// allow setting window frame only once
	GPOS_ASSERT(NULL == m_window_frame_dxl);
	m_window_frame_dxl = window_frame;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowKey::SetSortColList
//
//	@doc:
//		Set sort column list
//
//---------------------------------------------------------------------------
void
CDXLWindowKey::SetSortColList(CDXLNode *sort_col_list_dxlnode)
{
	// allow setting window frame only once
	GPOS_ASSERT(NULL == m_sort_col_list_dxlnode);
	m_sort_col_list_dxlnode = sort_col_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowKey::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLWindowKey::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	const CWStringConst *element_name =
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowKey);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	if (NULL != m_sort_col_list_dxlnode)
	{
		m_sort_col_list_dxlnode->SerializeToDXL(xml_serializer);
	}

	if (NULL != m_window_frame_dxl)
	{
		m_window_frame_dxl->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

// EOF
