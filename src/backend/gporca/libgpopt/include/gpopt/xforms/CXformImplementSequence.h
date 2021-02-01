//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementSequence.h
//
//	@doc:
//		Transform logical to physical Sequence
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementSequence_H
#define GPOPT_CXformImplementSequence_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementSequence
//
//	@doc:
//		Transform logical to physical Sequence
//
//---------------------------------------------------------------------------
class CXformImplementSequence : public CXformImplementation
{
private:
public:
	CXformImplementSequence(const CXformImplementSequence &) = delete;

	// ctor
	explicit CXformImplementSequence(CMemoryPool *);

	// dtor
	~CXformImplementSequence() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementSequence;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementSequence";
	}

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

};	// class CXformImplementSequence

}  // namespace gpopt


#endif	// !GPOPT_CXformImplementSequence_H

// EOF
