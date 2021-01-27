//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CErrorHandlerTest.cpp
//
//	@doc:
//		Tests for error handler
//---------------------------------------------------------------------------

#include "unittest/gpos/error/CErrorHandlerTest.h"

#include "gpos/base.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/test/CUnittest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CErrorHandlerTest::EresUnittest
//
//	@doc:
//		Function for raising assert exceptions; again, encapsulated in a function
//		to facilitate debugging
//
//---------------------------------------------------------------------------
GPOS_RESULT
CErrorHandlerTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CErrorHandlerTest::EresUnittest_Basic)
#ifdef GPOS_DEBUG
			,
		GPOS_UNITTEST_FUNC_ASSERT(CErrorHandlerTest::EresUnittest_BadRethrow),
		GPOS_UNITTEST_FUNC_ASSERT(CErrorHandlerTest::EresUnittest_BadReset),
		GPOS_UNITTEST_FUNC_ASSERT(CErrorHandlerTest::EresUnittest_Unhandled)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CErrorHandlerTest::EresUnittest_Basic
//
//	@doc:
//		Basic handling of an error
//
//---------------------------------------------------------------------------
GPOS_RESULT
CErrorHandlerTest::EresUnittest_Basic()
{
	CErrorHandlerStandard errhdl;
	GPOS_TRY_HDL(&errhdl)
	{
		// raise an OOM exception
		GPOS_OOM_CHECK(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		// make sure we catch an OOM
		GPOS_ASSERT(
			GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM));

		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	return GPOS_OK;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CErrorHandlerTest::EresUnittest_BadRethrow
//
//	@doc:
//		catch, reset, and attempt rethrow
//
//---------------------------------------------------------------------------
GPOS_RESULT
CErrorHandlerTest::EresUnittest_BadRethrow()
{
	GPOS_TRY
	{
		// raise an OOM exception
		GPOS_OOM_CHECK(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		// make sure we catch an OOM
		GPOS_ASSERT(
			GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM));

		// reset error context -- ignore, don't handle
		GPOS_RESET_EX;

		// this asserts because we've reset already
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}



//---------------------------------------------------------------------------
//	@function:
//		CErrorHandlerTest::EresUnittest_BadReset
//
//	@doc:
//		catch and reset twice
//
//---------------------------------------------------------------------------
GPOS_RESULT
CErrorHandlerTest::EresUnittest_BadReset()
{
	GPOS_TRY
	{
		// raise an OOM exception
		GPOS_OOM_CHECK(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		// make sure we catch an OOM
		GPOS_ASSERT(
			GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM));

		// reset error context
		GPOS_RESET_EX;

		// reset error context again -- this throws
		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CErrorHandlerTest::EresUnittest_Unhandled
//
//	@doc:
//		try to rethrow with a pending error
//
//---------------------------------------------------------------------------
GPOS_RESULT
CErrorHandlerTest::EresUnittest_Unhandled()
{
	GPOS_TRY
	{
		// raise an OOM exception
		GPOS_OOM_CHECK(NULL);
	}
	GPOS_CATCH_EX(ex)
	{
		// make sure we catch an OOM
		GPOS_ASSERT(
			GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM));

		// do not reset or rethrow here...
	}
	GPOS_CATCH_END;

	// try raising another OOM exception -- this must assert
	GPOS_OOM_CHECK(NULL);

	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG

// EOF
