//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CAutoTraceFlag.h
//
//	@doc:
//		Auto wrapper to set/reset a traceflag for a scope
//---------------------------------------------------------------------------
#ifndef GPOS_CAutoTraceFlag_H
#define GPOS_CAutoTraceFlag_H

#include "gpos/base.h"
#include "gpos/common/CStackObject.h"
#include "gpos/task/ITask.h"
#include "gpos/task/traceflags.h"


namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CAutoTraceFlag
//
//	@doc:
//		Auto wrapper;
//
//---------------------------------------------------------------------------
class CAutoTraceFlag : public CStackObject
{
private:
	// traceflag id
	ULONG m_trace;

	// original value
	BOOL m_orig;

	// no copy ctor
	CAutoTraceFlag(const CAutoTraceFlag &);

public:
	// ctor
	CAutoTraceFlag(ULONG trace, BOOL orig);

	// dtor
	virtual ~CAutoTraceFlag();

};	// class CAutoTraceFlag

}  // namespace gpos


#endif	// !GPOS_CAutoTraceFlag_H

// EOF
