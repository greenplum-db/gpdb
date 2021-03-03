//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CTraceFlagIter.h
//
//	@doc:
//		Trace flag iterator
//---------------------------------------------------------------------------
#ifndef GPOS_CTraceFlagIter_H
#define GPOS_CTraceFlagIter_H

#include "gpos/base.h"
#include "gpos/common/CBitSetIter.h"
#include "gpos/task/CTask.h"


namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CTraceFlagIter
//
//	@doc:
//		Trace flag iterator for the currently executing task
//
//---------------------------------------------------------------------------
class CTraceFlagIter : public CBitSetIter
{
private:
public:
	CTraceFlagIter(const CTraceFlagIter &) = delete;

	// ctor
	CTraceFlagIter() : CBitSetIter(*CTask::Self()->GetTaskCtxt()->m_bitset)
	{
	}

	// dtor
	virtual ~CTraceFlagIter() = default;

};	// class CTraceFlagIter

}  // namespace gpos


#endif	// !GPOS_CTraceFlagIter_H

// EOF
