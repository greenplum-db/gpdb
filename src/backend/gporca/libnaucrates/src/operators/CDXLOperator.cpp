//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperator.cpp
//
//	@doc:
//		Implementation of base DXL operator class
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLOperator.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperator::CDXLOperator
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLOperator::CDXLOperator(CMemoryPool *mp) : m_mp(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperator::~CDXLOperator
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLOperator::~CDXLOperator()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperator::GetJoinTypeNameStr
//
//	@doc:
//		Join type name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLOperator::GetJoinTypeNameStr(EdxlJoinType join_type)
{
	GPOS_ASSERT(EdxljtSentinel > join_type);

	switch (join_type)
	{
		case EdxljtInner:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinInner);

		case EdxljtLeft:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinLeft);

		case EdxljtFull:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinFull);

		case EdxljtRight:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinRight);

		case EdxljtIn:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinIn);

		case EdxljtLeftAntiSemijoin:
			return CDXLTokens::GetDXLTokenStr(EdxltokenJoinLeftAntiSemiJoin);

		case EdxljtLeftAntiSemijoinNotIn:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenJoinLeftAntiSemiJoinNotIn);

		default:
			return CDXLTokens::GetDXLTokenStr(EdxltokenUnknown);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperator::GetIdxScanDirectionStr
//
//	@doc:
//		Return the index scan direction name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLOperator::GetIdxScanDirectionStr(EdxlIndexScanDirection idx_scan_direction)
{
	switch (idx_scan_direction)
	{
		case EdxlisdBackward:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenIndexScanDirectionBackward);

		case EdxlisdForward:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenIndexScanDirectionForward);

		case EdxlisdNoMovement:
			return CDXLTokens::GetDXLTokenStr(
				EdxltokenIndexScanDirectionNoMovement);

		default:
			GPOS_ASSERT(!"Unrecognized index scan direction");
			return CDXLTokens::GetDXLTokenStr(EdxltokenUnknown);
	}
}

// EOF
