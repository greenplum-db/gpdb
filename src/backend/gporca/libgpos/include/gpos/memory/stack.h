//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2024 VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef GPOS_stack_H
#define GPOS_stack_H

#include <stack>

#include "gpos/memory/MemoryPoolAllocator.h"
#include "gpos/memory/deque.h"

namespace gpos
{
template <class T>
using stack = std::stack<T, gpos::deque<T>>;
}
#endif	//GPOS_stack_H
