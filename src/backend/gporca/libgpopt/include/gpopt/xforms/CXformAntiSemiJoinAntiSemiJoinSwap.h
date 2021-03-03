//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformAntiSemiJoinAntiSemiJoinSwap.h
//
//	@doc:
//		Swap two cascaded anti semi-joins
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAntiSemiJoinAntiSemiJoinSwap_H
#define GPOPT_CXformAntiSemiJoinAntiSemiJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformAntiSemiJoinAntiSemiJoinSwap
//
//	@doc:
//		Swap two cascaded anti semi-joins
//
//---------------------------------------------------------------------------
class CXformAntiSemiJoinAntiSemiJoinSwap
	: public CXformJoinSwap<CLogicalLeftAntiSemiJoin, CLogicalLeftAntiSemiJoin>
{
private:
public:
	CXformAntiSemiJoinAntiSemiJoinSwap(
		const CXformAntiSemiJoinAntiSemiJoinSwap &) = delete;

	// ctor
	explicit CXformAntiSemiJoinAntiSemiJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftAntiSemiJoin, CLogicalLeftAntiSemiJoin>(mp)
	{
	}

	// dtor
	~CXformAntiSemiJoinAntiSemiJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinAntiSemiJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAntiSemiJoinAntiSemiJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformAntiSemiJoinAntiSemiJoinSwap";
	}

};	// class CXformAntiSemiJoinAntiSemiJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformSemiJoinSemiJoinSwap_H

// EOF
