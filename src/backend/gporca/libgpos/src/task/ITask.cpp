//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		ITask.cpp
//
//	@doc:
//		 Task abstraction
//---------------------------------------------------------------------------


#include "gpos/task/ITask.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		ITask::Self
//
//	@doc:
//		Static function to lookup ones own worker in the pool manager
//
//---------------------------------------------------------------------------
ITask *
ITask::Self()
{
	IWorker *worker = IWorker::Self();
	if (NULL != worker)
	{
		return worker->GetTask();
	}
	return NULL;
}

// EOF
