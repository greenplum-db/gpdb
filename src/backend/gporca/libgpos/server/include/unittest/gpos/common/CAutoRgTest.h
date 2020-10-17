//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CAutoRgTest.h
//
//	@doc:
//		Test for basic auto range implementation
//---------------------------------------------------------------------------
#ifndef GPOS_CAutoRgTest_H
#define GPOS_CAutoRgTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CAutoRgTest
//
//	@doc:
//		Static unit tests for auto range
//
//---------------------------------------------------------------------------
class CAutoRgTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CAutoRgTest

}  // namespace gpos

#endif	// !GPOS_CAutoRgTest_H

// EOF
