//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2024 VMware, Inc. or its affiliates. All Rights Reserved.
//---------------------------------------------------------------------------
#ifndef GPOS_MemoryPoolAllocator_H
#define GPOS_MemoryPoolAllocator_H

#include "gpos/memory/CMemoryPool.h"

#include "gpopt/base/COptCtxt.h"

namespace gpos
{
template <class T>
class MemoryPoolAllocator
{
	template <class U>
	friend class MemoryPoolAllocator;

	CMemoryPool *mp_;

public:
	using value_type = T;

	/// Implicit conversion from a Memory Pool to an allocator. This allows the
	/// idiomatic construction of an allocator-aware standard containers, for
	/// example:
	/// \code
	///   std::vector<int, gpos::MemoryPoolAllocator<int>> v(mp);
	/// \endcode
	MemoryPoolAllocator(CMemoryPool *mp) noexcept : mp_(mp)
	{
	}

	template <class U>
	explicit MemoryPoolAllocator(const MemoryPoolAllocator<U> &other)
		: mp_(other.mp_)
	{
	}

	value_type *
	allocate(std::size_t n)
	{
		size_t bytes = sizeof(value_type) * n;
		void *addr =
			mp_->NewImpl(bytes, __FILE__, __LINE__, CMemoryPool::EatArray);
		return static_cast<value_type *>(addr);
	}

	void
	deallocate(value_type *p, std::size_t)
	{
		CMemoryPool::DeleteImpl(p, CMemoryPool::EatArray);
	}

	bool
	operator==(const MemoryPoolAllocator &rhs) const
	{
		return mp_ == rhs.mp_;
	}

	bool
	operator!=(const MemoryPoolAllocator &rhs) const
	{
		return !(*this == rhs);
	}
};

}  // namespace gpos
#endif	// GPOS_MemoryPoolAllocator_H
