//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformLeftOuterApply2LeftOuterJoin.h
//
//	@doc:
//		Turn left outer apply into left outer join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftOuterApply2LeftOuterJoin_H
#define GPOPT_CXformLeftOuterApply2LeftOuterJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftOuterApply.h"
#include "gpopt/operators/CLogicalLeftOuterJoin.h"
#include "gpopt/xforms/CXformApply2Join.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLeftOuterApply2LeftOuterJoin
//
//	@doc:
//		Transform Apply into Join by decorrelating the inner side
//
//---------------------------------------------------------------------------
class CXformLeftOuterApply2LeftOuterJoin
	: public CXformApply2Join<CLogicalLeftOuterApply, CLogicalLeftOuterJoin>
{
private:
public:
	CXformLeftOuterApply2LeftOuterJoin(
		const CXformLeftOuterApply2LeftOuterJoin &) = delete;

	// ctor
	explicit CXformLeftOuterApply2LeftOuterJoin(CMemoryPool *mp)
		: CXformApply2Join<CLogicalLeftOuterApply, CLogicalLeftOuterJoin>(
			  mp, true /*fDeepTree*/)
	{
	}

	// dtor
	~CXformLeftOuterApply2LeftOuterJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLeftOuterApply2LeftOuterJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformLeftOuterApply2LeftOuterJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformLeftOuterApply2LeftOuterJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformLeftOuterApply2LeftOuterJoin_H

// EOF
