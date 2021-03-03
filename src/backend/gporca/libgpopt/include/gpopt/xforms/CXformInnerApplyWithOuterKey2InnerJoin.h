//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformInnerApplyWithOuterKey2InnerJoin.h
//
//	@doc:
//		Turn inner apply into inner join under the condition that
//		outer child of apply has key
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerApplyWithOuterKey2InnerJoin_H
#define GPOPT_CXformInnerApplyWithOuterKey2InnerJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CDecorrelator.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerApplyWithOuterKey2InnerJoin
//
//	@doc:
//		Transform inner apply into inner join under the condition that
//		outer child of apply has key
//
//---------------------------------------------------------------------------
class CXformInnerApplyWithOuterKey2InnerJoin : public CXformExploration
{
private:
public:
	CXformInnerApplyWithOuterKey2InnerJoin(
		const CXformInnerApplyWithOuterKey2InnerJoin &) = delete;

	// ctor
	explicit CXformInnerApplyWithOuterKey2InnerJoin(CMemoryPool *mp);

	// dtor
	~CXformInnerApplyWithOuterKey2InnerJoin() override = default;

	// transformation promise
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerApplyWithOuterKey2InnerJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformInnerApplyWithOuterKey2InnerJoin";
	}

	// is transformation an Apply decorrelation (Apply To Join) xform?
	BOOL
	FApplyDecorrelating() const override
	{
		return true;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformInnerApplyWithOuterKey2InnerJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformInnerApplyWithOuterKey2InnerJoin_H

// EOF
