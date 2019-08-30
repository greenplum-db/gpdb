//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryPoolPalloc.cpp
//
//	@doc:
//		CMemoryPool implementation that uses PostgreSQL memory
//		contexts.
//
//---------------------------------------------------------------------------

extern "C" {
#include "postgres.h"
#include "utils/memutils.h"
}

#include "gpos/memory/CMemoryPool.h"
#include "gpopt/gpdbwrappers.h"

#include "gpopt/utils/CMemoryPoolPalloc.h"

using namespace gpos;

// ctor
CMemoryPoolPalloc::CMemoryPoolPalloc()
	: m_cxt(NULL)
{
	m_cxt = gpdb::GPDBAllocSetContextCreate();
}

void *
CMemoryPoolPalloc::Allocate
	(
	const ULONG bytes,
	const CHAR *,
	const ULONG
	)
{
	return gpdb::GPDBMemoryContextAlloc(m_cxt, bytes);
}

void
CMemoryPoolPalloc::Free
	(
	void *ptr
	)
{
	gpdb::GPDBFree(ptr);
}

// Prepare the memory pool to be deleted
void
CMemoryPoolPalloc::TearDown()
{
	gpdb::GPDBMemoryContextDelete(m_cxt);
}

// Total allocated size including management overheads
ULLONG
CMemoryPoolPalloc::TotalAllocatedSize() const
{
	return MemoryContextGetCurrentSpace(m_cxt);
}

// EOF
