//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformSelect2Filter.h
//
//	@doc:
//		Transform Select to Filter
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2Filter_H
#define GPOPT_CXformSelect2Filter_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2Filter
//
//	@doc:
//		Transform Select to Filter
//
//---------------------------------------------------------------------------
class CXformSelect2Filter : public CXformImplementation
{
private:
public:
	CXformSelect2Filter(const CXformSelect2Filter &) = delete;

	// ctor
	explicit CXformSelect2Filter(CMemoryPool *mp);

	// dtor
	~CXformSelect2Filter() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSelect2Filter;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformSelect2Filter";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformSelect2Filter

}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2Filter_H

// EOF
