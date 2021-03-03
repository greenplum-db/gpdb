//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementCTEConsumer.h
//
//	@doc:
//		Transform Logical CTE Consumer to Physical CTE Consumer
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementCTEConsumer_H
#define GPOPT_CXformImplementCTEConsumer_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementCTEConsumer
//
//	@doc:
//		Transform Logical CTE Consumer to Physical CTE Consumer
//
//---------------------------------------------------------------------------
class CXformImplementCTEConsumer : public CXformImplementation
{
private:
public:
	CXformImplementCTEConsumer(const CXformImplementCTEConsumer &) = delete;

	// ctor
	explicit CXformImplementCTEConsumer(CMemoryPool *mp);

	// dtor
	~CXformImplementCTEConsumer() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementCTEConsumer;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementCTEConsumer";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementCTEConsumer
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementCTEConsumer_H

// EOF
