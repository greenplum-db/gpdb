//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Greenplum, Inc.
//
//	@filename:
//		COptTasks.cpp
//
//	@doc:
//		Routines to perform optimization related tasks using the gpos framework
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/utils/gpdbdefs.h"
#include "gpopt/utils/CConstExprEvaluatorProxy.h"
#include "gpopt/utils/COptTasks.h"
#include "gpopt/relcache/CMDProviderRelcache.h"
#include "gpopt/config/CConfigParamMapping.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CTranslatorQueryToDXL.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "gpopt/eval/CConstExprEvaluatorDXL.h"
#include "gpopt/engine/CHint.h"

#include "cdb/cdbvars.h"
#include "utils/guc.h"

#include "gpos/base.h"
#include "gpos/error/CException.h"
#undef setstate

#include "gpos/_api.h"
#include "gpos/common/CAutoP.h"
#include "gpos/io/COstreamFile.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/common/CAutoP.h"

#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/engine/CCTEConfig.h"
#include "gpopt/mdcache/CAutoMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/optimizer/COptimizer.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformFactory.h"
#include "gpopt/exception.h"

#include "naucrates/init.h"
#include "naucrates/traceflags/traceflags.h"

#include "naucrates/base/CQueryToDXLResult.h"

#include "naucrates/md/IMDId.h"
#include "naucrates/md/CMDIdRelStats.h"

#include "naucrates/md/CSystemId.h"
#include "naucrates/md/IMDRelStats.h"
#include "naucrates/md/CMDIdCast.h"
#include "naucrates/md/CMDIdScCmp.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/parser/CParseHandlerDXL.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/exception.h"

#include "gpdbcost/CCostModelGPDB.h"
#include "gpdbcost/CCostModelGPDBLegacy.h"


#include "gpopt/gpdbwrappers.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;
using namespace gpdbcost;

// size of error buffer
#define GPOPT_ERROR_BUFFER_SIZE 10 * 1024 * 1024

// definition of default AutoMemoryPool
#define AUTO_MEM_POOL(amp) CAutoMemoryPool amp(CAutoMemoryPool::ElcExc, CMemoryPoolManager::EatTracker, false /* fThreadSafe */)

// default id for the source system
const CSystemId sysidDefault(IMDId::EmdidGPDB, GPOS_WSZ_STR_LENGTH("GPDB"));

// array of optimizer minor exception types that trigger expected fallback to the planner
const ULONG rgulExpectedOptFallback[] =
	{
		gpopt::ExmiInvalidPlanAlternative,		// chosen plan id is outside range of possible plans
		gpopt::ExmiUnsupportedOp,				// unsupported operator
		gpopt::ExmiUnsupportedPred,				// unsupported predicate
		gpopt::ExmiUnsupportedCompositePartKey	// composite partitioning keys
	};

// array of DXL minor exception types that trigger expected fallback to the planner
const ULONG rgulExpectedDXLFallback[] =
	{
		gpdxl::ExmiMDObjUnsupported,			// unsupported metadata object
		gpdxl::ExmiQuery2DXLUnsupportedFeature,	// unsupported feature during algebrization
		gpdxl::ExmiPlStmt2DXLConversion,		// unsupported feature during plan freezing
		gpdxl::ExmiDXL2PlStmtConversion			// unsupported feature during planned statement translation
	};

// array of DXL minor exception types that error out and NOT fallback to the planner
const ULONG rgulExpectedDXLErrors[] =
	{
		gpdxl::ExmiDXL2PlStmtExternalScanError,	// external table error
		gpdxl::ExmiQuery2DXLNotNullViolation,	// not null violation
	};


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::SOptContext
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
SOptContext::SOptContext()
	:
	m_szQueryDXL(NULL),
	m_pquery(NULL),
	m_szPlanDXL(NULL),
	m_pplstmt(NULL),
	m_fGeneratePlStmt(false),
	m_fSerializePlanDXL(false),
	m_fUnexpectedFailure(false),
	m_szErrorMsg(NULL)
{}

//---------------------------------------------------------------------------
//	@function:
//		SOptContext::HandleError
//
//	@doc:
//		If there is an error, throw GPOS_EXCEPTION to abort plan generation.
//		Calling elog::ERROR would result in longjump and hence a memory leak.
//---------------------------------------------------------------------------
void
SOptContext::HandleError
	(
	BOOL *pfUnexpectedFailure
	)
{
	*pfUnexpectedFailure = m_fUnexpectedFailure;
	if (NULL != m_szErrorMsg)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiOptimizerError);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::Free
//
//	@doc:
//		Free all members except those pointed to by either epinInput or
//		epinOutput
//
//---------------------------------------------------------------------------
void
SOptContext::Free
	(
	SOptContext::EPin epinInput,
	SOptContext::EPin epinOutput
	)
{
	if (NULL != m_szQueryDXL && epinQueryDXL != epinInput && epinQueryDXL != epinOutput)
	{
		gpdb::GPDBFree(m_szQueryDXL);
	}
	
	if (NULL != m_pquery && epinQuery != epinInput && epinQuery != epinOutput)
	{
		gpdb::GPDBFree(m_pquery);
	}
	
	if (NULL != m_szPlanDXL && epinPlanDXL != epinInput && epinPlanDXL != epinOutput)
	{
		gpdb::GPDBFree(m_szPlanDXL);
	}
	
	if (NULL != m_pplstmt && epinPlStmt != epinInput && epinPlStmt != epinOutput)
	{
		gpdb::GPDBFree(m_pplstmt);
	}
	
	if (NULL != m_szErrorMsg && epinErrorMsg != epinInput && epinErrorMsg != epinOutput)
	{
		gpdb::GPDBFree(m_szErrorMsg);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		SOptContext::CloneErrorMsg
//
//	@doc:
//		Clone m_szErrorMsg to given memory context. Return NULL if there is no
//		error message.
//
//---------------------------------------------------------------------------
CHAR*
SOptContext::CloneErrorMsg
	(
	MemoryContext context
	)
{
	if (NULL == context ||
		NULL == m_szErrorMsg)
	{
		return NULL;
	}
	return gpdb::SzMemoryContextStrdup(context, m_szErrorMsg);
}


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::PoptctxtConvert
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
SOptContext *
SOptContext::PoptctxtConvert
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	return reinterpret_cast<SOptContext*>(pv);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SContextRelcacheToDXL::SContextRelcacheToDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
COptTasks::SContextRelcacheToDXL::SContextRelcacheToDXL
	(
	List *plistOids,
	ULONG ulCmpt,
	const char *szFilename
	)
	:
	m_plistOids(plistOids),
	m_ulCmpt(ulCmpt),
	m_szFilename(szFilename)
{
	GPOS_ASSERT(NULL != plistOids);
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SContextRelcacheToDXL::PctxrelcacheConvert
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
COptTasks::SContextRelcacheToDXL *
COptTasks::SContextRelcacheToDXL::PctxrelcacheConvert
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	return reinterpret_cast<SContextRelcacheToDXL*>(pv);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SEvalExprContext::PevalctxtConvert
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
COptTasks::SEvalExprContext *
COptTasks::SEvalExprContext::PevalctxtConvert
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	return reinterpret_cast<SEvalExprContext*>(pv);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SOptimizeMinidumpContext::PexecmdpConvert
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
COptTasks::SOptimizeMinidumpContext *
COptTasks::SOptimizeMinidumpContext::PoptmdpConvert
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	return reinterpret_cast<SOptimizeMinidumpContext*>(pv);
}

//---------------------------------------------------------------------------
//	@function:
//		SzFromWsz
//
//	@doc:
//		Return regular string from wide-character string
//
//---------------------------------------------------------------------------
CHAR *
COptTasks::SzFromWsz
	(
	const WCHAR *wsz
	)
{
	GPOS_ASSERT(NULL != wsz);

	const ULONG ulInputLength = GPOS_WSZ_LENGTH(wsz);
	const ULONG ulWCHARSize = GPOS_SIZEOF(WCHAR);
	const ULONG ulMaxLength = (ulInputLength + 1) * ulWCHARSize;

	CHAR *sz = (CHAR *) gpdb::GPDBAlloc(ulMaxLength);

	gpos::clib::LWcsToMbs(sz, const_cast<WCHAR *>(wsz), ulMaxLength);
	sz[ulMaxLength - 1] = '\0';

	return sz;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvMDCast
//
//	@doc:
//		Task that dumps the relcache info for a cast object into DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvMDCast
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SContextRelcacheToDXL *pctxrelcache = SContextRelcacheToDXL::PctxrelcacheConvert(pv);
	GPOS_ASSERT(NULL != pctxrelcache);

	GPOS_ASSERT(2 == gpdb::UlListLength(pctxrelcache->m_plistOids));
	Oid oidSrc = gpdb::OidListNth(pctxrelcache->m_plistOids, 0);
	Oid oidDest = gpdb::OidListNth(pctxrelcache->m_plistOids, 1);

	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	IMemoryPool *pmp = amp.Pmp();

	DrgPimdobj *pdrgpmdobj = GPOS_NEW(pmp) DrgPimdobj(pmp);

	IMDId *pmdidCast = GPOS_NEW(pmp) CMDIdCast(GPOS_NEW(pmp) CMDIdGPDB(oidSrc), GPOS_NEW(pmp) CMDIdGPDB(oidDest));

	// relcache MD provider
	CMDProviderRelcache *pmdpr = GPOS_NEW(pmp) CMDProviderRelcache(pmp);
	
	{
		CAutoMDAccessor amda(pmp, pmdpr, sysidDefault);
	
		IMDCacheObject *pmdobj = CTranslatorRelcacheToDXL::Pimdobj(pmp, amda.Pmda(), pmdidCast);
		GPOS_ASSERT(NULL != pmdobj);
	
		pdrgpmdobj->Append(pmdobj);
		pmdidCast->Release();
	
		CWStringDynamic str(pmp);
		COstreamString oss(&str);

		CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, oss, true /*fSerializeHeaderFooter*/, true /*fIndent*/);
		CHAR *sz = SzFromWsz(str.Wsz());
		GPOS_ASSERT(NULL != sz);

		pctxrelcache->m_szDXL = sz;
		
		// cleanup
		pdrgpmdobj->Release();
	}
	
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvMDScCmp
//
//	@doc:
//		Task that dumps the relcache info for a scalar comparison object into DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvMDScCmp
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SContextRelcacheToDXL *pctxrelcache = SContextRelcacheToDXL::PctxrelcacheConvert(pv);
	GPOS_ASSERT(NULL != pctxrelcache);
	GPOS_ASSERT(CmptOther > pctxrelcache->m_ulCmpt && "Incorrect comparison type specified");

	GPOS_ASSERT(2 == gpdb::UlListLength(pctxrelcache->m_plistOids));
	Oid oidLeft = gpdb::OidListNth(pctxrelcache->m_plistOids, 0);
	Oid oidRight = gpdb::OidListNth(pctxrelcache->m_plistOids, 1);
	CmpType cmpt = (CmpType) pctxrelcache->m_ulCmpt;
	
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	IMemoryPool *pmp = amp.Pmp();

	DrgPimdobj *pdrgpmdobj = GPOS_NEW(pmp) DrgPimdobj(pmp);

	IMDId *pmdidScCmp = GPOS_NEW(pmp) CMDIdScCmp(GPOS_NEW(pmp) CMDIdGPDB(oidLeft), GPOS_NEW(pmp) CMDIdGPDB(oidRight), CTranslatorRelcacheToDXL::Ecmpt(cmpt));

	// relcache MD provider
	CMDProviderRelcache *pmdpr = GPOS_NEW(pmp) CMDProviderRelcache(pmp);
	
	{
		CAutoMDAccessor amda(pmp, pmdpr, sysidDefault);
	
		IMDCacheObject *pmdobj = CTranslatorRelcacheToDXL::Pimdobj(pmp, amda.Pmda(), pmdidScCmp);
		GPOS_ASSERT(NULL != pmdobj);
	
		pdrgpmdobj->Append(pmdobj);
		pmdidScCmp->Release();
	
		CWStringDynamic str(pmp);
		COstreamString oss(&str);

		CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, oss, true /*fSerializeHeaderFooter*/, true /*fIndent*/);
		CHAR *sz = SzFromWsz(str.Wsz());
		GPOS_ASSERT(NULL != sz);

		pctxrelcache->m_szDXL = sz;
		
		// cleanup
		pdrgpmdobj->Release();
	}
	
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::Execute
//
//	@doc:
//		Execute a task using GPOS. TODO extend gpos to provide
//		this functionality
//
//---------------------------------------------------------------------------
void
COptTasks::Execute
	(
	void *(*pfunc) (void *) ,
	void *pfuncArg
	)
{
	Assert(pfunc);

	CHAR *err_buf = (CHAR *) palloc(GPOPT_ERROR_BUFFER_SIZE);
	err_buf[0] = '\0';

	// initialize DXL support
	InitDXL();

	bool abort_flag = false;

	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone, CMemoryPoolManager::EatTracker, false /* fThreadSafe */);

	gpos_exec_params params;
	params.func = pfunc;
	params.arg = pfuncArg;
	params.stack_start = &params;
	params.error_buffer = err_buf;
	params.error_buffer_size = GPOPT_ERROR_BUFFER_SIZE;
	params.abort_requested = &abort_flag;

	// execute task and send log message to server log
	GPOS_TRY
	{
		(void) gpos_exec(&params);
	}
	GPOS_CATCH_EX(ex)
	{
		LogExceptionMessageAndDelete(err_buf, ex.UlSeverityLevel());
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	LogExceptionMessageAndDelete(err_buf);
}

void
COptTasks::LogExceptionMessageAndDelete(CHAR* err_buf, ULONG ulSeverityLevel)
{

	if ('\0' != err_buf[0])
	{
		int ulGpdbSeverityLevel;

		if (ulSeverityLevel == CException::ExsevDebug1)
			ulGpdbSeverityLevel = DEBUG1;
		else
			ulGpdbSeverityLevel = LOG;

		elog(ulGpdbSeverityLevel, "%s", SzFromWsz((WCHAR *)err_buf));
	}

	pfree(err_buf);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvDXLFromQueryTask
//
//	@doc:
//		task that does the translation from query to XML
//
//---------------------------------------------------------------------------
void*
COptTasks::PvDXLFromQueryTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SOptContext *poctx = SOptContext::PoptctxtConvert(pv);

	GPOS_ASSERT(NULL != poctx->m_pquery);
	GPOS_ASSERT(NULL == poctx->m_szQueryDXL);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	// ColId generator
	CIdGenerator *pidgtorCol = GPOS_NEW(pmp) CIdGenerator(GPDXL_COL_ID_START);
	CIdGenerator *pidgtorCTE = GPOS_NEW(pmp) CIdGenerator(GPDXL_CTE_ID_START);

	// map that stores gpdb att to optimizer col mapping
	CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);

	// relcache MD provider
	CMDProviderRelcache *pmdpr = GPOS_NEW(pmp) CMDProviderRelcache(pmp);

	{
		CAutoMDAccessor amda(pmp, pmdpr, sysidDefault);

		CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
		ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
						(
						pmp,
						amda.Pmda(),
						pidgtorCol,
						pidgtorCTE,
						pmapvarcolid,
						(Query*)poctx->m_pquery,
						0 /* ulQueryLevel */
						);
		CDXLNode *pdxlnQuery = ptrquerytodxl->PdxlnFromQuery();
		DrgPdxln *pdrgpdxlnQueryOutput = ptrquerytodxl->PdrgpdxlnQueryOutput();
		DrgPdxln *pdrgpdxlnCTE = ptrquerytodxl->PdrgpdxlnCTE();
		GPOS_ASSERT(NULL != pdrgpdxlnQueryOutput);

		GPOS_DELETE(pidgtorCol);
		GPOS_DELETE(pidgtorCTE);

		CWStringDynamic str(pmp);
		COstreamString oss(&str);

		CDXLUtils::SerializeQuery
						(
						pmp,
						oss,
						pdxlnQuery,
						pdrgpdxlnQueryOutput,
						pdrgpdxlnCTE,
						true, // fSerializeHeaderFooter
						true // fIndent
						);
		poctx->m_szQueryDXL = SzFromWsz(str.Wsz());

		// clean up
		pdxlnQuery->Release();
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::Pplstmt
//
//	@doc:
//		Translate a DXL tree into a planned statement
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::Pplstmt
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	const CDXLNode *pdxln,
	bool canSetTag
	)
{

	GPOS_ASSERT(NULL != pmda);
	GPOS_ASSERT(NULL != pdxln);

	CIdGenerator idgtorPlanId(1 /* ulStartId */);
	CIdGenerator idgtorMotionId(1 /* ulStartId */);
	CIdGenerator idgtorParamId(0 /* ulStartId */);

	List *plRTable = NULL;
	List *plSubplans = NULL;

	CContextDXLToPlStmt ctxdxltoplstmt
							(
							pmp,
							&idgtorPlanId,
							&idgtorMotionId,
							&idgtorParamId,
							&plRTable,
							&plSubplans
							);
	
	// translate DXL -> PlannedStmt
	CTranslatorDXLToPlStmt trdxltoplstmt(pmp, pmda, &ctxdxltoplstmt, gpdb::UlSegmentCountGP());
	return trdxltoplstmt.PplstmtFromDXL(pdxln, canSetTag);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PdrgPssLoad
//
//	@doc:
//		Load search strategy from given file
//
//---------------------------------------------------------------------------
DrgPss *
COptTasks::PdrgPssLoad
	(
	IMemoryPool *pmp,
	char *szPath
	)
{
	DrgPss *pdrgpss = NULL;
	CParseHandlerDXL *pphDXL = NULL;

	GPOS_TRY
	{
		if (NULL != szPath)
		{
			pphDXL = CDXLUtils::PphdxlParseDXLFile(pmp, szPath, NULL);
			if (NULL != pphDXL)
			{
				elog(DEBUG2, "\n[OPT]: Using search strategy in (%s)", szPath);

				pdrgpss = pphDXL->Pdrgpss();
				pdrgpss->AddRef();
			}
		}
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError)) {
			GPOS_RETHROW(ex);
		}
		elog(DEBUG2, "\n[OPT]: Using default search strategy");
		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	GPOS_DELETE(pphDXL);

	return pdrgpss;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PoconfCreate
//
//	@doc:
//		Create the optimizer configuration
//
//---------------------------------------------------------------------------
COptimizerConfig *
COptTasks::PoconfCreate
	(
	IMemoryPool *pmp,
	ICostModel *pcm
	)
{
	// get chosen plan number, cost threshold
	ULLONG ullPlanId = (ULLONG) optimizer_plan_id;
	ULLONG ullSamples = (ULLONG) optimizer_samples_number;
	DOUBLE dCostThreshold = (DOUBLE) optimizer_cost_threshold;

	DOUBLE dDampingFactorFilter = (DOUBLE) optimizer_damping_factor_filter;
	DOUBLE dDampingFactorJoin = (DOUBLE) optimizer_damping_factor_join;
	DOUBLE dDampingFactorGroupBy = (DOUBLE) optimizer_damping_factor_groupby;

	ULONG ulCTEInliningCutoff = (ULONG) optimizer_cte_inlining_bound;
	ULONG ulJoinArityForAssociativityCommutativity = (ULONG) optimizer_join_arity_for_associativity_commutativity;
	ULONG ulArrayExpansionThreshold = (ULONG) optimizer_array_expansion_threshold;
	ULONG ulJoinOrderThreshold = (ULONG) optimizer_join_order_threshold;
	ULONG ulBroadcastThreshold = (ULONG) optimizer_penalize_broadcast_threshold;

	return GPOS_NEW(pmp) COptimizerConfig
						(
						GPOS_NEW(pmp) CEnumeratorConfig(pmp, ullPlanId, ullSamples, dCostThreshold),
						GPOS_NEW(pmp) CStatisticsConfig(pmp, dDampingFactorFilter, dDampingFactorJoin, dDampingFactorGroupBy),
						GPOS_NEW(pmp) CCTEConfig(ulCTEInliningCutoff),
						pcm,
						GPOS_NEW(pmp) CHint
								(
								INT_MAX /* optimizer_parts_to_force_sort_on_insert */,
								ulJoinArityForAssociativityCommutativity,
								ulArrayExpansionThreshold,
								ulJoinOrderThreshold,
								ulBroadcastThreshold,
								false /* don't create Assert nodes for constraints, we'll
								      * enforce them ourselves in the executor */
								)
						);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::FExceptionFound
//
//	@doc:
//		Lookup given exception type in the given array
//
//---------------------------------------------------------------------------
BOOL
COptTasks::FExceptionFound
	(
	gpos::CException &exc,
	const ULONG *pulExceptions,
	ULONG ulSize
	)
{
	GPOS_ASSERT(NULL != pulExceptions);

	ULONG ulMinor = exc.UlMinor();
	BOOL fFound = false;
	for (ULONG ul = 0; !fFound && ul < ulSize; ul++)
	{
		fFound = (pulExceptions[ul] == ulMinor);
	}

	return fFound;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::FUnexpectedFailure
//
//	@doc:
//		Check if given exception is an unexpected reason for failing to
//		produce a plan
//
//---------------------------------------------------------------------------
BOOL
COptTasks::FUnexpectedFailure
	(
	gpos::CException &exc
	)
{
	ULONG ulMajor = exc.UlMajor();

	BOOL fExpectedOptFailure =
		gpopt::ExmaGPOPT == ulMajor &&
		FExceptionFound(exc, rgulExpectedOptFallback, GPOS_ARRAY_SIZE(rgulExpectedOptFallback));

	BOOL fExpectedDXLFailure =
		(gpdxl::ExmaDXL == ulMajor || gpdxl::ExmaMD == ulMajor) &&
		FExceptionFound(exc, rgulExpectedDXLFallback, GPOS_ARRAY_SIZE(rgulExpectedDXLFallback));

	return (!fExpectedOptFailure && !fExpectedDXLFailure);
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::FErrorOut
//
//	@doc:
//		Check if given exception should error out
//
//---------------------------------------------------------------------------
BOOL
COptTasks::FErrorOut
	(
	gpos::CException &exc
	)
{
	return
		gpdxl::ExmaDXL == exc.UlMajor() &&
		FExceptionFound(exc, rgulExpectedDXLErrors, GPOS_ARRAY_SIZE(rgulExpectedDXLErrors));
}

//---------------------------------------------------------------------------
//		@function:
//			COptTasks::SetCostModelParams
//
//      @doc:
//			Set cost model parameters
//
//---------------------------------------------------------------------------
void
COptTasks::SetCostModelParams
	(
	ICostModel *pcm
	)
{
	GPOS_ASSERT(NULL != pcm);

	if (optimizer_nestloop_factor > 1.0)
	{
		// change NLJ cost factor
		ICostModelParams::SCostParam *pcp = NULL;
		if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model)
		{
			pcp = pcm->Pcp()->PcpLookup(CCostModelParamsGPDB::EcpNLJFactor);
		}
		else
		{
			pcp = pcm->Pcp()->PcpLookup(CCostModelParamsGPDBLegacy::EcpNLJFactor);
		}
		CDouble dNLJFactor(optimizer_nestloop_factor);
		pcm->Pcp()->SetParam(pcp->UlId(), dNLJFactor, dNLJFactor - 0.5, dNLJFactor + 0.5);
	}

	if (optimizer_sort_factor > 1.0 || optimizer_sort_factor < 1.0)
	{
		// change sort cost factor
		ICostModelParams::SCostParam *pcp = NULL;
		if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model)
		{
			pcp = pcm->Pcp()->PcpLookup(CCostModelParamsGPDB::EcpSortTupWidthCostUnit);

			CDouble dSortFactor(optimizer_sort_factor);
			pcm->Pcp()->SetParam(pcp->UlId(), pcp->DVal() * optimizer_sort_factor, pcp->DLowerBound() * optimizer_sort_factor, pcp->DUpperBound() * optimizer_sort_factor);
		}
	}
}


//---------------------------------------------------------------------------
//      @function:
//			COptTasks::Pcm
//
//      @doc:
//			Generate an instance of optimizer cost model
//
//---------------------------------------------------------------------------
ICostModel *
COptTasks::Pcm
	(
	IMemoryPool *pmp,
	ULONG ulSegments
	)
{
	ICostModel *pcm = NULL;
	if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model)
	{
		pcm = GPOS_NEW(pmp) CCostModelGPDB(pmp, ulSegments);
	}
	else
	{
		pcm = GPOS_NEW(pmp) CCostModelGPDBLegacy(pmp, ulSegments);
	}

	SetCostModelParams(pcm);

	return pcm;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvOptimizeTask
//
//	@doc:
//		task that does the optimizes query to physical DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvOptimizeTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);
	SOptContext *poctx = SOptContext::PoptctxtConvert(pv);

	GPOS_ASSERT(NULL != poctx->m_pquery);
	GPOS_ASSERT(NULL == poctx->m_szPlanDXL);
	GPOS_ASSERT(NULL == poctx->m_pplstmt);

	// initially assume no unexpected failure
	poctx->m_fUnexpectedFailure = false;

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	// Does the metadatacache need to be reset?
	//
	// On the first call, before the cache has been initialized, we
	// don't care about the return value of FMDCacheNeedsReset(). But
	// we need to call it anyway, to give it a chance to initialize
	// the invalidation mechanism.
	bool reset_mdcache = gpdb::FMDCacheNeedsReset();

	// initialize metadata cache, or purge if needed, or change size if requested
	if (!CMDCache::FInitialized())
	{
		CMDCache::Init();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}
	else if (reset_mdcache)
	{
		CMDCache::Reset();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}
	else if (CMDCache::ULLGetCacheQuota() != (ULLONG) optimizer_mdcache_size * 1024L)
	{
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}


	// load search strategy
	DrgPss *pdrgpss = PdrgPssLoad(pmp, optimizer_search_strategy_path);

	CBitSet *pbsTraceFlags = NULL;
	CBitSet *pbsEnabled = NULL;
	CBitSet *pbsDisabled = NULL;
	CDXLNode *pdxlnPlan = NULL;

	DrgPmdid *pdrgmdidCol = NULL;
	HSMDId *phsmdidRel = NULL;

	GPOS_TRY
	{
		// set trace flags
		pbsTraceFlags = CConfigParamMapping::PbsPack(pmp, CXform::ExfSentinel);
		SetTraceflags(pmp, pbsTraceFlags, &pbsEnabled, &pbsDisabled);

		// set up relcache MD provider
		CMDProviderRelcache *pmdpRelcache = GPOS_NEW(pmp) CMDProviderRelcache(pmp);

		{
			// scope for MD accessor
			CMDAccessor mda(pmp, CMDCache::Pcache(), sysidDefault, pmdpRelcache);

			// ColId generator
			CIdGenerator idgtorColId(GPDXL_COL_ID_START);
			CIdGenerator idgtorCTE(GPDXL_CTE_ID_START);

			// map that stores gpdb att to optimizer col mapping
			CMappingVarColId *pmapvarcolid = GPOS_NEW(pmp) CMappingVarColId(pmp);

			ULONG ulSegments = gpdb::UlSegmentCountGP();
			ULONG ulSegmentsForCosting = optimizer_segments;
			if (0 == ulSegmentsForCosting)
			{
				ulSegmentsForCosting = ulSegments;
			}

			CAutoP<CTranslatorQueryToDXL> ptrquerytodxl;
			ptrquerytodxl = CTranslatorQueryToDXL::PtrquerytodxlInstance
							(
							pmp,
							&mda,
							&idgtorColId,
							&idgtorCTE,
							pmapvarcolid,
							(Query*) poctx->m_pquery,
							0 /* ulQueryLevel */
							);

			ICostModel *pcm = Pcm(pmp, ulSegmentsForCosting);
			COptimizerConfig *pocconf = PoconfCreate(pmp, pcm);
			CConstExprEvaluatorProxy ceevalproxy(pmp, &mda);
			IConstExprEvaluator *pceeval =
					GPOS_NEW(pmp) CConstExprEvaluatorDXL(pmp, &mda, &ceevalproxy);

			CDXLNode *pdxlnQuery = ptrquerytodxl->PdxlnFromQuery();
			DrgPdxln *pdrgpdxlnQueryOutput = ptrquerytodxl->PdrgpdxlnQueryOutput();
			DrgPdxln *pdrgpdxlnCTE = ptrquerytodxl->PdrgpdxlnCTE();
			GPOS_ASSERT(NULL != pdrgpdxlnQueryOutput);

			BOOL fMasterOnly = !optimizer_enable_motions ||
						(!optimizer_enable_motions_masteronly_queries && !ptrquerytodxl->FHasDistributedTables());
			CAutoTraceFlag atf(EopttraceDisableMotions, fMasterOnly);

			pdxlnPlan = COptimizer::PdxlnOptimize
									(
									pmp,
									&mda,
									pdxlnQuery,
									pdrgpdxlnQueryOutput,
									pdrgpdxlnCTE,
									pceeval,
									ulSegments,
									gp_session_id,
									gp_command_count,
									pdrgpss,
									pocconf
									);

			if (poctx->m_fSerializePlanDXL)
			{
				// serialize DXL to xml
				CWStringDynamic strPlan(pmp);
				COstreamString oss(&strPlan);
				CDXLUtils::SerializePlan(pmp, oss, pdxlnPlan, pocconf->Pec()->UllPlanId(), pocconf->Pec()->UllPlanSpaceSize(), true /*fSerializeHeaderFooter*/, true /*fIndent*/);
				poctx->m_szPlanDXL = SzFromWsz(strPlan.Wsz());
			}

			// translate DXL->PlStmt only when needed
			if (poctx->m_fGeneratePlStmt)
			{
				// always use poctx->m_pquery->canSetTag as the ptrquerytodxl->Pquery() is a mutated Query object
				// that may not have the correct canSetTag
				poctx->m_pplstmt = (PlannedStmt *) gpdb::PvCopyObject(Pplstmt(pmp, &mda, pdxlnPlan, poctx->m_pquery->canSetTag));
			}

			CStatisticsConfig *pstatsconf = pocconf->Pstatsconf();
			pdrgmdidCol = GPOS_NEW(pmp) DrgPmdid(pmp);
			pstatsconf->CollectMissingStatsColumns(pdrgmdidCol);

			phsmdidRel = GPOS_NEW(pmp) HSMDId(pmp);
			PrintMissingStatsWarning(pmp, &mda, pdrgmdidCol, phsmdidRel);

			phsmdidRel->Release();
			pdrgmdidCol->Release();

			pceeval->Release();
			pdxlnQuery->Release();
			pocconf->Release();
			pdxlnPlan->Release();
		}
	}
	GPOS_CATCH_EX(ex)
	{
		ResetTraceflags(pbsEnabled, pbsDisabled);
		CRefCount::SafeRelease(phsmdidRel);
		CRefCount::SafeRelease(pdrgmdidCol);
		CRefCount::SafeRelease(pbsEnabled);
		CRefCount::SafeRelease(pbsDisabled);
		CRefCount::SafeRelease(pbsTraceFlags);
		CRefCount::SafeRelease(pdxlnPlan);
		CMDCache::Shutdown();

		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			elog(DEBUG1, "GPDB Exception. Please check log for more information.");
		}
		else if (FErrorOut(ex))
		{
			IErrorContext *perrctxt = CTask::PtskSelf()->Perrctxt();
			poctx->m_szErrorMsg = SzFromWsz(perrctxt->WszMsg());
		}
		else
		{
			poctx->m_fUnexpectedFailure = FUnexpectedFailure(ex);
		}
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	// cleanup
	ResetTraceflags(pbsEnabled, pbsDisabled);
	CRefCount::SafeRelease(pbsEnabled);
	CRefCount::SafeRelease(pbsDisabled);
	CRefCount::SafeRelease(pbsTraceFlags);
	if (!optimizer_metadata_caching)
	{
		CMDCache::Shutdown();
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PrintMissingStatsWarning
//
//	@doc:
//		Print warning messages for columns with missing statistics
//
//---------------------------------------------------------------------------
void
COptTasks::PrintMissingStatsWarning
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	DrgPmdid *pdrgmdidCol,
	HSMDId *phsmdidRel
	)
{
	GPOS_ASSERT(NULL != pmda);
	GPOS_ASSERT(NULL != pdrgmdidCol);
	GPOS_ASSERT(NULL != phsmdidRel);

	CWStringDynamic str(pmp);
	COstreamString oss(&str);

	const ULONG ulMissingColStats = pdrgmdidCol->UlLength();
	for (ULONG ul = 0; ul < ulMissingColStats; ul++)
	{
		IMDId *pmdid = (*pdrgmdidCol)[ul];
		CMDIdColStats *pmdidColStats = CMDIdColStats::PmdidConvert(pmdid);

		IMDId *pmdidRel = pmdidColStats->PmdidRel();
		const ULONG ulPos = pmdidColStats->UlPos();
		const IMDRelation *pmdrel = pmda->Pmdrel(pmdidRel);

		if (IMDRelation::ErelstorageExternal != pmdrel->Erelstorage())
		{
			if (!phsmdidRel->FExists(pmdidRel))
			{
				if (0 != ul)
				{
					oss << ", ";
				}

				pmdidRel->AddRef();
				phsmdidRel->FInsert(pmdidRel);
				oss << pmdrel->Mdname().Pstr()->Wsz();
			}

			CMDName mdname = pmdrel->Pmdcol(ulPos)->Mdname();

			char msgbuf[NAMEDATALEN * 2 + 100];
			snprintf(msgbuf, sizeof(msgbuf), "Missing statistics for column: %s.%s", SzFromWsz(pmdrel->Mdname().Pstr()->Wsz()), SzFromWsz(mdname.Pstr()->Wsz()));
			GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION,
						   LOG,
						   msgbuf,
						   NULL);
		}
	}

	if (0 < phsmdidRel->UlEntries())
	{
		int length = NAMEDATALEN * phsmdidRel->UlEntries() + 200;
		char msgbuf[length];
		snprintf(msgbuf, sizeof(msgbuf), "One or more columns in the following table(s) do not have statistics: %s", SzFromWsz(str.Wsz()));
		GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION,
					   NOTICE,
					   msgbuf,
					   "For non-partitioned tables, run analyze <table_name>(<column_list>)."
					   " For partitioned tables, run analyze rootpartition <table_name>(<column_list>)."
					   " See log for columns missing statistics.");
	}

}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvOptimizeMinidumpTask
//
//	@doc:
//		Task that loads and optimizes a minidump and returns the result as string-serialized DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvOptimizeMinidumpTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SOptimizeMinidumpContext *poptmdpctxt = SOptimizeMinidumpContext::PoptmdpConvert(pv);
	GPOS_ASSERT(NULL != poptmdpctxt->m_szFileName);
	GPOS_ASSERT(NULL == poptmdpctxt->m_szDXLResult);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	ULONG ulSegments = gpdb::UlSegmentCountGP();
	ULONG ulSegmentsForCosting = optimizer_segments;
	if (0 == ulSegmentsForCosting)
	{
		ulSegmentsForCosting = ulSegments;
	}

	ICostModel *pcm = Pcm(pmp, ulSegmentsForCosting);
	COptimizerConfig *pocconf = PoconfCreate(pmp, pcm);
	CDXLNode *pdxlnResult = NULL;

	GPOS_TRY
	{
		pdxlnResult = CMinidumperUtils::PdxlnExecuteMinidump(pmp, poptmdpctxt->m_szFileName, ulSegments, gp_session_id, gp_command_count, pocconf);
	}
	GPOS_CATCH_EX(ex)
	{
		CRefCount::SafeRelease(pdxlnResult);
		CRefCount::SafeRelease(pocconf);
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	CWStringDynamic strDXL(pmp);
	COstreamString oss(&strDXL);
	CDXLUtils::SerializePlan
					(
					pmp,
					oss,
					pdxlnResult,
					0,  // ullPlanId
					0,  // ullPlanSpaceSize
					true, // fSerializeHeaderFooter
					true // fIndent
					);
	poptmdpctxt->m_szDXLResult = SzFromWsz(strDXL.Wsz());
	CRefCount::SafeRelease(pdxlnResult);
	pocconf->Release();

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvPlstmtFromDXLTask
//
//	@doc:
//		task that does the translation from xml to dxl to pplstmt
//
//---------------------------------------------------------------------------
void*
COptTasks::PvPlstmtFromDXLTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SOptContext *poctx = SOptContext::PoptctxtConvert(pv);

	GPOS_ASSERT(NULL == poctx->m_pquery);
	GPOS_ASSERT(NULL != poctx->m_szPlanDXL);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	CWStringDynamic str(pmp);
	COstreamString oss(&str);

	ULLONG ullPlanId = 0;
	ULLONG ullPlanSpaceSize = 0;
	CDXLNode *pdxlnOriginal =
		CDXLUtils::PdxlnParsePlan(pmp, poctx->m_szPlanDXL, NULL /*XSD location*/, &ullPlanId, &ullPlanSpaceSize);

	CIdGenerator idgtorPlanId(1);
	CIdGenerator idgtorMotionId(1);
	CIdGenerator idgtorParamId(0);

	List *plRTable = NULL;
	List *plSubplans = NULL;

	CContextDXLToPlStmt ctxdxlplstmt
							(
							pmp,
							&idgtorPlanId,
							&idgtorMotionId,
							&idgtorParamId,
							&plRTable,
							&plSubplans
							);

	// relcache MD provider
	CMDProviderRelcache *pmdpr = GPOS_NEW(pmp) CMDProviderRelcache(pmp);

	{
		CAutoMDAccessor amda(pmp, pmdpr, sysidDefault);

		// translate DXL -> PlannedStmt
		CTranslatorDXLToPlStmt trdxltoplstmt(pmp, amda.Pmda(), &ctxdxlplstmt, gpdb::UlSegmentCountGP());
		PlannedStmt *pplstmt = trdxltoplstmt.PplstmtFromDXL(pdxlnOriginal, poctx->m_pquery->canSetTag);
		if (optimizer_print_plan)
		{
			elog(NOTICE, "Plstmt: %s", gpdb::SzNodeToString(pplstmt));
		}

		GPOS_ASSERT(NULL != pplstmt);
		GPOS_ASSERT(NULL != CurrentMemoryContext);

		poctx->m_pplstmt = (PlannedStmt *) gpdb::PvCopyObject(pplstmt);
	}

	// cleanup
	pdxlnOriginal->Release();

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvDXLFromMDObjsTask
//
//	@doc:
//		task that does dumps the relcache info for an object into DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvDXLFromMDObjsTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SContextRelcacheToDXL *pctxrelcache = SContextRelcacheToDXL::PctxrelcacheConvert(pv);
	GPOS_ASSERT(NULL != pctxrelcache);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	DrgPimdobj *pdrgpmdobj = GPOS_NEW(pmp) DrgPimdobj(pmp, 1024, 1024);

	// relcache MD provider
	CMDProviderRelcache *pmdp = GPOS_NEW(pmp) CMDProviderRelcache(pmp);
	{
		CAutoMDAccessor amda(pmp, pmdp, sysidDefault);
		ListCell *plc = NULL;
		ForEach (plc, pctxrelcache->m_plistOids)
		{
			Oid oid = lfirst_oid(plc);
			// get object from relcache
			CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(oid, 1 /* major */, 0 /* minor */);

			IMDCacheObject *pimdobj = CTranslatorRelcacheToDXL::Pimdobj(pmp, amda.Pmda(), pmdid);
			GPOS_ASSERT(NULL != pimdobj);

			pdrgpmdobj->Append(pimdobj);
			pmdid->Release();
		}

		if (pctxrelcache->m_szFilename)
		{
			COstreamFile cofs(pctxrelcache->m_szFilename);

			CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, cofs, true /*fSerializeHeaderFooter*/, true /*fIndent*/);
		}
		else
		{
			CWStringDynamic str(pmp);
			COstreamString oss(&str);

			CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, oss, true /*fSerializeHeaderFooter*/, true /*fIndent*/);

			CHAR *sz = SzFromWsz(str.Wsz());

			GPOS_ASSERT(NULL != sz);

			pctxrelcache->m_szDXL = sz;
		}
	}
	// cleanup
	pdrgpmdobj->Release();

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvDXLFromRelStatsTask
//
//	@doc:
//		task that dumps relstats info for a table in DXL
//
//---------------------------------------------------------------------------
void*
COptTasks::PvDXLFromRelStatsTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);

	SContextRelcacheToDXL *pctxrelcache = SContextRelcacheToDXL::PctxrelcacheConvert(pv);
	GPOS_ASSERT(NULL != pctxrelcache);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	// relcache MD provider
	CMDProviderRelcache *pmdpr = GPOS_NEW(pmp) CMDProviderRelcache(pmp);
	CAutoMDAccessor amda(pmp, pmdpr, sysidDefault);
	ICostModel *pcm = Pcm(pmp, gpdb::UlSegmentCountGP());
	CAutoOptCtxt aoc(pmp, amda.Pmda(), NULL /*pceeval*/, pcm);

	DrgPimdobj *pdrgpmdobj = GPOS_NEW(pmp) DrgPimdobj(pmp);

	ListCell *plc = NULL;
	ForEach (plc, pctxrelcache->m_plistOids)
	{
		Oid oidRelation = lfirst_oid(plc);

		// get object from relcache
		CMDIdGPDB *pmdid = GPOS_NEW(pmp) CMDIdGPDB(oidRelation, 1 /* major */, 0 /* minor */);

		// generate mdid for relstats
		CMDIdRelStats *pmdidRelStats = GPOS_NEW(pmp) CMDIdRelStats(pmdid);
		IMDRelStats *pimdobjrelstats = const_cast<IMDRelStats *>(amda.Pmda()->Pmdrelstats(pmdidRelStats));
		pdrgpmdobj->Append(dynamic_cast<IMDCacheObject *>(pimdobjrelstats));

		// extract out column stats for this relation
		Relation rel = gpdb::RelGetRelation(oidRelation);
		ULONG ulPosCounter = 0;
		for (ULONG ul = 0; ul < ULONG(rel->rd_att->natts); ul++)
		{
			if (!rel->rd_att->attrs[ul]->attisdropped)
			{
				pmdid->AddRef();
				CMDIdColStats *pmdidColStats = GPOS_NEW(pmp) CMDIdColStats(pmdid, ulPosCounter);
				ulPosCounter++;
				IMDColStats *pimdobjcolstats = const_cast<IMDColStats *>(amda.Pmda()->Pmdcolstats(pmdidColStats));
				pdrgpmdobj->Append(dynamic_cast<IMDCacheObject *>(pimdobjcolstats));
				pmdidColStats->Release();
			}
		}
		gpdb::CloseRelation(rel);
		pmdidRelStats->Release();
	}

	if (pctxrelcache->m_szFilename)
	{
		COstreamFile cofs(pctxrelcache->m_szFilename);

		CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, cofs, true /*fSerializeHeaderFooter*/, true /*fIndent*/);
	}
	else
	{
		CWStringDynamic str(pmp);
		COstreamString oss(&str);

		CDXLUtils::SerializeMetadata(pmp, pdrgpmdobj, oss, true /*fSerializeHeaderFooter*/, true /*fIndent*/);

		CHAR *sz = SzFromWsz(str.Wsz());

		GPOS_ASSERT(NULL != sz);

		pctxrelcache->m_szDXL = sz;
	}

	// cleanup
	pdrgpmdobj->Release();

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PvEvalExprFromDXLTask
//
//	@doc:
//		Task that parses an XML representation of a DXL constant expression,
//		evaluates it and returns the result as a serialized DXL document.
//
//---------------------------------------------------------------------------
void*
COptTasks::PvEvalExprFromDXLTask
	(
	void *pv
	)
{
	GPOS_ASSERT(NULL != pv);
	SEvalExprContext *pevalctxt = SEvalExprContext::PevalctxtConvert(pv);

	GPOS_ASSERT(NULL != pevalctxt->m_szDXL);
	GPOS_ASSERT(NULL == pevalctxt->m_szDXLResult);

	AUTO_MEM_POOL(amp);
	IMemoryPool *pmp = amp.Pmp();

	CDXLNode *pdxlnInput = CDXLUtils::PdxlnParseScalarExpr(pmp, pevalctxt->m_szDXL, NULL /*szXSDPath*/);
	GPOS_ASSERT(NULL != pdxlnInput);

	CDXLNode *pdxlnResult = NULL;
	BOOL fReleaseCache = false;

	// Does the metadatacache need to be reset?
	//
	// On the first call, before the cache has been initialized, we
	// don't care about the return value of FMDCacheNeedsReset(). But
	// we need to call it anyway, to give it a chance to initialize
	// the invalidation mechanism.
	bool reset_mdcache = gpdb::FMDCacheNeedsReset();

	// initialize metadata cache, or purge if needed, or change size if requested
	if (!CMDCache::FInitialized())
	{
		CMDCache::Init();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
		fReleaseCache = true;
	}
	else if (reset_mdcache)
	{
		CMDCache::Reset();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}
	else if (CMDCache::ULLGetCacheQuota() != (ULLONG) optimizer_mdcache_size * 1024L)
	{
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}

	GPOS_TRY
	{
		// set up relcache MD provider
		CMDProviderRelcache *pmdpRelcache = GPOS_NEW(pmp) CMDProviderRelcache(pmp);
		{
			// scope for MD accessor
			CMDAccessor mda(pmp, CMDCache::Pcache(), sysidDefault, pmdpRelcache);

			CConstExprEvaluatorProxy ceeval(pmp, &mda);
			pdxlnResult = ceeval.PdxlnEvaluateExpr(pdxlnInput);
		}
	}
	GPOS_CATCH_EX(ex)
	{
		CRefCount::SafeRelease(pdxlnResult);
		CRefCount::SafeRelease(pdxlnInput);
		if (fReleaseCache)
		{
			CMDCache::Shutdown();
		}
		if (FErrorOut(ex))
		{
			IErrorContext *perrctxt = CTask::PtskSelf()->Perrctxt();
			char *szErrorMsg = SzFromWsz(perrctxt->WszMsg());
			elog(DEBUG1, "%s", szErrorMsg);
			gpdb::GPDBFree(szErrorMsg);
		}
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	CWStringDynamic *pstrDXL =
			CDXLUtils::PstrSerializeScalarExpr
						(
						pmp,
						pdxlnResult,
						true, // fSerializeHeaderFooter
						true // fIndent
						);
	pevalctxt->m_szDXLResult = SzFromWsz(pstrDXL->Wsz());
	GPOS_DELETE(pstrDXL);
	CRefCount::SafeRelease(pdxlnResult);
	pdxlnInput->Release();

	if (fReleaseCache)
	{
		CMDCache::Shutdown();
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzOptimize
//
//	@doc:
//		optimizes a query to physical DXL
//
//---------------------------------------------------------------------------
char *
COptTasks::SzOptimize
	(
	Query *pquery
	)
{
	Assert(pquery);

	SOptContext octx;
	octx.m_pquery = pquery;
	octx.m_fSerializePlanDXL = true;
	Execute(&PvOptimizeTask, &octx);

	// clean up context
	octx.Free(octx.epinQuery, octx.epinPlanDXL);

	return octx.m_szPlanDXL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PplstmtOptimize
//
//	@doc:
//		optimizes a query to plannedstmt
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::PplstmtOptimize
	(
	Query *pquery,
	SOptContext *octx,
	BOOL *pfUnexpectedFailure // output : set to true if optimizer unexpectedly failed to produce plan
	)
{
	Assert(pquery);
	Assert(octx);

	octx->m_pquery = pquery;
	octx->m_fGeneratePlStmt= true;
	GPOS_TRY
	{
		Execute(&PvOptimizeTask, octx);
	}
	GPOS_CATCH_EX(ex)
	{
		*pfUnexpectedFailure = octx->m_fUnexpectedFailure;
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	octx->HandleError(pfUnexpectedFailure);
	return octx->m_pplstmt;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzDXL
//
//	@doc:
//		serializes query to DXL
//
//---------------------------------------------------------------------------
char *
COptTasks::SzDXL
	(
	Query *pquery
	)
{
	Assert(pquery);

	SOptContext octx;
	octx.m_pquery = pquery;
	Execute(&PvDXLFromQueryTask, &octx);

	// clean up context
	octx.Free(octx.epinQuery, octx.epinQueryDXL);

	return octx.m_szQueryDXL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::PplstmtFromXML
//
//	@doc:
//		deserializes planned stmt from DXL
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::PplstmtFromXML
	(
	char *szDXL
	)
{
	Assert(NULL != szDXL);

	SOptContext octx;
	octx.m_szPlanDXL = szDXL;
	Execute(&PvPlstmtFromDXLTask, &octx);

	// clean up context
	octx.Free(octx.epinPlanDXL, octx.epinPlStmt);

	return octx.m_pplstmt;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::DumpMDObjs
//
//	@doc:
//		Dump relcache objects into DXL file
//
//---------------------------------------------------------------------------
void
COptTasks::DumpMDObjs
	(
	List *plistOids,
	const char *szFilename
	)
{
	SContextRelcacheToDXL ctxrelcache(plistOids, ULONG_MAX /*ulCmpt*/, szFilename);
	Execute(&PvDXLFromMDObjsTask, &ctxrelcache);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzMDObjs
//
//	@doc:
//		Dump relcache objects into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::SzMDObjs
	(
	List *plistOids
	)
{
	SContextRelcacheToDXL ctxrelcache(plistOids, ULONG_MAX /*ulCmpt*/, NULL /*szFilename*/);
	Execute(&PvDXLFromMDObjsTask, &ctxrelcache);

	return ctxrelcache.m_szDXL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzMDCast
//
//	@doc:
//		Dump cast object into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::SzMDCast
	(
	List *plistOids
	)
{
	SContextRelcacheToDXL ctxrelcache(plistOids, ULONG_MAX /*ulCmpt*/, NULL /*szFilename*/);
	Execute(&PvMDCast, &ctxrelcache);

	return ctxrelcache.m_szDXL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzMDScCmp
//
//	@doc:
//		Dump scalar comparison object into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::SzMDScCmp
	(
	List *plistOids,
	char *szCmpType
	)
{
	SContextRelcacheToDXL ctxrelcache(plistOids, UlCmpt(szCmpType), NULL /*szFilename*/);
	Execute(&PvMDScCmp, &ctxrelcache);

	return ctxrelcache.m_szDXL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzRelStats
//
//	@doc:
//		Dump statistics objects into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::SzRelStats
	(
	List *plistOids
	)
{
	SContextRelcacheToDXL ctxrelcache(plistOids, ULONG_MAX /*ulCmpt*/, NULL /*szFilename*/);
	Execute(&PvDXLFromRelStatsTask, &ctxrelcache);

	return ctxrelcache.m_szDXL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::FSetXform
//
//	@doc:
//		Enable/Disable a given xform
//
//---------------------------------------------------------------------------
bool
COptTasks::FSetXform
	(
	char *szXform,
	bool fDisable
	)
{
	CXform *pxform = CXformFactory::Pxff()->Pxf(szXform);
	if (NULL != pxform)
	{
		optimizer_xforms[pxform->Exfid()] = fDisable;

		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::UlCmpt
//
//	@doc:
//		Find the comparison type code given its string representation
//
//---------------------------------------------------------------------------
ULONG
COptTasks::UlCmpt
	(
	char *szCmpType
	)
{
	const ULONG ulCmpTypes = 6;
	const CmpType rgcmpt[] = {CmptEq, CmptNEq, CmptLT, CmptGT, CmptLEq, CmptGEq};
	const CHAR *rgszCmpTypes[] = {"Eq", "NEq", "LT", "GT", "LEq", "GEq"};
	
	for (ULONG ul = 0; ul < ulCmpTypes; ul++)
	{		
		if (0 == strcasecmp(szCmpType, rgszCmpTypes[ul]))
		{
			return rgcmpt[ul];
		}
	}

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiInvalidComparisonTypeCode);
	return CmptOther;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzEvalExprFromXML
//
//	@doc:
//		Converts XML string to DXL and evaluates the expression. Caller keeps
//		ownership of 'szXmlString' and takes ownership of the returned result.
//
//---------------------------------------------------------------------------
char *
COptTasks::SzEvalExprFromXML
	(
	char *szXmlString
	)
{
	GPOS_ASSERT(NULL != szXmlString);

	SEvalExprContext evalctxt;
	evalctxt.m_szDXL = szXmlString;
	evalctxt.m_szDXLResult = NULL;

	Execute(&PvEvalExprFromDXLTask, &evalctxt);
	return evalctxt.m_szDXLResult;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SzOptimizeMinidumpFromFile
//
//	@doc:
//		Loads a minidump from the given file path, optimizes it and returns
//		the serialized representation of the result as DXL.
//
//---------------------------------------------------------------------------
char *
COptTasks::SzOptimizeMinidumpFromFile
	(
	char *szFileName
	)
{
	GPOS_ASSERT(NULL != szFileName);
	SOptimizeMinidumpContext optmdpctxt;
	optmdpctxt.m_szFileName = szFileName;
	optmdpctxt.m_szDXLResult = NULL;

	Execute(&PvOptimizeMinidumpTask, &optmdpctxt);
	return optmdpctxt.m_szDXLResult;
}

// EOF
