//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//
//	@filename:
//		CPointTest.h
//
//	@doc:
//		Testing operations on points used to define histogram buckets
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CPointTest_H
#define GPNAUCRATES_CPointTest_H

#include "gpos/base.h"

namespace gpnaucrates
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPointTest
//
//	@doc:
//		Static unit tests for point
//
//---------------------------------------------------------------------------
class CPointTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();

	// point related tests
	static GPOS_RESULT EresUnittest_CPointInt4();

	static GPOS_RESULT EresUnittest_CPointBool();

};	// class CPointTest
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CPointTest_H


// EOF
