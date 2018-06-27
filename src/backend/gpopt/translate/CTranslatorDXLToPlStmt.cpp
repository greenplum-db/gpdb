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
	CContextDXLToPlStmt* pctxdxltoplstmt,
	ULONG ulSegments
	)
	:
	m_memory_pool(memory_pool),
	m_pmda(md_accessor),
	m_pctxdxltoplstmt(pctxdxltoplstmt),
	m_cmdtype(CMD_SELECT),
	m_fTargetTableDistributed(false),
	m_plResultRelations(NULL),
	m_ulExternalScanCounter(0),
	m_num_of_segments(ulSegments),
	m_ulPartitionSelectorCounter(0)
{
	m_pdxlsctranslator = GPOS_NEW(m_memory_pool) CTranslatorDXLToScalar(m_memory_pool, m_pmda, m_num_of_segments);
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
	GPOS_DELETE(m_pdxlsctranslator);
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
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_rgpfTranslators); ul++)
	{
		m_rgpfTranslators[ul] = NULL;
	}

	// array mapping operator type to translator function
	static const STranslatorMapping rgTranslators[] =
	{
			{EdxlopPhysicalTableScan,				&gpopt::CTranslatorDXLToPlStmt::PtsFromDXLTblScan},
			{EdxlopPhysicalExternalScan,			&gpopt::CTranslatorDXLToPlStmt::PtsFromDXLTblScan},
			{EdxlopPhysicalIndexScan,				&gpopt::CTranslatorDXLToPlStmt::PisFromDXLIndexScan},
			{EdxlopPhysicalHashJoin, 				&gpopt::CTranslatorDXLToPlStmt::PhjFromDXLHJ},
			{EdxlopPhysicalNLJoin, 					&gpopt::CTranslatorDXLToPlStmt::PnljFromDXLNLJ},
			{EdxlopPhysicalMergeJoin,				&gpopt::CTranslatorDXLToPlStmt::PmjFromDXLMJ},
			{EdxlopPhysicalMotionGather,			&gpopt::CTranslatorDXLToPlStmt::PplanMotionFromDXLMotion},
			{EdxlopPhysicalMotionBroadcast,			&gpopt::CTranslatorDXLToPlStmt::PplanMotionFromDXLMotion},
			{EdxlopPhysicalMotionRedistribute,		&gpopt::CTranslatorDXLToPlStmt::PplanTranslateDXLMotion},
			{EdxlopPhysicalMotionRandom,			&gpopt::CTranslatorDXLToPlStmt::PplanTranslateDXLMotion},
			{EdxlopPhysicalMotionRoutedDistribute,	&gpopt::CTranslatorDXLToPlStmt::PplanMotionFromDXLMotion},
			{EdxlopPhysicalLimit, 					&gpopt::CTranslatorDXLToPlStmt::PlimitFromDXLLimit},
			{EdxlopPhysicalAgg, 					&gpopt::CTranslatorDXLToPlStmt::PaggFromDXLAgg},
			{EdxlopPhysicalWindow, 					&gpopt::CTranslatorDXLToPlStmt::PwindowFromDXLWindow},
			{EdxlopPhysicalSort,					&gpopt::CTranslatorDXLToPlStmt::PsortFromDXLSort},
			{EdxlopPhysicalSubqueryScan,			&gpopt::CTranslatorDXLToPlStmt::PsubqscanFromDXLSubqScan},
			{EdxlopPhysicalResult, 					&gpopt::CTranslatorDXLToPlStmt::PresultFromDXLResult},
			{EdxlopPhysicalAppend, 					&gpopt::CTranslatorDXLToPlStmt::PappendFromDXLAppend},
			{EdxlopPhysicalMaterialize, 			&gpopt::CTranslatorDXLToPlStmt::PmatFromDXLMaterialize},
			{EdxlopPhysicalSequence, 				&gpopt::CTranslatorDXLToPlStmt::PplanSequence},
			{EdxlopPhysicalDynamicTableScan,		&gpopt::CTranslatorDXLToPlStmt::PplanDTS},
			{EdxlopPhysicalDynamicIndexScan,		&gpopt::CTranslatorDXLToPlStmt::PplanDIS},
			{EdxlopPhysicalTVF,						&gpopt::CTranslatorDXLToPlStmt::PplanFunctionScanFromDXLTVF},
			{EdxlopPhysicalDML,						&gpopt::CTranslatorDXLToPlStmt::PplanDML},
			{EdxlopPhysicalSplit,					&gpopt::CTranslatorDXLToPlStmt::PplanSplit},
			{EdxlopPhysicalRowTrigger,				&gpopt::CTranslatorDXLToPlStmt::PplanRowTrigger},
			{EdxlopPhysicalAssert,					&gpopt::CTranslatorDXLToPlStmt::PplanAssert},
			{EdxlopPhysicalCTEProducer, 			&gpopt::CTranslatorDXLToPlStmt::PshscanFromDXLCTEProducer},
			{EdxlopPhysicalCTEConsumer, 			&gpopt::CTranslatorDXLToPlStmt::PshscanFromDXLCTEConsumer},
			{EdxlopPhysicalBitmapTableScan,			&gpopt::CTranslatorDXLToPlStmt::PplanBitmapTableScan},
			{EdxlopPhysicalDynamicBitmapTableScan,	&gpopt::CTranslatorDXLToPlStmt::PplanBitmapTableScan},
			{EdxlopPhysicalCTAS, 					&gpopt::CTranslatorDXLToPlStmt::PplanCTAS},
			{EdxlopPhysicalPartitionSelector,		&gpopt::CTranslatorDXLToPlStmt::PplanPartitionSelector},
			{EdxlopPhysicalValuesScan,				&gpopt::CTranslatorDXLToPlStmt::PplanValueScan},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);

	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		STranslatorMapping elem = rgTranslators[ul];
		m_rgpfTranslators[elem.edxlopid] = elem.pf;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplstmtFromDXL
//
//	@doc:
//		Translate DXL node into a PlannedStmt
//
//---------------------------------------------------------------------------
PlannedStmt *
CTranslatorDXLToPlStmt::PplstmtFromDXL
	(
	const CDXLNode *dxlnode,
	bool canSetTag
	)
{
	GPOS_ASSERT(NULL != dxlnode);

	CDXLTranslateContext dxltrctx(m_memory_pool, false);

	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	Plan *pplan = PplFromDXL(dxlnode, &dxltrctx, pdrgpdxltrctxPrevSiblings);
	pdrgpdxltrctxPrevSiblings->Release();

	GPOS_ASSERT(NULL != pplan);

	// collect oids from rtable
	List *plOids = NIL;

	ListCell *plcRTE = NULL;
	ForEach (plcRTE, m_pctxdxltoplstmt->GetRTableEntriesList())
	{
		RangeTblEntry *pRTE = (RangeTblEntry *) lfirst(plcRTE);

		if (pRTE->rtekind == RTE_RELATION)
		{
			plOids = gpdb::PlAppendOid(plOids, pRTE->relid);
		}
	}

	// assemble planned stmt
	PlannedStmt *pplstmt = MakeNode(PlannedStmt);
	pplstmt->planGen = PLANGEN_OPTIMIZER;
	
	pplstmt->rtable = m_pctxdxltoplstmt->GetRTableEntriesList();
	pplstmt->subplans = m_pctxdxltoplstmt->GetSubplanEntriesList();
	pplstmt->planTree = pplan;

	// store partitioned table indexes in planned stmt
	pplstmt->queryPartOids = m_pctxdxltoplstmt->GetPartitionedTablesList();
	pplstmt->canSetTag = canSetTag;
	pplstmt->relationOids = plOids;
	pplstmt->numSelectorsPerScanId = m_pctxdxltoplstmt->GetNumPartitionSelectorsList();

	pplan->nMotionNodes  = m_pctxdxltoplstmt->GetCurrentMotionId()-1;
	pplstmt->nMotionNodes =  m_pctxdxltoplstmt->GetCurrentMotionId()-1;

	pplstmt->commandType = m_cmdtype;
	
	GPOS_ASSERT(pplan->nMotionNodes >= 0);
	if (0 == pplan->nMotionNodes && !m_fTargetTableDistributed)
	{
		// no motion nodes and not a DML on a distributed table
		pplan->dispatch = DISPATCH_SEQUENTIAL;
	}
	else
	{
		pplan->dispatch = DISPATCH_PARALLEL;
	}
	
	pplstmt->resultRelations = m_plResultRelations;
	pplstmt->intoClause = m_pctxdxltoplstmt->GetIntoClause();
	pplstmt->intoPolicy = m_pctxdxltoplstmt->GetDistributionPolicy();
	
	SetInitPlanVariables(pplstmt);
	
	if (CMD_SELECT == m_cmdtype && NULL != dxlnode->GetDXLDirectDispatchInfo())
	{
		List *plDirectDispatchSegIds = PlDirectDispatchSegIds(dxlnode->GetDXLDirectDispatchInfo());
		pplan->directDispatch.contentIds = plDirectDispatchSegIds;
		pplan->directDispatch.isDirectDispatch = (NIL != plDirectDispatchSegIds);
		
		if (pplan->directDispatch.isDirectDispatch)
		{
			List *plMotions = gpdb::PlExtractNodesPlan(pplstmt->planTree, T_Motion, true /*descendIntoSubqueries*/);
			ListCell *lc = NULL;
			ForEach(lc, plMotions)
			{
				Motion *pmotion = (Motion *) lfirst(lc);
				GPOS_ASSERT(IsA(pmotion, Motion));
				GPOS_ASSERT(gpdb::FMotionGather(pmotion));
				
				pmotion->plan.directDispatch.isDirectDispatch = true;
				pmotion->plan.directDispatch.contentIds = pplan->directDispatch.contentIds;
			}
		}
	}
	
	return pplstmt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplFromDXL
//
//	@doc:
//		Translates a DXL tree into a Plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplFromDXL
	(
	const CDXLNode *dxlnode,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != pdrgpdxltrctxPrevSiblings);

	CDXLOperator *pdxlop = dxlnode->GetOperator();
	ULONG ulOpId =  (ULONG) pdxlop->GetDXLOperator();

	PfPplan pf = m_rgpfTranslators[ulOpId];

	if (NULL == pf)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	return (this->* pf)(dxlnode, pdxltrctxOut, pdrgpdxltrctxPrevSiblings);
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
	if(1 != m_pctxdxltoplstmt->GetCurrentMotionId()) // For Distributed Tables m_ulMotionId > 1
	{
		pplstmt->nInitPlans = m_pctxdxltoplstmt->GetCurrentParamId();
		pplstmt->planTree->nInitPlans = m_pctxdxltoplstmt->GetCurrentParamId();
	}

	pplstmt->nParamExec = m_pctxdxltoplstmt->GetCurrentParamId();

	// Extract all subplans defined in the planTree
	List *plSubPlans = gpdb::PlExtractNodesPlan(pplstmt->planTree, T_SubPlan, true);

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
		plSubPlans = gpdb::PlExtractNodesPlan((Plan*) lfirst(lc), T_SubPlan, true);
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
		GPOS_ASSERT(0 < m_pctxdxltoplstmt->GetCurrentMotionId());

		if(1 < m_pctxdxltoplstmt->GetCurrentMotionId())
		{
			psubplan->qDispSliceId =  m_pctxdxltoplstmt->GetCurrentMotionId() + psubplan->plan_id-1;
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
CTranslatorDXLToPlStmt::SetParamIds(Plan* pplan)
{
	List *plParams = gpdb::PlExtractNodesPlan(pplan, T_Param, true);

	ListCell *lc = NULL;

	Bitmapset  *pbitmapset = NULL;

	ForEach (lc, plParams)
	{
		Param *pparam = (Param*) lfirst(lc);
		pbitmapset = gpdb::PbmsAddMember(pbitmapset, pparam->paramid);
	}

	pplan->extParam = pbitmapset;
	pplan->allParam = pbitmapset;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PtsFromDXLTblScan
//
//	@doc:
//		Translates a DXL table scan node into a TableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PtsFromDXLTblScan
	(
	const CDXLNode *pdxlnTblScan,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalTableScan *pdxlopTS = CDXLPhysicalTableScan::Cast(pdxlnTblScan->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// we will add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	const CDXLTableDescr *table_descr = pdxlopTS->GetDXLTableDescr();
	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	RangeTblEntry *prte = PrteFromTblDescr(table_descr, NULL /*index_descr_dxl*/, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= ACL_SELECT;
	m_pctxdxltoplstmt->AddRTE(prte);

	Plan *pplan = NULL;
	Plan *pplanReturn = NULL;
	if (IMDRelation::ErelstorageExternal == pmdrel->GetRelStorageType())
	{
		const IMDRelationExternal *pmdrelext = dynamic_cast<const IMDRelationExternal*>(pmdrel);
		OID oidRel = CMDIdGPDB::CastMdid(pmdrel->MDId())->OidObjectId();
		ExtTableEntry *pextentry = gpdb::Pexttable(oidRel);
		bool isMasterOnly;
		
		// create external scan node
		ExternalScan *pes = MakeNode(ExternalScan);
		pes->scan.scanrelid = iRel;
		pes->uriList = gpdb::PlExternalScanUriList(pextentry, &isMasterOnly);
		pes->fmtOptString = pextentry->fmtopts;
		pes->fmtType = pextentry->fmtcode;
		pes->isMasterOnly = isMasterOnly;
		GPOS_ASSERT((IMDRelation::EreldistrMasterOnly == pmdrelext->GetRelDistribution()) == isMasterOnly);
		pes->logErrors = pextentry->logerrors;
		pes->rejLimit = pmdrelext->RejectLimit();
		pes->rejLimitInRows = pmdrelext->IsRejectLimitInRows();

		pes->encoding = pextentry->encoding;
		pes->scancounter = m_ulExternalScanCounter++;

		pplan = &(pes->scan.plan);
		pplanReturn = (Plan *) pes;
	}
	else
	{
		// create table scan node
		TableScan *pts = MakeNode(TableScan);
		pts->scanrelid = iRel;
		pplan = &(pts->plan);
		pplanReturn = (Plan *) pts;
	}

	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnTblScan->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// a table scan node must have 2 children: projection list and filter
	GPOS_ASSERT(2 == pdxlnTblScan->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*pdxlnTblScan)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnTblScan)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		NULL,			// pdxltrctxLeft and pdxltrctxRight,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	SetParamIds(pplan);

	return pplanReturn;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::FSetIndexVarAttno
//
//	@doc:
//		Walker to set index var attno's,
//		attnos of index vars are set to their relative positions in index keys,
//		skip any outer references while walking the expression tree
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToPlStmt::FSetIndexVarAttno
	(
	Node *pnode,
	SContextIndexVarAttno *pctxtidxvarattno
	)
{
	if (NULL == pnode)
	{
		return false;
	}

	if (IsA(pnode, Var) && ((Var *)pnode)->varno != OUTER)
	{
		INT iAttno = ((Var *)pnode)->varattno;
		const IMDRelation *pmdrel = pctxtidxvarattno->m_pmdrel;
		const IMDIndex *index = pctxtidxvarattno->m_pmdindex;

		ULONG ulIndexColPos = gpos::ulong_max;
		const ULONG arity = pmdrel->ColumnCount();
		for (ULONG ulColPos = 0; ulColPos < arity; ulColPos++)
		{
			const IMDColumn *pmdcol = pmdrel->GetMdCol(ulColPos);
			if (iAttno == pmdcol->AttrNum())
			{
				ulIndexColPos = ulColPos;
				break;
			}
		}

		if (gpos::ulong_max > ulIndexColPos)
		{
			((Var *)pnode)->varattno =  1 + index->GetKeyPos(ulIndexColPos);
		}

		return false;
	}

	return gpdb::FWalkExpressionTree
			(
			pnode,
			(BOOL (*)()) CTranslatorDXLToPlStmt::FSetIndexVarAttno,
			pctxtidxvarattno
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PisFromDXLIndexScan
//
//	@doc:
//		Translates a DXL index scan node into a IndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PisFromDXLIndexScan
	(
	const CDXLNode *pdxlnIndexScan,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalIndexScan *pdxlopIndexScan = CDXLPhysicalIndexScan::Cast(pdxlnIndexScan->GetOperator());

	return PisFromDXLIndexScan(pdxlnIndexScan, pdxlopIndexScan, pdxltrctxOut, false /*fIndexOnlyScan*/, pdrgpdxltrctxPrevSiblings);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PisFromDXLIndexScan
//
//	@doc:
//		Translates a DXL index scan node into a IndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PisFromDXLIndexScan
	(
	const CDXLNode *pdxlnIndexScan,
	CDXLPhysicalIndexScan *pdxlopIndexScan,
	CDXLTranslateContext *pdxltrctxOut,
	BOOL fIndexOnlyScan,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	const CDXLIndexDescr *index_descr_dxl = NULL;
	if (fIndexOnlyScan)
	{
		index_descr_dxl = pdxlopIndexScan->GetDXLIndexDescr();
	}

	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxlopIndexScan->GetDXLTableDescr()->MDId());

	RangeTblEntry *prte = PrteFromTblDescr(pdxlopIndexScan->GetDXLTableDescr(), index_descr_dxl, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= ACL_SELECT;
	m_pctxdxltoplstmt->AddRTE(prte);

	IndexScan *pis = NULL;
	GPOS_ASSERT(!fIndexOnlyScan);
	pis = MakeNode(IndexScan);
	pis->scan.scanrelid = iRel;

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(pdxlopIndexScan->GetDXLIndexDescr()->MDId());
	const IMDIndex *index = m_pmda->Pmdindex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pis->indexid = oidIndex;

	Plan *pplan = &(pis->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnIndexScan->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// an index scan node must have 3 children: projection list, filter and index condition list
	GPOS_ASSERT(3 == pdxlnIndexScan->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*pdxlnIndexScan)[EdxlisIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnIndexScan)[EdxlisIndexFilter];
	CDXLNode *pdxlnIndexCondList = (*pdxlnIndexScan)[EdxlisIndexCondition];

	// translate proj list
	pplan->targetlist = PlTargetListFromProjList(project_list_dxl, &dxltrctxbt, NULL /*pdrgpdxltrctx*/, pdxltrctxOut);

	// translate index filter
	pplan->qual = PlTranslateIndexFilter
					(
					filter_dxlnode,
					pdxltrctxOut,
					&dxltrctxbt,
					pdrgpdxltrctxPrevSiblings
					);

	pis->indexorderdir = CTranslatorUtils::GetScanDirection(pdxlopIndexScan->GetIndexScanDir());

	// translate index condition list
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		pdxlnIndexCondList, 
		pdxlopIndexScan->GetDXLTableDescr(),
		fIndexOnlyScan, 
		index, 
		pmdrel,
		pdxltrctxOut,
		&dxltrctxbt, 
		pdrgpdxltrctxPrevSiblings,
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
	SetParamIds(pplan);

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
CTranslatorDXLToPlStmt::PlTranslateIndexFilter
	(
	CDXLNode *filter_dxlnode,
	CDXLTranslateContext *pdxltrctxOut,
	CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	List *plQuals = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, pdxltrctxbt, pdrgpdxltrctxPrevSiblings, pdxltrctxOut, m_pctxdxltoplstmt);

	const ULONG arity = filter_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *index_filter_dxlnode = (*filter_dxlnode)[ul];
		Expr *pexprIndexFilter = m_pdxlsctranslator->PexprFromDXLNodeScalar(index_filter_dxlnode, &mapcidvarplstmt);
		plQuals = gpdb::PlAppendElement(plQuals, pexprIndexFilter);
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
	CDXLNode *pdxlnIndexCondList,
	const CDXLTableDescr *pdxltd,
	BOOL fIndexOnlyScan,
	const IMDIndex *index,
	const IMDRelation *pmdrel,
	CDXLTranslateContext *pdxltrctxOut,
	CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
	List **pplIndexConditions,
	List **pplIndexOrigConditions,
	List **pplIndexStratgey,
	List **pplIndexSubtype
	)
{
	// array of index qual info
	IndexQualInfoArray *pdrgpindexqualinfo = GPOS_NEW(m_memory_pool) IndexQualInfoArray(m_memory_pool);

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, pdxltrctxbt, pdrgpdxltrctxPrevSiblings, pdxltrctxOut, m_pctxdxltoplstmt);

	const ULONG arity = pdxlnIndexCondList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnIndexCond = (*pdxlnIndexCondList)[ul];

		Expr *pexprOrigIndexCond = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnIndexCond, &mapcidvarplstmt);
		Expr *pexprIndexCond = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnIndexCond, &mapcidvarplstmt);
		GPOS_ASSERT((IsA(pexprIndexCond, OpExpr) || IsA(pexprIndexCond, ScalarArrayOpExpr))
				&& "expected OpExpr or ScalarArrayOpExpr in index qual");

		if (IsA(pexprIndexCond, ScalarArrayOpExpr) && IMDIndex::EmdindBitmap != index->IndexType())
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, GPOS_WSZ_LIT("ScalarArrayOpExpr condition on index scan"));
		}

		// for indexonlyscan, we already have the attno referring to the index
		if (!fIndexOnlyScan)
		{
			// Otherwise, we need to perform mapping of Varattnos relative to column positions in index keys
			SContextIndexVarAttno ctxtidxvarattno(pmdrel, index);
			FSetIndexVarAttno((Node *) pexprIndexCond, &ctxtidxvarattno);
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

		Node *pnodeFst = (Node *) lfirst(gpdb::PlcListHead(plistArgs));
		Node *pnodeSnd = (Node *) lfirst(gpdb::PlcListTail(plistArgs));
				
		BOOL fRelabel = false;
		if (IsA(pnodeFst, RelabelType) && IsA(((RelabelType *) pnodeFst)->arg, Var))
		{
			pnodeFst = (Node *) ((RelabelType *) pnodeFst)->arg;
			fRelabel = true;
		}
		else if (IsA(pnodeSnd, RelabelType) && IsA(((RelabelType *) pnodeSnd)->arg, Var))
		{
			pnodeSnd = (Node *) ((RelabelType *) pnodeSnd)->arg;
			fRelabel = true;
		}
		
		if (fRelabel)
		{
			List *plNewArgs = ListMake2(pnodeFst, pnodeSnd);
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
		
		GPOS_ASSERT((IsA(pnodeFst, Var) || IsA(pnodeSnd, Var)) && "expected index key in index qual");

		INT iAttno = 0;
		if (IsA(pnodeFst, Var) && ((Var *) pnodeFst)->varno != OUTER)
		{
			// index key is on the left side
			iAttno =  ((Var *) pnodeFst)->varattno;
		}
		else
		{
			// index key is on the right side
			GPOS_ASSERT(((Var *) pnodeSnd)->varno != OUTER && "unexpected outer reference in index qual");
			iAttno = ((Var *) pnodeSnd)->varattno;
		}
		
		// retrieve index strategy and subtype
		INT iSN = 0;
		OID oidIndexSubtype = InvalidOid;
		
		OID oidCmpOperator = CTranslatorUtils::OidCmpOperator(pexprIndexCond);
		GPOS_ASSERT(InvalidOid != oidCmpOperator);
		OID oidOpFamily = CTranslatorUtils::GetOpFamilyForIndexQual(iAttno, CMDIdGPDB::CastMdid(index->MDId())->OidObjectId());
		GPOS_ASSERT(InvalidOid != oidOpFamily);
		gpdb::IndexOpProperties(oidCmpOperator, oidOpFamily, &iSN, &oidIndexSubtype);
		
		// create index qual
		pdrgpindexqualinfo->Append(GPOS_NEW(m_memory_pool) CIndexQualInfo(iAttno, pexprIndexCond, pexprOrigIndexCond, (StrategyNumber) iSN, oidIndexSubtype));
	}

	// the index quals much be ordered by attribute number
	pdrgpindexqualinfo->Sort(CIndexQualInfo::IndexQualInfoCmp);

	ULONG ulLen = pdrgpindexqualinfo->Size();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		CIndexQualInfo *pindexqualinfo = (*pdrgpindexqualinfo)[ul];
		*pplIndexConditions = gpdb::PlAppendElement(*pplIndexConditions, pindexqualinfo->m_expr);
		*pplIndexOrigConditions = gpdb::PlAppendElement(*pplIndexOrigConditions, pindexqualinfo->m_original_expr);
		*pplIndexStratgey = gpdb::PlAppendInt(*pplIndexStratgey, pindexqualinfo->m_index_subtype_oid);
		*pplIndexSubtype = gpdb::PlAppendOid(*pplIndexSubtype, pindexqualinfo->m_index_subtype_oid);
	}

	// clean up
	pdrgpindexqualinfo->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlTranslateAssertConstraints
//
//	@doc:
//		Translate the constraints from an Assert node into a list of quals
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlTranslateAssertConstraints
	(
	CDXLNode *pdxlnAssertConstraintList,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctx
	)
{
	List *plQuals = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, NULL /*pdxltrctxbt*/, pdrgpdxltrctx, pdxltrctxOut, m_pctxdxltoplstmt);

	const ULONG arity = pdxlnAssertConstraintList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnpdxlnAssertConstraint = (*pdxlnAssertConstraintList)[ul];
		Expr *pexprAssertConstraint = m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnpdxlnAssertConstraint)[0], &mapcidvarplstmt);
		plQuals = gpdb::PlAppendElement(plQuals, pexprAssertConstraint);
	}

	return plQuals;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlimitFromDXLLimit
//
//	@doc:
//		Translates a DXL Limit node into a Limit node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PlimitFromDXLLimit
	(
	const CDXLNode *pdxlnLimit,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create limit node
	Limit *plimit = MakeNode(Limit);

	Plan *pplan = &(plimit->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnLimit->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	GPOS_ASSERT(4 == pdxlnLimit->Arity());

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	// translate proj list
	CDXLNode *project_list_dxl = (*pdxlnLimit)[EdxllimitIndexProjList];
	CDXLNode *pdxlnChildPlan = (*pdxlnLimit)[EdxllimitIndexChildPlan];
	CDXLNode *pdxlnLimitCount = (*pdxlnLimit)[EdxllimitIndexLimitCount];
	CDXLNode *pdxlnLimitOffset = (*pdxlnLimit)[EdxllimitIndexLimitOffset];

	// NOTE: Limit node has only the left plan while the right plan is left empty
	Plan *pplanLeft = PplFromDXL(pdxlnChildPlan, &dxltrctxLeft, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxLeft);

	pplan->targetlist = PlTargetListFromProjList
								(
								project_list_dxl,
								NULL,		// base table translation context
								pdrgpdxltrctx,
								pdxltrctxOut
								);

	pplan->lefttree = pplanLeft;

	if(NULL != pdxlnLimitCount && pdxlnLimitCount->Arity() >0)
	{
		CMappingColIdVarPlStmt mapcidvarplstmt(m_memory_pool, NULL, pdrgpdxltrctx, pdxltrctxOut, m_pctxdxltoplstmt);
		Node *pnodeLimitCount = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnLimitCount)[0], &mapcidvarplstmt);
		plimit->limitCount = pnodeLimitCount;
	}

	if(NULL != pdxlnLimitOffset && pdxlnLimitOffset->Arity() >0)
	{
		CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, NULL, pdrgpdxltrctx, pdxltrctxOut, m_pctxdxltoplstmt);
		Node *pexprLimitOffset = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnLimitOffset)[0], &mapcidvarplstmt);
		plimit->limitOffset = pexprLimitOffset;
	}

	pplan->nMotionNodes = pplanLeft->nMotionNodes;
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

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
	const CDXLNode *pdxlnHJ,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	GPOS_ASSERT(pdxlnHJ->GetOperator()->GetDXLOperator() == EdxlopPhysicalHashJoin);
	GPOS_ASSERT(pdxlnHJ->Arity() == EdxlhjIndexSentinel);

	// create hash join node
	HashJoin *phj = MakeNode(HashJoin);

	Join *pj = &(phj->join);
	Plan *pplan = &(pj->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalHashJoin *pdxlopHashJoin = CDXLPhysicalHashJoin::Cast(pdxlnHJ->GetOperator());

	// set join type
	pj->jointype = JtFromEdxljt(pdxlopHashJoin->GetJoinType());
	pj->prefetch_inner = true;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnHJ->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*pdxlnHJ)[EdxlhjIndexHashLeft];
	CDXLNode *pdxlnRight = (*pdxlnHJ)[EdxlhjIndexHashRight];
	CDXLNode *project_list_dxl = (*pdxlnHJ)[EdxlhjIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnHJ)[EdxlhjIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*pdxlnHJ)[EdxlhjIndexJoinFilter];
	CDXLNode *pdxlnHashCondList = (*pdxlnHJ)[EdxlhjIndexHashCondList];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanLeft = PplFromDXL(pdxlnLeft, &dxltrctxLeft, pdrgpdxltrctxPrevSiblings);

	// the right side of the join is the one where the hash phase is done
	DrgPdxltrctx *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
	pdrgpdxltrctxWithSiblings->AppendArray(pdrgpdxltrctxPrevSiblings);
	Plan *pplanRight = (Plan*) PhhashFromDXL(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxLeft));
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxRight));
	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	// translate join filter
	pj->joinqual = PlQualFromFilter
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					pdrgpdxltrctx,
					pdxltrctxOut
					);

	// translate hash cond
	List *plHashConditions = NIL;

	BOOL fHasINDFCond = false;

	const ULONG arity = pdxlnHashCondList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnHashCond = (*pdxlnHashCondList)[ul];

		List *plHashCond = PlQualFromScalarCondNode
				(
				pdxlnHashCond,
				NULL,			// base table translation context
				pdrgpdxltrctx,
				pdxltrctxOut
				);

		GPOS_ASSERT(1 == gpdb::ListLength(plHashCond));

		Expr *pexpr = (Expr *) LInitial(plHashCond);
		if (IsA(pexpr, BoolExpr) && ((BoolExpr *) pexpr)->boolop == NOT_EXPR)
		{
			// INDF test
			GPOS_ASSERT(gpdb::ListLength(((BoolExpr *) pexpr)->args) == 1 &&
						(IsA((Expr *) LInitial(((BoolExpr *) pexpr)->args), DistinctExpr)));
			fHasINDFCond = true;
		}
		plHashConditions = gpdb::PlConcat(plHashConditions, plHashCond);
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
														pdrgpdxltrctx,
														pdxltrctxOut,
														m_pctxdxltoplstmt
														);

			// translate the DXL scalar or scalar distinct comparison into an equality comparison
			// to store in the hash clauses
			Expr *pexpr2 = (Expr *) m_pdxlsctranslator->PopexprFromDXLNodeScCmp
									(
									pdxlnHashCond,
									&mapcidvarplstmt
									);

			plHashClauses = gpdb::PlAppendElement(plHashClauses, pexpr2);
		}

		phj->hashclauses = plHashClauses;
		phj->hashqualclauses = plHashConditions;
	}

	GPOS_ASSERT(NIL != phj->hashclauses);

	pplan->lefttree = pplanLeft;
	pplan->righttree = pplanRight;
	pplan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	pdrgpdxltrctx->Release();

	return  (Plan *) phj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanFunctionScanFromDXLTVF
//
//	@doc:
//		Translates a DXL TVF node into a GPDB Function scan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanFunctionScanFromDXLTVF
	(
	const CDXLNode *pdxlnTVF,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// create function scan node
	FunctionScan *pfuncscan = MakeNode(FunctionScan);
	Plan *pplan = &(pfuncscan->scan.plan);

	RangeTblEntry *prte = PrteFromDXLTVF(pdxlnTVF, pdxltrctxOut, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);

	pfuncscan->funcexpr = prte->funcexpr;
	pfuncscan->funccolnames = prte->eref->colnames;

	// we will add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;
	dxltrctxbt.SetRelIndex(iRel);
	pfuncscan->scan.scanrelid = iRel;

	m_pctxdxltoplstmt->AddRTE(prte);

	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnTVF->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// a table scan node must have at least 1 child: projection list
	GPOS_ASSERT(1 <= pdxlnTVF->Arity());

	CDXLNode *project_list_dxl = (*pdxlnTVF)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = PlTargetListFromProjList
						(
						project_list_dxl,
						&dxltrctxbt,
						NULL,
						pdxltrctxOut
						);

	pplan->targetlist = target_list;

	ListCell *plcTe = NULL;

	ForEach (plcTe, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTe);
		OID oidType = gpdb::OidExprType((Node*) target_entry->expr);
		GPOS_ASSERT(InvalidOid != oidType);

		INT typMod = gpdb::IExprTypeMod((Node*) target_entry->expr);
		Oid typCollation = gpdb::OidTypeCollation(oidType);

		pfuncscan->funccoltypes = gpdb::PlAppendOid(pfuncscan->funccoltypes, oidType);
		pfuncscan->funccoltypmods = gpdb::PlAppendInt(pfuncscan->funccoltypmods, typMod);
		// GDPB_91_MERGE_FIXME: collation
		pfuncscan->funccolcollations = gpdb::PlAppendOid(pfuncscan->funccolcollations, typCollation);
	}

	SetParamIds(pplan);

	return (Plan *) pfuncscan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PrteFromDXLTVF
//
//	@doc:
//		Create a range table entry from a CDXLPhysicalTVF node
//
//---------------------------------------------------------------------------
RangeTblEntry *
CTranslatorDXLToPlStmt::PrteFromDXLTVF
	(
	const CDXLNode *pdxlnTVF,
	CDXLTranslateContext *pdxltrctxOut,
	CDXLTranslateContextBaseTable *pdxltrctxbt
	)
{
	CDXLPhysicalTVF *pdxlop = CDXLPhysicalTVF::Cast(pdxlnTVF->GetOperator());

	RangeTblEntry *prte = MakeNode(RangeTblEntry);
	prte->rtekind = RTE_FUNCTION;

	FuncExpr *pfuncexpr = MakeNode(FuncExpr);

	pfuncexpr->funcid = CMDIdGPDB::CastMdid(pdxlop->FuncMdId())->OidObjectId();
	pfuncexpr->funcretset = true;
	// this is a function call, as opposed to a cast
	pfuncexpr->funcformat = COERCE_EXPLICIT_CALL;
	pfuncexpr->funcresulttype = CMDIdGPDB::CastMdid(pdxlop->ReturnTypeMdId())->OidObjectId();

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get function alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlop->Pstr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxl = (*pdxlnTVF)[EdxltsIndexProjList];

	// get column names
	const ULONG ulCols = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		CDXLNode *pdxlnPrElem = (*project_list_dxl)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(pdxlnPrElem->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *pvalColName = gpdb::PvalMakeString(col_name_char_array);
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);

		// save mapping col id -> index in translate context
		(void) pdxltrctxbt->InsertMapping(dxl_proj_elem->Id(), ul+1 /*iAttno*/);
	}

	// function arguments
	const ULONG ulChildren = pdxlnTVF->Arity();
	for (ULONG ul = 1; ul < ulChildren; ++ul)
	{
		CDXLNode *pdxlnFuncArg = (*pdxlnTVF)[ul];

		CMappingColIdVarPlStmt mapcidvarplstmt
									(
									m_memory_pool,
									pdxltrctxbt,
									NULL,
									pdxltrctxOut,
									m_pctxdxltoplstmt
									);

		Expr *pexprFuncArg = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnFuncArg, &mapcidvarplstmt);
		pfuncexpr->args = gpdb::PlAppendElement(pfuncexpr->args, pexprFuncArg);
	}

	// GDPB_91_MERGE_FIXME: collation
	pfuncexpr->inputcollid = gpdb::OidExprCollation((Node *) pfuncexpr->args);
	pfuncexpr->funccollid = gpdb::OidTypeCollation(pfuncexpr->funcresulttype);

	prte->funcexpr = (Node *)pfuncexpr;
	prte->inFromCl = true;
	prte->eref = palias;
	// GDPB_91_MERGE_FIXME: collation
	// set prte->funccoltypemods & prte->funccolcollations?

	return prte;
}


// create a range table entry from a CDXLPhysicalValuesScan node
RangeTblEntry *
CTranslatorDXLToPlStmt::PrteFromDXLValueScan
	(
	const CDXLNode *pdxlnValueScan,
	CDXLTranslateContext *pdxltrctxOut,
	CDXLTranslateContextBaseTable *pdxltrctxbt
	)
{
	CDXLPhysicalValuesScan *pdxlop = CDXLPhysicalValuesScan::Cast(pdxlnValueScan->GetOperator());

	RangeTblEntry *prte = MakeNode(RangeTblEntry);

	prte->relid = InvalidOid;
	prte->subquery = NULL;
	prte->rtekind = RTE_VALUES;
	prte->inh = false;			/* never true for values RTEs */
	prte->inFromCl = true;
	prte->requiredPerms = 0;
	prte->checkAsUser = InvalidOid;

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get value alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlop->GetOpNameStr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxl = (*pdxlnValueScan)[EdxltsIndexProjList];

	// get column names
	const ULONG ulCols = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < ulCols; ul++)
	{
		CDXLNode *pdxlnPrElem = (*project_list_dxl)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(pdxlnPrElem->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *pvalColName = gpdb::PvalMakeString(col_name_char_array);
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);

		// save mapping col id -> index in translate context
		(void) pdxltrctxbt->InsertMapping(dxl_proj_elem->Id(), ul+1 /*iAttno*/);
	}

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, pdxltrctxbt, NULL, pdxltrctxOut, m_pctxdxltoplstmt);
	const ULONG ulChildren = pdxlnValueScan->Arity();
	List *values_lists = NIL;
	List *values_collations = NIL;

	for (ULONG ulValue = EdxlValIndexConstStart; ulValue < ulChildren; ulValue++)
	{
		CDXLNode *pdxlnValueList = (*pdxlnValueScan)[ulValue];
		const ULONG ulCols = pdxlnValueList->Arity();
		List *value = NIL;
		for (ULONG ulCol = 0; ulCol < ulCols ; ulCol++)
		{
			Expr *pconst = m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnValueList)[ulCol], &mapcidvarplstmt);
			value = gpdb::PlAppendElement(value, pconst);

		}
		values_lists = gpdb::PlAppendElement(values_lists, value);

		// GPDB_91_MERGE_FIXME: collation
		if (NIL == values_collations)
		{
			// Set collation based on the first list of values
			for (ULONG ulCol = 0; ulCol < ulCols ; ulCol++)
			{
				values_collations = gpdb::PlAppendOid(values_collations, gpdb::OidExprCollation((Node *) value));
			}
		}
	}

	prte->values_lists = values_lists;
	prte->values_collations = values_collations;
	prte->eref = palias;

	return prte;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PnljFromDXLNLJ
//
//	@doc:
//		Translates a DXL nested loop join node into a NestLoop plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PnljFromDXLNLJ
	(
	const CDXLNode *pdxlnNLJ,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	GPOS_ASSERT(pdxlnNLJ->GetOperator()->GetDXLOperator() == EdxlopPhysicalNLJoin);
	GPOS_ASSERT(pdxlnNLJ->Arity() == EdxlnljIndexSentinel);

	// create hash join node
	NestLoop *pnlj = MakeNode(NestLoop);

	Join *pj = &(pnlj->join);
	Plan *pplan = &(pj->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalNLJoin *pdxlnlj = CDXLPhysicalNLJoin::PdxlConvert(pdxlnNLJ->GetOperator());

	// set join type
	pj->jointype = JtFromEdxljt(pdxlnlj->GetJoinType());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnNLJ->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*pdxlnNLJ)[EdxlnljIndexLeftChild];
	CDXLNode *pdxlnRight = (*pdxlnNLJ)[EdxlnljIndexRightChild];

	CDXLNode *project_list_dxl = (*pdxlnNLJ)[EdxlnljIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnNLJ)[EdxlnljIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*pdxlnNLJ)[EdxlnljIndexJoinFilter];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	// setting of prefetch_inner to true except for the case of index NLJ where we cannot prefetch inner
	// because inner child depends on variables coming from outer child
	pj->prefetch_inner = !pdxlnlj->IsIndexNLJ();

	DrgPdxltrctx *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	Plan *pplanLeft = NULL;
	Plan *pplanRight = NULL;
	if (pdxlnlj->IsIndexNLJ())
	{
		// right child (the index scan side) has references to left child's columns,
		// we need to translate left child first to load its columns into translation context
		pplanLeft = PplFromDXL(pdxlnLeft, &dxltrctxLeft, pdrgpdxltrctxPrevSiblings);

		pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
		 pdrgpdxltrctxWithSiblings->AppendArray(pdrgpdxltrctxPrevSiblings);

		 // translate right child after left child translation is complete
		pplanRight = PplFromDXL(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);
	}
	else
	{
		// left child may include a PartitionSelector with references to right child's columns,
		// we need to translate right child first to load its columns into translation context
		pplanRight = PplFromDXL(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxPrevSiblings);

		pdrgpdxltrctxWithSiblings->Append(&dxltrctxRight);
		pdrgpdxltrctxWithSiblings->AppendArray(pdrgpdxltrctxPrevSiblings);

		// translate left child after right child translation is complete
		pplanLeft = PplFromDXL(pdxlnLeft, &dxltrctxLeft, pdrgpdxltrctxWithSiblings);
	}
	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxLeft);
	pdrgpdxltrctx->Append(&dxltrctxRight);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	// translate join condition
	pj->joinqual = PlQualFromFilter
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					pdrgpdxltrctx,
					pdxltrctxOut
					);

	pplan->lefttree = pplanLeft;
	pplan->righttree = pplanRight;
	pplan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	pdrgpdxltrctx->Release();

	return  (Plan *) pnlj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PmjFromDXLMJ
//
//	@doc:
//		Translates a DXL merge join node into a MergeJoin node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PmjFromDXLMJ
	(
	const CDXLNode *pdxlnMJ,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	GPOS_ASSERT(pdxlnMJ->GetOperator()->GetDXLOperator() == EdxlopPhysicalMergeJoin);
	GPOS_ASSERT(pdxlnMJ->Arity() == EdxlmjIndexSentinel);

	// create merge join node
	MergeJoin *pmj = MakeNode(MergeJoin);

	Join *pj = &(pmj->join);
	Plan *pplan = &(pj->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalMergeJoin *pdxlopMergeJoin = CDXLPhysicalMergeJoin::Cast(pdxlnMJ->GetOperator());

	// set join type
	pj->jointype = JtFromEdxljt(pdxlopMergeJoin->GetJoinType());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnMJ->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate join children
	CDXLNode *pdxlnLeft = (*pdxlnMJ)[EdxlmjIndexLeftChild];
	CDXLNode *pdxlnRight = (*pdxlnMJ)[EdxlmjIndexRightChild];

	CDXLNode *project_list_dxl = (*pdxlnMJ)[EdxlmjIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnMJ)[EdxlmjIndexFilter];
	CDXLNode *pdxlnJoinFilter = (*pdxlnMJ)[EdxlmjIndexJoinFilter];
	CDXLNode *pdxlnMergeCondList = (*pdxlnMJ)[EdxlmjIndexMergeCondList];

	CDXLTranslateContext dxltrctxLeft(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());
	CDXLTranslateContext dxltrctxRight(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanLeft = PplFromDXL(pdxlnLeft, &dxltrctxLeft, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctxWithSiblings = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctxWithSiblings->Append(&dxltrctxLeft);
	pdrgpdxltrctxWithSiblings->AppendArray(pdrgpdxltrctxPrevSiblings);

	Plan *pplanRight = PplFromDXL(pdxlnRight, &dxltrctxRight, pdrgpdxltrctxWithSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxLeft));
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxRight));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	// translate join filter
	pj->joinqual = PlQualFromFilter
					(
					pdxlnJoinFilter,
					NULL,			// translate context for the base table
					pdrgpdxltrctx,
					pdxltrctxOut
					);

	// translate merge cond
	List *plMergeConditions = NIL;

	const ULONG arity = pdxlnMergeCondList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnMergeCond = (*pdxlnMergeCondList)[ul];
		List *plMergeCond = PlQualFromScalarCondNode
				(
				pdxlnMergeCond,
				NULL,			// base table translation context
				pdrgpdxltrctx,
				pdxltrctxOut
				);

		GPOS_ASSERT(1 == gpdb::ListLength(plMergeCond));
		plMergeConditions = gpdb::PlConcat(plMergeConditions, plMergeCond);
	}

	GPOS_ASSERT(NIL != plMergeConditions);

	pmj->mergeclauses = plMergeConditions;

	pplan->lefttree = pplanLeft;
	pplan->righttree = pplanRight;
	pplan->nMotionNodes = pplanLeft->nMotionNodes + pplanRight->nMotionNodes;
	SetParamIds(pplan);

	// GDPB_91_MERGE_FIXME: collation
	// Need to set pmj->mergeCollations, but ORCA does not produce plans with
	// Merge Joins.

	// cleanup
	pdrgpdxltrctxWithSiblings->Release();
	pdrgpdxltrctx->Release();

	return  (Plan *) pmj;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PhhashFromDXL
//
//	@doc:
//		Translates a DXL physical operator node into a Hash node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PhhashFromDXL
	(
	const CDXLNode *dxlnode,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	Hash *ph = MakeNode(Hash);

	Plan *pplan = &(ph->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate dxl node
	CDXLTranslateContext dxltrctx(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanLeft = PplFromDXL(dxlnode, &dxltrctx, pdrgpdxltrctxPrevSiblings);

	GPOS_ASSERT(0 < dxlnode->Arity());

	// create a reference to each entry in the child project list to create the target list of
	// the hash node
	CDXLNode *project_list_dxl = (*dxlnode)[0];
	List *target_list = PlTargetListForHashNode(project_list_dxl, &dxltrctx, pdxltrctxOut);

	// copy costs from child node; the startup cost for the hash node is the total cost
	// of the child plan, see make_hash in createplan.c
	pplan->startup_cost = pplanLeft->total_cost;
	pplan->total_cost = pplanLeft->total_cost;
	pplan->plan_rows = pplanLeft->plan_rows;
	pplan->plan_width = pplanLeft->plan_width;

	pplan->targetlist = target_list;
	pplan->lefttree = pplanLeft;
	pplan->righttree = NULL;
	pplan->nMotionNodes = pplanLeft->nMotionNodes;
	pplan->qual = NIL;
	ph->rescannable = false;

	SetParamIds(pplan);

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
	const CDXLNode *pdxlnMotion,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(pdxlnMotion->GetOperator());
	if (CTranslatorUtils::IsDuplicateSensitiveMotion(pdxlopMotion))
	{
		return PplanResultHashFilters(pdxlnMotion, pdxltrctxOut, pdrgpdxltrctxPrevSiblings);
	}
	
	return PplanMotionFromDXLMotion(pdxlnMotion, pdxltrctxOut, pdrgpdxltrctxPrevSiblings);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanMotionFromDXLMotion
//
//	@doc:
//		Translate DXL motion node into GPDB Motion plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanMotionFromDXLMotion
	(
	const CDXLNode *pdxlnMotion,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(pdxlnMotion->GetOperator());

	// create motion node
	Motion *pmotion = MakeNode(Motion);

	Plan *pplan = &(pmotion->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnMotion->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	CDXLNode *project_list_dxl = (*pdxlnMotion)[EdxlgmIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnMotion)[EdxlgmIndexFilter];
	CDXLNode *sort_col_list_dxl = (*pdxlnMotion)[EdxlgmIndexSortColList];

	// translate motion child
	// child node is in the same position in broadcast and gather motion nodes
	// but different in redistribute motion nodes

	ULONG ulChildIndex = pdxlopMotion->GetRelationChildIdx();

	CDXLNode *pdxlnChild = (*pdxlnMotion)[ulChildIndex];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
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

		TranslateSortCols(sort_col_list_dxl, pdxltrctxOut, pmotion->sortColIdx, pmotion->sortOperators, pmotion->collations, pmotion->nullsFirst);
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
			CDXLNode *pdxlnHashExprList = (*pdxlnMotion)[EdxlrmIndexHashExprList];

			TranslateHashExprList
				(
				pdxlnHashExprList,
				&dxltrctxChild,
				&plHashExpr,
				&plHashExprTypes,
				pdxltrctxOut
				);
		}
		GPOS_ASSERT(gpdb::ListLength(plHashExpr) == gpdb::ListLength(plHashExprTypes));

		pmotion->hashExpr = plHashExpr;
		pmotion->hashDataTypes = plHashExprTypes;
	}

	// cleanup
	pdrgpdxltrctx->Release();

	// create flow for child node to distinguish between singleton flows and all-segment flows
	Flow *pflow = MakeNode(Flow);

	const IntPtrArray *pdrgpiInputSegmentIds = pdxlopMotion->GetInputSegIdsArray();


	// only one sender
	if (1 == pdrgpiInputSegmentIds->Size())
	{
		pflow->segindex = *((*pdrgpiInputSegmentIds)[0]);

		// only one segment in total
		if (1 == gpdb::UlSegmentCountGP())
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

	pmotion->motionID = m_pctxdxltoplstmt->GetNextMotionId();
	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes + 1;

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

	SetParamIds(pplan);

	return (Plan *) pmotion;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanResultHashFilters
//
//	@doc:
//		Translate DXL duplicate sensitive redistribute motion node into 
//		GPDB result node with hash filters
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanResultHashFilters
	(
	const CDXLNode *pdxlnMotion,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create motion node
	Result *presult = MakeNode(Result);

	Plan *pplan = &(presult->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalMotion *pdxlopMotion = CDXLPhysicalMotion::Cast(pdxlnMotion->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnMotion->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	CDXLNode *project_list_dxl = (*pdxlnMotion)[EdxlrmIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnMotion)[EdxlrmIndexFilter];
	CDXLNode *pdxlnChild = (*pdxlnMotion)[pdxlopMotion->GetRelationChildIdx()];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	// translate hash expr list
	presult->hashFilter = true;

	if (EdxlopPhysicalMotionRedistribute == pdxlopMotion->GetDXLOperator())
	{
		CDXLNode *pdxlnHashExprList = (*pdxlnMotion)[EdxlrmIndexHashExprList];
		const ULONG length = pdxlnHashExprList->Arity();
		GPOS_ASSERT(0 < length);
		
		for (ULONG ul = 0; ul < length; ul++)
		{
			CDXLNode *pdxlnHashExpr = (*pdxlnHashExprList)[ul];
			CDXLNode *pdxlnExpr = (*pdxlnHashExpr)[0];
			
			INT iResno = gpos::int_max;
			if (EdxlopScalarIdent == pdxlnExpr->GetOperator()->GetDXLOperator())
			{
				ULONG col_id = CDXLScalarIdent::Cast(pdxlnExpr->GetOperator())->MakeDXLColRef()->Id();
				iResno = pdxltrctxOut->GetTargetEntry(col_id)->resno;
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
															pdrgpdxltrctx,
															pdxltrctxOut,
															m_pctxdxltoplstmt
															);
				
				Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnExpr, &mapcidvarplstmt);
				GPOS_ASSERT(NULL != pexpr);

				// create a target entry for the hash filter
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
				TargetEntry *target_entry = gpdb::PteMakeTargetEntry
											(
											pexpr, 
											gpdb::ListLength(pplan->targetlist) + 1, 
											CTranslatorUtils::CreateMultiByteCharStringFromWCString(strUnnamedCol.GetBuffer()),
											false /* resjunk */
											);
				pplan->targetlist = gpdb::PlAppendElement(pplan->targetlist, target_entry);

				iResno = target_entry->resno;
			}
			GPOS_ASSERT(gpos::int_max != iResno);
			
			presult->hashList = gpdb::PlAppendInt(presult->hashList, iResno);
		}
	}
	
	// cleanup
	pdrgpdxltrctx->Release();

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

	SetParamIds(pplan);

	return (Plan *) presult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PaggFromDXLAgg
//
//	@doc:
//		Translate DXL aggregate node into GPDB Agg plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PaggFromDXLAgg
	(
	const CDXLNode *pdxlnAgg,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create aggregate plan node
	Agg *pagg = MakeNode(Agg);

	Plan *pplan = &(pagg->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalAgg *pdxlopAgg = CDXLPhysicalAgg::Cast(pdxlnAgg->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnAgg->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate agg child
	CDXLNode *pdxlnChild = (*pdxlnAgg)[EdxlaggIndexChild];

	CDXLNode *project_list_dxl = (*pdxlnAgg)[EdxlaggIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnAgg)[EdxlaggIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, true, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,			// pdxltrctxRight,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

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
		Oid typeId = gpdb::OidExprType((Node *) pteGroupingCol->expr);
		pagg->grpOperators[ul] = gpdb::OidEqualityOp(typeId);
		Assert(pagg->grpOperators[ul] != 0);
	}

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) pagg;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PwindowFromDXLWindow
//
//	@doc:
//		Translate DXL window node into GPDB window plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PwindowFromDXLWindow
	(
	const CDXLNode *pdxlnWindow,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create a WindowAgg plan node
	WindowAgg *pwindow = MakeNode(WindowAgg);

	Plan *pplan = &(pwindow->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalWindow *pdxlopWindow = CDXLPhysicalWindow::Cast(pdxlnWindow->GetOperator());

	// translate the operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnWindow->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate children
	CDXLNode *pdxlnChild = (*pdxlnWindow)[EdxlwindowIndexChild];
	CDXLNode *project_list_dxl = (*pdxlnWindow)[EdxlwindowIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnWindow)[EdxlwindowIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, true, pdxltrctxOut->GetColIdToParamIdMap());
	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,			// pdxltrctxRight,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	ListCell *lc;

	foreach (lc, pplan->targetlist)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		if (IsA(target_entry->expr, WindowFunc))
		{
			WindowFunc *pwinfunc = (WindowFunc *) target_entry->expr;
			pwindow->winref = pwinfunc->winref;
			break;
		}
	}

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

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
		Oid typeId = gpdb::OidExprType((Node *) ptePartCol->expr);
		pwindow->partOperators[ul] = gpdb::OidEqualityOp(typeId);
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
		const CDXLNode *sort_col_list_dxl = pdxlwindowkey->GetSortColListDXL();

		const ULONG ulNumCols = sort_col_list_dxl->Arity();

		pwindow->ordNumCols = ulNumCols;
		pwindow->ordColIdx = (AttrNumber *) gpdb::GPDBAlloc(ulNumCols * sizeof(AttrNumber));
		pwindow->ordOperators = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
		bool *pNullsFirst = (bool *) gpdb::GPDBAlloc(ulNumCols * sizeof(bool));
		TranslateSortCols(sort_col_list_dxl, &dxltrctxChild, pwindow->ordColIdx, pwindow->ordOperators, NULL, pNullsFirst);

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
			pwindow->ordOperators[i] = gpdb::OidEqualityOpForOrderingOp(pwindow->ordOperators[i], NULL);
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
			DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
			pdrgpdxltrctx->Append(&dxltrctxChild);

			CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
			(
			 m_memory_pool,
			 NULL,
			 pdrgpdxltrctx,
			 pdxltrctxOut,
			 m_pctxdxltoplstmt
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
				pwindow->endOffset = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnLead)[0], &mapcidvarplstmt);
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
				pwindow->startOffset = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar((*pdxlnTrail)[0], &mapcidvarplstmt);
			}

			// cleanup
			pdrgpdxltrctx->Release();
		}
		else
			pwindow->frameOptions = FRAMEOPTION_DEFAULTS;
	}

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) pwindow;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PsortFromDXLSort
//
//	@doc:
//		Translate DXL sort node into GPDB Sort plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PsortFromDXLSort
	(
	const CDXLNode *pdxlnSort,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create sort plan node
	Sort *psort = MakeNode(Sort);

	Plan *pplan = &(psort->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalSort *pdxlopSort = CDXLPhysicalSort::Cast(pdxlnSort->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnSort->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate sort child
	CDXLNode *pdxlnChild = (*pdxlnSort)[EdxlsortIndexChild];
	CDXLNode *project_list_dxl = (*pdxlnSort)[EdxlsortIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnSort)[EdxlsortIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

	// set sorting info
	psort->noduplicates = pdxlopSort->FDiscardDuplicates();

	// translate sorting columns

	const CDXLNode *sort_col_list_dxl = (*pdxlnSort)[EdxlsortIndexSortColList];

	const ULONG ulNumCols = sort_col_list_dxl->Arity();
	psort->numCols = ulNumCols;
	psort->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(ulNumCols * sizeof(AttrNumber));
	psort->sortOperators = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
	psort->collations = (Oid *) gpdb::GPDBAlloc(ulNumCols * sizeof(Oid));
	psort->nullsFirst = (bool *) gpdb::GPDBAlloc(ulNumCols * sizeof(bool));

	TranslateSortCols(sort_col_list_dxl, &dxltrctxChild, psort->sortColIdx, psort->sortOperators, psort->collations, psort->nullsFirst);

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) psort;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PsubqscanFromDXLSubqScan
//
//	@doc:
//		Translate DXL subquery scan node into GPDB SubqueryScan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PsubqscanFromDXLSubqScan
	(
	const CDXLNode *pdxlnSubqScan,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create sort plan node
	SubqueryScan *psubqscan = MakeNode(SubqueryScan);

	Plan *pplan = &(psubqscan->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalSubqueryScan *pdxlopSubqscan = CDXLPhysicalSubqueryScan::Cast(pdxlnSubqScan->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnSubqScan->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate subplan
	CDXLNode *pdxlnChild = (*pdxlnSubqScan)[EdxlsubqscanIndexChild];
	CDXLNode *project_list_dxl = (*pdxlnSubqScan)[EdxlsubqscanIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnSubqScan)[EdxlsubqscanIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	// create an rtable entry for the subquery scan
	RangeTblEntry *prte = MakeNode(RangeTblEntry);
	prte->rtekind = RTE_SUBQUERY;

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get table alias
	palias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopSubqscan->MdName()->GetMDName()->GetBuffer());

	// get column names from child project list
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;
	(psubqscan->scan).scanrelid = iRel;
	dxltrctxbt.SetRelIndex(iRel);

	ListCell *plcTE = NULL;

	CDXLNode *pdxlnChildProjList = (*pdxlnChild)[0];

	ULONG ul = 0;

	ForEach (plcTE, pplanChild->targetlist)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(plcTE);

		// non-system attribute
		CHAR *col_name_char_array = PStrDup(target_entry->resname);
		Value *pvalColName = gpdb::PvalMakeString(col_name_char_array);
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);

		// get corresponding child project element
		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast((*pdxlnChildProjList)[ul]->GetOperator());

		// save mapping col id -> index in translate context
		(void) dxltrctxbt.InsertMapping(pdxlopPrel->Id(), target_entry->resno);
		ul++;
	}

	prte->eref = palias;

	// add range table entry for the subquery to the list
	m_pctxdxltoplstmt->AddRTE(prte);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,		// translate context for the base table
		NULL,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	psubqscan->subplan = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

	SetParamIds(pplan);
	return (Plan *) psubqscan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PresultFromDXLResult
//
//	@doc:
//		Translate DXL result node into GPDB result plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PresultFromDXLResult
	(
	const CDXLNode *pdxlnResult,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create result plan node
	Result *presult = MakeNode(Result);

	Plan *pplan = &(presult->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnResult->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	pplan->nMotionNodes = 0;

	CDXLNode *pdxlnChild = NULL;
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	if (pdxlnResult->Arity() - 1 == EdxlresultIndexChild)
	{
		// translate child plan
		pdxlnChild = (*pdxlnResult)[EdxlresultIndexChild];

		Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		presult->plan.lefttree = pplanChild;

		pplan->nMotionNodes = pplanChild->nMotionNodes;
	}

	CDXLNode *project_list_dxl = (*pdxlnResult)[EdxlresultIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnResult)[EdxlresultIndexFilter];
	CDXLNode *pdxlnOneTimeFilter = (*pdxlnResult)[EdxlresultIndexOneTimeFilter];

	List *plQuals = NULL;

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,		// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&plQuals,
		pdxltrctxOut
		);

	// translate one time filter
	List *plOneTimeQuals = PlQualFromFilter
							(
							pdxlnOneTimeFilter,
							NULL,			// base table translation context
							pdrgpdxltrctx,
							pdxltrctxOut
							);

	pplan->qual = plQuals;

	presult->resconstantqual = (Node *) plOneTimeQuals;

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) presult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanPartitionSelector
//
//	@doc:
//		Translate DXL PartitionSelector into a GPDB PartitionSelector node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanPartitionSelector
	(
	const CDXLNode *pdxlnPartitionSelector,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	PartitionSelector *ppartsel = MakeNode(PartitionSelector);

	Plan *pplan = &(ppartsel->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalPartitionSelector *pdxlopPartSel = CDXLPhysicalPartitionSelector::Cast(pdxlnPartitionSelector->GetOperator());
	const ULONG ulLevels = pdxlopPartSel->GetPartitioningLevel();
	ppartsel->nLevels = ulLevels;
	ppartsel->scanId = pdxlopPartSel->ScanId();
	ppartsel->relid = CMDIdGPDB::CastMdid(pdxlopPartSel->GetRelMdId())->OidObjectId();
	ppartsel->selectorId = m_ulPartitionSelectorCounter++;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnPartitionSelector->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	pplan->nMotionNodes = 0;

	CDXLNode *pdxlnChild = NULL;
	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	BOOL fHasChild = (EdxlpsIndexChild == pdxlnPartitionSelector->Arity() - 1);
	if (fHasChild)
	{
		// translate child plan
		pdxlnChild = (*pdxlnPartitionSelector)[EdxlpsIndexChild];

		Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);
		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		ppartsel->plan.lefttree = pplanChild;
		pplan->nMotionNodes = pplanChild->nMotionNodes;
	}

	pdrgpdxltrctx->Append(&dxltrctxChild);

	CDXLNode *project_list_dxl = (*pdxlnPartitionSelector)[EdxlpsIndexProjList];
	CDXLNode *pdxlnEqFilters = (*pdxlnPartitionSelector)[EdxlpsIndexEqFilters];
	CDXLNode *pdxlnFilters = (*pdxlnPartitionSelector)[EdxlpsIndexFilters];
	CDXLNode *pdxlnResidualFilter = (*pdxlnPartitionSelector)[EdxlpsIndexResidualFilter];
	CDXLNode *pdxlnPropExpr = (*pdxlnPartitionSelector)[EdxlpsIndexPropExpr];

	// translate proj list
	pplan->targetlist = PlTargetListFromProjList(project_list_dxl, NULL /*pdxltrctxbt*/, pdrgpdxltrctx, pdxltrctxOut);

	// translate filter lists
	GPOS_ASSERT(pdxlnEqFilters->Arity() == ulLevels);
	ppartsel->levelEqExpressions = PlFilterList(pdxlnEqFilters, NULL /*pdxltrctxbt*/, pdrgpdxltrctx, pdxltrctxOut);

	GPOS_ASSERT(pdxlnFilters->Arity() == ulLevels);
	ppartsel->levelExpressions = PlFilterList(pdxlnFilters, NULL /*pdxltrctxbt*/, pdrgpdxltrctx, pdxltrctxOut);

	//translate residual filter
	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, NULL /*pdxltrctxbt*/, pdrgpdxltrctx, pdxltrctxOut, m_pctxdxltoplstmt);
	if (!m_pdxlsctranslator->FConstTrue(pdxlnResidualFilter, m_pmda))
	{
		ppartsel->residualPredicate = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnResidualFilter, &mapcidvarplstmt);
	}

	//translate propagation expression
	if (!m_pdxlsctranslator->FConstNull(pdxlnPropExpr))
	{
		ppartsel->propagationExpression = (Node *) m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnPropExpr, &mapcidvarplstmt);
	}

	// no need to translate printable filter - since it is not needed by the executor

	ppartsel->staticPartOids = NIL;
	ppartsel->staticScanIds = NIL;
	ppartsel->staticSelection = !fHasChild;

	if (ppartsel->staticSelection)
	{
		SelectedParts *sp = gpdb::SpStaticPartitionSelection(ppartsel);
		ppartsel->staticPartOids = sp->partOids;
		ppartsel->staticScanIds = sp->scanIds;
		gpdb::GPDBFree(sp);
	}
	else
	{
		// if we cannot do static elimination then add this partitioned table oid
		// to the planned stmt so we can ship the constraints with the plan
		m_pctxdxltoplstmt->AddPartitionedTable(ppartsel->relid);
	}

	// increment the number of partition selectors for the given scan id
	m_pctxdxltoplstmt->IncrementPartitionSelectors(ppartsel->scanId);

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) ppartsel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlFilterList
//
//	@doc:
//		Translate DXL filter list into GPDB filter list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlFilterList
	(
	const CDXLNode *pdxlnFilterList,
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	GPOS_ASSERT(EdxlopScalarOpList == pdxlnFilterList->GetOperator()->GetDXLOperator());

	List *plFilters = NIL;

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt(m_memory_pool, pdxltrctxbt, pdrgpdxltrctx, pdxltrctxOut, m_pctxdxltoplstmt);
	const ULONG arity = pdxlnFilterList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnChildFilter = (*pdxlnFilterList)[ul];

		if (m_pdxlsctranslator->FConstTrue(pdxlnChildFilter, m_pmda))
		{
			plFilters = gpdb::PlAppendElement(plFilters, NULL /*datum*/);
			continue;
		}

		Expr *pexprFilter = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnChildFilter, &mapcidvarplstmt);
		plFilters = gpdb::PlAppendElement(plFilters, pexprFilter);
	}

	return plFilters;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PappendFromDXLAppend
//
//	@doc:
//		Translate DXL append node into GPDB Append plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PappendFromDXLAppend
	(
	const CDXLNode *pdxlnAppend,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create append plan node
	Append *pappend = MakeNode(Append);

	Plan *pplan = &(pappend->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnAppend->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	const ULONG arity = pdxlnAppend->Arity();
	GPOS_ASSERT(EdxlappendIndexFirstChild < arity);
	pplan->nMotionNodes = 0;
	pappend->appendplans = NIL;
	
	// translate children
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());
	for (ULONG ul = EdxlappendIndexFirstChild; ul < arity; ul++)
	{
		CDXLNode *pdxlnChild = (*pdxlnAppend)[ul];

		Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

		GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

		pappend->appendplans = gpdb::PlAppendElement(pappend->appendplans, pplanChild);
		pplan->nMotionNodes += pplanChild->nMotionNodes;
	}

	CDXLNode *project_list_dxl = (*pdxlnAppend)[EdxlappendIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnAppend)[EdxlappendIndexFilter];

	pplan->targetlist = NIL;
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

		Var *var = gpdb::PvarMakeVar
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
		pdxltrctxOut->InsertMapping(pdxlopPrel->Id(), target_entry);

		pplan->targetlist = gpdb::PlAppendElement(pplan->targetlist, target_entry);
	}

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(pdxltrctxOut));

	// translate filter
	pplan->qual = PlQualFromFilter
					(
					filter_dxlnode,
					NULL, // translate context for the base table
					pdrgpdxltrctx,
					pdxltrctxOut
					);

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) pappend;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PmatFromDXLMaterialize
//
//	@doc:
//		Translate DXL materialize node into GPDB Material plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PmatFromDXLMaterialize
	(
	const CDXLNode *pdxlnMaterialize,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create materialize plan node
	Material *pmat = MakeNode(Material);

	Plan *pplan = &(pmat->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalMaterialize *pdxlopMat = CDXLPhysicalMaterialize::Cast(pdxlnMaterialize->GetOperator());

	pmat->cdb_strict = pdxlopMat->IsEager();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnMaterialize->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate materialize child
	CDXLNode *pdxlnChild = (*pdxlnMaterialize)[EdxlmatIndexChild];

	CDXLNode *project_list_dxl = (*pdxlnMaterialize)[EdxlmatIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnMaterialize)[EdxlmatIndexFilter];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

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

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) pmat;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PshscanFromDXLCTEProducer
//
//	@doc:
//		Translate DXL CTE Producer node into GPDB share input scan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PshscanFromDXLCTEProducer
	(
	const CDXLNode *pdxlnCTEProducer,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalCTEProducer *pdxlopCTEProducer = CDXLPhysicalCTEProducer::Cast(pdxlnCTEProducer->GetOperator());
	ULONG ulCTEId = pdxlopCTEProducer->Id();

	// create the shared input scan representing the CTE Producer
	ShareInputScan *pshscanCTEProducer = MakeNode(ShareInputScan);
	pshscanCTEProducer->share_id = ulCTEId;
	Plan *pplan = &(pshscanCTEProducer->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// store share scan node for the translation of CTE Consumers
	m_pctxdxltoplstmt->AddCTEConsumerInfo(ulCTEId, pshscanCTEProducer);

	// translate cost of the producer
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnCTEProducer->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// translate child plan
	CDXLNode *project_list_dxl = (*pdxlnCTEProducer)[0];
	CDXLNode *pdxlnChild = (*pdxlnCTEProducer)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());
	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);
	GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxChild);
	// translate proj list
	pplan->targetlist = PlTargetListFromProjList
							(
							project_list_dxl,
							NULL,		// base table translation context
							pdrgpdxltrctx,
							pdxltrctxOut
							);

	// if the child node is neither a sort or materialize node then add a materialize node
	if (!IsA(pplanChild, Material) && !IsA(pplanChild, Sort))
	{
		Material *pmat = MakeNode(Material);
		pmat->cdb_strict = false; // eager-free

		Plan *pplanMat = &(pmat->plan);
		pplanMat->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

		TranslatePlanCosts
			(
			CDXLPhysicalProperties::PdxlpropConvert(pdxlnCTEProducer->GetProperties())->GetDXLOperatorCost(),
			&(pplanMat->startup_cost),
			&(pplanMat->total_cost),
			&(pplanMat->plan_rows),
			&(pplanMat->plan_width)
			);

		// create a target list for the newly added materialize
		ListCell *plcTe = NULL;
		pplanMat->targetlist = NIL;
		ForEach (plcTe, pplan->targetlist)
		{
			TargetEntry *target_entry = (TargetEntry *) lfirst(plcTe);
			Expr *pexpr = target_entry->expr;
			GPOS_ASSERT(IsA(pexpr, Var));

			Var *var = (Var *) pexpr;
			Var *pvarNew = gpdb::PvarMakeVar(OUTER, var->varattno, var->vartype, var->vartypmod,	0 /* varlevelsup */);
			pvarNew->varnoold = var->varnoold;
			pvarNew->varoattno = var->varoattno;

			TargetEntry *pteNew = gpdb::PteMakeTargetEntry((Expr *) pvarNew, var->varattno, PStrDup(target_entry->resname), target_entry->resjunk);
			pplanMat->targetlist = gpdb::PlAppendElement(pplanMat->targetlist, pteNew);
		}

		pplanMat->lefttree = pplanChild;
		pplanMat->nMotionNodes = pplanChild->nMotionNodes;

		pplanChild = pplanMat;
	}

	InitializeSpoolingInfo(pplanChild, ulCTEId);

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;
	pplan->qual = NIL;
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

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
	Plan *pplan,
	ULONG ulShareId
	)
{
	List *plshscanCTEConsumer = m_pctxdxltoplstmt->GetCTEConsumerList(ulShareId);
	GPOS_ASSERT(NULL != plshscanCTEConsumer);

	Flow *pflow = PflowCTEConsumer(plshscanCTEConsumer);

	const ULONG ulLenSis = gpdb::ListLength(plshscanCTEConsumer);

	ShareType share_type = SHARE_NOTSHARED;

	if (IsA(pplan, Material))
	{
		Material *pmat = (Material *) pplan;
		pmat->share_id = ulShareId;
		pmat->nsharer = ulLenSis;
		share_type = SHARE_MATERIAL;
		// the share_type is later reset to SHARE_MATERIAL_XSLICE (if needed) by the apply_shareinput_xslice
		pmat->share_type = share_type;
		GPOS_ASSERT(NULL == (pmat->plan).flow);
		(pmat->plan).flow = pflow;
	}
	else
	{
		GPOS_ASSERT(IsA(pplan, Sort));
		Sort *psort = (Sort *) pplan;
		psort->share_id = ulShareId;
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
	ForEach (plcShscanCTEConsumer, plshscanCTEConsumer)
	{
		ShareInputScan *pshscanConsumer = (ShareInputScan *) lfirst(plcShscanCTEConsumer);
		pshscanConsumer->share_type = share_type;
		pshscanConsumer->driver_slice = -1; // default
		if (NULL == (pshscanConsumer->scan.plan).flow)
		{
			(pshscanConsumer->scan.plan).flow = (Flow *) gpdb::PvCopyObject(pflow);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PflowCTEConsumer
//
//	@doc:
//		Retrieve the flow of the shared input scan of the cte consumers. If
//		multiple CTE consumers have a flow then ensure that they are of the
//		same type
//---------------------------------------------------------------------------
Flow *
CTranslatorDXLToPlStmt::PflowCTEConsumer
	(
	List *plshscanCTEConsumer
	)
{
	Flow *pflow = NULL;

	ListCell *plcShscanCTEConsumer = NULL;
	ForEach (plcShscanCTEConsumer, plshscanCTEConsumer)
	{
		ShareInputScan *pshscanConsumer = (ShareInputScan *) lfirst(plcShscanCTEConsumer);
		Flow *pflowCte = (pshscanConsumer->scan.plan).flow;
		if (NULL != pflowCte)
		{
			if (NULL == pflow)
			{
				pflow = (Flow *) gpdb::PvCopyObject(pflowCte);
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
//		CTranslatorDXLToPlStmt::PshscanFromDXLCTEConsumer
//
//	@doc:
//		Translate DXL CTE Consumer node into GPDB share input scan plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PshscanFromDXLCTEConsumer
	(
	const CDXLNode *pdxlnCTEConsumer,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalCTEConsumer *pdxlopCTEConsumer = CDXLPhysicalCTEConsumer::Cast(pdxlnCTEConsumer->GetOperator());
	ULONG ulCTEId = pdxlopCTEConsumer->Id();

	ShareInputScan *pshscanCTEConsumer = MakeNode(ShareInputScan);
	pshscanCTEConsumer->share_id = ulCTEId;

	Plan *pplan = &(pshscanCTEConsumer->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnCTEConsumer->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

#ifdef GPOS_DEBUG
	ULongPtrArray *pdrgpulCTEColId = pdxlopCTEConsumer->GetOutputColIdsArray();
#endif

	// generate the target list of the CTE Consumer
	pplan->targetlist = NIL;
	CDXLNode *project_list_dxl = (*pdxlnCTEConsumer)[0];
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

		Var *var = gpdb::PvarMakeVar(OUTER, (AttrNumber) (ul + 1), oidType, pdxlopScIdent->TypeModifier(),  0	/* varlevelsup */);

		CHAR *szResname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrE->GetMdNameAlias()->GetMDName()->GetBuffer());
		TargetEntry *target_entry = gpdb::PteMakeTargetEntry((Expr *) var, (AttrNumber) (ul + 1), szResname, false /* resjunk */);
		pplan->targetlist = gpdb::PlAppendElement(pplan->targetlist, target_entry);

		pdxltrctxOut->InsertMapping(col_id, target_entry);
	}

	pplan->qual = NULL;

	SetParamIds(pplan);

	// store share scan node for the translation of CTE Consumers
	m_pctxdxltoplstmt->AddCTEConsumerInfo(ulCTEId, pshscanCTEConsumer);

	return (Plan *) pshscanCTEConsumer;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanSequence
//
//	@doc:
//		Translate DXL sequence node into GPDB Sequence plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanSequence
	(
	const CDXLNode *pdxlnSequence,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create append plan node
	Sequence *psequence = MakeNode(Sequence);

	Plan *pplan = &(psequence->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnSequence->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	ULONG arity = pdxlnSequence->Arity();
	
	// translate last child
	// since last child may be a DynamicIndexScan with outer references,
	// we pass the context received from parent to translate outer refs here

	CDXLNode *pdxlnLastChild = (*pdxlnSequence)[arity - 1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanLastChild = PplFromDXL(pdxlnLastChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);
	pplan->nMotionNodes = pplanLastChild->nMotionNodes;

	CDXLNode *project_list_dxl = (*pdxlnSequence)[0];

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list
	pplan->targetlist = PlTargetListFromProjList
						(
						project_list_dxl,
						NULL,		// base table translation context
						pdrgpdxltrctx,
						pdxltrctxOut
						);

	// translate the rest of the children
	for (ULONG ul = 1; ul < arity - 1; ul++)
	{
		CDXLNode *pdxlnChild = (*pdxlnSequence)[ul];

		Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

		psequence->subplans = gpdb::PlAppendElement(psequence->subplans, pplanChild);
		pplan->nMotionNodes += pplanChild->nMotionNodes;
	}

	psequence->subplans = gpdb::PlAppendElement(psequence->subplans, pplanLastChild);

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) psequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanDTS
//
//	@doc:
//		Translates a DXL dynamic table scan node into a DynamicTableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanDTS
	(
	const CDXLNode *pdxlnDTS,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDynamicTableScan *pdxlop = CDXLPhysicalDynamicTableScan::Cast(pdxlnDTS->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	RangeTblEntry *prte = PrteFromTblDescr(pdxlop->GetDXLTableDescr(), NULL /*index_descr_dxl*/, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= ACL_SELECT;

	m_pctxdxltoplstmt->AddRTE(prte);

	// create dynamic scan node
	DynamicTableScan *pdts = MakeNode(DynamicTableScan);

	pdts->scanrelid = iRel;
	pdts->partIndex = pdxlop->GetPartIndexId();
	pdts->partIndexPrintable = pdxlop->GetPartIndexIdPrintable();

	Plan *pplan = &(pdts->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnDTS->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	GPOS_ASSERT(2 == pdxlnDTS->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*pdxlnDTS)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnDTS)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		NULL,			// pdxltrctxLeft and pdxltrctxRight,
		&pplan->targetlist,
		&pplan->qual,
		pdxltrctxOut
		);

	SetParamIds(pplan);

	return (Plan *) pdts;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanDIS
//
//	@doc:
//		Translates a DXL dynamic index scan node into a DynamicIndexScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanDIS
	(
	const CDXLNode *pdxlnDIS,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalDynamicIndexScan *pdxlop = CDXLPhysicalDynamicIndexScan::Cast(pdxlnDIS->GetOperator());
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxlop->GetDXLTableDescr()->MDId());
	RangeTblEntry *prte = PrteFromTblDescr(pdxlop->GetDXLTableDescr(), NULL /*index_descr_dxl*/, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= ACL_SELECT;
	m_pctxdxltoplstmt->AddRTE(prte);

	DynamicIndexScan *pdis = MakeNode(DynamicIndexScan);

	pdis->indexscan.scan.scanrelid = iRel;
	pdis->indexscan.scan.partIndex = pdxlop->GetPartIndexId();
	pdis->indexscan.scan.partIndexPrintable = pdxlop->GetPartIndexIdPrintable();

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(pdxlop->GetDXLIndexDescr()->MDId());
	const IMDIndex *index = m_pmda->Pmdindex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pdis->indexscan.indexid = oidIndex;
	pdis->logicalIndexInfo = gpdb::Plgidxinfo(prte->relid, oidIndex);

	Plan *pplan = &(pdis->indexscan.scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnDIS->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	// an index scan node must have 3 children: projection list, filter and index condition list
	GPOS_ASSERT(3 == pdxlnDIS->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*pdxlnDIS)[CDXLPhysicalDynamicIndexScan::EdxldisIndexProjList];
	CDXLNode *filter_dxlnode = (*pdxlnDIS)[CDXLPhysicalDynamicIndexScan::EdxldisIndexFilter];
	CDXLNode *pdxlnIndexCondList = (*pdxlnDIS)[CDXLPhysicalDynamicIndexScan::EdxldisIndexCondition];

	// translate proj list
	pplan->targetlist = PlTargetListFromProjList(project_list_dxl, &dxltrctxbt, NULL /*pdrgpdxltrctx*/, pdxltrctxOut);

	// translate index filter
	pplan->qual = PlTranslateIndexFilter
					(
					filter_dxlnode,
					pdxltrctxOut,
					&dxltrctxbt,
					pdrgpdxltrctxPrevSiblings
					);

	pdis->indexscan.indexorderdir = CTranslatorUtils::GetScanDirection(pdxlop->GetIndexScanDir());

	// translate index condition list
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		pdxlnIndexCondList, 
		pdxlop->GetDXLTableDescr(),
		false, // fIndexOnlyScan 
		index, 
		pmdrel,
		pdxltrctxOut,
		&dxltrctxbt,
		pdrgpdxltrctxPrevSiblings,
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
	SetParamIds(pplan);

	return (Plan *) pdis;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanDML
//
//	@doc:
//		Translates a DXL DML node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanDML
	(
	const CDXLNode *pdxlnDML,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDML *pdxlop = CDXLPhysicalDML::Cast(pdxlnDML->GetOperator());

	// create DML node
	DML *pdml = MakeNode(DML);
	Plan *pplan = &(pdml->plan);
	AclMode aclmode = ACL_NO_RIGHTS;
	
	switch (pdxlop->GetDmlOpType())
	{
		case gpdxl::Edxldmldelete:
		{
			m_cmdtype = CMD_DELETE;
			aclmode = ACL_DELETE;
			break;
		}
		case gpdxl::Edxldmlupdate:
		{
			m_cmdtype = CMD_UPDATE;
			aclmode = ACL_UPDATE;
			break;
		}
		case gpdxl::Edxldmlinsert:
		{
			m_cmdtype = CMD_INSERT;
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
	
	IMDId *pmdidTargetTable = pdxlop->GetDXLTableDescr()->MDId();
	if (IMDRelation::EreldistrMasterOnly != m_pmda->Pmdrel(pmdidTargetTable)->GetRelDistribution())
	{
		m_fTargetTableDistributed = true;
	}
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;
	pdml->scanrelid = iRel;
	
	m_plResultRelations = gpdb::PlAppendInt(m_plResultRelations, iRel);

	const IMDRelation *pmdrel = m_pmda->Pmdrel(pdxlop->GetDXLTableDescr()->MDId());

	CDXLTableDescr *table_descr = pdxlop->GetDXLTableDescr();
	RangeTblEntry *prte = PrteFromTblDescr(table_descr, NULL /*index_descr_dxl*/, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= aclmode;
	m_pctxdxltoplstmt->AddRTE(prte);
	
	CDXLNode *project_list_dxl = (*pdxlnDML)[0];
	CDXLNode *pdxlnChild = (*pdxlnDML)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxChild);

	// translate proj list
	List *plTargetListDML = PlTargetListFromProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		pdxltrctxOut
		);
	
	if (pmdrel->HasDroppedColumns())
	{
		// pad DML target list with NULLs for dropped columns for all DML operator types
		List *plTargetListWithDroppedCols = PlTargetListWithDroppedCols(plTargetListDML, pmdrel);
		gpdb::GPDBFree(plTargetListDML);
		plTargetListDML = plTargetListWithDroppedCols;
	}

	// Extract column numbers of the action and ctid columns from the
	// target list. ORCA also includes a third similar column for
	// partition Oid to the target list, but we don't use it for anything
	// in GPDB.
	pdml->actionColIdx = UlAddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, pdxlop->ActionColId(), true /*fResjunk*/);
	pdml->ctidColIdx = UlAddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, pdxlop->GetCtIdColId(), true /*fResjunk*/);
	if (pdxlop->IsOidsPreserved())
	{
		pdml->tupleoidColIdx = UlAddTargetEntryForColId(&plTargetListDML, &dxltrctxChild, pdxlop->GetTupleOid(), true /*fResjunk*/);
	}
	else
	{
		pdml->tupleoidColIdx = 0;
	}

	GPOS_ASSERT(0 != pdml->actionColIdx);

	pplan->targetlist = plTargetListDML;
	
	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	if (CMD_INSERT == m_cmdtype && 0 == pplan->nMotionNodes)
	{
		List *plDirectDispatchSegIds = PlDirectDispatchSegIds(pdxlop->GetDXLDirectDispatchInfo());
		pplan->directDispatch.contentIds = plDirectDispatchSegIds;
		pplan->directDispatch.isDirectDispatch = (NIL != plDirectDispatchSegIds);
	}
	
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();


	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnDML->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	return (Plan *) pdml;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlDirectDispatchSegIds
//
//	@doc:
//		Translate the direct dispatch info
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlDirectDispatchSegIds
	(
	CDXLDirectDispatchInfo *pdxlddinfo
	)
{
	if (!optimizer_enable_direct_dispatch || NULL == pdxlddinfo)
	{
		return NIL;
	}
	
	DXLDatumArrays *pdrgpdrgpdxldatum = pdxlddinfo->GetDispatchIdentifierDatumArray();
	
	if (pdrgpdrgpdxldatum == NULL || 0 == pdrgpdrgpdxldatum->Size())
	{
		return NIL;
	}
	
	DXLDatumArray *pdrgpdxldatum = (*pdrgpdrgpdxldatum)[0];
	GPOS_ASSERT(0 < pdrgpdxldatum->Size());
		
	ULONG ulHashCode = UlCdbHash(pdrgpdxldatum);
	const ULONG length = pdrgpdrgpdxldatum->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		DXLDatumArray *pdrgpdxldatumDisj = (*pdrgpdrgpdxldatum)[ul];
		GPOS_ASSERT(0 < pdrgpdxldatumDisj->Size());
		ULONG ulHashCodeNew = UlCdbHash(pdrgpdxldatumDisj);
		
		if (ulHashCode != ulHashCodeNew)
		{
			// values don't hash to the same segment
			return NIL;
		}
	}
	
	List *plSegIds = gpdb::PlAppendInt(NIL, ulHashCode);
	return plSegIds;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::UlCdbHash
//
//	@doc:
//		Hash a DXL datum
//
//---------------------------------------------------------------------------
ULONG
CTranslatorDXLToPlStmt::UlCdbHash
	(
	DXLDatumArray *pdrgpdxldatum
	)
{
	List *plConsts = NIL;
	
	const ULONG length = pdrgpdxldatum->Size();
	
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLDatum *datum_dxl = (*pdrgpdxldatum)[ul];
		
		Const *pconst = (Const *) m_pdxlsctranslator->PconstFromDXLDatum(datum_dxl);
		plConsts = gpdb::PlAppendElement(plConsts, pconst);
	}

	ULONG ulHash = gpdb::ICdbHashList(plConsts, m_num_of_segments);

	gpdb::FreeListDeep(plConsts);
	
	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanSplit
//
//	@doc:
//		Translates a DXL Split node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanSplit
	(
	const CDXLNode *pdxlnSplit,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalSplit *pdxlop = CDXLPhysicalSplit::Cast(pdxlnSplit->GetOperator());

	// create SplitUpdate node
	SplitUpdate *psplit = MakeNode(SplitUpdate);
	Plan *pplan = &(psplit->plan);
	
	CDXLNode *project_list_dxl = (*pdxlnSplit)[0];
	CDXLNode *pdxlnChild = (*pdxlnSplit)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxChild);

	// translate proj list and filter
	pplan->targetlist = PlTargetListFromProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		pdxltrctxOut
		);

	// translate delete and insert columns
	ULongPtrArray *pdrgpulDeleteCols = pdxlop->GetDeletionColIdArray();
	ULongPtrArray *pdrgpulInsertCols = pdxlop->GetInsertionColIdArray();
		
	GPOS_ASSERT(pdrgpulInsertCols->Size() == pdrgpulDeleteCols->Size());
	
	psplit->deleteColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulDeleteCols, &dxltrctxChild);
	psplit->insertColIdx = CTranslatorUtils::ConvertColidToAttnos(pdrgpulInsertCols, &dxltrctxChild);
	
	const TargetEntry *pteActionCol = pdxltrctxOut->GetTargetEntry(pdxlop->ActionColId());
	const TargetEntry *pteCtidCol = pdxltrctxOut->GetTargetEntry(pdxlop->GetCtIdColId());
	const TargetEntry *pteTupleOidCol = pdxltrctxOut->GetTargetEntry(pdxlop->GetTupleOid());

	if (NULL  == pteActionCol)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, pdxlop->ActionColId());
	}
	if (NULL  == pteCtidCol)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, pdxlop->GetCtIdColId());
	}	 
	
	psplit->actionColIdx = pteActionCol->resno;
	psplit->ctidColIdx = pteCtidCol->resno;
	
	psplit->tupleoidColIdx = FirstLowInvalidHeapAttributeNumber;
	if (NULL != pteTupleOidCol)
	{
		psplit->tupleoidColIdx = pteTupleOidCol->resno;
	}

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnSplit->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	return (Plan *) psplit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanAssert
//
//	@doc:
//		Translate DXL assert node into GPDB assert plan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanAssert
	(
	const CDXLNode *pdxlnAssert,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// create assert plan node
	AssertOp *passert = MakeNode(AssertOp);

	Plan *pplan = &(passert->plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	CDXLPhysicalAssert *pdxlopAssert = CDXLPhysicalAssert::Cast(pdxlnAssert->GetOperator());

	// translate error code into the its internal GPDB representation
	const CHAR *error_code = pdxlopAssert->GetSQLState();
	GPOS_ASSERT(GPOS_SQLSTATE_LENGTH == clib::Strlen(error_code));
	
	passert->errcode = MAKE_SQLSTATE(error_code[0], error_code[1], error_code[2], error_code[3], error_code[4]);
	CDXLNode *filter_dxlnode = (*pdxlnAssert)[CDXLPhysicalAssert::EdxlassertIndexFilter];

	passert->errmessage = CTranslatorUtils::GetAssertErrorMsgs(filter_dxlnode);

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnAssert->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	// translate child plan
	CDXLNode *pdxlnChild = (*pdxlnAssert)[CDXLPhysicalAssert::EdxlassertIndexChild];
	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	GPOS_ASSERT(NULL != pplanChild && "child plan cannot be NULL");

	passert->plan.lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;

	CDXLNode *project_list_dxl = (*pdxlnAssert)[CDXLPhysicalAssert::EdxlassertIndexProjList];

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(const_cast<CDXLTranslateContext*>(&dxltrctxChild));

	// translate proj list
	pplan->targetlist = PlTargetListFromProjList
				(
				project_list_dxl,
				NULL,			// translate context for the base table
				pdrgpdxltrctx,
				pdxltrctxOut
				);

	// translate assert constraints
	pplan->qual = PlTranslateAssertConstraints
					(
					filter_dxlnode,
					pdxltrctxOut,
					pdrgpdxltrctx
					);
	
	GPOS_ASSERT(gpdb::ListLength(pplan->qual) == gpdb::ListLength(passert->errmessage));
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	return (Plan *) passert;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanRowTrigger
//
//	@doc:
//		Translates a DXL Row Trigger node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanRowTrigger
	(
	const CDXLNode *pdxlnRowTrigger,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalRowTrigger *pdxlop = CDXLPhysicalRowTrigger::Cast(pdxlnRowTrigger->GetOperator());

	// create RowTrigger node
	RowTrigger *prowtrigger = MakeNode(RowTrigger);
	Plan *pplan = &(prowtrigger->plan);

	CDXLNode *project_list_dxl = (*pdxlnRowTrigger)[0];
	CDXLNode *pdxlnChild = (*pdxlnRowTrigger)[1];

	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplanChild = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxChild);

	// translate proj list and filter
	pplan->targetlist = PlTargetListFromProjList
		(
		project_list_dxl,
		NULL,			// translate context for the base table
		pdrgpdxltrctx,
		pdxltrctxOut
		);

	Oid oidRelid = CMDIdGPDB::CastMdid(pdxlop->GetRelMdId())->OidObjectId();
	GPOS_ASSERT(InvalidOid != oidRelid);
	prowtrigger->relid = oidRelid;
	prowtrigger->eventFlags = pdxlop->GetType();

	// translate old and new columns
	ULongPtrArray *pdrgpulOldCols = pdxlop->GetColIdsOld();
	ULongPtrArray *pdrgpulNewCols = pdxlop->GetColIdsNew();

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

	pplan->lefttree = pplanChild;
	pplan->nMotionNodes = pplanChild->nMotionNodes;
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();

	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnRowTrigger->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	return (Plan *) prowtrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PrteFromTblDescr
//
//	@doc:
//		Translates a DXL table descriptor into a range table entry. If an index
//		descriptor is provided, we use the mapping from colids to index attnos
//		instead of table attnos
//
//---------------------------------------------------------------------------
RangeTblEntry *
CTranslatorDXLToPlStmt::PrteFromTblDescr
	(
	const CDXLTableDescr *table_descr,
	const CDXLIndexDescr *index_descr_dxl, // should be NULL unless we have an index-only scan
	Index iRel,
	CDXLTranslateContextBaseTable *pdxltrctxbtOut
	)
{
	GPOS_ASSERT(NULL != table_descr);

	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());
	const ULONG ulRelColumns = CTranslatorUtils::GetNumNonSystemColumns(pmdrel);

	RangeTblEntry *prte = MakeNode(RangeTblEntry);
	prte->rtekind = RTE_RELATION;

	// get the index if given
	const IMDIndex *index = NULL;
	if (NULL != index_descr_dxl)
	{
		index = m_pmda->Pmdindex(index_descr_dxl->MDId());
	}

	// get oid for table
	Oid oid = CMDIdGPDB::CastMdid(table_descr->MDId())->OidObjectId();
	GPOS_ASSERT(InvalidOid != oid);

	prte->relid = oid;
	prte->checkAsUser = table_descr->GetExecuteAsUserId();
	prte->requiredPerms |= ACL_NO_RIGHTS;

	// save oid and range index in translation context
	pdxltrctxbtOut->SetOID(oid);
	pdxltrctxbtOut->SetRelIndex(iRel);

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

		INT iAttno = dxl_col_descr->AttrNum();

		GPOS_ASSERT(0 != iAttno);

		if (0 < iAttno)
		{
			// if iAttno > iLastAttno + 1, there were dropped attributes
			// add those to the RTE as they are required by GPDB
			for (INT iDroppedColAttno = iLastAttno + 1; iDroppedColAttno < iAttno; iDroppedColAttno++)
			{
				Value *pvalDroppedColName = gpdb::PvalMakeString(PStrDup(""));
				palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalDroppedColName);
			}
			
			// non-system attribute
			CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_col_descr->MdName()->GetMDName()->GetBuffer());
			Value *pvalColName = gpdb::PvalMakeString(col_name_char_array);

			palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);
			iLastAttno = iAttno;
		}

		// get the attno from the index, in case of indexonlyscan
		if (NULL != index)
		{
			iAttno = 1 + index->GetKeyPos((ULONG) iAttno - 1);
		}

		// save mapping col id -> index in translate context
		(void) pdxltrctxbtOut->InsertMapping(dxl_col_descr->Id(), iAttno);
	}

	// if there are any dropped columns at the end, add those too to the RangeTblEntry
	for (ULONG ul = iLastAttno + 1; ul <= ulRelColumns; ul++)
	{
		Value *pvalDroppedColName = gpdb::PvalMakeString(PStrDup(""));
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalDroppedColName);
	}
	
	prte->eref = palias;

	return prte;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlTargetListFromProjList
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
CTranslatorDXLToPlStmt::PlTargetListFromProjList
	(
	const CDXLNode *project_list_dxl,
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	CDXLTranslateContext *pdxltrctxOut
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
																pdxltrctxbt,
																pdrgpdxltrctx,
																pdxltrctxOut,
																m_pctxdxltoplstmt
																);

		Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnExpr, &mapcidvarplstmt);

		GPOS_ASSERT(NULL != pexpr);

		TargetEntry *target_entry = MakeNode(TargetEntry);
		target_entry->expr = pexpr;
		target_entry->resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlopPrel->GetMdNameAlias()->GetMDName()->GetBuffer());
		target_entry->resno = (AttrNumber) (ul + 1);

		if (IsA(pexpr, Var))
		{
			// check the origin of the left or the right side
			// of the current operator and if it is derived from a base relation,
			// set resorigtbl and resorigcol appropriately

			if (NULL != pdxltrctxbt)
			{
				// translating project list of a base table
				target_entry->resorigtbl = pdxltrctxbt->GetOid();
				target_entry->resorigcol = ((Var *) pexpr)->varattno;
			}
			else
			{
				// not translating a base table proj list: variable must come from
				// the left or right child of the operator

				GPOS_ASSERT(NULL != pdrgpdxltrctx);
				GPOS_ASSERT(0 != pdrgpdxltrctx->Size());
				ULONG col_id = CDXLScalarIdent::Cast(pdxlnExpr->GetOperator())->MakeDXLColRef()->Id();

				const CDXLTranslateContext *pdxltrctxLeft = (*pdrgpdxltrctx)[0];
				GPOS_ASSERT(NULL != pdxltrctxLeft);
				const TargetEntry *pteOriginal = pdxltrctxLeft->GetTargetEntry(col_id);

				if (NULL == pteOriginal)
				{
					// variable not found on the left side
					GPOS_ASSERT(2 == pdrgpdxltrctx->Size());
					const CDXLTranslateContext *pdxltrctxRight = (*pdrgpdxltrctx)[1];

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
		pdxltrctxOut->InsertMapping(pdxlopPrel->Id(), target_entry);

		target_list = gpdb::PlAppendElement(target_list, target_entry);
	}

	return target_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlTargetListWithDroppedCols
//
//	@doc:
//		Construct the target list for a DML statement by adding NULL elements
//		for dropped columns
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlTargetListWithDroppedCols
	(
	List *target_list,
	const IMDRelation *pmdrel
	)
{
	GPOS_ASSERT(NULL != target_list);
	GPOS_ASSERT(gpdb::ListLength(target_list) <= pmdrel->ColumnCount());

	List *plResult = NIL;
	ULONG ulLastTLElem = 0;
	ULONG ulResno = 1;
	
	const ULONG ulRelCols = pmdrel->ColumnCount();
	
	for (ULONG ul = 0; ul < ulRelCols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		
		if (pmdcol->IsSystemColumn())
		{
			continue;
		}
		
		Expr *pexpr = NULL;	
		if (pmdcol->IsDropped())
		{
			// add a NULL element
			OID oidType = CMDIdGPDB::CastMdid(m_pmda->PtMDType<IMDTypeInt4>()->MDId())->OidObjectId();

			pexpr = (Expr *) gpdb::PnodeMakeNULLConst(oidType);
		}
		else
		{
			TargetEntry *target_entry = (TargetEntry *) gpdb::PvListNth(target_list, ulLastTLElem);
			pexpr = (Expr *) gpdb::PvCopyObject(target_entry->expr);
			ulLastTLElem++;
		}
		
		CHAR *szName = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pmdcol->Mdname().GetMDName()->GetBuffer());
		TargetEntry *pteNew = gpdb::PteMakeTargetEntry(pexpr, ulResno, szName, false /*resjunk*/);
		plResult = gpdb::PlAppendElement(plResult, pteNew);
		ulResno++;
	}
	
	return plResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlTargetListForHashNode
//
//	@doc:
//		Create a target list for the hash node of a hash join plan node by creating a list
//		of references to the elements in the child project list
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlTargetListForHashNode
	(
	const CDXLNode *project_list_dxl,
	CDXLTranslateContext *pdxltrctxChild,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	List *target_list = NIL;
	const ULONG arity = project_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnPrEl = (*project_list_dxl)[ul];
		CDXLScalarProjElem *pdxlopPrel = CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());

		const TargetEntry *pteChild = pdxltrctxChild->GetTargetEntry(pdxlopPrel->Id());
		if (NULL  == pteChild)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, pdxlopPrel->Id());
		}	

		// get type oid for project element's expression
		GPOS_ASSERT(1 == pdxlnPrEl->Arity());

		// find column type
		OID oidType = gpdb::OidExprType((Node*) pteChild->expr);
		INT type_modifier = gpdb::IExprTypeMod((Node *) pteChild->expr);

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
		Var *var = gpdb::PvarMakeVar
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

		TargetEntry *target_entry = gpdb::PteMakeTargetEntry
							(
							(Expr *) var,
							(AttrNumber) (ul + 1),
							szResname,
							false		// resjunk
							);

		target_list = gpdb::PlAppendElement(target_list, target_entry);
		pdxltrctxOut->InsertMapping(pdxlopPrel->Id(), target_entry);
	}

	return target_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlQualFromFilter
//
//	@doc:
//		Translates a DXL filter node into a Qual list.
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlQualFromFilter
	(
	const CDXLNode * filter_dxlnode,
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	const ULONG arity = filter_dxlnode->Arity();
	if (0 == arity)
	{
		return NIL;
	}

	GPOS_ASSERT(1 == arity);

	CDXLNode *pdxlnFilterCond = (*filter_dxlnode)[0];
	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(pdxlnFilterCond, m_pmda));

	return PlQualFromScalarCondNode(pdxlnFilterCond, pdxltrctxbt, pdrgpdxltrctx, pdxltrctxOut);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlQualFromScalarCondNode
//
//	@doc:
//		Translates a DXL scalar condition node node into a Qual list.
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlQualFromScalarCondNode
	(
	const CDXLNode *pdxlnCond,
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	List *plQuals = NIL;

	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(const_cast<CDXLNode*>(pdxlnCond), m_pmda));

	CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
															(
															m_memory_pool,
															pdxltrctxbt,
															pdrgpdxltrctx,
															pdxltrctxOut,
															m_pctxdxltoplstmt
															);

	Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar
					(
					pdxlnCond,
					&mapcidvarplstmt
					);

	plQuals = gpdb::PlAppendElement(plQuals, pexpr);

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
	const CDXLOperatorCost *pdxlopcost,
	Cost *pcostStartupOut,
	Cost *pcostTotalOut,
	Cost *pcostRowsOut,
	INT * piWidthOut
	)
{
	*pcostStartupOut = CostFromStr(pdxlopcost->GetStartUpCostStr());
	*pcostTotalOut = CostFromStr(pdxlopcost->GetTotalCostStr());
	*pcostRowsOut = CostFromStr(pdxlopcost->GetRowsOutStr());
	*piWidthOut = CTranslatorUtils::GetIntFromStr(pdxlopcost->GetWidthStr());
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
	const CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctx,
	List **pplTargetListOut,
	List **pplQualOut,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	// translate proj list
	*pplTargetListOut = PlTargetListFromProjList
						(
						project_list_dxl,
						pdxltrctxbt,		// base table translation context
						pdrgpdxltrctx,
						pdxltrctxOut
						);

	// translate filter
	*pplQualOut = PlQualFromFilter
					(
					filter_dxlnode,
					pdxltrctxbt,			// base table translation context
					pdrgpdxltrctx,
					pdxltrctxOut
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
	const CDXLNode *pdxlnHashExprList,
	const CDXLTranslateContext *pdxltrctxChild,
	List **pplHashExprOut,
	List **pplHashExprTypesOut,
	CDXLTranslateContext *pdxltrctxOut
	)
{
	GPOS_ASSERT(NIL == *pplHashExprOut);
	GPOS_ASSERT(NIL == *pplHashExprTypesOut);

	List *plHashExpr = NIL;
	List *plHashExprTypes = NIL;

	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(pdxltrctxChild);

	const ULONG arity = pdxlnHashExprList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnHashExpr = (*pdxlnHashExprList)[ul];
		CDXLScalarHashExpr *pdxlopHashExpr = CDXLScalarHashExpr::Cast(pdxlnHashExpr->GetOperator());

		// the type of the hash expression in GPDB is computed as the left operand 
		// of the equality operator of the actual hash expression type
		const IMDType *pmdtype = m_pmda->Pmdtype(pdxlopHashExpr->MDIdType());
		const IMDScalarOp *md_scalar_op = m_pmda->Pmdscop(pmdtype->GetMdidForCmpType(IMDType::EcmptEq));
		
		const IMDId *pmdidHashType = md_scalar_op->GetLeftMdid();
		
		plHashExprTypes = gpdb::PlAppendOid(plHashExprTypes, CMDIdGPDB::CastMdid(pmdidHashType)->OidObjectId());

		GPOS_ASSERT(1 == pdxlnHashExpr->Arity());
		CDXLNode *pdxlnExpr = (*pdxlnHashExpr)[0];

		CMappingColIdVarPlStmt mapcidvarplstmt = CMappingColIdVarPlStmt
																(
																m_memory_pool,
																NULL,
																pdrgpdxltrctx,
																pdxltrctxOut,
																m_pctxdxltoplstmt
																);

		Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnExpr, &mapcidvarplstmt);

		plHashExpr = gpdb::PlAppendElement(plHashExpr, pexpr);

		GPOS_ASSERT((ULONG) gpdb::ListLength(plHashExpr) == ul + 1);
	}


	*pplHashExprOut = plHashExpr;
	*pplHashExprTypesOut = plHashExprTypes;

	// cleanup
	pdrgpdxltrctx->Release();
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
	const CDXLTranslateContext *pdxltrctxChild,
	AttrNumber *pattnoSortColIds,
	Oid *poidSortOpIds,
	Oid *poidSortCollations,
	bool *pboolNullsFirst
	)
{
	const ULONG arity = sort_col_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnSortCol = (*sort_col_list_dxl)[ul];
		CDXLScalarSortCol *pdxlopSortCol = CDXLScalarSortCol::Cast(pdxlnSortCol->GetOperator());

		ULONG ulSortColId = pdxlopSortCol->GetColId();
		const TargetEntry *pteSortCol = pdxltrctxChild->GetTargetEntry(ulSortColId);
		if (NULL  == pteSortCol)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, ulSortColId);
		}	

		pattnoSortColIds[ul] = pteSortCol->resno;
		poidSortOpIds[ul] = CMDIdGPDB::CastMdid(pdxlopSortCol->GetMdIdSortOp())->OidObjectId();
		if (poidSortCollations)
		{
			poidSortCollations[ul] = gpdb::OidExprCollation((Node *) pteSortCol->expr);
		}
		pboolNullsFirst[ul] = pdxlopSortCol->IsSortedNullsFirst();
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
//		CTranslatorDXLToPlStmt::FTargetTableDistributed
//
//	@doc:
//		Check if given operator is a DML on a distributed table
//
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToPlStmt::FTargetTableDistributed
	(
	CDXLOperator *pdxlop
	)
{
	if (EdxlopPhysicalDML != pdxlop->GetDXLOperator())
	{
		return false;
	}

	CDXLPhysicalDML *pdxlopDML = CDXLPhysicalDML::Cast(pdxlop);
	IMDId *pmdid = pdxlopDML->GetDXLTableDescr()->MDId();

	return IMDRelation::EreldistrMasterOnly != m_pmda->Pmdrel(pmdid)->GetRelDistribution();
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
CTranslatorDXLToPlStmt::UlAddTargetEntryForColId
	(
	List **pplTargetList,
	CDXLTranslateContext *pdxltrctx,
	ULONG col_id,
	BOOL fResjunk
	)
{
	GPOS_ASSERT(NULL != pplTargetList);
	
	const TargetEntry *target_entry = pdxltrctx->GetTargetEntry(col_id);
	
	if (NULL == target_entry)
	{
		// colid not found in translate context
		return 0;
	}
	
	// TODO: Oct 29, 2012; see if entry already exists in the target list
	
	OID oidExpr = gpdb::OidExprType((Node*) target_entry->expr);
	INT type_modifier = gpdb::IExprTypeMod((Node *) target_entry->expr);
	Var *var = gpdb::PvarMakeVar
						(
						OUTER,
						target_entry->resno,
						oidExpr,
						type_modifier,
						0	// varlevelsup
						);
	ULONG ulResNo = gpdb::ListLength(*pplTargetList) + 1;
	CHAR *szResName = PStrDup(target_entry->resname);
	TargetEntry *pteNew = gpdb::PteMakeTargetEntry((Expr*) var, ulResNo, szResName, fResjunk);
	*pplTargetList = gpdb::PlAppendElement(*pplTargetList, pteNew);
	
	return target_entry->resno;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::JtFromEdxljt
//
//	@doc:
//		Translates the join type from its DXL representation into the GPDB one
//
//---------------------------------------------------------------------------
JoinType
CTranslatorDXLToPlStmt::JtFromEdxljt
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
//		CTranslatorDXLToPlStmt::PplanCTAS
//
//	@doc:
//		Sets the vartypmod fields in the target entries of the given target list
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::SetVarTypMod
	(
	const CDXLPhysicalCTAS *pdxlop,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != target_list);

	IntPtrArray *pdrgpi = pdxlop->GetVarTypeModArray();
	GPOS_ASSERT(pdrgpi->Size() == gpdb::ListLength(target_list));

	ULONG ul = 0;
	ListCell *lc = NULL;
	ForEach (lc, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc);
		GPOS_ASSERT(IsA(target_entry, TargetEntry));

		if (IsA(target_entry->expr, Var))
		{
			Var *var = (Var*) target_entry->expr;
			var->vartypmod = *(*pdrgpi)[ul];
		}
		++ul;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanCTAS
//
//	@doc:
//		Translates a DXL CTAS node 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanCTAS
	(
	const CDXLNode *pdxlnCTAS,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	CDXLPhysicalCTAS *pdxlop = CDXLPhysicalCTAS::Cast(pdxlnCTAS->GetOperator());
	CDXLNode *project_list_dxl = (*pdxlnCTAS)[0];
	CDXLNode *pdxlnChild = (*pdxlnCTAS)[1];

	GPOS_ASSERT(NULL == pdxlop->GetDxlCtasStorageOption()->GetDXLCtasOptionArray());
	
	CDXLTranslateContext dxltrctxChild(m_memory_pool, false, pdxltrctxOut->GetColIdToParamIdMap());

	Plan *pplan = PplFromDXL(pdxlnChild, &dxltrctxChild, pdrgpdxltrctxPrevSiblings);
	
	// fix target list to match the required column names
	DrgPdxltrctx *pdrgpdxltrctx = GPOS_NEW(m_memory_pool) DrgPdxltrctx(m_memory_pool);
	pdrgpdxltrctx->Append(&dxltrctxChild);
	
	List *target_list = PlTargetListFromProjList
						(
						project_list_dxl,
						NULL,		// pdxltrctxbt
						pdrgpdxltrctx,
						pdxltrctxOut
						);
	SetVarTypMod(pdxlop, target_list);
	
	SetParamIds(pplan);

	// cleanup
	pdrgpdxltrctx->Release();


	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnCTAS->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	IntoClause *pintocl = PintoclFromCtas(pdxlop);
	GpPolicy *pdistrpolicy = PdistrpolicyFromCtas(pdxlop);
	m_pctxdxltoplstmt->AddCtasInfo(pintocl, pdistrpolicy);
	
	GPOS_ASSERT(IMDRelation::EreldistrMasterOnly != pdxlop->Ereldistrpolicy());
	
	m_fTargetTableDistributed = true;
	
	// Add a result node on top with the correct projection list
	Result *presult = MakeNode(Result);
	Plan *pplanResult = &(presult->plan);
	pplanResult->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplanResult->nMotionNodes = pplan->nMotionNodes;
	pplanResult->lefttree = pplan;

	pplanResult->targetlist = target_list;
	SetParamIds(pplanResult);

	pplan = (Plan *) presult;

	return (Plan *) pplan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PintoclFromCtas
//
//	@doc:
//		Translates a DXL CTAS into clause 
//
//---------------------------------------------------------------------------
IntoClause *
CTranslatorDXLToPlStmt::PintoclFromCtas
	(
	const CDXLPhysicalCTAS *pdxlop
	)
{
	IntoClause *pintocl = MakeNode(IntoClause);
	pintocl->rel = MakeNode(RangeVar);
	/* GPDB_91_MERGE_FIXME: what about unlogged? */
	pintocl->rel->relpersistence = pdxlop->IsTemporary() ? RELPERSISTENCE_TEMP : RELPERSISTENCE_PERMANENT;
	pintocl->rel->relname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlop->MdName()->GetMDName()->GetBuffer());
	pintocl->rel->schemaname = NULL;
	if (NULL != pdxlop->GetMdNameSchema())
	{
		pintocl->rel->schemaname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlop->GetMdNameSchema()->GetMDName()->GetBuffer());
	}
	
	CDXLCtasStorageOptions *pdxlctasopt = pdxlop->GetDxlCtasStorageOption();
	if (NULL != pdxlctasopt->GetMdNameTableSpace())
	{
		pintocl->tableSpaceName = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pdxlop->GetDxlCtasStorageOption()->GetMdNameTableSpace()->GetMDName()->GetBuffer());
	}
	
	pintocl->onCommit = (OnCommitAction) pdxlctasopt->GetOnCommitAction();
	pintocl->options = PlCtasOptions(pdxlctasopt->GetDXLCtasOptionArray());
	
	// get column names
	ColumnDescrDXLArray *pdrgpdxlcd = pdxlop->GetColumnDescrDXLArray();
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
		pcoldef->collOid = gpdb::OidTypeCollation(CMDIdGPDB::CastMdid(dxl_col_descr->MDIdType())->OidObjectId());
		pintocl->colNames = gpdb::PlAppendElement(pintocl->colNames, pcoldef);

	}

	return pintocl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PdistrpolicyFromCtas
//
//	@doc:
//		Translates distribution policy given by a physical CTAS operator 
//
//---------------------------------------------------------------------------
GpPolicy *
CTranslatorDXLToPlStmt::PdistrpolicyFromCtas
	(
	const CDXLPhysicalCTAS *pdxlop
	)
{
	ULongPtrArray *pdrgpulDistrCols = pdxlop->GetDistrColPosArray();

	const ULONG ulDistrCols = (pdrgpulDistrCols == NULL) ? 0 : pdrgpulDistrCols->Size();

	ULONG ulDistrColsAlloc = 1;
	if (0 < ulDistrCols)
	{
		ulDistrColsAlloc = ulDistrCols;
	}
	
	GpPolicy *pdistrpolicy = gpdb::PMakeGpPolicy(NULL, POLICYTYPE_PARTITIONED, ulDistrColsAlloc);

	GPOS_ASSERT(IMDRelation::EreldistrHash == pdxlop->Ereldistrpolicy() ||
				IMDRelation::EreldistrRandom == pdxlop->Ereldistrpolicy());
	
	pdistrpolicy->ptype = POLICYTYPE_PARTITIONED;
	pdistrpolicy->nattrs = 0;
	if (IMDRelation::EreldistrHash == pdxlop->Ereldistrpolicy())
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
//		CTranslatorDXLToPlStmt::PlCtasOptions
//
//	@doc:
//		Translates CTAS options
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::PlCtasOptions
	(
	CDXLCtasStorageOptions::DXLCtasOptionArray *pdrgpctasopt
	)
{
	if (NULL == pdrgpctasopt)
	{
		return NIL;
	}
	
	const ULONG ulOptions = pdrgpctasopt->Size();
	List *plOptions = NIL;
	for (ULONG ul = 0; ul < ulOptions; ul++)
	{
		CDXLCtasStorageOptions::CDXLCtasOption *pdxlopt = (*pdrgpctasopt)[ul];
		CWStringBase *pstrName = pdxlopt->m_str_name;
		CWStringBase *pstrValue = pdxlopt->m_str_value;
		DefElem *pdefelem = MakeNode(DefElem);
		pdefelem->defname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(pstrName->GetBuffer());

		if (!pdxlopt->m_is_null)
		{
			NodeTag argType = (NodeTag) pdxlopt->m_type;

			GPOS_ASSERT(T_Integer == argType || T_String == argType);
			if (T_Integer == argType)
			{
				pdefelem->arg = (Node *) gpdb::PvalMakeInteger(CTranslatorUtils::GetLongFromStr(pstrValue));
			}
			else
			{
				pdefelem->arg = (Node *) gpdb::PvalMakeString(CTranslatorUtils::CreateMultiByteCharStringFromWCString(pstrValue->GetBuffer()));
			}
		}

		plOptions = gpdb::PlAppendElement(plOptions, pdefelem);
	}
	
	return plOptions;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanBitmapTableScan
//
//	@doc:
//		Translates a DXL bitmap table scan node into a BitmapTableScan node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanBitmapTableScan
	(
	const CDXLNode *pdxlnBitmapScan,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	ULONG ulPartIndex = INVALID_PART_INDEX;
	ULONG ulPartIndexPrintable = INVALID_PART_INDEX;
	const CDXLTableDescr *table_descr = NULL;
	BOOL fDynamic = false;

	CDXLOperator *pdxlop = pdxlnBitmapScan->GetOperator();
	if (EdxlopPhysicalBitmapTableScan == pdxlop->GetDXLOperator())
	{
		table_descr = CDXLPhysicalBitmapTableScan::Cast(pdxlop)->GetDXLTableDescr();
	}
	else
	{
		GPOS_ASSERT(EdxlopPhysicalDynamicBitmapTableScan == pdxlop->GetDXLOperator());
		CDXLPhysicalDynamicBitmapTableScan *pdxlopDynamic =
				CDXLPhysicalDynamicBitmapTableScan::Cast(pdxlop);
		table_descr = pdxlopDynamic->GetDXLTableDescr();

		ulPartIndex = pdxlopDynamic->GetPartIndexId();
		ulPartIndexPrintable = pdxlopDynamic->GetPartIndexIdPrintable();
		fDynamic = true;
	}

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	const IMDRelation *pmdrel = m_pmda->Pmdrel(table_descr->MDId());

	RangeTblEntry *prte = PrteFromTblDescr(table_descr, NULL /*index_descr_dxl*/, iRel, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);
	prte->requiredPerms |= ACL_SELECT;

	m_pctxdxltoplstmt->AddRTE(prte);

	BitmapTableScan *pdbts = MakeNode(BitmapTableScan);
	pdbts->scan.scanrelid = iRel;
	pdbts->scan.partIndex = ulPartIndex;
	pdbts->scan.partIndexPrintable = ulPartIndexPrintable;

	Plan *pplan = &(pdbts->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnBitmapScan->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
		);

	GPOS_ASSERT(4 == pdxlnBitmapScan->Arity());

	// translate proj list and filter
	CDXLNode *project_list_dxl = (*pdxlnBitmapScan)[0];
	CDXLNode *filter_dxlnode = (*pdxlnBitmapScan)[1];
	CDXLNode *pdxlnRecheckCond = (*pdxlnBitmapScan)[2];
	CDXLNode *pdxlnBitmapAccessPath = (*pdxlnBitmapScan)[3];

	List *plQuals = NULL;
	TranslateProjListAndFilter
		(
		project_list_dxl,
		filter_dxlnode,
		&dxltrctxbt,	// translate context for the base table
		pdrgpdxltrctxPrevSiblings,
		&pplan->targetlist,
		&plQuals,
		pdxltrctxOut
		);
	pplan->qual = plQuals;

	pdbts->bitmapqualorig = PlQualFromFilter
							(
							pdxlnRecheckCond,
							&dxltrctxbt,
							pdrgpdxltrctxPrevSiblings,
							pdxltrctxOut
							);

	pdbts->scan.plan.lefttree = PplanBitmapAccessPath
								(
								pdxlnBitmapAccessPath,
								pdxltrctxOut,
								pmdrel,
								table_descr,
								&dxltrctxbt,
								pdrgpdxltrctxPrevSiblings,
								pdbts
								);
	SetParamIds(pplan);

	return (Plan *) pdbts;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanBitmapAccessPath
//
//	@doc:
//		Translate the tree of bitmap index operators that are under the given
//		(dynamic) bitmap table scan.
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanBitmapAccessPath
	(
	const CDXLNode *pdxlnBitmapAccessPath,
	CDXLTranslateContext *pdxltrctxOut,
	const IMDRelation *pmdrel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
	BitmapTableScan *pdbts
	)
{
	Edxlopid edxlopid = pdxlnBitmapAccessPath->GetOperator()->GetDXLOperator();
	if (EdxlopScalarBitmapIndexProbe == edxlopid)
	{
		return PplanBitmapIndexProbe
				(
				pdxlnBitmapAccessPath,
				pdxltrctxOut,
				pmdrel,
				table_descr,
				pdxltrctxbt,
				pdrgpdxltrctxPrevSiblings,
				pdbts
				);
	}
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == edxlopid);

	return PplanBitmapBoolOp
			(
			pdxlnBitmapAccessPath, 
			pdxltrctxOut, 
			pmdrel, 
			table_descr,
			pdxltrctxbt,
			pdrgpdxltrctxPrevSiblings,
			pdbts
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToScalar::PplanBitmapBoolOp
//
//	@doc:
//		Translates a DML bitmap bool op expression 
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanBitmapBoolOp
	(
	const CDXLNode *pdxlnBitmapBoolOp,
	CDXLTranslateContext *pdxltrctxOut,
	const IMDRelation *pmdrel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
	BitmapTableScan *pdbts
	)
{
	GPOS_ASSERT(NULL != pdxlnBitmapBoolOp);
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == pdxlnBitmapBoolOp->GetOperator()->GetDXLOperator());

	CDXLScalarBitmapBoolOp *pdxlop = CDXLScalarBitmapBoolOp::Cast(pdxlnBitmapBoolOp->GetOperator());
	
	CDXLNode *pdxlnLeft = (*pdxlnBitmapBoolOp)[0];
	CDXLNode *pdxlnRight = (*pdxlnBitmapBoolOp)[1];
	
	Plan *pplanLeft = PplanBitmapAccessPath
						(
						pdxlnLeft,
						pdxltrctxOut,
						pmdrel,
						table_descr,
						pdxltrctxbt,
						pdrgpdxltrctxPrevSiblings,
						pdbts
						);
	Plan *pplanRight = PplanBitmapAccessPath
						(
						pdxlnRight,
						pdxltrctxOut,
						pmdrel,
						table_descr,
						pdxltrctxbt,
						pdrgpdxltrctxPrevSiblings,
						pdbts
						);
	List *plChildPlans = ListMake2(pplanLeft, pplanRight);

	Plan *pplan = NULL;
	
	if (CDXLScalarBitmapBoolOp::EdxlbitmapAnd == pdxlop->GetDXLBitmapOpType())
	{
		BitmapAnd *bitmapand = MakeNode(BitmapAnd);
		bitmapand->bitmapplans = plChildPlans;
		bitmapand->plan.targetlist = NULL;
		bitmapand->plan.qual = NULL;
		pplan = (Plan *) bitmapand;
	}
	else
	{
		BitmapOr *bitmapor = MakeNode(BitmapOr);
		bitmapor->bitmapplans = plChildPlans;
		bitmapor->plan.targetlist = NULL;
		bitmapor->plan.qual = NULL;
		pplan = (Plan *) bitmapor;
	}
	
	
	return pplan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PplanBitmapIndexProbe
//
//	@doc:
//		Translate CDXLScalarBitmapIndexProbe into a BitmapIndexScan
//		or a DynamicBitmapIndexScan
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::PplanBitmapIndexProbe
	(
	const CDXLNode *pdxlnBitmapIndexProbe,
	CDXLTranslateContext *pdxltrctxOut,
	const IMDRelation *pmdrel,
	const CDXLTableDescr *table_descr,
	CDXLTranslateContextBaseTable *pdxltrctxbt,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings,
	BitmapTableScan *pdbts
	)
{
	CDXLScalarBitmapIndexProbe *pdxlopScalar =
			CDXLScalarBitmapIndexProbe::Cast(pdxlnBitmapIndexProbe->GetOperator());

	BitmapIndexScan *pbis;
	DynamicBitmapIndexScan *pdbis;

	if (pdbts->scan.partIndex)
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
	pbis->scan.scanrelid = pdbts->scan.scanrelid;
	pbis->scan.partIndex = pdbts->scan.partIndex;

	CMDIdGPDB *pmdidIndex = CMDIdGPDB::CastMdid(pdxlopScalar->GetDXLIndexDescr()->MDId());
	const IMDIndex *index = m_pmda->Pmdindex(pmdidIndex);
	Oid oidIndex = pmdidIndex->OidObjectId();

	GPOS_ASSERT(InvalidOid != oidIndex);
	pbis->indexid = oidIndex;
	OID oidRel = CMDIdGPDB::CastMdid(table_descr->MDId())->OidObjectId();
	Plan *pplan = &(pbis->scan.plan);
	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	GPOS_ASSERT(1 == pdxlnBitmapIndexProbe->Arity());
	CDXLNode *pdxlnIndexCondList = (*pdxlnBitmapIndexProbe)[0];
	List *plIndexConditions = NIL;
	List *plIndexOrigConditions = NIL;
	List *plIndexStratgey = NIL;
	List *plIndexSubtype = NIL;

	TranslateIndexConditions
		(
		pdxlnIndexCondList,
		table_descr,
		false /*fIndexOnlyScan*/,
		index,
		pmdrel,
		pdxltrctxOut,
		pdxltrctxbt,
		pdrgpdxltrctxPrevSiblings,
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
	SetParamIds(pplan);

	/*
	 * If it's a Dynamic Bitmap Index Scan, also fill in the information
	 * about the indexes on the partitions.
	 */
	if (pdbis)
	{
		pdbis->logicalIndexInfo = gpdb::Plgidxinfo(oidRel, oidIndex);
	}

	return pplan;
}

// translates a DXL Value Scan node into a GPDB Value scan node
Plan *
CTranslatorDXLToPlStmt::PplanValueScan
	(
	const CDXLNode *pdxlnValueScan,
	CDXLTranslateContext *pdxltrctxOut,
	DrgPdxltrctx *pdrgpdxltrctxPrevSiblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable dxltrctxbt(m_memory_pool);

	// we will add the new range table entry as the last element of the range table
	Index iRel = gpdb::ListLength(m_pctxdxltoplstmt->GetRTableEntriesList()) + 1;

	dxltrctxbt.SetRelIndex(iRel);

	// create value scan node
	ValuesScan *pvaluescan = MakeNode(ValuesScan);
	pvaluescan->scan.scanrelid = iRel;
	Plan *pplan = &(pvaluescan->scan.plan);

	RangeTblEntry *prte = PrteFromDXLValueScan(pdxlnValueScan, pdxltrctxOut, &dxltrctxbt);
	GPOS_ASSERT(NULL != prte);

	pvaluescan->values_lists = (List *)gpdb::PvCopyObject(prte->values_lists);

	m_pctxdxltoplstmt->AddRTE(prte);

	pplan->plan_node_id = m_pctxdxltoplstmt->GetNextPlanId();
	pplan->nMotionNodes = 0;

	// translate operator costs
	TranslatePlanCosts
	(
		CDXLPhysicalProperties::PdxlpropConvert(pdxlnValueScan->GetProperties())->GetDXLOperatorCost(),
		&(pplan->startup_cost),
		&(pplan->total_cost),
		&(pplan->plan_rows),
		&(pplan->plan_width)
	);

	// a table scan node must have at least 2 children: projection list and at least 1 value list
	GPOS_ASSERT(2 <= pdxlnValueScan->Arity());

	CDXLNode *project_list_dxl = (*pdxlnValueScan)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = PlTargetListFromProjList
							(
							project_list_dxl,
							&dxltrctxbt,
							NULL,
							pdxltrctxOut
							);

	pplan->targetlist = target_list;

	return (Plan *) pvaluescan;
}


// EOF
