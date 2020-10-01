//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CListTest.h
//
//	@doc:
//		Tests for CList
//---------------------------------------------------------------------------
#ifndef GPOS_CListTest_H
#define GPOS_CListTest_H

#include "gpos/types.h"
#include "gpos/common/CList.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CListTest
//
//	@doc:
//		Wrapper class for CList template to avoid compiler confusion regarding
//		instantiation with sample parameters
//
//---------------------------------------------------------------------------
class CListTest
{
public:
	//---------------------------------------------------------------------------
	//	@class:
	//		SElem
	//
	//	@doc:
	//		Local class for list experiment;
	//
	//---------------------------------------------------------------------------
	struct SElem
	{
	public:
		// generic link for primary list
		SLink m_linkFwd;

		// ..for secondary list
		SLink m_linkBwd;

	};	// struct SElem

	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();
	static GPOS_RESULT EresUnittest_Navigate();
	static GPOS_RESULT EresUnittest_Cursor();

};	// class CListTest
}  // namespace gpos


#endif	// !GPOS_CListTest_H

// EOF
