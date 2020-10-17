//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLMemoryManagerTest.h
//
//	@doc:
//		Tests the memory manager to be plugged in Xerces parser.
//---------------------------------------------------------------------------


#ifndef GPOPT_CDXLMemoryManagerTest_H
#define GPOPT_CDXLMemoryManagerTest_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CDXLMemoryManager.h"


namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDXLMemoryManagerTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------

class CDXLMemoryManagerTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CDXLMemoryManagerTest
}  // namespace gpdxl

#endif	// !GPOPT_CDXLMemoryManagerTest_H

// EOF
