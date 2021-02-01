//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarWindowFrameEdge.cpp
//
//	@doc:
//		Implementation of DXL scalar window frame edge
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarWindowFrameEdge.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::CDXLScalarWindowFrameEdge
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarWindowFrameEdge::CDXLScalarWindowFrameEdge(
	CMemoryPool *mp, BOOL fLeading, EdxlFrameBoundary frame_boundary)
	: CDXLScalar(mp),
	  m_leading_edge(fLeading),
	  m_dxl_frame_boundary(frame_boundary)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarWindowFrameEdge::GetDXLOperator() const
{
	return EdxlopScalarWindowFrameEdge;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarWindowFrameEdge::GetOpNameStr() const
{
	if (m_leading_edge)
	{
		return CDXLTokens::GetDXLTokenStr(
			EdxltokenScalarWindowFrameLeadingEdge);
	}

	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarWindowFrameTrailingEdge);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::GetFrameBoundaryStr
//
//	@doc:
//		Return the string representation of the window frame boundary
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarWindowFrameEdge::GetFrameBoundaryStr(
	EdxlFrameBoundary frame_boundary) const
{
	GPOS_ASSERT(EdxlfbSentinel > frame_boundary);

	ULONG dxl_frame_boundary_token_mapping[][2] = {
		{EdxlfbUnboundedPreceding, EdxltokenWindowBoundaryUnboundedPreceding},
		{EdxlfbBoundedPreceding, EdxltokenWindowBoundaryBoundedPreceding},
		{EdxlfbCurrentRow, EdxltokenWindowBoundaryCurrentRow},
		{EdxlfbUnboundedFollowing, EdxltokenWindowBoundaryUnboundedFollowing},
		{EdxlfbBoundedFollowing, EdxltokenWindowBoundaryBoundedFollowing},
		{EdxlfbDelayedBoundedPreceding,
		 EdxltokenWindowBoundaryDelayedBoundedPreceding},
		{EdxlfbDelayedBoundedFollowing,
		 EdxltokenWindowBoundaryDelayedBoundedFollowing}};

	const ULONG arity = GPOS_ARRAY_SIZE(dxl_frame_boundary_token_mapping);
	for (ULONG idx = 0; idx < arity; idx++)
	{
		ULONG *element = dxl_frame_boundary_token_mapping[idx];
		if ((ULONG) frame_boundary == element[0])
		{
			Edxltoken dxl_token = (Edxltoken) element[1];
			return CDXLTokens::GetDXLTokenStr(dxl_token);
		}
	}

	GPOS_ASSERT(!"Unrecognized window frame boundary");
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarWindowFrameEdge::SerializeToDXL(CXMLSerializer *xml_serializer,
										  const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	if (m_leading_edge)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowLeadingBoundary),
			GetFrameBoundaryStr(m_dxl_frame_boundary));
	}
	else
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenWindowTrailingBoundary),
			GetFrameBoundaryStr(m_dxl_frame_boundary));
	}

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowFrameEdge::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarWindowFrameEdge::AssertValid(const CDXLNode *dxlnode,
									   BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(1 >= arity);

	GPOS_ASSERT_IMP((m_dxl_frame_boundary == EdxlfbBoundedPreceding ||
					 m_dxl_frame_boundary == EdxlfbBoundedFollowing ||
					 m_dxl_frame_boundary == EdxlfbDelayedBoundedPreceding ||
					 m_dxl_frame_boundary == EdxlfbDelayedBoundedFollowing),
					1 == arity);
	GPOS_ASSERT_IMP((m_dxl_frame_boundary == EdxlfbUnboundedPreceding ||
					 m_dxl_frame_boundary == EdxlfbUnboundedFollowing ||
					 m_dxl_frame_boundary == EdxlfbCurrentRow),
					0 == arity);

	for (ULONG idx = 0; idx < arity; ++idx)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[idx];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_arg->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg,
													validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
