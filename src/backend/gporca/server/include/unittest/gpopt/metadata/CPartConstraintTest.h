//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPartConstraintTest.h
//
//	@doc:
//      Test for CPartConstraint
//---------------------------------------------------------------------------
#ifndef GPOPT_CPartConstraintTest_H
#define GPOPT_CPartConstraintTest_H

#include "gpos/base.h"
#include "gpopt/base/CConstraint.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPartConstraintTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CPartConstraintTest
{
private:
	// number of microseconds in one day
	static const LINT lMicrosecondsPerDay;

	// integer representation for '01-01-2012'
	static const LINT lInternalRepresentationFor2012_01_01;

	// integer representation for '01-21-2012'
	static const LINT lInternalRepresentationFor2012_01_21;

	// integer representation for '01-22-2012'
	static const LINT lInternalRepresentationFor2012_01_22;

	// byte representation for '01-01-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_01;

	// byte representation for '01-21-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_21;

	// byte representation for '01-22-2012'
	static const WCHAR *wszInternalRepresentationFor2012_01_22;

	// create an interval constraint for the given column and interval boundaries
	static CConstraint *PcnstrInterval(CMemoryPool *mp, CColRef *colref,
									   ULONG ulLeft, ULONG ulRight);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

	// unit tests for date interval constraints
	static GPOS_RESULT EresUnittest_DateIntervals();

};	// class CPartConstraintTest
}  // namespace gpopt

#endif	// !GPOPT_CPartConstraintTest_H

// EOF
