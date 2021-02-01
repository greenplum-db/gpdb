//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformExpandNAryJoin.h
//
//	@doc:
//		Expand n-ary join into series of binary joins
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandNAryJoin_H
#define GPOPT_CXformExpandNAryJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandNAryJoin
//
//	@doc:
//		Expand n-ary join into series of binary joins
//
//---------------------------------------------------------------------------
class CXformExpandNAryJoin : public CXformExploration
{
private:
public:
	CXformExpandNAryJoin(const CXformExpandNAryJoin &) = delete;

	// ctor
	explicit CXformExpandNAryJoin(CMemoryPool *mp);

	// dtor
	~CXformExpandNAryJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandNAryJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandNAryJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformExpandNAryJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandNAryJoin_H

// EOF
