//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2012 EMC Corp.
//
//	@filename:
//		CMaxCardTest.cpp
//
//	@doc:
//		Tests for max card computation
//---------------------------------------------------------------------------

#include "unittest/gpopt/base/CMaxCardTest.h"

#include "gpos/base.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/base/CMaxCard.h"

#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CMaxCardTest::EresUnittest
//
//	@doc:
//		Unittest for key collections
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMaxCardTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMaxCardTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CMaxCardTest::EresUnittest_RunMinidumpTests)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CMaxCardTest::EresUnittest_Basics
//
//	@doc:
//		Basic test for key collections
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMaxCardTest::EresUnittest_Basics()
{
#ifdef GPOS_DEBUG

	CMaxCard mcOne(1);
	CMaxCard mcTwo(1);
	GPOS_ASSERT(mcOne == mcTwo);

	CMaxCard mcThree;
	GPOS_ASSERT(!(mcOne == mcThree));

	CMaxCard mcFour(0);
	mcFour *= mcThree;
	GPOS_ASSERT(0 == mcFour);

	mcFour += mcOne;
	GPOS_ASSERT(1 == mcFour);

	mcFour *= mcThree;
	GPOS_ASSERT(GPOPT_MAX_CARD == mcFour);

	mcFour += mcThree;
	GPOS_ASSERT(GPOPT_MAX_CARD == mcFour);

#endif

	return GPOS_OK;
}

GPOS_RESULT
CMaxCardTest::EresUnittest_RunMinidumpTests()
{
	ULONG ulTestCounter = 0;
	// minidump files
	const CHAR *rgszFileNames[] = {
		"../data/dxl/minidump/FullOuterJoinMaxCardRightChild.mdp",
		"../data/dxl/minidump/FullOuterJoinMaxCardLeftChild.mdp",
		"../data/dxl/minidump/FullOuterJoinZeroMaxCard.mdp",
		"../data/dxl/minidump/FullOuterJoinLeftMultiplyRightMaxCard.mdp",
	};

	return CTestUtils::EresUnittest_RunTests(rgszFileNames, &ulTestCounter,
											 GPOS_ARRAY_SIZE(rgszFileNames));
}

// EOF
