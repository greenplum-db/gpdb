//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformPushDownLeftOuterJoin.h
//
//	@doc:
//		Push LOJ below NAry join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushDownLeftOuterJoin_H
#define GPOPT_CXformPushDownLeftOuterJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushDownLeftOuterJoin
//
//	@doc:
//		Transform LOJ whose outer child is an NAry-join to be a child
//		of NAry-join
//
//---------------------------------------------------------------------------
class CXformPushDownLeftOuterJoin : public CXformExploration
{
private:
public:
	CXformPushDownLeftOuterJoin(const CXformPushDownLeftOuterJoin &) = delete;

	// ctor
	explicit CXformPushDownLeftOuterJoin(CMemoryPool *mp);

	// dtor
	~CXformPushDownLeftOuterJoin() override = default;

	// xform promise
	CXform::EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfPushDownLeftOuterJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformPushDownLeftOuterJoin";
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformPushDownLeftOuterJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformPushDownLeftOuterJoin_H

// EOF
