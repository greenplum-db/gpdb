//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSimplifySelectWithSubquery.h
//
//	@doc:
//		Simplify Select with subquery
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSimplifySelectWithSubquery_H
#define GPOPT_CXformSimplifySelectWithSubquery_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternTree.h"
#include "gpopt/xforms/CXformSimplifySubquery.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSimplifySelectWithSubquery
//
//	@doc:
//		Simplify Select with subquery
//
//---------------------------------------------------------------------------
class CXformSimplifySelectWithSubquery : public CXformSimplifySubquery
{
private:
public:
	CXformSimplifySelectWithSubquery(const CXformSimplifySelectWithSubquery &) =
		delete;

	// ctor
	explicit CXformSimplifySelectWithSubquery(CMemoryPool *mp)
		:  // pattern
		  CXformSimplifySubquery(GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalSelect(mp),
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // relational child
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CPatternTree(mp))  // predicate tree
			  ))
	{
	}

	// dtor
	~CXformSimplifySelectWithSubquery() override = default;

	// Compatibility function for simplifying aggregates
	BOOL
	FCompatible(CXform::EXformId exfid) override
	{
		return (CXform::ExfSimplifySelectWithSubquery != exfid);
	}

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSimplifySelectWithSubquery;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformSimplifySelectWithSubquery";
	}

	// is transformation a subquery unnesting (Subquery To Apply) xform?
	BOOL
	FSubqueryUnnesting() const override
	{
		return true;
	}

};	// class CXformSimplifySelectWithSubquery

}  // namespace gpopt

#endif	// !GPOPT_CXformSimplifySelectWithSubquery_H

// EOF
