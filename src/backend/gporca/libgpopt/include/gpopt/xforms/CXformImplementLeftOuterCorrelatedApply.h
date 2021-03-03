//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementLeftOuterCorrelatedApply.h
//
//	@doc:
//		Transform LeftOuter correlated apply to physical LeftOuter correlated
//		apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftOuterCorrelatedApply_H
#define GPOPT_CXformImplementLeftOuterCorrelatedApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftOuterCorrelatedApply.h"
#include "gpopt/operators/CPhysicalCorrelatedLeftOuterNLJoin.h"
#include "gpopt/xforms/CXformImplementCorrelatedApply.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------------
//	@class:
//		CXformImplementLeftOuterCorrelatedApply
//
//	@doc:
//		Transform LeftOuter correlated apply to physical LeftOuter correlated
//		apply
//
//-------------------------------------------------------------------------
class CXformImplementLeftOuterCorrelatedApply
	: public CXformImplementCorrelatedApply<CLogicalLeftOuterCorrelatedApply,
											CPhysicalCorrelatedLeftOuterNLJoin>
{
private:
public:
	CXformImplementLeftOuterCorrelatedApply(
		const CXformImplementLeftOuterCorrelatedApply &) = delete;

	// ctor
	explicit CXformImplementLeftOuterCorrelatedApply(CMemoryPool *mp)
		: CXformImplementCorrelatedApply<CLogicalLeftOuterCorrelatedApply,
										 CPhysicalCorrelatedLeftOuterNLJoin>(mp)
	{
	}

	// dtor
	~CXformImplementLeftOuterCorrelatedApply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementLeftOuterCorrelatedApply;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformImplementLeftOuterCorrelatedApply";
	}

};	// class CXformImplementLeftOuterCorrelatedApply

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementLeftOuterCorrelatedApply_H

// EOF
