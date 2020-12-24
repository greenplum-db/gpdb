//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarRecheckCondFilter.h
//
//	@doc:
//		Filter for rechecking an index condition on the operator upstream of the index scan
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarRecheckCondFilter.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarRecheckCondFilter::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarRecheckCondFilter::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarRecheckCondFilter);
}

// EOF
