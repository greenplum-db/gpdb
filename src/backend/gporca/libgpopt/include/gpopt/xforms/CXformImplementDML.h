//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CXformImplementDML.h
//
//	@doc:
//		Transform Logical DML to Physical DML
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementDML_H
#define GPOPT_CXformImplementDML_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementDML
//
//	@doc:
//		Transform Logical DML to Physical DML
//
//---------------------------------------------------------------------------
class CXformImplementDML : public CXformImplementation
{
private:
public:
	CXformImplementDML(const CXformImplementDML &) = delete;

	// ctor
	explicit CXformImplementDML(CMemoryPool *mp);

	// dtor
	~CXformImplementDML() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementDML;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementDML";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformImplementDML
}  // namespace gpopt

#endif	// !GPOPT_CXformImplementDML_H

// EOF
