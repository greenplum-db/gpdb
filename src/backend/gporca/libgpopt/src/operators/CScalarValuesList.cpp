//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
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
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	return true;
}

// EOF
