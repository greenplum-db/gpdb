//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 Greenplum, Inc.
//
//	@filename:
//		CGPOptimizer.cpp
//
//	@doc:
//		Entry point to GP optimizer
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/CGPOptimizer.h"
#include "gpopt/utils/COptTasks.h"
#include "gpopt/gpdbwrappers.h"

// the following headers are needed to reference optimizer library initializers
#include "naucrates/init.h"
#include "gpopt/init.h"
#include "gpos/_api.h"

//---------------------------------------------------------------------------
//	@function:
//		CGPOptimizer::TouchLibraryInitializers
//
//	@doc:
//		Touch library initializers to enforce linker to include them
//
//---------------------------------------------------------------------------
void
CGPOptimizer::TouchLibraryInitializers()
{
	void (*gpos)(gpos_init_params*) = gpos_init;
	void (*dxl)() = gpdxl_init;
	void (*opt)() = gpopt_init;
}


//---------------------------------------------------------------------------
//	@function:
//		CGPOptimizer::PlstmtOptimize
//
//	@doc:
//		Optimize given query using GP optimizer
//
//---------------------------------------------------------------------------
PlannedStmt *
CGPOptimizer::PplstmtOptimize
	(
	Query *pquery,
	bool *pfUnexpectedFailure // output : set to true if optimizer unexpectedly failed to produce plan
	)
{
	return COptTasks::PplstmtOptimize(pquery, pfUnexpectedFailure);
}


//---------------------------------------------------------------------------
//	@function:
//		CGPOptimizer::SzDXL
//
//	@doc:
//		Serialize planned statement into DXL
//
//---------------------------------------------------------------------------
char *
CGPOptimizer::SzDXLPlan
	(
	Query *pquery
	)
{
	return COptTasks::SzOptimize(pquery);
}

//---------------------------------------------------------------------------
//	@function:
//		InitGPOPT()
//
//	@doc:
//		Initialize GPTOPT and dependent libraries
//
//---------------------------------------------------------------------------
void
CGPOptimizer::InitGPOPT (bool use_gpdb_allocators)
{
  // Use GPORCA's default allocators
  struct gpos_init_params params = { NULL, NULL };
  if ( use_gpdb_allocators ) {
    params.alloc = gpdb::GPMalloc;
    params.free = gpdb::GPFree;
  }

  gpos_init(&params);
  gpdxl_init();
  gpopt_init();
}

//---------------------------------------------------------------------------
//	@function:
//		TerminateGPOPT()
//
//	@doc:
//		Terminate GPOPT and dependent libraries
//
//---------------------------------------------------------------------------
void
CGPOptimizer::TerminateGPOPT ()
{
  gpopt_terminate();
  gpdxl_terminate();
  gpos_terminate();
}

//---------------------------------------------------------------------------
//	@function:
//		PplstmtOptimize
//
//	@doc:
//		Expose GP optimizer API to C files
//
//---------------------------------------------------------------------------
extern "C"
{
PlannedStmt *PplstmtOptimize
	(
	Query *pquery,
	bool *pfUnexpectedFailure
	)
{
	return CGPOptimizer::PplstmtOptimize(pquery, pfUnexpectedFailure);
}
}

//---------------------------------------------------------------------------
//	@function:
//		SzDXLPlan
//
//	@doc:
//		Serialize planned statement to DXL
//
//---------------------------------------------------------------------------
extern "C"
{
char *SzDXLPlan
	(
	Query *pquery
	)
{
	return CGPOptimizer::SzDXLPlan(pquery);
}
}

//---------------------------------------------------------------------------
//	@function:
//		InitGPOPT()
//
//	@doc:
//		Initialize GPTOPT and dependent libraries
//
//---------------------------------------------------------------------------
extern "C"
{
void InitGPOPT (bool use_gpdb_allocators)
{
	return CGPOptimizer::InitGPOPT(use_gpdb_allocators);
}
}

//---------------------------------------------------------------------------
//	@function:
//		TerminateGPOPT()
//
//	@doc:
//		Terminate GPOPT and dependent libraries
//
//---------------------------------------------------------------------------
extern "C"
{
void TerminateGPOPT ()
{
	return CGPOptimizer::TerminateGPOPT();
}
}

// EOF
