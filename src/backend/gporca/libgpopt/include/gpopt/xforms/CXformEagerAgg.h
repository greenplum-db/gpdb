//---------------------------------------------------------------------------

//  Greenplum Database
//  Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//  @filename:
//      CXformEagerAgg.h
//
//  @doc:
//      Eagerly push aggregates below join when there is no primary/foreign keys
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformEagerAgg_H
#define GPOPT_CXformEagerAgg_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformEagerAgg
//
//	@doc:
//		Eagerly push aggregates below join
//
//---------------------------------------------------------------------------
class CXformEagerAgg : public CXformExploration
{
public:
	CXformEagerAgg(const CXformEagerAgg &) = delete;

	// ctor
	explicit CXformEagerAgg(CMemoryPool *mp);

	// ctor
	explicit CXformEagerAgg(CExpression *exprPattern);

	// dtor
	~CXformEagerAgg() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfEagerAgg;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformEagerAgg";
	}

	// compatibility function for eager aggregation
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return (CXform::ExfEagerAgg != exfid) &&
			   (CXform::ExfSplitGbAgg != exfid) &&
			   (CXform::ExfSplitDQA != exfid);
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *expr) const override;

	// return true if xform should be applied only once
	BOOL
	IsApplyOnce() override
	{
		return true;
	};

private:
	// check if transform can be applied
	BOOL CanApplyTransform(CExpression *agg_expr) const;

	// is this aggregate supported for push down?
	BOOL CanPushAggBelowJoin(CExpression *scalar_agg_func_expr) const;

	// generate project lists for the lower and upper aggregates
	// from all the original aggregates
	void PopulateLowerUpperProjectList(
		CMemoryPool *mp,			  // memory pool
		CExpression *orig_proj_list,  // project list of the original aggregate
		CExpression *
			*lower_proj_list,  // output project list of the new lower aggregate
		CExpression *
			*upper_proj_list  // output project list of the new upper aggregate
	) const;

	// generate project element for lower aggregate for a single original aggregate
	void PopulateLowerProjectElement(
		CMemoryPool *mp,  // memory pool
		IMDId *agg_mdid,  // original global aggregate function
		CWStringConst *agg_name, CExpressionArray *agg_arg_array,
		BOOL is_distinct,
		CExpression **lower_proj_elem_expr	// output project element of the new
											// lower aggregate
	) const;

	// generate project element for upper aggregate
	void PopulateUpperProjectElement(
		CMemoryPool *mp,  // memory pool
		IMDId *agg_mdid,  // aggregate mdid to create
		CWStringConst *agg_name, CColRef *lower_colref, CColRef *output_colref,
		BOOL is_distinct,
		CExpression **upper_proj_elem_expr	// output project element of the new
											// upper aggregate
	) const;
};	// class CXformEagerAgg
}  // namespace gpopt

#endif	// !GPOPT_CXformEagerAgg_H

// EOF
