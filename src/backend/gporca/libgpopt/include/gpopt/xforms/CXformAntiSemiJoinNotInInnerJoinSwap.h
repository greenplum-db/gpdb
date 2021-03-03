//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformAntiSemiJoinNotInInnerJoinSwap.h
//
//	@doc:
//		Swap cascaded anti semi-join (NotIn) and inner join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAntiSemiJoinNotInInnerJoinSwap_H
#define GPOPT_CXformAntiSemiJoinNotInInnerJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformAntiSemiJoinNotInInnerJoinSwap
//
//	@doc:
//		Swap cascaded anti semi-join (NotIn) and inner join
//
//---------------------------------------------------------------------------
class CXformAntiSemiJoinNotInInnerJoinSwap
	: public CXformJoinSwap<CLogicalLeftAntiSemiJoinNotIn, CLogicalInnerJoin>
{
private:
public:
	CXformAntiSemiJoinNotInInnerJoinSwap(
		const CXformAntiSemiJoinNotInInnerJoinSwap &) = delete;

	// ctor
	explicit CXformAntiSemiJoinNotInInnerJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftAntiSemiJoinNotIn, CLogicalInnerJoin>(mp)
	{
	}

	// dtor
	~CXformAntiSemiJoinNotInInnerJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfInnerJoinAntiSemiJoinNotInSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAntiSemiJoinNotInInnerJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformAntiSemiJoinNotInInnerJoinSwap";
	}

};	// class CXformAntiSemiJoinNotInInnerJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformAntiSemiJoinNotInInnerJoinSwap_H

// EOF
