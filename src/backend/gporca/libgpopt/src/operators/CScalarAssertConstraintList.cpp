//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CScalarAssertConstraintList.cpp
//
//	@doc:
//		Implementation of scalar assert constraint list representing the predicate
//		of assert operators
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarAssertConstraintList.h"

#include "gpos/base.h"

#include "naucrates/md/IMDTypeBool.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraintList::CScalarAssertConstraintList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarAssertConstraintList::CScalarAssertConstraintList(CMemoryPool *mp)
	: CScalar(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraintList::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarAssertConstraintList::Matches(COperator *pop) const
{
	return pop->Eopid() == Eopid();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraintList::Matches
//
//	@doc:
//		Type of expression's result
//
//---------------------------------------------------------------------------
IMDId *
CScalarAssertConstraintList::MdidType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	return md_accessor->PtMDType<IMDTypeBool>()->MDId();
}


// EOF
