//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CDynamicPtrArrayTest.h
//
//	@doc:
//		Test for CDynamicPtrArray
//---------------------------------------------------------------------------
#ifndef GPOS_CDynamicPtrArrayTest_H
#define GPOS_CDynamicPtrArrayTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CDynamicPtrArrayTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CDynamicPtrArrayTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Ownership();
	static GPOS_RESULT EresUnittest_ArrayAppend();
	static GPOS_RESULT EresUnittest_ArrayAppendExactFit();
	static GPOS_RESULT EresUnittest_PdrgpulSubsequenceIndexes();

	// destructor function for char's
	static void DestroyChar(char *);

};	// class CDynamicPtrArrayTest
}  // namespace gpos

#endif	// !GPOS_CDynamicPtrArrayTest_H

// EOF
