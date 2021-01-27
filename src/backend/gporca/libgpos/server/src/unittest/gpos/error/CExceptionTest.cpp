//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CExceptionTest.cpp
//
//	@doc:
//		Tests for CException
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CExceptionTest.h"

#include "gpos/assert.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/error/CException.h"
#include "gpos/task/CWorkerPoolManager.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest
//
//	@doc:
//		Function for raising assert exceptions; again, encapsulated in a function
//		to facilitate debugging
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CExceptionTest::EresUnittest_BasicThrow),
		GPOS_UNITTEST_FUNC_THROW(CExceptionTest::EresUnittest_StackOverflow,
								 CException::ExmaSystem,
								 CException::ExmiOutOfStack),

		GPOS_UNITTEST_FUNC_THROW(CExceptionTest::EresUnittest_AdditionOverflow,
								 CException::ExmaSystem,
								 CException::ExmiOverflow),

		GPOS_UNITTEST_FUNC_THROW(
			CExceptionTest::EresUnittest_MultiplicationOverflow,
			CException::ExmaSystem, CException::ExmiOverflow),

#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC_THROW(CExceptionTest::EresUnittest_BasicRethrow,
								 CException::ExmaSystem, CException::ExmiOOM),
		GPOS_UNITTEST_FUNC_ASSERT(CExceptionTest::EresUnittest_Assert),
		GPOS_UNITTEST_FUNC_ASSERT(CExceptionTest::EresUnittest_AssertImp),
		GPOS_UNITTEST_FUNC_ASSERT(CExceptionTest::EresUnittest_AssertIffLHS),
		GPOS_UNITTEST_FUNC_ASSERT(CExceptionTest::EresUnittest_AssertIffRHS)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_BasicThrow
//
//	@doc:
//		Basic raising of exception and catching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_BasicThrow()
{
	GPOS_TRY
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiOOM);
		return GPOS_FAILED;
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			GPOS_RESET_EX;
			return GPOS_OK;
		}
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}

//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_StackOverflow
//
//	@doc:
//		Test stack overflow
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_StackOverflow()
{
	// character data to bloat the stack frame somewhat
	CHAR szTestData[5][1024] = {"5 KB data", "to bloat", "the", "stack frame"};

	// stack checker will throw after a few recursions
	IWorker::Self()->CheckStackSize();

	// infinite recursion
	CExceptionTest::EresUnittest_StackOverflow();

	GPOS_ASSERT(!"Must not return from recursion");
	GPOS_TRACE_FORMAT("%s", szTestData[0]);

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_AdditionOverflow
//
//	@doc:
//		Test addition overflow
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_AdditionOverflow()
{
	GPOS_TRY
	{
		// additions that must pass
		(void) gpos::Add(gpos::ullong_max - 2, 1);
		(void) gpos::Add(gpos::ullong_max, 0);
		(void) gpos::Add(gpos::ullong_max - 2, 2);
	}
	GPOS_CATCH_EX(ex)
	{
		// no exception is expected here
		return GPOS_FAILED;
	}
	GPOS_CATCH_END;

	// addition that throws overflow exception
	(void) gpos::Add(gpos::ullong_max, 1);

	GPOS_ASSERT(!"Must not add numbers successfully");

	return GPOS_FAILED;
}

//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_MultiplicationOverflow
//
//	@doc:
//		Test multiplication overflow
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_MultiplicationOverflow()
{
	GPOS_TRY
	{
		// multiplications that must pass
		(void) gpos::Multiply(gpos::ullong_max, 1);
		(void) gpos::Multiply(gpos::ullong_max, 0);
		(void) gpos::Multiply(gpos::ullong_max / 2, 2);
		(void) gpos::Multiply(gpos::ullong_max / 2 - 1, 2);
	}
	GPOS_CATCH_EX(ex)
	{
		// no exception is expected here
		return GPOS_FAILED;
	}
	GPOS_CATCH_END;

	// multiplication that throws overflow exception
	(void) gpos::Multiply(gpos::ullong_max - 4, 2);

	GPOS_ASSERT(!"Must not multiply numbers successfully");

	return GPOS_FAILED;
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_BasicRethrow
//
//	@doc:
//		Basic raising of exception and catching then rethrowing
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_BasicRethrow()
{
	GPOS_TRY
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiOOM);
		return GPOS_FAILED;
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			GPOS_RETHROW(ex);
			return GPOS_FAILED;
		}
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_Assert
//
//	@doc:
//		Fail an assertion
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_Assert()
{
	GPOS_ASSERT(2 * 2 == 5);

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_AssertImp
//
//	@doc:
//		Fail an implication assertion
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_AssertImp()
{
	// valid implications
	GPOS_ASSERT_IMP(true, true);
	GPOS_ASSERT_IMP(false, false);
	GPOS_ASSERT_IMP(false, true);

	// incorrect implication
	GPOS_ASSERT_IMP(true, false);

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_AssertIffLHS
//
//	@doc:
//		Fail an if-and-only-if assertion; LHS false
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_AssertIffLHS()
{
	// valid implications
	GPOS_ASSERT_IFF(true, true);
	GPOS_ASSERT_IFF(false, false);

	// failed assertion
	GPOS_ASSERT_IFF(false, true);

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CExceptionTest::EresUnittest_AssertIffRHS
//
//	@doc:
//		Fail an if-and-only-if assertion; RHS false
//
//---------------------------------------------------------------------------
GPOS_RESULT
CExceptionTest::EresUnittest_AssertIffRHS()
{
	// valid implications
	GPOS_ASSERT_IFF(true, true);
	GPOS_ASSERT_IFF(false, false);

	// failed assertion
	GPOS_ASSERT_IFF(true, false);

	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG

// EOF
