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

#include "gpopt/utils/COptTasks.h"

#include "gpos/base.h"
#include "gpos/error/CException.h"

#include "gpopt/config/CConfigParamMapping.h"
#include "gpopt/engine/CHint.h"
#include "gpopt/eval/CConstExprEvaluatorDXL.h"
#include "gpopt/relcache/CMDProviderRelcache.h"
#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "gpopt/translate/CTranslatorQueryToDXL.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/utils/CConstExprEvaluatorProxy.h"
#include "gpopt/utils/gpdbdefs.h"

#include "cdb/cdbvars.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#undef setstate

#include "gpos/_api.h"
#include "gpos/common/CAutoP.h"
#include "gpos/io/COstreamFile.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "gpdbcost/CCostModelGPDB.h"
#include "gpdbcost/CCostModelGPDBLegacy.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/engine/CCTEConfig.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/exception.h"
#include "gpopt/gpdbwrappers.h"
#include "gpopt/mdcache/CAutoMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/optimizer/COptimizer.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "gpopt/xforms/CXformFactory.h"
#include "naucrates/base/CQueryToDXLResult.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/parser/CParseHandlerDXL.h"
#include "naucrates/exception.h"
#include "naucrates/init.h"
#include "naucrates/md/CMDIdCast.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/CMDIdScCmp.h"
#include "naucrates/md/CSystemId.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDRelStats.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;
using namespace gpdbcost;

// size of error buffer
#define GPOPT_ERROR_BUFFER_SIZE 10 * 1024 * 1024

// definition of default AutoMemoryPool
#define AUTO_MEM_POOL(amp) CAutoMemoryPool amp(CAutoMemoryPool::ElcExc)

// OID for Window RANK function
#define F_WINDOW_RANK_OID 7001

// default id for the source system
const CSystemId default_sysid(IMDId::EmdidGPDB, GPOS_WSZ_STR_LENGTH("GPDB"));


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::SOptContext
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
SOptContext::SOptContext()
	: m_query_dxl(NULL),
	  m_query(NULL),
	  m_plan_dxl(NULL),
	  m_plan_stmt(NULL),
	  m_should_generate_plan_stmt(false),
	  m_should_serialize_plan_dxl(false),
	  m_is_unexpected_failure(false),
	  m_error_msg(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		SOptContext::HandleError
//
//	@doc:
//		If there is an error, throw GPOS_EXCEPTION to abort plan generation.
//		Calling elog::ERROR would result in longjump and hence a memory leak.
//---------------------------------------------------------------------------
void
SOptContext::HandleError(BOOL *has_unexpected_failure)
{
	*has_unexpected_failure = m_is_unexpected_failure;
	if (NULL != m_error_msg)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiOptimizerError);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::Free
//
//	@doc:
//		Free all members except those pointed to by either input or
//		output
//
//---------------------------------------------------------------------------
void
SOptContext::Free(SOptContext::EPin input, SOptContext::EPin output)
{
	if (NULL != m_query_dxl && epinQueryDXL != input && epinQueryDXL != output)
	{
		gpdb::GPDBFree(m_query_dxl);
	}

	if (NULL != m_query && epinQuery != input && epinQuery != output)
	{
		gpdb::GPDBFree(m_query);
	}

	if (NULL != m_plan_dxl && epinPlanDXL != input && epinPlanDXL != output)
	{
		gpdb::GPDBFree(m_plan_dxl);
	}

	if (NULL != m_plan_stmt && epinPlStmt != input && epinPlStmt != output)
	{
		gpdb::GPDBFree(m_plan_stmt);
	}

	if (NULL != m_error_msg && epinErrorMsg != input && epinErrorMsg != output)
	{
		gpdb::GPDBFree(m_error_msg);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		SOptContext::CloneErrorMsg
//
//	@doc:
//		Clone m_error_msg to given memory context. Return NULL if there is no
//		error message.
//
//---------------------------------------------------------------------------
CHAR *
SOptContext::CloneErrorMsg(MemoryContext context)
{
	if (NULL == context || NULL == m_error_msg)
	{
		return NULL;
	}
	return gpdb::MemCtxtStrdup(context, m_error_msg);
}


//---------------------------------------------------------------------------
//	@function:
//		SOptContext::Cast
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
SOptContext *
SOptContext::Cast(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	return reinterpret_cast<SOptContext *>(ptr);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SContextRelcacheToDXL::SContextRelcacheToDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
COptTasks::SContextRelcacheToDXL::SContextRelcacheToDXL(List *oid_list,
														ULONG cmp_type,
														const char *filename)
	: m_oid_list(oid_list), m_cmp_type(cmp_type), m_filename(filename)
{
	GPOS_ASSERT(NULL != oid_list);
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SContextRelcacheToDXL::RelcacheConvert
//
//	@doc:
//		Casting function
//
//---------------------------------------------------------------------------
COptTasks::SContextRelcacheToDXL *
COptTasks::SContextRelcacheToDXL::RelcacheConvert(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	return reinterpret_cast<SContextRelcacheToDXL *>(ptr);
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
COptTasks::SEvalExprContext::PevalctxtConvert(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	return reinterpret_cast<SEvalExprContext *>(ptr);
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
COptTasks::SOptimizeMinidumpContext::Cast(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	return reinterpret_cast<SOptimizeMinidumpContext *>(ptr);
}

//---------------------------------------------------------------------------
//	@function:
//		SzAllocate
//
//	@doc:
//		Allocate string buffer with protection against OOM
//
//---------------------------------------------------------------------------
CHAR *
COptTasks::SzAllocate(CMemoryPool *pmp, ULONG ulSize)
{
	CHAR *sz = NULL;
	GPOS_TRY
	{
		// allocation of string buffer may happen outside gpos_exec() function,
		// we must guard against system OOM here
#ifdef FAULT_INJECTOR
		gpdb::InjectFaultInOptTasks(OptTaskAllocateStringBuffer);
#endif	// FAULT_INJECTOR

		if (NULL == pmp)
		{
			sz = (CHAR *) gpdb::GPDBAlloc(ulSize);
		}
		else
		{
			sz = GPOS_NEW_ARRAY(pmp, CHAR, ulSize);
		}
	}
	GPOS_CATCH_EX(ex)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiNoAvailableMemory);
	}
	GPOS_CATCH_END;

	return sz;
}


//---------------------------------------------------------------------------
//	@function:
//		CreateMultiByteCharStringFromWCString
//
//	@doc:
//		Return regular string from wide-character string
//
//---------------------------------------------------------------------------
CHAR *
COptTasks::CreateMultiByteCharStringFromWCString(const WCHAR *wcstr)
{
	GPOS_ASSERT(NULL != wcstr);

	const ULONG input_len = GPOS_WSZ_LENGTH(wcstr);
	const ULONG wchar_size = GPOS_SIZEOF(WCHAR);
	const ULONG max_len = (input_len + 1) * wchar_size;

	CHAR *str = SzAllocate(NULL, max_len);

	gpos::clib::Wcstombs(str, const_cast<WCHAR *>(wcstr), max_len);
	str[max_len - 1] = '\0';

	return str;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToDXLFromMDCast
//
//	@doc:
//		Task that dumps the relcache info for a cast object into DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToDXLFromMDCast(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SContextRelcacheToDXL *relcache_ctxt =
		SContextRelcacheToDXL::RelcacheConvert(ptr);
	GPOS_ASSERT(NULL != relcache_ctxt);

	GPOS_ASSERT(2 == gpdb::ListLength(relcache_ctxt->m_oid_list));
	Oid src_oid = gpdb::ListNthOid(relcache_ctxt->m_oid_list, 0);
	Oid dest_oid = gpdb::ListNthOid(relcache_ctxt->m_oid_list, 1);

	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	IMDCacheObjectArray *mdcache_obj_array =
		GPOS_NEW(mp) IMDCacheObjectArray(mp);

	IMDId *cast = GPOS_NEW(mp) CMDIdCast(GPOS_NEW(mp) CMDIdGPDB(src_oid),
										 GPOS_NEW(mp) CMDIdGPDB(dest_oid));

	// relcache MD provider
	CMDProviderRelcache *relcache_provider =
		GPOS_NEW(mp) CMDProviderRelcache(mp);

	{
		CAutoMDAccessor md_accessor(mp, relcache_provider, default_sysid);

		IMDCacheObject *md_obj = CTranslatorRelcacheToDXL::RetrieveObject(
			mp, md_accessor.Pmda(), cast);
		GPOS_ASSERT(NULL != md_obj);

		mdcache_obj_array->Append(md_obj);
		cast->Release();

		CWStringDynamic wcstr(mp);
		COstreamString oss(&wcstr);

		CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, oss,
									 true /*serialize_header_footer*/,
									 true /*indentation*/);
		CHAR *str = CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());
		GPOS_ASSERT(NULL != str);

		relcache_ctxt->m_dxl = str;

		// cleanup
		mdcache_obj_array->Release();
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToDXLFromMDScalarCmp
//
//	@doc:
//		Task that dumps the relcache info for a scalar comparison object into DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToDXLFromMDScalarCmp(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SContextRelcacheToDXL *relcache_ctxt =
		SContextRelcacheToDXL::RelcacheConvert(ptr);
	GPOS_ASSERT(NULL != relcache_ctxt);
	GPOS_ASSERT(CmptOther > relcache_ctxt->m_cmp_type &&
				"Incorrect comparison type specified");

	GPOS_ASSERT(2 == gpdb::ListLength(relcache_ctxt->m_oid_list));
	Oid left_oid = gpdb::ListNthOid(relcache_ctxt->m_oid_list, 0);
	Oid right_oid = gpdb::ListNthOid(relcache_ctxt->m_oid_list, 1);
	CmpType cmpt = (CmpType) relcache_ctxt->m_cmp_type;

	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	CMemoryPool *mp = amp.Pmp();

	IMDCacheObjectArray *mdcache_obj_array =
		GPOS_NEW(mp) IMDCacheObjectArray(mp);

	IMDId *scalar_cmp = GPOS_NEW(mp) CMDIdScCmp(
		GPOS_NEW(mp) CMDIdGPDB(left_oid), GPOS_NEW(mp) CMDIdGPDB(right_oid),
		CTranslatorRelcacheToDXL::ParseCmpType(cmpt));

	// relcache MD provider
	CMDProviderRelcache *relcache_provider =
		GPOS_NEW(mp) CMDProviderRelcache(mp);

	{
		CAutoMDAccessor md_accessor(mp, relcache_provider, default_sysid);

		IMDCacheObject *md_obj = CTranslatorRelcacheToDXL::RetrieveObject(
			mp, md_accessor.Pmda(), scalar_cmp);
		GPOS_ASSERT(NULL != md_obj);

		mdcache_obj_array->Append(md_obj);
		scalar_cmp->Release();

		CWStringDynamic wcstr(mp);
		COstreamString oss(&wcstr);

		CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, oss,
									 true /*serialize_header_footer*/,
									 true /*indentation*/);
		CHAR *str = CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());
		GPOS_ASSERT(NULL != str);

		relcache_ctxt->m_dxl = str;

		// cleanup
		mdcache_obj_array->Release();
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
COptTasks::Execute(void *(*func)(void *), void *func_arg)
{
	Assert(func);

	CHAR *err_buf = (CHAR *) palloc(GPOPT_ERROR_BUFFER_SIZE);
	err_buf[0] = '\0';

	// initialize DXL support
	InitDXL();

	bool abort_flag = false;

	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);

	gpos_exec_params params;
	params.func = func;
	params.arg = func_arg;
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
		LogExceptionMessageAndDelete(err_buf, ex.SeverityLevel());
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	LogExceptionMessageAndDelete(err_buf);
}

void
COptTasks::LogExceptionMessageAndDelete(CHAR *err_buf, ULONG severity_level)
{
	if ('\0' != err_buf[0])
	{
		int gpdb_severity_level;

		if (severity_level == CException::ExsevDebug1)
			gpdb_severity_level = DEBUG1;
		else
			gpdb_severity_level = LOG;

		elog(gpdb_severity_level, "%s",
			 CreateMultiByteCharStringFromWCString((WCHAR *) err_buf));
	}

	pfree(err_buf);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToDXLFromQueryTask
//
//	@doc:
//		task that does the translation from query to XML
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToDXLFromQueryTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SOptContext *opt_ctxt = SOptContext::Cast(ptr);

	GPOS_ASSERT(NULL != opt_ctxt->m_query);
	GPOS_ASSERT(NULL == opt_ctxt->m_query_dxl);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	// ColId generator
	CIdGenerator *colid_generator =
		GPOS_NEW(mp) CIdGenerator(GPDXL_COL_ID_START);
	CIdGenerator *cteid_generator =
		GPOS_NEW(mp) CIdGenerator(GPDXL_CTE_ID_START);

	// map that stores gpdb att to optimizer col mapping
	CMappingVarColId *var_colid_mapping = GPOS_NEW(mp) CMappingVarColId(mp);

	// relcache MD provider
	CMDProviderRelcache *relcache_provider =
		GPOS_NEW(mp) CMDProviderRelcache(mp);

	{
		CAutoMDAccessor md_accessor(mp, relcache_provider, default_sysid);

		CAutoP<CTranslatorQueryToDXL> query_to_dxl_translator;
		query_to_dxl_translator = CTranslatorQueryToDXL::QueryToDXLInstance(
			mp, md_accessor.Pmda(), colid_generator, cteid_generator,
			var_colid_mapping, (Query *) opt_ctxt->m_query, 0 /* query_level */
		);
		CDXLNode *query_dxl = query_to_dxl_translator->TranslateQueryToDXL();
		CDXLNodeArray *query_output_dxlnode_array =
			query_to_dxl_translator->GetQueryOutputCols();
		CDXLNodeArray *cte_dxlnode_array = query_to_dxl_translator->GetCTEs();
		GPOS_ASSERT(NULL != query_output_dxlnode_array);

		GPOS_DELETE(colid_generator);
		GPOS_DELETE(cteid_generator);

		CWStringDynamic wcstr(mp);
		COstreamString oss(&wcstr);

		CDXLUtils::SerializeQuery(mp, oss, query_dxl,
								  query_output_dxlnode_array, cte_dxlnode_array,
								  true,	 // serialize_header_footer
								  true	 // indentation
		);
		opt_ctxt->m_query_dxl =
			CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());

		// clean up
		query_dxl->Release();
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToPlanStmtFromDXL
//
//	@doc:
//		Translate a DXL tree into a planned statement
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::ConvertToPlanStmtFromDXL(CMemoryPool *mp, CMDAccessor *md_accessor,
									const CDXLNode *dxlnode, bool can_set_tag)
{
	GPOS_ASSERT(NULL != md_accessor);
	GPOS_ASSERT(NULL != dxlnode);

	CIdGenerator plan_id_generator(1 /* ulStartId */);
	CIdGenerator motion_id_generator(1 /* ulStartId */);
	CIdGenerator param_id_generator(0 /* ulStartId */);

	List *table_list = NULL;
	List *subplans_list = NULL;

	CContextDXLToPlStmt dxl_to_plan_stmt_ctxt(
		mp, &plan_id_generator, &motion_id_generator, &param_id_generator,
		&table_list, &subplans_list);

	// translate DXL -> PlannedStmt
	CTranslatorDXLToPlStmt dxl_to_plan_stmt_translator(
		mp, md_accessor, &dxl_to_plan_stmt_ctxt, gpdb::GetGPSegmentCount());
	return dxl_to_plan_stmt_translator.GetPlannedStmtFromDXL(dxlnode,
															 can_set_tag);
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::LoadSearchStrategy
//
//	@doc:
//		Load search strategy from given file
//
//---------------------------------------------------------------------------
CSearchStageArray *
COptTasks::LoadSearchStrategy(CMemoryPool *mp, char *path)
{
	CSearchStageArray *search_strategy_arr = NULL;
	CParseHandlerDXL *dxl_parse_handler = NULL;

	GPOS_TRY
	{
		if (NULL != path)
		{
			dxl_parse_handler =
				CDXLUtils::GetParseHandlerForDXLFile(mp, path, NULL);
			if (NULL != dxl_parse_handler)
			{
				elog(DEBUG2, "\n[OPT]: Using search strategy in (%s)", path);

				search_strategy_arr = dxl_parse_handler->GetSearchStageArray();
				search_strategy_arr->AddRef();
			}
		}
	}
	GPOS_CATCH_EX(ex)
	{
		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			GPOS_RETHROW(ex);
		}
		elog(DEBUG2, "\n[OPT]: Using default search strategy");
		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;

	GPOS_DELETE(dxl_parse_handler);

	return search_strategy_arr;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::CreateOptimizerConfig
//
//	@doc:
//		Create the optimizer configuration
//
//---------------------------------------------------------------------------
COptimizerConfig *
COptTasks::CreateOptimizerConfig(CMemoryPool *mp, ICostModel *cost_model)
{
	// get chosen plan number, cost threshold
	ULLONG plan_id = (ULLONG) optimizer_plan_id;
	ULLONG num_samples = (ULLONG) optimizer_samples_number;
	DOUBLE cost_threshold = (DOUBLE) optimizer_cost_threshold;

	DOUBLE damping_factor_filter = (DOUBLE) optimizer_damping_factor_filter;
	DOUBLE damping_factor_join = (DOUBLE) optimizer_damping_factor_join;
	DOUBLE damping_factor_groupby = (DOUBLE) optimizer_damping_factor_groupby;

	ULONG cte_inlining_cutoff = (ULONG) optimizer_cte_inlining_bound;
	ULONG join_arity_for_associativity_commutativity =
		(ULONG) optimizer_join_arity_for_associativity_commutativity;
	ULONG array_expansion_threshold =
		(ULONG) optimizer_array_expansion_threshold;
	ULONG join_order_threshold = (ULONG) optimizer_join_order_threshold;
	ULONG broadcast_threshold = (ULONG) optimizer_penalize_broadcast_threshold;
	ULONG xform_bind_threshold = (ULONG) optimizer_xform_bind_threshold;

	return GPOS_NEW(mp) COptimizerConfig(
		GPOS_NEW(mp)
			CEnumeratorConfig(mp, plan_id, num_samples, cost_threshold),
		GPOS_NEW(mp)
			CStatisticsConfig(mp, damping_factor_filter, damping_factor_join,
							  damping_factor_groupby),
		GPOS_NEW(mp) CCTEConfig(cte_inlining_cutoff), cost_model,
		GPOS_NEW(mp)
			CHint(gpos::int_max /* optimizer_parts_to_force_sort_on_insert */,
				  join_arity_for_associativity_commutativity,
				  array_expansion_threshold, join_order_threshold,
				  broadcast_threshold,
				  false, /* don't create Assert nodes for constraints, we'll
								      * enforce them ourselves in the executor */
				  PUSH_GROUP_BY_BELOW_SETOP_THRESHOLD, xform_bind_threshold),
		GPOS_NEW(mp) CWindowOids(OID(F_WINDOW_DUMMY), OID(F_WINDOW_RANK_OID)));
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
COptTasks::SetCostModelParams(ICostModel *cost_model)
{
	GPOS_ASSERT(NULL != cost_model);

	if (optimizer_nestloop_factor > 1.0)
	{
		// change NLJ cost factor
		ICostModelParams::SCostParam *cost_param = NULL;
		if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model ||
			OPTIMIZER_GPDB_EXPERIMENTAL == optimizer_cost_model)
		{
			cost_param = cost_model->GetCostModelParams()->PcpLookup(
				CCostModelParamsGPDB::EcpNLJFactor);
		}
		else
		{
			cost_param = cost_model->GetCostModelParams()->PcpLookup(
				CCostModelParamsGPDBLegacy::EcpNLJFactor);
		}
		CDouble nlj_factor(optimizer_nestloop_factor);
		cost_model->GetCostModelParams()->SetParam(
			cost_param->Id(), nlj_factor, nlj_factor - 0.5, nlj_factor + 0.5);
	}

	if (optimizer_sort_factor > 1.0 || optimizer_sort_factor < 1.0)
	{
		// change sort cost factor
		ICostModelParams::SCostParam *cost_param = NULL;
		if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model ||
			OPTIMIZER_GPDB_EXPERIMENTAL == optimizer_cost_model)
		{
			cost_param = cost_model->GetCostModelParams()->PcpLookup(
				CCostModelParamsGPDB::EcpSortTupWidthCostUnit);

			CDouble sort_factor(optimizer_sort_factor);
			cost_model->GetCostModelParams()->SetParam(
				cost_param->Id(), cost_param->Get() * optimizer_sort_factor,
				cost_param->GetLowerBoundVal() * optimizer_sort_factor,
				cost_param->GetUpperBoundVal() * optimizer_sort_factor);
		}
	}
}


//---------------------------------------------------------------------------
//      @function:
//			COptTasks::GetCostModel
//
//      @doc:
//			Generate an instance of optimizer cost model
//
//---------------------------------------------------------------------------
ICostModel *
COptTasks::GetCostModel(CMemoryPool *mp, ULONG num_segments)
{
	ICostModel *cost_model = NULL;
	if (OPTIMIZER_GPDB_CALIBRATED == optimizer_cost_model ||
		OPTIMIZER_GPDB_EXPERIMENTAL == optimizer_cost_model)
	{
		cost_model = GPOS_NEW(mp) CCostModelGPDB(mp, num_segments);
	}
	else
	{
		cost_model = GPOS_NEW(mp) CCostModelGPDBLegacy(mp, num_segments);
	}

	SetCostModelParams(cost_model);

	return cost_model;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::OptimizeTask
//
//	@doc:
//		task that does the optimizes query to physical DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::OptimizeTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);
	SOptContext *opt_ctxt = SOptContext::Cast(ptr);

	GPOS_ASSERT(NULL != opt_ctxt->m_query);
	GPOS_ASSERT(NULL == opt_ctxt->m_plan_dxl);
	GPOS_ASSERT(NULL == opt_ctxt->m_plan_stmt);

	// initially assume no unexpected failure
	opt_ctxt->m_is_unexpected_failure = false;

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	// Does the metadatacache need to be reset?
	//
	// On the first call, before the cache has been initialized, we
	// don't care about the return value of MDCacheNeedsReset(). But
	// we need to call it anyway, to give it a chance to initialize
	// the invalidation mechanism.
	bool reset_mdcache = gpdb::MDCacheNeedsReset();

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
	else if (CMDCache::ULLGetCacheQuota() !=
			 (ULLONG) optimizer_mdcache_size * 1024L)
	{
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}


	// load search strategy
	CSearchStageArray *search_strategy_arr =
		LoadSearchStrategy(mp, optimizer_search_strategy_path);

	CBitSet *trace_flags = NULL;
	CBitSet *enabled_trace_flags = NULL;
	CBitSet *disabled_trace_flags = NULL;
	CDXLNode *plan_dxl = NULL;

	IMdIdArray *col_stats = NULL;
	MdidHashSet *rel_stats = NULL;

	GPOS_TRY
	{
		// set trace flags
		trace_flags = CConfigParamMapping::PackConfigParamInBitset(
			mp, CXform::ExfSentinel);
		SetTraceflags(mp, trace_flags, &enabled_trace_flags,
					  &disabled_trace_flags);

		// set up relcache MD provider
		CMDProviderRelcache *relcache_provider =
			GPOS_NEW(mp) CMDProviderRelcache(mp);

		{
			// scope for MD accessor
			CMDAccessor mda(mp, CMDCache::Pcache(), default_sysid,
							relcache_provider);

			// ColId generator
			CIdGenerator colid_generator(GPDXL_COL_ID_START);
			CIdGenerator cteid_generator(GPDXL_CTE_ID_START);

			// map that stores gpdb att to optimizer col mapping
			CMappingVarColId *var_colid_mapping =
				GPOS_NEW(mp) CMappingVarColId(mp);

			ULONG num_segments = gpdb::GetGPSegmentCount();
			ULONG num_segments_for_costing = optimizer_segments;
			if (0 == num_segments_for_costing)
			{
				num_segments_for_costing = num_segments;
			}

			CAutoP<CTranslatorQueryToDXL> query_to_dxl_translator;
			query_to_dxl_translator = CTranslatorQueryToDXL::QueryToDXLInstance(
				mp, &mda, &colid_generator, &cteid_generator, var_colid_mapping,
				(Query *) opt_ctxt->m_query, 0 /* query_level */
			);

			ICostModel *cost_model = GetCostModel(mp, num_segments_for_costing);
			COptimizerConfig *optimizer_config =
				CreateOptimizerConfig(mp, cost_model);
			CConstExprEvaluatorProxy expr_eval_proxy(mp, &mda);
			IConstExprEvaluator *expr_evaluator =
				GPOS_NEW(mp) CConstExprEvaluatorDXL(mp, &mda, &expr_eval_proxy);

			CDXLNode *query_dxl =
				query_to_dxl_translator->TranslateQueryToDXL();
			CDXLNodeArray *query_output_dxlnode_array =
				query_to_dxl_translator->GetQueryOutputCols();
			CDXLNodeArray *cte_dxlnode_array =
				query_to_dxl_translator->GetCTEs();
			GPOS_ASSERT(NULL != query_output_dxlnode_array);

			BOOL is_master_only =
				!optimizer_enable_motions ||
				(!optimizer_enable_motions_masteronly_queries &&
				 !query_to_dxl_translator->HasDistributedTables());
			CAutoTraceFlag atf(EopttraceDisableMotions, is_master_only);

			plan_dxl = COptimizer::PdxlnOptimize(
				mp, &mda, query_dxl, query_output_dxlnode_array,
				cte_dxlnode_array, expr_evaluator, num_segments, gp_session_id,
				gp_command_count, search_strategy_arr, optimizer_config);

			if (opt_ctxt->m_should_serialize_plan_dxl)
			{
				// serialize DXL to xml
				CWStringDynamic plan_str(mp);
				COstreamString oss(&plan_str);
				CDXLUtils::SerializePlan(
					mp, oss, plan_dxl,
					optimizer_config->GetEnumeratorCfg()->GetPlanId(),
					optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize(),
					true /*serialize_header_footer*/, true /*indentation*/);
				opt_ctxt->m_plan_dxl =
					CreateMultiByteCharStringFromWCString(plan_str.GetBuffer());
			}

			// translate DXL->PlStmt only when needed
			if (opt_ctxt->m_should_generate_plan_stmt)
			{
				// always use opt_ctxt->m_query->can_set_tag as the query_to_dxl_translator->Pquery() is a mutated Query object
				// that may not have the correct can_set_tag
				opt_ctxt->m_plan_stmt =
					(PlannedStmt *) gpdb::CopyObject(ConvertToPlanStmtFromDXL(
						mp, &mda, plan_dxl, opt_ctxt->m_query->canSetTag));
			}

			CStatisticsConfig *stats_conf = optimizer_config->GetStatsConf();
			col_stats = GPOS_NEW(mp) IMdIdArray(mp);
			stats_conf->CollectMissingStatsColumns(col_stats);

			rel_stats = GPOS_NEW(mp) MdidHashSet(mp);
			PrintMissingStatsWarning(mp, &mda, col_stats, rel_stats);

			rel_stats->Release();
			col_stats->Release();

			expr_evaluator->Release();
			query_dxl->Release();
			optimizer_config->Release();
			plan_dxl->Release();
		}
	}
	GPOS_CATCH_EX(ex)
	{
		ResetTraceflags(enabled_trace_flags, disabled_trace_flags);
		CRefCount::SafeRelease(rel_stats);
		CRefCount::SafeRelease(col_stats);
		CRefCount::SafeRelease(enabled_trace_flags);
		CRefCount::SafeRelease(disabled_trace_flags);
		CRefCount::SafeRelease(trace_flags);
		CRefCount::SafeRelease(plan_dxl);
		CMDCache::Shutdown();

		if (GPOS_MATCH_EX(ex, gpdxl::ExmaGPDB, gpdxl::ExmiGPDBError))
		{
			elog(DEBUG1,
				 "GPDB Exception. Please check log for more information.");
		}
		else if (ShouldErrorOut(ex))
		{
			IErrorContext *errctxt = CTask::Self()->GetErrCtxt();
			opt_ctxt->m_error_msg =
				CreateMultiByteCharStringFromWCString(errctxt->GetErrorMsg());
		}
		else
		{
			opt_ctxt->m_is_unexpected_failure = IsLoggableFailure(ex);
		}
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	// cleanup
	ResetTraceflags(enabled_trace_flags, disabled_trace_flags);
	CRefCount::SafeRelease(enabled_trace_flags);
	CRefCount::SafeRelease(disabled_trace_flags);
	CRefCount::SafeRelease(trace_flags);
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
COptTasks::PrintMissingStatsWarning(CMemoryPool *mp, CMDAccessor *md_accessor,
									IMdIdArray *col_stats,
									MdidHashSet *rel_stats)
{
	GPOS_ASSERT(NULL != md_accessor);
	GPOS_ASSERT(NULL != col_stats);
	GPOS_ASSERT(NULL != rel_stats);

	CWStringDynamic wcstr(mp);
	COstreamString oss(&wcstr);

	const ULONG num_missing_col_stats = col_stats->Size();
	for (ULONG ul = 0; ul < num_missing_col_stats; ul++)
	{
		IMDId *mdid = (*col_stats)[ul];
		CMDIdColStats *mdid_col_stats = CMDIdColStats::CastMdid(mdid);

		IMDId *rel_mdid = mdid_col_stats->GetRelMdId();
		const ULONG pos = mdid_col_stats->Position();
		const IMDRelation *rel = md_accessor->RetrieveRel(rel_mdid);

		if (IMDRelation::ErelstorageExternal != rel->RetrieveRelStorageType())
		{
			if (!rel_stats->Contains(rel_mdid))
			{
				if (0 != ul)
				{
					oss << ", ";
				}

				rel_mdid->AddRef();
				rel_stats->Insert(rel_mdid);
				oss << rel->Mdname().GetMDName()->GetBuffer();
			}

			CMDName mdname = rel->GetMdCol(pos)->Mdname();

			char msgbuf[NAMEDATALEN * 2 + 100];
			snprintf(msgbuf, sizeof(msgbuf),
					 "Missing statistics for column: %s.%s",
					 CreateMultiByteCharStringFromWCString(
						 rel->Mdname().GetMDName()->GetBuffer()),
					 CreateMultiByteCharStringFromWCString(
						 mdname.GetMDName()->GetBuffer()));
			GpdbEreport(ERRCODE_SUCCESSFUL_COMPLETION, LOG, msgbuf, NULL);
		}
	}

	if (0 < rel_stats->Size())
	{
		int length = NAMEDATALEN * rel_stats->Size() + 200;
		char msgbuf[length];
		snprintf(
			msgbuf, sizeof(msgbuf),
			"One or more columns in the following table(s) do not have statistics: %s",
			CreateMultiByteCharStringFromWCString(wcstr.GetBuffer()));
		GpdbEreport(
			ERRCODE_SUCCESSFUL_COMPLETION, NOTICE, msgbuf,
			"For non-partitioned tables, run analyze <table_name>(<column_list>)."
			" For partitioned tables, run analyze rootpartition <table_name>(<column_list>)."
			" See log for columns missing statistics.");
	}
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::OptimizeMinidumpTask
//
//	@doc:
//		Task that loads and optimizes a minidump and returns the result as string-serialized DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::OptimizeMinidumpTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SOptimizeMinidumpContext *minidump_ctxt =
		SOptimizeMinidumpContext::Cast(ptr);
	GPOS_ASSERT(NULL != minidump_ctxt->m_szFileName);
	GPOS_ASSERT(NULL == minidump_ctxt->m_dxl_result);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	ULONG num_segments = gpdb::GetGPSegmentCount();
	ULONG num_segments_for_costing = optimizer_segments;
	if (0 == num_segments_for_costing)
	{
		num_segments_for_costing = num_segments;
	}

	ICostModel *cost_model = GetCostModel(mp, num_segments_for_costing);
	COptimizerConfig *optimizer_config = CreateOptimizerConfig(mp, cost_model);
	CDXLNode *result_dxl = NULL;

	GPOS_TRY
	{
		result_dxl = CMinidumperUtils::PdxlnExecuteMinidump(
			mp, minidump_ctxt->m_szFileName, num_segments, gp_session_id,
			gp_command_count, optimizer_config);
	}
	GPOS_CATCH_EX(ex)
	{
		CRefCount::SafeRelease(result_dxl);
		CRefCount::SafeRelease(optimizer_config);
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	CWStringDynamic wcstr(mp);
	COstreamString oss(&wcstr);
	CDXLUtils::SerializePlan(mp, oss, result_dxl,
							 0,		// plan_id
							 0,		// plan_space_size
							 true,	// serialize_header_footer
							 true	// indentation
	);
	minidump_ctxt->m_dxl_result =
		CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());
	CRefCount::SafeRelease(result_dxl);
	optimizer_config->Release();

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToPlanStmtFromDXLTask
//
//	@doc:
//		task that does the translation from xml to dxl to planned_stmt
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToPlanStmtFromDXLTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SOptContext *opt_ctxt = SOptContext::Cast(ptr);

	GPOS_ASSERT(NULL == opt_ctxt->m_query);
	GPOS_ASSERT(NULL != opt_ctxt->m_plan_dxl);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic wcstr(mp);
	COstreamString oss(&wcstr);

	ULLONG plan_id = 0;
	ULLONG plan_space_size = 0;
	CDXLNode *original_plan_dxl = CDXLUtils::GetPlanDXLNode(
		mp, opt_ctxt->m_plan_dxl, NULL /*XSD location*/, &plan_id,
		&plan_space_size);

	CIdGenerator plan_id_generator(1);
	CIdGenerator motion_id_generator(1);
	CIdGenerator param_id_generator(0);

	List *table_list = NULL;
	List *subplans_list = NULL;

	CContextDXLToPlStmt dxl_to_plan_stmt_ctxt(
		mp, &plan_id_generator, &motion_id_generator, &param_id_generator,
		&table_list, &subplans_list);

	// relcache MD provider
	CMDProviderRelcache *relcache_provider =
		GPOS_NEW(mp) CMDProviderRelcache(mp);

	{
		CAutoMDAccessor md_accessor(mp, relcache_provider, default_sysid);

		// translate DXL -> PlannedStmt
		CTranslatorDXLToPlStmt dxl_to_plan_stmt_translator(
			mp, md_accessor.Pmda(), &dxl_to_plan_stmt_ctxt,
			gpdb::GetGPSegmentCount());
		PlannedStmt *plan_stmt =
			dxl_to_plan_stmt_translator.GetPlannedStmtFromDXL(
				original_plan_dxl, opt_ctxt->m_query->canSetTag);
		if (optimizer_print_plan)
		{
			elog(NOTICE, "Plstmt: %s", gpdb::NodeToString(plan_stmt));
		}

		GPOS_ASSERT(NULL != plan_stmt);
		GPOS_ASSERT(NULL != CurrentMemoryContext);

		opt_ctxt->m_plan_stmt = (PlannedStmt *) gpdb::CopyObject(plan_stmt);
	}

	// cleanup
	original_plan_dxl->Release();

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToDXLFromMDObjsTask
//
//	@doc:
//		task that does dumps the relcache info for an object into DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToDXLFromMDObjsTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SContextRelcacheToDXL *relcache_ctxt =
		SContextRelcacheToDXL::RelcacheConvert(ptr);
	GPOS_ASSERT(NULL != relcache_ctxt);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	IMDCacheObjectArray *mdcache_obj_array =
		GPOS_NEW(mp) IMDCacheObjectArray(mp, 1024, 1024);

	// relcache MD provider
	CMDProviderRelcache *md_provider = GPOS_NEW(mp) CMDProviderRelcache(mp);
	{
		CAutoMDAccessor md_accessor(mp, md_provider, default_sysid);
		ListCell *lc = NULL;
		ForEach(lc, relcache_ctxt->m_oid_list)
		{
			Oid oid = lfirst_oid(lc);
			// get object from relcache
			CMDIdGPDB *mdid =
				GPOS_NEW(mp) CMDIdGPDB(oid, 1 /* major */, 0 /* minor */);

			IMDCacheObject *mdobj = CTranslatorRelcacheToDXL::RetrieveObject(
				mp, md_accessor.Pmda(), mdid);
			GPOS_ASSERT(NULL != mdobj);

			mdcache_obj_array->Append(mdobj);
			mdid->Release();
		}

		if (relcache_ctxt->m_filename)
		{
			COstreamFile cofs(relcache_ctxt->m_filename);

			CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, cofs,
										 true /*serialize_header_footer*/,
										 true /*indentation*/);
		}
		else
		{
			CWStringDynamic wcstr(mp);
			COstreamString oss(&wcstr);

			CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, oss,
										 true /*serialize_header_footer*/,
										 true /*indentation*/);

			CHAR *str =
				CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());

			GPOS_ASSERT(NULL != str);

			relcache_ctxt->m_dxl = str;
		}
	}
	// cleanup
	mdcache_obj_array->Release();

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToDXLFromRelStatsTask
//
//	@doc:
//		task that dumps relstats info for a table in DXL
//
//---------------------------------------------------------------------------
void *
COptTasks::ConvertToDXLFromRelStatsTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);

	SContextRelcacheToDXL *relcache_ctxt =
		SContextRelcacheToDXL::RelcacheConvert(ptr);
	GPOS_ASSERT(NULL != relcache_ctxt);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	// relcache MD provider
	CMDProviderRelcache *relcache_provider =
		GPOS_NEW(mp) CMDProviderRelcache(mp);
	CAutoMDAccessor md_accessor(mp, relcache_provider, default_sysid);
	ICostModel *cost_model = GetCostModel(mp, gpdb::GetGPSegmentCount());
	CAutoOptCtxt aoc(mp, md_accessor.Pmda(), NULL /*expr_evaluator*/,
					 cost_model);

	IMDCacheObjectArray *mdcache_obj_array =
		GPOS_NEW(mp) IMDCacheObjectArray(mp);

	ListCell *lc = NULL;
	ForEach(lc, relcache_ctxt->m_oid_list)
	{
		Oid relation_oid = lfirst_oid(lc);

		// get object from relcache
		CMDIdGPDB *mdid =
			GPOS_NEW(mp) CMDIdGPDB(relation_oid, 1 /* major */, 0 /* minor */);

		// generate mdid for relstats
		CMDIdRelStats *m_rel_stats_mdid = GPOS_NEW(mp) CMDIdRelStats(mdid);
		IMDRelStats *mdobj_rel_stats = const_cast<IMDRelStats *>(
			md_accessor.Pmda()->Pmdrelstats(m_rel_stats_mdid));
		mdcache_obj_array->Append(
			dynamic_cast<IMDCacheObject *>(mdobj_rel_stats));

		// extract out column stats for this relation
		Relation rel = gpdb::GetRelation(relation_oid);
		ULONG pos_counter = 0;
		for (ULONG ul = 0; ul < ULONG(rel->rd_att->natts); ul++)
		{
			if (!rel->rd_att->attrs[ul]->attisdropped)
			{
				mdid->AddRef();
				CMDIdColStats *mdid_col_stats =
					GPOS_NEW(mp) CMDIdColStats(mdid, pos_counter);
				pos_counter++;
				IMDColStats *mdobj_col_stats = const_cast<IMDColStats *>(
					md_accessor.Pmda()->Pmdcolstats(mdid_col_stats));
				mdcache_obj_array->Append(
					dynamic_cast<IMDCacheObject *>(mdobj_col_stats));
				mdid_col_stats->Release();
			}
		}
		gpdb::CloseRelation(rel);
		m_rel_stats_mdid->Release();
	}

	if (relcache_ctxt->m_filename)
	{
		COstreamFile cofs(relcache_ctxt->m_filename);

		CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, cofs,
									 true /*serialize_header_footer*/,
									 true /*indentation*/);
	}
	else
	{
		CWStringDynamic wcstr(mp);
		COstreamString oss(&wcstr);

		CDXLUtils::SerializeMetadata(mp, mdcache_obj_array, oss,
									 true /*serialize_header_footer*/,
									 true /*indentation*/);

		CHAR *str = CreateMultiByteCharStringFromWCString(wcstr.GetBuffer());

		GPOS_ASSERT(NULL != str);

		relcache_ctxt->m_dxl = str;
	}

	// cleanup
	mdcache_obj_array->Release();

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::EvalExprFromDXLTask
//
//	@doc:
//		Task that parses an XML representation of a DXL constant expression,
//		evaluates it and returns the result as a serialized DXL document.
//
//---------------------------------------------------------------------------
void *
COptTasks::EvalExprFromDXLTask(void *ptr)
{
	GPOS_ASSERT(NULL != ptr);
	SEvalExprContext *eval_expr_ctxt = SEvalExprContext::PevalctxtConvert(ptr);

	GPOS_ASSERT(NULL != eval_expr_ctxt->m_dxl);
	GPOS_ASSERT(NULL == eval_expr_ctxt->m_dxl_result);

	AUTO_MEM_POOL(amp);
	CMemoryPool *mp = amp.Pmp();

	CDXLNode *input_dxl = CDXLUtils::ParseDXLToScalarExprDXLNode(
		mp, eval_expr_ctxt->m_dxl, NULL /*xsd_file_path*/);
	GPOS_ASSERT(NULL != input_dxl);

	CDXLNode *result_dxl = NULL;
	BOOL should_release_cache = false;

	// Does the metadatacache need to be reset?
	//
	// On the first call, before the cache has been initialized, we
	// don't care about the return value of MDCacheNeedsReset(). But
	// we need to call it anyway, to give it a chance to initialize
	// the invalidation mechanism.
	bool reset_mdcache = gpdb::MDCacheNeedsReset();

	// initialize metadata cache, or purge if needed, or change size if requested
	if (!CMDCache::FInitialized())
	{
		CMDCache::Init();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
		should_release_cache = true;
	}
	else if (reset_mdcache)
	{
		CMDCache::Reset();
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}
	else if (CMDCache::ULLGetCacheQuota() !=
			 (ULLONG) optimizer_mdcache_size * 1024L)
	{
		CMDCache::SetCacheQuota(optimizer_mdcache_size * 1024L);
	}

	GPOS_TRY
	{
		// set up relcache MD provider
		CMDProviderRelcache *relcache_provider =
			GPOS_NEW(mp) CMDProviderRelcache(mp);
		{
			// scope for MD accessor
			CMDAccessor mda(mp, CMDCache::Pcache(), default_sysid,
							relcache_provider);

			CConstExprEvaluatorProxy expr_eval_proxy(mp, &mda);
			result_dxl = expr_eval_proxy.EvaluateExpr(input_dxl);
		}
	}
	GPOS_CATCH_EX(ex)
	{
		CRefCount::SafeRelease(result_dxl);
		CRefCount::SafeRelease(input_dxl);
		if (should_release_cache)
		{
			CMDCache::Shutdown();
		}
		if (ShouldErrorOut(ex))
		{
			IErrorContext *errctxt = CTask::Self()->GetErrCtxt();
			char *serialized_error_msg =
				CreateMultiByteCharStringFromWCString(errctxt->GetErrorMsg());
			elog(DEBUG1, "%s", serialized_error_msg);
			gpdb::GPDBFree(serialized_error_msg);
		}
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	CWStringDynamic *dxl_string =
		CDXLUtils::SerializeScalarExpr(mp, result_dxl,
									   true,  // serialize_header_footer
									   true	  // indentation
		);
	eval_expr_ctxt->m_dxl_result =
		CreateMultiByteCharStringFromWCString(dxl_string->GetBuffer());
	GPOS_DELETE(dxl_string);
	CRefCount::SafeRelease(result_dxl);
	input_dxl->Release();

	if (should_release_cache)
	{
		CMDCache::Shutdown();
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::Optimize
//
//	@doc:
//		optimizes a query to physical DXL
//
//---------------------------------------------------------------------------
char *
COptTasks::Optimize(Query *query)
{
	Assert(query);

	SOptContext gpopt_context;
	gpopt_context.m_query = query;
	gpopt_context.m_should_serialize_plan_dxl = true;
	Execute(&OptimizeTask, &gpopt_context);

	// clean up context
	gpopt_context.Free(gpopt_context.epinQuery, gpopt_context.epinPlanDXL);

	return gpopt_context.m_plan_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::GPOPTOptimizedPlan
//
//	@doc:
//		optimizes a query to plannedstmt
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::GPOPTOptimizedPlan(
	Query *query, SOptContext *gpopt_context,
	BOOL *
		has_unexpected_failure	// output : set to true if optimizer unexpectedly failed to produce plan
)
{
	Assert(query);
	Assert(gpopt_context);

	gpopt_context->m_query = query;
	gpopt_context->m_should_generate_plan_stmt = true;
	GPOS_TRY
	{
		Execute(&OptimizeTask, gpopt_context);
	}
	GPOS_CATCH_EX(ex)
	{
		*has_unexpected_failure = gpopt_context->m_is_unexpected_failure;
		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;
	gpopt_context->HandleError(has_unexpected_failure);
	return gpopt_context->m_plan_stmt;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertQueryToDXL
//
//	@doc:
//		serializes query to DXL
//
//---------------------------------------------------------------------------
char *
COptTasks::ConvertQueryToDXL(Query *query)
{
	Assert(query);

	SOptContext gpopt_context;
	gpopt_context.m_query = query;
	Execute(&ConvertToDXLFromQueryTask, &gpopt_context);

	// clean up context
	gpopt_context.Free(gpopt_context.epinQuery, gpopt_context.epinQueryDXL);

	return gpopt_context.m_query_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::ConvertToiPlanStmtFromXML
//
//	@doc:
//		deserializes planned stmt from DXL
//
//---------------------------------------------------------------------------
PlannedStmt *
COptTasks::ConvertToiPlanStmtFromXML(char *dxl_string)
{
	Assert(NULL != dxl_string);

	SOptContext gpopt_context;
	gpopt_context.m_plan_dxl = dxl_string;
	Execute(&ConvertToPlanStmtFromDXLTask, &gpopt_context);

	// clean up context
	gpopt_context.Free(gpopt_context.epinPlanDXL, gpopt_context.epinPlStmt);

	return gpopt_context.m_plan_stmt;
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
COptTasks::DumpMDObjs(List *oid_list, const char *filename)
{
	SContextRelcacheToDXL relcache_ctxt(oid_list, gpos::ulong_max /*cmp_type*/,
										filename);
	Execute(&ConvertToDXLFromMDObjsTask, &relcache_ctxt);
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
COptTasks::SzMDObjs(List *oid_list)
{
	SContextRelcacheToDXL relcache_ctxt(oid_list, gpos::ulong_max /*cmp_type*/,
										NULL /*filename*/);
	Execute(&ConvertToDXLFromMDObjsTask, &relcache_ctxt);

	return relcache_ctxt.m_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::DumpMDCast
//
//	@doc:
//		Dump cast object into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::DumpMDCast(List *oid_list)
{
	SContextRelcacheToDXL relcache_ctxt(oid_list, gpos::ulong_max /*cmp_type*/,
										NULL /*filename*/);
	Execute(&ConvertToDXLFromMDCast, &relcache_ctxt);

	return relcache_ctxt.m_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::DumpMDScalarCmp
//
//	@doc:
//		Dump scalar comparison object into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::DumpMDScalarCmp(List *oid_list, char *cmp_type)
{
	SContextRelcacheToDXL relcache_ctxt(oid_list, GetComparisonType(cmp_type),
										NULL /*filename*/);
	Execute(&ConvertToDXLFromMDScalarCmp, &relcache_ctxt);

	return relcache_ctxt.m_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::DumpRelStats
//
//	@doc:
//		Dump statistics objects into DXL string
//
//---------------------------------------------------------------------------
char *
COptTasks::DumpRelStats(List *oid_list)
{
	SContextRelcacheToDXL relcache_ctxt(oid_list, gpos::ulong_max /*cmp_type*/,
										NULL /*filename*/);
	Execute(&ConvertToDXLFromRelStatsTask, &relcache_ctxt);

	return relcache_ctxt.m_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::SetXform
//
//	@doc:
//		Enable/Disable a given xform
//
//---------------------------------------------------------------------------
bool
COptTasks::SetXform(char *xform_str, bool should_disable)
{
	CXform *xform = CXformFactory::Pxff()->Pxf(xform_str);
	if (NULL != xform)
	{
		optimizer_xforms[xform->Exfid()] = should_disable;

		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		COptTasks::GetComparisonType
//
//	@doc:
//		Find the comparison type code given its string representation
//
//---------------------------------------------------------------------------
ULONG
COptTasks::GetComparisonType(char *cmp_type)
{
	const ULONG num_cmp_types = 6;
	const CmpType cmp_types[] = {CmptEq, CmptNEq, CmptLT,
								 CmptGT, CmptLEq, CmptGEq};
	const CHAR *cmp_types_str_arr[] = {"Eq", "NEq", "LT", "GT", "LEq", "GEq"};

	for (ULONG ul = 0; ul < num_cmp_types; ul++)
	{
		if (0 == strcasecmp(cmp_type, cmp_types_str_arr[ul]))
		{
			return cmp_types[ul];
		}
	}

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiInvalidComparisonTypeCode);
	return CmptOther;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::EvalExprFromXML
//
//	@doc:
//		Converts XML string to DXL and evaluates the expression. Caller keeps
//		ownership of 'xml_string' and takes ownership of the returned result.
//
//---------------------------------------------------------------------------
char *
COptTasks::EvalExprFromXML(char *xml_string)
{
	GPOS_ASSERT(NULL != xml_string);

	SEvalExprContext evalctxt;
	evalctxt.m_dxl = xml_string;
	evalctxt.m_dxl_result = NULL;

	Execute(&EvalExprFromDXLTask, &evalctxt);
	return evalctxt.m_dxl_result;
}


//---------------------------------------------------------------------------
//	@function:
//		COptTasks::OptimizeMinidumpFromFile
//
//	@doc:
//		Loads a minidump from the given file path, optimizes it and returns
//		the serialized representation of the result as DXL.
//
//---------------------------------------------------------------------------
char *
COptTasks::OptimizeMinidumpFromFile(char *file_name)
{
	GPOS_ASSERT(NULL != file_name);
	SOptimizeMinidumpContext optmdpctxt;
	optmdpctxt.m_szFileName = file_name;
	optmdpctxt.m_dxl_result = NULL;

	Execute(&OptimizeMinidumpTask, &optmdpctxt);
	return optmdpctxt.m_dxl_result;
}

// EOF
