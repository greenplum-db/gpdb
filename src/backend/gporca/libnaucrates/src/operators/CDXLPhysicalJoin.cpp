//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalJoin.cpp
//
//	@doc:
//		Implementation of the base class for DXL physical join operators
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalJoin.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalJoin::CDXLPhysicalJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalJoin::CDXLPhysicalJoin(CMemoryPool *mp, EdxlJoinType join_type)
	: CDXLPhysical(mp), m_join_type(join_type)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalJoin::GetJoinType
//
//	@doc:
//		Join type
//
//---------------------------------------------------------------------------
EdxlJoinType
CDXLPhysicalJoin::GetJoinType() const
{
	return m_join_type;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalJoin::GetJoinTypeNameStr
//
//	@doc:
//		Join type name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalJoin::GetJoinTypeNameStr() const
{
	return CDXLOperator::GetJoinTypeNameStr(m_join_type);
}

// EOF
