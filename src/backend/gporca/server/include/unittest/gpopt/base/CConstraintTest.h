//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraintTest.h
//
//	@doc:
//		Test for constraints
//---------------------------------------------------------------------------
#ifndef GPOPT_CConstraintTest_H
#define GPOPT_CConstraintTest_H

#include "gpos/base.h"

#include "gpopt/base/CRange.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintDisjunction.h"
#include "gpopt/base/CConstraintNegation.h"
#include "gpopt/eval/IConstExprEvaluator.h"

#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CConstraintTest
//
//	@doc:
//		Static unit tests for constraints
//
//---------------------------------------------------------------------------
class CConstraintTest
{
	struct SRangeInfo
	{
		CRange::ERangeInclusion eriLeft;   // inclusion for left end
		INT iLeft;						   // left end value
		CRange::ERangeInclusion eriRight;  // inclusion for right end
		INT iRight;						   // right end value
	};

private:
	// number of microseconds in one day
	static const LINT lMicrosecondsPerDay;

	// integer representation for '01-01-2012'
	static const LINT lInternalRepresentationFor2012_01_01;

	// integer representation for '01-21-2012'
	static const LINT lInternalRepresentationFor2012_01_21;

	// integer representation for '01-02-2012'
	static const LINT lInternalRepresentationFor2012_01_02;

	// integer representation for '01-22-2012'
	static const LINT lInternalRepresentationFor2012_01_22;

	// byte representation for '01-01-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_01;

	// byte representation for '01-21-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_21;

	// byte representation for '01-02-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_02;

	// byte representation for '01-22-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_22;

	// construct an array of ranges to be used to create an interval
	static CRangeArray *Pdrgprng(CMemoryPool *mp, IMDId *mdid,
								 const SRangeInfo rgRangeInfo[],
								 ULONG ulRanges);

	static CConstraintInterval *PciFirstInterval(CMemoryPool *mp, IMDId *mdid,
												 CColRef *colref);

	static CConstraintInterval *PciSecondInterval(CMemoryPool *mp, IMDId *mdid,
												  CColRef *colref);

	// interval from scalar comparison
	static GPOS_RESULT EresUnittest_CIntervalFromScalarCmp(
		CMemoryPool *mp, CMDAccessor *md_accessor, CColRef *colref);

	// generate comparison expression
	static CExpression *PexprScalarCmp(CMemoryPool *mp,
									   CMDAccessor *md_accessor,
									   CColRef *colref,
									   IMDType::ECmpType cmp_type, LINT val);

	// interval from scalar bool op
	static GPOS_RESULT EresUnittest_CIntervalFromScalarBoolOp(
		CMemoryPool *mp, CMDAccessor *md_accessor, CColRef *colref);

	// debug print
	static void PrintConstraint(CMemoryPool *mp, CConstraint *pcnstr);

	// build a conjunction
	static CConstraintConjunction *Pcstconjunction(CMemoryPool *mp, IMDId *mdid,
												   CColRef *colref);

	// build a disjunction
	static CConstraintDisjunction *Pcstdisjunction(CMemoryPool *mp, IMDId *mdid,
												   CColRef *colref);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_CInterval();
	static GPOS_RESULT EresUnittest_CIntervalFromScalarExpr();
	static GPOS_RESULT EresUnittest_CConjunction();
	static GPOS_RESULT EresUnittest_CDisjunction();
	static GPOS_RESULT EresUnittest_CNegation();
	static GPOS_RESULT EresUnittest_CConstraintFromScalarExpr();
	static GPOS_RESULT EresUnittest_CConstraintIntervalConvertsTo();
	static GPOS_RESULT EresUnittest_CConstraintIntervalPexpr();
	static GPOS_RESULT EresUnittest_CConstraintIntervalFromArrayExpr();

#ifdef GPOS_DEBUG
	// tests for unconstrainable types
	static GPOS_RESULT EresUnittest_NegativeTests();
#endif	// GPOS_DEBUG

	// test constraints on date intervals
	static GPOS_RESULT EresUnittest_ConstraintsOnDates();

	// print equivalence classes
	static void PrintEquivClasses(CMemoryPool *mp, CColRefSetArray *pdrgpcrs,
								  BOOL fExpected = false);
};	// class CConstraintTest
}  // namespace gpopt

#endif	// !GPOPT_CConstraintTest_H


// EOF
