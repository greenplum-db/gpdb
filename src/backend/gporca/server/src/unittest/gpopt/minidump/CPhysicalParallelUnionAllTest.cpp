//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.


#include "unittest/gpopt/minidump/CPhysicalParallelUnionAllTest.h"

#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "naucrates/traceflags/traceflags.h"

#include "unittest/gpopt/CTestUtils.h"

using namespace gpos;

static ULONG ulCounter = 0;

static const CHAR *rgszFileNames[] = {
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/NoOpMotionUsesOnlyGroupOutputColumns.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/RedundantMotionParallelUnionAll.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelUnionAllWithNoRedistributableColumns.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelUnionAllWithSingleNotRedistributableColumn.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/TwoHashedTables.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelAppend-Select.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelAppend-Insert.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelAppend-ConstTable.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/ParallelUnionAllWithNotEqualNumOfDistrColumns.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/FallBackToSerialAppend.mdp",
	"../data/dxl/minidump/CPhysicalParallelUnionAllTest/RandomDistributedChildrenUnhashableColumns.mdp",
};

namespace gpopt
{
GPOS_RESULT
CPhysicalParallelUnionAllTest::EresUnittest()
{
	BOOL fMatchPlans = true;
	BOOL fTestSpacePruning = true;
	CAutoTraceFlag atfParallelAppend(gpos::EopttraceEnableParallelAppend, true);
	return CTestUtils::EresUnittest_RunTestsWithoutAdditionalTraceFlags(
		rgszFileNames, &ulCounter, GPOS_ARRAY_SIZE(rgszFileNames), fMatchPlans,
		fTestSpacePruning);
}

}  // namespace gpopt
