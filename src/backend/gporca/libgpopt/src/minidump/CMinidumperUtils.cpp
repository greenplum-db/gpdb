//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMinidumperUtils.cpp
//
//	@doc:
//		Implementation of minidump utility functions
//---------------------------------------------------------------------------

#include "gpopt/minidump/CMinidumperUtils.h"

#include <fstream>

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/common/CAutoTimer.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/syslibwrapper.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/io/COstreamFile.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/task/CTask.h"
#include "gpos/task/CWorker.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/cost/ICostModel.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpopt/minidump/CMiniDumperDXL.h"
#include "gpopt/optimizer/COptimizer.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/parser/CParseHandlerDXL.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;
using namespace std;

//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::PdxlmdLoad
//
//	@doc:
//		Load minidump file
//
//---------------------------------------------------------------------------
CDXLMinidump *
CMinidumperUtils::PdxlmdLoad(CMemoryPool *mp, const CHAR *file_name)
{
	CAutoTraceFlag atf1(EtraceSimulateAbort, false);
	CAutoTraceFlag atf2(EtraceSimulateOOM, false);

	{
		CAutoTrace at(mp);
		at.Os() << "parsing DXL File " << file_name;
	}

	CParseHandlerDXL *parse_handler_dxl = CDXLUtils::GetParseHandlerForDXLFile(
		mp, file_name, NULL /*xsd_file_path*/);

	CBitSet *pbs = parse_handler_dxl->Pbs();
	COptimizerConfig *optimizer_config =
		parse_handler_dxl->GetOptimizerConfig();
	CDXLNode *query = parse_handler_dxl->GetQueryDXLRoot();
	CDXLNodeArray *query_output_dxlnode_array =
		parse_handler_dxl->GetOutputColumnsDXLArray();
	CDXLNodeArray *cte_producers = parse_handler_dxl->GetCTEProducerDXLArray();
	IMDCacheObjectArray *mdcache_obj_array =
		parse_handler_dxl->GetMdIdCachedObjArray();
	CSystemIdArray *pdrgpsysid = parse_handler_dxl->GetSysidPtrArray();
	CDXLNode *pdxlnPlan = parse_handler_dxl->PdxlnPlan();
	ULLONG plan_id = parse_handler_dxl->GetPlanId();
	ULLONG plan_space_size = parse_handler_dxl->GetPlanSpaceSize();

	if (NULL != pbs)
	{
		pbs->AddRef();
	}

	if (NULL != optimizer_config)
	{
		optimizer_config->AddRef();
	}

	if (NULL != query)
	{
		query->AddRef();
	}

	if (NULL != query_output_dxlnode_array)
	{
		query_output_dxlnode_array->AddRef();
	}

	if (NULL != cte_producers)
	{
		cte_producers->AddRef();
	}

	if (NULL != mdcache_obj_array)
	{
		mdcache_obj_array->AddRef();
	}

	if (NULL != pdrgpsysid)
	{
		pdrgpsysid->AddRef();
	}

	if (NULL != pdxlnPlan)
	{
		pdxlnPlan->AddRef();
	}

	// cleanup
	GPOS_DELETE(parse_handler_dxl);

	return GPOS_NEW(mp) CDXLMinidump(
		pbs, optimizer_config, query, query_output_dxlnode_array, cte_producers,
		pdxlnPlan, mdcache_obj_array, pdrgpsysid, plan_id, plan_space_size);
}


//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::GenerateMinidumpFileName
//
//	@doc:
//		Generate a timestamp-based minidump filename in the provided buffer.
//
//---------------------------------------------------------------------------
void
CMinidumperUtils::GenerateMinidumpFileName(
	CHAR *buf, ULONG length, ULONG ulSessionId, ULONG ulCmdId,
	const CHAR *szMinidumpFileName	// name of minidump file to be created,
									// if NULL, a time-based name is generated
)
{
	if (!gpos::ioutils::PathExists("minidumps"))
	{
		GPOS_TRY
		{
			// create a minidumps folder
			const ULONG ulWrPerms = S_IRUSR | S_IWUSR | S_IXUSR;
			gpos::ioutils::CreateDir("minidumps", ulWrPerms);
		}
		GPOS_CATCH_EX(ex)
		{
			std::cerr << "[OPT]: Failed to create minidumps directory";

			// ignore exceptions during the creation of directory
			GPOS_RESET_EX;
		}
		GPOS_CATCH_END;
	}

	if (NULL == szMinidumpFileName)
	{
		// generate a time-based file name
		CUtils::GenerateFileName(buf, "minidumps/Minidump", "mdp", length,
								 ulSessionId, ulCmdId);
	}
	else
	{
		// use provided file name
		const CHAR *szPrefix = "minidumps/";
		const ULONG ulPrefixLength = clib::Strlen(szPrefix);
		clib::Strncpy(buf, szPrefix, ulPrefixLength);

		// remove directory path before file name, if any
		ULONG ulNameLength = clib::Strlen(szMinidumpFileName);
		ULONG ulNameStart = ulNameLength - 1;
		while (ulNameStart > 0 && szMinidumpFileName[ulNameStart - 1] != '\\' &&
			   szMinidumpFileName[ulNameStart - 1] != '/')
		{
			ulNameStart--;
		}

		ulNameLength = clib::Strlen(szMinidumpFileName + ulNameStart);
		clib::Strncpy(buf + ulPrefixLength, szMinidumpFileName + ulNameStart,
					  ulNameLength);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::Finalize
//
//	@doc:
//		Finalize minidump and dump to file
//
//---------------------------------------------------------------------------
void
CMinidumperUtils::Finalize(CMiniDumperDXL *pmdmp, BOOL fSerializeErrCtx)
{
	CAutoTraceFlag atf1(EtraceSimulateAbort, false);
	CAutoTraceFlag atf2(EtraceSimulateOOM, false);
	CAutoTraceFlag atf3(EtraceSimulateNetError, false);
	CAutoTraceFlag atf4(EtraceSimulateIOError, false);

	if (fSerializeErrCtx)
	{
		CErrorContext *perrctxt = CTask::Self()->ConvertErrCtxt();
		perrctxt->Serialize();
	}

	pmdmp->Finalize();
}

//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::PdxlnExecuteMinidump
//
//	@doc:
//		Load and execute the minidump in the given file
//
//---------------------------------------------------------------------------
CDXLNode *
CMinidumperUtils::PdxlnExecuteMinidump(CMemoryPool *mp, const CHAR *file_name,
									   ULONG ulSegments, ULONG ulSessionId,
									   ULONG ulCmdId,
									   COptimizerConfig *optimizer_config,
									   IConstExprEvaluator *pceeval)
{
	GPOS_ASSERT(NULL != file_name);
	GPOS_ASSERT(NULL != optimizer_config);

	CAutoTimer at("Minidump", true /*fPrint*/);

	// load dump file
	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(mp, file_name);
	GPOS_CHECK_ABORT;

	CDXLNode *pdxlnPlan =
		PdxlnExecuteMinidump(mp, pdxlmd, file_name, ulSegments, ulSessionId,
							 ulCmdId, optimizer_config, pceeval);

	// cleanup
	GPOS_DELETE(pdxlmd);

	return pdxlnPlan;
}


//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::PdxlnExecuteMinidump
//
//	@doc:
//		Execute the given minidump
//
//---------------------------------------------------------------------------
CDXLNode *
CMinidumperUtils::PdxlnExecuteMinidump(CMemoryPool *mp, CDXLMinidump *pdxlmd,
									   const CHAR *file_name, ULONG ulSegments,
									   ULONG ulSessionId, ULONG ulCmdId,
									   COptimizerConfig *optimizer_config,
									   IConstExprEvaluator *pceeval)
{
	GPOS_ASSERT(NULL != file_name);

	// reset metadata ccache
	CMDCache::Reset();

	CMetadataAccessorFactory factory(mp, pdxlmd, file_name);

	CDXLNode *result = CMinidumperUtils::PdxlnExecuteMinidump(
		mp, factory.Pmda(), pdxlmd, file_name, ulSegments, ulSessionId, ulCmdId,
		optimizer_config, pceeval);

	return result;
}


//---------------------------------------------------------------------------
//	@function:
//		CMinidumperUtils::PdxlnExecuteMinidump
//
//	@doc:
//		Execute the given minidump using the given MD accessor
//
//---------------------------------------------------------------------------
CDXLNode *
CMinidumperUtils::PdxlnExecuteMinidump(
	CMemoryPool *mp, CMDAccessor *md_accessor, CDXLMinidump *pdxlmd,
	const CHAR *file_name, ULONG ulSegments, ULONG ulSessionId, ULONG ulCmdId,
	COptimizerConfig *optimizer_config, IConstExprEvaluator *pceeval)
{
	GPOS_ASSERT(NULL != md_accessor);
	GPOS_ASSERT(NULL != pdxlmd->GetQueryDXLRoot() &&
				NULL != pdxlmd->PdrgpdxlnQueryOutput() &&
				NULL != pdxlmd->GetCTEProducerDXLArray() &&
				"No query found in Minidump");
	GPOS_ASSERT(NULL != pdxlmd->GetMdIdCachedObjArray() &&
				NULL != pdxlmd->GetSysidPtrArray() &&
				"No metadata found in Minidump");
	GPOS_ASSERT(NULL != optimizer_config);

	CDXLNode *pdxlnPlan = NULL;
	CAutoTimer at("Minidump", true /*fPrint*/);

	GPOS_CHECK_ABORT;

	// set trace flags
	CBitSet *pbsEnabled = NULL;
	CBitSet *pbsDisabled = NULL;
	SetTraceflags(mp, pdxlmd->Pbs(), &pbsEnabled, &pbsDisabled);

	if (NULL == pceeval)
	{
		// disable constant expression evaluation when running minidump since
		// there no executor to compute the scalar expression
		GPOS_UNSET_TRACE(EopttraceEnableConstantExpressionEvaluation);
	}

	CErrorHandlerStandard errhdl;
	GPOS_TRY_HDL(&errhdl)
	{
		pdxlnPlan = COptimizer::PdxlnOptimize(
			mp, md_accessor, pdxlmd->GetQueryDXLRoot(),
			pdxlmd->PdrgpdxlnQueryOutput(), pdxlmd->GetCTEProducerDXLArray(),
			pceeval, ulSegments, ulSessionId, ulCmdId,
			NULL,  // search_stage_array
			optimizer_config, file_name);
	}
	GPOS_CATCH_EX(ex)
	{
		// reset trace flags
		ResetTraceflags(pbsEnabled, pbsDisabled);

		CRefCount::SafeRelease(pbsEnabled);
		CRefCount::SafeRelease(pbsDisabled);

		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	// reset trace flags
	ResetTraceflags(pbsEnabled, pbsDisabled);

	// clean up
	CRefCount::SafeRelease(pbsEnabled);
	CRefCount::SafeRelease(pbsDisabled);

	GPOS_CHECK_ABORT;

	return pdxlnPlan;
}

// EOF
