//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryPoolPalloc.h
//
//	@doc:
//		CMemoryPool implementation that uses PostgreSQL memory
//		contexts.
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMemoryPoolPalloc_H
#define GPDXL_CMemoryPoolPalloc_H

#include "gpos/base.h"

#include "gpos/memory/CMemoryPool.h"

namespace gpos
{
	// Memory pool that maps to a Postgres MemoryContext.
	class CMemoryPoolPalloc : public CMemoryPool
	{
		private:

			MemoryContext m_cxt;

		public:

			// ctor
			CMemoryPoolPalloc();

			// allocate memory
			void *Allocate
				(
				const ULONG bytes,
				const CHAR *file,
				const ULONG line
				);

			// free memory
			void Free(void *ptr);

			// prepare the memory pool to be deleted
			void TearDown();

			// return total allocated size include management overhead
			ULLONG TotalAllocatedSize() const;

	};
}

#endif // !GPDXL_CMemoryPoolPalloc_H

// EOF
