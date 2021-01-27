//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CStateMachineTest.cpp
//
//	@doc:
//      Test of state machine implmentation;
//
//		Implements functionality of CTestMachine: transitions and names
//		of states and events;
//---------------------------------------------------------------------------

#include "unittest/gpopt/base/CStateMachineTest.h"

#include "gpos/common/CRandom.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/test/CUnittest.h"
#include "gpos/types.h"

#include "gpopt/base/CStateMachine.h"

using namespace gpopt;

// State transition diagram for test machine:
// From every state, event eeX transitions into state esX, e.g. eeOne transitions always to esOne and so on
// All transitions are considered valid.
//
static const CStateMachineTest::EEvents
	rgeevTransitions[CStateMachineTest::esSentinel]
					[CStateMachineTest::esSentinel] = {
						{CStateMachineTest::eeOne, CStateMachineTest::eeTwo,
						 CStateMachineTest::eeSentinel},
						{CStateMachineTest::eeOne, CStateMachineTest::eeTwo,
						 CStateMachineTest::eeSentinel},
						{CStateMachineTest::eeOne, CStateMachineTest::eeTwo,
						 CStateMachineTest::eeSentinel}};

#ifdef GPOS_DEBUG
// names for events
static const WCHAR
	rgwszEvents[CStateMachineTest::eeSentinel][GPOPT_FSM_NAME_LENGTH] = {
		GPOS_WSZ_LIT("event_one"), GPOS_WSZ_LIT("event_two"),
		GPOS_WSZ_LIT("event_three")};

// names for states
static const WCHAR
	rgwszStates[CStateMachineTest::esSentinel][GPOPT_FSM_NAME_LENGTH] = {
		GPOS_WSZ_LIT("state_one"), GPOS_WSZ_LIT("state_two"),
		GPOS_WSZ_LIT("state_three")};
#endif	// GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CStateMachineTest::CTestMachine::CTestMachine
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CStateMachineTest::CTestMachine::CTestMachine()
{
	m_sm.Init(rgeevTransitions
#ifdef GPOS_DEBUG
			  ,
			  rgwszStates, rgwszEvents
#endif	// GPOS_DEBUG
	);
}


//---------------------------------------------------------------------------
//	@function:
//		CStateMachineTest::EresUnittest
//
//	@doc:
//		Unittest for state machine
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStateMachineTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CStateMachineTest::EresUnittest_Basics)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CStateMachineTest::EresUnittest_Basics
//
//	@doc:
//		Tests for random mix of transitions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CStateMachineTest::EresUnittest_Basics()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CTestMachine *ptm = GPOS_NEW(mp) CTestMachine;
	CRandom rand;
	EEvents rgev[] = {eeOne, eeTwo, eeThree};
#ifdef GPOS_DEBUG
	// states are only used for assertions
	EStates rgst[] = {esOne, esTwo, esThree};
#endif	// GPOS_DEBUG

	EStates es;
	// go into state One to start with
	(void) ptm->Psm()->FTransition(eeOne, es);
	GPOS_ASSERT(esOne == es);

	for (ULONG i = 0; i < 100; i++)
	{
		// choose random event
		ULONG ul = rand.Next() % GPOS_ARRAY_SIZE(rgev);

#ifdef GPOS_DEBUG
		BOOL fCheck =
#endif	// GPOS_DEBUG
			ptm->Psm()->FTransition(rgev[ul], es);

		GPOS_ASSERT_IFF(eeThree != rgev[ul], fCheck);
		GPOS_ASSERT_IFF(eeThree != rgev[ul], rgst[ul] == es);
	}

#ifdef GPOS_DEBUG
	CWStringDynamic str(mp);
	COstreamString oss(&str);
	(void) ptm->Psm()->OsHistory(oss);

	// dumping state graph
	(void) ptm->Psm()->OsDiagramToGraphviz(mp, oss,
										   GPOS_WSZ_LIT("CTestMachine"));

	GPOS_TRACE(str.GetBuffer());

	GPOS_ASSERT(!ptm->Psm()->FReachable(mp));
#endif	// GPOS_DEBUG
	GPOS_DELETE(ptm);

	return GPOS_OK;
}


// EOF
