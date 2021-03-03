//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CStackTest.cpp
//
//	@doc:
//		Test for CStack
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CStackTest.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CStack.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CStackTest::EresUnittest
//
//	@doc:
//		Unittest for ref-counting
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStackTest::EresUnittest()
{
	CUnittest rgut[] = {GPOS_UNITTEST_FUNC(CStackTest::EresUnittest_Basic),
						GPOS_UNITTEST_FUNC(CStackTest::EresUnittest_PushPop)
#ifdef GPOS_DEBUG
							,
						GPOS_UNITTEST_FUNC_ASSERT(CStackTest::EresUnittest_Pop)
#endif	// GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CStackTest::EresUnittest_Basic
//
//	@doc:
//		Basic stack test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStackTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test with CHAR array

	CHAR rgsz[][9] = {"abc",  "def", "ghi", "qwe", "wer",
					  "wert", "dfg", "xcv", "zxc"};
	CStack<CHAR> *pstk = GPOS_NEW(mp) CStack<CHAR>(mp, 4);

	// add elements incl trigger resize of array
	for (ULONG i = 0; i < 9; i++)
	{
		pstk->Push(rgsz[i]);
	}

	ULONG idx = 0;
	while (!pstk->IsEmpty())
	{
		GPOS_ASSERT(idx <= 8 && "Index out of range. Problem with IsEmpty.");
#ifdef GPOS_DEBUG
		GPOS_ASSERT(pstk->Pop() == rgsz[8 - idx] && "Incorrect pop value");
#else
		pstk->Pop();
#endif
		idx++;
	}

	GPOS_ASSERT(idx == 9 && "Stack is not empty!");

	GPOS_DELETE(pstk);

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CStackTest::EresUnittest_PushPop
//
//	@doc:
//		Interleaving push and pops
//
//---------------------------------------------------------------------------

GPOS_RESULT
CStackTest::EresUnittest_PushPop()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG rgul[4] = {1, 2, 3, 4};
	CStack<ULONG> *pstk = GPOS_NEW(mp) CStack<ULONG>(mp, 4);

	// scope for auto trace
	{
		CAutoTrace trace(mp);
		IOstream &oss(trace.Os());
		oss << "Pushing " << rgul[0] << " and " << rgul[1] << std::endl;
		pstk->Push(&rgul[0]);  // stack is 1
		pstk->Push(&rgul[1]);  // stack is 1,2

		ULONG ul = *(pstk->Peek());	 // ul should be 2
		oss << "Top of stack is " << ul << std::endl;

		GPOS_ASSERT(ul == rgul[1]);

		oss << "Popping stack" << std::endl;
		ul = *(pstk->Pop());  //ul should be 2
		oss << "Popped element is " << ul << std::endl;
		GPOS_ASSERT(ul == rgul[1]);

		oss << "Pushing " << rgul[2] << std::endl;
		pstk->Push(&rgul[2]);  // stack is 1,3
		ul = *(pstk->Peek());  // ul is 3
		oss << "Top of stack is " << ul << std::endl;
		GPOS_ASSERT(ul == rgul[2]);

		oss << "Popping stack" << std::endl;
		ul = *(pstk->Pop());  // ul is 3
		oss << "Popped element is " << ul << std::endl;
		GPOS_ASSERT(ul == rgul[2]);

		oss << "Popping stack " << std::endl;
		ul = *(pstk->Pop());  // ul is 1
		oss << "Popped element is " << ul << std::endl;
		GPOS_ASSERT(ul == rgul[0]);
	}

	GPOS_DELETE(pstk);

	return GPOS_OK;
}



#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CStackTest::EresUnittest_Pop
//
//	@doc:
//		This test should assert.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStackTest::EresUnittest_Pop()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	ULONG rgsz[] = {1, 2, 3, 4};

	CStack<ULONG> *pstk = GPOS_NEW(mp) CStack<ULONG>(mp, 4);

	CAutoP<CStack<ULONG> > cAP;
	cAP = pstk;

	// add elements incl trigger resize of array
	for (ULONG i = 0; i < 4; i++)
	{
		pstk->Push(&rgsz[i]);
		GPOS_ASSERT((*pstk->Peek()) == 4 - i);
		GPOS_ASSERT(pstk->Pop() == &rgsz[i]);
	}

	// now deliberately pop when the stack is empty
	pstk->Pop();

	return GPOS_FAILED;
}
#endif

// EOF
