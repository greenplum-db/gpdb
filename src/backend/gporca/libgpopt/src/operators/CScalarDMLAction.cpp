//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarDMLAction.cpp
//
//	@doc:
//		Implementation of scalar DML action operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarDMLAction.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "naucrates/md/IMDTypeInt4.h"

using namespace gpopt;
using namespace gpmd;


//---------------------------------------------------------------------------
//	@function:
//		CScalarDMLAction::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarDMLAction::Matches(COperator *pop) const
{
	return pop->Eopid() == Eopid();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarDMLAction::MdidType
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
IMDId *
CScalarDMLAction::MdidType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	return md_accessor->PtMDType<IMDTypeInt4>()->MDId();
}
// EOF
