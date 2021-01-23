//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementCTEProducer.h
//
//	@doc:
//		Transform Logical CTE Producer to Physical CTE Producer
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementCTEProducer_H
#define GPOPT_CXformImplementCTEProducer_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementCTEProducer
//
//	@doc:
//		Transform Logical CTE Producer to Physical CTE Producer
//
//---------------------------------------------------------------------------
class CXformImplementCTEProducer : public CXformImplementation
{
private:
public:
	CXformImplementCTEProducer(const CXformImplementCTEProducer &) = delete;

	// ctor
	explicit CXformImplementCTEProducer(CMemoryPool *mp);

	// dtor
	~CXformImplementCTEProducer() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementCTEProducer;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementCTEProducer";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementCTEProducer
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementCTEProducer_H

// EOF
