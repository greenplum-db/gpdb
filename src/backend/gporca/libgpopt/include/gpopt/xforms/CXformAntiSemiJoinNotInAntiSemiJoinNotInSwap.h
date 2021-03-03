//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap.h
//
//	@doc:
//		Swap two cascaded anti semi-joins with NotIn semantics
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap_H
#define GPOPT_CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"
#include "gpopt/xforms/CXformJoinSwap.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap
//
//	@doc:
//		Swap two cascaded anti semi-joins with NotIn semantics
//
//---------------------------------------------------------------------------
class CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap
	: public CXformJoinSwap<CLogicalLeftAntiSemiJoinNotIn,
							CLogicalLeftAntiSemiJoinNotIn>
{
private:
public:
	CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap(
		const CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap &) = delete;

	// ctor
	explicit CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap(CMemoryPool *mp)
		: CXformJoinSwap<CLogicalLeftAntiSemiJoinNotIn,
						 CLogicalLeftAntiSemiJoinNotIn>(mp)
	{
	}

	// dtor
	~CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap() override = default;

	// Compatibility function
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return ExfAntiSemiJoinNotInAntiSemiJoinNotInSwap != exfid;
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfAntiSemiJoinNotInAntiSemiJoinNotInSwap;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap";
	}

};	// class CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap

}  // namespace gpopt

#endif	// !GPOPT_CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap_H

// EOF
