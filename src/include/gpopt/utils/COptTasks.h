//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@filename:
//		COptTasks.h
//
//	@doc:
//		Tasks that will perform optimization and related tasks
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef COptTasks_H
#define COptTasks_H

#include "gpos/error/CException.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/search/CSearchStage.h"



// fwd decl
namespace gpos
{
	class IMemoryPool;
	class CBitSet;
}

namespace gpdxl
{
	class CDXLNode;
}

namespace gpopt
{
	class CExpression;
	class CMDAccessor;
	class CQueryContext;
	class COptimizerConfig;
	class ICostModel;
}

struct PlannedStmt;
struct Query;
struct List;
struct MemoryContextData;

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

// context of optimizer input and output objects
struct SOptContext
{

	// mark which pointer member should NOT be released
	// when calling Free() function
	enum EPin
	{
		epinQueryDXL, // keep m_query_dxl
		epinQuery, 	 // keep m_query
		epinPlanDXL, // keep m_plan_dxl
		epinPlStmt, // keep m_plan_stmt
		epinErrorMsg // keep m_error_msg
	};

	// query object serialized to DXL
	CHAR *m_query_dxl;

	// query object
	Query *m_query;

	// plan object serialized to DXL
	CHAR *m_plan_dxl;

	// plan object
	PlannedStmt *m_plan_stmt;

	// is generating a plan object required ?
	BOOL m_should_generate_plan_stmt;

	// is serializing a plan to DXL required ?
	BOOL m_should_serialize_plan_dxl;

	// did the optimizer fail unexpectedly?
	BOOL m_is_unexpected_failure;

	// buffer for optimizer error messages
	CHAR *m_error_msg;

	// ctor
	SOptContext();

	// If there is an error print as warning and throw exception to abort
	// plan generation
	void HandleError(BOOL *had_unexpected_failure);

	// free all members except input and output pointers
	void Free(EPin input, EPin epinOutput);

	// Clone the error message in given context.
	CHAR* CloneErrorMsg(struct MemoryContextData *context);

	// casting function
	static
	SOptContext *Cast(void *ptr);

}; // struct SOptContext

class COptTasks
{
	private:

		// context of relcache input and output objects
		struct SContextRelcacheToDXL
		{
			// list of object oids to lookup
			List *m_oid_list;

			// comparison type for tasks retrieving scalar comparisons
			ULONG m_cmp_type;

			// if filename is not null, then output will be written to file
			const char *m_filename;

			// if filename is null, then output will be stored here
			char *m_dxl;

			// ctor
			SContextRelcacheToDXL(List *oid_list, ULONG cmp_type, const char *filename);

			// casting function
			static
			SContextRelcacheToDXL *RelcacheConvert(void *ptr);
		};

		// Structure containing the input and output string for a task that evaluates expressions.
		struct SEvalExprContext
		{
			// Serialized DXL of the expression to be evaluated
			char *m_dxl;

			// The result of evaluating the expression
			char *m_dxl_result;

			// casting function
			static
			SEvalExprContext *PevalctxtConvert(void *ptr);
		};

		// context of minidump load and execution
		struct SOptimizeMinidumpContext
		{
			// the name of the file containing the minidump
			char *m_szFileName;

			// the result of optimizing the minidump
			char *m_dxl_result;

			// casting function
			static
			SOptimizeMinidumpContext *Cast(void *ptr);
		};

		// execute a task given the argument
		static
		void Execute ( void *(*func) (void *), void *func_arg);

		// map GPOS log severity level to GPDB, print error and delete the given error buffer
		static
		void LogExceptionMessageAndDelete(CHAR* err_buf, ULONG severity_level=CException::ExsevInvalid);

		// task that does the translation from xml to dxl to planned_stmt
		static
		void* ConvertToPlanStmtFromDXLTask(void *ptr);

		// task that does the translation from query to XML
		static
		void* ConvertToDXLFromQueryTask(void *ptr);

		// dump relcache info for an object into DXL
		static
		void* ConvertToDXLFromMDObjsTask(void *ptr);

		// dump metadata about cast objects from relcache to a string in DXL format
		static
		void *ConvertToDXLFromMDCast(void *ptr);
		
		// dump metadata about scalar comparison objects from relcache to a string in DXL format
		static
		void *ConvertToDXLFromMDScalarCmp(void *ptr);
		
		// dump relstats info for an object into DXL
		static
		void* ConvertToDXLFromRelStatsTask(void *ptr);

		// evaluates an expression given as a serialized DXL string and returns the serialized DXL result
		static
		void* EvalExprFromDXLTask(void *ptr);

		// create optimizer configuration object
		static
		COptimizerConfig *CreateOptimizerConfig(IMemoryPool *mp, ICostModel *cost_model);

		// optimize a query to a physical DXL
		static
		void* OptimizeTask(void *ptr);

		// optimize the query in a minidump and return resulting plan in DXL format
		static
		void* OptimizeMinidumpTask(void *ptr);

		// translate a DXL tree into a planned statement
		static
		PlannedStmt *ConvertToPlanStmtFromDXL(IMemoryPool *mp, CMDAccessor *md_accessor, const CDXLNode *dxlnode, bool can_set_tag);

		// load search strategy from given path
		static
		CSearchStageArray *LoadSearchStrategy(IMemoryPool *mp, char *path);

		// allocate memory for string
		static
		CHAR *SzAllocate(IMemoryPool *pmp, ULONG ulSize);

		// helper for converting wide character string to regular string
		static
		CHAR *CreateMultiByteCharStringFromWCString(const WCHAR *wcstr);

		// lookup given exception type in the given array
		static
		BOOL FoundException(gpos::CException &exc, const ULONG *exceptions, ULONG size);

		// check if given exception is an unexpected reason for failing to produce a plan
		static
		BOOL IsUnexpectedFailure(gpos::CException &exc);

		// check if given exception should error out
		static
		BOOL ShouldErrorOut(gpos::CException &exc);

		// set cost model parameters
		static
		void SetCostModelParams(ICostModel *cost_model);

		// generate an instance of optimizer cost model
		static
		ICostModel *GetCostModel(IMemoryPool *mp, ULONG num_segments);

		// print warning messages for columns with missing statistics
		static
		void PrintMissingStatsWarning(IMemoryPool *mp, CMDAccessor *md_accessor, IMdIdArray *col_stats, MdidHashSet *phsmdidRel);

	public:

		// convert Query->DXL->LExpr->Optimize->PExpr->DXL
		static
		char *Optimize(Query *query);

		// optimize Query->DXL->LExpr->Optimize->PExpr->DXL->PlannedStmt
		static
		PlannedStmt *GPOPTOptimizedPlan
			(
			Query *query,
			SOptContext* gpopt_context,
			BOOL *had_unexpected_failure // output : set to true if optimizer unexpectedly failed to produce plan
			);

		// convert query to DXL to xml string.
		static
		char *ConvertQueryToDXL(Query *query);

		// convert xml string to DXL and to PS
		static
		PlannedStmt *ConvertToiPlanStmtFromXML(char *xml_string);

		// dump metadata objects from relcache to file in DXL format
		static
		void DumpMDObjs(List *oids, const char *filename);

		// dump metadata objects from relcache to a string in DXL format
		static
		char *SzMDObjs(List *oids);
		
		// dump cast function from relcache to a string in DXL format
		static
		char *DumpMDCast(List *oids);
		
		// dump scalar comparison from relcache to a string in DXL format
		static
		char *DumpMDScalarCmp(List *oids, char *cmp_type);

		// dump statistics from relcache to a string in DXL format
		static
		char *DumpRelStats(List *oids);

		// enable/disable a given xforms
		static
		bool SetXform(char *xform_str, bool should_disable);
		
		// return comparison type code
		static
		ULONG GetComparisonType(char *cmp_type);

		// converts XML string to DXL and evaluates the expression
		static
		char *EvalExprFromXML(char *xml_string);

		// loads a minidump from the given file path, executes it and returns
		// the serialized representation of the result as DXL
		static
		char *OptimizeMinidumpFromFile(char *file_name);
};

#endif // COptTasks_H

// EOF
