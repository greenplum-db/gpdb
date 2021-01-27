//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLMemoryManager.h
//
//	@doc:
//		Memory manager for parsing DXL documents used in the Xerces XML parser.
//		Provides a wrapper around the GPOS CMemoryPool interface.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLMemoryManager_H
#define GPDXL_CDXLMemoryManager_H

#include <iostream>
#include <xercesc/framework/MemoryManager.hpp>
#include <xercesc/util/XercesDefs.hpp>

#include "gpos/base.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CDXLMemoryManager
//
//	@doc:
//		Memory manager for parsing DXL documents used in the Xerces XML parser.
//		Provides a wrapper around the GPOS CMemoryPool interface.
//
//---------------------------------------------------------------------------
class CDXLMemoryManager : public MemoryManager
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// private copy ctor
	CDXLMemoryManager(const CDXLMemoryManager &);

public:
	// ctor
	CDXLMemoryManager(CMemoryPool *mp);

	// MemoryManager interface functions

	// allocates memory
	void *allocate(XMLSize_t  // size
	);

	// deallocates memory
	void deallocate(void *pv);

	// accessor to the underlying memory pool
	CMemoryPool *
	Pmp()
	{
		return m_mp;
	}

	// returns the memory manager responsible for memory allocation
	// during exceptions
	MemoryManager *
	getExceptionMemoryManager()
	{
		return (MemoryManager *) this;
	}
};
}  // namespace gpdxl

#endif	// GPDXL_CDXLMemoryManager_H

// EOF
