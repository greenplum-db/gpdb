//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply.h
//
//	@doc:
//		Transform inner join over Select over a partitioned table into
//		a union-all of dynamic index get applies
//
//	@owner:
//		n
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply_H
#define GPOPT_CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformJoin2IndexApply.h"
#include "gpopt/xforms/CXformJoin2IndexApplyBase.h"

namespace gpopt
{
using namespace gpos;

// fwd declarations
class CExpression;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply
//
//	@doc:
//		Transform inner join over Select over a partitioned table into a union-all
//		of dynamic index get applies
//
//---------------------------------------------------------------------------
class CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply
	: public CXformJoin2IndexApplyBase<
		  CLogicalInnerJoin, CLogicalIndexApply, CLogicalDynamicGet,
		  true /*fWithSelect*/, true /*is_partial*/, IMDIndex::EmdindBtree>
{
public:
	// ctor
	explicit CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply(
		CMemoryPool *mp)
		: CXformJoin2IndexApplyBase<CLogicalInnerJoin, CLogicalIndexApply,
									CLogicalDynamicGet, true /*fWithSelect*/,
									true /*is_partial*/, IMDIndex::EmdindBtree>(
			  mp)
	{
	}

	// dtor
	~CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply() override =
		default;

	// compute xform promise for a given expression handle
	CXform::EXformPromise
	Exfp(CExpressionHandle &exprhdl) const override
	{
		if (CXform::ExfpNone == CXformJoin2IndexApply::Exfp(exprhdl))
		{
			return CXform::ExfpNone;
		}

		if (exprhdl.DeriveHasPartialIndexes(1))
		{
			return CXform::ExfpHigh;
		}

		return CXform::ExfpNone;
	}

	// ident accessor
	EXformId
	Exfid() const override
	{
		return ExfInnerJoinWithInnerSelect2PartialDynamicIndexGetApply;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply";
	}

	// return true if xform should be applied only once
	BOOL
	IsApplyOnce() override
	{
		return true;
	}
};
}  // namespace gpopt

#endif	// !GPOPT_CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply_H

// EOF
