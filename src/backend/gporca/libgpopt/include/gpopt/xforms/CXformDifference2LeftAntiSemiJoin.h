//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDifference2LeftAntiSemiJoin.h
//
//	@doc:
//		Class to transform logical difference into an aggregate over left anti-semi join
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDifference2DifferenceAll_H
#define GPOPT_CXformDifference2DifferenceAll_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDifference2LeftAntiSemiJoin
//
//	@doc:
//		Class to transform logical difference into an aggregate over
//		left anti-semi join
//
//---------------------------------------------------------------------------
class CXformDifference2LeftAntiSemiJoin : public CXformExploration
{
private:
public:
	CXformDifference2LeftAntiSemiJoin(
		const CXformDifference2LeftAntiSemiJoin &) = delete;

	// ctor
	explicit CXformDifference2LeftAntiSemiJoin(CMemoryPool *mp);

	// dtor
	~CXformDifference2LeftAntiSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDifference2LeftAntiSemiJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformDifference2LeftAntiSemiJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformDifference2LeftAntiSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformDifference2DifferenceAll_H

// EOF
