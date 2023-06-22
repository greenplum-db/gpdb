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
	// helper function for creating a limit expression
	static CExpression *PexprLimit(
		CMemoryPool *mp,				// memory pool
		CExpression *pexprRelational,	// relational child
		CExpression *pexprScalarStart,	// limit offset
		CExpression *pexprScalarRows,	// limit count
		COrderSpec *pos,				// ordering specification
		BOOL fGlobal,					// is it a local or global limit
		BOOL fHasCount,					// does limit specify a number of rows
		BOOL fTopLimitUnderDML);

	// helper function to validate if index is applicable, given order by
	// and index columns
	static BOOL FIndexApplicableForOrderBy(CColRefArray *pOrderByCols,
										   CColRefArray *pdrgpcrIndexColumns);

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
