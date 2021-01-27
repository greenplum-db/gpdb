//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CDMLTest.cpp
//
//	@doc:
//		Test for optimizing DML queries
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CDMLTest.h"

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

ULONG CDMLTest::m_ulDMLTestCounter = 0;	 // start from first test

// minidump files
const CHAR *rgszDMLFileNames[] = {
	"../data/dxl/minidump/Insert.mdp",
	"../data/dxl/minidump/MultipleUpdateWithJoinOnDistCol.mdp",
	"../data/dxl/minidump/UpdatingNonDistributionColumnFunc.mdp",
	"../data/dxl/minidump/UpdatingMultipleColumn.mdp",
	"../data/dxl/minidump/UpdateWithHashJoin.mdp",
	"../data/dxl/minidump/UpdatingDistributionColumn.mdp",
	"../data/dxl/minidump/UpdatingNonDistColSameTable.mdp",
	"../data/dxl/minidump/InsertRandomDistr.mdp",
	"../data/dxl/minidump/InsertMismatchedDistrubution.mdp",
	"../data/dxl/minidump/InsertMismatchedDistrubution-2.mdp",
	"../data/dxl/minidump/DeleteMismatchedDistribution.mdp",
	"../data/dxl/minidump/UpdateNoDistKeyMismatchedDistribution.mdp",
	"../data/dxl/minidump/UpdateDistKeyMismatchedDistribution.mdp",
	"../data/dxl/minidump/InsertConstTupleRandomDistribution.mdp",
	"../data/dxl/minidump/InsertMasterOnlyTable.mdp",
	"../data/dxl/minidump/InsertMasterOnlyTableConstTuple.mdp",
	"../data/dxl/minidump/InsertSort.mdp",
	"../data/dxl/minidump/InsertSortDistributed2MasterOnly.mdp",
	"../data/dxl/minidump/InsertProjectSort.mdp",
	"../data/dxl/minidump/InsertAssertSort.mdp",
	"../data/dxl/minidump/UpdateRandomDistr.mdp",
	"../data/dxl/minidump/DeleteRandomDistr.mdp",
	"../data/dxl/minidump/InsertConstTuple.mdp",
	"../data/dxl/minidump/InsertConstTupleVolatileFunction.mdp",
	"../data/dxl/minidump/InsertConstTupleVolatileFunctionMOTable.mdp",
	"../data/dxl/minidump/InsertPrimaryKeyFromMOTable.mdp",
	"../data/dxl/minidump/InsertNULLNotNULLConstraint.mdp",
	"../data/dxl/minidump/Insert-Parquet.mdp",
	"../data/dxl/minidump/Insert-Parquet-Partitioned.mdp",
	"../data/dxl/minidump/Insert-Parquet-Partitioned-SortDisabled.mdp",
	"../data/dxl/minidump/Insert-AO.mdp",
	"../data/dxl/minidump/Insert-AO-Partitioned.mdp",
	"../data/dxl/minidump/Insert-AO-Partitioned-SortDisabled.mdp",
	"../data/dxl/minidump/DML-Replicated-Input.mdp",
	"../data/dxl/minidump/InsertWithTriggers.mdp",
	"../data/dxl/minidump/DeleteWithTriggers.mdp",
	"../data/dxl/minidump/UpdateWithTriggers.mdp",
	"../data/dxl/minidump/InsertNotNullCols.mdp",
	"../data/dxl/minidump/InsertCheckConstraint.mdp",
	"../data/dxl/minidump/InsertWithDroppedCol.mdp",
	"../data/dxl/minidump/UpdateCheckConstraint.mdp",
	"../data/dxl/minidump/UpdateDistrKey.mdp",
	"../data/dxl/minidump/UpdateNoCardinalityAssert.mdp",
	"../data/dxl/minidump/SelfUpdate.mdp",
	"../data/dxl/minidump/UpdateWithOids.mdp",
	"../data/dxl/minidump/UpdateUniqueConstraint.mdp",
	"../data/dxl/minidump/UpdateUniqueConstraint-2.mdp",
	"../data/dxl/minidump/UpdateVolatileFunction.mdp",
	"../data/dxl/minidump/UpdatePartTable.mdp",
	"../data/dxl/minidump/UpdateDroppedCols.mdp",
	"../data/dxl/minidump/UpdateCardinalityAssert.mdp",
	"../data/dxl/minidump/UpdateNotNullCols.mdp",
	"../data/dxl/minidump/UpdateZeroRows.mdp",
	"../data/dxl/minidump/InsertNoEnforceConstraints.mdp",
	"../data/dxl/minidump/UpdateNoEnforceConstraints.mdp",
	"../data/dxl/minidump/Insert-With-HJ-CTE-Agg.mdp",
	"../data/dxl/minidump/CTAS-with-Limit.mdp",
	"../data/dxl/minidump/CTAS-With-Global-Local-Agg.mdp",
	"../data/dxl/minidump/Delete-With-Limit-In-Subquery.mdp",
	"../data/dxl/minidump/DML-With-WindowFunc-OuterRef.mdp",
	"../data/dxl/minidump/DML-Filter-With-OuterRef.mdp",
	"../data/dxl/minidump/DML-UnionAll-With-OuterRef.mdp",
	"../data/dxl/minidump/DML-ComputeScalar-With-Outerref.mdp",
	"../data/dxl/minidump/DML-UnionAll-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-With-MasterOnlyTable-1.mdp",
	"../data/dxl/minidump/DML-With-HJ-And-UniversalChild.mdp",
	"../data/dxl/minidump/DML-With-Join-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-With-CorrelatedNLJ-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-Function-With-SQL-Access.mdp",
};

//---------------------------------------------------------------------------
//	@function:
//		CDMLTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDMLTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CDMLTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDMLTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(rgszDMLFileNames,
											 &m_ulDMLTestCounter,
											 GPOS_ARRAY_SIZE(rgszDMLFileNames));
}

// EOF
