//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSemiJoinInnerJoinSwap.h
//
//	@doc:
//		Swap cascaded semi-join and inner join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSemiJoinInnerJoinSwap_H
#define GPOPT_CXformSemiJoinInnerJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftSemiJoin.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSemiJoinInnerJoinSwap
//
//	@doc:
//		Swap cascaded semi-join and inner join
//
//---------------------------------------------------------------------------
class CXformSemiJoinInnerJoinSwap
	: public CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalInnerJoin>
{
private:
public:
	CXformSemiJoinInnerJoinSwap(const CXformSemiJoinInnerJoinSwap &) = delete;

	// ctor
	explicit CXformSemiJoinInnerJoinSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftSemiJoin, CLogicalInnerJoin>(mp)
	{
	}

	// dtor
	~CXformSemiJoinInnerJoinSwap() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSemiJoinInnerJoinSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformSemiJoinInnerJoinSwap";
	}

};	// class CXformSemiJoinInnerJoinSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformSemiJoinInnerJoinSwap_H

// EOF
