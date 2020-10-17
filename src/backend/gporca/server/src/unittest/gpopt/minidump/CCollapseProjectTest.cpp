//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	@filename:
//		CCollapseProjectTest.cpp
//
//	@doc:
//		Test for optimizing queries with multiple project nodes
//---------------------------------------------------------------------------

#include "unittest/gpopt/minidump/CCollapseProjectTest.h"
#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"


using namespace gpopt;

ULONG CCollapseProjectTest::m_ulCollapseProjectTestCounter =
	0;	// start from first test

// minidump files
const CHAR *rgszCollapseProjectFileNames[] = {
	"../data/dxl/minidump/MultipleSetReturningFunction-1.mdp",	// project list has multiple set returning functions but one of them cannot be collapsed
	"../data/dxl/minidump/MultipleSetReturningFunction-2.mdp",	// project list has multiple set returning functions that can be collapsed
	"../data/dxl/minidump/MultipleSetReturningFunction-3.mdp",	// both child's and parent's project list has collapsible set returning functions, but we should not
	"../data/dxl/minidump/DMLCollapseProject.mdp",
	"../data/dxl/minidump/CollapseCascadeProjects2of2.mdp",
	"../data/dxl/minidump/CollapseCascadeProjects2of3.mdp",
	"../data/dxl/minidump/CannotCollapseCascadeProjects.mdp",
	"../data/dxl/minidump/CollapseProject-SetReturning.mdp",
	"../data/dxl/minidump/CollapseProject-SetReturning-CTE.mdp",

};


//---------------------------------------------------------------------------
//	@function:
//		CCollapseProjectTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCollapseProjectTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
	};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CCollapseProjectTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCollapseProjectTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests(
		rgszCollapseProjectFileNames, &m_ulCollapseProjectTestCounter,
		GPOS_ARRAY_SIZE(rgszCollapseProjectFileNames));
}

// EOF
