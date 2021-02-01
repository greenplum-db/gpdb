//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementAssert.h
//
//	@doc:
//		Implement Assert
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementAssert_H
#define GPOPT_CXformImplementAssert_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementAssert
//
//	@doc:
//		Implement Assert
//
//---------------------------------------------------------------------------
class CXformImplementAssert : public CXformImplementation
{
private:
public:
	CXformImplementAssert(const CXformImplementAssert &) = delete;

	// ctor
	explicit CXformImplementAssert(CMemoryPool *mp);

	// dtor
	~CXformImplementAssert() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementAssert;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementAssert";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformImplementAssert

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementAssert_H

// EOF
