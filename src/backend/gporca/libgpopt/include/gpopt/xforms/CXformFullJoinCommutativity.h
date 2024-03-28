//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 VMware by Broadcom
//
//	@filename:
//		CXformFullJoinCommutativity.h
//
//	@doc:
//		Transform join by commutativity
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformFullJoinCommutativity_H
#define GPOPT_CXformFullJoinCommutativity_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalFullOuterJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformJoinCommutativity.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformFullJoinCommutativity
//
//	@doc:
//		Commutative transformation of full join
//
//---------------------------------------------------------------------------
class CXformFullJoinCommutativity : public CXformJoinCommutativity
{
private:
public:
	CXformFullJoinCommutativity(const CXformFullJoinCommutativity &) = delete;

	// ctor
	explicit CXformFullJoinCommutativity(CMemoryPool *mp)
		: CXformJoinCommutativity(
			  // pattern
			  GPOS_NEW(mp) CExpression(
				  mp, GPOS_NEW(mp) CLogicalFullOuterJoin(mp),
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
	~CXformFullJoinCommutativity() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfFullJoinCommutativity;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformFullJoinCommutativity";
	}
};	// class CXformFullJoinCommutativity

}  // namespace gpopt


#endif	// !GPOPT_CXformFullJoinCommutativity_H

// EOF
