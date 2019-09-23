//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryPoolPallocManager.h
//
//	@doc:
//		MemoryPoolManager implementation that creates
//		CMemoryPoolPalloc memory pools
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMemoryPoolPallocManager_H
#define GPDXL_CMemoryPoolPallocManager_H

#include "gpos/base.h"

#include "gpos/memory/CMemoryPoolManager.h"

namespace gpos
{
	// memory pool manager that uses GPDB memory contexts
	class CMemoryPoolPallocManager : public CMemoryPoolManager
	{
		private:

			// private no copy ctor
			CMemoryPoolPallocManager(const CMemoryPoolPallocManager&);

		public:

			// ctor
			CMemoryPoolPallocManager(CMemoryPool *internal, EMemoryPoolType memory_pool_type);

			// allocate new memorypool
			virtual CMemoryPool *NewMemoryPool();

			// free allocation
			void DeleteImpl(void* ptr, CMemoryPool::EAllocationType eat);

			// get user requested size of allocation
			ULONG UserSizeOfAlloc(const void* ptr);


			static
			GPOS_RESULT Init();
	};
}

#endif // !GPDXL_CMemoryPoolPallocManager_H

// EOF
