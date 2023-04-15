//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010-2011 EMC Corp.
//
//	@filename:
// CParseHandlerFactory.cpp
//
//	@doc:
// Implementation of the factory methods for creating parse handlers
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/parsehandlers.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

CParseHandlerFactory::TokenParseHandlerFuncMap
	*CParseHandlerFactory::m_token_parse_handler_func_map = nullptr;

// adds a new mapping of token to corresponding parse handler
void
CParseHandlerFactory::AddMapping(
	Edxltoken token_type, ParseHandlerOpCreatorFunc *parse_handler_op_func)
{
	GPOS_ASSERT(nullptr != m_token_parse_handler_func_map);
	const XMLCh *token_identifier_str = CDXLTokens::XmlstrToken(token_type);
	GPOS_ASSERT(nullptr != token_identifier_str);

	BOOL success GPOS_ASSERTS_ONLY = m_token_parse_handler_func_map->Insert(
		token_identifier_str, parse_handler_op_func);

	GPOS_ASSERT(success);
}

// initialize mapping of tokens to parse handlers
void
CParseHandlerFactory::Init(CMemoryPool *mp)
{
	m_token_parse_handler_func_map =
		GPOS_NEW(mp) TokenParseHandlerFuncMap(mp, HASH_MAP_SIZE);

	// array mapping XML Token -> Parse Handler Creator mappings to hashmap
	SParseHandlerMapping token_parse_handler_map[] = {
		{EdxltokenPlan, &CreatePlanParseHandler},
		{EdxltokenMetadata, &CreateMetadataParseHandler},
		{EdxltokenMDRequest, &CreateMDRequestParseHandler},
		{EdxltokenTraceFlags, &CreateTraceFlagsParseHandler},
		{EdxltokenOptimizerConfig, &CreateOptimizerCfgParseHandler},
		{EdxltokenRelationCTAS, &CreateMDRelationCTASParseHandler},
		{EdxltokenEnumeratorConfig, &CreateEnumeratorCfgParseHandler},
		{EdxltokenStatisticsConfig, &CreateStatisticsCfgParseHandler},
		{EdxltokenCTEConfig, &CreateCTECfgParseHandler},
		{EdxltokenCostModelConfig, &CreateCostModelCfgParseHandler},
		{EdxltokenHint, &CreateHintParseHandler},
		{EdxltokenWindowOids, &CreateWindowOidsParseHandler},

		{EdxltokenRelation, &CreateMDRelationParseHandler},
		{EdxltokenIndex, &CreateMDIndexParseHandler},
		{EdxltokenMDType, &CreateMDTypeParseHandler},
		{EdxltokenGPDBScalarOp, &CreateMDScalarOpParseHandler},
		{EdxltokenGPDBFunc, &CreateMDFuncParseHandler},
		{EdxltokenGPDBAgg, &CreateMDAggParseHandler},
		{EdxltokenCheckConstraint, &CreateMDChkConstraintParseHandler},
		{EdxltokenRelationStats, &CreateRelStatsParseHandler},
		{EdxltokenRelationExtendedStats, &CreateRelationExtStatsParseHandler},
		{EdxltokenExtendedStats, &CreateExtStatsParseHandler},
		{EdxltokenExtendedStatsInfo, &CreateExtStatsInfoParseHandler},
		{EdxltokenMVDependencyList, &CreateExtStatsDependenciesParseHandler},
		{EdxltokenMVDependency, &CreateExtStatsDependencyParseHandler},
		{EdxltokenMVNDistinctList, &CreateExtStatsNDistinctListParseHandler},
		{EdxltokenMVNDistinct, &CreateExtStatsNDistinctParseHandler},
		{EdxltokenColumnStats, &CreateColStatsParseHandler},
		{EdxltokenMetadataIdList, &CreateMDIdListParseHandler},
		{EdxltokenIndexInfoList, &CreateMDIndexInfoListParseHandler},
		{EdxltokenMetadataColumns, &CreateMDColsParseHandler},
		{EdxltokenMetadataColumn, &CreateMDColParseHandler},
		{EdxltokenColumnDefaultValue, &CreateColDefaultValExprParseHandler},
		{EdxltokenColumnStatsBucket, &CreateColStatsBucketParseHandler},
		{EdxltokenGPDBCast, &CreateMDCastParseHandler},
		{EdxltokenGPDBMDScCmp, &CreateMDScCmpParseHandler},
		{EdxltokenGPDBArrayCoerceCast, &CreateMDArrayCoerceCastParseHandler},

		{EdxltokenPhysical, &CreatePhysicalOpParseHandler},

		{EdxltokenPhysicalAggregate, &CreateAggParseHandler},
		{EdxltokenPhysicalTableScan, &CreateTableScanParseHandler},
		{EdxltokenPhysicalBitmapTableScan, &CreateBitmapTableScanParseHandler},
		{EdxltokenPhysicalDynamicBitmapTableScan,
		 &CreateDynBitmapTableScanParseHandler},
		{EdxltokenPhysicalForeignScan, &CreateForeignScanParseHandler},
		{EdxltokenPhysicalHashJoin, &CreateHashJoinParseHandler},
		{EdxltokenPhysicalNLJoin, &CreateNLJoinParseHandler},
		{EdxltokenPhysicalMergeJoin, &CreateMergeJoinParseHandler},
		{EdxltokenPhysicalGatherMotion, &CreateGatherMotionParseHandler},
		{EdxltokenPhysicalBroadcastMotion, &CreateBroadcastMotionParseHandler},
		{EdxltokenPhysicalRedistributeMotion,
		 &CreateRedistributeMotionParseHandler},
		{EdxltokenPhysicalRoutedDistributeMotion,
		 &CreateRoutedMotionParseHandler},
		{EdxltokenPhysicalRandomMotion, &CreateRandomMotionParseHandler},
		{EdxltokenPhysicalSubqueryScan, &CreateSubqueryScanParseHandler},
		{EdxltokenPhysicalResult, &CreateResultParseHandler},
		{EdxltokenPhysicalLimit, &CreateLimitParseHandler},
		{EdxltokenPhysicalSort, &CreateSortParseHandler},
		{EdxltokenPhysicalAppend, &CreateAppendParseHandler},
		{EdxltokenPhysicalMaterialize, &CreateMaterializeParseHandler},
		{EdxltokenPhysicalDynamicTableScan, &CreateDTSParseHandler},
		{EdxltokenPhysicalDynamicIndexScan, &CreateDynamicIdxScanParseHandler},
		{EdxltokenPhysicalDynamicForeignScan, &CreateDFSParseHandler},
		{EdxltokenPhysicalPartitionSelector,
		 &CreatePartitionSelectorParseHandler},
		{EdxltokenPhysicalSequence, &CreateSequenceParseHandler},
		{EdxltokenPhysicalIndexScan, &CreateIdxScanListParseHandler},
		{EdxltokenPhysicalIndexOnlyScan, &CreateIdxOnlyScanParseHandler},
		{EdxltokenScalarBitmapIndexProbe, &CreateBitmapIdxProbeParseHandler},
		{EdxltokenIndexDescr, &CreateIdxDescrParseHandler},

		{EdxltokenPhysicalWindow, &CreateWindowParseHandler},
		{EdxltokenScalarWindowref, &CreateWindowRefParseHandler},
		{EdxltokenWindowFrame, &CreateWindowFrameParseHandler},
		{EdxltokenScalarWindowFrameLeadingEdge,
		 &CreateFrameLeadingEdgeParseHandler},
		{EdxltokenScalarWindowFrameTrailingEdge,
		 &CreateFrameTrailingEdgeParseHandler},
		{EdxltokenWindowKey, &CreateWindowKeyParseHandler},
		{EdxltokenWindowKeyList, &CreateWindowKeyListParseHandler},

		{EdxltokenScalarIndexCondList, &CreateIdxCondListParseHandler},

		{EdxltokenScalar, &CreateScalarOpParseHandler},

		{EdxltokenScalarFilter, &CreateFilterParseHandler},
		{EdxltokenScalarOneTimeFilter, &CreateFilterParseHandler},
		{EdxltokenScalarRecheckCondFilter, &CreateFilterParseHandler},
		{EdxltokenScalarProjList, &CreateProjListParseHandler},
		{EdxltokenScalarProjElem, &CreateProjElemParseHandler},
		{EdxltokenScalarAggref, &CreateAggRefParseHandler},
		{EdxltokenScalarSortColList, &CreateSortColListParseHandler},
		{EdxltokenScalarSortCol, &CreateSortColParseHandler},
		{EdxltokenScalarCoalesce, &CreateScCoalesceParseHandler},
		{EdxltokenScalarComp, &CreateScCmpParseHandler},
		{EdxltokenScalarDistinctComp, &CreateDistinctCmpParseHandler},
		{EdxltokenScalarIdent, &CreateScIdParseHandler},
		{EdxltokenScalarOpExpr, &CreateScOpExprParseHandler},
		{EdxltokenScalarArrayComp, &CreateScArrayCmpParseHandler},
		{EdxltokenScalarBoolOr, &CreateScBoolExprParseHandler},
		{EdxltokenScalarBoolNot, &CreateScBoolExprParseHandler},
		{EdxltokenScalarBoolAnd, &CreateScBoolExprParseHandler},
		{EdxltokenScalarMin, &CreateScMinMaxParseHandler},
		{EdxltokenScalarMax, &CreateScMinMaxParseHandler},
		{EdxltokenScalarBoolTestIsTrue, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotTrue, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsFalse, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotFalse, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsUnknown, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotUnknown, &CreateBooleanTestParseHandler},
		{EdxltokenScalarSubPlan, &CreateScSubPlanParseHandler},
		{EdxltokenScalarConstValue, &CreateScConstValueParseHandler},
		{EdxltokenScalarIfStmt, &CreateIfStmtParseHandler},
		{EdxltokenScalarSwitch, &CreateScSwitchParseHandler},
		{EdxltokenScalarSwitchCase, &CreateScSwitchCaseParseHandler},
		{EdxltokenScalarCaseTest, &CreateScCaseTestParseHandler},
		{EdxltokenScalarFuncExpr, &CreateScFuncExprParseHandler},
		{EdxltokenScalarIsNull, &CreateScNullTestParseHandler},
		{EdxltokenScalarIsNotNull, &CreateScNullTestParseHandler},
		{EdxltokenScalarNullIf, &CreateScNullIfParseHandler},
		{EdxltokenScalarCast, &CreateScCastParseHandler},
		{EdxltokenScalarCoerceToDomain, CreateScCoerceToDomainParseHandler},
		{EdxltokenScalarCoerceViaIO, CreateScCoerceViaIOParseHandler},
		{EdxltokenScalarArrayCoerceExpr, CreateScArrayCoerceExprParseHandler},
		{EdxltokenScalarHashExpr, &CreateHashExprParseHandler},
		{EdxltokenScalarHashCondList, &CreateCondListParseHandler},
		{EdxltokenScalarMergeCondList, &CreateCondListParseHandler},
		{EdxltokenScalarHashExprList, &CreateHashExprListParseHandler},
		{EdxltokenScalarGroupingColList, &CreateGroupingColListParseHandler},
		{EdxltokenScalarLimitOffset, &CreateLimitOffsetParseHandler},
		{EdxltokenScalarLimitCount, &CreateLimitCountParseHandler},
		{EdxltokenScalarSubPlanTestExpr, &CreateScSubPlanTestExprParseHandler},
		{EdxltokenScalarSubPlanParamList,
		 &CreateScSubPlanParamListParseHandler},
		{EdxltokenScalarSubPlanParam, &CreateScSubPlanParamParseHandler},
		{EdxltokenScalarOpList, &CreateScOpListParseHandler},

		{EdxltokenScalarSubquery, &CreateScSubqueryParseHandler},
		{EdxltokenScalarBitmapAnd, &CreateScBitmapBoolOpParseHandler},
		{EdxltokenScalarBitmapOr, &CreateScBitmapBoolOpParseHandler},

		{EdxltokenScalarArray, &CreateScArrayParseHandler},
		{EdxltokenScalarArrayRef, &CreateScArrayRefParseHandler},
		{EdxltokenScalarArrayRefIndexList,
		 &CreateScArrayRefIdxListParseHandler},

		{EdxltokenScalarAssertConstraintList,
		 &CreateScAssertConstraintListParseHandler},

		{EdxltokenScalarDMLAction, &CreateScDMLActionParseHandler},
		{EdxltokenDirectDispatchInfo, &CreateDirectDispatchParseHandler},

		{EdxltokenQueryOutput, &CreateLogicalQueryOpParseHandler},

		{EdxltokenCost, &CreateCostParseHandler},
		{EdxltokenTableDescr, &CreateTableDescParseHandler},
		{EdxltokenColumns, &CreateColDescParseHandler},
		{EdxltokenProperties, &CreatePropertiesParseHandler},
		{EdxltokenPhysicalTVF, &CreatePhysicalTVFParseHandler},
		{EdxltokenLogicalTVF, &CreateLogicalTVFParseHandler},

		{EdxltokenQuery, &CreateQueryParseHandler},
		{EdxltokenLogicalGet, &CreateLogicalGetParseHandler},
		{EdxltokenLogicalForeignGet, &CreateLogicalForeignGetParseHandler},
		{EdxltokenLogical, &CreateLogicalOpParseHandler},
		{EdxltokenLogicalProject, &CreateLogicalProjParseHandler},
		{EdxltokenLogicalSelect, &CreateLogicalSelectParseHandler},
		{EdxltokenLogicalJoin, &CreateLogicalJoinParseHandler},
		{EdxltokenLogicalGrpBy, &CreateLogicalGrpByParseHandler},
		{EdxltokenLogicalLimit, &CreateLogicalLimitParseHandler},
		{EdxltokenLogicalConstTable, &CreateLogicalConstTableParseHandler},
		{EdxltokenLogicalCTEProducer, &CreateLogicalCTEProdParseHandler},
		{EdxltokenLogicalCTEConsumer, &CreateLogicalCTEConsParseHandler},
		{EdxltokenLogicalCTEAnchor, &CreateLogicalCTEAnchorParseHandler},
		{EdxltokenCTEList, &CreateCTEListParseHandler},

		{EdxltokenLogicalWindow, &CreateLogicalWindowParseHandler},
		{EdxltokenWindowSpec, &CreateWindowSpecParseHandler},
		{EdxltokenWindowSpecList, &CreateWindowSpecListParseHandler},

		{EdxltokenLogicalInsert, &CreateLogicalInsertParseHandler},
		{EdxltokenLogicalDelete, &CreateLogicalDeleteParseHandler},
		{EdxltokenLogicalUpdate, &CreateLogicalUpdateParseHandler},
		{EdxltokenPhysicalDMLInsert, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalDMLDelete, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalDMLUpdate, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalSplit, &CreatePhysicalSplitParseHandler},
		{EdxltokenPhysicalAssert, &CreatePhysicalAssertParseHandler},
		{EdxltokenPhysicalCTEProducer, &CreatePhysicalCTEProdParseHandler},
		{EdxltokenPhysicalCTEConsumer, &CreatePhysicalCTEConsParseHandler},
		{EdxltokenLogicalCTAS, &CreateLogicalCTASParseHandler},
		{EdxltokenPhysicalCTAS, &CreatePhysicalCTASParseHandler},
		{EdxltokenCTASOptions, &CreateCTASOptionsParseHandler},

		{EdxltokenScalarSubqueryAny,
		 &CreateScScalarSubqueryQuantifiedParseHandler},
		{EdxltokenScalarSubqueryAll,
		 &CreateScScalarSubqueryQuantifiedParseHandler},
		{EdxltokenScalarSubqueryExists,
		 &CreateScScalarSubqueryExistsParseHandler},
		{EdxltokenScalarSubqueryNotExists,
		 &CreateScScalarSubqueryExistsParseHandler},

		{EdxltokenStackTrace, &CreateStackTraceParseHandler},
		{EdxltokenLogicalUnion, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalUnionAll, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalIntersect, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalIntersectAll, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalDifference, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalDifferenceAll, &CreateLogicalSetOpParseHandler},

		{EdxltokenStatistics, &CreateStatsParseHandler},
		{EdxltokenStatsDerivedColumn, &CreateStatsDrvdColParseHandler},
		{EdxltokenStatsDerivedRelation, &CreateStatsDrvdRelParseHandler},
		{EdxltokenStatsBucketLowerBound, &CreateStatsBucketBoundParseHandler},
		{EdxltokenStatsBucketUpperBound, &CreateStatsBucketBoundParseHandler},

		{EdxltokenSearchStrategy, &CreateSearchStrategyParseHandler},
		{EdxltokenSearchStage, &CreateSearchStageParseHandler},
		{EdxltokenXform, &CreateXformParseHandler},

		{EdxltokenCostParams, &CreateCostParamsParseHandler},
		{EdxltokenCostParam, &CreateCostParamParseHandler},

		{EdxltokenScalarExpr, &CreateScExprParseHandler},
		{EdxltokenScalarValuesList, &CreateScValuesListParseHandler},
		{EdxltokenScalarSortGroupClause, &CreateScSortGroupClauseParseHandler},
		{EdxltokenPhysicalValuesScan, &CreateValuesScanParseHandler},
		{EdxltokenNLJIndexParamList, &CreateNLJIndexParamListParseHandler},
		{EdxltokenNLJIndexParam, &CreateNLJIndexParamParseHandler}

	};

	const ULONG num_of_parse_handlers =
		GPOS_ARRAY_SIZE(token_parse_handler_map);

	for (ULONG idx = 0; idx < num_of_parse_handlers; idx++)
	{
		SParseHandlerMapping elem = token_parse_handler_map[idx];
		AddMapping(elem.token_type, elem.parse_handler_op_func);
	}
}

// creates a parse handler instance given an xml tag
CParseHandlerBase *
CParseHandlerFactory::GetParseHandler(CMemoryPool *mp,
									  const XMLCh *token_identifier_str,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root)
{
	GPOS_ASSERT(nullptr != m_token_parse_handler_func_map);

	ParseHandlerOpCreatorFunc *create_parse_handler_func =
		m_token_parse_handler_func_map->Find(token_identifier_str);

	if (create_parse_handler_func != nullptr)
	{
		return (*create_parse_handler_func)(mp, parse_handler_mgr,
											parse_handler_root);
	}

	CDXLMemoryManager dxl_memory_manager(mp);

	// did not find the physical operator in the table
	CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
		&dxl_memory_manager, token_identifier_str);

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedOperator,
			   str->GetBuffer());

	return nullptr;
}

// creates a parse handler for parsing a DXL document.
CParseHandlerDXL *
CParseHandlerFactory::GetParseHandlerDXL(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr)
{
	return GPOS_NEW(mp) CParseHandlerDXL(mp, parse_handler_mgr);
}

// creates a parse handler for parsing a Plan
CParseHandlerBase *
CParseHandlerFactory::CreatePlanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPlan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMetadataParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMetadata(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a metadata request
CParseHandlerBase *
CParseHandlerFactory::CreateMDRequestParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDRequest(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing trace flags
CParseHandlerBase *
CParseHandlerFactory::CreateTraceFlagsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerTraceFlags(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing optimizer config
CParseHandlerBase *
CParseHandlerFactory::CreateOptimizerCfgParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerOptimizerConfig(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing enumerator config
CParseHandlerBase *
CParseHandlerFactory::CreateEnumeratorCfgParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerEnumeratorConfig(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing statistics configuration
CParseHandlerBase *
CParseHandlerFactory::CreateStatisticsCfgParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerStatisticsConfig(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing CTE configuration
CParseHandlerBase *
CParseHandlerFactory::CreateCTECfgParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCTEConfig(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing cost model configuration
CParseHandlerBase *
CParseHandlerFactory::CreateCostModelCfgParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCostModel(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing hint configuration
CParseHandlerBase *
CParseHandlerFactory::CreateHintParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerHint(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window oids configuration
CParseHandlerBase *
CParseHandlerFactory::CreateWindowOidsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowOids(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDRelationParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDRelation(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing CTAS relation metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDRelationCTASParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDRelationCtas(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a MD index
CParseHandlerBase *
CParseHandlerFactory::CreateMDIndexParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDIndex(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation stats
CParseHandlerBase *
CParseHandlerFactory::CreateRelStatsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerRelStats(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation stats
CParseHandlerBase *
CParseHandlerFactory::CreateRelationExtStatsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerRelationExtendedStats(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation stats
CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerExtStats(mp, parse_handler_mgr, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsInfoParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerExtStatsInfo(mp, parse_handler_mgr, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsDependenciesParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerExtStatsDependencies(mp, parse_handler_mgr,
														  parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsDependencyParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerExtStatsDependency(mp, parse_handler_mgr,
														parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsNDistinctListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerExtStatsNDistinctList(
		mp, parse_handler_mgr, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateExtStatsNDistinctParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerExtStatsNDistinct(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing column stats
CParseHandlerBase *
CParseHandlerFactory::CreateColStatsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerColStats(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing column stats bucket
CParseHandlerBase *
CParseHandlerFactory::CreateColStatsBucketParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerColStatsBucket(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB type metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDTypeParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDType(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific operator metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDScalarOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDGPDBScalarOp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific function metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDFuncParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDGPDBFunc(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific aggregate metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDAggParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDGPDBAgg(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific cast metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDCastParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDCast(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific scalar comparison metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDScCmpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDScCmp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of metadata identifiers
CParseHandlerBase *
CParseHandlerFactory::CreateMDIdListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMetadataIdList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of column metadata info
CParseHandlerBase *
CParseHandlerFactory::CreateMDColsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMetadataColumns(mp, parse_handler_mgr, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateMDIndexInfoListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMDIndexInfoList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing column info
CParseHandlerBase *
CParseHandlerFactory::CreateMDColParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMetadataColumn(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a a default value for a column
CParseHandlerBase *
CParseHandlerFactory::CreateColDefaultValExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerDefaultValueExpr(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a physical operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar operator
CParseHandlerBase *
CParseHandlerFactory::CreateScalarOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarOp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the properties of a physical operator
CParseHandlerBase *
CParseHandlerFactory::CreatePropertiesParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerProperties(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a filter operator
CParseHandlerBase *
CParseHandlerFactory::CreateFilterParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerFilter(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a table scan
CParseHandlerBase *
CParseHandlerFactory::CreateTableScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerTableScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a bitmap table scan
CParseHandlerBase *
CParseHandlerFactory::CreateBitmapTableScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerPhysicalBitmapTableScan(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a dynamic bitmap table scan
CParseHandlerBase *
CParseHandlerFactory::CreateDynBitmapTableScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerPhysicalDynamicBitmapTableScan(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a foreign scan
CParseHandlerBase *
CParseHandlerFactory::CreateForeignScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerForeignScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a subquery scan
CParseHandlerBase *
CParseHandlerFactory::CreateSubqueryScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSubqueryScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a result node
CParseHandlerBase *
CParseHandlerFactory::CreateResultParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerResult(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash join operator
CParseHandlerBase *
CParseHandlerFactory::CreateHashJoinParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerHashJoin(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a nested loop join operator
CParseHandlerBase *
CParseHandlerFactory::CreateNLJoinParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerNLJoin(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a merge join operator
CParseHandlerBase *
CParseHandlerFactory::CreateMergeJoinParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMergeJoin(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sort operator
CParseHandlerBase *
CParseHandlerFactory::CreateSortParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSort(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an append operator
CParseHandlerBase *
CParseHandlerFactory::CreateAppendParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerAppend(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a materialize operator
CParseHandlerBase *
CParseHandlerFactory::CreateMaterializeParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerMaterialize(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a dynamic table scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateDTSParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerDynamicTableScan(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a dynamic index scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateDynamicIdxScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerDynamicIndexScan(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a dynamic table scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateDFSParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerDynamicForeignScan(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a partition selector operator
CParseHandlerBase *
CParseHandlerFactory::CreatePartitionSelectorParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerPartitionSelector(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing a sequence operator
CParseHandlerBase *
CParseHandlerFactory::CreateSequenceParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSequence(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Limit operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLimit(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Limit Count operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitCountParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarLimitCount(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a scalar subquery operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSubqueryParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarSubquery(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar bitmap boolean operator
CParseHandlerBase *
CParseHandlerFactory::CreateScBitmapBoolOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarBitmapBoolOp(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a scalar array operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerArray(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar arrayref operator
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayRefParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarArrayRef(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an arrayref index list
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayRefIdxListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarArrayRefIndexList(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar assert predicate operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScAssertConstraintListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarAssertConstraintList(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar DML action operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScDMLActionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarDMLAction(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar operator list
CParseHandlerBase *
CParseHandlerFactory::CreateScOpListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarOpList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing direct dispatch info
CParseHandlerBase *
CParseHandlerFactory::CreateDirectDispatchParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerDirectDispatchInfo(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a Limit Count operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitOffsetParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarLimitOffset(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing a gather motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateGatherMotionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerGatherMotion(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a broadcast motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateBroadcastMotionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerBroadcastMotion(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a redistribute motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRedistributeMotionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerRedistributeMotion(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a routed motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRoutedMotionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerRoutedMotion(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a random motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRandomMotionParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerRandomMotion(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a group by operator
CParseHandlerBase *
CParseHandlerFactory::CreateAggParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerAgg(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing aggref operator
CParseHandlerBase *
CParseHandlerFactory::CreateAggRefParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarAggref(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a grouping cols list in a group by
// operator
CParseHandlerBase *
CParseHandlerFactory::CreateGroupingColListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerGroupingColList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar comparison operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCmpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarComp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a distinct comparison operator
CParseHandlerBase *
CParseHandlerFactory::CreateDistinctCmpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerDistinctComp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar identifier operator
CParseHandlerBase *
CParseHandlerFactory::CreateScIdParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarIdent(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar FuncExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScFuncExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarFuncExpr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar OpExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScOpExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarOpExpr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar OpExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayCmpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarArrayComp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a BoolExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScBoolExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarBoolExpr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a MinMax
CParseHandlerBase *
CParseHandlerFactory::CreateScMinMaxParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarMinMax(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a BooleanTest
CParseHandlerBase *
CParseHandlerFactory::CreateBooleanTestParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarBooleanTest(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing a NullTest
CParseHandlerBase *
CParseHandlerFactory::CreateScNullTestParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarNullTest(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a NullIf
CParseHandlerBase *
CParseHandlerFactory::CreateScNullIfParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarNullIf(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a cast
CParseHandlerBase *
CParseHandlerFactory::CreateScCastParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarCast(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a CoerceToDomain operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoerceToDomainParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarCoerceToDomain(mp, parse_handler_mgr,
														  parse_handler_root);
}

// creates a parse handler for parsing a CoerceViaIO operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoerceViaIOParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarCoerceViaIO(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing an array coerce expression operator
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayCoerceExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarArrayCoerceExpr(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan.
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarSubPlan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan test expression
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanTestExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSubPlanTestExpr(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan Params DXL node
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParamListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSubPlanParamList(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a single SubPlan Param
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParamParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSubPlanParam(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a logical TVF
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalTVFParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalTVF(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical TVF
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalTVFParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalTVF(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a coalesce operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoalesceParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarCoalesce(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Switch operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSwitchParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarSwitch(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SwitchCase operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSwitchCaseParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSwitchCase(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a case test
CParseHandlerBase *
CParseHandlerFactory::CreateScCaseTestParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarCaseTest(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Const
CParseHandlerBase *
CParseHandlerFactory::CreateScConstValueParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarConstValue(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing an if statement
CParseHandlerBase *
CParseHandlerFactory::CreateIfStmtParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarIfStmt(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a projection list
CParseHandlerBase *
CParseHandlerFactory::CreateProjListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerProjList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a projection element
CParseHandlerBase *
CParseHandlerFactory::CreateProjElemParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerProjElem(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash expr list
CParseHandlerBase *
CParseHandlerFactory::CreateHashExprListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerHashExprList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash expression in a redistribute
// motion node
CParseHandlerBase *
CParseHandlerFactory::CreateHashExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerHashExpr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a condition list in a hash join or
// merge join node
CParseHandlerBase *
CParseHandlerFactory::CreateCondListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCondList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sorting column list in a sort node
CParseHandlerBase *
CParseHandlerFactory::CreateSortColListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSortColList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sorting column in a sort node
CParseHandlerBase *
CParseHandlerFactory::CreateSortColParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSortCol(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the cost estimates of a physical
// operator
CParseHandlerBase *
CParseHandlerFactory::CreateCostParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCost(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a table descriptor
CParseHandlerBase *
CParseHandlerFactory::CreateTableDescParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerTableDescr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a column descriptor
CParseHandlerBase *
CParseHandlerFactory::CreateColDescParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerColDescr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxScanListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerIndexScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index only scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxOnlyScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerIndexOnlyScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a bitmap index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateBitmapIdxProbeParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarBitmapIndexProbe(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index descriptor of an
// index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxDescrParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerIndexDescr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the list of index condition in a
// index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxCondListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerIndexCondList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a query
CParseHandlerBase *
CParseHandlerFactory::CreateQueryParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerQuery(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical get operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalGetParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalGet(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical foreign get operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalForeignGetParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerLogicalForeignGet(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing a logical project operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalProjParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalProject(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTE producer operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEProdParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerLogicalCTEProducer(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a logical CTE consumer operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEConsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerLogicalCTEConsumer(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a logical CTE anchor operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEAnchorParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerLogicalCTEAnchor(mp, parse_handler_mgr,
													  parse_handler_root);
}

// creates a parse handler for parsing a CTE list
CParseHandlerBase *
CParseHandlerFactory::CreateCTEListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCTEList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical set operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalSetOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalSetOp(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical select operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalSelectParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalSelect(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical join operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalJoinParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalJoin(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing dxl representing query output
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalQueryOpParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerQueryOutput(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical group by operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalGrpByParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalGroupBy(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical limit operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalLimitParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalLimit(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical constant table operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalConstTableParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerLogicalConstTable(mp, parse_handler_mgr,
													   parse_handler_root);
}

// creates a parse handler for parsing ALL/ANY subquery operators
CParseHandlerBase *
CParseHandlerFactory::CreateScScalarSubqueryQuantifiedParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSubqueryQuantified(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an EXISTS/NOT EXISTS subquery operator
CParseHandlerBase *
CParseHandlerFactory::CreateScScalarSubqueryExistsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSubqueryExists(mp, parse_handler_mgr,
														  parse_handler_root);
}

// creates a parse handler for parsing relation statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerStatistics(mp, parse_handler_mgr, parse_handler_root);
}

// creates a pass-through parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateStackTraceParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerStacktrace(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsDrvdRelParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerStatsDerivedRelation(mp, parse_handler_mgr,
														  parse_handler_root);
}

// creates a parse handler for parsing derived column statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsDrvdColParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerStatsDerivedColumn(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing bucket bound in a histogram
CParseHandlerBase *
CParseHandlerFactory::CreateStatsBucketBoundParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerStatsBound(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a window node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalWindow(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing WindowRef operator
CParseHandlerBase *
CParseHandlerFactory::CreateWindowRefParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarWindowRef(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window frame node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowFrameParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowFrame(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window key node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowKeyParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowKey(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of window keys
CParseHandlerBase *
CParseHandlerFactory::CreateWindowKeyListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowKeyList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window specification node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowSpecParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowSpec(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of window specifications
CParseHandlerBase *
CParseHandlerFactory::CreateWindowSpecListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerWindowSpecList(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical window operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalWindowParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalWindow(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical insert operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalInsertParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalInsert(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical delete operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalDeleteParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalDelete(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical update operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalUpdateParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalUpdate(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTAS operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTASParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerLogicalCTAS(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical CTAS operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTASParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalCTAS(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing CTAS storage options
CParseHandlerBase *
CParseHandlerFactory::CreateCTASOptionsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerCtasStorageOptions(mp, parse_handler_mgr,
														parse_handler_root);
}

// creates a parse handler for parsing a physical CTE producer operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTEProdParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerPhysicalCTEProducer(mp, parse_handler_mgr,
														 parse_handler_root);
}

// creates a parse handler for parsing a physical CTE consumer operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTEConsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerPhysicalCTEConsumer(mp, parse_handler_mgr,
														 parse_handler_root);
}

// creates a parse handler for parsing a physical DML operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalDMLParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalDML(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical split operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalSplitParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerPhysicalSplit(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical assert operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalAssertParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerAssert(mp, parse_handler_mgr, parse_handler_root);
}

// creates a trailing window frame edge parser
CParseHandlerBase *
CParseHandlerFactory::CreateFrameTrailingEdgeParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarWindowFrameEdge(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a leading window frame edge parser
CParseHandlerBase *
CParseHandlerFactory::CreateFrameLeadingEdgeParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarWindowFrameEdge(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing search strategy
CParseHandlerBase *
CParseHandlerFactory::CreateSearchStrategyParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSearchStrategy(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing search stage
CParseHandlerBase *
CParseHandlerFactory::CreateSearchStageParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerSearchStage(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing xform
CParseHandlerBase *
CParseHandlerFactory::CreateXformParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerXform(mp, parse_handler_mgr, parse_handler_root);
}

// creates cost params parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateCostParamsParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCostParams(mp, parse_handler_mgr, parse_handler_root);
}

// creates cost param parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateCostParamParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerCostParam(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for top level scalar expressions
CParseHandlerBase *
CParseHandlerFactory::CreateScExprParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerScalarExpr(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific check constraint
CParseHandlerBase *
CParseHandlerFactory::CreateMDChkConstraintParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerMDGPDBCheckConstraint(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Values List operator
CParseHandlerBase *
CParseHandlerFactory::CreateScValuesListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarValuesList(mp, parse_handler_mgr,
													  parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateScSortGroupClauseParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerScalarSortGroupClause(
		mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Values Scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateValuesScanParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp)
		CParseHandlerValuesScan(mp, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific array coerce cast metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDArrayCoerceCastParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerMDArrayCoerceCast(mp, parse_handler_mgr,
													   parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateNLJIndexParamListParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_manager,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerNLJIndexParamList(
		mp, parse_handler_manager, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateNLJIndexParamParseHandler(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_manager,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(mp) CParseHandlerNLJIndexParam(mp, parse_handler_manager,
												   parse_handler_root);
}
// EOF
