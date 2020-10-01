//---------------------------------------------------------------------------
//	Pivotal Software, Inc
//	Copyright (C) 2017 Pivotal Software, Inc
//---------------------------------------------------------------------------

#ifndef GPOPT_CEquivalenceClassesTest_H
#define GPOPT_CEquivalenceClassesTest_H

#include "gpos/base.h"
#include "gpopt/base/CColRefSet.h"

namespace gpopt
{
using namespace gpos;

// Static unit tests for equivalence classes
class CEquivalenceClassesTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_NotDisjointEquivalanceClasses();
	static GPOS_RESULT EresUnittest_IntersectEquivalanceClasses();
	static CColRefSetArray *createEquivalenceClasses(CMemoryPool *mp,
													 CColRefSet *pcrs,
													 int breakpoints[]);

};	// class CEquivalenceClassesTest
}  // namespace gpopt

#endif	// !GPOPT_CEquivalenceClassesTest_H


// EOF
