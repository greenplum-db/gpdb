//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformSelect2Apply.h
//
//	@doc:
//		Transform Select to Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2Apply_H
#define GPOPT_CXformSelect2Apply_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformSubqueryUnnest.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSelect2Apply
//
//	@doc:
//		Transform Select to Apply; this transformation is only applicable
//		to a Select expression with subqueries in its scalar predicate
//
//---------------------------------------------------------------------------
class CXformSelect2Apply : public CXformSubqueryUnnest
{
private:
public:
	CXformSelect2Apply(const CXformSelect2Apply &) = delete;

	// ctor
	explicit CXformSelect2Apply(CMemoryPool *mp);

	// dtor
	~CXformSelect2Apply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSelect2Apply;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSelect2Apply";
	}

};	// class CXformSelect2Apply

}  // namespace gpopt

#endif	// !GPOPT_CXformSelect2Apply_H

// EOF
