//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformCTEAnchor2Sequence.h
//
//	@doc:
//		Transform logical CTE anchor to logical sequence over CTE producer
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformCTEAnchor2Sequence_H
#define GPOPT_CXformCTEAnchor2Sequence_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformCTEAnchor2Sequence
//
//	@doc:
//		Transform logical CTE anchor to logical sequence over CTE producer
//
//---------------------------------------------------------------------------
class CXformCTEAnchor2Sequence : public CXformExploration
{
private:
public:
	CXformCTEAnchor2Sequence(const CXformCTEAnchor2Sequence &) = delete;

	// ctor
	explicit CXformCTEAnchor2Sequence(CMemoryPool *mp);

	// dtor
	~CXformCTEAnchor2Sequence() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfCTEAnchor2Sequence;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformCTEAnchor2Sequence";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformCTEAnchor2Sequence
}  // namespace gpopt

#endif	// !GPOPT_CXformCTEAnchor2Sequence_H

// EOF
