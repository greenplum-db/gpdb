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

#include "gpopt/utils/CMemoryPoolPalloc.h"
#include "gpopt/utils/CMemoryPoolPallocManager.h"
#include "gpopt/utils/COptTasks.h"

// the following headers are needed to reference optimizer library initializers
#include "gpos/_api.h"
#include "gpos/memory/CMemoryPoolManager.h"

#include "gpopt/gpdbwrappers.h"
#include "gpopt/init.h"
#include "naucrates/exception.h"
#include "naucrates/init.h"

#include "utils/guc.h"
#include "utils/memutils.h"

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
CGPOptimizer::GPOPTOptimizedPlan(
	Query *query,
	bool *
		had_unexpected_failure	// output : set to true if optimizer unexpectedly failed to produce plan
)
{
	SOptContext gpopt_context;
	PlannedStmt *plStmt = NULL;
	GPOS_TRY
	{
		plStmt = COptTasks::GPOPTOptimizedPlan(query, &gpopt_context,
											   had_unexpected_failure);
		// clean up context
		gpopt_context.Free(gpopt_context.epinQuery, gpopt_context.epinPlStmt);
	}
	GPOS_CATCH_EX(ex)
	{
		// clone the error message before context free.
		CHAR *serialized_error_msg =
			gpopt_context.CloneErrorMsg(MessageContext);
		// clean up context
		gpopt_context.Free(gpopt_context.epinQuery, gpopt_context.epinPlStmt);

		// Special handler for a few common user-facing errors. In particular,
		// we want to use the correct error code for these, in case an application
		// tries to do something smart with them. Also, ERRCODE_INTERNAL_ERROR
		// is handled specially in elog.c, and we don't want that for "normal"
		// application errors.
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL,
						  gpdxl::ExmiQuery2DXLNotNullViolation))
		{
			errstart(ERROR, ex.Filename(), ex.Line(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_NOT_NULL_VIOLATION),
					  errmsg("%s", serialized_error_msg));
		}

		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL, gpdxl::ExmiOptimizerError) ||
				 NULL != serialized_error_msg)
		{
			Assert(NULL != serialized_error_msg);
			errstart(ERROR, ex.Filename(), ex.Line(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_INTERNAL_ERROR),
					  errmsg("%s", serialized_error_msg));
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			PG_RE_THROW();
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL,
							   gpdxl::ExmiNoAvailableMemory))
		{
			errstart(ERROR, ex.Filename(), ex.Line(), NULL, TEXTDOMAIN);
			errfinish(errcode(ERRCODE_INTERNAL_ERROR),
					  errmsg("No available memory to allocate string buffer."));
		}
		else if (GPOS_MATCH_EX(ex, gpdxl::ExmaDXL,
							   gpdxl::ExmiInvalidComparisonTypeCode))
		{
			errstart(ERROR, ex.Filename(), ex.Line(), NULL, TEXTDOMAIN);
			errfinish(
				errcode(ERRCODE_INTERNAL_ERROR),
				errmsg(
					"Invalid comparison type code. Valid values are Eq, NEq, LT, LEq, GT, GEq."));
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
CGPOptimizer::SerializeDXLPlan(Query *query)
{
	return COptTasks::Optimize(query);
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
CGPOptimizer::InitGPOPT()
{
	if (optimizer_use_gpdb_allocators)
	{
		CMemoryPoolPallocManager::Init();
	}

	struct gpos_init_params params = {gpdb::IsAbortRequested};

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
CGPOptimizer::TerminateGPOPT()
{
	gpopt_terminate();
	gpdxl_terminate();
	gpos_terminate();
}

//---------------------------------------------------------------------------
//	@function:
//		GPOPTOptimizedPlan
//
//	@doc:
//		Expose GP optimizer API to C files
//
//---------------------------------------------------------------------------
extern "C" {
PlannedStmt *
GPOPTOptimizedPlan(Query *query, bool *had_unexpected_failure)
{
	return CGPOptimizer::GPOPTOptimizedPlan(query, had_unexpected_failure);
}
}

//---------------------------------------------------------------------------
//	@function:
//		SerializeDXLPlan
//
//	@doc:
//		Serialize planned statement to DXL
//
//---------------------------------------------------------------------------
extern "C" {
char *
SerializeDXLPlan(Query *query)
{
	return CGPOptimizer::SerializeDXLPlan(query);
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
extern "C" {
void
InitGPOPT()
{
	GPOS_TRY
	{
		return CGPOptimizer::InitGPOPT();
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			PG_RE_THROW();
		}
	}
	GPOS_CATCH_END;
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
extern "C" {
void
TerminateGPOPT()
{
	GPOS_TRY
	{
		return CGPOptimizer::TerminateGPOPT();
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			PG_RE_THROW();
		}
	}
	GPOS_CATCH_END;
}
}

extern "C" {
bool
InProgressGPOPT()
{
	return gpos::CWorker::Self() != NULL;
}
}

// EOF
