//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalWindow.cpp
//
//	@doc:
//		Implementation of DXL physical window operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalWindow.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::CDXLPhysicalWindow
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalWindow::CDXLPhysicalWindow(CMemoryPool *mp,
									   ULongPtrArray *part_by_colid_array,
									   CDXLWindowKeyArray *window_key_array)
	: CDXLPhysical(mp),
	  m_part_by_colid_array(part_by_colid_array),
	  m_dxl_window_key_array(window_key_array)
{
	GPOS_ASSERT(NULL != m_part_by_colid_array);
	GPOS_ASSERT(NULL != m_dxl_window_key_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::~CDXLPhysicalWindow
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalWindow::~CDXLPhysicalWindow()
{
	m_part_by_colid_array->Release();
	m_dxl_window_key_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalWindow::GetDXLOperator() const
{
	return EdxlopPhysicalWindow;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalWindow::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalWindow);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::UlPartCols
//
//	@doc:
//		Returns the number of partition columns
//
//---------------------------------------------------------------------------
ULONG
CDXLPhysicalWindow::PartByColsCount() const
{
	return m_part_by_colid_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::UlWindowKeys
//
//	@doc:
//		Returns the number of window keys
//
//---------------------------------------------------------------------------
ULONG
CDXLPhysicalWindow::WindowKeysCount() const
{
	return m_dxl_window_key_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::PdxlWindowKey
//
//	@doc:
//		Return the window key at a given position
//
//---------------------------------------------------------------------------
CDXLWindowKey *
CDXLPhysicalWindow::GetDXLWindowKeyAt(ULONG position) const
{
	GPOS_ASSERT(position <= m_dxl_window_key_array->Size());
	return (*m_dxl_window_key_array)[position];
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalWindow::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize partition keys
	CWStringDynamic *part_by_cols_str =
		CDXLUtils::Serialize(m_mp, m_part_by_colid_array);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenPartKeys),
								 part_by_cols_str);
	GPOS_DELETE(part_by_cols_str);

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	// serialize the list of window keys
	const CWStringConst *window_keys_list_str =
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowKeyList);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		window_keys_list_str);
	const ULONG size = m_dxl_window_key_array->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLWindowKey *window_key_dxlnode = (*m_dxl_window_key_array)[ul];
		window_key_dxlnode->SerializeToDXL(xml_serializer);
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		window_keys_list_str);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalWindow::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalWindow::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);
	GPOS_ASSERT(NULL != m_part_by_colid_array);
	GPOS_ASSERT(NULL != m_dxl_window_key_array);
	GPOS_ASSERT(EdxlwindowIndexSentinel == dxlnode->Arity());
	CDXLNode *child_dxlnode = (*dxlnode)[EdxlwindowIndexChild];
	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
