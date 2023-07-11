//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CDXLPhysicalDynamicIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of DXL physical dynamic index only scan operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalDynamicIndexOnlyScan.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDCacheObject.h"

using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexOnlyScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalDynamicIndexOnlyScan::GetDXLOperator() const
{
	return EdxlopPhysicalDynamicIndexOnlyScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalDynamicIndexOnlyScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalDynamicIndexOnlyScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalDynamicIndexOnlyScan);
}

// EOF
