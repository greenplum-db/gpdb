//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalAssert.cpp
//
//	@doc:
//		Implementation of assert operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalAssert.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/operators/CPatternTree.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::CLogicalAssert
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalAssert::CLogicalAssert(CMemoryPool *mp)
	: CLogicalUnary(mp), m_pexc(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::CLogicalAssert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalAssert::CLogicalAssert(CMemoryPool *mp, CException *pexc)
	: CLogicalUnary(mp), m_pexc(pexc)
{
	GPOS_ASSERT(NULL != pexc);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalAssert::Matches(COperator *pop) const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CLogicalAssert *popAssert = CLogicalAssert::PopConvert(pop);
	return CException::Equals(*(popAssert->Pexc()), *m_pexc);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalAssert::DeriveOutputColumns(CMemoryPool *,	// mp
									CExpressionHandle &exprhdl)
{
	return PcrsDeriveOutputPassThru(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalAssert::DeriveKeyCollection(CMemoryPool *,	// mp
									CExpressionHandle &exprhdl) const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalAssert::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfImplementAssert);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalAssert::DeriveMaxCard(CMemoryPool *,  // mp
							  CExpressionHandle &exprhdl) const
{
	// in case of a false condition or a contradiction, maxcard should be 1
	CExpression *pexprScalar = exprhdl.PexprScalarExactChild(1);

	if ((NULL != pexprScalar && CUtils::FScalarConstFalse(pexprScalar)) ||
		exprhdl.DerivePropertyConstraint()->FContradiction())
	{
		return CMaxCard(1 /*ull*/);
	}

	// if Assert operator was generated from MaxOneRow operator,
	// then a max cardinality of 1 is expected
	if (NULL != exprhdl.Pgexpr() &&
		CXform::ExfMaxOneRow2Assert == exprhdl.Pgexpr()->ExfidOrigin())
	{
		return CMaxCard(1 /*ull*/);
	}

	// pass on max card of first child
	return exprhdl.DeriveMaxCard(0);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::PstatsDerive
//
//	@doc:
//		Derive statistics based on filter predicates
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalAssert::PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 IStatisticsArray *	 // not used
) const
{
	CMaxCard maxcard =
		CLogicalAssert::PopConvert(exprhdl.Pop())->DeriveMaxCard(mp, exprhdl);
	if (1 == maxcard.Ull())
	{
		// a max card of one requires re-scaling stats
		IStatistics *stats = exprhdl.Pstats(0);
		return stats->ScaleStats(mp, CDouble(1.0 / stats->Rows()));
	}

	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalAssert::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalAssert::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " (Error code: " << m_pexc->GetSQLState() << ")";
	return os;
}

// EOF
