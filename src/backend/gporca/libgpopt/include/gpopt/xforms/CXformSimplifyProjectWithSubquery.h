//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSimplifyProjectWithSubquery.h
//
//	@doc:
//		Simplify Project with subquery
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSimplifyProjectWithSubquery_H
#define GPOPT_CXformSimplifyProjectWithSubquery_H

#include "gpos/base.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CLogicalProject.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformSimplifySubquery.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSimplifyProjectWithSubquery
//
//	@doc:
//		Simplify Project with subquery
//
//---------------------------------------------------------------------------
class CXformSimplifyProjectWithSubquery : public CXformSimplifySubquery
{
private:
public:
	CXformSimplifyProjectWithSubquery(
		const CXformSimplifyProjectWithSubquery &) = delete;

	// ctor
	explicit CXformSimplifyProjectWithSubquery(CMemoryPool *mp)
		:  // pattern
		  CXformSimplifySubquery(GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalProject(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // relational child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternTree(mp))  // project list
			  ))
	{
	}

	// dtor
	~CXformSimplifyProjectWithSubquery() override = default;

	// Compatibility function for simplifying aggregates
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return (CXform::ExfSimplifyProjectWithSubquery != exfid);
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSimplifyProjectWithSubquery;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSimplifyProjectWithSubquery";
	}

	// is transformation a subquery unnesting (Subquery To Apply) xform?
	BOOL
	FSubqueryUnnesting() const override
	{
		return true;
	}

};	// class CXformSimplifyProjectWithSubquery

}  // namespace gpopt

#endif	// !GPOPT_CXformSimplifyProjectWithSubquery_H

// EOF
