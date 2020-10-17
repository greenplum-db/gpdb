//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CDXLLogicalExternalGet.cpp
//
//	@doc:
//		Implementation of DXL logical external get operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalExternalGet.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalExternalGet::CDXLLogicalExternalGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalExternalGet::CDXLLogicalExternalGet(CMemoryPool *mp,
											   CDXLTableDescr *table_descr)
	: CDXLLogicalGet(mp, table_descr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalExternalGet::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalExternalGet::GetDXLOperator() const
{
	return EdxlopLogicalExternalGet;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalExternalGet::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalExternalGet::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalExternalGet);
}

// EOF
