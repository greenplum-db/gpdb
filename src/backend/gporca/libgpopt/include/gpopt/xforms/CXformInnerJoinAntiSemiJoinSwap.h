//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformInnerJoinAntiSemiJoinSwap.h
//
//	@doc:
//		Swap cascaded inner join and anti semi-join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoinAntiSemiJoinSwap_H
#define GPOPT_CXformInnerJoinAntiSemiJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoinAntiSemiJoinSwap
//
//	@doc:
//		Swap cascaded inner join and anti semi-join
//
//---------------------------------------------------------------------------
class CXformInnerJoinAntiSemiJoinSwap
	: public CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftAntiSemiJoin>
{
private:
public:
	CXformInnerJoinAntiSemiJoinSwap(const CXformInnerJoinAntiSemiJoinSwap &) =
		delete;

	// ctor
	explicit CXformInnerJoinAntiSemiJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftAntiSemiJoin>(mp)
	{
	}

	// dtor
	~CXformInnerJoinAntiSemiJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinInnerJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoinAntiSemiJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoinAntiSemiJoinSwap";
	}

};	// class CXformInnerJoinAntiSemiJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformInnerJoinAntiSemiJoinSwap_H

// EOF
