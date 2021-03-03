//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformExpandNAryJoinMinCard.h
//
//	@doc:
//		Expand n-ary join into series of binary joins while minimizing
//		cardinality of intermediate results
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandNAryJoinMinCard_H
#define GPOPT_CXformExpandNAryJoinMinCard_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandNAryJoinMinCard
//
//	@doc:
//		Expand n-ary join into series of binary joins while minimizing
//		cardinality of intermediate results
//
//---------------------------------------------------------------------------
class CXformExpandNAryJoinMinCard : public CXformExploration
{
private:
public:
	CXformExpandNAryJoinMinCard(const CXformExpandNAryJoinMinCard &) = delete;

	// ctor
	explicit CXformExpandNAryJoinMinCard(CMemoryPool *mp);

	// dtor
	~CXformExpandNAryJoinMinCard() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandNAryJoinMinCard;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandNAryJoinMinCard";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// do stats need to be computed before applying xform?
	BOOL
	FNeedsStats() const override
	{
		return true;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

	BOOL
	IsApplyOnce() override
	{
		return true;
	}
};	// class CXformExpandNAryJoinMinCard

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandNAryJoinMinCard_H

// EOF
