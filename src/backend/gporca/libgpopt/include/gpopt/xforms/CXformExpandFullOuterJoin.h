//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformExpandFullOuterJoin.h
//
//	@doc:
//		Transform logical FOJ to a UNION ALL between LOJ and LASJ
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandFullOuterJoin_H
#define GPOPT_CXformExpandFullOuterJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandFullOuterJoin
//
//	@doc:
//		Transform logical FOJ with a rename on top to a UNION ALL between LOJ
//		and LASJ
//
//---------------------------------------------------------------------------
class CXformExpandFullOuterJoin : public CXformExploration
{
private:
	// construct a join expression of two CTEs using the given CTE ids
	// and output columns
	static CExpression *PexprLogicalJoinOverCTEs(
		CMemoryPool *mp, EdxlJoinType edxljointype, ULONG ulLeftCTEId,
		CColRefArray *pdrgpcrLeft, ULONG ulRightCTEId,
		CColRefArray *pdrgpcrRight, CExpression *pexprScalar);

public:
	CXformExpandFullOuterJoin(const CXformExpandFullOuterJoin &) = delete;

	// ctor
	explicit CXformExpandFullOuterJoin(CMemoryPool *mp);

	// dtor
	~CXformExpandFullOuterJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandFullOuterJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandFullOuterJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformExpandFullOuterJoin
}  // namespace gpopt

#endif	// !GPOPT_CXformExpandFullOuterJoin_H

// EOF
