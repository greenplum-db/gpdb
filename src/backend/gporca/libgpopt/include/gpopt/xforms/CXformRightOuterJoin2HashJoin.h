//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CXformRightOuterJoin2HashJoin.h
//
//	@doc:
//		Transform left outer join to left outer hash join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformRightOuterJoin2HashJoin_H
#define GPOPT_CXformRightOuterJoin2HashJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformRightOuterJoin2HashJoin
//
//	@doc:
//		Transform left outer join to left outer hash join
//
//---------------------------------------------------------------------------
class CXformRightOuterJoin2HashJoin : public CXformImplementation
{
private:
	// private copy ctor
	CXformRightOuterJoin2HashJoin(const CXformRightOuterJoin2HashJoin &);


public:
	// ctor
	explicit CXformRightOuterJoin2HashJoin(CMemoryPool *mp);

	// dtor
	virtual ~CXformRightOuterJoin2HashJoin()
	{
	}

	// ident accessors
	virtual EXformId
	Exfid() const
	{
		return ExfRightOuterJoin2HashJoin;
	}

	// return a string for xform name
	virtual const CHAR *
	SzId() const
	{
		return "CXformRightOuterJoin2HashJoin";
	}

	// compute xform promise for a given expression handle
	virtual EXformPromise Exfp(CExpressionHandle &exprhdl) const;


	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const;

};	// class CXformRightOuterJoin2HashJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformRightOuterJoin2HashJoin_H

// EOF
