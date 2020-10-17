//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLMemoryManagerTest.cpp
//
//	@doc:
//		Tests the memory manager to be plugged in Xerces.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/dxl/CDXLMemoryManagerTest.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLMemoryManagerTest::EresUnittest
//
//	@doc:
//
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDXLMemoryManagerTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CDXLMemoryManagerTest::EresUnittest_Basic)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLMemoryManagerTest::EresUnittest_Basic
//
//	@doc:
//		Test for allocating and deallocating memory, as required by the Xerces parser
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDXLMemoryManagerTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CDXLMemoryManager *dxl_memory_manager = GPOS_NEW(mp) CDXLMemoryManager(mp);
	void *pvMemory = dxl_memory_manager->allocate(5);

	GPOS_ASSERT(NULL != pvMemory);

	dxl_memory_manager->deallocate(pvMemory);

	// cleanup
	GPOS_DELETE(dxl_memory_manager);
	// pvMemory is deallocated through the memory manager, otherwise the test will throw
	// with a memory leak

	return GPOS_OK;
}



// EOF
