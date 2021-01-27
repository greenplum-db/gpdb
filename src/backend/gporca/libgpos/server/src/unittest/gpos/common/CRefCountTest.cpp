//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CRefCountTest.cpp
//
//	@doc:
//		Tests for CRefCount
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CRefCountTest.h"

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CRefCountTest::EresUnittest
//
//	@doc:
//		Unittest for ref-counting
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRefCountTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CRefCountTest::EresUnittest_CountUpAndDown),
		GPOS_UNITTEST_FUNC(CRefCountTest::EresUnittest_DeletableObjects)

#ifdef GPOS_DEBUG
			,
		GPOS_UNITTEST_FUNC_ASSERT(CRefCountTest::EresUnittest_Stack),
		GPOS_UNITTEST_FUNC_ASSERT(CRefCountTest::EresUnittest_Check)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CRefCountTest::EresUnittest_CountUpAndDown
//
//	@doc:
//		Simple count up and down of ref counted object
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRefCountTest::EresUnittest_CountUpAndDown()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// blank ref count object
	CRefCount *pref = GPOS_NEW(mp) CRefCount;

	// add counts
	for (ULONG i = 0; i < 10; i++)
	{
		pref->AddRef();
	}

	// release all additional refs
	for (ULONG i = 0; i < 10; i++)
	{
		pref->Release();
	}

	// destruct the object
	pref->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CRefCountTest::EresUnittest_DeletableObjects
//
//	@doc:
//		Test deletable/undeletable objects
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRefCountTest::EresUnittest_DeletableObjects()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTraceFlag atfOOM(EtraceSimulateOOM, false);

	CDeletableTest *pdt = GPOS_NEW(mp) CDeletableTest;

	GPOS_TRY
	{
		// trying to release object here throws InvalidDeletion exception
		pdt->Release();
	}
	GPOS_CATCH_EX(ex)
	{
		if (!GPOS_MATCH_EX(ex, CException::ExmaSystem,
						   CException::ExmiInvalidDeletion))
		{
			// unexpected exception -- rethrow it
			GPOS_RETHROW(ex);
		}

		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	pdt->AllowDeletion();

	// now deletion is allowed
	pdt->Release();

	return GPOS_OK;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CRefCountTest::EresUnittest_Stack
//
//	@doc:
//		Put CRefCount on stack -- this must assert in destructor
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRefCountTest::EresUnittest_Stack()
{
	CRefCount ref;

	// does not reach this line
	return GPOS_FAILED;
}



//---------------------------------------------------------------------------
//	@function:
//		CRefCountTest::EresUnittest_Check
//
//	@doc:
//		Call AddRef on a deleted ref count; this test is quite experimental;
//
//---------------------------------------------------------------------------
GPOS_RESULT
CRefCountTest::EresUnittest_Check()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	BYTE *rgb = GPOS_NEW_ARRAY(mp, BYTE, 128);
	CRefCount *pref = (CRefCount *) rgb;

	GPOS_DELETE_ARRAY(rgb);


	// must throw
	pref->AddRef();

	// does not reach this line
	return GPOS_FAILED;
}

#endif	// GPOS_DEBUG

// EOF
