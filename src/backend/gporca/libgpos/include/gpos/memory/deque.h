//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2024 VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef GPOS_deque_H
#define GPOS_deque_H

#include <deque>

#include "gpos/memory/MemoryPoolAllocator.h"

namespace gpos
{
template <class T>
using deque = std::deque<T, MemoryPoolAllocator<T>>;
}
#endif	//GPOS_deque_H
