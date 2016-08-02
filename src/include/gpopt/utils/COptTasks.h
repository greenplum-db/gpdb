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

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

class COptTasks
{

	private:

		// context of optimizer input and output objects
		struct SOptContext
		{

			// mark which pointer member should NOT be released
			// when calling Free() function
			enum EPin
			{
				epinQueryDXL, // keep m_szQueryDXL
				epinQuery, 	 // keep m_pquery
				epinPlanDXL, // keep m_szPlanDXL
				epinPlStmt, // keep m_pplstmt
				epinErrorMsg // keep m_szErrorMsg
			};

			// query object serialized to DXL
			CHAR *m_szQueryDXL;

			// query object
			Query *m_pquery;

			// plan object serialized to DXL
			CHAR *m_szPlanDXL;

			// plan object
			PlannedStmt *m_pplstmt;

			// is generating a plan object required ?
			BOOL m_fGeneratePlStmt;

			// is serializing a plan to DXL required ?
			BOOL m_fSerializePlanDXL;

			// did the optimizer fail unexpectedly?
			BOOL m_fUnexpectedFailure;

			// buffer for optimizer error messages
			CHAR *m_szErrorMsg;

			// ctor
			SOptContext();

			// If there is an error print as warning and throw exception to abort
			// plan generation
			void HandleError(BOOL *pfUnexpectedFailure);

			// free all members except input and output pointers
			void Free(EPin epinInput, EPin epinOutput);

			// casting function
			static
			SOptContext *PoptctxtConvert(void *pv);

		}; // struct SOptContext


		// context of relcache input and output objects
		struct SContextRelcacheToDXL
		{
			// list of object oids to lookup
			List *m_plistOids;

			// comparison type for tasks retrieving scalar comparisons
			ULONG m_ulCmpt;

			// if filename is not null, then output will be written to file
			const char *m_szFilename;

			// if filename is null, then output will be stored here
			char *m_szDXL;

			// ctor
			SContextRelcacheToDXL(List *plistOids, ULONG ulCmpt, const char *szFilename);

			// casting function
			static
			SContextRelcacheToDXL *PctxrelcacheConvert(void *pv);
		};

		// Structure containing the input and output string for a task that evaluates expressions.
		struct SEvalExprContext
		{
			// Serialized DXL of the expression to be evaluated
			char *m_szDXL;

			// The result of evaluating the expression
			char *m_szDXLResult;

			// casting function
			static
			SEvalExprContext *PevalctxtConvert(void *pv);
		};

		// context of minidump load and execution
		struct SOptimizeMinidumpContext
		{
			// the name of the file containing the minidump
			char *m_szFileName;

			// the result of optimizing the minidump
			char *m_szDXLResult;

			// casting function
			static
			SOptimizeMinidumpContext *PoptmdpConvert(void *pv);
		};

		// execute a task given the argument
		static
		void Execute ( void *(*pfunc) (void *), void *pfuncArg);

		// task that does the translation from xml to dxl to pplstmt
		static
		void* PvPlstmtFromDXLTask(void *pv);

		// task that does the translation from query to XML
		static
		void* PvDXLFromQueryTask(void *pv);

		// dump relcache info for an object into DXL
		static
		void* PvDXLFromMDObjsTask(void *pv);

		// dump metadata about cast objects from relcache to a string in DXL format
		static
		void *PvMDCast(void *pv);
		
		// dump metadata about scalar comparison objects from relcache to a string in DXL format
		static
		void *PvMDScCmp(void *pv);
		
		// dump relstats info for an object into DXL
		static
		void* PvDXLFromRelStatsTask(void *pv);

		// evaluates an expression given as a serialized DXL string and returns the serialized DXL result
		static
		void* PvEvalExprFromDXLTask(void *pv);

		// create optimizer configuration object
		static
		COptimizerConfig *PoconfCreate(IMemoryPool *pmp, ICostModel *pcm);

		// optimize a query to a physical DXL
		static
		void* PvOptimizeTask(void *pv);

		// optimize the query in a minidump and return resulting plan in DXL format
		static
		void* PvOptimizeMinidumpTask(void *pv);

		// translate a DXL tree into a planned statement
		static
		PlannedStmt *Pplstmt(IMemoryPool *pmp, CMDAccessor *pmda, const CDXLNode *pdxln, bool canSetTag);

		// load search strategy from given path
		static
		DrgPss *PdrgPssLoad(IMemoryPool *pmp, char *szPath);

		// allocate memory for string
		static
		CHAR *SzAllocate(IMemoryPool *pmp, ULONG ulSize);

		// helper for converting wide character string to regular string
		static
		CHAR *SzFromWsz(const WCHAR *wsz);

		// lookup given exception type in the given array
		static
		BOOL FExceptionFound(gpos::CException &exc, const ULONG *pulExceptions, ULONG ulSize);

		// check if given exception is an unexpected reason for failing to produce a plan
		static
		BOOL FUnexpectedFailure(gpos::CException &exc);

		// check if given exception should error out
		static
		BOOL FErrorOut(gpos::CException &exc);

		// set cost model parameters
		static
		void SetCostModelParams(ICostModel *pcm);

		// generate an instance of optimizer cost model
		static
		ICostModel *Pcm(IMemoryPool *pmp, ULONG ulSegments);

		// print warning messages for columns with missing statistics
		static
		void PrintMissingStatsWarning(IMemoryPool *pmp, CMDAccessor *pmda, DrgPmdid *pdrgmdidCol, HMMDIdMDId *phmmdidRel);

	public:

		// convert Query->DXL->LExpr->Optimize->PExpr->DXL
		static
		char *SzOptimize(Query *pquery);

		// optimize Query->DXL->LExpr->Optimize->PExpr->DXL->PlannedStmt
		static
		PlannedStmt *PplstmtOptimize
			(
			Query *pquery,
			BOOL *pfUnexpectedFailure // output : set to true if optimizer unexpectedly failed to produce plan
			);

		// convert query to DXL to xml string.
		static
		char *SzDXL(Query *pquery);

		// convert xml string to DXL and to PS
		static
		PlannedStmt *PplstmtFromXML(char *szXmlString);

		// dump metadata objects from relcache to file in DXL format
		static
		void DumpMDObjs(List *oids, const char *szFilename);

		// dump metadata objects from relcache to a string in DXL format
		static
		char *SzMDObjs(List *oids);
		
		// dump cast function from relcache to a string in DXL format
		static
		char *SzMDCast(List *oids);
		
		// dump scalar comparison from relcache to a string in DXL format
		static
		char *SzMDScCmp(List *oids, char *szCmpType);

		// dump statistics from relcache to a string in DXL format
		static
		char *SzRelStats(List *oids);

		// enable/disable a given xforms
		static
		bool FSetXform(char *szXform, bool fDisable);
		
		// return comparison type code
		static
		ULONG UlCmpt(char *szCmpType);

		// converts XML string to DXL and evaluates the expression
		static
		char *SzEvalExprFromXML(char *szXmlString);

		// loads a minidump from the given file path, executes it and returns
		// the serialized representation of the result as DXL
		static
		char *SzOptimizeMinidumpFromFile(char *szFileName);
};

#endif // COptTasks_H

// EOF
