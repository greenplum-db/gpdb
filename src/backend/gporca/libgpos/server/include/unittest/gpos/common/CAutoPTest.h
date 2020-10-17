//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CAutoPTest.h
//
//	@doc:
//      Unit test for CAutoP
//---------------------------------------------------------------------------
#ifndef GPOS_CAutoPTest_H
#define GPOS_CAutoPTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CAutoPTest
//
//	@doc:
//		Static unit tests for auto pointer
//
//---------------------------------------------------------------------------
class CAutoPTest
{
public:
	class CElem
	{
	public:
		ULONG m_ul;

	};	// class CElem


	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CAutoPTest

}  // namespace gpos

#endif	// !GPOS_CAutoPTest_H

// EOF
