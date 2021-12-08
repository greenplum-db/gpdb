//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformInnerJoin2HashJoin.h
//
//	@doc:
//		Transform inner join to inner Hash Join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoin2HashJoin_H
#define GPOPT_CXformInnerJoin2HashJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoin2HashJoin
//
//	@doc:
//		Transform inner join to inner Hash Join
//		Deprecated in favor of CXformImplementInnerJoin.
//
//---------------------------------------------------------------------------
class CXformInnerJoin2HashJoin : public CXformImplementation
{
private:
public:
	CXformInnerJoin2HashJoin(const CXformInnerJoin2HashJoin &) = delete;

	// ctor
	explicit CXformInnerJoin2HashJoin(CMemoryPool *mp);

	// dtor
	~CXformInnerJoin2HashJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoin2HashJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoin2HashJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformInnerJoin2HashJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformInnerJoin2HashJoin_H

// EOF
