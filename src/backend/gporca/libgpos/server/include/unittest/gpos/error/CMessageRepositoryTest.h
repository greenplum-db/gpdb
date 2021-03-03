//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMessageRepositoryTest.h
//
//	@doc:
//		Test for CMessageTable
//---------------------------------------------------------------------------
#ifndef GPOS_CMessageRepositoryTest_H
#define GPOS_CMessageRepositoryTest_H

#include "gpos/assert.h"
#include "gpos/types.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CMessageRepositoryTest
//
//	@doc:
//		Static unit tests for message table
//
//---------------------------------------------------------------------------
class CMessageRepositoryTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
};
}  // namespace gpos

#endif	// !GPOS_CMessageRepositoryTest_H

// EOF
