//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiNLJoinNotIn.h
//
//	@doc:
//		Left anti semi nested-loops join operator with NotIn semantics
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftAntiSemiNLJoinNotIn_H
#define GPOPT_CPhysicalLeftAntiSemiNLJoinNotIn_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalLeftAntiSemiNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftAntiSemiNLJoinNotIn
//
//	@doc:
//		Left anti semi nested-loops join operator with NotIn semantics
//
//---------------------------------------------------------------------------
class CPhysicalLeftAntiSemiNLJoinNotIn : public CPhysicalLeftAntiSemiNLJoin
{
private:
public:
	CPhysicalLeftAntiSemiNLJoinNotIn(const CPhysicalLeftAntiSemiNLJoinNotIn &) =
		delete;

	// ctor
	explicit CPhysicalLeftAntiSemiNLJoinNotIn(CMemoryPool *mp)
		: CPhysicalLeftAntiSemiNLJoin(mp)
	{
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftAntiSemiNLJoinNotIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftAntiSemiNLJoinNotIn";
	}

	// conversion function
	static CPhysicalLeftAntiSemiNLJoinNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftAntiSemiNLJoinNotIn == pop->Eopid());

		return dynamic_cast<CPhysicalLeftAntiSemiNLJoinNotIn *>(pop);
	}

};	// class CPhysicalLeftAntiSemiNLJoinNotIn

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftAntiSemiNLJoinNotIn_H

// EOF
