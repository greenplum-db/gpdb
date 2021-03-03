//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformInlineCTEConsumer.h
//
//	@doc:
//		Transform logical CTE consumer to a copy of the expression under its
//		corresponding producer
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInlineCTEConsumer_H
#define GPOPT_CXformInlineCTEConsumer_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInlineCTEConsumer
//
//	@doc:
//		Transform logical CTE consumer to a copy of the expression under its
//		corresponding producer
//
//---------------------------------------------------------------------------
class CXformInlineCTEConsumer : public CXformExploration
{
private:
public:
	CXformInlineCTEConsumer(const CXformInlineCTEConsumer &) = delete;

	// ctor
	explicit CXformInlineCTEConsumer(CMemoryPool *mp);

	// dtor
	~CXformInlineCTEConsumer() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInlineCTEConsumer;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInlineCTEConsumer";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformInlineCTEConsumer
}  // namespace gpopt

#endif	// !GPOPT_CXformInlineCTEConsumer_H

// EOF
