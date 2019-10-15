//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryPoolPallocManager.cpp
//
//	@doc:
//		MemoryPoolManager implementation that creates
//		CMemoryPoolPalloc memory pools
//
//---------------------------------------------------------------------------

extern "C" {
#include "postgres.h"

#include "utils/memutils.h"
}

#include "gpopt/utils/CMemoryPoolPalloc.h"
#include "gpopt/utils/CMemoryPoolPallocManager.h"

using namespace gpos;

// ctor
CMemoryPoolPallocManager::CMemoryPoolPallocManager(CMemoryPool *internal, EMemoryPoolType)
	:
	CMemoryPoolManager(internal, EMemoryPoolExternal)
{
}

// create new memory pool
CMemoryPool *
CMemoryPoolPallocManager::NewMemoryPool()
{
	return GPOS_NEW(GetInternalMemoryPool()) CMemoryPoolPalloc();
}

GPOS_RESULT
CMemoryPoolPallocManager::Init()
{
	return CMemoryPoolManager::SetupMemoryPoolManager<CMemoryPoolPallocManager, CMemoryPoolPalloc>();
}
// EOF
