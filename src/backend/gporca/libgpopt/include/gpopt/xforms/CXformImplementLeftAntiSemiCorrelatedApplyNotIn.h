//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates..
//
//	@filename:
//		CXformImplementLeftAntiSemiCorrelatedApplyNotIn.h
//
//	@doc:
//		Transform left anti semi correlated apply with NOT-IN/ALL semantics
//		to physical left anti semi correlated join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftAntiSemiCorrelatedApplyNotIn_H
#define GPOPT_CXformImplementLeftAntiSemiCorrelatedApplyNotIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiCorrelatedApplyNotIn.h"
#include "gpopt/operators/CPhysicalCorrelatedNotInLeftAntiSemiNLJoin.h"
#include "gpopt/xforms/CXformImplementCorrelatedApply.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------------
//	@class:
//		CXformImplementLeftAntiSemiCorrelatedApplyNotIn
//
//	@doc:
//		Transform left anti semi correlated apply with NOT-IN/ALL semantics
//		to physical left anti semi correlated join
//
//-------------------------------------------------------------------------
class CXformImplementLeftAntiSemiCorrelatedApplyNotIn
	: public CXformImplementCorrelatedApply<
		  CLogicalLeftAntiSemiCorrelatedApplyNotIn,
		  CPhysicalCorrelatedNotInLeftAntiSemiNLJoin>
{
private:
public:
	CXformImplementLeftAntiSemiCorrelatedApplyNotIn(
		const CXformImplementLeftAntiSemiCorrelatedApplyNotIn &) = delete;

	// ctor
	explicit CXformImplementLeftAntiSemiCorrelatedApplyNotIn(CMemoryPool *mp)
		: CXformImplementCorrelatedApply<
			  CLogicalLeftAntiSemiCorrelatedApplyNotIn,
			  CPhysicalCorrelatedNotInLeftAntiSemiNLJoin>(mp)
	{
	}

	// dtor
	~CXformImplementLeftAntiSemiCorrelatedApplyNotIn() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLeftAntiSemiCorrelatedApplyNotIn;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLeftAntiSemiCorrelatedApplyNotIn";
	}

};	// class CXformImplementLeftAntiSemiCorrelatedApplyNotIn

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLeftAntiSemiCorrelatedApplyNotIn_H

// EOF
