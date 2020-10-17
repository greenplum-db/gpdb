//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CStateMachineTest.h
//
//	@doc:
//		Test for state machines
//---------------------------------------------------------------------------
#ifndef GPOPT_CStateMachineTest_H
#define GPOPT_CStateMachineTest_H

#include "gpopt/base/CStateMachine.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CStateMachineTest
//
//	@doc:
//		Static unit tests for state machines
//
//---------------------------------------------------------------------------
class CStateMachineTest
{
public:
	enum EStates
	{
		esOne = 0,
		esTwo,
		esThree,

		esSentinel
	};

	enum EEvents
	{
		eeOne = 0,
		eeTwo,
		eeThree,

		eeSentinel
	};

	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basics();

	//---------------------------------------------------------------------------
	//	@class:
	//		CTestMachine
	//
	//	@doc:
	//		The tested state machine definition;
	//		Implements a 3 states/3 events FSM with simple transition graph;
	//
	//---------------------------------------------------------------------------
	class CTestMachine
	{
	private:
		// shorthand for state machine
		typedef CStateMachine<EStates, esSentinel, EEvents, eeSentinel> SM;

		// state machine
		SM m_sm;

	public:
		// ctor
		CTestMachine();

		// dtor
		~CTestMachine(){};

		// state machine accessor
		SM *
		Psm()
		{
			return &m_sm;
		}
	};

};	// class CStateMachineTest
}  // namespace gpopt

#endif	// !GPOPT_CStateMachineTest_H


// EOF
