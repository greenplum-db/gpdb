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
