//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008-2010 Greenplum Inc.
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IMemoryVisitor.h
//
//	@doc:
//      Interface for applying a common operation to all allocated objects
//		inside a memory pool.
//
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOS_IMemoryVisitor_H
#define GPOS_IMemoryVisitor_H

#include "gpos/assert.h"
#include "gpos/types.h"

namespace gpos
{
// prototypes
class CStackDescriptor;

// wrapper for common operation on allocated memory;
// called by memory pools when a walk of the memory is requested;
class IMemoryVisitor
{
private:
	// private copy ctor
	IMemoryVisitor(IMemoryVisitor &);

public:
	// ctor
	IMemoryVisitor()
	{
	}

	// dtor
	virtual ~IMemoryVisitor()
	{
	}

	// executed operation during a walk of objects;
	// file name may be NULL (when debugging is not enabled);
	// line number will be zero in that case;
	// sequence number is a constant in case allocation sequencing is not supported;
	virtual void Visit(void *user_addr, SIZE_T user_size, void *total_addr,
					   SIZE_T total_size, const CHAR *alloc_filename,
					   const ULONG alloc_line, ULLONG alloc_seq_number,
					   CStackDescriptor *desc) = 0;
};
}  // namespace gpos

#endif	// GPOS_IMemoryVisitor_H

// EOF
