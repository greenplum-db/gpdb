//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates..
//
//	@filename:
//		CXformImplementLeftAntiSemiCorrelatedApply.h
//
//	@doc:
//		Transform left anti semi correlated apply (for NOT EXISTS subqueries)
//		to physical left anti semi correlated join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftAntiSemiCorrelatedApply_H
#define GPOPT_CXformImplementLeftAntiSemiCorrelatedApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiCorrelatedApply.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftAntiSemiNLJoin.h"
#include "gpopt/xforms/CXformImplementCorrelatedApply.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------------
//	@class:
//		CXformImplementLeftAntiSemiCorrelatedApply
//
//	@doc:
//		Transform left anti semi correlated apply  (for NOT EXISTS subqueries)
//		to physical left anti semi correlated join
//
//-------------------------------------------------------------------------
class CXformImplementLeftAntiSemiCorrelatedApply
	: public CXformImplementCorrelatedApply<
		  CLogicalLeftAntiSemiCorrelatedApply,
		  CPhysicalCorrelatedLeftAntiSemiNLJoin>
{
private:
public:
	CXformImplementLeftAntiSemiCorrelatedApply(
		const CXformImplementLeftAntiSemiCorrelatedApply &) = delete;

	// ctor
	explicit CXformImplementLeftAntiSemiCorrelatedApply(CMemoryPool *mp)
		: CXformImplementCorrelatedApply<CLogicalLeftAntiSemiCorrelatedApply,
										 CPhysicalCorrelatedLeftAntiSemiNLJoin>(
			  mp)
	{
	}

	// dtor
	~CXformImplementLeftAntiSemiCorrelatedApply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLeftAntiSemiCorrelatedApply;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLeftAntiSemiCorrelatedApply";
	}

};	// class CXformImplementLeftAntiSemiCorrelatedApply

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLeftAntiSemiCorrelatedApply_H

// EOF
