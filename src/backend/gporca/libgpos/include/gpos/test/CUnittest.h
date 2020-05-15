//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CUnittest.h
//
//	@doc:
//		Unittest driver
//---------------------------------------------------------------------------
#ifndef GPOS_CUnittest_H
#define GPOS_CUnittest_H

#include "gpos/base.h"
#include "gpos/common/CMainArgs.h"
#include "gpos/common/CStackObject.h"
#include "gpos/task/ITask.h"

// helper macros
#define GPOS_UNITTEST_FUNC(x)			gpos::CUnittest(#x, CUnittest::EttStandard, x)

#define GPOS_UNITTEST_STD(x)			gpos::CUnittest(#x, CUnittest::EttStandard, x::EresUnittest)
#define GPOS_UNITTEST_EXT(x)			gpos::CUnittest(#x, CUnittest::EttExtended, x::EresUnittest)

// helper for subtest identified by ULONG parameter
#define GPOS_UNITTEST_STD_SUBTEST(x, i)		gpos::CUnittest(#x "_" #i, CUnittest::EttStandard, x::EresSubtest, i)

// helpers for test that are expected to fail
#define GPOS_UNITTEST_FUNC_THROW(x, major, minor)	\
										gpos::CUnittest(#x, CUnittest::EttStandard, \
											x, major, minor)
#define GPOS_UNITTEST_FUNC_ASSERT(x)	GPOS_UNITTEST_FUNC_THROW(x, CException::ExmaSystem, CException::ExmiAssert)

#define TEST_ASSERT(x) GPOS_RTL_ASSERT(x)

namespace gpos
{

	class CMemoryPool;
	class CBitVector;
	class CTask;

	//---------------------------------------------------------------------------
	//	@class:
	//		CUnittest
	//
	//	@doc:
	//		carries class name and function pointer to class' unittest
	//		plus info about expected exceptions
	//
	//---------------------------------------------------------------------------
	class CUnittest
	{
		public:
		
			// type of test
			enum ETestType
			{
				EttStandard,
				EttExtended
			};
			
		
		private:
			
			// internal auto class;
			// ensures that calls to configuration and cleanup functions are not missed
			class CAutoConfig : CStackObject
			{
				private:

					// unittest cleanup function; it's called after executing all unittests
					void (*m_pfCleanup)();

					// counter of nested calls to execute function
					ULONG &m_ulNested;

				public:

					// ctor
					CAutoConfig
						(
						void (*pfConfig)(),
						void (*pfCleanup)(),
						ULONG &ulNested
						)
						:
						m_pfCleanup(pfCleanup),
						m_ulNested(ulNested)
					{
						if (NULL != pfConfig && 0 == m_ulNested++)
						{
							pfConfig();
						}
					}

					// dtor
					~CAutoConfig()
					{
						if (NULL != m_pfCleanup && 0 == --m_ulNested)
						{
							m_pfCleanup();
						}
					}

			};

			// name of unittest for display
			const CHAR *m_szTitle;
			
			// test type
			ETestType m_ett;

			// test function
			GPOS_RESULT (*m_pfunc)(void);

			// subtest function
			GPOS_RESULT (*m_pfuncSubtest)(ULONG);
			ULONG m_ulSubtest;

			
			// flag whether to expect the test to throw
			BOOL m_fExcep;

			// expected exception, if any
			ULONG m_ulMajor;
			ULONG m_ulMinor;
			
			// array of unittests to execute
			static CUnittest *m_rgut;

			// number of unittests to execute
			static ULONG m_ulTests;

			// counter of nested calls to execute function
			static ULONG m_ulNested;

			// unittest configuration function; it's called before executing any unittest
			static void (*m_pfConfig)();

			// unittest cleanup function; it's called after executing all unittests
			static void (*m_pfCleanup)();

			// execution of individual UT
			static 
			GPOS_RESULT EresExecTest(const CUnittest &ut);

			// check if exception was injected by simulation
			static
			BOOL FSimulated(CException ex);

			// top-level loop around execution of individual UT;
			// used for exception simulation;
			static 
			GPOS_RESULT EresExecLoop(const CUnittest &ut);
		

		public:

			// ctors
			CUnittest(const CHAR *szTitle, ETestType ett,
					  GPOS_RESULT (*pfunc)(void));
					  
			CUnittest(const CHAR *szTitle, ETestType ett,
					  GPOS_RESULT (*pfunc)(void), ULONG major, ULONG minor);

			CUnittest(const CHAR *szTitle, ETestType ett,
					  GPOS_RESULT (*pfuncSubtest)(ULONG), ULONG ulSubtest);

			// copy ctor
			CUnittest(const CUnittest &ut);
			
			// determine whether this is expected to throw and if so whether the given exception is the right one
			BOOL FThrows() const;
			BOOL FThrows(ULONG major, ULONG minor) const;
			
			// test type
			ETestType Ett() const
			{
				return m_ett;
			}
			
			// check if title equals given string
			BOOL Equals(CHAR *sz) const;
			
			// find test with given attributes and add to list
			static
			void FindTest(CBitVector &bv, ETestType ett, CHAR *szTestName);

			// parse and set a trace flag
			static
			void SetTraceFlag(const CHAR *szTrace);

			// Parse plan id
			static
			ULLONG UllParsePlanId(const CHAR *szPlanId);

			// get number of unittests
			static
			ULONG UlTests()
			{
				return m_ulTests;
			}

			// driver routine for groups of UTs
			static 
			GPOS_RESULT EresExecute(const CUnittest *, const ULONG cSize);
			
			// driver routine; returns the number of failed tests
			// (0 if all specified tests succeeded)
			static
			ULONG Driver(CBitVector *pbv);
			
			// driver routine parsing input arguments; returns the
			// number of failed tests (0 if all specified tests
			// succeeded)
			static
			ULONG Driver(CMainArgs *pma);

			// initialize unittest array
			static
			void Init(CUnittest *rgut, ULONG ulUtCnt, void (*pfConfig)(), void (*pfCleanup)());

	}; // class CUnittest
}

#endif // GPOS_CUnittest_H

// EOF

