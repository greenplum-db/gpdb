//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalar.cpp
//
//	@doc:
//		Implementation of DXL scalar operators
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalar.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalar::CDXLScalar
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalar::CDXLScalar(CMemoryPool *mp) : CDXLOperator(mp)
{
}

//---------------------------------------------------------------------------
//      @function:
//              CDXLScalar::GetDXLOperatorType
//
//      @doc:
//              Operator Type
//
//---------------------------------------------------------------------------
Edxloptype
CDXLScalar::GetDXLOperatorType() const
{
	return EdxloptypeScalar;
}

void
CDXLScalar::SetIsSecurityQual(BOOL is_security_qual)
{
	m_is_security_qual = is_security_qual;
}

BOOL
CDXLScalar::GetIsSecurityQual()
{
	return m_is_security_qual;
}



// EOF
