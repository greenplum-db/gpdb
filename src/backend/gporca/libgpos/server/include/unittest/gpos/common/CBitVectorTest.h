//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CBitVectorTest.h
//
//	@doc:
//		Unit test for CBitVector
//---------------------------------------------------------------------------
#ifndef GPOS_CBitVectorTest_H
#define GPOS_CBitVectorTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CBitVectorTest
//
//	@doc:
//		Static unit tests for bit vector
//
//---------------------------------------------------------------------------
class CBitVectorTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();
	static GPOS_RESULT EresUnittest_SetOps();
	static GPOS_RESULT EresUnittest_Cursor();
	static GPOS_RESULT EresUnittest_Random();

#ifdef GPOS_DEBUG
	static GPOS_RESULT EresUnittest_OutOfBounds();
#endif	// GPOS_DEBUG

};	// class CBitVectorTest
}  // namespace gpos

#endif	// !GPOS_CBitVectorTest_H

// EOF
