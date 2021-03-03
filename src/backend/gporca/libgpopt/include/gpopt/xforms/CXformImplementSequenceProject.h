//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementSequenceProject.h
//
//	@doc:
//		Transform Logical Sequence Project to Physical Sequence Project
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementSequenceProject_H
#define GPOPT_CXformImplementSequenceProject_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementSequenceProject
//
//	@doc:
//		Transform Project to ComputeScalar
//
//---------------------------------------------------------------------------
class CXformImplementSequenceProject : public CXformImplementation
{
private:
public:
	CXformImplementSequenceProject(const CXformImplementSequenceProject &) =
		delete;

	// ctor
	explicit CXformImplementSequenceProject(CMemoryPool *mp);

	// dtor
	~CXformImplementSequenceProject() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementSequenceProject;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementSequenceProject";
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

};	// class CXformImplementSequenceProject

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementSequenceProject_H

// EOF
