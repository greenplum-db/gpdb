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

// the following headers are needed to reference optimizer library initializers
#include "naucrates/init.h"
#include "gpopt/init.h"
#include "gpos/_api.h"
#include "gpopt/gpdbwrappers.h"

#include "naucrates/exception.h"
#include "utils/guc.h"

extern MemoryContext MessageContext;

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
	SOptContext octx;
	PlannedStmt* plStmt = NULL;
	GPOS_TRY
	{
		plStmt = COptTasks::PplstmtOptimize(pquery, &octx, pfUnexpectedFailure);
		// clean up context
		octx.Free(octx.epinQuery, octx.epinPlStmt);
	}
	GPOS_CATCH_EX(ex)
	{
		// clone the error message before context free.
		CHAR* szErrorMsg = octx.CloneErrorMsg(MessageContext);
		// clean up context
		octx.Free(octx.epinQuery, octx.epinPlStmt);

		// Special handler for a few common user-facing errors. In particular,
		// we want to use the correct error code for these, in case an application
		// tries to do something smart with them. Also, ERRCODE_INTERNAL_ERROR
		// is handled specially in elog.c, and we don't want that for "normal"
		// application errors.
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLNotNullViolation))
		{
			errstart(ERROR, ex.SzFilename(), ex.UlLine(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_NOT_NULL_VIOLATION),
				  errmsg("%s", szErrorMsg));
		}

		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL, gpdxl::ExmiOptimizerError) ||
			NULL != szErrorMsg)
		{
			Assert(NULL != szErrorMsg);
			errstart(ERROR, ex.SzFilename(), ex.UlLine(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("%s", szErrorMsg));
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			PG_RE_THROW();
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL, gpdxl::ExmiNoAvailableMemory))
		{
			errstart(ERROR, ex.SzFilename(), ex.UlLine(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("No available memory to allocate string buffer."));
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL, gpdxl::ExmiInvalidComparisonTypeCode))
		{
			errstart(ERROR, ex.SzFilename(), ex.UlLine(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("Invalid comparison type code. Valid values are Eq, NEq, LT, LEq, GT, GEq."));
		}
	}
	GPOS_CATCH_END;
	return plStmt;
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
CGPOptimizer::InitGPOPT ()
{
  // Use GPORCA's default allocators
  void *(*gpos_alloc)(size_t) = NULL;
  void (*gpos_free)(void *) = NULL;
  if (optimizer_use_gpdb_allocators)
  {
	gpos_alloc = gpdb::OptimizerAlloc;
	gpos_free = gpdb::OptimizerFree;
  }
  struct gpos_init_params params = {gpos_alloc, gpos_free};
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
void InitGPOPT ()
{
	return CGPOptimizer::InitGPOPT();
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
