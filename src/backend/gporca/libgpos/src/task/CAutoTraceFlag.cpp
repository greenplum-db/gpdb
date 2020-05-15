//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CAutoTraceFlag.cpp
//
//	@doc:
//		Auto object to toggle TF in scope
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CAutoTraceFlag.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CAutoTraceFlag::CAutoTraceFlag
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CAutoTraceFlag::CAutoTraceFlag
	(
	ULONG trace,
	BOOL orig
	)
	:
	m_trace(trace),
	m_orig(false)
{
	GPOS_ASSERT(NULL != ITask::Self());
	m_orig = ITask::Self()->SetTrace(m_trace, orig);
}


//---------------------------------------------------------------------------
//	@function:
//		CAutoTraceFlag::~CAutoTraceFlag
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CAutoTraceFlag::~CAutoTraceFlag()
{
	GPOS_ASSERT(NULL != ITask::Self());
		
	// reset original value
	ITask::Self()->SetTrace(m_trace, m_orig);
}


// EOF
