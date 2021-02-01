//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformPushGbWithHavingBelowJoin.h
//
//	@doc:
//		Push group by with having clause below join transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushGbWithHavingBelowJoin_H
#define GPOPT_CXformPushGbWithHavingBelowJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushGbWithHavingBelowJoin
//
//	@doc:
//		Push group by with having clause below join transform
//
//---------------------------------------------------------------------------
class CXformPushGbWithHavingBelowJoin : public CXformExploration
{
private:
public:
	CXformPushGbWithHavingBelowJoin(const CXformPushGbWithHavingBelowJoin &) =
		delete;

	// ctor
	explicit CXformPushGbWithHavingBelowJoin(CMemoryPool *mp);

	// dtor
	~CXformPushGbWithHavingBelowJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfPushGbWithHavingBelowJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformPushGbWithHavingBelowJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformPushGbWithHavingBelowJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformPushGbWithHavingBelowJoin_H

// EOF
