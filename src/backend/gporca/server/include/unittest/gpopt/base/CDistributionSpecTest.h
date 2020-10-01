//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecTest.h
//
//	@doc:
//		Test for distribution spec
//---------------------------------------------------------------------------
#ifndef GPOS_CDistributionSpecTest_H
#define GPOS_CDistributionSpecTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecTest
//
//	@doc:
//		Static unit tests for distribution specs
//
//---------------------------------------------------------------------------
class CDistributionSpecTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Any();
	static GPOS_RESULT EresUnittest_Singleton();
	static GPOS_RESULT EresUnittest_Replicated();
	static GPOS_RESULT EresUnittest_Universal();
	static GPOS_RESULT EresUnittest_Random();
	static GPOS_RESULT EresUnittest_Hashed();
#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_NegativeAny();
	static GPOS_RESULT EresUnittest_NegativeUniversal();
	static GPOS_RESULT EresUnittest_NegativeRandom();
#endif	// GPOS_DEBUG

};	// class CDistributionSpecTest
}  // namespace gpopt

#endif	// !GPOS_CDistributionSpecTest_H


// EOF
