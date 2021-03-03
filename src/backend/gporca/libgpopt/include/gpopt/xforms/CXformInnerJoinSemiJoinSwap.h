//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformInnerJoinSemiJoinSwap.h
//
//	@doc:
//		Swap cascaded inner join and semi-join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoinSemiJoinSwap_H
#define GPOPT_CXformInnerJoinSemiJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoinSemiJoinSwap
//
//	@doc:
//		Swap cascaded inner join and semi-join
//
//---------------------------------------------------------------------------
class CXformInnerJoinSemiJoinSwap
	: public CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftSemiJoin>
{
private:
public:
	CXformInnerJoinSemiJoinSwap(const CXformInnerJoinSemiJoinSwap &) = delete;

	// ctor
	explicit CXformInnerJoinSemiJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalInnerJoin, CLogicalLeftSemiJoin>(mp)
	{
	}

	// dtor
	~CXformInnerJoinSemiJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfSemiJoinInnerJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoinSemiJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoinSemiJoinSwap";
	}

};	// class CXformInnerJoinSemiJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformInnerJoinSemiJoinSwap_H

// EOF
