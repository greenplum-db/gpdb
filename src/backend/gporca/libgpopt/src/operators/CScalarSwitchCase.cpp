//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSwitchCase.cpp
//
//	@doc:
//		Implementation of scalar SwitchCase operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarSwitchCase.h"

#include "gpos/base.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitchCase::CScalarSwitchCase
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSwitchCase::CScalarSwitchCase(CMemoryPool *mp) : CScalar(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitchCase::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSwitchCase::Matches(COperator *pop) const
{
	return (pop->Eopid() == Eopid());
}

// EOF
