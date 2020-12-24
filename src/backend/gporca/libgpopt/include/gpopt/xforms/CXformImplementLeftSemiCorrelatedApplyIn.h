//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates..
//
//	@filename:
//		CXformImplementLeftSemiCorrelatedApplyIn.h
//
//	@doc:
//		Transform left semi correlated apply with IN/ANY semantics
//		to physical left semi correlated join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftSemiCorrelatedApplyIn_H
#define GPOPT_CXformImplementLeftSemiCorrelatedApplyIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiCorrelatedApplyIn.h"
#include "gpopt/operators/CPhysicalCorrelatedInLeftSemiNLJoin.h"
#include "gpopt/xforms/CXformImplementCorrelatedApply.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------------
//	@class:
//		CXformImplementLeftSemiCorrelatedApplyIn
//
//	@doc:
//		Transform left semi correlated apply with IN/ANY semantics
//		to physical left semi correlated join
//
//-------------------------------------------------------------------------
class CXformImplementLeftSemiCorrelatedApplyIn
	: public CXformImplementCorrelatedApply<CLogicalLeftSemiCorrelatedApplyIn,
											CPhysicalCorrelatedInLeftSemiNLJoin>
{
private:
public:
	CXformImplementLeftSemiCorrelatedApplyIn(
		const CXformImplementLeftSemiCorrelatedApplyIn &) = delete;

	// ctor
	explicit CXformImplementLeftSemiCorrelatedApplyIn(CMemoryPool *mp)
		: CXformImplementCorrelatedApply<CLogicalLeftSemiCorrelatedApplyIn,
										 CPhysicalCorrelatedInLeftSemiNLJoin>(
			  mp)
	{
	}

	// dtor
	~CXformImplementLeftSemiCorrelatedApplyIn() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLeftSemiCorrelatedApplyIn;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLeftSemiCorrelatedApplyIn";
	}

};	// class CXformImplementLeftSemiCorrelatedApplyIn

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLeftSemiCorrelatedApplyIn_H

// EOF
