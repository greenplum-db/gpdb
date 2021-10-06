//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformImplementInnerJoin.h
//
//	@doc:
//		Transform inner join to inner Hash/NLJ Join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementInnerJoin_H
#define GPOPT_CXformImplementInnerJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementInnerJoin
//
//	@doc:
//		Transform inner join to inner Hash Join or Inner Nested Loop Join
//		(if a Hash Join is not possible)
//
//---------------------------------------------------------------------------
class CXformImplementInnerJoin : public CXformImplementation
{
private:
	// private copy ctor
	CXformImplementInnerJoin(const CXformImplementInnerJoin &);

public:
	// ctor
	explicit CXformImplementInnerJoin(CMemoryPool *mp);

	// dtor
	~CXformImplementInnerJoin()
	{
	}

	// ident accessors
	EXformId
	Exfid() const
	{
		return ExfImplementInnerJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const
	{
		return "CXformImplementInnerJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const;

};	// class CXformImplementInnerJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformImplementInnerJoin_H

// EOF
