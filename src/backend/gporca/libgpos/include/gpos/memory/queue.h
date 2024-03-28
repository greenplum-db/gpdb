//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2024 VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef GPOS_queue_H
#define GPOS_queue_H

#include <queue>

#include "gpos/memory/MemoryPoolAllocator.h"
#include "gpos/memory/deque.h"

namespace gpos
{
template <class T>
using queue = std::queue<T, gpos::deque<T>>;
}
#endif	//GPOS_queue_H
