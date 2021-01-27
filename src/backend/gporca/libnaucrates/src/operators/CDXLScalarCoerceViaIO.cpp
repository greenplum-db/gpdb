//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLScalarCoerceViaIO.cpp
//
//	@doc:
//		Implementation of DXL scalar coerce
//
//	@owner:
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarCoerceViaIO.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceViaIO::CDXLScalarCoerceViaIO
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarCoerceViaIO::CDXLScalarCoerceViaIO(CMemoryPool *mp, IMDId *mdid_type,
											 INT type_modifier,
											 EdxlCoercionForm dxl_coerce_format,
											 INT location)
	: CDXLScalarCoerceBase(mp, mdid_type, type_modifier, dxl_coerce_format,
						   location)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceViaIO::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarCoerceViaIO::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarCoerceViaIO);
}

// EOF
