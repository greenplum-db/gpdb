//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformPushGbBelowJoin.h
//
//	@doc:
//		Push group by below join transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushGbBelowJoin_H
#define GPOPT_CXformPushGbBelowJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushGbBelowJoin
//
//	@doc:
//		Push group by below join transform
//
//---------------------------------------------------------------------------
class CXformPushGbBelowJoin : public CXformExploration
{
private:
public:
	CXformPushGbBelowJoin(const CXformPushGbBelowJoin &) = delete;

	// ctor
	explicit CXformPushGbBelowJoin(CMemoryPool *mp);

	// ctor
	explicit CXformPushGbBelowJoin(CExpression *pexprPattern);

	// dtor
	~CXformPushGbBelowJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfPushGbBelowJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformPushGbBelowJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformPushGbBelowJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformPushGbBelowJoin_H

// EOF
