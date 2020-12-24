//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#include "unittest/gpopt/minidump/CPullUpProjectElementTest.h"
#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
using namespace gpos;

GPOS_RESULT
CPullUpProjectElementTest::EresUnittest()
{
	ULONG ulTestCounter = 0;
	const CHAR *rgszFileNames[] = {
		"../data/dxl/minidump/SubqueryNoPullUpTableValueFunction.mdp",
	};

	return CTestUtils::EresUnittest_RunTestsWithoutAdditionalTraceFlags(
		rgszFileNames, &ulTestCounter, GPOS_ARRAY_SIZE(rgszFileNames), true,
		true);
}
}  // namespace gpopt
