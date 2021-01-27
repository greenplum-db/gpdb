//---------------------------------------------------------------------------
//	Greenplum Database
//  Copyright (c) 2020 VMware, Inc.
//
//	@filename:
//		CRefCount.cpp
//
//	@doc:
//		Implementation of class for ref-counted objects
//---------------------------------------------------------------------------

#include "gpos/common/CRefCount.h"

#include "gpos/error/CAutoTrace.h"
#include "gpos/task/CTask.h"

using namespace gpos;

#ifdef GPOS_DEBUG
// debug print for interactive debugging sessions only
void
CRefCount::DbgPrint() const
{
	CAutoTrace at(CTask::Self()->Pmp());

	OsPrint(at.Os());
}
#endif	// GPOS_DEBUG

// EOF
