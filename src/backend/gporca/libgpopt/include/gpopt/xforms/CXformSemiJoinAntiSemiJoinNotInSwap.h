//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformSemiJoinAntiSemiJoinNotInSwap.h
//
//	@doc:
//		Swap cascaded semi-join and anti semi-join with NotIn semantics
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSemiJoinAntiSemiJoinNotInSwap_H
#define GPOPT_CXformSemiJoinAntiSemiJoinNotInSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSemiJoinAntiSemiJoinNotInSwap
//
//	@doc:
//		Swap cascaded semi-join and anti semi-join with NotIn semantics
//
//---------------------------------------------------------------------------
class CXformSemiJoinAntiSemiJoinNotInSwap
	: public CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalLeftAntiSemiJoinNotIn>
{
private:
public:
	CXformSemiJoinAntiSemiJoinNotInSwap(
		const CXformSemiJoinAntiSemiJoinNotInSwap &) = delete;

	// ctor
	explicit CXformSemiJoinAntiSemiJoinNotInSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalLeftAntiSemiJoinNotIn>(
			  mp)
	{
	}

	// dtor
	~CXformSemiJoinAntiSemiJoinNotInSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinNotInSemiJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSemiJoinAntiSemiJoinNotInSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformSemiJoinAntiSemiJoinNotInSwap";
	}

};	// class CXformSemiJoinAntiSemiJoinNotInSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformSemiJoinAntiSemiJoinNotInSwap_H

// EOF
