//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformJoinAssociativity.h
//
//	@doc:
//		Transform left-deep join tree by associativity
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoinAssociativity_H
#define GPOPT_CXformJoinAssociativity_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformJoinAssociativity
//
//	@doc:
//		Associative transformation of left-deep join tree
//
//---------------------------------------------------------------------------
class CXformJoinAssociativity : public CXformExploration
{
private:
	// helper function for creating the new join predicate
	static void CreatePredicates(CMemoryPool *mp, CExpression *pexpr,
								 CExpressionArray *pdrgpexprLower,
								 CExpressionArray *pdrgpexprUpper);

public:
	CXformJoinAssociativity(const CXformJoinAssociativity &) = delete;

	// ctor
	explicit CXformJoinAssociativity(CMemoryPool *mp);

	// dtor
	~CXformJoinAssociativity() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfJoinAssociativity;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformJoinAssociativity";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformJoinAssociativity

}  // namespace gpopt


#endif	// !GPOPT_CXformJoinAssociativity_H

// EOF
