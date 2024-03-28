//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware by Broadcom
//
//	@filename:
//		CXformInnerJoinCommutativity.h
//
//	@doc:
//		Transform join by commutativity
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformInnerJoinCommutativity_H
#define GPOPT_CXformInnerJoinCommutativity_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformJoinCommutativity.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformInnerJoinCommutativity
//
//	@doc:
//		Commutative transformation of inner join
//
//---------------------------------------------------------------------------
class CXformInnerJoinCommutativity : public CXformJoinCommutativity
{
private:
public:
	CXformInnerJoinCommutativity(const CXformInnerJoinCommutativity &) = delete;

	// ctor
	explicit CXformInnerJoinCommutativity(CMemoryPool *mp)
		: CXformJoinCommutativity(
			  // pattern
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CLogicalInnerJoin(mp),
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // left child
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // right child
				  GPOS_NEW(mp) CExpression(
					  mp, GPOS_NEW(mp) CPatternLeaf(mp)))  // predicate
		  )
	{
	}

	// dtor
	~CXformInnerJoinCommutativity() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInnerJoinCommutativity;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInnerJoinCommutativity";
	}
};	// class CXformInnerJoinCommutativity

}  // namespace gpopt


#endif	// !GPOPT_CXformInnerJoinCommutativity_H

// EOF
