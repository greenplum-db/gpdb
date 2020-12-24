//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarCoerceToDomain.cpp
//
//	@doc:
//		Implementation of DXL scalar coerce
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarCoerceToDomain.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceToDomain::CDXLScalarCoerceToDomain
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarCoerceToDomain::CDXLScalarCoerceToDomain(
	CMemoryPool *mp, IMDId *mdid_type, INT type_modifier,
	EdxlCoercionForm dxl_coerce_format, INT location)
	: CDXLScalarCoerceBase(mp, mdid_type, type_modifier, dxl_coerce_format,
						   location)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoerceToDomain::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarCoerceToDomain::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarCoerceToDomain);
}

// EOF
