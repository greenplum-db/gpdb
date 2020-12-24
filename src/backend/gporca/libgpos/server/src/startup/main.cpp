//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2004-2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		main.cpp
//
//	@doc:
//		Startup routines for optimizer
//---------------------------------------------------------------------------

#include "gpos/_api.h"
#include "gpos/types.h"

#include "gpos/common/CMainArgs.h"
#include "gpos/test/CUnittest.h"


// test headers

#include "unittest/gpos/common/CAutoPTest.h"
#include "unittest/gpos/common/CAutoRefTest.h"
#include "unittest/gpos/common/CAutoRgTest.h"
#include "unittest/gpos/common/CBitSetIterTest.h"
#include "unittest/gpos/common/CBitSetTest.h"
#include "unittest/gpos/common/CBitVectorTest.h"
#include "unittest/gpos/common/CDynamicPtrArrayTest.h"
#include "unittest/gpos/common/CEnumSetTest.h"
#include "unittest/gpos/common/CDoubleTest.h"
#include "unittest/gpos/common/CHashMapTest.h"
#include "unittest/gpos/common/CHashMapIterTest.h"
#include "unittest/gpos/common/CHashSetTest.h"
#include "unittest/gpos/common/CHashSetIterTest.h"
#include "unittest/gpos/common/CListTest.h"
#include "unittest/gpos/common/CRefCountTest.h"
#include "unittest/gpos/common/CStackTest.h"
#include "unittest/gpos/common/CSyncHashtableTest.h"
#include "unittest/gpos/common/CSyncListTest.h"

#include "unittest/gpos/error/CErrorHandlerTest.h"
#include "unittest/gpos/error/CExceptionTest.h"
#include "unittest/gpos/error/CLoggerTest.h"
#include "unittest/gpos/error/CMessageTest.h"
#include "unittest/gpos/error/CMessageTableTest.h"
#include "unittest/gpos/error/CMessageRepositoryTest.h"
#include "unittest/gpos/error/CMiniDumperTest.h"

#include "unittest/gpos/io/COstreamBasicTest.h"
#include "unittest/gpos/io/COstreamStringTest.h"
#include "unittest/gpos/io/CFileTest.h"

#include "unittest/gpos/memory/CMemoryPoolBasicTest.h"
#include "unittest/gpos/memory/CCacheTest.h"

#include "unittest/gpos/string/CStringTest.h"
#include "unittest/gpos/string/CWStringTest.h"

#include "unittest/gpos/task/CTaskLocalStorageTest.h"

#include "unittest/gpos/test/CUnittestTest.h"


using namespace gpos;

// static array of all known unittest routines
static gpos::CUnittest rgut[] = {
	// common
	GPOS_UNITTEST_STD(CAutoPTest),
	GPOS_UNITTEST_STD(CAutoRefTest),
	GPOS_UNITTEST_STD(CAutoRgTest),
	GPOS_UNITTEST_STD(CBitSetIterTest),
	GPOS_UNITTEST_STD(CBitSetTest),
	GPOS_UNITTEST_STD(CBitVectorTest),
	GPOS_UNITTEST_STD(CDynamicPtrArrayTest),
	GPOS_UNITTEST_STD(CEnumSetTest),
	GPOS_UNITTEST_STD(CDoubleTest),
	GPOS_UNITTEST_STD(CHashMapTest),
	GPOS_UNITTEST_STD(CHashMapIterTest),
	GPOS_UNITTEST_STD(CHashSetTest),
	GPOS_UNITTEST_STD(CHashSetIterTest),
	GPOS_UNITTEST_STD(CRefCountTest),
	GPOS_UNITTEST_STD(CListTest),
	GPOS_UNITTEST_STD(CStackTest),
	GPOS_UNITTEST_STD(CSyncHashtableTest),
	GPOS_UNITTEST_STD(CSyncListTest),

	// error
	GPOS_UNITTEST_STD(CErrorHandlerTest),
	GPOS_UNITTEST_STD(CExceptionTest),
	GPOS_UNITTEST_STD(CLoggerTest),
	GPOS_UNITTEST_STD(CMessageTest),
	GPOS_UNITTEST_STD(CMessageTableTest),
	GPOS_UNITTEST_STD(CMessageRepositoryTest),
	GPOS_UNITTEST_STD(CMiniDumperTest),

	// io
	GPOS_UNITTEST_STD(COstreamBasicTest),
	GPOS_UNITTEST_STD(COstreamStringTest),
	GPOS_UNITTEST_STD(CFileTest),

	// memory
	GPOS_UNITTEST_STD(CMemoryPoolBasicTest),
	GPOS_UNITTEST_STD(CCacheTest),

	// string
	GPOS_UNITTEST_STD(CWStringTest),
	GPOS_UNITTEST_STD(CStringTest),

	// task
	GPOS_UNITTEST_STD(CTaskLocalStorageTest),

	// test
	GPOS_UNITTEST_STD_SUBTEST(CUnittestTest, 0),
	GPOS_UNITTEST_STD_SUBTEST(CUnittestTest, 1),
	GPOS_UNITTEST_STD_SUBTEST(CUnittestTest, 2),
};

// static variable counting the number of failed tests; PvExec overwrites with
// the actual count of failed tests
static ULONG tests_failed = 0;

//---------------------------------------------------------------------------
//	@function:
//		PvExec
//
//	@doc:
//		Function driving execution.
//
//---------------------------------------------------------------------------
static void *
PvExec(void *pv)
{
	CMainArgs *pma = (CMainArgs *) pv;
	tests_failed = CUnittest::Driver(pma);

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		main
//
//	@doc:
//		Entry point for stand-alone optimizer binary; ignore arguments for the
//		time being
//
//---------------------------------------------------------------------------
INT
main(INT iArgs, const CHAR **rgszArgs)
{
	// setup args for unittest params
	CMainArgs ma(iArgs, rgszArgs, "cuU:xT:");

	struct gpos_init_params init_params = {NULL};
	gpos_init(&init_params);

	GPOS_ASSERT(iArgs >= 0);

	// initialize unittest framework
	CUnittest::Init(rgut, GPOS_ARRAY_SIZE(rgut), NULL, NULL);

	gpos_exec_params params;
	params.func = PvExec;
	params.arg = &ma;
	params.result = NULL;
	params.stack_start = &params;
	params.error_buffer = NULL;
	params.error_buffer_size = -1;
	params.abort_requested = NULL;

	if (gpos_exec(&params) || (tests_failed != 0))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


// EOF
