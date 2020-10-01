//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CAutoRefTest.h
//
//	@doc:
//		Unit Test for CAutoRef
//---------------------------------------------------------------------------
#ifndef GPOS_CAutoRefTest_H
#define GPOS_CAutoRefTest_H

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CRefCount.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CAutoRefTest
//
//	@doc:
//		Static unit tests for auto pointer
//
//---------------------------------------------------------------------------
class CAutoRefTest
{
public:
	class CElem : public CRefCount
	{
	public:
		CElem(ULONG ul) : m_ul(ul)
		{
		}

		ULONG m_ul;

	};	// class CElem


	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

};	// class CAutoRefTest

}  // namespace gpos

#endif	// !GPOS_CAutoRefTest_H

// EOF
