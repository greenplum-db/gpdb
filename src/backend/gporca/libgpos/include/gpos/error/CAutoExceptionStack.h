//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CAutoExceptionStack.h
//
//	@doc:
//		Auto object for saving and restoring exception stack
//
//	@owner:
//		elhela
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPOS_CAutoExceptionStack_H
#define GPOS_CAutoExceptionStack_H

#include "gpos/base.h"

#include "gpos/common/CStackObject.h"

namespace gpos
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CAutoExceptionStack
	//
	//	@doc:
    //		Auto object for saving and restoring exception stack
	//
	//---------------------------------------------------------------------------
	class CAutoExceptionStack : public CStackObject
	{

		private:

			// address of the global exception stack value
			void **m_global_exception_stack;

		// value of exception stack when object is created
			void *m_exception_stack;

		// address of the global error context stack value
			void **m_global_error_context_stack;

		// value of error context stack when object is created
			void *m_error_context_stack;

			// private copy ctor
			CAutoExceptionStack(const CAutoExceptionStack &);

		public:

			// ctor
			CAutoExceptionStack(void **global_exception_stack, void **global_error_context_stack);

			// dtor
			~CAutoExceptionStack();

			// set the exception stack to the given address
			void SetLocalJmp(void *local_jump);

	}; // class CAutoExceptionStack
}

#endif // !GPOS_CAutoExceptionStack_H

// EOF

