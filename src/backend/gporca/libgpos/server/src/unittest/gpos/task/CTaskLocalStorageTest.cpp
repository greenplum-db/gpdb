//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CTaskLocalStorageTest.cpp
//
//	@doc:
//		Tests for CTaskLocalStorage
//---------------------------------------------------------------------------

#include "unittest/gpos/task/CTaskLocalStorageTest.h"

#include "gpos/common/CSyncHashtableAccessByKey.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/task/CTask.h"
#include "gpos/task/CTaskLocalStorage.h"
#include "gpos/task/CTaskLocalStorageObject.h"
#include "gpos/task/CTraceFlagIter.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CTaskLocalStorageTest::EresUnittest
//
//	@doc:
//		Unittest for TLS
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTaskLocalStorageTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CTaskLocalStorageTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CTaskLocalStorageTest::EresUnittest_TraceFlags),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CTaskLocalStorageTest::EresUnittest_Basics
//
//	@doc:
//		Simple store/retrieve test for TLS
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTaskLocalStorageTest::EresUnittest_Basics()
{
	CTestObject tobj;

	// store object in TLS
	ITask::Self()->GetTls().Store(&tobj);

	// assert identiy when looking it up
	GPOS_ASSERT(&tobj ==
				ITask::Self()->GetTls().Get(CTaskLocalStorage::EtlsidxTest));

	// clean out TLS
	ITask::Self()->GetTls().Remove(&tobj);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CTaskLocalStorageTest::EresUnittest_TraceFlags
//
//	@doc:
//		Test trace flag set, retrieve and iterate
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTaskLocalStorageTest::EresUnittest_TraceFlags()
{
	GPOS_ASSERT(!GPOS_FTRACE(EtraceTest));

	GPOS_SET_TRACE(EtraceTest);

	GPOS_ASSERT(GPOS_FTRACE(EtraceTest));

	// test auto trace flag
	{
		CAutoTraceFlag atf(EtraceTest, false /*value*/);

		GPOS_ASSERT(!GPOS_FTRACE(EtraceTest));
	}
	GPOS_ASSERT(GPOS_FTRACE(EtraceTest));

#ifdef GPOS_DEBUG
	// test trace flag iterator
	CTraceFlagIter tfi;
	BOOL fFound = false;
	while (tfi.Advance())
	{
		GPOS_ASSERT_IMP(!fFound, EtraceTest == tfi.Bit());
		fFound = true;
	}
#endif	// GPOS_DEBUG

	GPOS_ASSERT(GPOS_FTRACE(EtraceTest));
	GPOS_UNSET_TRACE(EtraceTest);
	GPOS_ASSERT(!GPOS_FTRACE(EtraceTest));

	return GPOS_OK;
}
// EOF
