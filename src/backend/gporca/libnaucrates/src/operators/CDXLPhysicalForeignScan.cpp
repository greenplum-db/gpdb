//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalForeignScan.cpp
//
//	@doc:
//		Implementation of DXL physical foreign scan operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalForeignScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalForeignScan::CDXLPhysicalForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalForeignScan::CDXLPhysicalForeignScan(CMemoryPool *mp)
	: CDXLPhysicalTableScan(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalForeignScan::CDXLPhysicalForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalForeignScan::CDXLPhysicalForeignScan(CMemoryPool *mp,
												 CDXLTableDescr *table_descr)
	: CDXLPhysicalTableScan(mp, table_descr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalForeignScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalForeignScan::GetDXLOperator() const
{
	return EdxlopPhysicalForeignScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalForeignScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalForeignScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalForeignScan);
}

// EOF
