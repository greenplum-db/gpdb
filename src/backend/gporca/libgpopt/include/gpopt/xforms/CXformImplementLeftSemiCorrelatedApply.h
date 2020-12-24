//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates..
//
//	@filename:
//		CXformImplementLeftSemiCorrelatedApply.h
//
//	@doc:
//		Transform left semi correlated apply to physical left semi
//		correlated join
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftSemiCorrelatedApply_H
#define GPOPT_CXformImplementLeftSemiCorrelatedApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftSemiCorrelatedApply.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftSemiNLJoin.h"
#include "gpopt/xforms/CXformImplementCorrelatedApply.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------------
//	@class:
//		CXformImplementLeftSemiCorrelatedApply
//
//	@doc:
//		Transform left semi correlated apply to physical left semi
//		correlated join
//
//-------------------------------------------------------------------------
class CXformImplementLeftSemiCorrelatedApply
	: public CXformImplementCorrelatedApply<CLogicalLeftSemiCorrelatedApply,
											CPhysicalCorrelatedLeftSemiNLJoin>
{
private:
public:
	CXformImplementLeftSemiCorrelatedApply(
		const CXformImplementLeftSemiCorrelatedApply &) = delete;

	// ctor
	explicit CXformImplementLeftSemiCorrelatedApply(CMemoryPool *mp)
		: CXformImplementCorrelatedApply<CLogicalLeftSemiCorrelatedApply,
										 CPhysicalCorrelatedLeftSemiNLJoin>(mp)
	{
	}

	// dtor
	~CXformImplementLeftSemiCorrelatedApply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLeftSemiCorrelatedApply;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLeftSemiCorrelatedApply";
	}

};	// class CXformImplementLeftSemiCorrelatedApply

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLeftSemiCorrelatedApply_H

// EOF
