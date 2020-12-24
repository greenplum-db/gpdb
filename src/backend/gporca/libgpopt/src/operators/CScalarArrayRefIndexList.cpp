//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarArrayRefIndexList.cpp
//
//	@doc:
//		Implementation of scalar arrayref index list
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CScalarArrayRefIndexList.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRefIndexList::CScalarArrayRefIndexList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarArrayRefIndexList::CScalarArrayRefIndexList(CMemoryPool *mp,
												   EIndexListType eilt)
	: CScalar(mp), m_eilt(eilt)
{
	GPOS_ASSERT(EiltSentinel > eilt);
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRefIndexList::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarArrayRefIndexList::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CScalarArrayRefIndexList *popIndexList =
		CScalarArrayRefIndexList::PopConvert(pop);

	return m_eilt == popIndexList->Eilt();
}

// EOF
