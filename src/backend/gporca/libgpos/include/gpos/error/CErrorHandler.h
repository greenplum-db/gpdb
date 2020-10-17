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

#include "gpos/types.h"
#include "gpos/assert.h"
#include "gpos/error/CException.h"

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
	// private copy ctor
	CErrorHandler(const CErrorHandler &);

public:
	// ctor
	CErrorHandler()
	{
	}

	// dtor
	virtual ~CErrorHandler()
	{
	}

	// process error
	virtual void Process(CException exception) = 0;

};	// class CErrorHandler
}  // namespace gpos

#endif	// !GPOS_CErrorHandler_H

// EOF
