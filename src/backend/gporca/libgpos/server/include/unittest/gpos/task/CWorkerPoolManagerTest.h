//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CWorkerPoolManagerTest.h
//
//	@doc:
//		Test for CWorkerPoolManager
//---------------------------------------------------------------------------
#ifndef GPOS_CWorkerPoolManagerTest_H
#define GPOS_CWorkerPoolManagerTest_H

#include "gpos/task/CWorkerPoolManager.h"

#include "unittest/gpos/task/CAutoTaskProxyTest.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CWorkerPoolManagerTest
//
//	@doc:
//		Unit and stress tests for worker pool class
//
//---------------------------------------------------------------------------
class CWorkerPoolManagerTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Performance();
	static GPOS_RESULT EresUnittest_Stress();

	static void Unittest_TestTaskPerformance(ULONG culWrkrCnt, ULONG culIterCnt,
											 void *funcSingle(void *),
											 void *funcRepeated(void *));

	static void Unittest_TestSingleTaskPerformance(CMemoryPool *mp,
												   ULONG culWrkrCnt,
												   ULONG culIterCnt,
												   void *funcRepeated(void *));

	static void Unittest_TestMultiTaskPerformance(CMemoryPool *mp,
												  ULONG culWrkrCnt,
												  ULONG culIterCnt,
												  void *funcSingle(void *));

	static void Unittest_Stress(ULONG culWrkrCnt, ULONG culTskCnt,
								void *func(void *));
	static void *PvUnittest_ShortRepeated(void *);
	static void *PvUnittest_LongRepeated(void *);

};	// CWorkerPoolManagerTest
}  // namespace gpos

#endif	// !GPOS_CWorkerPoolManagerTest_H

// EOF
