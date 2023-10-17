//-------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformMinMax2IndexGet.h
//
//	@doc:
//		Transform aggregates min, max to queries with IndexScan with
//		Limit.
//-------------------------------------------------------------------
#ifndef GPOPT_CXformMinMax2IndexGet_H
#define GPOPT_CXformMinMax2IndexGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------
//	@class:
//		CXformMinMax2IndexGet
//
//	@doc:
//		Transform aggregates min, max to queries with IndexScan with
//		Limit.
//-------------------------------------------------------------------
class CXformMinMax2IndexGet : public CXformExploration
{
private:
	// helper function to validate if index is applicable and determine Index Scan
	// direction, given index columns.
	static EIndexScanDirection GetScanDirection(const IMDIndex *pmdindex,
												CScalarAggFunc *popScAggFunc,
												const IMDType *agg_col_type);

	static BOOL IsMinMaxAggOnColumn(const IMDType *agg_func_type,
									CExpression *pexprAggFunc,
									const CColRef **agg_colref);

	static IMdIdArray *GetApplicableIndices(CMemoryPool *mp,
											const CColRef *agg_colref,
											CColRefArray *output_col_array,
											CMDAccessor *md_accessor,
											const IMDRelation *pmdrel,
											ULONG ulIndices);

public:
	CXformMinMax2IndexGet(const CXformMinMax2IndexGet &) = delete;

	// ctor
	CXformMinMax2IndexGet(CMemoryPool *mp);

	// dtor
	~CXformMinMax2IndexGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfMinMax2IndexGet;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformMinMax2IndexGet";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformMinMax2IndexGet

}  // namespace gpopt


#endif	//GPOPT_CXformMinMax2IndexGet_H
