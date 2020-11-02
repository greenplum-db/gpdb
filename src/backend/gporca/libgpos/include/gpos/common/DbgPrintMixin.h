//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2020-Present VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef GPDB_DbgPrintMixin_H
#define GPDB_DbgPrintMixin_H

#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CTask.h"
namespace gpos
{
template <class T>
struct DbgPrintMixin
{
#ifdef GPOS_DEBUG
	// debug print for interactive debugging sessions only
	void
	DbgPrint() const __attribute__((used))
	{
		CAutoTrace at(CTask::Self()->Pmp());
		static_cast<const T *>(this)->OsPrint(at.Os());
	}
#endif
};
}  // namespace gpos

#endif
