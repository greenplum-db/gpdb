//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CErrorHandler.h
//
//	@doc:
//		Error handler base class;
//---------------------------------------------------------------------------
#ifndef GPOS_CErrorHandler_H
#define GPOS_CErrorHandler_H

#include "gpos/assert.h"
#include "gpos/error/CException.h"
#include "gpos/types.h"

namespace gpos
{
// fwd declarations
class CMemoryPool;

//---------------------------------------------------------------------------
//	@class:
//		CErrorHandler
//
//	@doc:
//		Error handler to be installed inside a worker;
//
//---------------------------------------------------------------------------
class CErrorHandler
{
private:
public:
	CErrorHandler(const CErrorHandler &) = delete;

	// ctor
	CErrorHandler() = default;

	// dtor
	virtual ~CErrorHandler() = default;

	// process error
	virtual void Process(CException exception) = 0;

};	// class CErrorHandler
}  // namespace gpos

#endif	// !GPOS_CErrorHandler_H

// EOF
