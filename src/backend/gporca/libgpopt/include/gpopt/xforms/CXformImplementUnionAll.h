//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementUnionAll.h
//
//	@doc:
//		Transform Logical into Physical Union All
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementUnionAll_H
#define GPOPT_CXformImplementUnionAll_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementUnionAll
//
//	@doc:
//		Transform Logical into Physical Union All
//
//---------------------------------------------------------------------------
class CXformImplementUnionAll : public CXformImplementation
{
private:
public:
	CXformImplementUnionAll(const CXformImplementUnionAll &) = delete;

	// ctor
	explicit CXformImplementUnionAll(CMemoryPool *mp);

	// dtor
	~CXformImplementUnionAll() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementUnionAll;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementUnionAll";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformImplementUnionAll

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementUnionAll_H

// EOF
