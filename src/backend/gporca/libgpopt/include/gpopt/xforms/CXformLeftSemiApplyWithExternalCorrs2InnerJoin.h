//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftSemiApplyWithExternalCorrs2InnerJoin.h
//
//	@doc:
//		Turn LS apply with external correlations apply into inner join;
//		external correlations are correlations in the inner child of LSA
//		that use columns not defined by the outer child of LSA
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftSemiApplyWithExternalCorrs2InnerJoin_H
#define GPOPT_CXformLeftSemiApplyWithExternalCorrs2InnerJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftSemiApply.h"
#include "gpopt/xforms/CXformApply2Join.h"
#include "gpopt/xforms/CXformUtils.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftSemiApplyWithExternalCorrs2InnerJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftSemiApplyWithExternalCorrs2InnerJoin
	: public CXformApply2Join<CLogicalLeftSemiApply, CLogicalInnerJoin>
{
private:
	// helper for splitting correlations into external and residual
	static BOOL FSplitCorrelations(CMemoryPool *mp, CExpression *pexprOuter,
								   CExpression *pexprInner,
								   CExpressionArray *pdrgpexprAllCorr,
								   CExpressionArray **ppdrgpexprExternal,
								   CExpressionArray **ppdrgpexprResidual,
								   CColRefSet **ppcrsInnerUsed);

	// helper for collecting correlations
	static BOOL FDecorrelate(CMemoryPool *mp, CExpression *pexpr,
							 CExpression **ppexprInnerNew,
							 CExpressionArray **ppdrgpexprCorr);

	// decorrelate semi apply with external correlations
	static CExpression *PexprDecorrelate(CMemoryPool *mp, CExpression *pexpr);

public:
	CXformLeftSemiApplyWithExternalCorrs2InnerJoin(
		const CXformLeftSemiApplyWithExternalCorrs2InnerJoin &) = delete;

	// ctor
	explicit CXformLeftSemiApplyWithExternalCorrs2InnerJoin(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalInnerJoin>(
			  mp, true /*fDeepTree*/)
	{
	}

	// ctor with a passed pattern
	CXformLeftSemiApplyWithExternalCorrs2InnerJoin(CMemoryPool *mp,
												   CExpression *pexprPattern)
		: CXformApply2Join<CLogicalLeftSemiApply, CLogicalInnerJoin>(
			  mp, pexprPattern)
	{
	}

	// dtor
	~CXformLeftSemiApplyWithExternalCorrs2InnerJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftSemiApplyWithExternalCorrs2InnerJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftSemiApplyWithExternalCorrs2InnerJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftSemiApplyWithExternalCorrs2InnerJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftSemiApplyWithExternalCorrs2InnerJoin_H

// EOF
