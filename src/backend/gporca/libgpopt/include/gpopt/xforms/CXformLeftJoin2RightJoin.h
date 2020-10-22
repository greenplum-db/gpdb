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
	// private copy ctor
	CXformLeftJoin2RightJoin(const CXformLeftJoin2RightJoin &);

public:
	// ctor
	explicit CXformLeftJoin2RightJoin(CMemoryPool *mp);

	// dtor
	virtual ~CXformLeftJoin2RightJoin()
	{
	}

	// ident accessors
	virtual EXformId
	Exfid() const
	{
		return ExfLeftJoin2RightJoin;
	}

	// return a string for xform name
	virtual const CHAR *
	SzId() const
	{
		return "CXformLeftJoin2RightJoin";
	}

	// compute xform promise for a given expression handle
	virtual EXformPromise Exfp(CExpressionHandle &exprhdl) const;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const;

};	// class CXformLeftJoin2RightJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformLeftJoin2RightJoin_H

// EOF
