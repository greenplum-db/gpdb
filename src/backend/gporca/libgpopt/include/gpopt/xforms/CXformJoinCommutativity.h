//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformJoinCommutativity.h
//
//	@doc:
//		Transform join by commutativity
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoinCommutativity_H
#define GPOPT_CXformJoinCommutativity_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformJoinCommutativity
//
//	@doc:
//		Commutative transformation of join
//
//---------------------------------------------------------------------------
class CXformJoinCommutativity : public CXformExploration
{
private:
public:
	CXformJoinCommutativity(const CXformJoinCommutativity &) = delete;

	// ctor
	explicit CXformJoinCommutativity(CMemoryPool *mp);

	// dtor
	~CXformJoinCommutativity() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfJoinCommutativity;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformJoinCommutativity";
	}

	// compatibility function
	BOOL FCompatible(CXform::EXformId exfid) override;

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  // exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformJoinCommutativity

}  // namespace gpopt


#endif	// !GPOPT_CXformJoinCommutativity_H

// EOF
