//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalUnion.cpp
//
//	@doc:
//		Implementation of union operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalUnion.h"

#include "gpos/base.h"

#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalUnionAll.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::CLogicalUnion
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalUnion::CLogicalUnion(CMemoryPool *mp) : CLogicalSetOp(mp)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::CLogicalUnion
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalUnion::CLogicalUnion(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
							 CColRef2dArray *pdrgpdrgpcrInput)
	: CLogicalSetOp(mp, pdrgpcrOutput, pdrgpdrgpcrInput)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::~CLogicalUnion
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalUnion::~CLogicalUnion() = default;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalUnion::PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist)
{
	CColRefArray *pdrgpcrOutput =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput, colref_mapping, must_exist);
	CColRef2dArray *pdrgpdrgpcrInput = CUtils::PdrgpdrgpcrRemap(
		mp, m_pdrgpdrgpcrInput, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalUnion(mp, pdrgpcrOutput, pdrgpdrgpcrInput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalUnion::DeriveMaxCard(CMemoryPool *,	 // mp
							 CExpressionHandle &exprhdl) const
{
	const ULONG arity = exprhdl.Arity();

	CMaxCard maxcard = exprhdl.DeriveMaxCard(0);
	for (ULONG ul = 1; ul < arity; ul++)
	{
		maxcard += exprhdl.DeriveMaxCard(ul);
	}

	return maxcard;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalUnion::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfUnion2UnionAll);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUnion::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalUnion::PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							IStatisticsArray *	// not used
) const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);

	// union is transformed into a group by over an union all
	// we follow the same route to compute statistics
	IStatistics *pstatsUnionAll =
		CLogicalUnionAll::PstatsDeriveUnionAll(mp, exprhdl);

	// computed columns
	ULongPtrArray *pdrgpulComputedCols = GPOS_NEW(mp) ULongPtrArray(mp);

	IStatistics *stats = CLogicalGbAgg::PstatsDerive(
		mp, pstatsUnionAll,
		m_pdrgpcrOutput,	  // we group by the output columns
		pdrgpulComputedCols,  // no computed columns for set ops
		nullptr				  // no keys, use all grouping cols
	);

	// clean up
	pdrgpulComputedCols->Release();
	pstatsUnionAll->Release();

	return stats;
}


// EOF
