//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CXformLeftJoin2RightJoin.h
//
//	@doc:
//		Transform left outer join to right outer join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftJoin2RightJoin_H
#define GPOPT_CXformLeftJoin2RightJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftJoin2RightJoin
//
//	@doc:
//		Transform left outer join to right outer join
//
//---------------------------------------------------------------------------
class CXformLeftJoin2RightJoin : public CXformExploration
{
private:
public:
	CXformLeftJoin2RightJoin(const CXformLeftJoin2RightJoin &) = delete;

	// ctor
	explicit CXformLeftJoin2RightJoin(CMemoryPool *mp);

	// dtor
	~CXformLeftJoin2RightJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftJoin2RightJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLeftJoin2RightJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformLeftJoin2RightJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftJoin2RightJoin_H

// EOF
