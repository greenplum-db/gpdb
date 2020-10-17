//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CMemoryPoolBasicTest.cpp
//
//	@doc:
//		Tests for CMemoryPoolBasicTest
//---------------------------------------------------------------------------

#include "gpos/assert.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/common/CAutoTimer.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/error/CException.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/memory/CMemoryVisitorPrint.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/task/CWorkerPoolManager.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/memory/CMemoryPoolBasicTest.h"

#define GPOS_MEM_TEST_ALLOC_SMALL (8)
#define GPOS_MEM_TEST_ALLOC_LARGE (256)

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresUnittest
//
//	@doc:
//		Basic tests for memory management abstraction
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresUnittest()
{
	CUnittest rgut[] = {
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CMemoryPoolBasicTest::EresUnittest_Print),
#endif	// GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CMemoryPoolBasicTest::EresUnittest_TestTracker)};

	CAutoTraceFlag atf(EtraceTestMemoryPools, true /*value*/);

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresUnittest_Print
//
//	@doc:
//		Print memory pools
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresUnittest_Print()
{
	CAutoTraceFlag atfStackTrace(EtracePrintMemoryLeakStackTrace, true);

	const ULONG ulBufferSize = 4096;
	WCHAR wsz[ulBufferSize];
	CWStringStatic str(wsz, GPOS_ARRAY_SIZE(wsz));
	COstreamString os(&str);

	(void) CMemoryPoolManager::GetMemoryPoolMgr()->OsPrint(os);
	GPOS_TRACE(str.GetBuffer());

	return GPOS_OK;
}

#endif	// GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresUnittest_TestTracker
//
//	@doc:
//		Run tests for pool tracking allocations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresUnittest_TestTracker()
{
	return EresTestType();
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresTestType
//
//	@doc:
//		Run tests per pool type
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresTestType()
{
	if (GPOS_OK != EresNewDelete() ||
		GPOS_OK != EresTestExpectedError(EresThrowingCtor, CException::ExmiOOM)

#ifdef GPOS_DEBUG
		|| GPOS_OK != EresTestExpectedError(EresLeak, CException::ExmiAssert) ||
		GPOS_OK !=
			EresTestExpectedError(EresLeakByException, CException::ExmiAssert)
#endif	// GPOS_DEBUG
	)
	{
		return GPOS_FAILED;
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresTestExpectedError
//
//	@doc:
//		Run test that is expected to raise an exception
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresTestExpectedError(GPOS_RESULT (*pfunc)(), ULONG minor)
{
	CErrorHandlerStandard errhdl;
	GPOS_TRY_HDL(&errhdl)
	{
		pfunc();
	}
	GPOS_CATCH_EX(ex)
	{
		if (CException::ExmaSystem == ex.Major() && minor == ex.Minor())
		{
			GPOS_RESET_EX;

			return GPOS_OK;
		}

		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresNewDelete
//
//	@doc:
//		Basic tests for allocation and free-ing
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresNewDelete()
{
	// create memory pool
	CAutoTimer at("NewDelete test", true /*fPrint*/);
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	WCHAR rgwszText[] = GPOS_WSZ_LIT(
		"This is a lengthy test string. "
		"Nothing serious just to demonstrate that "
		"you can allocate using New and free using "
		"delete. That's all. Special characters anybody? "
		"Sure thing: \x07 \xAB \xFF!. End of string.");

	// use overloaded New operator
	WCHAR *wsz = GPOS_NEW_ARRAY(mp, WCHAR, GPOS_ARRAY_SIZE(rgwszText));
	(void) clib::Wmemcpy(wsz, rgwszText, GPOS_ARRAY_SIZE(rgwszText));

#ifdef GPOS_DEBUG

	WCHAR rgBuffer[8 * 1024];
	CWStringStatic str(rgBuffer, GPOS_ARRAY_SIZE(rgBuffer));
	COstreamString os(&str);

	// dump allocations
	if (mp->SupportsLiveObjectWalk())
	{
		CMemoryVisitorPrint movp(os);
		mp->WalkLiveObjects(&movp);
	}
	else
	{
		os << "Memory dump unavailable";
	}
	os << std::endl;

	GPOS_TRACE(str.GetBuffer());

#endif	// GPOS_DEBUG

	GPOS_DELETE_ARRAY(wsz);

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresThrowingCtor
//
//	@doc:
//		Basic tests for exception in constructor
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresThrowingCtor()
{
	CAutoTimer at("ThrowingCtor test", true /*fPrint*/);

	// create memory pool
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	// malicious test class
	class CMyTestClass
	{
	public:
		CMyTestClass()
		{
			// throw in ctor
			GPOS_RAISE(CException::ExmaSystem, CException::ExmiOOM);
		}
	};

	// try instantiating the class
	GPOS_NEW(mp) CMyTestClass();

	// doesn't reach this line
	return GPOS_FAILED;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresLeak
//
//	@doc:
//		Basic tests for leak checking
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresLeak()
{
	CAutoTraceFlag atfDump(EtracePrintMemoryLeakDump, true);
	CAutoTraceFlag atfStackTrace(EtracePrintMemoryLeakStackTrace, true);
	CAutoTimer at("Leak test", true /*fPrint*/);

	// scope for pool
	{
		CAutoMemoryPool amp(CAutoMemoryPool::ElcStrict);
		CMemoryPool *mp = amp.Pmp();

		for (ULONG i = 0; i < 10; i++)
		{
			// use overloaded New operator
			ULONG *rgul = GPOS_NEW_ARRAY(mp, ULONG, 10);
			rgul[2] = 1;

			if (i < 8)
			{
				GPOS_DELETE_ARRAY(rgul);
			}
		}
	}

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::EresLeakByException
//
//	@doc:
//		Basic test for ignored leaks under exception
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMemoryPoolBasicTest::EresLeakByException()
{
	CAutoTraceFlag atfDump(EtracePrintMemoryLeakDump, true);
	CAutoTraceFlag atfStackTrace(EtracePrintMemoryLeakStackTrace, true);
	CAutoTimer at("LeakByException test", true /*fPrint*/);

	// scope for pool
	{
		// create memory pool
		CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
		CMemoryPool *mp = amp.Pmp();

		for (ULONG i = 0; i < 10; i++)
		{
			// use overloaded New operator
			ULONG *rgul = GPOS_NEW_ARRAY(mp, ULONG, 3);
			rgul[2] = 1;
		}

		GPOS_ASSERT(!"Trigger leak with exception");
	}

	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolBasicTest::Size
//
//	@doc:
//		Pick allocation size based on offset
//
//---------------------------------------------------------------------------
ULONG
CMemoryPoolBasicTest::Size(ULONG offset)
{
	if (0 == (offset & 1))
	{
		return GPOS_MEM_TEST_ALLOC_SMALL;
	}
	return GPOS_MEM_TEST_ALLOC_LARGE;
}

// EOF
