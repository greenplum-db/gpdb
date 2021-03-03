//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformInnerJoinAntiSemiJoinNotInSwap.h
//
//	@doc:
//		Swap cascaded inner join and anti semi-join with NotIn semantics
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoinAntiSemiJoinNotInSwap_H
#define GPOPT_CXformInnerJoinAntiSemiJoinNotInSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoinAntiSemiJoinNotInSwap
//
//	@doc:
//		Swap cascaded inner join and anti semi-join with NotIn semantics
//
//---------------------------------------------------------------------------
class CXformInnerJoinAntiSemiJoinNotInSwap
	: public CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftAntiSemiJoinNotIn>
{
private:
public:
	CXformInnerJoinAntiSemiJoinNotInSwap(
		const CXformInnerJoinAntiSemiJoinNotInSwap &) = delete;

	// ctor
	explicit CXformInnerJoinAntiSemiJoinNotInSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftAntiSemiJoinNotIn>(mp)
	{
	}

	// dtor
	~CXformInnerJoinAntiSemiJoinNotInSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinNotInInnerJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoinAntiSemiJoinNotInSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoinAntiSemiJoinNotInSwap";
	}

};	// class CXformInnerJoinAntiSemiJoinNotInSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformInnerJoinAntiSemiJoinNotInSwap_H

// EOF
