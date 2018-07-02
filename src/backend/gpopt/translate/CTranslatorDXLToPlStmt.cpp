//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CTranslatorDXLToPlStmt.cpp
//
//	@doc:
//		Implementation of the methods for translating from DXL tree to GPDB
//		PlannedStmt.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "nodes/nodes.h"
#include "nodes/plannodes.h"
#include "nodes/primnodes.h"
#include "catalog/gp_policy.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_collation.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/partitionselection.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/uri.h"
#include "gpos/base.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CIndexQualInfo.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"

#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/IMDRelationExternal.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpos;
using namespace gpopt;
using namespace gpmd;

#define GPDXL_ROOT_PLAN_ID -1
#define GPDXL_PLAN_ID_START 1
#define GPDXL_MOTION_ID_START 1
#define GPDXL_PARAM_ID_START 0

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::CTranslatorDXLToPlStmt
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CTranslatorDXLToPlStmt::CTranslatorDXLToPlStmt
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CContextDXLToPlStmt* dxl_to_plstmt_context,
	ULONG num_of_segments
	)
	:
	m_memory_pool(memory_pool),
	m_md_accessor(md_accessor),
	m_dxl_to_plstmt_context(dxl_to_plstmt_context),
	m_cmd_type(CMD_SELECT),
	m_is_tgt_tbl_distributed(false),
	m_result_rel_list(NULL),
	m_external_scan_counter(0),
	m_num_of_segments(num_of_segments),
	m_partition_selector_counter(0)
{
	m_translator_dxl_to_scalar = GPOS_NEW(m_memory_pool) CTranslatorDXLToScalar(m_memory_pool, m_md_accessor, m_num_of_segments);
	InitTranslators();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::~CTranslatorDXLToPlStmt
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CTranslatorDXLToPlStmt::~CTranslatorDXLToPlStmt()
{
	GPOS_DELETE(m_translator_dxl_to_scalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::InitTranslators
//
//	@doc:
//		Initialize index of translators
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::InitTranslators()
{
	for (ULONG idx = 0; idx < GPOS_ARRAY_SIZE(m_rgpfTranslators); idx++)
	{
		m_rgpfTranslators[idx] = NULL;
	}

	// array mapping operator type to translator function
	static const STranslatorMapping rgTranslators[] =
	{
			{EdxlopPhysicalTableScan,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLTblScan},
			{EdxlopPhysicalExternalScan,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLTblScan},
			{EdxlopPhysicalIndexScan,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLIndexScan},
			{EdxlopPhysicalHashJoin, 				&gpopt::CTranslatorDXLToPlStmt::PhjFromDXLHJ},
			{EdxlopPhysicalNLJoin, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLNLJoin},
			{EdxlopPhysicalMergeJoin,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMergeJoin},
			{EdxlopPhysicalMotionGather,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMotion},
			{EdxlopPhysicalMotionBroadcast,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMotion},
			{EdxlopPhysicalMotionRedistribute,		&gpopt::CTranslatorDXLToPlStmt::PplanTranslateDXLMotion},
			{EdxlopPhysicalMotionRandom,			&gpopt::CTranslatorDXLToPlStmt::PplanTranslateDXLMotion},
			{EdxlopPhysicalMotionRoutedDistribute,	&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMotion},
			{EdxlopPhysicalLimit, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLLimit},
			{EdxlopPhysicalAgg, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLAgg},
			{EdxlopPhysicalWindow, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLWindow},
			{EdxlopPhysicalSort,					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLSort},
			{EdxlopPhysicalSubqueryScan,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLSubQueryScan},
			{EdxlopPhysicalResult, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLResult},
			{EdxlopPhysicalAppend, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLAppend},
			{EdxlopPhysicalMaterialize, 			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMaterialize},
			{EdxlopPhysicalSequence, 				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLSequence},
			{EdxlopPhysicalDynamicTableScan,		&gpopt::CTranslatorDXLToPlStmt::TranslateDXLDynTblScan},
			{EdxlopPhysicalDynamicIndexScan,		&gpopt::CTranslatorDXLToPlStmt::TranslateDXLDynIdxScan},
			{EdxlopPhysicalTVF,						&gpopt::CTranslatorDXLToPlStmt::TranslateDXLTvf},
			{EdxlopPhysicalDML,						&gpopt::CTranslatorDXLToPlStmt::TranslateDXLDml},
			{EdxlopPhysicalSplit,					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLSplit},
			{EdxlopPhysicalRowTrigger,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLRowTrigger},
			{EdxlopPhysicalAssert,					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLAssert},
			{EdxlopPhysicalCTEProducer, 			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLCTEProducerToSharedScan},
			{EdxlopPhysicalCTEConsumer, 			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLCTEConsumerToSharedScan},
			{EdxlopPhysicalBitmapTableScan,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLBitmapTblScan},
			{EdxlopPhysicalDynamicBitmapTableScan,	&gpopt::CTranslatorDXLToPlStmt::TranslateDXLBitmapTblScan},
			{EdxlopPhysicalCTAS, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLCtas},
			{EdxlopPhysicalPartitionSelector,		&gpopt::CTranslatorDXLToPlStmt::TranslateDXLPartSelector},
			{EdxlopPhysicalValuesScan,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLValueScan},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);

	for (ULONG idx = 0; idx < ulTranslators; idx++)
	{
		STranslatorMapping elem = rgTranslators[idx];
		m_rgpfTranslators[elem.edxlopid] = elem.dxlnode_to_logical_funct;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::GetPlannedStmtFromDXL
//
//	@doc:
//		Translate DXL node into a PlannedStmt
//
//---------------------------------------------------------------------------
PlannedStmt *
CTranslatorDXLToPlStmt::GetPlannedStmtFromDXL
	(
	const CDXLNode *dxlnode,
	bool can_set_tag
	)
{
	GPOS_ASSERT(NULL != dxlnode);

	CDXLTranslateContext dxltrctx(m_memory_pool, false);

	DXLTranslationContextArr *ctxt_translation_prev_siblings = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	Plan *plan = TranslateDXLOperatorToPlan(dxlnode, &dxltrctx, ctxt_translation_prev_siblings);
	ctxt_translation_prev_siblings->Release();

	GPOS_ASSERT(NULL != plan);

	// collect oids from rtable
	List *plOids = NIL;

	ListCell *plcRTE = NULL;
	ForEach (plcRTE, m_dxl_to_plstmt_context->GetRTableEntriesList())
	{
		RangeTblEntry *pRTE = (RangeTblEntry *) lfirst(plcRTE);

		if (pRTE->rtekind == RTE_RELATION)
		{
			plOids = gpdb::LAppendOid(plOids, pRTE->relid);
		}
	}

	// assemble planned stmt
	PlannedStmt *pplstmt = MakeNode(PlannedStmt);
	pplstmt->planGen = PLANGEN_OPTIMIZER;
	
	pplstmt->rtable = m_dxl_to_plstmt_context->GetRTableEntriesList();
	pplstmt->subplans = m_dxl_to_plstmt_context->GetSubplanEntriesList();
	pplstmt->planTree = plan;

	// store partitioned table indexes in planned stmt
	pplstmt->queryPartOids = m_dxl_to_plstmt_context->GetPartitionedTablesList();
	pplstmt->canSetTag = can_set_tag;
	pplstmt->relationOids = plOids;
	pplstmt->numSelectorsPerScanId = m_dxl_to_plstmt_context->GetNumPartitionSelectorsList();

	plan->nMotionNodes  = m_dxl_to_plstmt_context->GetCurrentMotionId()-1;
	pplstmt->nMotionNodes =  m_dxl_to_plstmt_context->GetCurrentMotionId()-1;

	pplstmt->commandType = m_cmd_type;
	
	GPOS_ASSERT(plan->nMotionNodes >= 0);
	if (0 == plan->nMotionNodes && !m_is_tgt_tbl_distributed)
	{
		// no motion nodes and not a DML on a distributed table
		plan->dispatch = DISPATCH_SEQUENTIAL;
	}
	else
	{
		plan->dispatch = DISPATCH_PARALLEL;
	}
	
	pplstmt->resultRelations = m_result_rel_list;
	pplstmt->intoClause = m_dxl_to_plstmt_context->GetIntoClause();
	pplstmt->intoPolicy = m_dxl_to_plstmt_context->GetDistributionPolicy();
	
	SetInitPlanVariables(pplstmt);
	
	if (CMD_SELECT == m_cmd_type && NULL != dxlnode->GetDXLDirectDispatchInfo())
	{
		List *plDirectDispatchSegIds = TranslateDXLDirectDispatchInfo(dxlnode->GetDXLDirectDispatchInfo());
		plan->directDispatch.contentIds = plDirectDispatchSegIds;
		plan->directDispatch.isDirectDispatch = (NIL != plDirectDispatchSegIds);
		
		if (plan->directDispatch.isDirectDispatch)
		{
			List *plMotions = gpdb::ExtractNodesPlan(pplstmt->planTree, T_Motion, true /*descendIntoSubqueries*/);
			ListCell *lc = NULL;
			ForEach(lc, plMotions)
			{
				Motion *pmotion = (Motion *) lfirst(lc);
				GPOS_ASSERT(IsA(pmotion, Motion));
				GPOS_ASSERT(gpdb::IsMotionGather(pmotion));
				
				pmotion->plan.directDispatch.isDirectDispatch = true;
				pmotion->plan.directDispatch.contentIds = plan->directDispatch.contentIds;
			}
		}
	}
	
	return pplstmt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLOperatorToPlan
//
//	@doc:
//		Translates a DXL tree into a Plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLOperatorToPlan
	(
	const CDXLNode *dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != ctxt_translation_prev_siblings);

	CDXLOperator *dxlop = dxlnode->GetOperator();
	ULONG ulOpId =  (ULONG) dxlop->GetDXLOperator();

	PfPplan dxlnode_to_logical_funct = m_rgpfTranslators[ulOpId];

	if (NULL == dxlnode_to_logical_funct)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	return (this->* dxlnode_to_logical_funct)(dxlnode, output_context, ctxt_translation_prev_siblings);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::SetInitPlanVariables
//
//	@doc:
//		Iterates over the plan to set the qDispSliceId that is found in the plan
//		as well as its subplans. Set the number of parameters used in the plan.
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::SetInitPlanVariables(PlannedStmt* pplstmt)
{
	if(1 != m_dxl_to_plstmt_context->GetCurrentMotionId()) // For Distributed Tables m_ulMotionId > 1
	{
		pplstmt->nInitPlans = m_dxl_to_plstmt_context->GetCurrentParamId();
		pplstmt->planTree->nInitPlans = m_dxl_to_plstmt_context->GetCurrentParamId();
	}

	pplstmt->nParamExec = m_dxl_to_plstmt_context->GetCurrentParamId();

	// Extract all subplans defined in the planTree
	List *plSubPlans = gpdb::ExtractNodesPlan(pplstmt->planTree, T_SubPlan, true);

	ListCell *lc = NULL;

	ForEach (lc, plSubPlans)
	{
		SubPlan *psubplan = (SubPlan*) lfirst(lc);
		if (psubplan->is_initplan)
		{
			SetInitPlanSliceInformation(psubplan);
		}
	}

	// InitPlans can also be defined in subplans. We therefore have to iterate
	// over all the subplans referred to in the planned statement.

	List *plInitPlans = pplstmt->subplans;

	ForEach (lc,plInitPlans)
	{
		plSubPlans = gpdb::ExtractNodesPlan((Plan*) lfirst(lc), T_SubPlan, true);
		ListCell *plc2;

		ForEach (plc2, plSubPlans)
		{
			SubPlan *psubplan = (SubPlan*) lfirst(plc2);
			if (psubplan->is_initplan)
			{
				SetInitPlanSliceInformation(psubplan);
			}
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::SetInitPlanSliceInformation
//
//	@doc:
//		Set the qDispSliceId for a given subplans. In GPDB once all motion node
// 		have been assigned a slice, each initplan is assigned a slice number.
//		The initplan are traversed in an postorder fashion. Since in CTranslatorDXLToPlStmt
//		we assign the plan_id to each initplan in a postorder fashion, we take
//		advantage of this.
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::SetInitPlanSliceInformation(SubPlan * psubplan)
{
	GPOS_ASSERT(psubplan->is_initplan && "This is processed for initplans only");

	if (psubplan->is_initplan)
	{
		GPOS_ASSERT(0 < m_dxl_to_plstmt_context->GetCurrentMotionId());

		if(1 < m_dxl_to_plstmt_context->GetCurrentMotionId())
		{
			psubplan->qDispSliceId =  m_dxl_to_plstmt_context->GetCurrentMotionId() + psubplan->plan_id-1;
		}
		else
		{
			psubplan->qDispSliceId = 0;
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::SetParamIds
//
//	@doc:
//		Set the bitmapset with the param_ids defined in the plan
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::SetParamIds(Plan* plan)
{
	List *plParams = gpdb::ExtractNodesPlan(plan, T_Param, true);

	ListCell *lc = NULL;

	Bitmapset  *pbitmapset = NULL;

	ForEach (lc, plParams)
	{
		Param *pparam = (Param*) lfirst(lc);
		pbitmapset = gpdb::BmsAddMember(pbitmapset, pparam->paramid);
	}

	plan->extParam = pbitmapset;
	plan->allParam = pbitmapset;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLTblScan
//
//	@doc:
//		Translates a DXL table scan node into a TableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLTblScan
	(
	const CDXLNode *tbl_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalTableScan *pdxlopTS = CDXLPhysicalTableScan::Cast(tbl_scan_dxlnode->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const CDXLTableDescr *table_descr = pdxlopTS->GetDXLTableDescr();
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(table_descr, NULL /*index_descr_dxl*/, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	Plan *plan = NULL;
	Plan *pplanReturn = NULL;
	if (IMDRelation::ErelstorageExternal == md_rel->RetrieveRelStorageType())
	{
		const IMDRelationExternal *pmdrelext = dynamic_cast<const IMDRelationExternal*>(md_rel);
		OID oidRel = CMDIdGPDB::CastMdid(md_rel->MDId())->OidObjectId();
		ExtTableEntry *pextentry = gpdb::GetExtTableEntry(oidRel);
		bool isMasterOnly;
		
		// create external scan node
		ExternalScan *pes = MakeNode(ExternalScan);
		pes->scan.scanrelid = index;
		pes->uriList = gpdb::GetExternalScanUriList(pextentry, &isMasterOnly);
		pes->fmtOptString = pextentry->fmtopts;
		pes->fmtType = pextentry->fmtcode;
		pes->isMasterOnly = isMasterOnly;
		GPOS_ASSERT((IMDRelation::EreldistrMasterOnly == pmdrelext->GetRelDistribution()) == isMasterOnly);
		pes->logErrors = pextentry->logerrors;
		pes->rejLimit = pmdrelext->RejectLimit();
		pes->rejLimitInRows = pmdrelext->IsRejectLimitInRows();

		pes->encoding = pextentry->encoding;
		pes->scancounter = m_external_scan_counter++;

		plan = &(pes->scan.plan);
		pplanReturn = (Plan *) pes;
	}
	else
	{
		// create table scan node
		TableScan *pts = MakeNode(TableScan);
		pts->scanrelid = index;
		plan = &(pts->plan);
		pplanReturn = (Plan *) pts;
	}

	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(tbl_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// a table scan node must have 2 children: projection list and filter
	GPOS_ASSERT(2 == tbl_scan_dxlnode->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*tbl_scan_dxlnode)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*tbl_scan_dxlnode)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		NULL,			// pdxltrctxLeft and pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	SetParamIds(plan);

	return pplanReturn;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::SetIndexVarAttnoWalker
//
//	@doc:
//		Walker to set index var attno's,
//		attnos of index vars are set to their relative positions in index keys,
//		skip any outer references while walking the expression tree
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToPlStmt::SetIndexVarAttnoWalker
	(
	Node *node,
	SContextIndexVarAttno *ctxt_index_var_attno_walker
	)
{
	if (NULL == node)
	{
		return false;
	}

	if (IsA(node, Var) && ((Var *)node)->varno != OUTER)
	{
		INT attno = ((Var *)node)->varattno;
		const IMDRelation *md_rel = ctxt_index_var_attno_walker->m_md_rel;
		const IMDIndex *index = ctxt_index_var_attno_walker->m_md_index;

		ULONG ulIndexColPos = gpos::ulong_max;
		const ULONG arity = md_rel->ColumnCount();
		for (ULONG ulColPos = 0; ulColPos < arity; ulColPos++)
		{
			const IMDColumn *pmdcol = md_rel->GetMdCol(ulColPos);
			if (attno == pmdcol->AttrNum())
			{
				ulIndexColPos = ulColPos;
				break;
			}
		}

		if (gpos::ulong_max > ulIndexColPos)
		{
			((Var *)node)->varattno =  1 + index->GetKeyPos(ulIndexColPos);
		}

		return false;
	}

	return gpdb::WalkExpressionTree
			(
			node,
			(BOOL (*)()) CTranslatorDXLToPlStmt::SetIndexVarAttnoWalker,
			ctxt_index_var_attno_walker
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLIndexScan
//
//	@doc:
//		Translates a DXL index scan node into a IndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLIndexScan
	(
	const CDXLNode *index_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalIndexScan *pdxlopIndexScan = CDXLPhysicalIndexScan::Cast(index_scan_dxlnode->GetOperator());

	return TranslateDXLIndexScan(index_scan_dxlnode, pdxlopIndexScan, output_context, false /*is_index_only_scan*/, ctxt_translation_prev_siblings);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLIndexScan
//
//	@doc:
//		Translates a DXL index scan node into a IndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLIndexScan
	(
	const CDXLNode *index_scan_dxlnode,
	CDXLPhysicalIndexScan *pdxlopIndexScan,
	CDXLTranslateContext *output_context,
	BOOL is_index_only_scan,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const CDXLIndexDescr *index_descr_dxl = NULL;
	if (is_index_only_scan)
	{
		index_descr_dxl = pdxlopIndexScan->GetDXLIndexDescr();
	}

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(pdxlopIndexScan->GetDXLTableDescr()->MDId());

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(pdxlopIndexScan->GetDXLTableDescr(), index_descr_dxl, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	IndexScan *pis = NULL;
	GPOS_ASSERT(!is_index_only_scan);
	pis = MakeNode(IndexScan);
	pis->scan.scanrelid = index;

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(pdxlopIndexScan->GetDXLIndexDescr()->MDId());
	const IMDIndex *md_index = m_md_accessor->RetrieveIndex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pis->indexid = oidIndex;

	Plan *plan = &(pis->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(index_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// an index scan node must have 3 children: projection list, filter and index condition list
	GPOS_ASSERT(3 == index_scan_dxlnode->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*index_scan_dxlnode)[EdxlisIndexProjList];
	CDXLNode *filter_dxlnode = (*index_scan_dxlnode)[EdxlisIndexFilter];
	CDXLNode *index_cond_list_dxlnode = (*index_scan_dxlnode)[EdxlisIndexCondition];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxl, &dxltrctxbt, NULL /*child_contexts*/, output_context);

	// translate index filter
	plan->qual = TranslateDXLIndexFilter
					(
					filter_dxlnode,
					output_context,
					&dxltrctxbt,
					ctxt_translation_prev_siblings
					);

	pis->indexorderdir = CTranslatorUtils::GetScanDirection(pdxlopIndexScan->GetIndexScanDir());

	// translate index condition list
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		index_cond_list_dxlnode, 
		pdxlopIndexScan->GetDXLTableDescr(),
		is_index_only_scan, 
		md_index,
		md_rel,
		output_context,
		&dxltrctxbt, 
		ctxt_translation_prev_siblings,
		&plIndexConditions, 
		&plIndexOrigConditions, 
		&plIndexStratgey, 
		&plIndexSubtype
		);

	pis->indexqual = plIndexConditions;
	pis->indexqualorig = plIndexOrigConditions;
	/*
	 * As of 8.4, the indexstrategy and indexsubtype fields are no longer
	 * available or needed in IndexScan. Ignore them.
	 */
	SetParamIds(plan);

	return (Plan *) pis;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateIndexFilter
//
//	@doc:
//		Translate the index filter list in an Index scan
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLIndexFilter
	(
	CDXLNode *filter_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	List *plQuals = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, base_table_context, ctxt_translation_prev_siblings, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = filter_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *index_filter_dxlnode = (*filter_dxlnode)[ul];
		Expr *pexprIndexFilter = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(index_filter_dxlnode, &mapcidvarplstmt);
		plQuals = gpdb::LAppend(plQuals, pexprIndexFilter);
	}

	return plQuals;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateIndexConditions
//
//	@doc:
//		Translate the index condition list in an Index scan
//
//---------------------------------------------------------------------------
void 
CTranslatorDXLToPlStmt::TranslateIndexConditions
	(
	CDXLNode *index_cond_list_dxlnode,
	const CDXLTableDescr *dxl_tbl_descr,
	BOOL is_index_only_scan,
	const IMDIndex *index,
	const IMDRelation *md_rel,
	CDXLTranslateContext *output_context,
	CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings,
	List **index_cond,
	List **index_orig_cond,
	List **index_strategy_list,
	List **index_subtype_list
	)
{
	// array of index qual info
	IndexQualInfoArray *pdrgpindexqualinfo = GPOS_NEW(m_memory_pool) IndexQualInfoArray(m_memory_pool);

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, base_table_context, ctxt_translation_prev_siblings, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = index_cond_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnIndexCond = (*index_cond_list_dxlnode)[ul];

		Expr *pexprOrigIndexCond = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnIndexCond, &mapcidvarplstmt);
		Expr *pexprIndexCond = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnIndexCond, &mapcidvarplstmt);
		GPOS_ASSERT((IsA(pexprIndexCond, OpExpr) || IsA(pexprIndexCond, ScalarArrayOpExpr))
				&& "expected OpExpr or ScalarArrayOpExpr in index qual");

		if (IsA(pexprIndexCond, ScalarArrayOpExpr) && IMDIndex::EmdindBitmap != index->IndexType())
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, GPOS_WSZ_LIT("ScalarArrayOpExpr condition on index scan"));
		}

		// for indexonlyscan, we already have the attno referring to the index
		if (!is_index_only_scan)
		{
			// Otherwise, we need to perform mapping of Varattnos relative to column positions in index keys
			SContextIndexVarAttno ctxtidxvarattno(md_rel, index);
			SetIndexVarAttnoWalker((Node *) pexprIndexCond, &ctxtidxvarattno);
		}
		
		// find index key's attno
		List *plistArgs = NULL;
		if (IsA(pexprIndexCond, OpExpr))
		{
			plistArgs = ((OpExpr *) pexprIndexCond)->args;
		}
		else
		{
			plistArgs = ((ScalarArrayOpExpr *) pexprIndexCond)->args;
		}

		Node *left_arg = (Node *) lfirst(gpdb::ListHead(plistArgs));
		Node *right_arg = (Node *) lfirst(gpdb::ListTail(plistArgs));
				
		BOOL fRelabel = false;
		if (IsA(left_arg, RelabelType) && IsA(((RelabelType *) left_arg)->arg, Var))
		{
			left_arg = (Node *) ((RelabelType *) left_arg)->arg;
			fRelabel = true;
		}
		else if (IsA(right_arg, RelabelType) && IsA(((RelabelType *) right_arg)->arg, Var))
		{
			right_arg = (Node *) ((RelabelType *) right_arg)->arg;
			fRelabel = true;
		}
		
		if (fRelabel)
		{
			List *plNewArgs = ListMake2(left_arg, right_arg);
			gpdb::GPDBFree(plistArgs);
			if (IsA(pexprIndexCond, OpExpr))
			{
				((OpExpr *) pexprIndexCond)->args = plNewArgs;
			}
			else
			{
				((ScalarArrayOpExpr *) pexprIndexCond)->args = plNewArgs;
			}
		}
		
		GPOS_ASSERT((IsA(left_arg, Var) || IsA(right_arg, Var)) && "expected index key in index qual");

		INT attno = 0;
		if (IsA(left_arg, Var) && ((Var *) left_arg)->varno != OUTER)
		{
			// index key is on the left side
			attno =  ((Var *) left_arg)->varattno;
		}
		else
		{
			// index key is on the right side
			GPOS_ASSERT(((Var *) right_arg)->varno != OUTER && "unexpected outer reference in index qual");
			attno = ((Var *) right_arg)->varattno;
		}
		
		// retrieve index strategy and subtype
		INT iSN = 0;
		OID oidIndexSubtype = InvalidOid;
		
		OID oidCmpOperator = CTranslatorUtils::OidCmpOperator(pexprIndexCond);
		GPOS_ASSERT(InvalidOid != oidCmpOperator);
		OID oidOpFamily = CTranslatorUtils::GetOpFamilyForIndexQual(attno, CMDIdGPDB::CastMdid(index->MDId())->OidObjectId());
		GPOS_ASSERT(InvalidOid != oidOpFamily);
		gpdb::IndexOpProperties(oidCmpOperator, oidOpFamily, &iSN, &oidIndexSubtype);
		
		// create index qual
		pdrgpindexqualinfo->Append(GPOS_NEW(m_memory_pool) CIndexQualInfo(attno, pexprIndexCond, pexprOrigIndexCond, (StrategyNumber) iSN, oidIndexSubtype));
	}

	// the index quals much be ordered by attribute number
	pdrgpindexqualinfo->Sort(CIndexQualInfo::IndexQualInfoCmp);

	ULONG ulLen = pdrgpindexqualinfo->Size();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		CIndexQualInfo *pindexqualinfo = (*pdrgpindexqualinfo)[ul];
		*index_cond = gpdb::LAppend(*index_cond, pindexqualinfo->m_expr);
		*index_orig_cond = gpdb::LAppend(*index_orig_cond, pindexqualinfo->m_original_expr);
		*index_strategy_list = gpdb::LAppendInt(*index_strategy_list, pindexqualinfo->m_index_subtype_oid);
		*index_subtype_list = gpdb::LAppendOid(*index_subtype_list, pindexqualinfo->m_index_subtype_oid);
	}

	// clean up
	pdrgpindexqualinfo->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLAssertConstraints
//
//	@doc:
//		Translate the constraints from an Assert node into a list of quals
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLAssertConstraints
	(
	CDXLNode *pdxlnAssertConstraintList,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *child_contexts
	)
{
	List *plQuals = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, NULL /*base_table_context*/, child_contexts, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = pdxlnAssertConstraintList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnpdxlnAssertConstraint = (*pdxlnAssertConstraintList)[ul];
		Expr *pexprAssertConstraint = m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnpdxlnAssertConstraint)[0], &mapcidvarplstmt);
		plQuals = gpdb::LAppend(plQuals, pexprAssertConstraint);
	}

	return plQuals;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLLimit
//
//	@doc:
//		Translates a DXL Limit node into a Limit node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLLimit
	(
	const CDXLNode *limit_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create limit node
	Limit *plimit = MakeNode(Limit);

	Plan *plan = &(plimit->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(limit_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	GPOS_ASSERT(4 == limit_dxlnode->Arity());

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	// translate proj list
	CDXLNode *project_list_dxl = (*limit_dxlnode)[EdxllimitIndexProjList];
	CDXLNode *pdxlnChildPlan = (*limit_dxlnode)[EdxllimitIndexChildPlan];
	CDXLNode *pdxlnLimitCount = (*limit_dxlnode)[EdxllimitIndexLimitCount];
	CDXLNode *pdxlnLimitOffset = (*limit_dxlnode)[EdxllimitIndexLimitOffset];

	// NOTE: Limit node has only the left plan while the right plan is left empty
	Plan *pplanLeft = TranslateDXLOperatorToPlan(pdxlnChildPlan, &dxltrctxLeft, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxLeft);

	plan->targetlist = TranslateDXLProjList
								(
								project_list_dxl,
								NULL,		// base table translation context
								child_contexts,
								output_context
								);

	plan->lefttree = pplanLeft;

	if(NULL != pdxlnLimitCount && pdxlnLimitCount->Arity() >0)
	{
		CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, NULL, child_contexts, output_context, m_dxl_to_plstmt_context);
		Node *pnodeLimitCount = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnLimitCount)[0], &mapcidvarplstmt);
		plimit->limitCount = pnodeLimitCount;
	}

	if(NULL != pdxlnLimitOffset && pdxlnLimitOffset->Arity() >0)
	{
		CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, NULL, child_contexts, output_context, m_dxl_to_plstmt_context);
		Node *pexprLimitOffset = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnLimitOffset)[0], &mapcidvarplstmt);
		plimit->limitOffset = pexprLimitOffset;
	}

	plan->nMotionNodes = pplanLeft->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return  (Plan *) plimit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PhjFromDXLHJ
//
//	@doc:
//		Translates a DXL hash join node into a HashJoin node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PhjFromDXLHJ
	(
	const CDXLNode *hj_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(hj_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalHashJoin);
	GPOS_ASSERT(hj_dxlnode->Arity() == EdxlhjIndexSentinel);

	// create hash join node
	HashJoin *phj = MakeNode(HashJoin);

	Join *pj = &(phj->join);
	Plan *plan = &(pj->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalHashJoin *pdxlopHashJoin = CDXLPhysicalHashJoin::Cast(hj_dxlnode->GetOperator());

	// set join type
	pj->jointype = GetGPDBJoinTypeFromDXLJoinType(pdxlopHashJoin->GetJoinType());
	pj->prefetch_inner = true;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(hj_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*hj_dxlnode)[EdxlhjIndexHashLeft];
	CDXLNode *pdxlnRight = (*hj_dxlnode)[EdxlhjIndexHashRight];
	CDXLNode *project_list_dxl = (*hj_dxlnode)[EdxlhjIndexProjList];
	CDXLNode *filter_dxlnode = (*hj_dxlnode)[EdxlhjIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*hj_dxlnode)[EdxlhjIndexJoinFilter];
	CDXLNode *pdxlnHashCondList = (*hj_dxlnode)[EdxlhjIndexHashCondList];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanLeft = TranslateDXLOperatorToPlan(pdxlnLeft, &dxltrctxLeft, ctxt_translation_prev_siblings);

	// the right side of the join is the one where the hash phase is done
	DXLTranslationContextArr *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
	pdrgpdxltrctxWithSiblings->AppendArray(ctxt_translation_prev_siblings);
	Plan *pplanRight = (Plan*) TranslateDXLHash(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxLeft));
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxRight));
	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join filter
	pj->joinqual = TranslateDXLFilterToQual
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	// translate hash cond
	List *plHashConditions = NIL;

	BOOL fHasINDFCond = false;

	const ULONG arity = pdxlnHashCondList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnHashCond = (*pdxlnHashCondList)[ul];

		List *plHashCond = TranslateDXLScCondToQual
				(
				pdxlnHashCond,
				NULL,			// base table translation context
				child_contexts,
				output_context
				);

		GPOS_ASSERT(1 == gpdb::ListLength(plHashCond));

		Expr *expr = (Expr *) LInitial(plHashCond);
		if (IsA(expr, BoolExpr) && ((BoolExpr *) expr)->boolop == NOT_EXPR)
		{
			// INDF test
			GPOS_ASSERT(gpdb::ListLength(((BoolExpr *) expr)->args) == 1 &&
						(IsA((Expr *) LInitial(((BoolExpr *) expr)->args), DistinctExpr)));
			fHasINDFCond = true;
		}
		plHashConditions = gpdb::ListConcat(plHashConditions, plHashCond);
	}

	if (!fHasINDFCond)
	{
		// no INDF conditions in the hash condition list
		phj->hashclauses = plHashConditions;
	}
	else
	{
		// hash conditions contain INDF clauses -> extract equality conditions to
		// construct the hash clauses list
		List *plHashClauses = NIL;

		for (ULONG ul = 0; ul < arity; ul++)
		{
			CDXLNode *pdxlnHashCond = (*pdxlnHashCondList)[ul];

			// condition can be either a scalar comparison or a NOT DISTINCT FROM expression
			GPOS_ASSERT(EdxlopScalarCmp == pdxlnHashCond->GetOperator()->GetDXLOperator() ||
						EdxlopScalarBoolExpr == pdxlnHashCond->GetOperator()->GetDXLOperator());

			if (EdxlopScalarBoolExpr == pdxlnHashCond->GetOperator()->GetDXLOperator())
			{
				// clause is a NOT DISTINCT FROM check -> extract the distinct comparison node
				GPOS_ASSERT(Edxlnot == CDXLScalarBoolExpr::Cast(pdxlnHashCond->GetOperator())->GetDxlBoolTypeStr());
				pdxlnHashCond = (*pdxlnHashCond)[0];
				GPOS_ASSERT(EdxlopScalarDistinct == pdxlnHashCond->GetOperator()->GetDXLOperator());
			}

			CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
														(
														m_memory_pool,
														NULL,
														child_contexts,
														output_context,
														m_dxl_to_plstmt_context
														);

			// translate the DXL scalar or scalar distinct comparison into an equality comparison
			// to store in the hash clauses
			Expr *pexpr2 = (Expr *) m_translator_dxl_to_scalar->CreateScalarCmpExprFromDXL
									(
									pdxlnHashCond,
									&mapcidvarplstmt
									);

			plHashClauses = gpdb::LAppend(plHashClauses, pexpr2);
		}

		phj->hashclauses = plHashClauses;
		phj->hashqualclauses = plHashConditions;
	}

	GPOS_ASSERT(NIL != phj->hashclauses);

	plan->lefttree = pplanLeft;
	plan->righttree = pplanRight;
	plan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	child_contexts->Release();

	return  (Plan *) phj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLTvf
//
//	@doc:
//		Translates a DXL TVF node into a GPDB Function scan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLTvf
	(
	const CDXLNode *tvf_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// create function scan node
	FunctionScan *pfuncscan = MakeNode(FunctionScan);
	Plan *plan = &(pfuncscan->scan.plan);

	RangeTblEntry *rte = TranslateDXLTvfToRangeTblEntry(tvf_dxlnode, output_context, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);

	pfuncscan->funcexpr = rte->funcexpr;
	pfuncscan->funccolnames = rte->eref->colnames;

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	dxltrctxbt.SetRelIndex(index);
	pfuncscan->scan.scanrelid = index;

	m_dxl_to_plstmt_context->AddRTE(rte);

	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(tvf_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// a table scan node must have at least 1 child: projection list
	GPOS_ASSERT(1 <= tvf_dxlnode->Arity());

	CDXLNode *project_list_dxl = (*tvf_dxlnode)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = TranslateDXLProjList
						(
						project_list_dxl,
						&dxltrctxbt,
						NULL,
						output_context
						);

	plan->targetlist = target_list;

	ListCell *plcTe = NULL;

	ForEach (plcTe, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTe);
		OID oidType = gpdb::ExprType((Node*) target_entry->expr);
		GPOS_ASSERT(InvalidOid != oidType);

		INT typMod = gpdb::ExprTypeMod((Node*) target_entry->expr);
		Oid typCollation = gpdb::TypeCollation(oidType);

		pfuncscan->funccoltypes = gpdb::LAppendOid(pfuncscan->funccoltypes, oidType);
		pfuncscan->funccoltypmods = gpdb::LAppendInt(pfuncscan->funccoltypmods, typMod);
		// GDPB_91_MERGE_FIXME: collation
		pfuncscan->funccolcollations = gpdb::LAppendOid(pfuncscan->funccolcollations, typCollation);
	}

	SetParamIds(plan);

	return (Plan *) pfuncscan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLTvfToRangeTblEntry
//
//	@doc:
//		Create a range table entry from a CDXLPhysicalTVF node
//
//---------------------------------------------------------------------------
RangeTblEntry *
CTranslatorDXLToPlStmt::TranslateDXLTvfToRangeTblEntry
	(
	const CDXLNode *tvf_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslateContextBaseTable *base_table_context
	)
{
	CDXLPhysicalTVF *dxlop = CDXLPhysicalTVF::Cast(tvf_dxlnode->GetOperator());

	RangeTblEntry *rte = MakeNode(RangeTblEntry);
	rte->rtekind = RTE_FUNCTION;

	FuncExpr *pfuncexpr = MakeNode(FuncExpr);

	pfuncexpr->funcid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->OidObjectId();
	pfuncexpr->funcretset = true;
	// this is a function call, as opposed to a cast
	pfuncexpr->funcformat = COERCE_EXPLICIT_CALL;
	pfuncexpr->funcresulttype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->OidObjectId();

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get function alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxlop->Pstr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxl = (*tvf_dxlnode)[EdxltsIndexProjList];

	// get column names
	const ULONG ulCols = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		CDXLNode *pdxlnPrElem = (*project_list_dxl)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(pdxlnPrElem->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *pvalColName = gpdb::MakeStringValue(col_name_char_array);
		palias->colnames = gpdb::LAppend(palias->colnames, pvalColName);

		// save mapping col id -> index in translate context
		(void) base_table_context->InsertMapping(dxl_proj_elem->Id(), ul+1 /*attno*/);
	}

	// function arguments
	const ULONG ulChildren = tvf_dxlnode->Arity();
	for (ULONG ul = 1; ul < ulChildren; ++ul)
	{
		CDXLNode *pdxlnFuncArg = (*tvf_dxlnode)[ul];

		CMappingColIdVarPlStmt mapcidvarplstmt
									(
									m_memory_pool,
									base_table_context,
									NULL,
									output_context,
									m_dxl_to_plstmt_context
									);

		Expr *pexprFuncArg = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnFuncArg, &mapcidvarplstmt);
		pfuncexpr->args = gpdb::LAppend(pfuncexpr->args, pexprFuncArg);
	}

	// GDPB_91_MERGE_FIXME: collation
	pfuncexpr->inputcollid = gpdb::ExprCollation((Node *) pfuncexpr->args);
	pfuncexpr->funccollid = gpdb::TypeCollation(pfuncexpr->funcresulttype);

	rte->funcexpr = (Node *)pfuncexpr;
	rte->inFromCl = true;
	rte->eref = palias;
	// GDPB_91_MERGE_FIXME: collation
	// set rte->funccoltypemods & rte->funccolcollations?

	return rte;
}


// create a range table entry from a CDXLPhysicalValuesScan node
RangeTblEntry *
CTranslatorDXLToPlStmt::TranslateDXLValueScanToRangeTblEntry
	(
	const CDXLNode *value_scan_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslateContextBaseTable *base_table_context
	)
{
	CDXLPhysicalValuesScan *dxl_phy_values_scan_op = CDXLPhysicalValuesScan::Cast(value_scan_dxlnode->GetOperator());

	RangeTblEntry *rte = MakeNode(RangeTblEntry);

	rte->relid = InvalidOid;
	rte->subquery = NULL;
	rte->rtekind = RTE_VALUES;
	rte->inh = false;			/* never true for values RTEs */
	rte->inFromCl = true;
	rte->requiredPerms = 0;
	rte->checkAsUser = InvalidOid;

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get value alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_phy_values_scan_op->GetOpNameStr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxl = (*value_scan_dxlnode)[EdxltsIndexProjList];

	// get column names
	const ULONG ulCols = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		CDXLNode *pdxlnPrElem = (*project_list_dxl)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(pdxlnPrElem->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *pvalColName = gpdb::MakeStringValue(col_name_char_array);
		palias->colnames = gpdb::LAppend(palias->colnames, pvalColName);

		// save mapping col id -> index in translate context
		(void) base_table_context->InsertMapping(dxl_proj_elem->Id(), ul+1 /*attno*/);
	}

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, base_table_context, NULL, output_context, m_dxl_to_plstmt_context);
	const ULONG ulChildren = value_scan_dxlnode->Arity();
	List *values_lists = NIL;
	List *values_collations = NIL;

	for (ULONG ulValue = EdxlValIndexConstStart; ulValue < ulChildren; ulValue++)
	{
		CDXLNode *pdxlnValueList = (*value_scan_dxlnode)[ulValue];
		const ULONG ulCols = pdxlnValueList->Arity();
		List *value = NIL;
		for (ULONG ulCol = 0; ulCol < ulCols ; ulCol++)
		{
			Expr *pconst = m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnValueList)[ulCol], &mapcidvarplstmt);
			value = gpdb::LAppend(value, pconst);

		}
		values_lists = gpdb::LAppend(values_lists, value);

		// GPDB_91_MERGE_FIXME: collation
		if (NIL == values_collations)
		{
			// Set collation based on the first list of values
			for (ULONG ulCol = 0; ulCol < ulCols ; ulCol++)
			{
				values_collations = gpdb::LAppendOid(values_collations, gpdb::ExprCollation((Node *) value));
			}
		}
	}

	rte->values_lists = values_lists;
	rte->values_collations = values_collations;
	rte->eref = palias;

	return rte;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLNLJoin
//
//	@doc:
//		Translates a DXL nested loop join node into a NestLoop plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLNLJoin
	(
	const CDXLNode *nl_join_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(nl_join_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalNLJoin);
	GPOS_ASSERT(nl_join_dxlnode->Arity() == EdxlnljIndexSentinel);

	// create hash join node
	NestLoop *pnlj = MakeNode(NestLoop);

	Join *pj = &(pnlj->join);
	Plan *plan = &(pj->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalNLJoin *pdxlnlj = CDXLPhysicalNLJoin::PdxlConvert(nl_join_dxlnode->GetOperator());

	// set join type
	pj->jointype = GetGPDBJoinTypeFromDXLJoinType(pdxlnlj->GetJoinType());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(nl_join_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*nl_join_dxlnode)[EdxlnljIndexLeftChild];
	CDXLNode *pdxlnRight = (*nl_join_dxlnode)[EdxlnljIndexRightChild];

	CDXLNode *project_list_dxl = (*nl_join_dxlnode)[EdxlnljIndexProjList];
	CDXLNode *filter_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*nl_join_dxlnode)[EdxlnljIndexJoinFilter];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	// setting of prefetch_inner to true except for the case of index NLJ where we cannot prefetch inner
	// because inner child depends on variables coming from outer child
	pj->prefetch_inner = !pdxlnlj->IsIndexNLJ();

	DXLTranslationContextArr *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	Plan *pplanLeft = NULL;
	Plan *pplanRight = NULL;
	if (pdxlnlj->IsIndexNLJ())
	{
		// right child (the index scan side) has references to left child's columns,
		// we need to translate left child first to load its columns into translation context
		pplanLeft = TranslateDXLOperatorToPlan(pdxlnLeft, &dxltrctxLeft, ctxt_translation_prev_siblings);

		pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
		 pdrgpdxltrctxWithSiblings->AppendArray(ctxt_translation_prev_siblings);

		 // translate right child after left child translation is complete
		pplanRight = TranslateDXLOperatorToPlan(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);
	}
	else
	{
		// left child may include a PartitionSelector with references to right child's columns,
		// we need to translate right child first to load its columns into translation context
		pplanRight = TranslateDXLOperatorToPlan(pdxlnRight, &dxltrctxRight, ctxt_translation_prev_siblings);

		pdrgpdxltrctxWithSiblings->Append(&dxltrctxRight);
		pdrgpdxltrctxWithSiblings->AppendArray(ctxt_translation_prev_siblings);

		// translate left child after right child translation is complete
		pplanLeft = TranslateDXLOperatorToPlan(pdxlnLeft, &dxltrctxLeft, pdrgpdxltrctxWithSiblings);
	}
	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxLeft);
	child_contexts->Append(&dxltrctxRight);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join condition
	pj->joinqual = TranslateDXLFilterToQual
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	plan->lefttree = pplanLeft;
	plan->righttree = pplanRight;
	plan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	child_contexts->Release();

	return  (Plan *) pnlj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLMergeJoin
//
//	@doc:
//		Translates a DXL merge join node into a MergeJoin node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLMergeJoin
	(
	const CDXLNode *merge_join_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(merge_join_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalMergeJoin);
	GPOS_ASSERT(merge_join_dxlnode->Arity() == EdxlmjIndexSentinel);

	// create merge join node
	MergeJoin *pmj = MakeNode(MergeJoin);

	Join *pj = &(pmj->join);
	Plan *plan = &(pj->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMergeJoin *pdxlopMergeJoin = CDXLPhysicalMergeJoin::Cast(merge_join_dxlnode->GetOperator());

	// set join type
	pj->jointype = GetGPDBJoinTypeFromDXLJoinType(pdxlopMergeJoin->GetJoinType());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(merge_join_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*merge_join_dxlnode)[EdxlmjIndexLeftChild];
	CDXLNode *pdxlnRight = (*merge_join_dxlnode)[EdxlmjIndexRightChild];

	CDXLNode *project_list_dxl = (*merge_join_dxlnode)[EdxlmjIndexProjList];
	CDXLNode *filter_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*merge_join_dxlnode)[EdxlmjIndexJoinFilter];
	CDXLNode *pdxlnMergeCondList = (*merge_join_dxlnode)[EdxlmjIndexMergeCondList];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanLeft = TranslateDXLOperatorToPlan(pdxlnLeft, &dxltrctxLeft, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
	pdrgpdxltrctxWithSiblings->AppendArray(ctxt_translation_prev_siblings);

	Plan *pplanRight = TranslateDXLOperatorToPlan(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxLeft));
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxRight));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join filter
	pj->joinqual = TranslateDXLFilterToQual
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	// translate merge cond
	List *plMergeConditions = NIL;

	const ULONG arity = pdxlnMergeCondList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnMergeCond = (*pdxlnMergeCondList)[ul];
		List *plMergeCond = TranslateDXLScCondToQual
				(
				pdxlnMergeCond,
				NULL,			// base table translation context
				child_contexts,
				output_context
				);

		GPOS_ASSERT(1 == gpdb::ListLength(plMergeCond));
		plMergeConditions = gpdb::ListConcat(plMergeConditions, plMergeCond);
	}

	GPOS_ASSERT(NIL != plMergeConditions);

	pmj->mergeclauses = plMergeConditions;

	plan->lefttree = pplanLeft;
	plan->righttree = pplanRight;
	plan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(plan);

	// GDPB_91_MERGE_FIXME: collation
	// Need to set pmj->mergeCollations, but ORCA does not produce plans with
	// Merge Joins.

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	child_contexts->Release();

	return  (Plan *) pmj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLHash
//
//	@doc:
//		Translates a DXL physical operator node into a Hash node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLHash
	(
	const CDXLNode *dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	Hash *ph = MakeNode(Hash);

	Plan *plan = &(ph->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate dxl node
	CDXLTranslateContext dxltrctx(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanLeft = TranslateDXLOperatorToPlan(dxlnode, &dxltrctx, ctxt_translation_prev_siblings);

	GPOS_ASSERT(0 < dxlnode->Arity());

	// create a reference to each entry in the child project list to create the target list of
	// the hash node
	CDXLNode *project_list_dxl = (*dxlnode)[0];
	List *target_list = TranslateDXLProjectListToHashTargetList(project_list_dxl, &dxltrctx, output_context);

	// copy costs from child node; the startup cost for the hash node is the total cost
	// of the child plan, see make_hash in createplan.c
	plan->startup_cost = pplanLeft->total_cost;
	plan->total_cost = pplanLeft->total_cost;
	plan->plan_rows = pplanLeft->plan_rows;
	plan->plan_width = pplanLeft->plan_width;

	plan->targetlist = target_list;
	plan->lefttree = pplanLeft;
	plan->righttree = NULL;
	plan->nMotionNodes = pplanLeft->nMotionNodes;
	plan->qual = NIL;
	ph->rescannable = false;

	SetParamIds(plan);

	return (Plan *) ph;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanTranslateDXLMotion
//
//	@doc:
//		Translate DXL motion node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanTranslateDXLMotion
	(
	const CDXLNode *motion_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());
	if (CTranslatorUtils::IsDuplicateSensitiveMotion(pdxlopMotion))
	{
		return TranslateDXLRedistributeMotionToResultHashFilters(motion_dxlnode, output_context, ctxt_translation_prev_siblings);
	}
	
	return TranslateDXLMotion(motion_dxlnode, output_context, ctxt_translation_prev_siblings);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLMotion
//
//	@doc:
//		Translate DXL motion node into GPDB Motion plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLMotion
	(
	const CDXLNode *motion_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());

	// create motion node
	Motion *pmotion = MakeNode(Motion);

	Plan *plan = &(pmotion->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(motion_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	CDXLNode *project_list_dxl = (*motion_dxlnode)[EdxlgmIndexProjList];
	CDXLNode *filter_dxlnode = (*motion_dxlnode)[EdxlgmIndexFilter];
	CDXLNode *sort_col_list_dxl = (*motion_dxlnode)[EdxlgmIndexSortColList];

	// translate motion child
	// child node is in the same position in broadcast and gather motion nodes
	// but different in redistribute motion nodes

	ULONG ulChildIndex = pdxlopMotion->GetRelationChildIdx();

	CDXLNode *dxl_node_child = (*motion_dxlnode)[ulChildIndex];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate sorting info
	ULONG ulNumSortCols = sort_col_list_dxl->Arity();
	if (0 < ulNumSortCols)
	{
		pmotion->sendSorted = true;
		pmotion->numSortCols = ulNumSortCols;
		pmotion->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(ulNumSortCols * sizeof(AttrNumber));
		pmotion->sortOperators = (Oid *) gpdb::GPDBAlloc(ulNumSortCols * sizeof(Oid));
		pmotion->collations = (Oid *) gpdb::GPDBAlloc(ulNumSortCols * sizeof(Oid));
		pmotion->nullsFirst = (bool *) gpdb::GPDBAlloc(ulNumSortCols * sizeof(bool));

		TranslateSortCols(sort_col_list_dxl, output_context, pmotion->sortColIdx, pmotion->sortOperators, pmotion->collations, pmotion->nullsFirst);
	}
	else
	{
		// not a sorting motion
		pmotion->sendSorted = false;
		pmotion->numSortCols = 0;
		pmotion->sortColIdx = NULL;
		pmotion->sortOperators = NULL;
		pmotion->nullsFirst = NULL;
	}

	if (pdxlopMotion->GetDXLOperator() == EdxlopPhysicalMotionRedistribute ||
		pdxlopMotion->GetDXLOperator() == EdxlopPhysicalMotionRoutedDistribute ||
		pdxlopMotion->GetDXLOperator() == EdxlopPhysicalMotionRandom)
	{
		// translate hash expr list
		List *plHashExpr = NIL;
		List *plHashExprTypes = NIL;

		if (EdxlopPhysicalMotionRedistribute == pdxlopMotion->GetDXLOperator())
		{
			CDXLNode *hash_expr_list_dxlnode = (*motion_dxlnode)[EdxlrmIndexHashExprList];

			TranslateHashExprList
				(
				hash_expr_list_dxlnode,
				&dxltrctxChild,
				&plHashExpr,
				&plHashExprTypes,
				output_context
				);
		}
		GPOS_ASSERT(gpdb::ListLength(plHashExpr) == gpdb::ListLength(plHashExprTypes));

		pmotion->hashExpr = plHashExpr;
		pmotion->hashDataTypes = plHashExprTypes;
	}

	// cleanup
	child_contexts->Release();

	// create flow for child node to distinguish between singleton flows and all-segment flows
	Flow *pflow = MakeNode(Flow);

	const IntPtrArray *pdrgpiInputSegmentIds = pdxlopMotion->GetInputSegIdsArray();


	// only one sender
	if (1 == pdrgpiInputSegmentIds->Size())
	{
		pflow->segindex = *((*pdrgpiInputSegmentIds)[0]);

		// only one segment in total
		if (1 == gpdb::GetGPSegmentCount())
		{
			if (pflow->segindex == MASTER_CONTENT_ID)
				// sender is on master, must be singleton flow
				pflow->flotype = FLOW_SINGLETON;
			else
				// sender is on segment, can not tell it's singleton or
				// all-segment flow, just treat it as all-segment flow so
				// it can be promoted to writer gang later if needed.
				pflow->flotype = FLOW_UNDEFINED;
		}
		else
		{
			// multiple segments, must be singleton flow
			pflow->flotype = FLOW_SINGLETON;
		}
	}
	else
	{
		pflow->flotype = FLOW_UNDEFINED;
	}

	pplanChild->flow = pflow;

	pmotion->motionID = m_dxl_to_plstmt_context->GetNextMotionId();
	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes + 1;

	// translate properties of the specific type of motion operator

	switch (pdxlopMotion->GetDXLOperator())
	{
		case EdxlopPhysicalMotionGather:
		{
			pmotion->motionType = MOTIONTYPE_FIXED;
			// get segment id
			INT iSegId = CDXLPhysicalGatherMotion::Cast(pdxlopMotion)->IOutputSegIdx();
			pmotion->numOutputSegs = 1;
			pmotion->outputSegIdx = (INT *) gpdb::GPDBAlloc(sizeof(INT));
			*(pmotion->outputSegIdx) = iSegId;
			break;
		}
		case EdxlopPhysicalMotionRedistribute:
		case EdxlopPhysicalMotionRandom:
		{
			pmotion->motionType = MOTIONTYPE_HASH;
			// translate output segment ids
			const IntPtrArray *pdrgpiSegIds = CDXLPhysicalMotion::Cast(pdxlopMotion)->GetOutputSegIdsArray();

			GPOS_ASSERT(NULL != pdrgpiSegIds && 0 < pdrgpiSegIds->Size());
			ULONG ulSegIdCount = pdrgpiSegIds->Size();
			pmotion->outputSegIdx = (INT *) gpdb::GPDBAlloc (ulSegIdCount * sizeof(INT));
			pmotion->numOutputSegs = ulSegIdCount;

			for(ULONG ul = 0; ul < ulSegIdCount; ul++)
			{
				INT iSegId = *((*pdrgpiSegIds)[ul]);
				pmotion->outputSegIdx[ul] = iSegId;
			}

			break;
		}
		case EdxlopPhysicalMotionBroadcast:
		{
			pmotion->motionType = MOTIONTYPE_FIXED;
			pmotion->numOutputSegs = 0;
			pmotion->outputSegIdx = NULL;
			break;
		}
		case EdxlopPhysicalMotionRoutedDistribute:
		{
			pmotion->motionType = MOTIONTYPE_EXPLICIT;
			pmotion->numOutputSegs = 0;
			pmotion->outputSegIdx = NULL;
			ULONG ulSegIdCol = CDXLPhysicalRoutedDistributeMotion::Cast(pdxlopMotion)->SegmentIdCol();
			const TargetEntry *pteSortCol = dxltrctxChild.GetTargetEntry(ulSegIdCol);
			pmotion->segidColIdx = pteSortCol->resno;

			break;
			
		}
		default:
			GPOS_ASSERT(!"Unrecognized Motion operator");
			return NULL;
	}

	SetParamIds(plan);

	return (Plan *) pmotion;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLRedistributeMotionToResultHashFilters
//
//	@doc:
//		Translate DXL duplicate sensitive redistribute motion node into 
//		GPDB result node with hash filters
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLRedistributeMotionToResultHashFilters
	(
	const CDXLNode *motion_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create motion node
	Result *presult = MakeNode(Result);

	Plan *plan = &(presult->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(motion_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	CDXLNode *project_list_dxl = (*motion_dxlnode)[EdxlrmIndexProjList];
	CDXLNode *filter_dxlnode = (*motion_dxlnode)[EdxlrmIndexFilter];
	CDXLNode *dxl_node_child = (*motion_dxlnode)[pdxlopMotion->GetRelationChildIdx()];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate hash expr list
	presult->hashFilter = true;

	if (EdxlopPhysicalMotionRedistribute == pdxlopMotion->GetDXLOperator())
	{
		CDXLNode *hash_expr_list_dxlnode = (*motion_dxlnode)[EdxlrmIndexHashExprList];
		const ULONG length = hash_expr_list_dxlnode->Arity();
		GPOS_ASSERT(0 < length);
		
		for (ULONG ul = 0; ul < length; ul++)
		{
			CDXLNode *pdxlnHashExpr = (*hash_expr_list_dxlnode)[ul];
			CDXLNode *pdxlnExpr = (*pdxlnHashExpr)[0];
			
			INT iResno = gpos::int_max;
			if (EdxlopScalarIdent == pdxlnExpr->GetOperator()->GetDXLOperator())
			{
				ULONG col_id = CDXLScalarIdent::Cast(pdxlnExpr->GetOperator())->MakeDXLColRef()->Id();
				iResno = output_context->GetTargetEntry(col_id)->resno;
			}
			else
			{
				// The expression is not a scalar ident that points to an output column in the child node.
				// Rather, it is an expresssion that is evaluated by the hash filter such as CAST(a) or a+b.
				// We therefore, create a corresponding GPDB scalar expression and add it to the project list
				// of the hash filter
				CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
															(
															m_memory_pool,
															NULL, // translate context for the base table
															child_contexts,
															output_context,
															m_dxl_to_plstmt_context
															);
				
				Expr *expr = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnExpr, &mapcidvarplstmt);
				GPOS_ASSERT(NULL != expr);

				// create a target entry for the hash filter
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
				TargetEntry *target_entry = gpdb::MakeTargetEntry
											(
											expr, 
											gpdb::ListLength(plan->targetlist) + 1, 
											CTranslatorUtils::CreateMultiByteCharStringFromWCString(strUnnamedCol.GetBuffer()),
											false /* resjunk */
											);
				plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);

				iResno = target_entry->resno;
			}
			GPOS_ASSERT(gpos::int_max != iResno);
			
			presult->hashList = gpdb::LAppendInt(presult->hashList, iResno);
		}
	}
	
	// cleanup
	child_contexts->Release();

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	SetParamIds(plan);

	return (Plan *) presult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLAgg
//
//	@doc:
//		Translate DXL aggregate node into GPDB Agg plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLAgg
	(
	const CDXLNode *pdxlnAgg,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create aggregate plan node
	Agg *pagg = MakeNode(Agg);

	Plan *plan = &(pagg->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalAgg *pdxlopAgg = CDXLPhysicalAgg::Cast(pdxlnAgg->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnAgg->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate agg child
	CDXLNode *dxl_node_child = (*pdxlnAgg)[EdxlaggIndexChild];

	CDXLNode *project_list_dxl = (*pdxlnAgg)[EdxlaggIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnAgg)[EdxlaggIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, true, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,			// pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	// translate aggregation strategy
	switch (pdxlopAgg->GetAggStrategy())
	{
		case EdxlaggstrategyPlain:
			pagg->aggstrategy = AGG_PLAIN;
			break;
		case EdxlaggstrategySorted:
			pagg->aggstrategy = AGG_SORTED;
			break;
		case EdxlaggstrategyHashed:
			pagg->aggstrategy = AGG_HASHED;
			break;
		default:
			GPOS_ASSERT(!"Invalid aggregation strategy");
	}

	pagg->streaming = pdxlopAgg->IsStreamSafe();

	// translate grouping cols
	const ULongPtrArray *pdrpulGroupingCols = pdxlopAgg->GetGroupingColidArray();
	pagg->numCols = pdrpulGroupingCols->Size();
	if (pagg->numCols > 0)
	{
		pagg->grpColIdx = (AttrNumber *) gpdb::GPDBAlloc(pagg->numCols * sizeof(AttrNumber));
		pagg->grpOperators = (Oid *) gpdb::GPDBAlloc(pagg->numCols * sizeof(Oid));
	}
	else
	{
		pagg->grpColIdx = NULL;
		pagg->grpOperators = NULL;
	}

	const ULONG ulLen = pdrpulGroupingCols->Size();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		ULONG ulGroupingColId = *((*pdrpulGroupingCols)[ul]);
		const TargetEntry *pteGroupingCol = dxltrctxChild.GetTargetEntry(ulGroupingColId);
		if (NULL  == pteGroupingCol)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, ulGroupingColId);
		}
		pagg->grpColIdx[ul] = pteGroupingCol->resno;

		// Also find the equality operators to use for each grouping col.
		Oid typeId = gpdb::ExprType((Node *) pteGroupingCol->expr);
		pagg->grpOperators[ul] = gpdb::GetEqualityOp(typeId);
		Assert(pagg->grpOperators[ul] != 0);
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) pagg;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLWindow
//
//	@doc:
//		Translate DXL window node into GPDB window plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLWindow
	(
	const CDXLNode *pdxlnWindow,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create a WindowAgg plan node
	WindowAgg *pwindow = MakeNode(WindowAgg);

	Plan *plan = &(pwindow->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalWindow *pdxlopWindow = CDXLPhysicalWindow::Cast(pdxlnWindow->GetOperator());

	// translate the operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnWindow->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate children
	CDXLNode *dxl_node_child = (*pdxlnWindow)[EdxlwindowIndexChild];
	CDXLNode *project_list_dxl = (*pdxlnWindow)[EdxlwindowIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnWindow)[EdxlwindowIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, true, output_context->GetColIdToParamIdMap());
	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,			// pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	ListCell *lc;

	foreach (lc, plan->targetlist)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		if (IsA(target_entry->expr, WindowFunc))
		{
			WindowFunc *pwinfunc = (WindowFunc *) target_entry->expr;
			pwindow->winref = pwinfunc->winref;
			break;
		}
	}

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	// translate partition columns
	const ULongPtrArray *pdrpulPartCols = pdxlopWindow->GetPartByColsArray();
	pwindow->partNumCols = pdrpulPartCols->Size();
	pwindow->partColIdx = NULL;
	pwindow->partOperators = NULL;

	if (pwindow->partNumCols > 0)
	{
		pwindow->partColIdx = (AttrNumber *) gpdb::GPDBAlloc(pwindow->partNumCols * sizeof(AttrNumber));
		pwindow->partOperators = (Oid *) gpdb::GPDBAlloc(pwindow->partNumCols * sizeof(Oid));
	}

	const ULONG ulPartCols = pdrpulPartCols->Size();
	for (ULONG ul = 0; ul < ulPartCols; ul++)
	{
		ULONG ulPartColId = *((*pdrpulPartCols)[ul]);
		const TargetEntry *ptePartCol = dxltrctxChild.GetTargetEntry(ulPartColId);
		if (NULL  == ptePartCol)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, ulPartColId);
		}
		pwindow->partColIdx[ul] = ptePartCol->resno;

		// Also find the equality operators to use for each partitioning key col.
		Oid typeId = gpdb::ExprType((Node *) ptePartCol->expr);
		pwindow->partOperators[ul] = gpdb::GetEqualityOp(typeId);
		Assert(pwindow->partOperators[ul] != 0);
	}

	// translate window keys
	const ULONG size = pdxlopWindow->WindowKeysCount();
	if (size > 1)
	  {
	    GpdbEreport(ERRCODE_INTERNAL_ERROR,
			ERROR,
			"ORCA produced a plan with more than one window key",
			NULL);
	  }
	GPOS_ASSERT(size <= 1 && "cannot have more than one window key");

	if (size == 1)
	{
		// translate the sorting columns used in the window key
		const CDXLWindowKey *pdxlwindowkey = pdxlopWindow->GetDXLWindowKeyAt(0);
		const CDXLWindowFrame *window_frame = pdxlwindowkey->GetWindowFrame();
		const CDXLNode *sort_col_list_dxlnode = pdxlwindowkey->GetSortColListDXL();

		const ULONG ulNumCols = sort_col_list_dxlnode->Arity();

		pwindow->ordNumCols = ulNumCols;
		pwindow->ordColIdx = (AttrNumber *) gpdb::GPDBAlloc(ulNumCols * sizeof(AttrNumber));
		pwindow->ordOperators = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
		bool *pNullsFirst = (bool *) gpdb::GPDBAlloc(ulNumCols * sizeof(bool));
		TranslateSortCols(sort_col_list_dxlnode, &dxltrctxChild, pwindow->ordColIdx, pwindow->ordOperators, NULL, pNullsFirst);

		// The firstOrder* fields are separate from just picking the first of ordCol*,
		// because the Postgres planner might omit columns that are redundant with the
		// PARTITION BY from ordCol*. But ORCA doesn't do that, so we can just copy
		// the first entry of ordColIdx/ordOperators into firstOrder* fields.
		if (ulNumCols > 0)
		{
			pwindow->firstOrderCol = pwindow->ordColIdx[0];
			pwindow->firstOrderCmpOperator = pwindow->ordOperators[0];
			pwindow->firstOrderNullsFirst = pNullsFirst[0];
		}
		gpdb::GPDBFree(pNullsFirst);

		// The ordOperators array is actually supposed to contain equality operators,
		// not ordering operators (< or >). So look up the corresponding equality
		// operator for each ordering operator.
		for (ULONG i = 0; i < ulNumCols; i++)
		{
			pwindow->ordOperators[i] = gpdb::GetEqualityOpForOrderingOp(pwindow->ordOperators[i], NULL);
		}

		// translate the window frame specified in the window key
		if (NULL != pdxlwindowkey->GetWindowFrame())
		{
			pwindow->frameOptions = FRAMEOPTION_NONDEFAULT;
			if (EdxlfsRow == window_frame->ParseDXLFrameSpec())
			{
				pwindow->frameOptions |= FRAMEOPTION_ROWS;
			}
			else
			{
				pwindow->frameOptions |= FRAMEOPTION_RANGE;
			}

			if (window_frame->ParseFrameExclusionStrategy() != EdxlfesNulls)
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiQuery2DXLUnsupportedFeature,
					   GPOS_WSZ_LIT("EXCLUDE clause in window frame"));
			}

			// translate the CDXLNodes representing the leading and trailing edge
			DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
			child_contexts->Append(&dxltrctxChild);

			CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
			(
			 m_memory_pool,
			 NULL,
			 child_contexts,
			 output_context,
			 m_dxl_to_plstmt_context
			);

			// Translate lead boundary
			//
			// Note that we don't distinguish between the delayed and undelayed
			// versions beoynd this point. Executor will make that decision
			// without our help.
			//
			CDXLNode *pdxlnLead = window_frame->PdxlnLeading();
			EdxlFrameBoundary leadBoundary = CDXLScalarWindowFrameEdge::Cast(pdxlnLead->GetOperator())->ParseDXLFrameBoundary();
			if (leadBoundary == EdxlfbUnboundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_END_UNBOUNDED_PRECEDING;
			if (leadBoundary == EdxlfbBoundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_END_VALUE_PRECEDING;
			if (leadBoundary == EdxlfbCurrentRow)
				pwindow->frameOptions |= FRAMEOPTION_END_CURRENT_ROW;
			if (leadBoundary == EdxlfbBoundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_END_VALUE_FOLLOWING;
			if (leadBoundary == EdxlfbUnboundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_END_UNBOUNDED_FOLLOWING;
			if (leadBoundary == EdxlfbDelayedBoundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_END_VALUE_PRECEDING;
			if (leadBoundary == EdxlfbDelayedBoundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_END_VALUE_FOLLOWING;
			if (0 != pdxlnLead->Arity())
			{
				pwindow->endOffset = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnLead)[0], &mapcidvarplstmt);
			}

			// And the same for the trail boundary
			CDXLNode *pdxlnTrail = window_frame->PdxlnTrailing();
			EdxlFrameBoundary trailBoundary = CDXLScalarWindowFrameEdge::Cast(pdxlnTrail->GetOperator())->ParseDXLFrameBoundary();
			if (trailBoundary == EdxlfbUnboundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_START_UNBOUNDED_PRECEDING;
			if (trailBoundary == EdxlfbBoundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_START_VALUE_PRECEDING;
			if (trailBoundary == EdxlfbCurrentRow)
				pwindow->frameOptions |= FRAMEOPTION_START_CURRENT_ROW;
			if (trailBoundary == EdxlfbBoundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_START_VALUE_FOLLOWING;
			if (trailBoundary == EdxlfbUnboundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_START_UNBOUNDED_FOLLOWING;
			if (trailBoundary == EdxlfbDelayedBoundedPreceding)
				pwindow->frameOptions |= FRAMEOPTION_START_VALUE_PRECEDING;
			if (trailBoundary == EdxlfbDelayedBoundedFollowing)
				pwindow->frameOptions |= FRAMEOPTION_START_VALUE_FOLLOWING;
			if (0 != pdxlnTrail->Arity())
			{
				pwindow->startOffset = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL((*pdxlnTrail)[0], &mapcidvarplstmt);
			}

			// cleanup
			child_contexts->Release();
		}
		else
			pwindow->frameOptions = FRAMEOPTION_DEFAULTS;
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) pwindow;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLSort
//
//	@doc:
//		Translate DXL sort node into GPDB Sort plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLSort
	(
	const CDXLNode *sort_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create sort plan node
	Sort *psort = MakeNode(Sort);

	Plan *plan = &(psort->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalSort *pdxlopSort = CDXLPhysicalSort::Cast(sort_dxlnode->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(sort_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate sort child
	CDXLNode *dxl_node_child = (*sort_dxlnode)[EdxlsortIndexChild];
	CDXLNode *project_list_dxl = (*sort_dxlnode)[EdxlsortIndexProjList];
	CDXLNode *filter_dxlnode = (*sort_dxlnode)[EdxlsortIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	// set sorting info
	psort->noduplicates = pdxlopSort->FDiscardDuplicates();

	// translate sorting columns

	const CDXLNode *sort_col_list_dxl = (*sort_dxlnode)[EdxlsortIndexSortColList];

	const ULONG ulNumCols = sort_col_list_dxl->Arity();
	psort->numCols = ulNumCols;
	psort->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(ulNumCols * sizeof(AttrNumber));
	psort->sortOperators = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
	psort->collations = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
	psort->nullsFirst = (bool *) gpdb::GPDBAlloc(ulNumCols * sizeof(bool));

	TranslateSortCols(sort_col_list_dxl, &dxltrctxChild, psort->sortColIdx, psort->sortOperators, psort->collations, psort->nullsFirst);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) psort;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLSubQueryScan
//
//	@doc:
//		Translate DXL subquery scan node into GPDB SubqueryScan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLSubQueryScan
	(
	const CDXLNode *subquery_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create sort plan node
	SubqueryScan *psubqscan = MakeNode(SubqueryScan);

	Plan *plan = &(psubqscan->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalSubqueryScan *pdxlopSubqscan = CDXLPhysicalSubqueryScan::Cast(subquery_scan_dxlnode->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(subquery_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate subplan
	CDXLNode *dxl_node_child = (*subquery_scan_dxlnode)[EdxlsubqscanIndexChild];
	CDXLNode *project_list_dxl = (*subquery_scan_dxlnode)[EdxlsubqscanIndexProjList];
	CDXLNode *filter_dxlnode = (*subquery_scan_dxlnode)[EdxlsubqscanIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	// create an rtable entry for the subquery scan
	RangeTblEntry *rte = MakeNode(RangeTblEntry);
	rte->rtekind = RTE_SUBQUERY;

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get table alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopSubqscan->MdName()->GetMDName()->GetBuffer());

	// get column names from child project list
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	(psubqscan->scan).scanrelid = index;
	dxltrctxbt.SetRelIndex(index);

	ListCell *plcTE = NULL;

	CDXLNode *pdxlnChildProjList = (*dxl_node_child)[0];

	ULONG ul = 0;

	ForEach (plcTE, pplanChild->targetlist)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);

		// non-system attribute
		CHAR *col_name_char_array = PStrDup(target_entry->resname);
		Value *pvalColName = gpdb::MakeStringValue(col_name_char_array);
		palias->colnames = gpdb::LAppend(palias->colnames, pvalColName);

		// get corresponding child project element
		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast((*pdxlnChildProjList)[ul]->GetOperator());

		// save mapping col id -> index in translate context
		(void) dxltrctxbt.InsertMapping(pdxlopPrel->Id(), target_entry->resno);
		ul++;
	}

	rte->eref = palias;

	// add range table entry for the subquery to the list
	m_dxl_to_plstmt_context->AddRTE(rte);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,		// translate context for the base table
		NULL,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	psubqscan->subplan = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	SetParamIds(plan);
	return (Plan *) psubqscan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLResult
//
//	@doc:
//		Translate DXL result node into GPDB result plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLResult
	(
	const CDXLNode *result_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create result plan node
	Result *presult = MakeNode(Result);

	Plan *plan = &(presult->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(result_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	plan->nMotionNodes = 0;

	CDXLNode *dxl_node_child = NULL;
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	if (result_dxlnode->Arity() - 1 == EdxlresultIndexChild)
	{
		// translate child plan
		dxl_node_child = (*result_dxlnode)[EdxlresultIndexChild];

		Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		presult->plan.lefttree = pplanChild;

		plan->nMotionNodes = pplanChild->nMotionNodes;
	}

	CDXLNode *project_list_dxl = (*result_dxlnode)[EdxlresultIndexProjList];
	CDXLNode *filter_dxlnode = (*result_dxlnode)[EdxlresultIndexFilter];
	CDXLNode *pdxlnOneTimeFilter = (*result_dxlnode)[EdxlresultIndexOneTimeFilter];

	List *plQuals = NULL;

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,		// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plQuals,
		output_context
		);

	// translate one time filter
	List *plOneTimeQuals = TranslateDXLFilterToQual
							(
							pdxlnOneTimeFilter,
							NULL,			// base table translation context
							child_contexts,
							output_context
							);

	plan->qual = plQuals;

	presult->resconstantqual = (Node *) plOneTimeQuals;

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) presult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLPartSelector
//
//	@doc:
//		Translate DXL PartitionSelector into a GPDB PartitionSelector node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLPartSelector
	(
	const CDXLNode *partition_selector_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	PartitionSelector *ppartsel = MakeNode(PartitionSelector);

	Plan *plan = &(ppartsel->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalPartitionSelector *pdxlopPartSel = CDXLPhysicalPartitionSelector::Cast(partition_selector_dxlnode->GetOperator());
	const ULONG ulLevels = pdxlopPartSel->GetPartitioningLevel();
	ppartsel->nLevels = ulLevels;
	ppartsel->scanId = pdxlopPartSel->ScanId();
	ppartsel->relid = CMDIdGPDB::CastMdid(pdxlopPartSel->GetRelMdId())->OidObjectId();
	ppartsel->selectorId = m_partition_selector_counter++;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(partition_selector_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	plan->nMotionNodes = 0;

	CDXLNode *dxl_node_child = NULL;
	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	BOOL fHasChild = (EdxlpsIndexChild == partition_selector_dxlnode->Arity() - 1);
	if (fHasChild)
	{
		// translate child plan
		dxl_node_child = (*partition_selector_dxlnode)[EdxlpsIndexChild];

		Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);
		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		ppartsel->plan.lefttree = pplanChild;
		plan->nMotionNodes = pplanChild->nMotionNodes;
	}

	child_contexts->Append(&dxltrctxChild);

	CDXLNode *project_list_dxl = (*partition_selector_dxlnode)[EdxlpsIndexProjList];
	CDXLNode *pdxlnEqFilters = (*partition_selector_dxlnode)[EdxlpsIndexEqFilters];
	CDXLNode *pdxlnFilters = (*partition_selector_dxlnode)[EdxlpsIndexFilters];
	CDXLNode *pdxlnResidualFilter = (*partition_selector_dxlnode)[EdxlpsIndexResidualFilter];
	CDXLNode *pdxlnPropExpr = (*partition_selector_dxlnode)[EdxlpsIndexPropExpr];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxl, NULL /*base_table_context*/, child_contexts, output_context);

	// translate filter lists
	GPOS_ASSERT(pdxlnEqFilters->Arity() == ulLevels);
	ppartsel->levelEqExpressions = TranslateDXLFilterList(pdxlnEqFilters, NULL /*base_table_context*/, child_contexts, output_context);

	GPOS_ASSERT(pdxlnFilters->Arity() == ulLevels);
	ppartsel->levelExpressions = TranslateDXLFilterList(pdxlnFilters, NULL /*base_table_context*/, child_contexts, output_context);

	//translate residual filter
	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, NULL /*base_table_context*/, child_contexts, output_context, m_dxl_to_plstmt_context);
	if (!m_translator_dxl_to_scalar->HasConstTrue(pdxlnResidualFilter, m_md_accessor))
	{
		ppartsel->residualPredicate = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnResidualFilter, &mapcidvarplstmt);
	}

	//translate propagation expression
	if (!m_translator_dxl_to_scalar->HasConstNull(pdxlnPropExpr))
	{
		ppartsel->propagationExpression = (Node *) m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnPropExpr, &mapcidvarplstmt);
	}

	// no need to translate printable filter - since it is not needed by the executor

	ppartsel->staticPartOids = NIL;
	ppartsel->staticScanIds = NIL;
	ppartsel->staticSelection = !fHasChild;

	if (ppartsel->staticSelection)
	{
		SelectedParts *sp = gpdb::RunStaticPartitionSelection(ppartsel);
		ppartsel->staticPartOids = sp->partOids;
		ppartsel->staticScanIds = sp->scanIds;
		gpdb::GPDBFree(sp);
	}
	else
	{
		// if we cannot do static elimination then add this partitioned table oid
		// to the planned stmt so we can ship the constraints with the plan
		m_dxl_to_plstmt_context->AddPartitionedTable(ppartsel->relid);
	}

	// increment the number of partition selectors for the given scan id
	m_dxl_to_plstmt_context->IncrementPartitionSelectors(ppartsel->scanId);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) ppartsel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLFilterList
//
//	@doc:
//		Translate DXL filter list into GPDB filter list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLFilterList
	(
	const CDXLNode *filter_list_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	GPOS_ASSERT(EdxlopScalarOpList == filter_list_dxlnode->GetOperator()->GetDXLOperator());

	List *plFilters = NIL;

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, base_table_context, child_contexts, output_context, m_dxl_to_plstmt_context);
	const ULONG arity = filter_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnChildFilter = (*filter_list_dxlnode)[ul];

		if (m_translator_dxl_to_scalar->HasConstTrue(pdxlnChildFilter, m_md_accessor))
		{
			plFilters = gpdb::LAppend(plFilters, NULL /*datum*/);
			continue;
		}

		Expr *pexprFilter = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnChildFilter, &mapcidvarplstmt);
		plFilters = gpdb::LAppend(plFilters, pexprFilter);
	}

	return plFilters;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLAppend
//
//	@doc:
//		Translate DXL append node into GPDB Append plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLAppend
	(
	const CDXLNode *append_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create append plan node
	Append *pappend = MakeNode(Append);

	Plan *plan = &(pappend->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(append_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	const ULONG arity = append_dxlnode->Arity();
	GPOS_ASSERT(EdxlappendIndexFirstChild < arity);
	plan->nMotionNodes = 0;
	pappend->appendplans = NIL;
	
	// translate children
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());
	for (ULONG ul = EdxlappendIndexFirstChild; ul < arity; ul++)
	{
		CDXLNode *dxl_node_child = (*append_dxlnode)[ul];

		Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		pappend->appendplans = gpdb::LAppend(pappend->appendplans, pplanChild);
		plan->nMotionNodes += pplanChild->nMotionNodes;
	}

	CDXLNode *project_list_dxl = (*append_dxlnode)[EdxlappendIndexProjList];
	CDXLNode *filter_dxlnode = (*append_dxlnode)[EdxlappendIndexFilter];

	plan->targetlist = NIL;
	const ULONG ulLen = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < ulLen; ++ul)
	{
		CDXLNode *pdxlnPrEl = (*project_list_dxl)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == pdxlnPrEl->GetOperator()->GetDXLOperator());

		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());
		GPOS_ASSERT(1 == pdxlnPrEl->Arity());

		// translate proj element expression
		CDXLNode *pdxlnExpr = (*pdxlnPrEl)[0];
		CDXLScalarIdent *pdxlopScIdent = CDXLScalarIdent::Cast(pdxlnExpr->GetOperator());

		Index idxVarno = OUTER;
		AttrNumber attno = (AttrNumber) (ul + 1);

		Var *var = gpdb::MakeVar
							(
							idxVarno,
							attno,
							CMDIdGPDB::CastMdid(pdxlopScIdent->MDIdType())->OidObjectId(),
							pdxlopScIdent->TypeModifier(),
							0	// varlevelsup
							);

		TargetEntry *target_entry = MakeNode(TargetEntry);
		target_entry->expr = (Expr *) var;
		target_entry->resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrel->GetMdNameAlias()->GetMDName()->GetBuffer());
		target_entry->resno = attno;

		// add column mapping to output translation context
		output_context->InsertMapping(pdxlopPrel->Id(), target_entry);

		plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);
	}

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(output_context));

	// translate filter
	plan->qual = TranslateDXLFilterToQual
					(
					filter_dxlnode,
					NULL, // translate context for the base table
					child_contexts,
					output_context
					);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) pappend;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLMaterialize
//
//	@doc:
//		Translate DXL materialize node into GPDB Material plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLMaterialize
	(
	const CDXLNode *materialize_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create materialize plan node
	Material *pmat = MakeNode(Material);

	Plan *plan = &(pmat->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMaterialize *pdxlopMat = CDXLPhysicalMaterialize::Cast(materialize_dxlnode->GetOperator());

	pmat->cdb_strict = pdxlopMat->IsEager();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(materialize_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate materialize child
	CDXLNode *dxl_node_child = (*materialize_dxlnode)[EdxlmatIndexChild];

	CDXLNode *project_list_dxl = (*materialize_dxlnode)[EdxlmatIndexProjList];
	CDXLNode *filter_dxlnode = (*materialize_dxlnode)[EdxlmatIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	// set spooling info
	if (pdxlopMat->IsSpooling())
	{
		pmat->share_id = pdxlopMat->GetSpoolingOpId();
		pmat->driver_slice = pdxlopMat->GetExecutorSlice();
		pmat->nsharer_xslice = pdxlopMat->GetNumConsumerSlices();
		pmat->share_type = (0 < pdxlopMat->GetNumConsumerSlices()) ?
							SHARE_MATERIAL_XSLICE : SHARE_MATERIAL;
	}
	else
	{
		pmat->share_type = SHARE_NOTSHARED;
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) pmat;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLCTEProducerToSharedScan
//
//	@doc:
//		Translate DXL CTE Producer node into GPDB share input scan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLCTEProducerToSharedScan
	(
	const CDXLNode *cte_producer_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTEProducer *pdxlopCTEProducer = CDXLPhysicalCTEProducer::Cast(cte_producer_dxlnode->GetOperator());
	ULONG ulCTEId = pdxlopCTEProducer->Id();

	// create the shared input scan representing the CTE Producer
	ShareInputScan *pshscanCTEProducer = MakeNode(ShareInputScan);
	pshscanCTEProducer->share_id = ulCTEId;
	Plan *plan = &(pshscanCTEProducer->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// store share scan node for the translation of CTE Consumers
	m_dxl_to_plstmt_context->AddCTEConsumerInfo(ulCTEId, pshscanCTEProducer);

	// translate cost of the producer
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(cte_producer_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate child plan
	CDXLNode *project_list_dxl = (*cte_producer_dxlnode)[0];
	CDXLNode *dxl_node_child = (*cte_producer_dxlnode)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());
	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);
	GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxChild);
	// translate proj list
	plan->targetlist = TranslateDXLProjList
							(
							project_list_dxl,
							NULL,		// base table translation context
							child_contexts,
							output_context
							);

	// if the child node is neither a sort or materialize node then add a materialize node
	if (!IsA(pplanChild, Material) && !IsA(pplanChild, Sort))
	{
		Material *pmat = MakeNode(Material);
		pmat->cdb_strict = false; // eager-free

		Plan *pplanMat = &(pmat->plan);
		pplanMat->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

		TranslatePlanCosts
			(
			CDXLPhysicalProperties::PdxlpropConvert(cte_producer_dxlnode->GetProperties())->GetDXLOperatorCost(),
			&(pplanMat->startup_cost),
			&(pplanMat->total_cost),
			&(pplanMat->plan_rows),
			&(pplanMat->plan_width)
			);

		// create a target list for the newly added materialize
		ListCell *plcTe = NULL;
		pplanMat->targetlist = NIL;
		ForEach (plcTe, plan->targetlist)
		{
			TargetEntry *target_entry = (TargetEntry *) lfirst(plcTe);
			Expr *expr = target_entry->expr;
			GPOS_ASSERT(IsA(expr, Var));

			Var *var = (Var *) expr;
			Var *pvarNew = gpdb::MakeVar(OUTER, var->varattno, var->vartype, var->vartypmod,	0 /* varlevelsup */);
			pvarNew->varnoold = var->varnoold;
			pvarNew->varoattno = var->varoattno;

			TargetEntry *pteNew = gpdb::MakeTargetEntry((Expr *) pvarNew, var->varattno, PStrDup(target_entry->resname), target_entry->resjunk);
			pplanMat->targetlist = gpdb::LAppend(pplanMat->targetlist, pteNew);
		}

		pplanMat->lefttree = pplanChild;
		pplanMat->nMotionNodes = pplanChild->nMotionNodes;

		pplanChild = pplanMat;
	}

	InitializeSpoolingInfo(pplanChild, ulCTEId);

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;
	plan->qual = NIL;
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) pshscanCTEProducer;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::InitializeSpoolingInfo
//
//	@doc:
//		Initialize spooling information for (1) the materialize/sort node under the
//		shared input scan nodes representing the CTE producer node and
//		(2) SIS nodes representing the producer/consumer nodes
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::InitializeSpoolingInfo
	(
	Plan *plan,
	ULONG share_id
	)
{
	List *shared_scan_cte_consumer_list = m_dxl_to_plstmt_context->GetCTEConsumerList(share_id);
	GPOS_ASSERT(NULL != shared_scan_cte_consumer_list);

	Flow *pflow = GetFlowCTEConsumer(shared_scan_cte_consumer_list);

	const ULONG ulLenSis = gpdb::ListLength(shared_scan_cte_consumer_list);

	ShareType share_type = SHARE_NOTSHARED;

	if (IsA(plan, Material))
	{
		Material *pmat = (Material *) plan;
		pmat->share_id = share_id;
		pmat->nsharer = ulLenSis;
		share_type = SHARE_MATERIAL;
		// the share_type is later reset to SHARE_MATERIAL_XSLICE (if needed) by the apply_shareinput_xslice
		pmat->share_type = share_type;
		GPOS_ASSERT(NULL == (pmat->plan).flow);
		(pmat->plan).flow = pflow;
	}
	else
	{
		GPOS_ASSERT(IsA(plan, Sort));
		Sort *psort = (Sort *) plan;
		psort->share_id = share_id;
		psort->nsharer = ulLenSis;
		share_type = SHARE_SORT;
		// the share_type is later reset to SHARE_SORT_XSLICE (if needed) the apply_shareinput_xslice
		psort->share_type = share_type;
		GPOS_ASSERT(NULL == (psort->plan).flow);
		(psort->plan).flow = pflow;
	}

	GPOS_ASSERT(SHARE_NOTSHARED != share_type);

	// set the share type of the consumer nodes based on the producer
	ListCell *plcShscanCTEConsumer = NULL;
	ForEach (plcShscanCTEConsumer, shared_scan_cte_consumer_list)
	{
		ShareInputScan *pshscanConsumer = (ShareInputScan *) lfirst(plcShscanCTEConsumer);
		pshscanConsumer->share_type = share_type;
		pshscanConsumer->driver_slice = -1; // default
		if (NULL == (pshscanConsumer->scan.plan).flow)
		{
			(pshscanConsumer->scan.plan).flow = (Flow *) gpdb::CopyObject(pflow);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::GetFlowCTEConsumer
//
//	@doc:
//		Retrieve the flow of the shared input scan of the cte consumers. If
//		multiple CTE consumers have a flow then ensure that they are of the
//		same type
//---------------------------------------------------------------------------
Flow *
CTranslatorDXLToPlStmt::GetFlowCTEConsumer
	(
	List *shared_scan_cte_consumer_list
	)
{
	Flow *pflow = NULL;

	ListCell *plcShscanCTEConsumer = NULL;
	ForEach (plcShscanCTEConsumer, shared_scan_cte_consumer_list)
	{
		ShareInputScan *pshscanConsumer = (ShareInputScan *) lfirst(plcShscanCTEConsumer);
		Flow *pflowCte = (pshscanConsumer->scan.plan).flow;
		if (NULL != pflowCte)
		{
			if (NULL == pflow)
			{
				pflow = (Flow *) gpdb::CopyObject(pflowCte);
			}
			else
			{
				GPOS_ASSERT(pflow->flotype == pflowCte->flotype);
			}
		}
	}

	if (NULL == pflow)
	{
		pflow = MakeNode(Flow);
		pflow->flotype = FLOW_UNDEFINED; // default flow
	}

	return pflow;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLCTEConsumerToSharedScan
//
//	@doc:
//		Translate DXL CTE Consumer node into GPDB share input scan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLCTEConsumerToSharedScan
	(
	const CDXLNode *cte_consumer_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTEConsumer *pdxlopCTEConsumer = CDXLPhysicalCTEConsumer::Cast(cte_consumer_dxlnode->GetOperator());
	ULONG ulCTEId = pdxlopCTEConsumer->Id();

	ShareInputScan *pshscanCTEConsumer = MakeNode(ShareInputScan);
	pshscanCTEConsumer->share_id = ulCTEId;

	Plan *plan = &(pshscanCTEConsumer->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(cte_consumer_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

#ifdef GPOS_DEBUG
	ULongPtrArray *pdrgpulCTEColId = pdxlopCTEConsumer->GetOutputColIdsArray();
#endif

	// generate the target list of the CTE Consumer
	plan->targetlist = NIL;
	CDXLNode *project_list_dxl = (*cte_consumer_dxlnode)[0];
	const ULONG ulLenPrL = project_list_dxl->Arity();
	GPOS_ASSERT(ulLenPrL == pdrgpulCTEColId->Size());
	for (ULONG ul = 0; ul < ulLenPrL; ul++)
	{
		CDXLNode *pdxlnPrE = (*project_list_dxl)[ul];
		CDXLScalarProjElem *pdxlopPrE = CDXLScalarProjElem::Cast(pdxlnPrE->GetOperator());
		ULONG col_id = pdxlopPrE->Id();
		GPOS_ASSERT(col_id == *(*pdrgpulCTEColId)[ul]);

		CDXLNode *pdxlnScIdent = (*pdxlnPrE)[0];
		CDXLScalarIdent *pdxlopScIdent = CDXLScalarIdent::Cast(pdxlnScIdent->GetOperator());
		OID oidType = CMDIdGPDB::CastMdid(pdxlopScIdent->MDIdType())->OidObjectId();

		Var *var = gpdb::MakeVar(OUTER, (AttrNumber) (ul + 1), oidType, pdxlopScIdent->TypeModifier(),  0	/* varlevelsup */);

		CHAR *szResname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrE->GetMdNameAlias()->GetMDName()->GetBuffer());
		TargetEntry *target_entry = gpdb::MakeTargetEntry((Expr *) var, (AttrNumber) (ul + 1), szResname, false /* resjunk */);
		plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);

		output_context->InsertMapping(col_id, target_entry);
	}

	plan->qual = NULL;

	SetParamIds(plan);

	// store share scan node for the translation of CTE Consumers
	m_dxl_to_plstmt_context->AddCTEConsumerInfo(ulCTEId, pshscanCTEConsumer);

	return (Plan *) pshscanCTEConsumer;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLSequence
//
//	@doc:
//		Translate DXL sequence node into GPDB Sequence plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLSequence
	(
	const CDXLNode *sequence_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create append plan node
	Sequence *psequence = MakeNode(Sequence);

	Plan *plan = &(psequence->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(sequence_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	ULONG arity = sequence_dxlnode->Arity();
	
	// translate last child
	// since last child may be a DynamicIndexScan with outer references,
	// we pass the context received from parent to translate outer refs here

	CDXLNode *pdxlnLastChild = (*sequence_dxlnode)[arity - 1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanLastChild = TranslateDXLOperatorToPlan(pdxlnLastChild, &dxltrctxChild, ctxt_translation_prev_siblings);
	plan->nMotionNodes = pplanLastChild->nMotionNodes;

	CDXLNode *project_list_dxl = (*sequence_dxlnode)[0];

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list
	plan->targetlist = TranslateDXLProjList
						(
						project_list_dxl,
						NULL,		// base table translation context
						child_contexts,
						output_context
						);

	// translate the rest of the children
	for (ULONG ul = 1; ul < arity - 1; ul++)
	{
		CDXLNode *dxl_node_child = (*sequence_dxlnode)[ul];

		Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

		psequence->subplans = gpdb::LAppend(psequence->subplans, pplanChild);
		plan->nMotionNodes += pplanChild->nMotionNodes;
	}

	psequence->subplans = gpdb::LAppend(psequence->subplans, pplanLastChild);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) psequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLDynTblScan
//
//	@doc:
//		Translates a DXL dynamic table scan node into a DynamicTableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLDynTblScan
	(
	const CDXLNode *dyn_tbl_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDynamicTableScan *dxl_dyn_tbl_scan_op = CDXLPhysicalDynamicTableScan::Cast(dyn_tbl_scan_dxlnode->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(dxl_dyn_tbl_scan_op->GetDXLTableDescr(), NULL /*index_descr_dxl*/, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;

	m_dxl_to_plstmt_context->AddRTE(rte);

	// create dynamic scan node
	DynamicTableScan *pdts = MakeNode(DynamicTableScan);

	pdts->scanrelid = index;
	pdts->partIndex = dxl_dyn_tbl_scan_op->GetPartIndexId();
	pdts->partIndexPrintable = dxl_dyn_tbl_scan_op->GetPartIndexIdPrintable();

	Plan *plan = &(pdts->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(dyn_tbl_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	GPOS_ASSERT(2 == dyn_tbl_scan_dxlnode->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*dyn_tbl_scan_dxlnode)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*dyn_tbl_scan_dxlnode)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		NULL,			// pdxltrctxLeft and pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	SetParamIds(plan);

	return (Plan *) pdts;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLDynIdxScan
//
//	@doc:
//		Translates a DXL dynamic index scan node into a DynamicIndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLDynIdxScan
	(
	const CDXLNode *dyn_idx_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalDynamicIndexScan *dxl_dyn_index_scan_op = CDXLPhysicalDynamicIndexScan::Cast(dyn_idx_scan_dxlnode->GetOperator());
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(dxl_dyn_index_scan_op->GetDXLTableDescr()->MDId());
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(dxl_dyn_index_scan_op->GetDXLTableDescr(), NULL /*index_descr_dxl*/, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	DynamicIndexScan *pdis = MakeNode(DynamicIndexScan);

	pdis->indexscan.scan.scanrelid = index;
	pdis->indexscan.scan.partIndex = dxl_dyn_index_scan_op->GetPartIndexId();
	pdis->indexscan.scan.partIndexPrintable = dxl_dyn_index_scan_op->GetPartIndexIdPrintable();

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(dxl_dyn_index_scan_op->GetDXLIndexDescr()->MDId());
	const IMDIndex *md_index = m_md_accessor->RetrieveIndex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pdis->indexscan.indexid = oidIndex;
	pdis->logicalIndexInfo = gpdb::GetLogicalIndexInfo(rte->relid, oidIndex);

	Plan *plan = &(pdis->indexscan.scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(dyn_idx_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// an index scan node must have 3 children: projection list, filter and index condition list
	GPOS_ASSERT(3 == dyn_idx_scan_dxlnode->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexProjList];
	CDXLNode *filter_dxlnode = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexFilter];
	CDXLNode *index_cond_list_dxlnode = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexCondition];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxl, &dxltrctxbt, NULL /*child_contexts*/, output_context);

	// translate index filter
	plan->qual = TranslateDXLIndexFilter
					(
					filter_dxlnode,
					output_context,
					&dxltrctxbt,
					ctxt_translation_prev_siblings
					);

	pdis->indexscan.indexorderdir = CTranslatorUtils::GetScanDirection(dxl_dyn_index_scan_op->GetIndexScanDir());

	// translate index condition list
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		index_cond_list_dxlnode, 
		dxl_dyn_index_scan_op->GetDXLTableDescr(),
		false, // is_index_only_scan 
		md_index,
		md_rel,
		output_context,
		&dxltrctxbt,
		ctxt_translation_prev_siblings,
		&plIndexConditions, 
		&plIndexOrigConditions, 
		&plIndexStratgey, 
		&plIndexSubtype
		);


	pdis->indexscan.indexqual = plIndexConditions;
	pdis->indexscan.indexqualorig = plIndexOrigConditions;
	/*
	 * As of 8.4, the indexstrategy and indexsubtype fields are no longer
	 * available or needed in IndexScan. Ignore them.
	 */
	SetParamIds(plan);

	return (Plan *) pdis;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLDml
//
//	@doc:
//		Translates a DXL DML node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLDml
	(
	const CDXLNode *dml_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDML *dxl_phy_dml_op = CDXLPhysicalDML::Cast(dml_dxlnode->GetOperator());

	// create DML node
	DML *pdml = MakeNode(DML);
	Plan *plan = &(pdml->plan);
	AclMode aclmode = ACL_NO_RIGHTS;
	
	switch (dxl_phy_dml_op->GetDmlOpType())
	{
		case gpdxl::Edxldmldelete:
		{
			m_cmd_type = CMD_DELETE;
			aclmode = ACL_DELETE;
			break;
		}
		case gpdxl::Edxldmlupdate:
		{
			m_cmd_type = CMD_UPDATE;
			aclmode = ACL_UPDATE;
			break;
		}
		case gpdxl::Edxldmlinsert:
		{
			m_cmd_type = CMD_INSERT;
			aclmode = ACL_INSERT;
			break;
		}
		case gpdxl::EdxldmlSentinel:
		default:
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,
				GPOS_WSZ_LIT("Unexpected error during plan generation."));
			break;
		}
	}
	
	IMDId *pmdidTargetTable = dxl_phy_dml_op->GetDXLTableDescr()->MDId();
	if (IMDRelation::EreldistrMasterOnly != m_md_accessor->RetrieveRel(pmdidTargetTable)->GetRelDistribution())
	{
		m_is_tgt_tbl_distributed = true;
	}
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	pdml->scanrelid = index;
	
	m_result_rel_list = gpdb::LAppendInt(m_result_rel_list, index);

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(dxl_phy_dml_op->GetDXLTableDescr()->MDId());

	CDXLTableDescr *table_descr = dxl_phy_dml_op->GetDXLTableDescr();
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(table_descr, NULL /*index_descr_dxl*/, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= aclmode;
	m_dxl_to_plstmt_context->AddRTE(rte);
	
	CDXLNode *project_list_dxl = (*dml_dxlnode)[0];
	CDXLNode *dxl_node_child = (*dml_dxlnode)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxChild);

	// translate proj list
	List *plTargetListDML = TranslateDXLProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);
	
	if (md_rel->HasDroppedColumns())
	{
		// pad DML target list with NULLs for dropped columns for all DML operator types
		List *plTargetListWithDroppedCols = CreateTargetListWithNullsForDroppedCols(plTargetListDML, md_rel);
		gpdb::GPDBFree(plTargetListDML);
		plTargetListDML = plTargetListWithDroppedCols;
	}

	// Extract column numbers of the action and ctid columns from the
	// target list. ORCA also includes a third similar column for
	// partition Oid to the target list, but we don't use it for anything
	// in GPDB.
	pdml->actionColIdx = AddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, dxl_phy_dml_op->ActionColId(), true /*is_resjunk*/);
	pdml->ctidColIdx = AddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, dxl_phy_dml_op->GetCtIdColId(), true /*is_resjunk*/);
	if (dxl_phy_dml_op->IsOidsPreserved())
	{
		pdml->tupleoidColIdx = AddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, dxl_phy_dml_op->GetTupleOid(), true /*is_resjunk*/);
	}
	else
	{
		pdml->tupleoidColIdx = 0;
	}

	GPOS_ASSERT(0 != pdml->actionColIdx);

	plan->targetlist = plTargetListDML;
	
	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	if (CMD_INSERT == m_cmd_type && 0 == plan->nMotionNodes)
	{
		List *plDirectDispatchSegIds = TranslateDXLDirectDispatchInfo(dxl_phy_dml_op->GetDXLDirectDispatchInfo());
		plan->directDispatch.contentIds = plDirectDispatchSegIds;
		plan->directDispatch.isDirectDispatch = (NIL != plDirectDispatchSegIds);
	}
	
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();


	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(dml_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	return (Plan *) pdml;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLDirectDispatchInfo
//
//	@doc:
//		Translate the direct dispatch info
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLDirectDispatchInfo
	(
	CDXLDirectDispatchInfo *dxl_direct_dispatch_info
	)
{
	if (!optimizer_enable_direct_dispatch || NULL == dxl_direct_dispatch_info)
	{
		return NIL;
	}
	
	DXLDatumArrays *pdrgpdrgpdxldatum = dxl_direct_dispatch_info->GetDispatchIdentifierDatumArray();
	
	if (pdrgpdrgpdxldatum == NULL || 0 == pdrgpdrgpdxldatum->Size())
	{
		return NIL;
	}
	
	DXLDatumArray *dxl_datum_array = (*pdrgpdrgpdxldatum)[0];
	GPOS_ASSERT(0 < dxl_datum_array->Size());
		
	ULONG ulHashCode = GetDXLDatumGPDBHash(dxl_datum_array);
	const ULONG length = pdrgpdrgpdxldatum->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		DXLDatumArray *pdrgpdxldatumDisj = (*pdrgpdrgpdxldatum)[ul];
		GPOS_ASSERT(0 < pdrgpdxldatumDisj->Size());
		ULONG ulHashCodeNew = GetDXLDatumGPDBHash(pdrgpdxldatumDisj);
		
		if (ulHashCode != ulHashCodeNew)
		{
			// values don't hash to the same segment
			return NIL;
		}
	}
	
	List *plSegIds = gpdb::LAppendInt(NIL, ulHashCode);
	return plSegIds;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::GetDXLDatumGPDBHash
//
//	@doc:
//		Hash a DXL datum
//
//---------------------------------------------------------------------------
ULONG
CTranslatorDXLToPlStmt::GetDXLDatumGPDBHash
	(
	DXLDatumArray *dxl_datum_array
	)
{
	List *plConsts = NIL;
	
	const ULONG length = dxl_datum_array->Size();
	
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLDatum *datum_dxl = (*dxl_datum_array)[ul];
		
		Const *pconst = (Const *) m_translator_dxl_to_scalar->CreateConstExprFromDXL(datum_dxl);
		plConsts = gpdb::LAppend(plConsts, pconst);
	}

	ULONG ulHash = gpdb::CdbHashConstList(plConsts, m_num_of_segments);

	gpdb::ListFreeDeep(plConsts);
	
	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLSplit
//
//	@doc:
//		Translates a DXL Split node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLSplit
	(
	const CDXLNode *split_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalSplit *dxl_phy_split_op = CDXLPhysicalSplit::Cast(split_dxlnode->GetOperator());

	// create SplitUpdate node
	SplitUpdate *psplit = MakeNode(SplitUpdate);
	Plan *plan = &(psplit->plan);
	
	CDXLNode *project_list_dxl = (*split_dxlnode)[0];
	CDXLNode *dxl_node_child = (*split_dxlnode)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxChild);

	// translate proj list and filter
	plan->targetlist = TranslateDXLProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);

	// translate delete and insert columns
	ULongPtrArray *pdrgpulDeleteCols = dxl_phy_split_op->GetDeletionColIdArray();
	ULongPtrArray *pdrgpulInsertCols = dxl_phy_split_op->GetInsertionColIdArray();
		
	GPOS_ASSERT(pdrgpulInsertCols->Size() == pdrgpulDeleteCols->Size());
	
	psplit->deleteColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulDeleteCols, &dxltrctxChild);
	psplit->insertColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulInsertCols, &dxltrctxChild);
	
	const TargetEntry *pteActionCol = output_context->GetTargetEntry(dxl_phy_split_op->ActionColId());
	const TargetEntry *pteCtidCol = output_context->GetTargetEntry(dxl_phy_split_op->GetCtIdColId());
	const TargetEntry *pteTupleOidCol = output_context->GetTargetEntry(dxl_phy_split_op->GetTupleOid());

	if (NULL  == pteActionCol)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, dxl_phy_split_op->ActionColId());
	}
	if (NULL  == pteCtidCol)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, dxl_phy_split_op->GetCtIdColId());
	}	 
	
	psplit->actionColIdx = pteActionCol->resno;
	psplit->ctidColIdx = pteCtidCol->resno;
	
	psplit->tupleoidColIdx = FirstLowInvalidHeapAttributeNumber;
	if (NULL != pteTupleOidCol)
	{
		psplit->tupleoidColIdx = pteTupleOidCol->resno;
	}

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(split_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	return (Plan *) psplit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLAssert
//
//	@doc:
//		Translate DXL assert node into GPDB assert plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLAssert
	(
	const CDXLNode *assert_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// create assert plan node
	AssertOp *passert = MakeNode(AssertOp);

	Plan *plan = &(passert->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalAssert *pdxlopAssert = CDXLPhysicalAssert::Cast(assert_dxlnode->GetOperator());

	// translate error code into the its internal GPDB representation
	const CHAR *error_code = pdxlopAssert->GetSQLState();
	GPOS_ASSERT(GPOS_SQLSTATE_LENGTH == clib::Strlen(error_code));
	
	passert->errcode = MAKE_SQLSTATE(error_code[0], error_code[1], error_code[2], error_code[3], error_code[4]);
	CDXLNode *filter_dxlnode = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexFilter];

	passert->errmessage = CTranslatorUtils::GetAssertErrorMsgs(filter_dxlnode);

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(assert_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	// translate child plan
	CDXLNode *dxl_node_child = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexChild];
	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

	passert->plan.lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;

	CDXLNode *project_list_dxl = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexProjList];

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list
	plan->targetlist = TranslateDXLProjList
				(
				project_list_dxl,
				NULL,			// translate context for the base table
				child_contexts,
				output_context
				);

	// translate assert constraints
	plan->qual = TranslateDXLAssertConstraints
					(
					filter_dxlnode,
					output_context,
					child_contexts
					);
	
	GPOS_ASSERT(gpdb::ListLength(plan->qual) == gpdb::ListLength(passert->errmessage));
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) passert;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLRowTrigger
//
//	@doc:
//		Translates a DXL Row Trigger node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLRowTrigger
	(
	const CDXLNode *row_trigger_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalRowTrigger *dxl_phy_row_trigger_op = CDXLPhysicalRowTrigger::Cast(row_trigger_dxlnode->GetOperator());

	// create RowTrigger node
	RowTrigger *prowtrigger = MakeNode(RowTrigger);
	Plan *plan = &(prowtrigger->plan);

	CDXLNode *project_list_dxl = (*row_trigger_dxlnode)[0];
	CDXLNode *dxl_node_child = (*row_trigger_dxlnode)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *pplanChild = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxChild);

	// translate proj list and filter
	plan->targetlist = TranslateDXLProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);

	Oid oidRelid = CMDIdGPDB::CastMdid(dxl_phy_row_trigger_op->GetRelMdId())->OidObjectId();
	GPOS_ASSERT(InvalidOid != oidRelid);
	prowtrigger->relid = oidRelid;
	prowtrigger->eventFlags = dxl_phy_row_trigger_op->GetType();

	// translate old and new columns
	ULongPtrArray *pdrgpulOldCols = dxl_phy_row_trigger_op->GetColIdsOld();
	ULongPtrArray *pdrgpulNewCols = dxl_phy_row_trigger_op->GetColIdsNew();

	GPOS_ASSERT_IMP(NULL != pdrgpulOldCols && NULL != pdrgpulNewCols,
					pdrgpulNewCols->Size() == pdrgpulOldCols->Size());

	if (NULL == pdrgpulOldCols)
	{
		prowtrigger->oldValuesColIdx = NIL;
	}
	else
	{
		prowtrigger->oldValuesColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulOldCols, &dxltrctxChild);
	}

	if (NULL == pdrgpulNewCols)
	{
		prowtrigger->newValuesColIdx = NIL;
	}
	else
	{
		prowtrigger->newValuesColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulNewCols, &dxltrctxChild);
	}

	plan->lefttree = pplanChild;
	plan->nMotionNodes = pplanChild->nMotionNodes;
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(row_trigger_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	return (Plan *) prowtrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLTblDescrToRangeTblEntry
//
//	@doc:
//		Translates a DXL table descriptor into a range table entry. If an index
//		descriptor is provided, we use the mapping from colids to index attnos
//		instead of table attnos
//
//---------------------------------------------------------------------------
RangeTblEntry *
CTranslatorDXLToPlStmt::TranslateDXLTblDescrToRangeTblEntry
	(
	const CDXLTableDescr *table_descr,
	const CDXLIndexDescr *index_descr_dxl, // should be NULL unless we have an index-only scan
	Index index,
	CDXLTranslateContextBaseTable *base_table_context
	)
{
	GPOS_ASSERT(NULL != table_descr);

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());
	const ULONG ulRelColumns = CTranslatorUtils::GetNumNonSystemColumns(md_rel);

	RangeTblEntry *rte = MakeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;

	// get the index if given
	const IMDIndex *md_index = NULL;
	if (NULL != index_descr_dxl)
	{
		md_index = m_md_accessor->RetrieveIndex(index_descr_dxl->MDId());
	}

	// get oid for table
	Oid oid = CMDIdGPDB::CastMdid(table_descr->MDId())->OidObjectId();
	GPOS_ASSERT(InvalidOid != oid);

	rte->relid = oid;
	rte->checkAsUser = table_descr->GetExecuteAsUserId();
	rte->requiredPerms |= ACL_NO_RIGHTS;

	// save oid and range index in translation context
	base_table_context->SetOID(oid);
	base_table_context->SetRelIndex(index);

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get table alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(table_descr->MdName()->GetMDName()->GetBuffer());

	// get column names
	const ULONG arity = table_descr->Arity();
	
	INT iLastAttno = 0;
	
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		const CDXLColDescr *dxl_col_descr = table_descr->GetColumnDescrAt(ul);
		GPOS_ASSERT(NULL != dxl_col_descr);

		INT attno = dxl_col_descr->AttrNum();

		GPOS_ASSERT(0 != attno);

		if (0 < attno)
		{
			// if attno > iLastAttno + 1, there were dropped attributes
			// add those to the RTE as they are required by GPDB
			for (INT iDroppedColAttno = iLastAttno + 1; iDroppedColAttno < attno; iDroppedColAttno++)
			{
				Value *pvalDroppedColName = gpdb::MakeStringValue(PStrDup(""));
				palias->colnames = gpdb::LAppend(palias->colnames, pvalDroppedColName);
			}
			
			// non-system attribute
			CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_col_descr->MdName()->GetMDName()->GetBuffer());
			Value *pvalColName = gpdb::MakeStringValue(col_name_char_array);

			palias->colnames = gpdb::LAppend(palias->colnames, pvalColName);
			iLastAttno = attno;
		}

		// get the attno from the index, in case of indexonlyscan
		if (NULL != md_index)
		{
			attno = 1 + md_index->GetKeyPos((ULONG) attno - 1);
		}

		// save mapping col id -> index in translate context
		(void) base_table_context->InsertMapping(dxl_col_descr->Id(), attno);
	}

	// if there are any dropped columns at the end, add those too to the RangeTblEntry
	for (ULONG ul = iLastAttno + 1; ul <= ulRelColumns; ul++)
	{
		Value *pvalDroppedColName = gpdb::MakeStringValue(PStrDup(""));
		palias->colnames = gpdb::LAppend(palias->colnames, pvalDroppedColName);
	}
	
	rte->eref = palias;

	return rte;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLProjList
//
//	@doc:
//		Translates a DXL projection list node into a target list.
//		For base table projection lists, the caller should provide a base table
//		translation context with table oid, rtable index and mappings for the columns.
//		For other nodes pdxltrctxLeft and pdxltrctxRight give
//		the mappings of column ids to target entries in the corresponding child nodes
//		for resolving the origin of the target entries
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLProjList
	(
	const CDXLNode *project_list_dxl,
	const CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	if (NULL == project_list_dxl)
	{
		return NULL;
	}

	List *target_list = NIL;

	// translate each DXL project element into a target entry
	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *pdxlnPrEl = (*project_list_dxl)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == pdxlnPrEl->GetOperator()->GetDXLOperator());
		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());
		GPOS_ASSERT(1 == pdxlnPrEl->Arity());

		// translate proj element expression
		CDXLNode *pdxlnExpr = (*pdxlnPrEl)[0];

		CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
																(
																m_memory_pool,
																base_table_context,
																child_contexts,
																output_context,
																m_dxl_to_plstmt_context
																);

		Expr *expr = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnExpr, &mapcidvarplstmt);

		GPOS_ASSERT(NULL != expr);

		TargetEntry *target_entry = MakeNode(TargetEntry);
		target_entry->expr = expr;
		target_entry->resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrel->GetMdNameAlias()->GetMDName()->GetBuffer());
		target_entry->resno = (AttrNumber) (ul + 1);

		if (IsA(expr, Var))
		{
			// check the origin of the left or the right side
			// of the current operator and if it is derived from a base relation,
			// set resorigtbl and resorigcol appropriately

			if (NULL != base_table_context)
			{
				// translating project list of a base table
				target_entry->resorigtbl = base_table_context->GetOid();
				target_entry->resorigcol = ((Var *) expr)->varattno;
			}
			else
			{
				// not translating a base table proj list: variable must come from
				// the left or right child of the operator

				GPOS_ASSERT(NULL != child_contexts);
				GPOS_ASSERT(0 != child_contexts->Size());
				ULONG col_id = CDXLScalarIdent::Cast(pdxlnExpr->GetOperator())->MakeDXLColRef()->Id();

				const CDXLTranslateContext *pdxltrctxLeft = (*child_contexts)[0];
				GPOS_ASSERT(NULL != pdxltrctxLeft);
				const TargetEntry *pteOriginal = pdxltrctxLeft->GetTargetEntry(col_id);

				if (NULL == pteOriginal)
				{
					// variable not found on the left side
					GPOS_ASSERT(2 == child_contexts->Size());
					const CDXLTranslateContext *pdxltrctxRight = (*child_contexts)[1];

					GPOS_ASSERT(NULL != pdxltrctxRight);
					pteOriginal = pdxltrctxRight->GetTargetEntry(col_id);
				}

				if (NULL  == pteOriginal)
				{
					GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, col_id);
				}	
				target_entry->resorigtbl = pteOriginal->resorigtbl;
				target_entry->resorigcol = pteOriginal->resorigcol;
			}
		}

		// add column mapping to output translation context
		output_context->InsertMapping(pdxlopPrel->Id(), target_entry);

		target_list = gpdb::LAppend(target_list, target_entry);
	}

	return target_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::CreateTargetListWithNullsForDroppedCols
//
//	@doc:
//		Construct the target list for a DML statement by adding NULL elements
//		for dropped columns
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::CreateTargetListWithNullsForDroppedCols
	(
	List *target_list,
	const IMDRelation *md_rel
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(gpdb::ListLength(target_list) <= md_rel->ColumnCount());

	List *plResult = NIL;
	ULONG ulLastTLElem = 0;
	ULONG ulResno = 1;
	
	const ULONG ulRelCols = md_rel->ColumnCount();
	
	for (ULONG ul = 0; ul < ulRelCols; ul++)
	{
		const IMDColumn *pmdcol = md_rel->GetMdCol(ul);
		
		if (pmdcol->IsSystemColumn())
		{
			continue;
		}
		
		Expr *expr = NULL;	
		if (pmdcol->IsDropped())
		{
			// add a NULL element
			OID oidType = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeInt4>()->MDId())->OidObjectId();

			expr = (Expr *) gpdb::MakeNULLConst(oidType);
		}
		else
		{
			TargetEntry *target_entry = (TargetEntry *) gpdb::ListNth(target_list, ulLastTLElem);
			expr = (Expr *) gpdb::CopyObject(target_entry->expr);
			ulLastTLElem++;
		}
		
		CHAR *szName = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pmdcol->Mdname().GetMDName()->GetBuffer());
		TargetEntry *pteNew = gpdb::MakeTargetEntry(expr, ulResno, szName, false /*resjunk*/);
		plResult = gpdb::LAppend(plResult, pteNew);
		ulResno++;
	}
	
	return plResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLProjectListToHashTargetList
//
//	@doc:
//		Create a target list for the hash node of a hash join plan node by creating a list
//		of references to the elements in the child project list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLProjectListToHashTargetList
	(
	const CDXLNode *project_list_dxl,
	CDXLTranslateContext *child_context,
	CDXLTranslateContext *output_context
	)
{
	List *target_list = NIL;
	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnPrEl = (*project_list_dxl)[ul];
		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());

		const TargetEntry *pteChild = child_context->GetTargetEntry(pdxlopPrel->Id());
		if (NULL  == pteChild)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, pdxlopPrel->Id());
		}	

		// get type oid for project element's expression
		GPOS_ASSERT(1 == pdxlnPrEl->Arity());

		// find column type
		OID oidType = gpdb::ExprType((Node*) pteChild->expr);
		INT type_modifier = gpdb::ExprTypeMod((Node *) pteChild->expr);

		// find the original varno and attno for this column
		Index idxVarnoold = 0;
		AttrNumber attnoOld = 0;

		if (IsA(pteChild->expr, Var))
		{
			Var *pv = (Var*) pteChild->expr;
			idxVarnoold = pv->varnoold;
			attnoOld = pv->varoattno;
		}
		else
		{
			idxVarnoold = OUTER;
			attnoOld = pteChild->resno;
		}

		// create a Var expression for this target list entry expression
		Var *var = gpdb::MakeVar
					(
					OUTER,
					pteChild->resno,
					oidType,
					type_modifier,
					0	// varlevelsup
					);

		// set old varno and varattno since makeVar does not set them
		var->varnoold = idxVarnoold;
		var->varoattno = attnoOld;

		CHAR *szResname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrel->GetMdNameAlias()->GetMDName()->GetBuffer());

		TargetEntry *target_entry = gpdb::MakeTargetEntry
							(
							(Expr *) var,
							(AttrNumber) (ul + 1),
							szResname,
							false		// resjunk
							);

		target_list = gpdb::LAppend(target_list, target_entry);
		output_context->InsertMapping(pdxlopPrel->Id(), target_entry);
	}

	return target_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLFilterToQual
//
//	@doc:
//		Translates a DXL filter node into a Qual list.
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLFilterToQual
	(
	const CDXLNode * filter_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	const ULONG arity = filter_dxlnode->Arity();
	if (0 == arity)
	{
		return NIL;
	}

	GPOS_ASSERT(1 == arity);

	CDXLNode *pdxlnFilterCond = (*filter_dxlnode)[0];
	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(pdxlnFilterCond, m_md_accessor));

	return TranslateDXLScCondToQual(pdxlnFilterCond, base_table_context, child_contexts, output_context);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLScCondToQual
//
//	@doc:
//		Translates a DXL scalar condition node node into a Qual list.
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLScCondToQual
	(
	const CDXLNode *pdxlnCond,
	const CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	List *plQuals = NIL;

	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(const_cast<CDXLNode*>(pdxlnCond), m_md_accessor));

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
															(
															m_memory_pool,
															base_table_context,
															child_contexts,
															output_context,
															m_dxl_to_plstmt_context
															);

	Expr *expr = m_translator_dxl_to_scalar->CreateScalarExprFromDXL
					(
					pdxlnCond,
					&mapcidvarplstmt
					);

	plQuals = gpdb::LAppend(plQuals, expr);

	return plQuals;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslatePlanCosts
//
//	@doc:
//		Translates DXL plan costs into the GPDB cost variables
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::TranslatePlanCosts
	(
	const CDXLOperatorCost *dxl_operator_cost,
	Cost *startup_cost_out,
	Cost *total_cost_out,
	Cost *cost_rows_out,
	INT * width_out
	)
{
	*startup_cost_out = CostFromStr(dxl_operator_cost->GetStartUpCostStr());
	*total_cost_out = CostFromStr(dxl_operator_cost->GetTotalCostStr());
	*cost_rows_out = CostFromStr(dxl_operator_cost->GetRowsOutStr());
	*width_out = CTranslatorUtils::GetIntFromStr(dxl_operator_cost->GetWidthStr());
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateProjListAndFilter
//
//	@doc:
//		Translates DXL proj list and filter into GPDB's target and qual lists,
//		respectively
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::TranslateProjListAndFilter
	(
	const CDXLNode *project_list_dxl,
	const CDXLNode *filter_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *child_contexts,
	List **targetlist_out,
	List **qual_out,
	CDXLTranslateContext *output_context
	)
{
	// translate proj list
	*targetlist_out = TranslateDXLProjList
						(
						project_list_dxl,
						base_table_context,		// base table translation context
						child_contexts,
						output_context
						);

	// translate filter
	*qual_out = TranslateDXLFilterToQual
					(
					filter_dxlnode,
					base_table_context,			// base table translation context
					child_contexts,
					output_context
					);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateHashExprList
//
//	@doc:
//		Translates DXL hash expression list in a redistribute motion node into
//		GPDB's hash expression and expression types lists, respectively
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::TranslateHashExprList
	(
	const CDXLNode *hash_expr_list_dxlnode,
	const CDXLTranslateContext *child_context,
	List **hash_expr_out_list,
	List **hash_expr_types_out_list,
	CDXLTranslateContext *output_context
	)
{
	GPOS_ASSERT(NIL == *hash_expr_out_list);
	GPOS_ASSERT(NIL == *hash_expr_types_out_list);

	List *plHashExpr = NIL;
	List *plHashExprTypes = NIL;

	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(child_context);

	const ULONG arity = hash_expr_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnHashExpr = (*hash_expr_list_dxlnode)[ul];
		CDXLScalarHashExpr *pdxlopHashExpr = CDXLScalarHashExpr::Cast(pdxlnHashExpr->GetOperator());

		// the type of the hash expression in GPDB is computed as the left operand 
		// of the equality operator of the actual hash expression type
		const IMDType *pmdtype = m_md_accessor->RetrieveType(pdxlopHashExpr->MDIdType());
		const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(pmdtype->GetMdidForCmpType(IMDType::EcmptEq));
		
		const IMDId *pmdidHashType = md_scalar_op->GetLeftMdid();
		
		plHashExprTypes = gpdb::LAppendOid(plHashExprTypes, CMDIdGPDB::CastMdid(pmdidHashType)->OidObjectId());

		GPOS_ASSERT(1 == pdxlnHashExpr->Arity());
		CDXLNode *pdxlnExpr = (*pdxlnHashExpr)[0];

		CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
																(
																m_memory_pool,
																NULL,
																child_contexts,
																output_context,
																m_dxl_to_plstmt_context
																);

		Expr *expr = m_translator_dxl_to_scalar->CreateScalarExprFromDXL(pdxlnExpr, &mapcidvarplstmt);

		plHashExpr = gpdb::LAppend(plHashExpr, expr);

		GPOS_ASSERT((ULONG) gpdb::ListLength(plHashExpr) == ul + 1);
	}


	*hash_expr_out_list = plHashExpr;
	*hash_expr_types_out_list = plHashExprTypes;

	// cleanup
	child_contexts->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateSortCols
//
//	@doc:
//		Translates DXL sorting columns list into GPDB's arrays of sorting attribute numbers,
//		and sorting operator ids, respectively.
//		The two arrays must be allocated by the caller.
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::TranslateSortCols
	(
	const CDXLNode *sort_col_list_dxl,
	const CDXLTranslateContext *child_context,
	AttrNumber *att_no_sort_colids,
	Oid *sort_op_oids,
	Oid *sort_collations_oids,
	bool *is_nulls_first
	)
{
	const ULONG arity = sort_col_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnSortCol = (*sort_col_list_dxl)[ul];
		CDXLScalarSortCol *pdxlopSortCol = CDXLScalarSortCol::Cast(pdxlnSortCol->GetOperator());

		ULONG ulSortColId = pdxlopSortCol->GetColId();
		const TargetEntry *pteSortCol = child_context->GetTargetEntry(ulSortColId);
		if (NULL  == pteSortCol)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, ulSortColId);
		}	

		att_no_sort_colids[ul] = pteSortCol->resno;
		sort_op_oids[ul] = CMDIdGPDB::CastMdid(pdxlopSortCol->GetMdIdSortOp())->OidObjectId();
		if (sort_collations_oids)
		{
			sort_collations_oids[ul] = gpdb::ExprCollation((Node *) pteSortCol->expr);
		}
		is_nulls_first[ul] = pdxlopSortCol->IsSortedNullsFirst();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::CostFromStr
//
//	@doc:
//		Parses a cost value from a string
//
//---------------------------------------------------------------------------
Cost
CTranslatorDXLToPlStmt::CostFromStr
	(
	const CWStringBase *str
	)
{
	CHAR *sz = CTranslatorUtils::CreateMultiByteCharStringFromWCString(str->GetBuffer());
	return gpos::clib::Strtod(sz);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::IsTgtTblDistributed
//
//	@doc:
//		Check if given operator is a DML on a distributed table
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToPlStmt::IsTgtTblDistributed
	(
	CDXLOperator *dxlop
	)
{
	if (EdxlopPhysicalDML != dxlop->GetDXLOperator())
	{
		return false;
	}

	CDXLPhysicalDML *pdxlopDML = CDXLPhysicalDML::Cast(dxlop);
	IMDId *pmdid = pdxlopDML->GetDXLTableDescr()->MDId();

	return IMDRelation::EreldistrMasterOnly != m_md_accessor->RetrieveRel(pmdid)->GetRelDistribution();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::IAddTargetEntryForColId
//
//	@doc:
//		Add a new target entry for the given colid to the given target list and
//		return the position of the new entry
//
//---------------------------------------------------------------------------
ULONG
CTranslatorDXLToPlStmt::AddTargetEntryForColId
	(
	List **target_list,
	CDXLTranslateContext *pdxltrctx,
	ULONG col_id,
	BOOL is_resjunk
	)
{
	GPOS_ASSERT(NULL != target_list);
	
	const TargetEntry *target_entry = pdxltrctx->GetTargetEntry(col_id);
	
	if (NULL == target_entry)
	{
		// colid not found in translate context
		return 0;
	}
	
	// TODO: Oct 29, 2012; see if entry already exists in the target list
	
	OID oidExpr = gpdb::ExprType((Node*) target_entry->expr);
	INT type_modifier = gpdb::ExprTypeMod((Node *) target_entry->expr);
	Var *var = gpdb::MakeVar
						(
						OUTER,
						target_entry->resno,
						oidExpr,
						type_modifier,
						0	// varlevelsup
						);
	ULONG ulResNo = gpdb::ListLength(*target_list) + 1;
	CHAR *szResName = PStrDup(target_entry->resname);
	TargetEntry *pteNew = gpdb::MakeTargetEntry((Expr*) var, ulResNo, szResName, is_resjunk);
	*target_list = gpdb::LAppend(*target_list, pteNew);
	
	return target_entry->resno;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::GetGPDBJoinTypeFromDXLJoinType
//
//	@doc:
//		Translates the join type from its DXL representation into the GPDB one
//
//---------------------------------------------------------------------------
JoinType
CTranslatorDXLToPlStmt::GetGPDBJoinTypeFromDXLJoinType
	(
	EdxlJoinType join_type
	)
{
	GPOS_ASSERT(EdxljtSentinel > join_type);

	JoinType jt = JOIN_INNER;

	switch (join_type)
	{
		case EdxljtInner:
			jt = JOIN_INNER;
			break;
		case EdxljtLeft:
			jt = JOIN_LEFT;
			break;
		case EdxljtFull:
			jt = JOIN_FULL;
			break;
		case EdxljtRight:
			jt = JOIN_RIGHT;
			break;
		case EdxljtIn:
			jt = JOIN_SEMI;
			break;
		case EdxljtLeftAntiSemijoin:
			jt = JOIN_ANTI;
			break;
		case EdxljtLeftAntiSemijoinNotIn:
			jt = JOIN_LASJ_NOTIN;
			break;
		default:
			GPOS_ASSERT(!"Unrecognized join type");
	}

	return jt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLCtas
//
//	@doc:
//		Sets the vartypmod fields in the target entries of the given target list
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::SetVarTypMod
	(
	const CDXLPhysicalCTAS *dxl_phy_ctas_op,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != target_list);

	IntPtrArray *var_type_mod_array = dxl_phy_ctas_op->GetVarTypeModArray();
	GPOS_ASSERT(var_type_mod_array->Size() == gpdb::ListLength(target_list));

	ULONG ul = 0;
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		if (IsA(target_entry->expr, Var))
		{
			Var *var = (Var*) target_entry->expr;
			var->vartypmod = *(*var_type_mod_array)[ul];
		}
		++ul;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLCtas
//
//	@doc:
//		Translates a DXL CTAS node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLCtas
	(
	const CDXLNode *pdxlnCTAS,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTAS *dxl_phy_ctas_op = CDXLPhysicalCTAS::Cast(pdxlnCTAS->GetOperator());
	CDXLNode *project_list_dxl = (*pdxlnCTAS)[0];
	CDXLNode *dxl_node_child = (*pdxlnCTAS)[1];

	GPOS_ASSERT(NULL == dxl_phy_ctas_op->GetDxlCtasStorageOption()->GetDXLCtasOptionArray());
	
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, output_context->GetColIdToParamIdMap());

	Plan *plan = TranslateDXLOperatorToPlan(dxl_node_child, &dxltrctxChild, ctxt_translation_prev_siblings);
	
	// fix target list to match the required column names
	DXLTranslationContextArr *child_contexts = GPOS_NEW(m_memory_pool) DXLTranslationContextArr(m_memory_pool);
	child_contexts->Append(&dxltrctxChild);
	
	List *target_list = TranslateDXLProjList
						(
						project_list_dxl,
						NULL,		// base_table_context
						child_contexts,
						output_context
						);
	SetVarTypMod(dxl_phy_ctas_op, target_list);
	
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();


	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnCTAS->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	IntoClause *pintocl = TranslateDXLPhyCtasToIntoClause(dxl_phy_ctas_op);
	GpPolicy *pdistrpolicy = TranslateDXLPhyCtasToDistrPolicy(dxl_phy_ctas_op);
	m_dxl_to_plstmt_context->AddCtasInfo(pintocl, pdistrpolicy);
	
	GPOS_ASSERT(IMDRelation::EreldistrMasterOnly != dxl_phy_ctas_op->Ereldistrpolicy());
	
	m_is_tgt_tbl_distributed = true;
	
	// Add a result node on top with the correct projection list
	Result *presult = MakeNode(Result);
	Plan *pplanResult = &(presult->plan);
	pplanResult->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	pplanResult->nMotionNodes = plan->nMotionNodes;
	pplanResult->lefttree = plan;

	pplanResult->targetlist = target_list;
	SetParamIds(pplanResult);

	plan = (Plan *) presult;

	return (Plan *) plan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLPhyCtasToIntoClause
//
//	@doc:
//		Translates a DXL CTAS into clause 
//
//---------------------------------------------------------------------------
IntoClause *
CTranslatorDXLToPlStmt::TranslateDXLPhyCtasToIntoClause
	(
	const CDXLPhysicalCTAS *dxl_phy_ctas_op
	)
{
	IntoClause *pintocl = MakeNode(IntoClause);
	pintocl->rel = MakeNode(RangeVar);
	/* GPDB_91_MERGE_FIXME: what about unlogged? */
	pintocl->rel->relpersistence = dxl_phy_ctas_op->IsTemporary() ? RELPERSISTENCE_TEMP : RELPERSISTENCE_PERMANENT;
	pintocl->rel->relname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_phy_ctas_op->MdName()->GetMDName()->GetBuffer());
	pintocl->rel->schemaname = NULL;
	if (NULL != dxl_phy_ctas_op->GetMdNameSchema())
	{
		pintocl->rel->schemaname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_phy_ctas_op->GetMdNameSchema()->GetMDName()->GetBuffer());
	}
	
	CDXLCtasStorageOptions *pdxlctasopt = dxl_phy_ctas_op->GetDxlCtasStorageOption();
	if (NULL != pdxlctasopt->GetMdNameTableSpace())
	{
		pintocl->tableSpaceName = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_phy_ctas_op->GetDxlCtasStorageOption()->GetMdNameTableSpace()->GetMDName()->GetBuffer());
	}
	
	pintocl->onCommit = (OnCommitAction) pdxlctasopt->GetOnCommitAction();
	pintocl->options = TranslateDXLCtasStorageOptions(pdxlctasopt->GetDXLCtasOptionArray());
	
	// get column names
	ColumnDescrDXLArray *pdrgpdxlcd = dxl_phy_ctas_op->GetColumnDescrDXLArray();
	const ULONG ulCols = pdrgpdxlcd->Size();
	pintocl->colNames = NIL;
	for (ULONG ul = 0; ul < ulCols; ++ul)
	{
		const CDXLColDescr *dxl_col_descr = (*pdrgpdxlcd)[ul];

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_col_descr->MdName()->GetMDName()->GetBuffer());
		
		ColumnDef *pcoldef = MakeNode(ColumnDef);
		pcoldef->colname = col_name_char_array;
		pcoldef->is_local = true;

		// GDPB_91_MERGE_FIXME: collation
		pcoldef->collClause = NULL;
		pcoldef->collOid = gpdb::TypeCollation(CMDIdGPDB::CastMdid(dxl_col_descr->MDIdType())->OidObjectId());
		pintocl->colNames = gpdb::LAppend(pintocl->colNames, pcoldef);

	}

	return pintocl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLPhyCtasToDistrPolicy
//
//	@doc:
//		Translates distribution policy given by a physical CTAS operator 
//
//---------------------------------------------------------------------------
GpPolicy *
CTranslatorDXLToPlStmt::TranslateDXLPhyCtasToDistrPolicy
	(
	const CDXLPhysicalCTAS *dxlop
	)
{
	ULongPtrArray *pdrgpulDistrCols = dxlop->GetDistrColPosArray();

	const ULONG ulDistrCols = (pdrgpulDistrCols == NULL) ? 0 : pdrgpulDistrCols->Size();

	ULONG ulDistrColsAlloc = 1;
	if (0 < ulDistrCols)
	{
		ulDistrColsAlloc = ulDistrCols;
	}
	
	GpPolicy *pdistrpolicy = gpdb::MakeGpPolicy(NULL, POLICYTYPE_PARTITIONED, ulDistrColsAlloc);

	GPOS_ASSERT(IMDRelation::EreldistrHash == dxlop->Ereldistrpolicy() ||
				IMDRelation::EreldistrRandom == dxlop->Ereldistrpolicy());
	
	pdistrpolicy->ptype = POLICYTYPE_PARTITIONED;
	pdistrpolicy->nattrs = 0;
	if (IMDRelation::EreldistrHash == dxlop->Ereldistrpolicy())
	{
		
		GPOS_ASSERT(0 < ulDistrCols);
		pdistrpolicy->nattrs = ulDistrCols;
		
		for (ULONG ul = 0; ul < ulDistrCols; ul++)
		{
			ULONG ulColPos = *((*pdrgpulDistrCols)[ul]);
			pdistrpolicy->attrs[ul] = ulColPos + 1;
		}
	}
	return pdistrpolicy;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLCtasStorageOptions
//
//	@doc:
//		Translates CTAS options
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLCtasStorageOptions
	(
	CDXLCtasStorageOptions::DXLCtasOptionArray *ctas_storage_option_array
	)
{
	if (NULL == ctas_storage_option_array)
	{
		return NIL;
	}
	
	const ULONG ulOptions = ctas_storage_option_array->Size();
	List *options = NIL;
	for (ULONG ul = 0; ul < ulOptions; ul++)
	{
		CDXLCtasStorageOptions::CDXLCtasOption *pdxlopt = (*ctas_storage_option_array)[ul];
		CWStringBase *pstrName = pdxlopt->m_str_name;
		CWStringBase *pstrValue = pdxlopt->m_str_value;
		DefElem *def_elem = MakeNode(DefElem);
		def_elem->defname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pstrName->GetBuffer());

		if (!pdxlopt->m_is_null)
		{
			NodeTag argType = (NodeTag) pdxlopt->m_type;

			GPOS_ASSERT(T_Integer == argType || T_String == argType);
			if (T_Integer == argType)
			{
				def_elem->arg = (Node *) gpdb::MakeIntegerValue(CTranslatorUtils::GetLongFromStr(pstrValue));
			}
			else
			{
				def_elem->arg = (Node *) gpdb::MakeStringValue(CTranslatorUtils::CreateMultiByteCharStringFromWCString(pstrValue->GetBuffer()));
			}
		}

		options = gpdb::LAppend(options, def_elem);
	}
	
	return options;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLBitmapTblScan
//
//	@doc:
//		Translates a DXL bitmap table scan node into a BitmapTableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLBitmapTblScan
	(
	const CDXLNode *bitmapscan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	ULONG ulPartIndex = INVALID_PART_INDEX;
	ULONG ulPartIndexPrintable = INVALID_PART_INDEX;
	const CDXLTableDescr *table_descr = NULL;
	BOOL fDynamic = false;

	CDXLOperator *dxl_operator = bitmapscan_dxlnode->GetOperator();
	if (EdxlopPhysicalBitmapTableScan == dxl_operator->GetDXLOperator())
	{
		table_descr = CDXLPhysicalBitmapTableScan::Cast(dxl_operator)->GetDXLTableDescr();
	}
	else
	{
		GPOS_ASSERT(EdxlopPhysicalDynamicBitmapTableScan == dxl_operator->GetDXLOperator());
		CDXLPhysicalDynamicBitmapTableScan *pdxlopDynamic =
				CDXLPhysicalDynamicBitmapTableScan::Cast(dxl_operator);
		table_descr = pdxlopDynamic->GetDXLTableDescr();

		ulPartIndex = pdxlopDynamic->GetPartIndexId();
		ulPartIndexPrintable = pdxlopDynamic->GetPartIndexIdPrintable();
		fDynamic = true;
	}

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(table_descr, NULL /*index_descr_dxl*/, index, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;

	m_dxl_to_plstmt_context->AddRTE(rte);

	BitmapTableScan *bitmap_tbl_scan = MakeNode(BitmapTableScan);
	bitmap_tbl_scan->scan.scanrelid = index;
	bitmap_tbl_scan->scan.partIndex = ulPartIndex;
	bitmap_tbl_scan->scan.partIndexPrintable = ulPartIndexPrintable;

	Plan *plan = &(bitmap_tbl_scan->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(bitmapscan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	GPOS_ASSERT(4 == bitmapscan_dxlnode->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*bitmapscan_dxlnode)[0];
	CDXLNode *filter_dxlnode = (*bitmapscan_dxlnode)[1];
	CDXLNode *pdxlnRecheckCond = (*bitmapscan_dxlnode)[2];
	CDXLNode *bitmap_access_path_dxlnode = (*bitmapscan_dxlnode)[3];

	List *plQuals = NULL;
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		ctxt_translation_prev_siblings,
		&plan->targetlist,
		&plQuals,
		output_context
		);
	plan->qual = plQuals;

	bitmap_tbl_scan->bitmapqualorig = TranslateDXLFilterToQual
							(
							pdxlnRecheckCond,
							&dxltrctxbt,
							ctxt_translation_prev_siblings,
							output_context
							);

	bitmap_tbl_scan->scan.plan.lefttree = TranslateDXLBitmapAccessPath
								(
								bitmap_access_path_dxlnode,
								output_context,
								md_rel,
								table_descr,
								&dxltrctxbt,
								ctxt_translation_prev_siblings,
								bitmap_tbl_scan
								);
	SetParamIds(plan);

	return (Plan *) bitmap_tbl_scan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLBitmapAccessPath
//
//	@doc:
//		Translate the tree of bitmap index operators that are under the given
//		(dynamic) bitmap table scan.
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLBitmapAccessPath
	(
	const CDXLNode *bitmap_access_path_dxlnode,
	CDXLTranslateContext *output_context,
	const IMDRelation *md_rel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	Edxlopid edxlopid = bitmap_access_path_dxlnode->GetOperator()->GetDXLOperator();
	if (EdxlopScalarBitmapIndexProbe == edxlopid)
	{
		return TranslateDXLBitmapIndexProbe
				(
				bitmap_access_path_dxlnode,
				output_context,
				md_rel,
				table_descr,
				base_table_context,
				ctxt_translation_prev_siblings,
				bitmap_tbl_scan
				);
	}
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == edxlopid);

	return TranslateDXLBitmapBoolOp
			(
			bitmap_access_path_dxlnode, 
			output_context, 
			md_rel, 
			table_descr,
			base_table_context,
			ctxt_translation_prev_siblings,
			bitmap_tbl_scan
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::TranslateDXLBitmapBoolOp
//
//	@doc:
//		Translates a DML bitmap bool op expression 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLBitmapBoolOp
	(
	const CDXLNode *bitmap_boolop_dxlnode,
	CDXLTranslateContext *output_context,
	const IMDRelation *md_rel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	GPOS_ASSERT(NULL != bitmap_boolop_dxlnode);
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == bitmap_boolop_dxlnode->GetOperator()->GetDXLOperator());

	CDXLScalarBitmapBoolOp *dxl_sc_bitmap_boolop_op = CDXLScalarBitmapBoolOp::Cast(bitmap_boolop_dxlnode->GetOperator());
	
	CDXLNode *pdxlnLeft = (*bitmap_boolop_dxlnode)[0];
	CDXLNode *pdxlnRight = (*bitmap_boolop_dxlnode)[1];
	
	Plan *pplanLeft = TranslateDXLBitmapAccessPath
						(
						pdxlnLeft,
						output_context,
						md_rel,
						table_descr,
						base_table_context,
						ctxt_translation_prev_siblings,
						bitmap_tbl_scan
						);
	Plan *pplanRight = TranslateDXLBitmapAccessPath
						(
						pdxlnRight,
						output_context,
						md_rel,
						table_descr,
						base_table_context,
						ctxt_translation_prev_siblings,
						bitmap_tbl_scan
						);
	List *plChildPlans = ListMake2(pplanLeft, pplanRight);

	Plan *plan = NULL;
	
	if (CDXLScalarBitmapBoolOp::EdxlbitmapAnd == dxl_sc_bitmap_boolop_op->GetDXLBitmapOpType())
	{
		BitmapAnd *bitmapand = MakeNode(BitmapAnd);
		bitmapand->bitmapplans = plChildPlans;
		bitmapand->plan.targetlist = NULL;
		bitmapand->plan.qual = NULL;
		plan = (Plan *) bitmapand;
	}
	else
	{
		BitmapOr *bitmapor = MakeNode(BitmapOr);
		bitmapor->bitmapplans = plChildPlans;
		bitmapor->plan.targetlist = NULL;
		bitmapor->plan.qual = NULL;
		plan = (Plan *) bitmapor;
	}
	
	
	return plan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLBitmapIndexProbe
//
//	@doc:
//		Translate CDXLScalarBitmapIndexProbe into a BitmapIndexScan
//		or a DynamicBitmapIndexScan
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLBitmapIndexProbe
	(
	const CDXLNode *bitmap_index_probe_dxlnode,
	CDXLTranslateContext *output_context,
	const IMDRelation *md_rel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *base_table_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	CDXLScalarBitmapIndexProbe *dxl_sc_bitmap_idx_probe_op =
			CDXLScalarBitmapIndexProbe::Cast(bitmap_index_probe_dxlnode->GetOperator());

	BitmapIndexScan *pbis;
	DynamicBitmapIndexScan *pdbis;

	if (bitmap_tbl_scan->scan.partIndex)
	{
		/* It's a Dynamic Bitmap Index Scan */
		pdbis = MakeNode(DynamicBitmapIndexScan);
		pbis = &(pdbis->biscan);
	}
	else
	{
		pdbis = NULL;
		pbis = MakeNode(BitmapIndexScan);
	}
	pbis->scan.scanrelid = bitmap_tbl_scan->scan.scanrelid;
	pbis->scan.partIndex = bitmap_tbl_scan->scan.partIndex;

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(dxl_sc_bitmap_idx_probe_op->GetDXLIndexDescr()->MDId());
	const IMDIndex *index = m_md_accessor->RetrieveIndex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pbis->indexid = oidIndex;
	OID oidRel = CMDIdGPDB::CastMdid(table_descr->MDId())->OidObjectId();
	Plan *plan = &(pbis->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	GPOS_ASSERT(1 == bitmap_index_probe_dxlnode->Arity());
	CDXLNode *index_cond_list_dxlnode = (*bitmap_index_probe_dxlnode)[0];
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		index_cond_list_dxlnode,
		table_descr,
		false /*is_index_only_scan*/,
		index,
		md_rel,
		output_context,
		base_table_context,
		ctxt_translation_prev_siblings,
		&plIndexConditions,
		&plIndexOrigConditions,
		&plIndexStratgey,
		&plIndexSubtype
		);

	pbis->indexqual = plIndexConditions;
	pbis->indexqualorig = plIndexOrigConditions;
	/*
	 * As of 8.4, the indexstrategy and indexsubtype fields are no longer
	 * available or needed in IndexScan. Ignore them.
	 */
	SetParamIds(plan);

	/*
	 * If it's a Dynamic Bitmap Index Scan, also fill in the information
	 * about the indexes on the partitions.
	 */
	if (pdbis)
	{
		pdbis->logicalIndexInfo = gpdb::GetLogicalIndexInfo(oidRel, oidIndex);
	}

	return plan;
}

// translates a DXL Value Scan node into a GPDB Value scan node
Plan *
CTranslatorDXLToPlStmt::TranslateDXLValueScan
	(
	const CDXLNode *value_scan_dxlnode,
	CDXLTranslateContext *output_context,
	DXLTranslationContextArr *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	dxltrctxbt.SetRelIndex(index);

	// create value scan node
	ValuesScan *pvaluescan = MakeNode(ValuesScan);
	pvaluescan->scan.scanrelid = index;
	Plan *plan = &(pvaluescan->scan.plan);

	RangeTblEntry *rte = TranslateDXLValueScanToRangeTblEntry(value_scan_dxlnode, output_context, &dxltrctxbt);
	GPOS_ASSERT(NULL != rte);

	pvaluescan->values_lists = (List *)gpdb::CopyObject(rte->values_lists);

	m_dxl_to_plstmt_context->AddRTE(rte);

	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
	(
		CDXLPhysicalProperties::PdxlpropConvert(value_scan_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
	);

	// a table scan node must have at least 2 children: projection list and at least 1 value list
	GPOS_ASSERT(2 <= value_scan_dxlnode->Arity());

	CDXLNode *project_list_dxl = (*value_scan_dxlnode)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = TranslateDXLProjList
							(
							project_list_dxl,
							&dxltrctxbt,
							NULL,
							output_context
							);

	plan->targetlist = target_list;

	return (Plan *) pvaluescan;
}


// EOF
