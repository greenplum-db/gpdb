//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformDifferenceAll2LeftAntiSemiJoin.h
//
//	@doc:
//		Class to transform logical difference all into a LASJ
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformDifferenceAll2LeftAntiSemiJoin_H
#define GPOPT_CXformDifferenceAll2LeftAntiSemiJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformDifferenceAll2LeftAntiSemiJoin
//
//	@doc:
//		Class to transform logical difference all into a LASJ
//
//---------------------------------------------------------------------------
class CXformDifferenceAll2LeftAntiSemiJoin : public CXformExploration
{
private:
public:
	CXformDifferenceAll2LeftAntiSemiJoin(
		const CXformDifferenceAll2LeftAntiSemiJoin &) = delete;

	// ctor
	explicit CXformDifferenceAll2LeftAntiSemiJoin(CMemoryPool *mp);

	// dtor
	~CXformDifferenceAll2LeftAntiSemiJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfDifferenceAll2LeftAntiSemiJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformDifferenceAll2LeftAntiSemiJoin";
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

};	// class CXformDifferenceAll2LeftAntiSemiJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformDifferenceAll2LeftAntiSemiJoin_H

// EOF
