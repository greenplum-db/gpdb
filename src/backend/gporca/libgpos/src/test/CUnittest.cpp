//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2004-2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CUnittest.cpp
//
//	@doc:
//		Driver for unittests
//---------------------------------------------------------------------------

#include <stddef.h>


#include "gpos/base.h"
#include "gpos/common/CAutoTimer.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CTask.h"
#include "gpos/task/CWorker.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;


// threshold of oversized memory pool
#define GPOS_OVERSIZED_POOL_SIZE 500 * 1024 * 1024

// initialize static members
CUnittest *CUnittest::m_rgut = NULL;
ULONG CUnittest::m_ulTests = 0;
ULONG CUnittest::m_ulNested = 0;
void (*CUnittest::m_pfConfig)() = NULL;
void (*CUnittest::m_pfCleanup)() = NULL;


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::CUnittest
//
//	@doc:
//		Constructor for exception-free test
//
//---------------------------------------------------------------------------
CUnittest::CUnittest(const CHAR *szTitle, ETestType ett,
					 GPOS_RESULT (*pfunc)(void))
	: m_szTitle(szTitle),
	  m_ett(ett),
	  m_pfunc(pfunc),
	  m_pfuncSubtest(NULL),
	  m_ulSubtest(0),
	  m_fExcep(false),
	  m_ulMajor(CException::ExmaInvalid),
	  m_ulMinor(CException::ExmiInvalid)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::CUnittest
//
//	@doc:
//		Constructor for test which are expected to throw an exception
//
//---------------------------------------------------------------------------
CUnittest::CUnittest(const CHAR *szTitle, ETestType ett,
					 GPOS_RESULT (*pfunc)(void), ULONG major, ULONG minor)
	: m_szTitle(szTitle),
	  m_ett(ett),
	  m_pfunc(pfunc),
	  m_pfuncSubtest(NULL),
	  m_ulSubtest(0),
	  m_fExcep(true),
	  m_ulMajor(major),
	  m_ulMinor(minor)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CUnittest::CUnittest
//
//	@doc:
//		Constructor for subtest identified by ULONG id
//
//---------------------------------------------------------------------------
CUnittest::CUnittest(const CHAR *szTitle, ETestType ett,
					 GPOS_RESULT (*pfuncSubtest)(ULONG), ULONG ulSubtest)
	: m_szTitle(szTitle),
	  m_ett(ett),
	  m_pfunc(NULL),
	  m_pfuncSubtest(pfuncSubtest),
	  m_ulSubtest(ulSubtest),
	  m_fExcep(false),
	  m_ulMajor(CException::ExmaInvalid),
	  m_ulMinor(CException::ExmiInvalid)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::CUnittest
//
//	@doc:
//		Copy constructor
//
//---------------------------------------------------------------------------
CUnittest::CUnittest(const CUnittest &ut)


	= default;



//---------------------------------------------------------------------------
//	@function:
//		CUnittest::FThrows
//
//	@doc:
//		Is test expected to throw?
//
//---------------------------------------------------------------------------
BOOL
CUnittest::FThrows() const
{
	return m_fExcep;
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::Equals
//
//	@doc:
//		Is given string equal to title of test?
//
//---------------------------------------------------------------------------
BOOL
CUnittest::Equals(CHAR *sz) const
{
	return 0 == clib::Strcmp(sz, m_szTitle);
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::FThrows
//
//	@doc:
//		Is test expected to throw given exception?
//
//---------------------------------------------------------------------------
BOOL
CUnittest::FThrows(ULONG major, ULONG minor) const
{
	return (m_ulMajor == major && m_ulMinor == minor);
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::EresExecTest
//
//	@doc:
//		Execute a single test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CUnittest::EresExecTest(const CUnittest &ut)
{
	GPOS_RESULT eres = GPOS_FAILED;

	CErrorHandlerStandard errhdl;
	GPOS_TRY_HDL(&errhdl)
	{
		// reset cancellation flag
		CTask::Self()->ResetCancel();

		eres = ut.m_pfunc != NULL ? ut.m_pfunc()
								  : ut.m_pfuncSubtest(ut.m_ulSubtest);

		// check if this was expected to throw
		if (ut.FThrows())
		{
			// should have thrown
			return GPOS_FAILED;
		}

		GPOS_CHECK_ABORT;

		return eres;
	}
	GPOS_CATCH_EX(ex)
	{
		// if time slice was exceeded, mark test as failed
		if (GPOS_MATCH_EX(ex, CException::ExmaSystem,
						  CException::ExmiAbortTimeout))
		{
			GPOS_RESET_EX;
			return GPOS_FAILED;
		}

		// check if exception was expected
		if (ut.FThrows(ex.Major(), ex.Minor()))
		{
			GPOS_RESET_EX;
			return GPOS_OK;
		}

		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	return GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::EresExecute
//
//	@doc:
//		Execute a set of test given as an array of CUnittest objects
//
//---------------------------------------------------------------------------
GPOS_RESULT
CUnittest::EresExecute(const CUnittest *rgut, const ULONG cSize)
{
	GPOS_RESULT eres = GPOS_OK;

	for (ULONG i = 0; i < cSize; i++)
	{
		GPOS_RESULT eresPart = GPOS_FAILED;
		const CUnittest &ut = rgut[i];

		{  // scope for timer
			CAutoTimer timer(ut.m_szTitle, true /*fPrint*/);
			eresPart = EresExecTest(ut);
		}

		GPOS_TRACE_FORMAT("Unittest %s...%s.", ut.m_szTitle,
						  (GPOS_OK == eresPart ? "OK" : "*** FAILED ***"));

#ifdef GPOS_DEBUG
		{
			CAutoMemoryPool amp;
			CMemoryPoolManager::GetMemoryPoolMgr()->PrintOverSizedPools(
				amp.Pmp(), GPOS_OVERSIZED_POOL_SIZE);
		}
#endif	// GPOS_DEBUG

		// invalidate result summary if any part fails
		if (GPOS_OK != eresPart)
		{
			eres = GPOS_FAILED;
		}
	}

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::UlFindTest
//
//	@doc:
//		Find a test with given attributes and add it the list;
//		If no name passed (NULL), match only on attribute;
//
//---------------------------------------------------------------------------
void
CUnittest::FindTest(CBitVector &bv, ETestType ett, CHAR *szTestName)
{
	for (ULONG i = 0; i < CUnittest::m_ulTests; i++)
	{
		CUnittest &ut = CUnittest::m_rgut[i];

		if ((ut.Ett() == ett &&
			 (NULL == szTestName || ut.Equals(szTestName))) ||
			(NULL != szTestName && ut.Equals(szTestName)))
		{
			(void) bv.ExchangeSet(i);
		}
	}

	if (bv.IsEmpty())
	{
		GPOS_TRACE_FORMAT("'%s' is not a valid test case.", szTestName);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::SetTraceFlag
//
//	@doc:
//		Parse and set a trace flag
//
//---------------------------------------------------------------------------
void
CUnittest::SetTraceFlag(const CHAR *szTrace)
{
	CHAR *pcEnd = NULL;
	LINT lTrace = clib::Strtol(szTrace, &pcEnd, 0 /*iBase*/);

	GPOS_SET_TRACE((ULONG) lTrace);
}

// Parse plan id
ULLONG
CUnittest::UllParsePlanId(const CHAR *szPlanId)
{
	CHAR *pcEnd = NULL;
	LINT ullPlanId = clib::Strtol(szPlanId, &pcEnd, 0 /*iBase*/);
	return ullPlanId;
}

//---------------------------------------------------------------------------
//	@function:
//		CUnittest::EresExecute
//
//	@doc:
//		Execute requested unittests
//
//---------------------------------------------------------------------------
ULONG
CUnittest::Driver(CBitVector *pbv)
{
	CAutoConfig ac(m_pfConfig, m_pfCleanup, m_ulNested);
	ULONG ulOk = 0;

	// scope of timer
	{
		gpos::CAutoTimer timer("total test run time", true /*fPrint*/);

		for (ULONG i = 0; i < CUnittest::m_ulTests; i++)
		{
			if (pbv->Get(i))
			{
				CUnittest &ut = CUnittest::m_rgut[i];
				GPOS_RESULT eres = EresExecute(&ut, 1 /*size*/);
				GPOS_ASSERT((GPOS_OK == eres || GPOS_FAILED == eres) &&
							"Unexpected result from unittest");

				if (GPOS_OK == eres)
				{
					++ulOk;
				}

#ifdef GPOS_DEBUG
				{
					CAutoMemoryPool amp;
					CMemoryPoolManager::GetMemoryPoolMgr()->PrintOverSizedPools(
						amp.Pmp(), GPOS_OVERSIZED_POOL_SIZE);
				}
#endif	// GPOS_DEBUG
			}
		}
	}

	GPOS_TRACE_FORMAT("Tests succeeded: %d", ulOk);
	GPOS_TRACE_FORMAT("Tests failed:    %d", pbv->CountSetBits() - ulOk);

	return pbv->CountSetBits() - ulOk;
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::EresExecute
//
//	@doc:
//		Execute unittests by parsing input arguments
//
//---------------------------------------------------------------------------
ULONG
CUnittest::Driver(CMainArgs *pma)
{
	CBitVector bv(ITask::Self()->Pmp(), CUnittest::UlTests());

	CHAR ch = '\0';
	while (pma->Getopt(&ch))
	{
		CHAR *szTestName = NULL;

		switch (ch)
		{
			case 'U':
				szTestName = optarg;
				// fallthru
			case 'u':
				FindTest(bv, EttStandard, szTestName);
				break;

			case 'x':
				FindTest(bv, EttExtended, NULL /*szTestName*/);
				break;

			case 'T':
				SetTraceFlag(optarg);

			default:
				// ignore other parameters
				break;
		}
	}

	return Driver(&bv);
}


//---------------------------------------------------------------------------
//	@function:
//		CUnittest::Init
//
//	@doc:
//		Initialize unittest array
//
//---------------------------------------------------------------------------
void
CUnittest::Init(CUnittest *rgut, ULONG ulUtCnt, void (*pfConfig)(),
				void (*pfCleanup)())
{
	GPOS_ASSERT(0 == m_ulTests &&
				"Unittest array has already been initialized");

	m_rgut = rgut;
	m_ulTests = ulUtCnt;
	m_pfConfig = pfConfig;
	m_pfCleanup = pfCleanup;

	// disable allocations using global new operator
	CMemoryPoolManager::GetMemoryPoolMgr()->DisableGlobalNew();
}

// EOF
