//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2DynamicIndexGet.h
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalDynamicIndexGet if order by
//		columns match any of the index that has partition columns as its prefix
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLimit2DynamicIndexGet_H
#define GPOPT_CXformLimit2DynamicIndexGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformLimit2IndexGet.h"
namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLimit2DynamicIndexGet
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalDynamicIndexGet if order by
//		columns match any of the index that has partition columns as its prefix
//---------------------------------------------------------------------------
class CXformLimit2DynamicIndexGet : public CXformExploration
{
private:
	// helper function to validate if index is applicable, given OrderSpec
	// and index columns. This function checks if
	// 1. ORDER BY columns are prefix of the index that has partition columns in its prefix
	// 2. Sort and Nulls Direction of ORDER BY columns is either equal or commutative to the index columns
	static BOOL FIndexApplicableForOrderBy(CMemoryPool *mp, COrderSpec *pos,
											   const IMDRelation *pmdrel,
											   const IMDIndex *pmdindex,
											   CLogicalDynamicGet *popDynGet);

public:
	CXformLimit2DynamicIndexGet(const CXformLimit2DynamicIndexGet &) = delete;
	// ctor
	explicit CXformLimit2DynamicIndexGet(CMemoryPool *mp);

	// dtor
	~CXformLimit2DynamicIndexGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLimit2DynamicIndexGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLimit2DynamicIndexGet";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
	CExpression *pexpr) const override;
};
// class CXformLimit2DynamicIndexGet

}  // namespace gpopt


#endif	//GPOPT_CXformLimit2DynamicIndexGet_H
