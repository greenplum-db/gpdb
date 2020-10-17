//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarSortCol.cpp
//
//	@doc:
//		Implementation of DXL sorting columns for sort and motion operator nodes
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalarSortCol.h"

#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::CDXLScalarSortCol
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarSortCol::CDXLScalarSortCol(CMemoryPool *mp, ULONG colid,
									 IMDId *mdid_sort_op,
									 CWStringConst *sort_op_name_str,
									 BOOL sort_nulls_first)
	: CDXLScalar(mp),
	  m_colid(colid),
	  m_mdid_sort_op(mdid_sort_op),
	  m_sort_op_name_str(sort_op_name_str),
	  m_must_sort_nulls_first(sort_nulls_first)
{
	GPOS_ASSERT(m_mdid_sort_op->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::~CDXLScalarSortCol
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarSortCol::~CDXLScalarSortCol()
{
	m_mdid_sort_op->Release();
	GPOS_DELETE(m_sort_op_name_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarSortCol::GetDXLOperator() const
{
	return EdxlopScalarSortCol;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarSortCol::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSortCol);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::GetColId
//
//	@doc:
//		Id of the sorting column
//
//---------------------------------------------------------------------------
ULONG
CDXLScalarSortCol::GetColId() const
{
	return m_colid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::GetMdIdSortOp
//
//	@doc:
//		Oid of the sorting operator for the column from the catalog
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarSortCol::GetMdIdSortOp() const
{
	return m_mdid_sort_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::IsSortedNullsFirst
//
//	@doc:
//		Whether nulls are sorted before other values
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarSortCol::IsSortedNullsFirst() const
{
	return m_must_sort_nulls_first;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarSortCol::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *	// dxlnode
) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
								 m_colid);
	m_mdid_sort_op->Serialize(xml_serializer,
							  CDXLTokens::GetDXLTokenStr(EdxltokenSortOpId));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSortOpName), m_sort_op_name_str);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSortNullsFirst),
		m_must_sort_nulls_first);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSortCol::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarSortCol::AssertValid(const CDXLNode *dxlnode,
							   BOOL	 // validate_children
) const
{
	GPOS_ASSERT(0 == dxlnode->Arity());
}
#endif	// GPOS_DEBUG


// EOF
