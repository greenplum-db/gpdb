//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformAntiSemiJoinInnerJoinSwap.h
//
//	@doc:
//		Swap cascaded anti semi-join and inner join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAntiSemiJoinInnerJoinSwap_H
#define GPOPT_CXformAntiSemiJoinInnerJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformAntiSemiJoinInnerJoinSwap
//
//	@doc:
//		Swap cascaded anti semi-join and inner join
//
//---------------------------------------------------------------------------
class CXformAntiSemiJoinInnerJoinSwap
	: public CXformJoinSwap<CLogicalLeftAntiSemiJoin, CLogicalInnerJoin>
{
private:
public:
	CXformAntiSemiJoinInnerJoinSwap(const CXformAntiSemiJoinInnerJoinSwap &) =
		delete;

	// ctor
	explicit CXformAntiSemiJoinInnerJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftAntiSemiJoin, CLogicalInnerJoin>(mp)
	{
	}

	// dtor
	~CXformAntiSemiJoinInnerJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfInnerJoinAntiSemiJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAntiSemiJoinInnerJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformAntiSemiJoinInnerJoinSwap";
	}

};	// class CXformAntiSemiJoinInnerJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformAntiSemiJoinInnerJoinSwap_H

// EOF
