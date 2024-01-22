//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CDynamicPtrArrayTest.cpp
//
//	@doc:
//		Test for CDynamicPtrArray
//---------------------------------------------------------------------------

#include "unittest/gpos/common/CDynamicPtrArrayTest.h"

#include <string.h>

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/string/CWStringConst.h"
#include "gpos/test/CUnittest.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest
//
//	@doc:
//		Unittest for ref-counting
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CDynamicPtrArrayTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CDynamicPtrArrayTest::EresUnittest_Ownership),
		GPOS_UNITTEST_FUNC(CDynamicPtrArrayTest::EresUnittest_ArrayAppend),
		GPOS_UNITTEST_FUNC(
			CDynamicPtrArrayTest::EresUnittest_ArrayAppendExactFit),
		GPOS_UNITTEST_FUNC(
			CDynamicPtrArrayTest::EresUnittest_PdrgpulSubsequenceIndexes),
		GPOS_UNITTEST_FUNC(CDynamicPtrArrayTest::EresUnittest_Equals),
		GPOS_UNITTEST_FUNC(CDynamicPtrArrayTest::EresUnittest_Sort),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest_Basic
//
//	@doc:
//		Basic array allocation test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test with CHAR array

	CHAR rgsz[][9] = {"abc",  "def", "ghi", "qwe", "wer",
					  "wert", "dfg", "xcv", "zxc"};
	const CHAR *szMissingElem = "missing";

	CDynamicPtrArray<CHAR, CleanupNULL<CHAR>> *pdrg =
		GPOS_NEW(mp) CDynamicPtrArray<CHAR, CleanupNULL<CHAR>>(mp, 2);

	// add elements incl trigger resize of array
	for (ULONG i = 0; i < 9; i++)
	{
		pdrg->Append(rgsz[i]);
		GPOS_ASSERT(i + 1 == pdrg->Size());
		GPOS_ASSERT(rgsz[i] == (*pdrg)[i]);

		// Using CompareUlongPtr as comparison function for Sort/IsSorted
		// requires that the data fits into the size of ULONG.
		GPOS_ASSERT((sizeof(CHAR) * strlen(rgsz[i])) <= sizeof(ULONG));
	}

	// lookup tests
#ifdef GPOS_DEBUG
	const CHAR *szElem =
#endif	// GPOS_DEBUG
		pdrg->Find(rgsz[0]);
	GPOS_ASSERT(nullptr != szElem);

#ifdef GPOS_DEBUG
	ULONG ulPos =
#endif	// GPOS_DEBUG
		pdrg->IndexOf(rgsz[0]);
	GPOS_ASSERT(0 == ulPos);

#ifdef GPOS_DEBUG
	ULONG ulPosMissing =
#endif	// GPOS_DEBUG
		pdrg->IndexOf(szMissingElem);
	GPOS_ASSERT(gpos::ulong_max == ulPosMissing);
	// all elements were inserted in ascending order
	GPOS_ASSERT(pdrg->IsSorted(CompareUlongPtr));

	pdrg->Release();


	// test with ULONG array

	using UlongArray = CDynamicPtrArray<ULONG, CleanupNULL<ULONG>>;
	UlongArray *pdrgULONG = GPOS_NEW(mp) UlongArray(mp, 1);
	ULONG c = 256;

	// add elements incl trigger resize of array
	for (ULONG_PTR ulpK = c; ulpK > 0; ulpK--)
	{
		ULONG *pul = (ULONG *) (ulpK - 1);
		pdrgULONG->Append(pul);
	}

	GPOS_ASSERT(c == pdrgULONG->Size());

	// all elements were inserted in descending order
	GPOS_ASSERT(!pdrgULONG->IsSorted(CompareUlongPtr));

	pdrgULONG->Sort(CompareUlongPtr);
	GPOS_ASSERT(pdrgULONG->IsSorted(CompareUlongPtr));

	// test that all positions got copied and sorted properly
	for (ULONG_PTR ulpJ = 0; ulpJ < c; ulpJ++)
	{
		GPOS_ASSERT((ULONG *) ulpJ == (*pdrgULONG)[(ULONG) ulpJ]);
	}
	pdrgULONG->Release();


	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest_Ownership
//
//	@doc:
//		Basic array test with ownership
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_Ownership()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// test with ULONGs

	using UlongArray = CDynamicPtrArray<ULONG, CleanupDelete<ULONG>>;
	UlongArray *pdrgULONG = GPOS_NEW(mp) UlongArray(mp, 1);

	// add elements incl trigger resize of array
	for (ULONG k = 0; k < 256; k++)
	{
		ULONG *pul = GPOS_NEW(mp) ULONG;
		pdrgULONG->Append(pul);
		GPOS_ASSERT(k + 1 == pdrgULONG->Size());
		GPOS_ASSERT(pul == (*pdrgULONG)[k]);
	}
	pdrgULONG->Release();

	// test with CHAR array

	using CharArray = CDynamicPtrArray<CHAR, CleanupDeleteArray<CHAR>>;
	CharArray *pdrgCHAR = GPOS_NEW(mp) CharArray(mp, 2);

	// add elements incl trigger resize of array
	for (ULONG i = 0; i < 3; i++)
	{
		CHAR *sz = GPOS_NEW_ARRAY(mp, CHAR, 5);
		pdrgCHAR->Append(sz);
		GPOS_ASSERT(i + 1 == pdrgCHAR->Size());
		GPOS_ASSERT(sz == (*pdrgCHAR)[i]);
	}

	pdrgCHAR->Clear();
	GPOS_ASSERT(0 == pdrgCHAR->Size());

	pdrgCHAR->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest_ArrayAppend
//
//	@doc:
//		Appending arrays
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_ArrayAppend()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	using UlongArray = CDynamicPtrArray<ULONG, CleanupNULL<ULONG>>;

	ULONG cVal = 0;

	// array with 1 element
	UlongArray *pdrgULONG1 = GPOS_NEW(mp) UlongArray(mp, 1);
	pdrgULONG1->Append(&cVal);
	GPOS_ASSERT(1 == pdrgULONG1->Size());

	// array with x elements
	ULONG cX = 1000;
	UlongArray *pdrgULONG2 = GPOS_NEW(mp) UlongArray(mp, 1);
	for (ULONG i = 0; i < cX; i++)
	{
		pdrgULONG2->Append(&cX);
	}
	GPOS_ASSERT(cX == pdrgULONG2->Size());

	// add one to another
	pdrgULONG1->AppendArray(pdrgULONG2);
	GPOS_ASSERT(cX + 1 == pdrgULONG1->Size());
	for (ULONG j = 0; j < pdrgULONG2->Size(); j++)
	{
		GPOS_ASSERT((*pdrgULONG1)[j + 1] == (*pdrgULONG2)[j]);
	}

	pdrgULONG1->Release();
	pdrgULONG2->Release();

	return GPOS_OK;
}



//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest_ArrayAppendExactFit
//
//	@doc:
//		Appending arrays when there is enough memory in first
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_ArrayAppendExactFit()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	using UlongArray = CDynamicPtrArray<ULONG, CleanupNULL<ULONG>>;

	ULONG cVal = 0;

	// array with 1 element
	UlongArray *pdrgULONG1 = GPOS_NEW(mp) UlongArray(mp, 10);
	pdrgULONG1->Append(&cVal);
	GPOS_ASSERT(1 == pdrgULONG1->Size());

	// array with x elements
	ULONG cX = 9;
	UlongArray *pdrgULONG2 = GPOS_NEW(mp) UlongArray(mp, 15);
	for (ULONG i = 0; i < cX; i++)
	{
		pdrgULONG2->Append(&cX);
	}
	GPOS_ASSERT(cX == pdrgULONG2->Size());

	// add one to another
	pdrgULONG1->AppendArray(pdrgULONG2);
	GPOS_ASSERT(cX + 1 == pdrgULONG1->Size());
	for (ULONG j = 0; j < pdrgULONG2->Size(); j++)
	{
		GPOS_ASSERT((*pdrgULONG1)[j + 1] == (*pdrgULONG2)[j]);
	}

	UlongArray *pdrgULONG3 = GPOS_NEW(mp) UlongArray(mp, 15);
	pdrgULONG1->AppendArray(pdrgULONG3);
	GPOS_ASSERT(cX + 1 == pdrgULONG1->Size());

	pdrgULONG1->Release();
	pdrgULONG2->Release();
	pdrgULONG3->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CDynamicPtrArrayTest::EresUnittest_PdrgpulSubsequenceIndexes
//
//	@doc:
//		Finding the first occurrences of the elements of the first array
//		in the second one.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_PdrgpulSubsequenceIndexes()
{
	using UlongArray = CDynamicPtrArray<ULONG, CleanupNULL<ULONG>>;

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// the array containing elements to look up
	UlongArray *pdrgULONGLookup = GPOS_NEW(mp) UlongArray(mp);

	// the array containing the target elements that will give the positions
	UlongArray *pdrgULONGTarget = GPOS_NEW(mp) UlongArray(mp);

	ULONG *pul1 = GPOS_NEW(mp) ULONG(10);
	ULONG *pul2 = GPOS_NEW(mp) ULONG(20);
	ULONG *pul3 = GPOS_NEW(mp) ULONG(30);

	pdrgULONGLookup->Append(pul1);
	pdrgULONGLookup->Append(pul2);
	pdrgULONGLookup->Append(pul3);
	pdrgULONGLookup->Append(pul3);

	// since target is empty, there are elements in lookup with no match, so the function
	// should return NULL
	GPOS_ASSERT(nullptr ==
				pdrgULONGTarget->IndexesOfSubsequence(pdrgULONGLookup));

	pdrgULONGTarget->Append(pul1);
	pdrgULONGTarget->Append(pul3);
	pdrgULONGTarget->Append(pul3);
	pdrgULONGTarget->Append(pul3);
	pdrgULONGTarget->Append(pul2);

	ULongPtrArray *pdrgpulIndexes =
		pdrgULONGTarget->IndexesOfSubsequence(pdrgULONGLookup);

	GPOS_ASSERT(nullptr != pdrgpulIndexes);
	GPOS_ASSERT(4 == pdrgpulIndexes->Size());
	GPOS_ASSERT(0 == *(*pdrgpulIndexes)[0]);
	GPOS_ASSERT(4 == *(*pdrgpulIndexes)[1]);
	GPOS_ASSERT(1 == *(*pdrgpulIndexes)[2]);
	GPOS_ASSERT(1 == *(*pdrgpulIndexes)[3]);

	GPOS_DELETE(pul1);
	GPOS_DELETE(pul2);
	GPOS_DELETE(pul3);
	pdrgpulIndexes->Release();
	pdrgULONGTarget->Release();
	pdrgULONGLookup->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//     @function:
//             CDynamicPtrArrayTest::EresUnittest_Equals
//
//     @doc:
//             Testing whether two arrays are equal
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_Equals()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	StringPtrArray *myStringArray = GPOS_NEW(mp) StringPtrArray(mp);
	myStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "a_string"));
	myStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "b_string"));

	StringPtrArray *yourStringArray = GPOS_NEW(mp) StringPtrArray(mp);
	yourStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "a_string"));
	yourStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "b_string"));

	// string array with same elements, in the same order should be equal
	GPOS_ASSERT(myStringArray->Equals(yourStringArray));
	GPOS_ASSERT(yourStringArray->Equals(myStringArray));

	// string array with same elements, in the different order should not be equal
	StringPtrArray *ourStringArray = GPOS_NEW(mp) StringPtrArray(mp);
	ourStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "b_string"));
	ourStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "a_string"));

	GPOS_ASSERT(!myStringArray->Equals(ourStringArray));
	GPOS_ASSERT(!ourStringArray->Equals(myStringArray));

	myStringArray->Release();
	yourStringArray->Release();
	ourStringArray->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//     @function:
//             CDynamicPtrArrayTest::EresUnittest_Sort
//
//     @doc:
//             Testing whether an array can be sorted
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDynamicPtrArrayTest::EresUnittest_Sort()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	StringPtrArray *unsortedStringArray = GPOS_NEW(mp) StringPtrArray(mp);
	unsortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "c_string"));
	unsortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "a_string"));
	unsortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "b_string"));

	unsortedStringArray->Sort(CWStringBase::Compare);

	StringPtrArray *sortedStringArray = GPOS_NEW(mp) StringPtrArray(mp);
	sortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "a_string"));
	sortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "b_string"));
	sortedStringArray->Append(GPOS_NEW(mp) CWStringConst(mp, "c_string"));

	for (ULONG ul = 0; ul < unsortedStringArray->Size(); ul++)
	{
		GPOS_ASSERT(
			(*unsortedStringArray)[ul]->Equals((*sortedStringArray)[ul]));
	}

	unsortedStringArray->Release();
	sortedStringArray->Release();

	return GPOS_OK;
}

// EOF
