//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CCastTest.h"
#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"


using namespace gpopt;

ULONG CCastTest::m_ulTestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszCastMdpFiles[] = {
	"../data/dxl/minidump/InferPredicatesForLimit.mdp",
	"../data/dxl/minidump/ArrayCoerceExpr.mdp",
	"../data/dxl/minidump/SelectOnCastedCol.mdp",
	"../data/dxl/minidump/JOIN-Pred-Cast-Int4.mdp",
	"../data/dxl/minidump/JOIN-Pred-Cast-Varchar.mdp",
	"../data/dxl/minidump/JOIN-int4-Eq-double.mdp",
	"../data/dxl/minidump/JOIN-cast2text-int4-Eq-cast2text-double.mdp",
	"../data/dxl/minidump/JOIN-int4-Eq-int2.mdp",
	"../data/dxl/minidump/CastOnSubquery.mdp",
	"../data/dxl/minidump/CoerceToDomain.mdp",
	"../data/dxl/minidump/CoerceViaIO.mdp",
	"../data/dxl/minidump/ArrayCoerceCast.mdp",
	"../data/dxl/minidump/SimpleArrayCoerceCast.mdp",
	"../data/dxl/minidump/EstimateJoinRowsForCastPredicates.mdp",
	"../data/dxl/minidump/HashJoinOnRelabeledColumns.mdp",
	"../data/dxl/minidump/Correlation-With-Casting-1.mdp",
	"../data/dxl/minidump/Correlation-With-Casting-2.mdp",
	"../data/dxl/minidump/Date-TimeStamp-HashJoin.mdp",
	"../data/dxl/minidump/TimeStamp-Date-HashJoin.mdp",
};


GPOS_RESULT
CCastTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

// Run all Minidump-based tests with plan matching
GPOS_RESULT
CCastTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(rgszCastMdpFiles, &m_ulTestCounter,
											 GPOS_ARRAY_SIZE(rgszCastMdpFiles));
}

// EOF
