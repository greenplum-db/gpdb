//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		CWorker.cpp
//
//	@doc:
//		Worker abstraction, e.g. thread
//---------------------------------------------------------------------------


#include "gpos/task/IWorker.h"

#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/task/CWorkerPoolManager.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		IWorker::Self
//
//	@doc:
//		static function to lookup ones own worker in the pool manager
//
//---------------------------------------------------------------------------
IWorker *
IWorker::Self()
{
	IWorker *worker = NULL;

	if (NULL != CWorkerPoolManager::WorkerPoolManager())
	{
		worker = CWorkerPoolManager::WorkerPoolManager()->Self();
	}

	return worker;
}


//---------------------------------------------------------------------------
//	@function:
//		IWorker::CheckForAbort
//
//	@doc:
//		Check for aborts
//
//---------------------------------------------------------------------------
void
IWorker::CheckAbort(const CHAR *file, ULONG line_num)
{
	IWorker *worker = Self();
	if (NULL != worker)
	{
		worker->CheckForAbort(file, line_num);
	}
}

// EOF
