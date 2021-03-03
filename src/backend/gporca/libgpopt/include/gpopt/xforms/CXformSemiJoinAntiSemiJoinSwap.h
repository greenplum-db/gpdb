//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSemiJoinAntiSemiJoinSwap.h
//
//	@doc:
//		Swap cascaded semi-join and anti semi-join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSemiJoinAntiSemiJoinSwap_H
#define GPOPT_CXformSemiJoinAntiSemiJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSemiJoinAntiSemiJoinSwap
//
//	@doc:
//		Swap cascaded semi-join and anti semi-join
//
//---------------------------------------------------------------------------
class CXformSemiJoinAntiSemiJoinSwap
	: public CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalLeftAntiSemiJoin>
{
private:
public:
	CXformSemiJoinAntiSemiJoinSwap(const CXformSemiJoinAntiSemiJoinSwap &) =
		delete;

	// ctor
	explicit CXformSemiJoinAntiSemiJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalLeftAntiSemiJoin>(mp)
	{
	}

	// dtor
	~CXformSemiJoinAntiSemiJoinSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinSemiJoinSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSemiJoinAntiSemiJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformSemiJoinAntiSemiJoinSwap";
	}

};	// class CXformSemiJoinAntiSemiJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformSemiJoinAntiSemiJoinSwap_H

// EOF
