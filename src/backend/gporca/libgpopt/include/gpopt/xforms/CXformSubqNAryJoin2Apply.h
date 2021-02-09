//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSubqNAryJoin2Apply.h
//
//	@doc:
//		Transform NAry Join to Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSubqNAryJoin2Apply_H
#define GPOPT_CXformSubqNAryJoin2Apply_H

#include "gpos/base.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CLogicalNAryJoin.h"
#include "gpopt/operators/CPatternMultiLeaf.h"
#include "gpopt/operators/CPatternTree.h"
#include "gpopt/xforms/CXformSubqJoin2Apply.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformSubqNAryJoin2Apply
//
//	@doc:
//		Transform NAry Join with subquery predicates to Apply
//
//---------------------------------------------------------------------------
class CXformSubqNAryJoin2Apply : public CXformSubqJoin2Apply
{
private:
public:
	CXformSubqNAryJoin2Apply(const CXformSubqNAryJoin2Apply &) = delete;

	// ctor
	explicit CXformSubqNAryJoin2Apply(CMemoryPool *mp)
		: CXformSubqJoin2Apply(
			  // pattern
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CLogicalNAryJoin(mp),
				  GPOS_NEW(mp)
					  CExpression(mp, GPOS_NEW(mp) CPatternMultiLeaf(mp)),
				  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))))
	{
	}

	// dtor
	~CXformSubqNAryJoin2Apply() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfSubqNAryJoin2Apply;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformSubqNAryJoin2Apply";
	}

};	// class CXformSubqNAryJoin2Apply

}  // namespace gpopt

#endif	// !GPOPT_CXformSubqNAryJoin2Apply_H

// EOF
