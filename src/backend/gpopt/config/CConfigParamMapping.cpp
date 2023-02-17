//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConfigParamMapping.cpp
//
//	@doc:
//		Implementation of GPDB config params->trace flags mapping
//
//	@test:
//
//
//---------------------------------------------------------------------------

extern "C" {
#include "postgres.h"

#include "utils/guc.h"
}

#include "gpopt/config/CConfigParamMapping.h"
#include "gpopt/xforms/CXform.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

// array mapping GUCs to traceflags
CConfigParamMapping::SConfigMappingElem CConfigParamMapping::m_elements[] = {
	{EopttracePrintQuery, &optimizer_print_query,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints the optimizer's input query expression tree.")},

	{EopttracePrintPlan, &optimizer_print_plan,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Prints the plan expression tree produced by the optimizer.")},

	{EopttracePrintXform, &optimizer_print_xform,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Prints the input and output expression trees of the optimizer transformations.")},

	{EopttracePrintXformResults, &optimizer_print_xform_results,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Print input and output of xforms.")},

	{EopttracePrintMemoAfterExploration,
	 &optimizer_print_memo_after_exploration,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints MEMO after exploration.")},

	{EopttracePrintMemoAfterImplementation,
	 &optimizer_print_memo_after_implementation,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints MEMO after implementation.")},

	{EopttracePrintMemoAfterOptimization,
	 &optimizer_print_memo_after_optimization,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints MEMO after optimization.")},

	{EopttracePrintJobScheduler, &optimizer_print_job_scheduler,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints jobs in scheduler on each job completion.")},

	{EopttracePrintExpressionProperties, &optimizer_print_expression_properties,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints expression properties.")},

	{EopttracePrintGroupProperties, &optimizer_print_group_properties,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints group properties.")},

	{EopttracePrintOptimizationContext, &optimizer_print_optimization_context,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints optimization context.")},

	{EopttracePrintOptimizationStatistics, &optimizer_print_optimization_stats,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Prints optimization stats.")},

	{EopttraceMinidump,
	 // GPDB_91_MERGE_FIXME: I turned optimizer_minidump from bool into
	 // an enum-type GUC. It's a bit dirty to cast it like this..
	 (bool *) &optimizer_minidump,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Generate optimizer minidump.")},

	{EopttraceDisableMotions, &optimizer_enable_motions,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion nodes in optimizer.")},

	{EopttraceDisableMotionBroadcast, &optimizer_enable_motion_broadcast,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion broadcast nodes in optimizer.")},

	{EopttraceDisableMotionGather, &optimizer_enable_motion_gather,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion gather nodes in optimizer.")},

	{EopttraceDisableMotionHashDistribute,
	 &optimizer_enable_motion_redistribute,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion hash-distribute nodes in optimizer.")},

	{EopttraceDisableMotionRandom, &optimizer_enable_motion_redistribute,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion random nodes in optimizer.")},

	{EopttraceDisableMotionRountedDistribute,
	 &optimizer_enable_motion_redistribute,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable motion routed-distribute nodes in optimizer.")},

	{EopttraceDisableSort, &optimizer_enable_sort,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable sort nodes in optimizer.")},

	{EopttraceDisableSpool, &optimizer_enable_materialize,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable spool nodes in optimizer.")},

	{EopttraceDisablePartPropagation, &optimizer_enable_partition_propagation,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable partition propagation nodes in optimizer.")},

	{EopttraceDisablePartSelection, &optimizer_enable_partition_selection,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable partition selection in optimizer.")},

	{EopttraceDisableOuterJoin2InnerJoinRewrite,
	 &optimizer_enable_outerjoin_rewrite,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable outer join to inner join rewrite in optimizer.")},

	{EopttraceDonotDeriveStatsForAllGroups,
	 &optimizer_enable_derive_stats_all_groups,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Disable deriving stats for all groups after exploration.")},

	{EopttraceEnableSpacePruning, &optimizer_enable_space_pruning,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable space pruning in optimizer.")},

	{EopttraceForceMultiStageAgg, &optimizer_force_multistage_agg,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Force optimizer to always pick multistage aggregates when such a plan alternative is generated.")},

	{EopttracePrintColsWithMissingStats, &optimizer_print_missing_stats,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Print columns with missing statistics.")},

	{EopttraceEnableRedistributeBroadcastHashJoin,
	 &optimizer_enable_hashjoin_redistribute_broadcast_children,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable generating hash join plan where outer child is Redistribute and inner child is Broadcast.")},

	{EopttraceExtractDXLStats, &optimizer_extract_dxl_stats,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Extract plan stats in dxl.")},

	{EopttraceExtractDXLStatsAllNodes, &optimizer_extract_dxl_stats_all_nodes,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Extract plan stats for all physical dxl nodes.")},

	{EopttraceDeriveStatsForDPE, &optimizer_dpe_stats,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable stats derivation of partitioned tables with dynamic partition elimination.")},

	{EopttraceEnumeratePlans, &optimizer_enumerate_plans,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable plan enumeration.")},

	{EopttraceSamplePlans, &optimizer_sample_plans,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable plan sampling.")},

	{EopttraceEnableCTEInlining, &optimizer_cte_inlining,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable CTE inlining.")},

	{EopttraceEnableConstantExpressionEvaluation,
	 &optimizer_enable_constant_expression_evaluation,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable constant expression evaluation in the optimizer")},

	{EopttraceUseExternalConstantExpressionEvaluationForInts,
	 &optimizer_use_external_constant_expression_evaluation_for_ints,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable constant expression evaluation for integers in the optimizer")},

	{EopttraceApplyLeftOuter2InnerUnionAllLeftAntiSemiJoinDisregardingStats,
	 &optimizer_apply_left_outer_to_union_all_disregarding_stats,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Always apply Left Outer Join to Inner Join UnionAll Left Anti Semi Join without looking at stats")},

	{EopttraceRemoveOrderBelowDML, &optimizer_remove_order_below_dml,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Remove OrderBy below a DML operation")},

	{EopttraceDisableReplicateInnerNLJOuterChild,
	 &optimizer_enable_broadcast_nestloop_outer_child,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable plan alternatives where NLJ's outer child is replicated")},

	{EopttraceDiscardRedistributeHashJoin,
	 &optimizer_discard_redistribute_hashjoin,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Discard plan alternatives where hash join has a redistribute motion child")},

	{EopttraceMotionHazardHandling, &optimizer_enable_streaming_material,
	 false,	 // m_fNegate
	 GPOS_WSZ_LIT(
		 "Enable motion hazard handling during NLJ optimization and generate streaming material when appropriate")},

	{EopttraceDisableNonMasterGatherForDML,
	 &optimizer_enable_gather_on_segment_for_dml,
	 true,	// m_fNegate
	 GPOS_WSZ_LIT(
		 "Enable DML optimization by enforcing a non-master gather when appropriate")},

	{EopttraceEnforceCorrelatedExecution, &optimizer_enforce_subplans,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enforce correlated execution in the optimizer")},

	{EopttraceForceExpandedMDQAs, &optimizer_force_expanded_distinct_aggs,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Always pick plans that expand multiple distinct aggregates into join of single distinct aggregate in the optimizer")},
	{EopttraceDisablePushingCTEConsumerReqsToCTEProducer,
	 &optimizer_push_requirements_from_consumer_to_producer,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT(
		 "Optimize CTE producer plan on requirements enforced on top of CTE consumer")},

	{EopttraceDisablePruneUnusedComputedColumns,
	 &optimizer_prune_computed_columns,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Prune unused computed columns when pre-processing query")},

	{EopttraceForceThreeStageScalarDQA, &optimizer_force_three_stage_scalar_dqa,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Force optimizer to always pick 3 stage aggregate plan for scalar distinct qualified aggregate.")},

	{EopttraceEnableParallelAppend, &optimizer_parallel_union,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT("Enable parallel execution for UNION/UNION ALL queries.")},

	{EopttraceArrayConstraints, &optimizer_array_constraints,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Allows the constraint framework to derive array constraints in the optimizer.")},

	{EopttraceForceAggSkewAvoidance, &optimizer_force_agg_skew_avoidance,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Always pick a plan for aggregate distinct that minimizes skew.")},

	{EopttraceEnableEagerAgg, &optimizer_enable_eageragg,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable Eager Agg transform for pushing aggregate below an innerjoin.")},
	{EopttraceExpandFullJoin, &optimizer_expand_fulljoin,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable Expand Full Join transform for converting FULL JOIN into UNION ALL.")},
	{EopttracePenalizeSkewedHashJoin, &optimizer_penalize_skew,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT(
		 "Penalize a hash join with a skewed redistribute as a child.")},
	{EopttraceTranslateUnusedColrefs, &optimizer_prune_unused_columns,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Prune unused columns from the query.")},
	{EopttraceAllowGeneralPredicatesforDPE,
	 &optimizer_enable_range_predicate_dpe,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable range predicates for dynamic partition elimination.")},
	{EopttraceEnableRedistributeNLLOJInnerChild,
	 &optimizer_enable_redistribute_nestloop_loj_inner_child,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Enable plan alternatives where NLJ's inner child is redistributed")},
	{EopttraceForceComprehensiveJoinImplementation,
	 &optimizer_force_comprehensive_join_implementation,
	 false,	 // m_negate_param
	 GPOS_WSZ_LIT(
		 "Explore a nested loop join even if a hash join is possible")},
	{EopttraceDisableInnerHashJoin, &optimizer_enable_hashjoin,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Explore hash join alternatives")},
	{EopttraceDisableInnerNLJ, &optimizer_enable_nljoin,
	 true,	// m_negate_param
	 GPOS_WSZ_LIT("Enable nested loop join alternatives")},

};

//---------------------------------------------------------------------------
//	@function:
//		CConfigParamMapping::PackConfigParamInBitset
//
//	@doc:
//		Pack the GPDB config params into a bitset
//
//---------------------------------------------------------------------------
CBitSet *
CConfigParamMapping::PackConfigParamInBitset(
	CMemoryPool *mp,
	ULONG xform_id	// number of available xforms
)
{
	CBitSet *traceflag_bitset = GPOS_NEW(mp) CBitSet(mp, EopttraceSentinel);

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_elements); ul++)
	{
		SConfigMappingElem elem = m_elements[ul];
		GPOS_ASSERT(!traceflag_bitset->Get((ULONG) elem.m_trace_flag) &&
					"trace flag already set");

		BOOL value = *elem.m_is_param;
		if (elem.m_negate_param)
		{
			// negate the value of config param
			value = !value;
		}

		if (value)
		{
			BOOL is_traceflag_set GPOS_ASSERTS_ONLY =
				traceflag_bitset->ExchangeSet((ULONG) elem.m_trace_flag);
			GPOS_ASSERT(!is_traceflag_set);
		}
	}

	// pack disable flags of xforms
	for (ULONG ul = 0; ul < xform_id; ul++)
	{
		GPOS_ASSERT(!traceflag_bitset->Get(EopttraceDisableXformBase + ul) &&
					"xform trace flag already set");

		if (optimizer_xforms[ul])
		{
			BOOL is_traceflag_set GPOS_ASSERTS_ONLY =
				traceflag_bitset->ExchangeSet(EopttraceDisableXformBase + ul);
			GPOS_ASSERT(!is_traceflag_set);
		}
	}

	if (!optimizer_enable_nljoin)
	{
		CBitSet *nl_join_bitset = CXform::PbsNLJoinXforms(mp);
		traceflag_bitset->Union(nl_join_bitset);
		nl_join_bitset->Release();
	}

	if (!optimizer_enable_indexjoin)
	{
		CBitSet *index_join_bitset = CXform::PbsIndexJoinXforms(mp);
		traceflag_bitset->Union(index_join_bitset);
		index_join_bitset->Release();
	}

	// disable bitmap scan if the corresponding GUC is turned off
	if (!optimizer_enable_bitmapscan)
	{
		CBitSet *bitmap_index_bitset = CXform::PbsBitmapIndexXforms(mp);
		traceflag_bitset->Union(bitmap_index_bitset);
		bitmap_index_bitset->Release();
	}

	// disable outerjoin to unionall transformation if GUC is turned off
	if (!optimizer_enable_outerjoin_to_unionall_rewrite)
	{
		traceflag_bitset->ExchangeSet(GPOPT_DISABLE_XFORM_TF(
			CXform::ExfLeftOuter2InnerUnionAllLeftAntiSemiJoin));
	}

	// disable Assert MaxOneRow plans if GUC is turned off
	if (!optimizer_enable_assert_maxonerow)
	{
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfMaxOneRow2Assert));
	}

	if (!optimizer_enable_hashjoin)
	{
		// disable hash-join if the corresponding GUC is turned off
		CBitSet *hash_join_bitste = CXform::PbsHashJoinXforms(mp);
		traceflag_bitset->Union(hash_join_bitste);
		hash_join_bitste->Release();
	}

	if (!optimizer_enable_dynamictablescan)
	{
		// disable dynamic table scan if the corresponding GUC is turned off
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfDynamicGet2DynamicTableScan));
	}

	if (!optimizer_enable_tablescan)
	{
		// disable table scan if the corresponding GUC is turned off
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfGet2TableScan));
	}

	if (!optimizer_enable_indexscan)
	{
		// disable index scan if the corresponding GUC is turned off
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfIndexGet2IndexScan));
	}

	if (!optimizer_enable_indexonlyscan)
	{
		// disable index only scan if the corresponding GUC is turned off
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfIndexGet2IndexOnlyScan));
	}

	if (!optimizer_enable_hashagg)
	{
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfGbAgg2HashAgg));
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfGbAggDedup2HashAggDedup));
	}

	if (!optimizer_enable_groupagg)
	{
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfGbAgg2StreamAgg));
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfGbAggDedup2StreamAggDedup));
	}

	if (!optimizer_enable_mergejoin)
	{
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfImplementFullOuterMergeJoin));
	}

	CBitSet *join_heuristic_bitset = nullptr;
	switch (optimizer_join_order)
	{
		case JOIN_ORDER_IN_QUERY:
			join_heuristic_bitset = CXform::PbsJoinOrderInQueryXforms(mp);
			break;
		case JOIN_ORDER_GREEDY_SEARCH:
			join_heuristic_bitset = CXform::PbsJoinOrderOnGreedyXforms(mp);
			break;
		case JOIN_ORDER_EXHAUSTIVE_SEARCH:
			join_heuristic_bitset = CXform::PbsJoinOrderOnExhaustiveXforms(mp);
			break;
		case JOIN_ORDER_EXHAUSTIVE2_SEARCH:
			join_heuristic_bitset = CXform::PbsJoinOrderOnExhaustive2Xforms(mp);
			break;
		default:
			elog(ERROR,
				 "Invalid value for optimizer_join_order, must \
				 not come here");
			break;
	}
	traceflag_bitset->Union(join_heuristic_bitset);
	join_heuristic_bitset->Release();

	// disable join associativity transform if the corresponding GUC
	// is turned off independent of the join order algorithm chosen
	if (!optimizer_enable_associativity)
	{
		traceflag_bitset->ExchangeSet(
			GPOPT_DISABLE_XFORM_TF(CXform::ExfJoinAssociativity));
	}

	if (OPTIMIZER_GPDB_LEGACY == optimizer_cost_model)
	{
		traceflag_bitset->ExchangeSet(EopttraceLegacyCostModel);
	}
	else if (OPTIMIZER_GPDB_EXPERIMENTAL == optimizer_cost_model)
	{
		traceflag_bitset->ExchangeSet(EopttraceExperimentalCostModel);
	}

	// enable nested loop index plans using nest params
	// instead of outer reference as in the case with GPDB 4/5
	traceflag_bitset->ExchangeSet(EopttraceIndexedNLJOuterRefAsParams);

	// enable using opfamilies in distribution specs for GPDB 6
	traceflag_bitset->ExchangeSet(EopttraceConsiderOpfamiliesForDistribution);

	return traceflag_bitset;
}

// EOF
