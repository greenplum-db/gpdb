//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarValuesList.cpp
//
//	@doc:
//		Implementation of scalar arrayref index list
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarValuesList.h"

#include "gpos/base.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarValuesList::CScalarValuesList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarValuesList::CScalarValuesList(CMemoryPool *mp) : CScalar(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarValuesList::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarValuesList::Matches(COperator *pop) const
{
	return pop->Eopid() == Eopid();
}

// EOF
