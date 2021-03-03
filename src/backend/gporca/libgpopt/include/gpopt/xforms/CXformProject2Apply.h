//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformProject2Apply.h
//
//	@doc:
//		Transform Project to Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformProject2Apply_H
#define GPOPT_CXformProject2Apply_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformSubqueryUnnest.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformProject2Apply
//
//	@doc:
//		Transform Project to Apply; this transformation is only applicable
//		to a Project expression with subqueries in its scalar project list
//
//---------------------------------------------------------------------------
class CXformProject2Apply : public CXformSubqueryUnnest
{
private:
public:
	CXformProject2Apply(const CXformProject2Apply &) = delete;

	// ctor
	explicit CXformProject2Apply(CMemoryPool *mp);

	// dtor
	~CXformProject2Apply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfProject2Apply;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformProject2Apply";
	}

};	// class CXformProject2Apply

}  // namespace gpopt

#endif	// !GPOPT_CXformProject2Apply_H

// EOF
