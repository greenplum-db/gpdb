//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformImplementLimit.h
//
//	@doc:
//		Transform Logical into Physical Limit
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLimit_H
#define GPOPT_CXformImplementLimit_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementLimit
//
//	@doc:
//		Transform Logical into Physical Limit
//
//---------------------------------------------------------------------------
class CXformImplementLimit : public CXformImplementation
{
private:
public:
	CXformImplementLimit(const CXformImplementLimit &) = delete;

	// ctor
	explicit CXformImplementLimit(CMemoryPool *mp);

	// dtor
	~CXformImplementLimit() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLimit;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLimit";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformImplementLimit

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLimit_H

// EOF
