//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalSetOp.cpp
//
//	@doc:
//		Implementation of DXL logical set operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLLogicalSetOp.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"


using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::CDXLLogicalSetOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalSetOp::CDXLLogicalSetOp(CMemoryPool *mp, EdxlSetOpType edxlsetoptype,
								   CDXLColDescrArray *col_descr_array,
								   ULongPtr2dArray *input_colids_arrays,
								   BOOL fCastAcrossInputs)
	: CDXLLogical(mp),
	  m_set_operation_dxl_type(edxlsetoptype),
	  m_col_descr_array(col_descr_array),
	  m_input_colids_arrays(input_colids_arrays),
	  m_cast_across_input_req(fCastAcrossInputs)
{
	GPOS_ASSERT(NULL != m_col_descr_array);
	GPOS_ASSERT(NULL != m_input_colids_arrays);
	GPOS_ASSERT(EdxlsetopSentinel > edxlsetoptype);

#ifdef GPOS_DEBUG
	const ULONG num_of_cols = m_col_descr_array->Size();
	const ULONG length = m_input_colids_arrays->Size();
	for (ULONG idx = 0; idx < length; idx++)
	{
		ULongPtrArray *input_colids_array = (*m_input_colids_arrays)[idx];
		GPOS_ASSERT(num_of_cols == input_colids_array->Size());
	}

#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::~CDXLLogicalSetOp
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLLogicalSetOp::~CDXLLogicalSetOp()
{
	m_col_descr_array->Release();
	m_input_colids_arrays->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalSetOp::GetDXLOperator() const
{
	return EdxlopLogicalSetOp;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalSetOp::GetOpNameStr() const
{
	switch (m_set_operation_dxl_type)
	{
		case EdxlsetopUnion:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalUnion);

		case EdxlsetopUnionAll:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalUnionAll);

		case EdxlsetopIntersect:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalIntersect);

		case EdxlsetopIntersectAll:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalIntersectAll);

		case EdxlsetopDifference:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalDifference);

		case EdxlsetopDifferenceAll:
			return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalDifferenceAll);

		default:
			GPOS_ASSERT(!"Unrecognized set operator type");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalSetOp::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize the array of input colid arrays
	CWStringDynamic *input_colids_array_str =
		CDXLUtils::Serialize(m_mp, m_input_colids_arrays);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenInputCols),
								 input_colids_array_str);
	GPOS_DELETE(input_colids_array_str);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenCastAcrossInputs),
		m_cast_across_input_req);

	// serialize output columns
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));
	GPOS_ASSERT(NULL != m_col_descr_array);

	const ULONG length = m_col_descr_array->Size();
	for (ULONG idx = 0; idx < length; idx++)
	{
		CDXLColDescr *col_descr_dxl = (*m_col_descr_array)[idx];
		col_descr_dxl->SerializeToDXL(xml_serializer);
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalSetOp::IsColDefined
//
//	@doc:
//		Check if given column is defined by operator
//
//---------------------------------------------------------------------------
BOOL
CDXLLogicalSetOp::IsColDefined(ULONG colid) const
{
	const ULONG size = Arity();
	for (ULONG descr_id = 0; descr_id < size; descr_id++)
	{
		ULONG id = GetColumnDescrAt(descr_id)->Id();
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
//		CDXLLogicalSetOp::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalSetOp::AssertValid(const CDXLNode *node,
							  BOOL validate_children) const
{
	GPOS_ASSERT(2 <= node->Arity());
	GPOS_ASSERT(NULL != m_col_descr_array);

	// validate output columns
	const ULONG num_of_output_cols = m_col_descr_array->Size();
	GPOS_ASSERT(0 < num_of_output_cols);

	// validate children
	const ULONG num_of_child = node->Arity();
	for (ULONG idx = 0; idx < num_of_child; ++idx)
	{
		CDXLNode *child_dxlnode = (*node)[idx];
		GPOS_ASSERT(EdxloptypeLogical ==
					child_dxlnode->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}

#endif	// GPOS_DEBUG

// EOF
