//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CWorker.h
//
//	@doc:
//		Abstraction of schedule-able unit, e.g. a pthread etc.
//---------------------------------------------------------------------------
#ifndef GPOS_CWorker_H
#define GPOS_CWorker_H

#include "gpos/base.h"
#include "gpos/common/CList.h"
#include "gpos/common/CStackDescriptor.h"
#include "gpos/common/CTimerUser.h"
#include "gpos/task/CTask.h"
#include "gpos/task/IWorker.h"

namespace gpos
{
class CTask;

//---------------------------------------------------------------------------
//	@class:
//		CWorker
//
//	@doc:
//		Worker abstraction keeps track of resource held by worker; management
//		of control flow such as abort signal etc.
//
//---------------------------------------------------------------------------

class CWorker : public IWorker
{
	friend class CAutoTaskProxy;

private:
	// current task
	CTask *m_task;

	// available stack
	ULONG m_stack_size;

	// start address of current thread's stack
	const ULONG_PTR m_stack_start;

	// execute single task
	void Execute(CTask *task);

	// check for abort request
	void CheckForAbort(const CHAR *file, ULONG line_num) override;

public:
	CWorker(const CWorker &) = delete;

	// ctor
	CWorker(ULONG stack_size, ULONG_PTR stack_start);

	// dtor
	~CWorker() override;

	// stack start accessor
	inline ULONG_PTR
	GetStackStart() const override
	{
		return m_stack_start;
	}

	// stack check
	BOOL CheckStackSize(ULONG request = 0) const override;

	// accessor
	inline CTask *
	GetTask() override
	{
		return m_task;
	}

	// slink for hashtable
	SLink m_link;

	// lookup worker in worker pool manager
	static CWorker *
	Self()
	{
		return dynamic_cast<CWorker *>(IWorker::Self());
	}

	// host system callback function to report abort requests
	static bool (*abort_requested_by_system)(void);

};	// class CWorker
}  // namespace gpos

#endif	// !GPOS_CWorker_H

// EOF
