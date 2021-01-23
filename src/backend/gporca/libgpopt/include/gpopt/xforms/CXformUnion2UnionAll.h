//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformUnion2UnionAll.h
//
//	@doc:
//		Transform logical union into an aggregate over a logical union all
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformUnion2UnionAll_H
#define GPOPT_CXformUnion2UnionAll_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformUnion2UnionAll
//
//	@doc:
//		Transform logical union into an aggregate over a logical union all
//
//---------------------------------------------------------------------------
class CXformUnion2UnionAll : public CXformExploration
{
private:
public:
	CXformUnion2UnionAll(const CXformUnion2UnionAll &) = delete;

	// ctor
	explicit CXformUnion2UnionAll(CMemoryPool *mp);

	// dtor
	~CXformUnion2UnionAll() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfUnion2UnionAll;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformUnion2UnionAll";
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

};	// class CXformUnion2UnionAll

}  // namespace gpopt

#endif	// !GPOPT_CXformUnion2UnionAll_H

// EOF
