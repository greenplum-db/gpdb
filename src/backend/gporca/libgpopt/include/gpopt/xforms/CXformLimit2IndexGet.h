//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2IndexGet.h
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexGet if order by columns
//		match any of the index prefix
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLimit2IndexGet_H
#define GPOPT_CXformLimit2IndexGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/xforms/CXformExploration.h"
namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLimit2IndexGet
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexGet if order by columns
//		match any of the index prefix
//---------------------------------------------------------------------------
class CXformLimit2IndexGet : public CXformExploration
{
private:
	// helper function to validate if index is applicable and determine Index Scan
	// direction, given OrderSpec and index columns. This function checks if
	// 1. ORDER BY columns are prefix of the index columns
	// 2. Sort and Nulls Direction of ORDER BY columns is either equal or
	//    reversed to the index columns
	static EIndexScanDirection GetScanDirection(
		COrderSpec *pos, CColRefArray *pdrgpcrIndexColumns,
		const IMDIndex *pmdindex);

public:
	CXformLimit2IndexGet(const CXformLimit2IndexGet &) = delete;
	// ctor
	explicit CXformLimit2IndexGet(CMemoryPool *mp);

	// dtor
	~CXformLimit2IndexGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLimit2IndexGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLimit2IndexGet";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;


};	// class CXformLimit2IndexGet

}  // namespace gpopt


#endif	//GPOPT_CXformLimit2IndexGet_H
