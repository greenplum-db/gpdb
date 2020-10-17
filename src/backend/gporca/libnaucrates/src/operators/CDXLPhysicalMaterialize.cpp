//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMaterialize.cpp
//
//	@doc:
//		Implementation of DXL physical materialize operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalMaterialize.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMaterialize::CDXLPhysicalMaterialize
//
//	@doc:
//		Construct a non-spooling materialize
//
//---------------------------------------------------------------------------
CDXLPhysicalMaterialize::CDXLPhysicalMaterialize(CMemoryPool *mp, BOOL is_eager)
	: CDXLPhysical(mp),
	  m_is_eager(is_eager),
	  m_spooling_op_id(0),
	  m_spool_type(EdxlspoolNone),
	  m_executor_slice(-1),
	  m_num_consumer_slices(0)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMaterialize::CDXLPhysicalMaterialize
//
//	@doc:
//		Construct a spooling materialize
//
//---------------------------------------------------------------------------
CDXLPhysicalMaterialize::CDXLPhysicalMaterialize(CMemoryPool *mp, BOOL is_eager,
												 ULONG spooling_op_id,
												 INT executor_slice,
												 ULONG num_consumer_slices)
	: CDXLPhysical(mp),
	  m_is_eager(is_eager),
	  m_spooling_op_id(spooling_op_id),
	  m_spool_type(EdxlspoolMaterialize),
	  m_executor_slice(executor_slice),
	  m_num_consumer_slices(num_consumer_slices)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMaterialize::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalMaterialize::GetDXLOperator() const
{
	return EdxlopPhysicalMaterialize;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMaterialize::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalMaterialize::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalMaterialize);
}

//		Is this a spooling materialize operator
BOOL
CDXLPhysicalMaterialize::IsSpooling() const
{
	return (EdxlspoolNone != m_spool_type);
}

//		Id of the spool if the materialize node is spooling
ULONG
CDXLPhysicalMaterialize::GetSpoolingOpId() const
{
	return m_spooling_op_id;
}

//		Id of the slice executing the spool
INT
CDXLPhysicalMaterialize::GetExecutorSlice() const
{
	return m_executor_slice;
}

//		Number of slices consuming the spool
ULONG
CDXLPhysicalMaterialize::GetNumConsumerSlices() const
{
	return m_num_consumer_slices;
}


//		Does the materialize node do eager materialization
BOOL
CDXLPhysicalMaterialize::IsEager() const
{
	return m_is_eager;
}

//		Serialize operator in DXL format
void
CDXLPhysicalMaterialize::SerializeToDXL(CXMLSerializer *xml_serializer,
										const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenMaterializeEager), m_is_eager);

	if (EdxlspoolMaterialize == m_spool_type)
	{
		// serialize spool info
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenSpoolId), m_spooling_op_id);

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenExecutorSliceId),
			m_executor_slice);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenConsumerSliceCount),
			m_num_consumer_slices);
	}

	// serialize properties
	node->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMaterialize::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMaterialize::AssertValid(const CDXLNode *node,
									 BOOL validate_children) const
{
	GPOS_ASSERT(EdxlspoolNone == m_spool_type ||
				EdxlspoolMaterialize == m_spool_type);
	GPOS_ASSERT(EdxlmatIndexSentinel == node->Arity());

	CDXLNode *child_dxlnode = (*node)[EdxlmatIndexChild];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
