//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformProject2ComputeScalar.h
//
//	@doc:
//		Transform Project to ComputeScalar
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformProject2ComputeScalar_H
#define GPOPT_CXformProject2ComputeScalar_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformProject2ComputeScalar
//
//	@doc:
//		Transform Project to ComputeScalar
//
//---------------------------------------------------------------------------
class CXformProject2ComputeScalar : public CXformImplementation
{
private:
public:
	CXformProject2ComputeScalar(const CXformProject2ComputeScalar &) = delete;

	// ctor
	explicit CXformProject2ComputeScalar(CMemoryPool *mp);

	// dtor
	~CXformProject2ComputeScalar() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfProject2ComputeScalar;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformProject2ComputeScalar";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &exprhdl) const override
	{
		if (exprhdl.DeriveHasSubquery(1))
		{
			return CXform::ExfpNone;
		}

		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformProject2ComputeScalar

}  // namespace gpopt

#endif	// !GPOPT_CXformProject2ComputeScalar_H

// EOF
