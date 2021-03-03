//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformAntiSemiJoinAntiSemiJoinNotInSwap.h
//
//	@doc:
//		Swap cascaded anti semi-join and anti semi-join (NotIn)
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAntiSemiJoinAntiSemiJoinNotInSwap_H
#define GPOPT_CXformAntiSemiJoinAntiSemiJoinNotInSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformAntiSemiJoinAntiSemiJoinNotInSwap
//
//	@doc:
//		Swap cascaded anti semi-join and anti semi-join (NotIn)
//
//---------------------------------------------------------------------------
class CXformAntiSemiJoinAntiSemiJoinNotInSwap
	: public CXformJoinSwap<CLogicalLeftAntiSemiJoin,
							CLogicalLeftAntiSemiJoinNotIn>
{
private:
public:
	CXformAntiSemiJoinAntiSemiJoinNotInSwap(
		const CXformAntiSemiJoinAntiSemiJoinNotInSwap &) = delete;

	// ctor
	explicit CXformAntiSemiJoinAntiSemiJoinNotInSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftAntiSemiJoin,
						 CLogicalLeftAntiSemiJoinNotIn>(mp)
	{
	}

	// dtor
	~CXformAntiSemiJoinAntiSemiJoinNotInSwap() override = default;

	// compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinNotInAntiSemiJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAntiSemiJoinAntiSemiJoinNotInSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformAntiSemiJoinAntiSemiJoinNotInSwap";
	}

};	// class CXformAntiSemiJoinAntiSemiJoinNotInSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformAntiSemiJoinAntiSemiJoinNotInSwap_H

// EOF
