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

#include "postgres.h"
#include "utils/guc.h"

#include "gpopt/config/CConfigParamMapping.h"
#include "gpopt/xforms/CXform.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

// array mapping GUCs to traceflags
CConfigParamMapping::SConfigMappingElem CConfigParamMapping::m_elem[] =
{
		{
		EopttracePrintQuery,
		&optimizer_print_query,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints the optimizer's input query expression tree.")
		},

		{
		EopttracePrintPlan,
		&optimizer_print_plan,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints the plan expression tree produced by the optimizer.")
		},

		{
		EopttracePrintXform,
		&optimizer_print_xform,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints the input and output expression trees of the optimizer transformations.")
		},

		{
		EopttraceDisablePrintXformRes,
		&optimizer_disable_xform_result_printing,
		false, // m_fNegate
		GPOS_WSZ_LIT("Disable printing input and output of xforms.")
		},

		{
		EopttracePrintMemoExplrd,
		&optimizer_print_memo_after_exploration,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints MEMO after exploration.")
		},

		{
		EopttracePrintMemoImpld,
		&optimizer_print_memo_after_implementation,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints MEMO after implementation.")
		},

		{
		EopttracePrintMemoOptd,
		&optimizer_print_memo_after_optimization,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints MEMO after optimization.")
		},

		{
		EopttracePrintScheduler,
		&optimizer_print_job_scheduler,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints jobs in scheduler on each job completion.")
		},

		{
		EopttracePrintExprProps,
		&optimizer_print_expression_properties,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints expression properties.")
		},

		{
		EopttracePrintGrpProps,
		&optimizer_print_group_properties,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints group properties.")
		},

		{
		EopttracePrintOptCtxt,
		&optimizer_print_optimization_context,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints optimization context.")
		},

		{
		EopttracePrintOptStats,
		&optimizer_print_optimization_stats,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prints optimization stats.")
		},

		{
		EopttraceMinidump,
		&optimizer_minidump,
		false, // m_fNegate
		GPOS_WSZ_LIT("Generate optimizer minidump.")
		},
             	
		{
		EopttraceDisableMotions,
		&optimizer_enable_motions,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion nodes in optimizer.")
		},

		{
		EopttraceDisableMotionBroadcast,
		&optimizer_enable_motion_broadcast,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion broadcast nodes in optimizer.")
		},

		{
		EopttraceDisableMotionGather,
		&optimizer_enable_motion_gather,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion gather nodes in optimizer.")
		},

		{
		EopttraceDisableMotionHashDistribute,
		&optimizer_enable_motion_redistribute,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion hash-distribute nodes in optimizer.")
		},

		{
		EopttraceDisableMotionRandom,
		&optimizer_enable_motion_redistribute,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion random nodes in optimizer.")
		},

		{
		EopttraceDisableMotionRountedDistribute,
		&optimizer_enable_motion_redistribute,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable motion routed-distribute nodes in optimizer.")
		},

		{
		EopttraceDisableSort,
		&optimizer_enable_sort,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable sort nodes in optimizer.")
		},

		{
		EopttraceDisableSpool,
		&optimizer_enable_materialize,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable spool nodes in optimizer.")
		},

		{
		EopttraceDisablePartPropagation,
		&optimizer_enable_partition_propagation,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable partition propagation nodes in optimizer.")
		},

		{
		EopttraceDisablePartSelection,
		&optimizer_enable_partition_selection,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable partition selection in optimizer.")
		},

		{
		EopttraceDisableOuterJoin2InnerJoinRewrite,
		&optimizer_enable_outerjoin_rewrite,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable outer join to inner join rewrite in optimizer.")
		},

		{
		EopttraceDonotDeriveStatsForAllGroups,
		&optimizer_enable_derive_stats_all_groups,
		true, // m_fNegate
		GPOS_WSZ_LIT("Disable deriving stats for all groups after exploration.")
		},

		{
		EopttraceEnableSpacePruning,
		&optimizer_enable_space_pruning,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable space pruning in optimizer.")
		},

		{
		EopttracePreferMultiStageAgg,
		&optimizer_prefer_multistage_agg,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prefer multistage agg in optimizer.")
		},

		{
		EopttraceDonotCollectMissingStatsCols,
		&optimizer_disable_missing_stats_collection,
		false, // m_fNegate
		GPOS_WSZ_LIT("Disable collection of columns with missing statistics.")
		},

		{
		EopttraceEnableRedistributeBroadcastHashJoin,
		&optimizer_enable_hashjoin_redistribute_broadcast_children,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable generating hash join plan where outer child is Redistribute and inner child is Broadcast.")
		},

		{
		EopttraceExtractDXLStats,
		&optimizer_extract_dxl_stats,
		false, // m_fNegate
		GPOS_WSZ_LIT("Extract plan stats in dxl.")
		},

		{
		EopttraceExtractDXLStatsAllNodes,
		&optimizer_extract_dxl_stats_all_nodes,
		false, // m_fNegate
		GPOS_WSZ_LIT("Extract plan stats for all physical dxl nodes.")
		},

		{
		EopttraceDeriveStatsForDPE,
		&optimizer_dpe_stats,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable stats derivation of partitioned tables with dynamic partition elimination.")
		},

		{
		EopttraceEnumeratePlans,
		&optimizer_enumerate_plans,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable plan enumeration.")
		},

		{
		EopttraceSamplePlans,
		&optimizer_sample_plans,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable plan sampling.")
		},

		{
		EopttraceEnableCTEInlining,
		&optimizer_cte_inlining,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable CTE inlining.")
		},

		{
		EopttraceEnableConstantExpressionEvaluation,
		&optimizer_enable_constant_expression_evaluation,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Enable constant expression evaluation in the optimizer")
		},

		{
		EopttraceUseExternalConstantExpressionEvaluationForInts,
		&optimizer_use_external_constant_expression_evaluation_for_ints,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Enable constant expression evaluation for integers in the optimizer")
		},

		{
		EopttraceApplyLeftOuter2InnerUnionAllLeftAntiSemiJoinDisregardingStats,
		&optimizer_apply_left_outer_to_union_all_disregarding_stats,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Always apply Left Outer Join to Inner Join UnionAll Left Anti Semi Join without looking at stats")
		},

		{
		EopttraceRemoveOrderBelowDML,
		&optimizer_remove_order_below_dml,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Remove OrderBy below a DML operation")
		},

		{
		EopttraceDisableReplicateInnerNLJOuterChild,
		&optimizer_enable_broadcast_nestloop_outer_child,
		true,  // m_fNegate
		GPOS_WSZ_LIT("Enable plan alternatives where NLJ's outer child is replicated")
		},

		{
		EopttraceEnforceCorrelatedExecution,
		&optimizer_enforce_subplans,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Enforce correlated execution in the optimizer")
		},

		{
		EopttracePreferExpandedMDQAs,
		&optimizer_prefer_expanded_distinct_aggs,
		false,  // m_fNegate
		GPOS_WSZ_LIT("Prefer plans that expand multiple distinct aggregates into join of single distinct aggregate")
		},

		{
		EopttraceDisablePushingCTEConsumerReqsToCTEProducer,
		&optimizer_push_requirements_from_consumer_to_producer,
		true,  // m_fNegate
		GPOS_WSZ_LIT("Optimize CTE producer plan on requirements enforced on top of CTE consumer")
		},

		{
		EopttraceDisablePruneUnusedComputedColumns,
		&optimizer_prune_computed_columns,
		true,  // m_fNegate
		GPOS_WSZ_LIT("Prune unused computed columns when pre-processing query")
		},

		{
		EopttracePreferScalarDQAMultiStageAgg,
		&optimizer_prefer_scalar_dqa_multistage_agg,
		false, // m_fNegate
		GPOS_WSZ_LIT("Prefer multistage aggregates for scalar distinct qualified aggregate in the optimizer.")
		},

		{
		EopttraceEnableParallelAppend,
		&optimizer_parallel_union,
		false, // m_fNegate
		GPOS_WSZ_LIT("Enable parallel execution for UNION/UNION ALL queries.")
		},

		{
		EopttraceArrayConstraints,
		&optimizer_array_constraints,
		false, // m_fNegate
		GPOS_WSZ_LIT("Allows the constraint framework to derive array constraints in the optimizer.")
		}
};

//---------------------------------------------------------------------------
//	@function:
//		CConfigParamMapping::PbsPack
//
//	@doc:
//		Pack the GPDB config params into a bitset
//
//---------------------------------------------------------------------------
CBitSet *
CConfigParamMapping::PbsPack
	(
	IMemoryPool *pmp,
	ULONG ulXforms // number of available xforms
	)
{
	CBitSet *pbs = GPOS_NEW(pmp) CBitSet(pmp, EopttraceSentinel);

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_elem); ul++)
	{
		SConfigMappingElem elem = m_elem[ul];
		GPOS_ASSERT(!pbs->FBit((ULONG) elem.m_etf) &&
					"trace flag already set");

		BOOL fVal = *elem.m_pfParam;
		if (elem.m_fNegate)
		{
			// negate the value of config param
			fVal = !fVal;
		}

		if (fVal)
		{
#ifdef GPOS_DEBUG
			BOOL fSet =
#endif // GPOS_DEBUG
				pbs->FExchangeSet((ULONG) elem.m_etf);
			GPOS_ASSERT(!fSet);
		}
	}

	// pack disable flags of xforms
	for (ULONG ul = 0; ul < ulXforms; ul++)
	{
		GPOS_ASSERT(!pbs->FBit(EopttraceDisableXformBase + ul) &&
					"xform trace flag already set");

		if (optimizer_xforms[ul])
		{
#ifdef GPOS_DEBUG
			BOOL fSet =
#endif // GPOS_DEBUG
				pbs->FExchangeSet(EopttraceDisableXformBase + ul);
			GPOS_ASSERT(!fSet);
		}
	}

	// disable index-join if the corresponding GUC is turned off
	if (!optimizer_enable_indexjoin)
	{
		CBitSet *pbsIndexJoin = CXform::PbsIndexJoinXforms(pmp);
		pbs->Union(pbsIndexJoin);
		pbsIndexJoin->Release();
	}

	// disable bitmap scan if the corresponding GUC is turned off
	if (!optimizer_enable_bitmapscan)
	{
		CBitSet *pbsBitmapScan = CXform::PbsBitmapIndexXforms(pmp);
		pbs->Union(pbsBitmapScan);
		pbsBitmapScan->Release();
	}

	// disable outerjoin to unionall transformation if GUC is turned off
	if (!optimizer_enable_outerjoin_to_unionall_rewrite)
	{
		 pbs->FExchangeSet(GPOPT_DISABLE_XFORM_TF(CXform::ExfLeftOuter2InnerUnionAllLeftAntiSemiJoin));
	}

	// disable Assert MaxOneRow plans if GUC is turned off
	if (!optimizer_enable_assert_maxonerow)
	{
		 pbs->FExchangeSet(GPOPT_DISABLE_XFORM_TF(CXform::ExfMaxOneRow2Assert));
	}

	CBitSet *pbsHeterogeneousIndex = CXform::PbsHeterogeneousIndexXforms(pmp);
	pbs->Union(pbsHeterogeneousIndex);
	pbsHeterogeneousIndex->Release();

	return pbs;
}

// EOF
