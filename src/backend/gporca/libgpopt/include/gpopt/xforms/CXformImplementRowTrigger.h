//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformImplementRowTrigger.h
//
//	@doc:
//		Transform Logical Row Trigger to Physical Row Trigger
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementRowTrigger_H
#define GPOPT_CXformImplementRowTrigger_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementRowTrigger
//
//	@doc:
//		Transform Logical RowTrigger to Physical RowTrigger
//
//---------------------------------------------------------------------------
class CXformImplementRowTrigger : public CXformImplementation
{
private:
public:
	CXformImplementRowTrigger(const CXformImplementRowTrigger &) = delete;

	// ctor
	explicit CXformImplementRowTrigger(CMemoryPool *mp);

	// dtor
	~CXformImplementRowTrigger() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementRowTrigger;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementRowTrigger";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementRowTrigger
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementRowTrigger_H

// EOF
