//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLLogicalForeignGet.cpp
//
//	@doc:
//		Implementation of DXL logical foreign get operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalForeignGet.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalForeignGet::CDXLLogicalForeignGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalForeignGet::CDXLLogicalForeignGet(CMemoryPool *mp,
											 CDXLTableDescr *table_descr)
	: CDXLLogicalGet(mp, table_descr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalForeignGet::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalForeignGet::GetDXLOperator() const
{
	return EdxlopLogicalForeignGet;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalForeignGet::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalForeignGet::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalForeignGet);
}

// EOF
