//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLLogicalConstTable.cpp
//
//	@doc:
//		Implementation of DXL logical constant tables
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalConstTable.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::CDXLLogicalConstTable
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLLogicalConstTable::CDXLLogicalConstTable(
	CMemoryPool *mp, CDXLColDescrArray *col_descr_array,
	CDXLDatum2dArray *const_tuples_datum_array)
	: CDXLLogical(mp),
	  m_col_descr_array(col_descr_array),
	  m_const_tuples_datum_array(const_tuples_datum_array)
{
	GPOS_ASSERT(NULL != col_descr_array);
	GPOS_ASSERT(NULL != const_tuples_datum_array);

#ifdef GPOS_DEBUG
	const ULONG length = const_tuples_datum_array->Size();
	for (ULONG idx = 0; idx < length; idx++)
	{
		CDXLDatumArray *pdrgpdxldatum = (*const_tuples_datum_array)[idx];
		GPOS_ASSERT(pdrgpdxldatum->Size() == col_descr_array->Size());
	}
#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::~CDXLLogicalConstTable
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLLogicalConstTable::~CDXLLogicalConstTable()
{
	m_col_descr_array->Release();
	m_const_tuples_datum_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalConstTable::GetDXLOperator() const
{
	return EdxlopLogicalConstTable;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalConstTable::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalConstTable);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::GetColumnDescrAt
//
//	@doc:
//		Type of const table element at given position
//
//---------------------------------------------------------------------------
CDXLColDescr *
CDXLLogicalConstTable::GetColumnDescrAt(ULONG idx) const
{
	GPOS_ASSERT(m_col_descr_array->Size() > idx);
	return (*m_col_descr_array)[idx];
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::Arity
//
//	@doc:
//		Const table arity
//
//---------------------------------------------------------------------------
ULONG
CDXLLogicalConstTable::Arity() const
{
	return m_col_descr_array->Size();
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalConstTable::SerializeToDXL(CXMLSerializer *xml_serializer,
									  const CDXLNode *	//dxlnode
) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize columns
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	for (ULONG idx = 0; idx < Arity(); idx++)
	{
		CDXLColDescr *col_descr = (*m_col_descr_array)[idx];
		col_descr->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	const CWStringConst *pstrElemNameConstTuple =
		CDXLTokens::GetDXLTokenStr(EdxltokenConstTuple);
	const CWStringConst *pstrElemNameDatum =
		CDXLTokens::GetDXLTokenStr(EdxltokenDatum);

	const ULONG num_of_tuples = m_const_tuples_datum_array->Size();
	for (ULONG tuple_idx = 0; tuple_idx < num_of_tuples; tuple_idx++)
	{
		// serialize a const tuple
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			pstrElemNameConstTuple);
		CDXLDatumArray *pdrgpdxldatum =
			(*m_const_tuples_datum_array)[tuple_idx];

		const ULONG num_of_cols = pdrgpdxldatum->Size();
		for (ULONG idx = 0; idx < num_of_cols; idx++)
		{
			CDXLDatum *dxl_datum = (*pdrgpdxldatum)[idx];
			dxl_datum->Serialize(xml_serializer, pstrElemNameDatum);
		}

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			pstrElemNameConstTuple);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::IsColDefined
//
//	@doc:
//		Check if given column is defined by operator
//
//---------------------------------------------------------------------------
BOOL
CDXLLogicalConstTable::IsColDefined(ULONG colid) const
{
	const ULONG size = Arity();
	for (ULONG descr_idx = 0; descr_idx < size; descr_idx++)
	{
		ULONG id = GetColumnDescrAt(descr_idx)->Id();
		if (id == colid)
		{
			return true;
		}
	}

	return false;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalConstTable::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalConstTable::AssertValid(const CDXLNode *node,
								   BOOL	 //validate_children
) const
{
	// assert validity of col descr
	GPOS_ASSERT(m_col_descr_array != NULL);
	GPOS_ASSERT(0 < m_col_descr_array->Size());
	GPOS_ASSERT(0 == node->Arity());
}
#endif	// GPOS_DEBUG

// EOF
