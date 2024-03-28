//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware by Broadcom
//
//	@filename:
//		CXformFullOuterJoin2HashJoin.h
//
//	@doc:
//		Transform logical full join to full hash join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformFullOuterJoin2HashJoin_H
#define GPOPT_CXformFullOuterJoin2HashJoin_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformFullOuterJoin2HashJoin
//
//	@doc:
//		Transform logical full join to full hash join
//
//---------------------------------------------------------------------------
class CXformFullOuterJoin2HashJoin : public CXformImplementation
{
private:
public:
	CXformFullOuterJoin2HashJoin(const CXformFullOuterJoin2HashJoin &) = delete;

	// ctor
	explicit CXformFullOuterJoin2HashJoin(CMemoryPool *mp);

	// dtor
	~CXformFullOuterJoin2HashJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfFullOuterJoin2HashJoin;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformFullOuterJoin2HashJoin";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;


	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformFullOuterJoin2HashJoin

}  // namespace gpopt


#endif	// !GPOPT_CXformFullOuterJoin2HashJoin_H

// EOF
