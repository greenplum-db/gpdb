//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowFrame.cpp
//
//	@doc:
//		Implementation of DXL Window Frame
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLWindowFrame.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDAggregate.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowFrame::CDXLWindowFrame
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLWindowFrame::CDXLWindowFrame(EdxlFrameSpec edxlfs,
								 EdxlFrameExclusionStrategy frame_exc_strategy,
								 CDXLNode *dxlnode_leading,
								 CDXLNode *dxlnode_trailing,
								 OID start_in_range_func, OID end_in_range_func,
								 OID in_range_coll, bool in_range_asc,
								 bool in_range_nulls_first)
	: m_dxl_win_frame_spec(edxlfs),
	  m_dxl_frame_exclusion_strategy(frame_exc_strategy),
	  m_dxlnode_leading(dxlnode_leading),
	  m_dxlnode_trailing(dxlnode_trailing),
	  m_start_in_range_func(start_in_range_func),
	  m_end_in_range_func(end_in_range_func),
	  m_in_range_coll(in_range_coll),
	  m_in_range_asc(in_range_asc),
	  m_in_range_nulls_first(in_range_nulls_first)
{
	GPOS_ASSERT(EdxlfsSentinel > m_dxl_win_frame_spec);
	GPOS_ASSERT(EdxlfesSentinel > m_dxl_frame_exclusion_strategy);
	GPOS_ASSERT(nullptr != dxlnode_leading);
	GPOS_ASSERT(nullptr != dxlnode_trailing);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowFrame::~CDXLWindowFrame
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLWindowFrame::~CDXLWindowFrame()
{
	m_dxlnode_leading->Release();
	m_dxlnode_trailing->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowFrame::PstrES
//
//	@doc:
//		Return the string representation of the window frame exclusion strategy
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLWindowFrame::PstrES(EdxlFrameExclusionStrategy edxles)
{
	GPOS_ASSERT(EdxlfesSentinel > edxles);
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] = {
		{EdxlfesNone, EdxltokenWindowESNone},
		{EdxlfesNulls, EdxltokenWindowESNulls},
		{EdxlfesCurrentRow, EdxltokenWindowESCurrentRow},
		{EdxlfesGroup, EdxltokenWindowESGroup},
		{EdxlfesTies, EdxltokenWindowESTies}};

	const ULONG arity =
		GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *pulElem = window_frame_boundary_to_frame_boundary_mapping[ul];
		if ((ULONG) edxles == pulElem[0])
		{
			Edxltoken edxltk = (Edxltoken) pulElem[1];
			return CDXLTokens::GetDXLTokenStr(edxltk);
			break;
		}
	}

	GPOS_ASSERT(!"Unrecognized window frame exclusion strategy");
	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowFrame::PstrFS
//
//	@doc:
//		Return the string representation of the window frame specification
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLWindowFrame::PstrFS(EdxlFrameSpec edxlfs)
{
	GPOS_ASSERT(EdxlfsSentinel > edxlfs &&
				"Unrecognized window frame specification");

	if (EdxlfsRow == edxlfs)
	{
		return CDXLTokens::GetDXLTokenStr(EdxltokenWindowFSRow);
	}
	else if (EdxlfsGroups == edxlfs)
	{
		return CDXLTokens::GetDXLTokenStr(EdxltokenWindowFSGroups);
	}

	return CDXLTokens::GetDXLTokenStr(EdxltokenWindowFSRange);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLWindowFrame::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLWindowFrame::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	const CWStringConst *element_name =
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrame);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// add attributes
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowFrameSpec),
		PstrFS(m_dxl_win_frame_spec));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowExclusionStrategy),
		PstrES(m_dxl_frame_exclusion_strategy));

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowStartInRangeOid),
		m_start_in_range_func);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowEndInRangeOid),
		m_end_in_range_func);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowInRangeColl),
		m_in_range_coll);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowInRangeAsc), m_in_range_asc);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowInRangeNullsFirst),
		m_in_range_nulls_first);

	// add the values representing the window boundary
	m_dxlnode_trailing->SerializeToDXL(xml_serializer);
	m_dxlnode_leading->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

// EOF
