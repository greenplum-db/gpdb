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
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/partitionselection.h"
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
	IMemoryPool *mp,
	CMDAccessor *md_accessor,
	CContextDXLToPlStmt* dxl_to_plstmt_context,
	ULONG num_of_segments
	)
	:
	m_mp(mp),
	m_md_accessor(md_accessor),
	m_dxl_to_plstmt_context(dxl_to_plstmt_context),
	m_cmd_type(CMD_SELECT),
	m_is_tgt_tbl_distributed(false),
	m_result_rel_list(NULL),
	m_external_scan_counter(0),
	m_num_of_segments(num_of_segments),
	m_partition_selector_counter(0)
{
	m_translator_dxl_to_scalar = GPOS_NEW(m_mp) CTranslatorDXLToScalar(m_mp, m_md_accessor, m_num_of_segments);
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
	for (ULONG idx = 0; idx < GPOS_ARRAY_SIZE(m_dxlop_translator_func_mapping_array); idx++)
	{
		m_dxlop_translator_func_mapping_array[idx] = NULL;
	}

	// array mapping operator type to translator function
	static const STranslatorMapping dxlop_translator_func_mapping_array[] =
	{
			{EdxlopPhysicalTableScan,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLTblScan},
			{EdxlopPhysicalExternalScan,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLTblScan},
			{EdxlopPhysicalIndexScan,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLIndexScan},
			{EdxlopPhysicalHashJoin, 				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLHashJoin},
			{EdxlopPhysicalNLJoin, 					&gpopt::CTranslatorDXLToPlStmt::TranslateDXLNLJoin},
			{EdxlopPhysicalMergeJoin,				&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMergeJoin},
			{EdxlopPhysicalMotionGather,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMotion},
			{EdxlopPhysicalMotionBroadcast,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLMotion},
			{EdxlopPhysicalMotionRedistribute,		&gpopt::CTranslatorDXLToPlStmt::TranslateDXLDuplicateSensitiveMotion},
			{EdxlopPhysicalMotionRandom,			&gpopt::CTranslatorDXLToPlStmt::TranslateDXLDuplicateSensitiveMotion},
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

	const ULONG num_of_translators = GPOS_ARRAY_SIZE(dxlop_translator_func_mapping_array);

	for (ULONG idx = 0; idx < num_of_translators; idx++)
	{
		STranslatorMapping elem = dxlop_translator_func_mapping_array[idx];
		m_dxlop_translator_func_mapping_array[elem.dxl_op_id] = elem.dxlnode_to_logical_funct;
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

	CDXLTranslateContext dxl_translate_ctxt(m_mp, false);

	CDXLTranslationContextArray *ctxt_translation_prev_siblings = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	Plan *plan = TranslateDXLOperatorToPlan(dxlnode, &dxl_translate_ctxt, ctxt_translation_prev_siblings);
	ctxt_translation_prev_siblings->Release();

	GPOS_ASSERT(NULL != plan);

	// collect oids from rtable
	List *oids_list = NIL;

	ListCell *lc_rte = NULL;
	ForEach (lc_rte, m_dxl_to_plstmt_context->GetRTableEntriesList())
	{
		RangeTblEntry *pRTE = (RangeTblEntry *) lfirst(lc_rte);

		if (pRTE->rtekind == RTE_RELATION)
		{
			oids_list = gpdb::LAppendOid(oids_list, pRTE->relid);
		}
	}

	// assemble planned stmt
	PlannedStmt *planned_stmt = MakeNode(PlannedStmt);
	planned_stmt->planGen = PLANGEN_OPTIMIZER;
	
	planned_stmt->rtable = m_dxl_to_plstmt_context->GetRTableEntriesList();
	planned_stmt->subplans = m_dxl_to_plstmt_context->GetSubplanEntriesList();
	planned_stmt->planTree = plan;

	// store partitioned table indexes in planned stmt
	planned_stmt->queryPartOids = m_dxl_to_plstmt_context->GetPartitionedTablesList();
	planned_stmt->canSetTag = can_set_tag;
	planned_stmt->relationOids = oids_list;
	planned_stmt->numSelectorsPerScanId = m_dxl_to_plstmt_context->GetNumPartitionSelectorsList();

	plan->nMotionNodes  = m_dxl_to_plstmt_context->GetCurrentMotionId()-1;
	planned_stmt->nMotionNodes =  m_dxl_to_plstmt_context->GetCurrentMotionId()-1;

	planned_stmt->commandType = m_cmd_type;
	
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
	
	planned_stmt->resultRelations = m_result_rel_list;
	planned_stmt->intoClause = m_dxl_to_plstmt_context->GetIntoClause();
	planned_stmt->intoPolicy = m_dxl_to_plstmt_context->GetDistributionPolicy();
	
	SetInitPlanVariables(planned_stmt);
	
	if (CMD_SELECT == m_cmd_type && NULL != dxlnode->GetDXLDirectDispatchInfo())
	{
		List *direct_dispatch_segids = TranslateDXLDirectDispatchInfo(dxlnode->GetDXLDirectDispatchInfo());
		plan->directDispatch.contentIds = direct_dispatch_segids;
		plan->directDispatch.isDirectDispatch = (NIL != direct_dispatch_segids);
		
		if (plan->directDispatch.isDirectDispatch)
		{
			List *motion_node_list = gpdb::ExtractNodesPlan(planned_stmt->planTree, T_Motion, true /*descendIntoSubqueries*/);
			ListCell *lc = NULL;
			ForEach(lc, motion_node_list)
			{
				Motion *motion = (Motion *) lfirst(lc);
				GPOS_ASSERT(IsA(motion, Motion));
				GPOS_ASSERT(gpdb::IsMotionGather(motion));
				
				motion->plan.directDispatch.isDirectDispatch = true;
				motion->plan.directDispatch.contentIds = plan->directDispatch.contentIds;
			}
		}
	}
	
	return planned_stmt;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != ctxt_translation_prev_siblings);

	CDXLOperator *dxlop = dxlnode->GetOperator();
	ULONG ulOpId =  (ULONG) dxlop->GetDXLOperator();

	PfPplan dxlnode_to_logical_funct = m_dxlop_translator_func_mapping_array[ulOpId];

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
CTranslatorDXLToPlStmt::SetInitPlanVariables(PlannedStmt* planned_stmt)
{
	if(1 != m_dxl_to_plstmt_context->GetCurrentMotionId()) // For Distributed Tables m_ulMotionId > 1
	{
		planned_stmt->nInitPlans = m_dxl_to_plstmt_context->GetCurrentParamId();
		planned_stmt->planTree->nInitPlans = m_dxl_to_plstmt_context->GetCurrentParamId();
	}

	planned_stmt->nParamExec = m_dxl_to_plstmt_context->GetCurrentParamId();

	// Extract all subplans defined in the planTree
	List *subplan_list = gpdb::ExtractNodesPlan(planned_stmt->planTree, T_SubPlan, true);

	ListCell *lc = NULL;

	ForEach (lc, subplan_list)
	{
		SubPlan *subplan = (SubPlan*) lfirst(lc);
		if (subplan->is_initplan)
		{
			SetInitPlanSliceInformation(subplan);
		}
	}

	// InitPlans can also be defined in subplans. We therefore have to iterate
	// over all the subplans referred to in the planned statement.

	List *initplan_list = planned_stmt->subplans;

	ForEach (lc,initplan_list)
	{
		subplan_list = gpdb::ExtractNodesPlan((Plan*) lfirst(lc), T_SubPlan, true);
		ListCell *lc2;

		ForEach (lc2, subplan_list)
		{
			SubPlan *subplan = (SubPlan*) lfirst(lc2);
			if (subplan->is_initplan)
			{
				SetInitPlanSliceInformation(subplan);
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
CTranslatorDXLToPlStmt::SetInitPlanSliceInformation(SubPlan * subplan)
{
	GPOS_ASSERT(subplan->is_initplan && "This is processed for initplans only");

	if (subplan->is_initplan)
	{
		GPOS_ASSERT(0 < m_dxl_to_plstmt_context->GetCurrentMotionId());

		if(1 < m_dxl_to_plstmt_context->GetCurrentMotionId())
		{
			subplan->qDispSliceId =  m_dxl_to_plstmt_context->GetCurrentMotionId() + subplan->plan_id-1;
		}
		else
		{
			subplan->qDispSliceId = 0;
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
	List *params_node_list = gpdb::ExtractNodesPlan(plan, T_Param, true);

	ListCell *lc = NULL;

	Bitmapset  *bitmapset = NULL;

	ForEach (lc, params_node_list)
	{
		Param *param = (Param*) lfirst(lc);
		bitmapset = gpdb::BmsAddMember(bitmapset, param->paramid);
	}

	plan->extParam = bitmapset;
	plan->allParam = bitmapset;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsFile
//
//	@doc:
//		Segment mapping for tables with LOCATION http:// or file://
//		These two protocols are very similar in that they enforce a 1-URI:1-segdb
//		relationship. The only difference between them is that file:// URI must
//		be assigned to a segdb on a host that is local to that URI.
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsFile
	(
	OID oidRel,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	// extract file path and name from URI strings and assign them a primary segdb

	ExtTableEntry *extentry = gpdb::GetExternalTableEntry(oidRel);

	ListCell *plcLocation = NULL;
	ForEach (plcLocation, extentry->urilocations)
	{
		Value* pvLocation = (Value *)lfirst(plcLocation);
		CHAR *szUri = pvLocation->val.str;

		Uri *pUri = gpdb::ParseExternTableUri(szUri);

		BOOL fCandidateFound = false;
		BOOL fMatchFound = false;

		// try to find a segment database that can handle this uri
		for (int i = 0; i < pcdbCompDB->total_segment_dbs && !fMatchFound; i++)
		{
			CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
			INT iSegInd = pcdbCompDBInfo->segindex;
			if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
			{
				if (URI_FILE == pUri->protocol &&
					0 != gpdb::StrCmpIgnoreCase(pUri->hostname, pcdbCompDBInfo->hostname) &&
					0 != gpdb::StrCmpIgnoreCase(pUri->hostname, pcdbCompDBInfo->address))
				{
					continue;
				}

				fCandidateFound = true;
				if (NULL == rgszSegFileMap[iSegInd])
				{
					rgszSegFileMap[iSegInd] = PStrDup(szUri);
					fMatchFound = true;
				}
			}
		}

		if (!fMatchFound)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
					GPOS_WSZ_LIT("Could not assign a segment database for external file"));
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsFdist
//
//	@doc:
// 		Segment mapping for tables with LOCATION gpfdist(s):// or custom protocol
//		The user supplied gpfdist(s):// URIs are duplicated so that there is one
//		available to every segdb. However, in some cases (as determined by
//		gp_external_max_segs GUC) we don't want to use *all* segdbs but instead
//		figure out how many and pick them randomly (for better performance)
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsFdist
	(
	OID oidRel,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB,
	Uri *pUri,
	const ULONG ulTotalPrimaries
	)
{
	ULONG ulParticipatingSegments = ulTotalPrimaries;
	ULONG ulMaxParticipants = ulParticipatingSegments;

	ExtTableEntry *extentry = gpdb::GetExternalTableEntry(oidRel);

	const ULONG ulLocations = gpdb::ListLength(extentry->urilocations);
	if (URI_GPFDIST == pUri->protocol || URI_GPFDISTS == pUri->protocol)
	{
		ulMaxParticipants = ulLocations * gp_external_max_segs;
	}

	ULONG ulSkip = 0;
	BOOL fSkipRandomly = false;
	if (ulParticipatingSegments > ulMaxParticipants)
	{
		ulSkip = ulParticipatingSegments - ulMaxParticipants;
		ulParticipatingSegments = ulMaxParticipants;
		fSkipRandomly = true;
	}

	if (ulLocations > ulParticipatingSegments)
	{
		// This should match the same error in createplan.c
		char msgbuf[200];

		snprintf(msgbuf, sizeof(msgbuf),
				 "There are more external files (URLs) than primary segments that can read them. Found %d URLs and %d primary segments.",
				 ulLocations, ulParticipatingSegments);

		GpdbEreport(ERRCODE_INVALID_TABLE_DEFINITION, // errcode
					   ERROR,
					   msgbuf, // errmsg
					   NULL);  // errhint
	}

	BOOL fDone = false;
	List *plModifiedLocations = NIL;
	ULONG ulModifiedLocations = 0;
	while (!fDone)
	{
		ListCell *plcLocation = NULL;
		ForEach (plcLocation, extentry->urilocations)
		{
			Value* pvLocation = (Value *)lfirst(plcLocation);
			CHAR *szUri = pvLocation->val.str;
			plModifiedLocations = gpdb::LAppend(plModifiedLocations, gpdb::MakeStringValue(szUri));
			ulModifiedLocations ++;

			if (ulModifiedLocations == ulParticipatingSegments)
			{
				fDone = true;
				break;
			}

			if (ulModifiedLocations > ulParticipatingSegments)
			{
				GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
						GPOS_WSZ_LIT("External scan location list failed building distribution"));
			}
		}
	}

	BOOL *rgfSkipMap = NULL;
	if (fSkipRandomly)
	{
		rgfSkipMap = gpdb::ConstructRandomSegMap(ulTotalPrimaries, ulSkip);
	}

	// assign each URI from the new location list a primary segdb
	ListCell *plc = NULL;
	ForEach (plc, plModifiedLocations)
	{
		const CHAR *szUri = (CHAR *) strVal(lfirst(plc));

		BOOL fCandidateFound = false;
		BOOL fMatchFound = false;

		for (int i = 0; i < pcdbCompDB->total_segment_dbs && !fMatchFound; i++)
		{
			CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
			INT iSegInd = pcdbCompDBInfo->segindex;

			if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
			{
				if (fSkipRandomly)
				{
					GPOS_ASSERT(iSegInd < (INT) ulTotalPrimaries);
					if (rgfSkipMap[iSegInd])
					{
						continue;
					}
				}

				fCandidateFound = true;
				if (NULL == rgszSegFileMap[iSegInd])
				{
					rgszSegFileMap[iSegInd] = PStrDup(szUri);
					fMatchFound = true;
				}
			}
		}

		if (!fMatchFound)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
					GPOS_WSZ_LIT("Unable to assign segments for gpfdist(s)"));
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecute
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' ON.
//		In here we don't have URI's. We have a single command string and a
//		specification of the segdb granularity it should get executed on (the
//		ON clause). Depending on the ON clause specification we could go many
//		different ways, for example: assign the command to all segdb, or one
//		command per host, or assign to 5 random segments, etc...
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecute
	(
	OID oidRel,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB,
	const ULONG ulTotalPrimaries
	)
{
	ExtTableEntry *extentry = gpdb::GetExternalTableEntry(oidRel);
	CHAR *szCommand = extentry->command;
	const CHAR *szPrefix = "execute:";

	StringInfo si = gpdb::MakeStringInfo();
	gpdb::AppendStringInfo(si, szPrefix, szCommand);
	CHAR *szPrefixedCommand = PStrDup(si->data);

	gpdb::GPDBFree(si->data);
	gpdb::GPDBFree(si);
	si = NULL;

	// get the ON clause (execute location) information
	Value *pvOnClause = (Value *) gpdb::ListNth(extentry->execlocations, 0);
	CHAR *szOnClause = pvOnClause->val.str;

	if (0 == gpos::clib::Strcmp(szOnClause, "ALL_SEGMENTS"))
	{
		MapLocationsExecuteAllSegments(szPrefixedCommand, rgszSegFileMap, pcdbCompDB);
	}
	else if (0 == gpos::clib::Strcmp(szOnClause, "PER_HOST"))
	{
		MapLocationsExecutePerHost(szPrefixedCommand, rgszSegFileMap, pcdbCompDB);
	}
	else if (0 == gpos::clib::Strncmp(szOnClause, "HOST:", gpos::clib::Strlen("HOST:")))
	{
		CHAR *szHostName = szOnClause + gpos::clib::Strlen("HOST:");
		MapLocationsExecuteOneHost(szHostName, szPrefixedCommand, rgszSegFileMap, pcdbCompDB);
	}
	else if (0 == gpos::clib::Strncmp(szOnClause, "SEGMENT_ID:", gpos::clib::Strlen("SEGMENT_ID:")))
	{
		CHAR *pcEnd = NULL;
		INT iTargetSegInd = (INT) gpos::clib::Strtol(szOnClause + gpos::clib::Strlen("SEGMENT_ID:"), &pcEnd, 10);
		MapLocationsExecuteOneSegment(iTargetSegInd, szPrefixedCommand, rgszSegFileMap, pcdbCompDB);
	}
	else if (0 == gpos::clib::Strncmp(szOnClause, "TOTAL_SEGS:", gpos::clib::Strlen("TOTAL_SEGS:")))
	{
		// total n segments selected randomly
		CHAR *pcEnd = NULL;
		ULONG ulSegsToUse = gpos::clib::Strtol(szOnClause + gpos::clib::Strlen("TOTAL_SEGS:"), &pcEnd, 10);

		MapLocationsExecuteRandomSegments(ulSegsToUse, ulTotalPrimaries, szPrefixedCommand, rgszSegFileMap, pcdbCompDB);
	}
	else if (0 == gpos::clib::Strcmp(szOnClause, "MASTER_ONLY"))
	{
		rgszSegFileMap[0] = PStrDup(szPrefixedCommand);
	}
	else
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
				GPOS_WSZ_LIT("Invalid ON clause"));
	}

	gpdb::GPDBFree(szPrefixedCommand);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecuteAllSegments
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' on all segments
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecuteAllSegments
	(
	CHAR *szPrefixedCommand,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		INT iSegInd = pcdbCompDBInfo->segindex;
		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
		{
			rgszSegFileMap[iSegInd] = PStrDup(szPrefixedCommand);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecutePerHost
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' per host
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecutePerHost
	(
	CHAR *szPrefixedCommand,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	List *plVisitedHosts = NIL;
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		INT iSegInd = pcdbCompDBInfo->segindex;
		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
		{
			BOOL fHostTaken = false;
			ListCell *plc = NULL;
			ForEach (plc, plVisitedHosts)
			{
				const CHAR *szHostName = (CHAR *) strVal(lfirst(plc));
				if (0 == gpdb::StrCmpIgnoreCase(szHostName, pcdbCompDBInfo->hostname))
				{
					fHostTaken = true;
					break;
				}
			}

			if (!fHostTaken)
			{
				rgszSegFileMap[iSegInd] = PStrDup(szPrefixedCommand);
				plVisitedHosts = gpdb::LAppend
										(
										plVisitedHosts,
										gpdb::MakeStringValue(PStrDup(pcdbCompDBInfo->hostname))
										);
			}
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecuteOneHost
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' on a given host
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecuteOneHost
	(
	CHAR *szHostName,
	CHAR *szPrefixedCommand,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	BOOL fMatchFound = false;
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		INT iSegInd = pcdbCompDBInfo->segindex;

		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo) &&
			0 == gpdb::StrCmpIgnoreCase(szHostName, pcdbCompDBInfo->hostname))
		{
			rgszSegFileMap[iSegInd] = PStrDup(szPrefixedCommand);
			fMatchFound = true;
		}
	}

	if (!fMatchFound)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
				GPOS_WSZ_LIT("Could not assign a segment database for given command. No valid primary segment was found in the requested host name."));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecuteOneSegment
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' on a given segment
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecuteOneSegment
	(
	INT iTargetSegInd,
	CHAR *szPrefixedCommand,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	BOOL fMatchFound = false;
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		INT iSegInd = pcdbCompDBInfo->segindex;
		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo) && iSegInd == iTargetSegInd)
		{
			rgszSegFileMap[iSegInd] = PStrDup(szPrefixedCommand);
			fMatchFound = true;
		}
	}

	if(!fMatchFound)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
				GPOS_WSZ_LIT("Could not assign a segment database for given command. The requested segment id is not a valid primary segment."));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsExecuteRandomSegments
//
//	@doc:
// 		Segment mapping for tables with EXECUTE 'cmd' on N random segments
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsExecuteRandomSegments
	(
	ULONG ulSegments,
	const ULONG ulTotalPrimaries,
	CHAR *szPrefixedCommand,
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB
	)
{
	if (ulSegments > ulTotalPrimaries)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError,
				GPOS_WSZ_LIT("More segments in table definition than valid primary segments in the database."));
	}

	ULONG ulSkip = ulTotalPrimaries - ulSegments;
	BOOL *rgfSkipMap = gpdb::ConstructRandomSegMap(ulTotalPrimaries, ulSkip);

	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		INT iSegInd = pcdbCompDBInfo->segindex;
		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
		{
			GPOS_ASSERT(iSegInd < (INT) ulTotalPrimaries);
			if (rgfSkipMap[iSegInd])
			{
				continue;
			}
			rgszSegFileMap[iSegInd] = PStrDup(szPrefixedCommand);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::MapLocationsHdfs
//
//	@doc:
// 		Segment mapping for tables with LOCATION gphdfs://
//		The file chuck division and assignment will be done in the external
//		Java program. We simply assign the location to all the segdbs.
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToPlStmt::MapLocationsHdfs
	(
	char **rgszSegFileMap,
	CdbComponentDatabases *pcdbCompDB,
	CHAR *szFirstUri
	)
{
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		rgszSegFileMap[pcdbCompDBInfo->segindex] = PStrDup(szFirstUri);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::PlExternalScanUriList
//
//	@doc:
//		List of URIs for external scan
//
//---------------------------------------------------------------------------
List*
CTranslatorDXLToPlStmt::PlExternalScanUriList
	(
	OID oidRel
	)
{
	ExtTableEntry *extentry = gpdb::GetExternalTableEntry(oidRel);

	if (extentry->iswritable)
	{
		// This should match the same error in createplan.c
		GpdbEreport(ERRCODE_WRONG_OBJECT_TYPE, // errcode
					   ERROR,
					   "cannot read from a WRITABLE external table", // errmsg
					   "Create the table as READABLE instead."); // errhint
	}

	//get the total valid primary segdb count
	CdbComponentDatabases *pcdbCompDB = gpdb::GetComponentDatabases();
	ULONG ulTotalPrimaries = 0;
	for (int i = 0; i < pcdbCompDB->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *pcdbCompDBInfo = &pcdbCompDB->segment_db_info[i];
		if (SEGMENT_IS_ACTIVE_PRIMARY(pcdbCompDBInfo))
		{
			ulTotalPrimaries++;
		}
	}

	char **rgszSegFileMap = NULL;
    rgszSegFileMap = (char **) gpdb::GPDBAlloc(ulTotalPrimaries * sizeof(char *));
    gpos::clib::Memset(rgszSegFileMap, 0, ulTotalPrimaries * sizeof(char *));

	// is this an EXECUTE table or a LOCATION (URI) table
	BOOL fUsingExecute = false;
	BOOL fUsingLocation = false;
	const CHAR *szCommand = extentry->command;
	if (NULL != szCommand)
	{
		if (!gp_external_enable_exec)
		{
			// This should match the same error in createplan.c
			GpdbEreport(ERRCODE_GP_FEATURE_NOT_CONFIGURED, // errcode
						   ERROR,
						   "Using external tables with OS level commands (EXECUTE clause) is disabled", // errmsg
						   "To enable set gp_external_enable_exec=on"); // errhint
		}
		fUsingExecute = true;
	}
	else
	{
		fUsingLocation = true;
	}

	GPOS_ASSERT(0 < gpdb::ListLength(extentry->urilocations));

	CHAR *szFirstUri = NULL;
	Uri *pUri = NULL;
	if (!fUsingExecute)
	{
		szFirstUri = ((Value *) gpdb::ListNth(extentry->urilocations, 0))->val.str;
		pUri = gpdb::ParseExternTableUri(szFirstUri);
	}

	if (fUsingLocation && (URI_FILE == pUri->protocol || URI_HTTP == pUri->protocol))
	{
		MapLocationsFile(oidRel, rgszSegFileMap, pcdbCompDB);
	}
	else if (fUsingLocation && (URI_GPFDIST == pUri->protocol || URI_GPFDISTS == pUri->protocol || URI_CUSTOM == pUri->protocol))
	{
		MapLocationsFdist(oidRel, rgszSegFileMap, pcdbCompDB, pUri, ulTotalPrimaries);
	}
	else if (fUsingExecute)
	{
		MapLocationsExecute(oidRel, rgszSegFileMap, pcdbCompDB, ulTotalPrimaries);
	}
	else if (fUsingLocation && URI_GPHDFS == pUri->protocol)
	{
		MapLocationsHdfs(rgszSegFileMap, pcdbCompDB, szFirstUri);
	}
	else
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtExternalScanError, GPOS_WSZ_LIT("Unsupported protocol and/or file location"));
	}

    // convert array map to a list so it can be serialized as part of the plan
	List *plFileNames = NIL;
	for (ULONG ul = 0; ul < ulTotalPrimaries; ul++)
	{
		Value *pval = NULL;
	    if (NULL != rgszSegFileMap[ul])
	    {
	    	pval = gpdb::MakeStringValue(rgszSegFileMap[ul]);
	    }
	    else
		{
			// no file for this segdb. add a null entry
			pval = MakeNode(Value);
			pval->type = T_Null;
		}
		plFileNames = gpdb::LAppend(plFileNames, pval);
	}

	return plFileNames;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalTableScan *phy_tbl_scan_dxlop = CDXLPhysicalTableScan::Cast(tbl_scan_dxlnode->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const CDXLTableDescr *dxl_table_descr = phy_tbl_scan_dxlop->GetDXLTableDescr();
	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(dxl_table_descr->MDId());
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(dxl_table_descr, NULL /*index_descr_dxl*/, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	Plan *plan = NULL;
	Plan *plan_return = NULL;
	if (IMDRelation::ErelstorageExternal == md_rel->RetrieveRelStorageType())
	{
		const IMDRelationExternal *md_rel_ext = dynamic_cast<const IMDRelationExternal*>(md_rel);
		OID oidRel = CMDIdGPDB::CastMdid(md_rel->MDId())->Oid();
		ExtTableEntry *ext_table_entry = gpdb::GetExternalTableEntry(oidRel);
		
		// create external scan node
		ExternalScan *ext_scan = MakeNode(ExternalScan);
		ext_scan->scan.scanrelid = index;
		ext_scan->uriList = PlExternalScanUriList(oidRel);
		Value *val = gpdb::MakeStringValue(ext_table_entry->fmtopts);
		ext_scan->fmtOpts = ListMake1(val);
		ext_scan->fmtType = ext_table_entry->fmtcode;
		ext_scan->isMasterOnly = (IMDRelation::EreldistrMasterOnly == md_rel_ext->GetRelDistribution());
		ext_scan->rejLimit = md_rel_ext->RejectLimit();
		ext_scan->rejLimitInRows = md_rel_ext->IsRejectLimitInRows();

		IMDId *pmdidFmtErrTbl = md_rel_ext->GetFormatErrTableMdid();
		if (NULL == pmdidFmtErrTbl)
		{
			ext_scan->fmterrtbl = InvalidOid;
		}
		else
		{
			ext_scan->fmterrtbl = CMDIdGPDB::CastMdid(pmdidFmtErrTbl)->Oid();
		}

		ext_scan->encoding = ext_table_entry->encoding;
		ext_scan->scancounter = m_external_scan_counter++;

		plan = &(ext_scan->scan.plan);
		plan_return = (Plan *) ext_scan;
	}
	else
	{
		// create table scan node
		TableScan *table_scan = MakeNode(TableScan);
		table_scan->scanrelid = index;
		plan = &(table_scan->plan);
		plan_return = (Plan *) table_scan;
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
	CDXLNode *project_list_dxlnode = (*tbl_scan_dxlnode)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*tbl_scan_dxlnode)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		&base_table_context,	// translate context for the base table
		NULL,			// translate_ctxt_left and pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	SetParamIds(plan);

	return plan_return;
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

		ULONG index_col_pos_idx_max = gpos::ulong_max;
		const ULONG arity = md_rel->ColumnCount();
		for (ULONG col_pos_idx = 0; col_pos_idx < arity; col_pos_idx++)
		{
			const IMDColumn *md_col = md_rel->GetMdCol(col_pos_idx);
			if (attno == md_col->AttrNum())
			{
				index_col_pos_idx_max = col_pos_idx;
				break;
			}
		}

		if (gpos::ulong_max > index_col_pos_idx_max)
		{
			((Var *)node)->varattno =  1 + index->GetKeyPos(index_col_pos_idx_max);
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalIndexScan *physical_idx_scan_dxlop = CDXLPhysicalIndexScan::Cast(index_scan_dxlnode->GetOperator());

	return TranslateDXLIndexScan(index_scan_dxlnode, physical_idx_scan_dxlop, output_context, false /*is_index_only_scan*/, ctxt_translation_prev_siblings);
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
	CDXLPhysicalIndexScan *physical_idx_scan_dxlop,
	CDXLTranslateContext *output_context,
	BOOL is_index_only_scan,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const CDXLIndexDescr *index_descr_dxl = NULL;
	if (is_index_only_scan)
	{
		index_descr_dxl = physical_idx_scan_dxlop->GetDXLIndexDescr();
	}

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(physical_idx_scan_dxlop->GetDXLTableDescr()->MDId());

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(physical_idx_scan_dxlop->GetDXLTableDescr(), index_descr_dxl, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	IndexScan *index_scan = NULL;
	GPOS_ASSERT(!is_index_only_scan);
	index_scan = MakeNode(IndexScan);
	index_scan->scan.scanrelid = index;

	CMDIdGPDB *mdid_index = CMDIdGPDB::CastMdid(physical_idx_scan_dxlop->GetDXLIndexDescr()->MDId());
	const IMDIndex *md_index = m_md_accessor->RetrieveIndex(mdid_index);
	Oid index_oid = mdid_index->Oid();

	GPOS_ASSERT(InvalidOid != index_oid);
	index_scan->indexid = index_oid;

	Plan *plan = &(index_scan->scan.plan);
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
	CDXLNode *project_list_dxlnode = (*index_scan_dxlnode)[EdxlisIndexProjList];
	CDXLNode *filter_dxlnode = (*index_scan_dxlnode)[EdxlisIndexFilter];
	CDXLNode *index_cond_list_dxlnode = (*index_scan_dxlnode)[EdxlisIndexCondition];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxlnode, &base_table_context, NULL /*child_contexts*/, output_context);

	// translate index filter
	plan->qual = TranslateDXLIndexFilter
					(
					filter_dxlnode,
					output_context,
					&base_table_context,
					ctxt_translation_prev_siblings
					);

	index_scan->indexorderdir = CTranslatorUtils::GetScanDirection(physical_idx_scan_dxlop->GetIndexScanDir());

	// translate index condition list
	List *index_cond = NIL;
	List *index_orig_cond = NIL;
	List *index_strategy_list = NIL;
	List *index_subtype_list = NIL;

	TranslateIndexConditions
		(
		index_cond_list_dxlnode,
		physical_idx_scan_dxlop->GetDXLTableDescr(),
		is_index_only_scan,
		md_index,
		md_rel,
		output_context,
		&base_table_context,
		ctxt_translation_prev_siblings,
		&index_cond,
		&index_orig_cond,
		&index_strategy_list,
		&index_subtype_list
		);

	index_scan->indexqual = index_cond;
	index_scan->indexqualorig = index_orig_cond;
	index_scan->indexstrategy = index_strategy_list;
	index_scan->indexsubtype = index_subtype_list;
	SetParamIds(plan);

	return (Plan *) index_scan;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	List *quals_list = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt colid_var_mapping(m_mp, base_table_context, ctxt_translation_prev_siblings, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = filter_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *index_filter_dxlnode = (*filter_dxlnode)[ul];
		Expr *index_filter_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(index_filter_dxlnode, &colid_var_mapping);
		quals_list = gpdb::LAppend(quals_list, index_filter_expr);
	}

	return quals_list;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings,
	List **index_cond,
	List **index_orig_cond,
	List **index_strategy_list,
	List **index_subtype_list
	)
{
	// array of index qual info
	CIndexQualInfoArray *index_qual_info_array = GPOS_NEW(m_mp) CIndexQualInfoArray(m_mp);

	// build colid->var mapping
	CMappingColIdVarPlStmt colid_var_mapping(m_mp, base_table_context, ctxt_translation_prev_siblings, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = index_cond_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *index_cond_dxlnode = (*index_cond_list_dxlnode)[ul];

		Expr *original_index_cond_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(index_cond_dxlnode, &colid_var_mapping);
		Expr *index_cond_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(index_cond_dxlnode, &colid_var_mapping);
		GPOS_ASSERT((IsA(index_cond_expr, OpExpr) || IsA(index_cond_expr, ScalarArrayOpExpr))
				&& "expected OpExpr or ScalarArrayOpExpr in index qual");

		if (IsA(index_cond_expr, ScalarArrayOpExpr) && IMDIndex::EmdindBitmap != index->IndexType())
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion, GPOS_WSZ_LIT("ScalarArrayOpExpr condition on index scan"));
		}

		// for indexonlyscan, we already have the attno referring to the index
		if (!is_index_only_scan)
		{
			// Otherwise, we need to perform mapping of Varattnos relative to column positions in index keys
			SContextIndexVarAttno index_varattno_ctxt(md_rel, index);
			SetIndexVarAttnoWalker((Node *) index_cond_expr, &index_varattno_ctxt);
		}
		
		// find index key's attno
		List *args_list = NULL;
		if (IsA(index_cond_expr, OpExpr))
		{
			args_list = ((OpExpr *) index_cond_expr)->args;
		}
		else
		{
			args_list = ((ScalarArrayOpExpr *) index_cond_expr)->args;
		}

		Node *left_arg = (Node *) lfirst(gpdb::ListHead(args_list));
		Node *right_arg = (Node *) lfirst(gpdb::ListTail(args_list));
				
		BOOL is_relabel_type = false;
		if (IsA(left_arg, RelabelType) && IsA(((RelabelType *) left_arg)->arg, Var))
		{
			left_arg = (Node *) ((RelabelType *) left_arg)->arg;
			is_relabel_type = true;
		}
		else if (IsA(right_arg, RelabelType) && IsA(((RelabelType *) right_arg)->arg, Var))
		{
			right_arg = (Node *) ((RelabelType *) right_arg)->arg;
			is_relabel_type = true;
		}
		
		if (is_relabel_type)
		{
			List *new_args_list = ListMake2(left_arg, right_arg);
			gpdb::GPDBFree(args_list);
			if (IsA(index_cond_expr, OpExpr))
			{
				((OpExpr *) index_cond_expr)->args = new_args_list;
			}
			else
			{
				((ScalarArrayOpExpr *) index_cond_expr)->args = new_args_list;
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
		INT strategy_num = 0;
		OID index_subtype_oid = InvalidOid;
		BOOL recheck = false;
		
		OID cmp_operator_oid = CTranslatorUtils::OidCmpOperator(index_cond_expr);
		GPOS_ASSERT(InvalidOid != cmp_operator_oid);
		OID op_family_oid = CTranslatorUtils::GetOpFamilyForIndexQual(attno, CMDIdGPDB::CastMdid(index->MDId())->Oid());
		GPOS_ASSERT(InvalidOid != op_family_oid);
		gpdb::IndexOpProperties(cmp_operator_oid, op_family_oid, &strategy_num, &index_subtype_oid, &recheck);
		
		// create index qual
		index_qual_info_array->Append(GPOS_NEW(m_mp) CIndexQualInfo(attno, index_cond_expr, original_index_cond_expr, (StrategyNumber) strategy_num, index_subtype_oid));
	}

	// the index quals much be ordered by attribute number
	index_qual_info_array->Sort(CIndexQualInfo::IndexQualInfoCmp);

	ULONG length = index_qual_info_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CIndexQualInfo *index_qual_info = (*index_qual_info_array)[ul];
		*index_cond = gpdb::LAppend(*index_cond, index_qual_info->m_expr);
		*index_orig_cond = gpdb::LAppend(*index_orig_cond, index_qual_info->m_original_expr);
		*index_strategy_list = gpdb::LAppendInt(*index_strategy_list, index_qual_info->m_strategy_num);
		*index_subtype_list = gpdb::LAppendOid(*index_subtype_list, index_qual_info->m_index_subtype_oid);
	}

	// clean up
	index_qual_info_array->Release();
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
	CDXLNode *assert_contraint_list_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *child_contexts
	)
{
	List *quals_list = NIL;

	// build colid->var mapping
	CMappingColIdVarPlStmt colid_var_mapping(m_mp, NULL /*base_table_context*/, child_contexts, output_context, m_dxl_to_plstmt_context);

	const ULONG arity = assert_contraint_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *assert_contraint_dxlnode = (*assert_contraint_list_dxlnode)[ul];
		Expr *assert_contraint_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar((*assert_contraint_dxlnode)[0], &colid_var_mapping);
		quals_list = gpdb::LAppend(quals_list, assert_contraint_expr);
	}

	return quals_list;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create limit node
	Limit *limit = MakeNode(Limit);

	Plan *plan = &(limit->plan);
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

	CDXLTranslateContext left_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());

	// translate proj list
	CDXLNode *project_list_dxlnode = (*limit_dxlnode)[EdxllimitIndexProjList];
	CDXLNode *child_plan_dxlnode = (*limit_dxlnode)[EdxllimitIndexChildPlan];
	CDXLNode *limit_count_dxlnode = (*limit_dxlnode)[EdxllimitIndexLimitCount];
	CDXLNode *limit_offset_dxlnode = (*limit_dxlnode)[EdxllimitIndexLimitOffset];

	// NOTE: Limit node has only the left plan while the right plan is left empty
	Plan *left_plan = TranslateDXLOperatorToPlan(child_plan_dxlnode, &left_dxl_translate_ctxt, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&left_dxl_translate_ctxt);

	plan->targetlist = TranslateDXLProjList
								(
								project_list_dxlnode,
								NULL,		// base table translation context
								child_contexts,
								output_context
								);

	plan->lefttree = left_plan;

	if(NULL != limit_count_dxlnode && limit_count_dxlnode->Arity() >0)
	{
		CMappingColIdVarPlStmt colid_var_mapping(m_mp, NULL, child_contexts, output_context, m_dxl_to_plstmt_context);
		Node *limit_count = (Node *) m_translator_dxl_to_scalar->TranslateDXLToScalar((*limit_count_dxlnode)[0], &colid_var_mapping);
		limit->limitCount = limit_count;
	}

	if(NULL != limit_offset_dxlnode && limit_offset_dxlnode->Arity() >0)
	{
		CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt(m_mp, NULL, child_contexts, output_context, m_dxl_to_plstmt_context);
		Node *limit_offset = (Node *) m_translator_dxl_to_scalar->TranslateDXLToScalar((*limit_offset_dxlnode)[0], &colid_var_mapping);
		limit->limitOffset = limit_offset;
	}

	plan->nMotionNodes = left_plan->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return  (Plan *) limit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLHashJoin
//
//	@doc:
//		Translates a DXL hash join node into a HashJoin node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLHashJoin
	(
	const CDXLNode *hj_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(hj_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalHashJoin);
	GPOS_ASSERT(hj_dxlnode->Arity() == EdxlhjIndexSentinel);

	// create hash join node
	HashJoin *hashjoin = MakeNode(HashJoin);

	Join *join = &(hashjoin->join);
	Plan *plan = &(join->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalHashJoin *hashjoin_dxlop = CDXLPhysicalHashJoin::Cast(hj_dxlnode->GetOperator());

	// set join type
	join->jointype = GetGPDBJoinTypeFromDXLJoinType(hashjoin_dxlop->GetJoinType());
	join->prefetch_inner = true;

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
	CDXLNode *left_tree_dxlnode = (*hj_dxlnode)[EdxlhjIndexHashLeft];
	CDXLNode *right_tree_dxlnode = (*hj_dxlnode)[EdxlhjIndexHashRight];
	CDXLNode *project_list_dxlnode = (*hj_dxlnode)[EdxlhjIndexProjList];
	CDXLNode *filter_dxlnode = (*hj_dxlnode)[EdxlhjIndexFilter];
	CDXLNode *join_filter_dxlnode = (*hj_dxlnode)[EdxlhjIndexJoinFilter];
	CDXLNode *hash_cond_list_dxlnode = (*hj_dxlnode)[EdxlhjIndexHashCondList];

	CDXLTranslateContext left_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext right_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *left_plan = TranslateDXLOperatorToPlan(left_tree_dxlnode, &left_dxl_translate_ctxt, ctxt_translation_prev_siblings);

	// the right side of the join is the one where the hash phase is done
	CDXLTranslationContextArray *translation_context_arr_with_siblings = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	translation_context_arr_with_siblings->Append(&left_dxl_translate_ctxt);
	translation_context_arr_with_siblings->AppendArray(ctxt_translation_prev_siblings);
	Plan *right_plan = (Plan*) TranslateDXLHash(right_tree_dxlnode, &right_dxl_translate_ctxt, translation_context_arr_with_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&left_dxl_translate_ctxt));
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&right_dxl_translate_ctxt));
	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join filter
	join->joinqual = TranslateDXLFilterToQual
					(
					join_filter_dxlnode,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	// translate hash cond
	List *hash_conditions_list = NIL;

	BOOL has_is_not_distinct_from_cond = false;

	const ULONG arity = hash_cond_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *hash_cond_dxlnode = (*hash_cond_list_dxlnode)[ul];

		List *hash_cond_list = TranslateDXLScCondToQual
				(
				hash_cond_dxlnode,
				NULL,			// base table translation context
				child_contexts,
				output_context
				);

		GPOS_ASSERT(1 == gpdb::ListLength(hash_cond_list));

		Expr *expr = (Expr *) LInitial(hash_cond_list);
		if (IsA(expr, BoolExpr) && ((BoolExpr *) expr)->boolop == NOT_EXPR)
		{
			// INDF test
			GPOS_ASSERT(gpdb::ListLength(((BoolExpr *) expr)->args) == 1 &&
						(IsA((Expr *) LInitial(((BoolExpr *) expr)->args), DistinctExpr)));
			has_is_not_distinct_from_cond = true;
		}
		hash_conditions_list = gpdb::ListConcat(hash_conditions_list, hash_cond_list);
	}

	if (!has_is_not_distinct_from_cond)
	{
		// no INDF conditions in the hash condition list
		hashjoin->hashclauses = hash_conditions_list;
	}
	else
	{
		// hash conditions contain INDF clauses -> extract equality conditions to
		// construct the hash clauses list
		List *hash_clauses_list = NIL;

		for (ULONG ul = 0; ul < arity; ul++)
		{
			CDXLNode *hash_cond_dxlnode = (*hash_cond_list_dxlnode)[ul];

			// condition can be either a scalar comparison or a NOT DISTINCT FROM expression
			GPOS_ASSERT(EdxlopScalarCmp == hash_cond_dxlnode->GetOperator()->GetDXLOperator() ||
						EdxlopScalarBoolExpr == hash_cond_dxlnode->GetOperator()->GetDXLOperator());

			if (EdxlopScalarBoolExpr == hash_cond_dxlnode->GetOperator()->GetDXLOperator())
			{
				// clause is a NOT DISTINCT FROM check -> extract the distinct comparison node
				GPOS_ASSERT(Edxlnot == CDXLScalarBoolExpr::Cast(hash_cond_dxlnode->GetOperator())->GetDxlBoolTypeStr());
				hash_cond_dxlnode = (*hash_cond_dxlnode)[0];
				GPOS_ASSERT(EdxlopScalarDistinct == hash_cond_dxlnode->GetOperator()->GetDXLOperator());
			}

			CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
														(
														m_mp,
														NULL,
														child_contexts,
														output_context,
														m_dxl_to_plstmt_context
														);

			// translate the DXL scalar or scalar distinct comparison into an equality comparison
			// to store in the hash clauses
			Expr *hash_clause_expr = (Expr *) m_translator_dxl_to_scalar->TranslateDXLScalarCmpToScalar
									(
									hash_cond_dxlnode,
									&colid_var_mapping
									);

			hash_clauses_list = gpdb::LAppend(hash_clauses_list, hash_clause_expr);
		}

		hashjoin->hashclauses = hash_clauses_list;
		hashjoin->hashqualclauses = hash_conditions_list;
	}

	GPOS_ASSERT(NIL != hashjoin->hashclauses);

	plan->lefttree = left_plan;
	plan->righttree = right_plan;
	plan->nMotionNodes = left_plan->nMotionNodes + right_plan->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	translation_context_arr_with_siblings->Release();
	child_contexts->Release();

	return  (Plan *) hashjoin;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// create function scan node
	FunctionScan *func_scan = MakeNode(FunctionScan);
	Plan *plan = &(func_scan->scan.plan);

	RangeTblEntry *rte = TranslateDXLTvfToRangeTblEntry(tvf_dxlnode, output_context, &base_table_context);
	GPOS_ASSERT(NULL != rte);

	func_scan->funcexpr = rte->funcexpr;
	func_scan->funccolnames = rte->eref->colnames;

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	base_table_context.SetRelIndex(index);
	func_scan->scan.scanrelid = index;

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

	CDXLNode *project_list_dxlnode = (*tvf_dxlnode)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = TranslateDXLProjList
						(
						project_list_dxlnode,
						&base_table_context,
						NULL,
						output_context
						);

	plan->targetlist = target_list;

	ListCell *lc_target_entry = NULL;

	ForEach (lc_target_entry, target_list)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc_target_entry);
		OID oid_type = gpdb::ExprType((Node*) target_entry->expr);
		GPOS_ASSERT(InvalidOid != oid_type);

		INT typ_mod = gpdb::ExprTypeMod((Node*) target_entry->expr);

		func_scan->funccoltypes = gpdb::LAppendOid(func_scan->funccoltypes, oid_type);
		func_scan->funccoltypmods = gpdb::LAppendInt(func_scan->funccoltypmods, typ_mod);
	}

	SetParamIds(plan);

	return (Plan *) func_scan;
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

	FuncExpr *func_expr = MakeNode(FuncExpr);

	func_expr->funcid = CMDIdGPDB::CastMdid(dxlop->FuncMdId())->Oid();
	func_expr->funcretset = true;
	// this is a function call, as opposed to a cast
	func_expr->funcformat = COERCE_EXPLICIT_CALL;
	func_expr->funcresulttype = CMDIdGPDB::CastMdid(dxlop->ReturnTypeMdId())->Oid();

	Alias *alias = MakeNode(Alias);
	alias->colnames = NIL;

	// get function alias
	alias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxlop->Pstr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxlnode = (*tvf_dxlnode)[EdxltsIndexProjList];

	// get column names
	const ULONG num_of_cols = project_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < num_of_cols; ul++)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *val_colname = gpdb::MakeStringValue(col_name_char_array);
		alias->colnames = gpdb::LAppend(alias->colnames, val_colname);

		// save mapping col id -> index in translate context
		(void) base_table_context->InsertMapping(dxl_proj_elem->Id(), ul+1 /*attno*/);
	}

	// function arguments
	const ULONG num_of_child = tvf_dxlnode->Arity();
	for (ULONG ul = 1; ul < num_of_child; ++ul)
	{
		CDXLNode *func_arg_dxlnode = (*tvf_dxlnode)[ul];

		CMappingColIdVarPlStmt colid_var_mapping
									(
									m_mp,
									base_table_context,
									NULL,
									output_context,
									m_dxl_to_plstmt_context
									);

		Expr *pexprFuncArg = m_translator_dxl_to_scalar->TranslateDXLToScalar(func_arg_dxlnode, &colid_var_mapping);
		func_expr->args = gpdb::LAppend(func_expr->args, pexprFuncArg);
	}

	rte->funcexpr = (Node *)func_expr;
	rte->inFromCl = true;
	rte->eref = alias;

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
	CDXLPhysicalValuesScan *phy_values_scan_dxlop = CDXLPhysicalValuesScan::Cast(value_scan_dxlnode->GetOperator());

	RangeTblEntry *rte = MakeNode(RangeTblEntry);

	rte->relid = InvalidOid;
	rte->subquery = NULL;
	rte->rtekind = RTE_VALUES;
	rte->inh = false;			/* never true for values RTEs */
	rte->inFromCl = true;
	rte->requiredPerms = 0;
	rte->checkAsUser = InvalidOid;

	Alias *alias = MakeNode(Alias);
	alias->colnames = NIL;

	// get value alias
	alias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(phy_values_scan_dxlop->GetOpNameStr()->GetBuffer());

	// project list
	CDXLNode *project_list_dxlnode = (*value_scan_dxlnode)[EdxltsIndexProjList];

	// get column names
	const ULONG num_of_cols = project_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < num_of_cols; ul++)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		CDXLScalarProjElem *dxl_proj_elem = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_proj_elem->GetMdNameAlias()->GetMDName()->GetBuffer());

		Value *val_colname = gpdb::MakeStringValue(col_name_char_array);
		alias->colnames = gpdb::LAppend(alias->colnames, val_colname);

		// save mapping col id -> index in translate context
		(void) base_table_context->InsertMapping(dxl_proj_elem->Id(), ul+1 /*attno*/);
	}

	CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt(m_mp, base_table_context, NULL, output_context, m_dxl_to_plstmt_context);
	const ULONG num_of_child = value_scan_dxlnode->Arity();
	List *values_lists = NIL;
	for (ULONG ulValue = EdxlValIndexConstStart; ulValue < num_of_child; ulValue++)
	{
		CDXLNode *value_list_dxlnode = (*value_scan_dxlnode)[ulValue];
		const ULONG num_of_cols = value_list_dxlnode->Arity();
		List *value = NIL;
		for (ULONG ulCol = 0; ulCol < num_of_cols ; ulCol++)
		{
			Expr *const_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar((*value_list_dxlnode)[ulCol], &colid_var_mapping);
			value = gpdb::LAppend(value, const_expr);
		}
		values_lists = gpdb::LAppend(values_lists, value);
	}

	rte->values_lists = (List *) values_lists;
	rte->eref = alias;

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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(nl_join_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalNLJoin);
	GPOS_ASSERT(nl_join_dxlnode->Arity() == EdxlnljIndexSentinel);

	// create hash join node
	NestLoop *nested_loop = MakeNode(NestLoop);

	Join *join = &(nested_loop->join);
	Plan *plan = &(join->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalNLJoin *dxl_nlj = CDXLPhysicalNLJoin::PdxlConvert(nl_join_dxlnode->GetOperator());

	// set join type
	join->jointype = GetGPDBJoinTypeFromDXLJoinType(dxl_nlj->GetJoinType());

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
	CDXLNode *left_tree_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexLeftChild];
	CDXLNode *right_tree_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexRightChild];

	CDXLNode *project_list_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexProjList];
	CDXLNode *filter_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexFilter];
	CDXLNode *join_filter_dxlnode = (*nl_join_dxlnode)[EdxlnljIndexJoinFilter];

	CDXLTranslateContext left_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext right_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());

	// setting of prefetch_inner to true except for the case of index NLJ where we cannot prefetch inner
	// because inner child depends on variables coming from outer child
	join->prefetch_inner = !dxl_nlj->IsIndexNLJ();

	CDXLTranslationContextArray *translation_context_arr_with_siblings = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	Plan *left_plan = NULL;
	Plan *right_plan = NULL;
	if (dxl_nlj->IsIndexNLJ())
	{
		// right child (the index scan side) has references to left child's columns,
		// we need to translate left child first to load its columns into translation context
		left_plan = TranslateDXLOperatorToPlan(left_tree_dxlnode, &left_dxl_translate_ctxt, ctxt_translation_prev_siblings);

		translation_context_arr_with_siblings->Append(&left_dxl_translate_ctxt);
		 translation_context_arr_with_siblings->AppendArray(ctxt_translation_prev_siblings);

		 // translate right child after left child translation is complete
		right_plan = TranslateDXLOperatorToPlan(right_tree_dxlnode, &right_dxl_translate_ctxt, translation_context_arr_with_siblings);
	}
	else
	{
		// left child may include a PartitionSelector with references to right child's columns,
		// we need to translate right child first to load its columns into translation context
		right_plan = TranslateDXLOperatorToPlan(right_tree_dxlnode, &right_dxl_translate_ctxt, ctxt_translation_prev_siblings);

		translation_context_arr_with_siblings->Append(&right_dxl_translate_ctxt);
		translation_context_arr_with_siblings->AppendArray(ctxt_translation_prev_siblings);

		// translate left child after right child translation is complete
		left_plan = TranslateDXLOperatorToPlan(left_tree_dxlnode, &left_dxl_translate_ctxt, translation_context_arr_with_siblings);
	}
	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&left_dxl_translate_ctxt);
	child_contexts->Append(&right_dxl_translate_ctxt);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join condition
	join->joinqual = TranslateDXLFilterToQual
					(
					join_filter_dxlnode,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	plan->lefttree = left_plan;
	plan->righttree = right_plan;
	plan->nMotionNodes = left_plan->nMotionNodes + right_plan->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	translation_context_arr_with_siblings->Release();
	child_contexts->Release();

	return  (Plan *) nested_loop;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	GPOS_ASSERT(merge_join_dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalMergeJoin);
	GPOS_ASSERT(merge_join_dxlnode->Arity() == EdxlmjIndexSentinel);

	// create merge join node
	MergeJoin *merge_join = MakeNode(MergeJoin);

	Join *join = &(merge_join->join);
	Plan *plan = &(join->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMergeJoin *merge_join_dxlop = CDXLPhysicalMergeJoin::Cast(merge_join_dxlnode->GetOperator());

	// set join type
	join->jointype = GetGPDBJoinTypeFromDXLJoinType(merge_join_dxlop->GetJoinType());

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
	CDXLNode *left_tree_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexLeftChild];
	CDXLNode *right_tree_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexRightChild];

	CDXLNode *project_list_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexProjList];
	CDXLNode *filter_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexFilter];
	CDXLNode *join_filter_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexJoinFilter];
	CDXLNode *merge_cond_list_dxlnode = (*merge_join_dxlnode)[EdxlmjIndexMergeCondList];

	CDXLTranslateContext left_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());
	CDXLTranslateContext right_dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *left_plan = TranslateDXLOperatorToPlan(left_tree_dxlnode, &left_dxl_translate_ctxt, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *translation_context_arr_with_siblings = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	translation_context_arr_with_siblings->Append(&left_dxl_translate_ctxt);
	translation_context_arr_with_siblings->AppendArray(ctxt_translation_prev_siblings);

	Plan *right_plan = TranslateDXLOperatorToPlan(right_tree_dxlnode, &right_dxl_translate_ctxt, translation_context_arr_with_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&left_dxl_translate_ctxt));
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&right_dxl_translate_ctxt));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate join filter
	join->joinqual = TranslateDXLFilterToQual
					(
					join_filter_dxlnode,
					NULL,			// translate context for the base table
					child_contexts,
					output_context
					);

	// translate merge cond
	List *merge_conditions_list = NIL;

	const ULONG arity = merge_cond_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *merge_condition_dxlnode = (*merge_cond_list_dxlnode)[ul];
		List *merge_condition_list = TranslateDXLScCondToQual
				(
				merge_condition_dxlnode,
				NULL,			// base table translation context
				child_contexts,
				output_context
				);

		GPOS_ASSERT(1 == gpdb::ListLength(merge_condition_list));
		merge_conditions_list = gpdb::ListConcat(merge_conditions_list, merge_condition_list);
	}

	GPOS_ASSERT(NIL != merge_conditions_list);

	merge_join->mergeclauses = merge_conditions_list;

	plan->lefttree = left_plan;
	plan->righttree = right_plan;
	plan->nMotionNodes = left_plan->nMotionNodes + right_plan->nMotionNodes;
	SetParamIds(plan);

	// cleanup
	translation_context_arr_with_siblings->Release();
	child_contexts->Release();

	return  (Plan *) merge_join;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	Hash *hash = MakeNode(Hash);

	Plan *plan = &(hash->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// translate dxl node
	CDXLTranslateContext dxl_translate_ctxt(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *left_plan = TranslateDXLOperatorToPlan(dxlnode, &dxl_translate_ctxt, ctxt_translation_prev_siblings);

	GPOS_ASSERT(0 < dxlnode->Arity());

	// create a reference to each entry in the child project list to create the target list of
	// the hash node
	CDXLNode *project_list_dxlnode = (*dxlnode)[0];
	List *target_list = TranslateDXLProjectListToHashTargetList(project_list_dxlnode, &dxl_translate_ctxt, output_context);

	// copy costs from child node; the startup cost for the hash node is the total cost
	// of the child plan, see make_hash in createplan.c
	plan->startup_cost = left_plan->total_cost;
	plan->total_cost = left_plan->total_cost;
	plan->plan_rows = left_plan->plan_rows;
	plan->plan_width = left_plan->plan_width;

	plan->targetlist = target_list;
	plan->lefttree = left_plan;
	plan->righttree = NULL;
	plan->nMotionNodes = left_plan->nMotionNodes;
	plan->qual = NIL;
	hash->rescannable = false;

	SetParamIds(plan);

	return (Plan *) hash;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::TranslateDXLDuplicateSensitiveMotion
//
//	@doc:
//		Translate DXL motion node
//
//---------------------------------------------------------------------------
Plan *
CTranslatorDXLToPlStmt::TranslateDXLDuplicateSensitiveMotion
	(
	const CDXLNode *motion_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalMotion *motion_dxlop = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());
	if (CTranslatorUtils::IsDuplicateSensitiveMotion(motion_dxlop))
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalMotion *motion_dxlop = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());

	// create motion node
	Motion *motion = MakeNode(Motion);

	Plan *plan = &(motion->plan);
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

	CDXLNode *project_list_dxlnode = (*motion_dxlnode)[EdxlgmIndexProjList];
	CDXLNode *filter_dxlnode = (*motion_dxlnode)[EdxlgmIndexFilter];
	CDXLNode *sort_col_list_dxl = (*motion_dxlnode)[EdxlgmIndexSortColList];

	// translate motion child
	// child node is in the same position in broadcast and gather motion nodes
	// but different in redistribute motion nodes

	ULONG child_index = motion_dxlop->GetRelationChildIdx();

	CDXLNode *child_dxlnode = (*motion_dxlnode)[child_index];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate sorting info
	ULONG num_sort_cols = sort_col_list_dxl->Arity();
	if (0 < num_sort_cols)
	{
		motion->sendSorted = true;
		motion->numSortCols = num_sort_cols;
		motion->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(num_sort_cols * sizeof(AttrNumber));
		motion->sortOperators = (Oid *) gpdb::GPDBAlloc(num_sort_cols * sizeof(Oid));
		motion->nullsFirst = (bool *) gpdb::GPDBAlloc(num_sort_cols * sizeof(bool));

		TranslateSortCols(sort_col_list_dxl, output_context, motion->sortColIdx, motion->sortOperators, motion->nullsFirst);
	}
	else
	{
		// not a sorting motion
		motion->sendSorted = false;
		motion->numSortCols = 0;
		motion->sortColIdx = NULL;
		motion->sortOperators = NULL;
		motion->nullsFirst = NULL;
	}

	if (motion_dxlop->GetDXLOperator() == EdxlopPhysicalMotionRedistribute ||
		motion_dxlop->GetDXLOperator() == EdxlopPhysicalMotionRoutedDistribute ||
		motion_dxlop->GetDXLOperator() == EdxlopPhysicalMotionRandom)
	{
		// translate hash expr list
		List *hash_expr_list = NIL;
		List *hash_expr_types_list = NIL;

		if (EdxlopPhysicalMotionRedistribute == motion_dxlop->GetDXLOperator())
		{
			CDXLNode *hash_expr_list_dxlnode = (*motion_dxlnode)[EdxlrmIndexHashExprList];

			TranslateHashExprList
				(
				hash_expr_list_dxlnode,
				&child_context,
				&hash_expr_list,
				&hash_expr_types_list,
				output_context
				);
		}
		GPOS_ASSERT(gpdb::ListLength(hash_expr_list) == gpdb::ListLength(hash_expr_types_list));

		motion->hashExpr = hash_expr_list;
		motion->hashDataTypes = hash_expr_types_list;
	}

	// cleanup
	child_contexts->Release();

	// create flow for child node to distinguish between singleton flows and all-segment flows
	Flow *flow = MakeNode(Flow);

	const IntPtrArray *input_segids_array = motion_dxlop->GetInputSegIdsArray();


	// only one sender
	if (1 == input_segids_array->Size())
	{
		flow->segindex = *((*input_segids_array)[0]);

		// only one segment in total
		if (1 == gpdb::GetGPSegmentCount())
		{
			if (flow->segindex == MASTER_CONTENT_ID)
				// sender is on master, must be singleton flow
				flow->flotype = FLOW_SINGLETON;
			else
				// sender is on segment, can not tell it's singleton or
				// all-segment flow, just treat it as all-segment flow so
				// it can be promoted to writer gang later if needed.
				flow->flotype = FLOW_UNDEFINED;
		}
		else
		{
			// multiple segments, must be singleton flow
			flow->flotype = FLOW_SINGLETON;
		}
	}
	else
	{
		flow->flotype = FLOW_UNDEFINED;
	}

	child_plan->flow = flow;

	motion->motionID = m_dxl_to_plstmt_context->GetNextMotionId();
	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes + 1;

	// translate properties of the specific type of motion operator

	switch (motion_dxlop->GetDXLOperator())
	{
		case EdxlopPhysicalMotionGather:
		{
			motion->motionType = MOTIONTYPE_FIXED;
			// get segment id
			INT segid = CDXLPhysicalGatherMotion::Cast(motion_dxlop)->IOutputSegIdx();
			motion->numOutputSegs = 1;
			motion->outputSegIdx = (INT *) gpdb::GPDBAlloc(sizeof(INT));
			*(motion->outputSegIdx) = segid;
			break;
		}
		case EdxlopPhysicalMotionRedistribute:
		case EdxlopPhysicalMotionRandom:
		{
			motion->motionType = MOTIONTYPE_HASH;
			// translate output segment ids
			const IntPtrArray *output_segids_array = CDXLPhysicalMotion::Cast(motion_dxlop)->GetOutputSegIdsArray();

			GPOS_ASSERT(NULL != output_segids_array && 0 < output_segids_array->Size());
			ULONG segid_count = output_segids_array->Size();
			motion->outputSegIdx = (INT *) gpdb::GPDBAlloc (segid_count * sizeof(INT));
			motion->numOutputSegs = segid_count;

			for(ULONG ul = 0; ul < segid_count; ul++)
			{
				INT segid = *((*output_segids_array)[ul]);
				motion->outputSegIdx[ul] = segid;
			}

			break;
		}
		case EdxlopPhysicalMotionBroadcast:
		{
			motion->motionType = MOTIONTYPE_FIXED;
			motion->numOutputSegs = 0;
			motion->outputSegIdx = NULL;
			break;
		}
		case EdxlopPhysicalMotionRoutedDistribute:
		{
			motion->motionType = MOTIONTYPE_EXPLICIT;
			motion->numOutputSegs = 0;
			motion->outputSegIdx = NULL;
			ULONG segid_col = CDXLPhysicalRoutedDistributeMotion::Cast(motion_dxlop)->SegmentIdCol();
			const TargetEntry *te_sort_col = child_context.GetTargetEntry(segid_col);
			motion->segidColIdx = te_sort_col->resno;

			break;
			
		}
		default:
			GPOS_ASSERT(!"Unrecognized Motion operator");
			return NULL;
	}

	SetParamIds(plan);

	return (Plan *) motion;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create motion node
	Result *result = MakeNode(Result);

	Plan *plan = &(result->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMotion *motion_dxlop = CDXLPhysicalMotion::Cast(motion_dxlnode->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(motion_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	CDXLNode *project_list_dxlnode = (*motion_dxlnode)[EdxlrmIndexProjList];
	CDXLNode *filter_dxlnode = (*motion_dxlnode)[EdxlrmIndexFilter];
	CDXLNode *child_dxlnode = (*motion_dxlnode)[motion_dxlop->GetRelationChildIdx()];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	// translate hash expr list
	result->hashFilter = true;

	if (EdxlopPhysicalMotionRedistribute == motion_dxlop->GetDXLOperator())
	{
		CDXLNode *hash_expr_list_dxlnode = (*motion_dxlnode)[EdxlrmIndexHashExprList];
		const ULONG length = hash_expr_list_dxlnode->Arity();
		GPOS_ASSERT(0 < length);
		
		for (ULONG ul = 0; ul < length; ul++)
		{
			CDXLNode *hash_expr_dxlnode = (*hash_expr_list_dxlnode)[ul];
			CDXLNode *expr_dxlnode = (*hash_expr_dxlnode)[0];
			
			INT resno = gpos::int_max;
			if (EdxlopScalarIdent == expr_dxlnode->GetOperator()->GetDXLOperator())
			{
				ULONG colid = CDXLScalarIdent::Cast(expr_dxlnode->GetOperator())->GetDXLColRef()->Id();
				resno = output_context->GetTargetEntry(colid)->resno;
			}
			else
			{
				// The expression is not a scalar ident that points to an output column in the child node.
				// Rather, it is an expresssion that is evaluated by the hash filter such as CAST(a) or a+b.
				// We therefore, create a corresponding GPDB scalar expression and add it to the project list
				// of the hash filter
				CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
															(
															m_mp,
															NULL, // translate context for the base table
															child_contexts,
															output_context,
															m_dxl_to_plstmt_context
															);
				
				Expr *expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(expr_dxlnode, &colid_var_mapping);
				GPOS_ASSERT(NULL != expr);

				// create a target entry for the hash filter
				CWStringConst str_unnamed_col(GPOS_WSZ_LIT("?column?"));
				TargetEntry *target_entry = gpdb::MakeTargetEntry
											(
											expr,
											gpdb::ListLength(plan->targetlist) + 1,
											CTranslatorUtils::CreateMultiByteCharStringFromWCString(str_unnamed_col.GetBuffer()),
											false /* resjunk */
											);
				plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);

				resno = target_entry->resno;
			}
			GPOS_ASSERT(gpos::int_max != resno);
			
			result->hashList = gpdb::LAppendInt(result->hashList, resno);
		}
	}
	
	// cleanup
	child_contexts->Release();

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	SetParamIds(plan);

	return (Plan *) result;
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
	const CDXLNode *agg_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create aggregate plan node
	Agg *agg = MakeNode(Agg);

	Plan *plan = &(agg->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalAgg *dxl_phy_agg_dxlop = CDXLPhysicalAgg::Cast(agg_dxlnode->GetOperator());

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(agg_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate agg child
	CDXLNode *child_dxlnode = (*agg_dxlnode)[EdxlaggIndexChild];

	CDXLNode *project_list_dxlnode = (*agg_dxlnode)[EdxlaggIndexProjList];
	CDXLNode *filter_dxlnode = (*agg_dxlnode)[EdxlaggIndexFilter];

	CDXLTranslateContext child_context(m_mp, true, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,			// pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	// translate aggregation strategy
	switch (dxl_phy_agg_dxlop->GetAggStrategy())
	{
		case EdxlaggstrategyPlain:
			agg->aggstrategy = AGG_PLAIN;
			break;
		case EdxlaggstrategySorted:
			agg->aggstrategy = AGG_SORTED;
			break;
		case EdxlaggstrategyHashed:
			agg->aggstrategy = AGG_HASHED;
			break;
		default:
			GPOS_ASSERT(!"Invalid aggregation strategy");
	}

	agg->streaming = dxl_phy_agg_dxlop->IsStreamSafe();

	// translate grouping cols
	const ULongPtrArray *grouping_colid_array = dxl_phy_agg_dxlop->GetGroupingColidArray();
	agg->numCols = grouping_colid_array->Size();
	if (agg->numCols > 0)
	{
		agg->grpColIdx = (AttrNumber *) gpdb::GPDBAlloc(agg->numCols * sizeof(AttrNumber));
		agg->grpOperators = (Oid *) gpdb::GPDBAlloc(agg->numCols * sizeof(Oid));
	}
	else
	{
		agg->grpColIdx = NULL;
		agg->grpOperators = NULL;
	}

	const ULONG length = grouping_colid_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG grouping_colid = *((*grouping_colid_array)[ul]);
		const TargetEntry *target_entry_grouping_col = child_context.GetTargetEntry(grouping_colid);
		if (NULL  == target_entry_grouping_col)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, grouping_colid);
		}
		agg->grpColIdx[ul] = target_entry_grouping_col->resno;

		// Also find the equality operators to use for each grouping col.
		Oid typeId = gpdb::ExprType((Node *) target_entry_grouping_col->expr);
		agg->grpOperators[ul] = gpdb::GetEqualityOp(typeId);
		Assert(agg->grpOperators[ul] != 0);
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) agg;
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
	const CDXLNode *window_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create a window plan node
	Window *window = MakeNode(Window);

	Plan *plan = &(window->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalWindow *window_dxlop = CDXLPhysicalWindow::Cast(window_dxlnode->GetOperator());

	// translate the operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(window_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	// translate children
	CDXLNode *child_dxlnode = (*window_dxlnode)[EdxlwindowIndexChild];
	CDXLNode *project_list_dxlnode = (*window_dxlnode)[EdxlwindowIndexProjList];
	CDXLNode *filter_dxlnode = (*window_dxlnode)[EdxlwindowIndexFilter];

	CDXLTranslateContext child_context(m_mp, true, output_context->GetColIdToParamIdMap());
	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,			// pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	// translate partition columns
	const ULongPtrArray *part_by_cols_array = window_dxlop->GetPartByColsArray();
	window->numPartCols = part_by_cols_array->Size();
	window->partColIdx = NULL;
	window->partOperators = NULL;

	if (window->numPartCols > 0)
	{
		window->partColIdx = (AttrNumber *) gpdb::GPDBAlloc(window->numPartCols * sizeof(AttrNumber));
		window->partOperators = (Oid *) gpdb::GPDBAlloc(window->numPartCols * sizeof(Oid));
	}

	const ULONG num_of_part_cols = part_by_cols_array->Size();
	for (ULONG ul = 0; ul < num_of_part_cols; ul++)
	{
		ULONG part_colid = *((*part_by_cols_array)[ul]);
		const TargetEntry *te_part_colid = child_context.GetTargetEntry(part_colid);
		if (NULL  == te_part_colid)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, part_colid);
		}
		window->partColIdx[ul] = te_part_colid->resno;

		// Also find the equality operators to use for each partitioning key col.
		Oid type_id = gpdb::ExprType((Node *) te_part_colid->expr);
		window->partOperators[ul] = gpdb::GetEqualityOp(type_id);
		Assert(window->partOperators[ul] != 0);
	}

	// translate window keys
	window->windowKeys = NIL;
	const ULONG size = window_dxlop->WindowKeysCount();
	for (ULONG ul = 0; ul < size; ul++)
	{
		WindowKey *windowkey = MakeNode(WindowKey);

		// translate the sorting columns used in the window key
		const CDXLWindowKey *window_key_dxl_op = window_dxlop->GetDXLWindowKeyAt(ul);
		const CDXLNode *sort_col_list_dxlnode = window_key_dxl_op->GetSortColListDXL();

		const ULONG num_of_cols = sort_col_list_dxlnode->Arity();
		windowkey->numSortCols = num_of_cols;
		windowkey->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(num_of_cols * sizeof(AttrNumber));
		windowkey->sortOperators = (Oid *) gpdb::GPDBAlloc(num_of_cols * sizeof(Oid));
		windowkey->nullsFirst = (bool *) gpdb::GPDBAlloc(num_of_cols * sizeof(bool));
		TranslateSortCols(sort_col_list_dxlnode, &child_context, windowkey->sortColIdx, windowkey->sortOperators, windowkey->nullsFirst);

		// translate the window frame specified in the window key
		windowkey->frame = NULL;
		if (NULL != window_key_dxl_op->GetWindowFrame())
		{
			windowkey->frame = Pwindowframe(window_key_dxl_op->GetWindowFrame(), &child_context, output_context, plan);
		}
		window->windowKeys = gpdb::LAppend(window->windowKeys, windowkey);
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) window;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToPlStmt::Pwindowframe
//
//	@doc:
//		Translate the DXL window frame into GPDB Window frame node
//
//---------------------------------------------------------------------------
WindowFrame *
CTranslatorDXLToPlStmt::Pwindowframe
	(
	const CDXLWindowFrame *pdxlwf,
	const CDXLTranslateContext *pdxltrctxChild,
	CDXLTranslateContext *output_context,
	Plan *pplan
	)
{
	WindowFrame *window_frame = MakeNode(WindowFrame);

	if (EdxlfsRow == pdxlwf->ParseDXLFrameSpec())
	{
		window_frame->is_rows = true;
	}
	else
	{
		window_frame->is_rows = false;
	}
	window_frame->is_between = true;

	window_frame->exclude = CTranslatorUtils::Windowexclusion(pdxlwf->ParseFrameExclusionStrategy());

	// translate the CDXLNodes representing the leading and trailing edge
	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(pdxltrctxChild);

			CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
			(
			 m_mp,
			 NULL,
			 child_contexts,
			 output_context,
			 m_dxl_to_plstmt_context
			);

	CDXLNode *win_frame_leading_dxlnode = pdxlwf->PdxlnLeading();
	window_frame->lead = MakeNode(WindowFrameEdge);
	window_frame->lead->kind = CTranslatorUtils::Windowboundkind(CDXLScalarWindowFrameEdge::Cast(win_frame_leading_dxlnode->GetOperator())->ParseDXLFrameBoundary());
	window_frame->lead->val = NULL;
	if (0 != win_frame_leading_dxlnode->Arity())
	{
		window_frame->lead->val = (Node*) m_translator_dxl_to_scalar->TranslateDXLToScalar((*win_frame_leading_dxlnode)[0], &colid_var_mapping);
	}


	CDXLNode *win_frame_trailing_dxlnode = pdxlwf->PdxlnTrailing();
	window_frame->trail = MakeNode(WindowFrameEdge);
	window_frame->trail->kind = CTranslatorUtils::Windowboundkind(CDXLScalarWindowFrameEdge::Cast(win_frame_trailing_dxlnode->GetOperator())->ParseDXLFrameBoundary());
	window_frame->trail->val = NULL;
	if (0 != win_frame_trailing_dxlnode->Arity())
	{
		window_frame->trail->val = (Node*) m_translator_dxl_to_scalar->TranslateDXLToScalar((*win_frame_trailing_dxlnode)[0], &colid_var_mapping);
	}

	// cleanup
	child_contexts->Release();

	return window_frame;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create sort plan node
	Sort *sort = MakeNode(Sort);

	Plan *plan = &(sort->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalSort *sort_dxlop = CDXLPhysicalSort::Cast(sort_dxlnode->GetOperator());

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
	CDXLNode *child_dxlnode = (*sort_dxlnode)[EdxlsortIndexChild];
	CDXLNode *project_list_dxlnode = (*sort_dxlnode)[EdxlsortIndexProjList];
	CDXLNode *filter_dxlnode = (*sort_dxlnode)[EdxlsortIndexFilter];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	// set sorting info
	sort->noduplicates = sort_dxlop->FDiscardDuplicates();

	// translate sorting columns

	const CDXLNode *sort_col_list_dxl = (*sort_dxlnode)[EdxlsortIndexSortColList];

	const ULONG num_of_cols = sort_col_list_dxl->Arity();
	sort->numCols = num_of_cols;
	sort->sortColIdx = (AttrNumber *) gpdb::GPDBAlloc(num_of_cols * sizeof(AttrNumber));
	sort->sortOperators = (Oid *) gpdb::GPDBAlloc(num_of_cols * sizeof(Oid));
	sort->nullsFirst = (bool *) gpdb::GPDBAlloc(num_of_cols * sizeof(bool));

	TranslateSortCols(sort_col_list_dxl, &child_context, sort->sortColIdx, sort->sortOperators, sort->nullsFirst);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) sort;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create sort plan node
	SubqueryScan *subquery_scan = MakeNode(SubqueryScan);

	Plan *plan = &(subquery_scan->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalSubqueryScan *subquery_scan_dxlop = CDXLPhysicalSubqueryScan::Cast(subquery_scan_dxlnode->GetOperator());

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
	CDXLNode *child_dxlnode = (*subquery_scan_dxlnode)[EdxlsubqscanIndexChild];
	CDXLNode *project_list_dxlnode = (*subquery_scan_dxlnode)[EdxlsubqscanIndexProjList];
	CDXLNode *filter_dxlnode = (*subquery_scan_dxlnode)[EdxlsubqscanIndexFilter];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	// create an rtable entry for the subquery scan
	RangeTblEntry *rte = MakeNode(RangeTblEntry);
	rte->rtekind = RTE_SUBQUERY;

	Alias *alias = MakeNode(Alias);
	alias->colnames = NIL;

	// get table alias
	alias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(subquery_scan_dxlop->MdName()->GetMDName()->GetBuffer());

	// get column names from child project list
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	(subquery_scan->scan).scanrelid = index;
	base_table_context.SetRelIndex(index);

	ListCell *lc_tgtentry = NULL;

	CDXLNode *child_proj_list_dxlnode = (*child_dxlnode)[0];

	ULONG ul = 0;

	ForEach (lc_tgtentry, child_plan->targetlist)
	{
		TargetEntry *target_entry = (TargetEntry *) lfirst(lc_tgtentry);

		// non-system attribute
		CHAR *col_name_char_array = PStrDup(target_entry->resname);
		Value *val_colname = gpdb::MakeStringValue(col_name_char_array);
		alias->colnames = gpdb::LAppend(alias->colnames, val_colname);

		// get corresponding child project element
		CDXLScalarProjElem *sc_proj_elem_dxlop = CDXLScalarProjElem::Cast((*child_proj_list_dxlnode)[ul]->GetOperator());

		// save mapping col id -> index in translate context
		(void) base_table_context.InsertMapping(sc_proj_elem_dxlop->Id(), target_entry->resno);
		ul++;
	}

	rte->eref = alias;

	// add range table entry for the subquery to the list
	m_dxl_to_plstmt_context->AddRTE(rte);

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		&base_table_context,		// translate context for the base table
		NULL,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	subquery_scan->subplan = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	SetParamIds(plan);
	return (Plan *) subquery_scan;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create result plan node
	Result *result = MakeNode(Result);

	Plan *plan = &(result->plan);
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

	CDXLNode *child_dxlnode = NULL;
	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	if (result_dxlnode->Arity() - 1 == EdxlresultIndexChild)
	{
		// translate child plan
		child_dxlnode = (*result_dxlnode)[EdxlresultIndexChild];

		Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

		GPOS_ASSERT(NULL != child_plan && "child plan cannot be NULL");

		result->plan.lefttree = child_plan;

		plan->nMotionNodes = child_plan->nMotionNodes;
	}

	CDXLNode *project_list_dxlnode = (*result_dxlnode)[EdxlresultIndexProjList];
	CDXLNode *filter_dxlnode = (*result_dxlnode)[EdxlresultIndexFilter];
	CDXLNode *one_time_filter_dxlnode = (*result_dxlnode)[EdxlresultIndexOneTimeFilter];

	List *quals_list = NULL;

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,		// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&quals_list,
		output_context
		);

	// translate one time filter
	List *one_time_quals_list = TranslateDXLFilterToQual
							(
							one_time_filter_dxlnode,
							NULL,			// base table translation context
							child_contexts,
							output_context
							);

	plan->qual = quals_list;

	result->resconstantqual = (Node *) one_time_quals_list;

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) result;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	PartitionSelector *partition_selector = MakeNode(PartitionSelector);

	Plan *plan = &(partition_selector->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalPartitionSelector *partition_selector_dxlop = CDXLPhysicalPartitionSelector::Cast(partition_selector_dxlnode->GetOperator());
	const ULONG num_of_levels = partition_selector_dxlop->GetPartitioningLevel();
	partition_selector->nLevels = num_of_levels;
	partition_selector->scanId = partition_selector_dxlop->ScanId();
	partition_selector->relid = CMDIdGPDB::CastMdid(partition_selector_dxlop->GetRelMdId())->Oid();
	partition_selector->selectorId = m_partition_selector_counter++;

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

	CDXLNode *child_dxlnode = NULL;
	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	BOOL has_childs = (EdxlpsIndexChild == partition_selector_dxlnode->Arity() - 1);
	if (has_childs)
	{
		// translate child plan
		child_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexChild];

		Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);
		GPOS_ASSERT(NULL != child_plan && "child plan cannot be NULL");

		partition_selector->plan.lefttree = child_plan;
		plan->nMotionNodes = child_plan->nMotionNodes;
	}

	child_contexts->Append(&child_context);

	CDXLNode *project_list_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexProjList];
	CDXLNode *eq_filters_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexEqFilters];
	CDXLNode *filters_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexFilters];
	CDXLNode *residual_filter_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexResidualFilter];
	CDXLNode *proj_expr_dxlnode = (*partition_selector_dxlnode)[EdxlpsIndexPropExpr];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxlnode, NULL /*base_table_context*/, child_contexts, output_context);

	// translate filter lists
	GPOS_ASSERT(eq_filters_dxlnode->Arity() == num_of_levels);
	partition_selector->levelEqExpressions = TranslateDXLFilterList(eq_filters_dxlnode, NULL /*base_table_context*/, child_contexts, output_context);

	GPOS_ASSERT(filters_dxlnode->Arity() == num_of_levels);
	partition_selector->levelExpressions = TranslateDXLFilterList(filters_dxlnode, NULL /*base_table_context*/, child_contexts, output_context);

	//translate residual filter
	CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt(m_mp, NULL /*base_table_context*/, child_contexts, output_context, m_dxl_to_plstmt_context);
	if (!m_translator_dxl_to_scalar->HasConstTrue(residual_filter_dxlnode, m_md_accessor))
	{
		partition_selector->residualPredicate = (Node *) m_translator_dxl_to_scalar->TranslateDXLToScalar(residual_filter_dxlnode, &colid_var_mapping);
	}

	//translate propagation expression
	if (!m_translator_dxl_to_scalar->HasConstNull(proj_expr_dxlnode))
	{
		partition_selector->propagationExpression = (Node *) m_translator_dxl_to_scalar->TranslateDXLToScalar(proj_expr_dxlnode, &colid_var_mapping);
	}

	// no need to translate printable filter - since it is not needed by the executor

	partition_selector->staticPartOids = NIL;
	partition_selector->staticScanIds = NIL;
	partition_selector->staticSelection = !has_childs;

	if (partition_selector->staticSelection)
	{
		SelectedParts *sp = gpdb::RunStaticPartitionSelection(partition_selector);
		partition_selector->staticPartOids = sp->partOids;
		partition_selector->staticScanIds = sp->scanIds;
		gpdb::GPDBFree(sp);
	}
	else
	{
		// if we cannot do static elimination then add this partitioned table oid
		// to the planned stmt so we can ship the constraints with the plan
		m_dxl_to_plstmt_context->AddPartitionedTable(partition_selector->relid);
	}

	// increment the number of partition selectors for the given scan id
	m_dxl_to_plstmt_context->IncrementPartitionSelectors(partition_selector->scanId);

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) partition_selector;
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
	CDXLTranslationContextArray *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	GPOS_ASSERT(EdxlopScalarOpList == filter_list_dxlnode->GetOperator()->GetDXLOperator());

	List *filters_list = NIL;

	CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt(m_mp, base_table_context, child_contexts, output_context, m_dxl_to_plstmt_context);
	const ULONG arity = filter_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *child_filter_dxlnode = (*filter_list_dxlnode)[ul];

		if (m_translator_dxl_to_scalar->HasConstTrue(child_filter_dxlnode, m_md_accessor))
		{
			filters_list = gpdb::LAppend(filters_list, NULL /*datum*/);
			continue;
		}

		Expr *filter_expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(child_filter_dxlnode, &colid_var_mapping);
		filters_list = gpdb::LAppend(filters_list, filter_expr);
	}

	return filters_list;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create append plan node
	Append *append = MakeNode(Append);

	Plan *plan = &(append->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalAppend *pdxlopAppend = CDXLPhysicalAppend::Cast(append_dxlnode->GetOperator());

	append->isTarget = pdxlopAppend->IsUsedInUpdDel();
	append->isZapped = pdxlopAppend->IsZapped();

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
	append->appendplans = NIL;
	
	// translate children
	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());
	for (ULONG ul = EdxlappendIndexFirstChild; ul < arity; ul++)
	{
		CDXLNode *child_dxlnode = (*append_dxlnode)[ul];

		Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

		GPOS_ASSERT(NULL != child_plan && "child plan cannot be NULL");

		append->appendplans = gpdb::LAppend(append->appendplans, child_plan);
		plan->nMotionNodes += child_plan->nMotionNodes;
	}

	CDXLNode *project_list_dxlnode = (*append_dxlnode)[EdxlappendIndexProjList];
	CDXLNode *filter_dxlnode = (*append_dxlnode)[EdxlappendIndexFilter];

	plan->targetlist = NIL;
	const ULONG length = project_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < length; ++ul)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == proj_elem_dxlnode->GetOperator()->GetDXLOperator());

		CDXLScalarProjElem *sc_proj_elem_dxlop = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());
		GPOS_ASSERT(1 == proj_elem_dxlnode->Arity());

		// translate proj element expression
		CDXLNode *expr_dxlnode = (*proj_elem_dxlnode)[0];
		CDXLScalarIdent *sc_ident_dxlop = CDXLScalarIdent::Cast(expr_dxlnode->GetOperator());

		Index idxVarno = OUTER;
		AttrNumber attno = (AttrNumber) (ul + 1);

		Var *var = gpdb::MakeVar
							(
							idxVarno,
							attno,
							CMDIdGPDB::CastMdid(sc_ident_dxlop->MdidType())->Oid(),
							sc_ident_dxlop->TypeModifier(),
							0	// varlevelsup
							);

		TargetEntry *target_entry = MakeNode(TargetEntry);
		target_entry->expr = (Expr *) var;
		target_entry->resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(sc_proj_elem_dxlop->GetMdNameAlias()->GetMDName()->GetBuffer());
		target_entry->resno = attno;

		// add column mapping to output translation context
		output_context->InsertMapping(sc_proj_elem_dxlop->Id(), target_entry);

		plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);
	}

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
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

	return (Plan *) append;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create materialize plan node
	Material *materialize = MakeNode(Material);

	Plan *plan = &(materialize->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalMaterialize *materialize_dxlop = CDXLPhysicalMaterialize::Cast(materialize_dxlnode->GetOperator());

	materialize->cdb_strict = materialize_dxlop->IsEager();

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
	CDXLNode *child_dxlnode = (*materialize_dxlnode)[EdxlmatIndexChild];

	CDXLNode *project_list_dxlnode = (*materialize_dxlnode)[EdxlmatIndexProjList];
	CDXLNode *filter_dxlnode = (*materialize_dxlnode)[EdxlmatIndexFilter];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list and filter
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	// set spooling info
	if (materialize_dxlop->IsSpooling())
	{
		materialize->share_id = materialize_dxlop->GetSpoolingOpId();
		materialize->driver_slice = materialize_dxlop->GetExecutorSlice();
		materialize->nsharer_xslice = materialize_dxlop->GetNumConsumerSlices();
		materialize->share_type = (0 < materialize_dxlop->GetNumConsumerSlices()) ?
							SHARE_MATERIAL_XSLICE : SHARE_MATERIAL;
	}
	else
	{
		materialize->share_type = SHARE_NOTSHARED;
	}

	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) materialize;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTEProducer *cte_prod_dxlop = CDXLPhysicalCTEProducer::Cast(cte_producer_dxlnode->GetOperator());
	ULONG cte_id = cte_prod_dxlop->Id();

	// create the shared input scan representing the CTE Producer
	ShareInputScan *shared_input_scan = MakeNode(ShareInputScan);
	shared_input_scan->share_id = cte_id;
	Plan *plan = &(shared_input_scan->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	// store share scan node for the translation of CTE Consumers
	m_dxl_to_plstmt_context->AddCTEConsumerInfo(cte_id, shared_input_scan);

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
	CDXLNode *project_list_dxlnode = (*cte_producer_dxlnode)[0];
	CDXLNode *child_dxlnode = (*cte_producer_dxlnode)[1];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());
	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);
	GPOS_ASSERT(NULL != child_plan && "child plan cannot be NULL");

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&child_context);
	// translate proj list
	plan->targetlist = TranslateDXLProjList
							(
							project_list_dxlnode,
							NULL,		// base table translation context
							child_contexts,
							output_context
							);

	// if the child node is neither a sort or materialize node then add a materialize node
	if (!IsA(child_plan, Material) && !IsA(child_plan, Sort))
	{
		Material *materialize = MakeNode(Material);
		materialize->cdb_strict = false; // eager-free

		Plan *materialize_plan = &(materialize->plan);
		materialize_plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

		TranslatePlanCosts
			(
			CDXLPhysicalProperties::PdxlpropConvert(cte_producer_dxlnode->GetProperties())->GetDXLOperatorCost(),
			&(materialize_plan->startup_cost),
			&(materialize_plan->total_cost),
			&(materialize_plan->plan_rows),
			&(materialize_plan->plan_width)
			);

		// create a target list for the newly added materialize
		ListCell *lc_target_entry = NULL;
		materialize_plan->targetlist = NIL;
		ForEach (lc_target_entry, plan->targetlist)
		{
			TargetEntry *target_entry = (TargetEntry *) lfirst(lc_target_entry);
			Expr *expr = target_entry->expr;
			GPOS_ASSERT(IsA(expr, Var));

			Var *var = (Var *) expr;
			Var *var_new = gpdb::MakeVar(OUTER, var->varattno, var->vartype, var->vartypmod,	0 /* varlevelsup */);
			var_new->varnoold = var->varnoold;
			var_new->varoattno = var->varoattno;

			TargetEntry *te_new = gpdb::MakeTargetEntry((Expr *) var_new, var->varattno, PStrDup(target_entry->resname), target_entry->resjunk);
			materialize_plan->targetlist = gpdb::LAppend(materialize_plan->targetlist, te_new);
		}

		materialize_plan->lefttree = child_plan;
		materialize_plan->nMotionNodes = child_plan->nMotionNodes;

		child_plan = materialize_plan;
	}

	InitializeSpoolingInfo(child_plan, cte_id);

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;
	plan->qual = NIL;
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) shared_input_scan;
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

	Flow *flow = GetFlowCTEConsumer(shared_scan_cte_consumer_list);

	const ULONG num_of_shared_scan = gpdb::ListLength(shared_scan_cte_consumer_list);

	ShareType share_type = SHARE_NOTSHARED;

	if (IsA(plan, Material))
	{
		Material *materialize = (Material *) plan;
		materialize->share_id = share_id;
		materialize->nsharer = num_of_shared_scan;
		share_type = SHARE_MATERIAL;
		// the share_type is later reset to SHARE_MATERIAL_XSLICE (if needed) by the apply_shareinput_xslice
		materialize->share_type = share_type;
		GPOS_ASSERT(NULL == (materialize->plan).flow);
		(materialize->plan).flow = flow;
	}
	else
	{
		GPOS_ASSERT(IsA(plan, Sort));
		Sort *sort = (Sort *) plan;
		sort->share_id = share_id;
		sort->nsharer = num_of_shared_scan;
		share_type = SHARE_SORT;
		// the share_type is later reset to SHARE_SORT_XSLICE (if needed) the apply_shareinput_xslice
		sort->share_type = share_type;
		GPOS_ASSERT(NULL == (sort->plan).flow);
		(sort->plan).flow = flow;
	}

	GPOS_ASSERT(SHARE_NOTSHARED != share_type);

	// set the share type of the consumer nodes based on the producer
	ListCell *lc_sh_scan_cte_consumer = NULL;
	ForEach (lc_sh_scan_cte_consumer, shared_scan_cte_consumer_list)
	{
		ShareInputScan *share_input_scan_consumer = (ShareInputScan *) lfirst(lc_sh_scan_cte_consumer);
		share_input_scan_consumer->share_type = share_type;
		share_input_scan_consumer->driver_slice = -1; // default
		if (NULL == (share_input_scan_consumer->scan.plan).flow)
		{
			(share_input_scan_consumer->scan.plan).flow = (Flow *) gpdb::CopyObject(flow);
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
	Flow *flow = NULL;

	ListCell *lc_sh_scan_cte_consumer = NULL;
	ForEach (lc_sh_scan_cte_consumer, shared_scan_cte_consumer_list)
	{
		ShareInputScan *share_input_scan_consumer = (ShareInputScan *) lfirst(lc_sh_scan_cte_consumer);
		Flow *flow_cte = (share_input_scan_consumer->scan.plan).flow;
		if (NULL != flow_cte)
		{
			if (NULL == flow)
			{
				flow = (Flow *) gpdb::CopyObject(flow_cte);
			}
			else
			{
				GPOS_ASSERT(flow->flotype == flow_cte->flotype);
			}
		}
	}

	if (NULL == flow)
	{
		flow = MakeNode(Flow);
		flow->flotype = FLOW_UNDEFINED; // default flow
	}

	return flow;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTEConsumer *cte_consumer_dxlop = CDXLPhysicalCTEConsumer::Cast(cte_consumer_dxlnode->GetOperator());
	ULONG cte_id = cte_consumer_dxlop->Id();

	ShareInputScan *share_input_scan_cte_consumer = MakeNode(ShareInputScan);
	share_input_scan_cte_consumer->share_id = cte_id;

	Plan *plan = &(share_input_scan_cte_consumer->scan.plan);
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
	ULongPtrArray *output_colids_array = cte_consumer_dxlop->GetOutputColIdsArray();
#endif

	// generate the target list of the CTE Consumer
	plan->targetlist = NIL;
	CDXLNode *project_list_dxlnode = (*cte_consumer_dxlnode)[0];
	const ULONG num_of_proj_list_elem = project_list_dxlnode->Arity();
	GPOS_ASSERT(num_of_proj_list_elem == output_colids_array->Size());
	for (ULONG ul = 0; ul < num_of_proj_list_elem; ul++)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		CDXLScalarProjElem *sc_proj_elem_dxlop = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());
		ULONG colid = sc_proj_elem_dxlop->Id();
		GPOS_ASSERT(colid == *(*output_colids_array)[ul]);

		CDXLNode *sc_ident_dxlnode = (*proj_elem_dxlnode)[0];
		CDXLScalarIdent *sc_ident_dxlop = CDXLScalarIdent::Cast(sc_ident_dxlnode->GetOperator());
		OID oid_type = CMDIdGPDB::CastMdid(sc_ident_dxlop->MdidType())->Oid();

		Var *var = gpdb::MakeVar(OUTER, (AttrNumber) (ul + 1), oid_type, sc_ident_dxlop->TypeModifier(),  0	/* varlevelsup */);

		CHAR *resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(sc_proj_elem_dxlop->GetMdNameAlias()->GetMDName()->GetBuffer());
		TargetEntry *target_entry = gpdb::MakeTargetEntry((Expr *) var, (AttrNumber) (ul + 1), resname, false /* resjunk */);
		plan->targetlist = gpdb::LAppend(plan->targetlist, target_entry);

		output_context->InsertMapping(colid, target_entry);
	}

	plan->qual = NULL;

	SetParamIds(plan);

	// store share scan node for the translation of CTE Consumers
	m_dxl_to_plstmt_context->AddCTEConsumerInfo(cte_id, share_input_scan_cte_consumer);

	return (Plan *) share_input_scan_cte_consumer;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
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

	CDXLNode *last_child_dxlnode = (*sequence_dxlnode)[arity - 1];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *last_child_plan = TranslateDXLOperatorToPlan(last_child_dxlnode, &child_context, ctxt_translation_prev_siblings);
	plan->nMotionNodes = last_child_plan->nMotionNodes;

	CDXLNode *project_list_dxlnode = (*sequence_dxlnode)[0];

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list
	plan->targetlist = TranslateDXLProjList
						(
						project_list_dxlnode,
						NULL,		// base table translation context
						child_contexts,
						output_context
						);

	// translate the rest of the children
	for (ULONG ul = 1; ul < arity - 1; ul++)
	{
		CDXLNode *child_dxlnode = (*sequence_dxlnode)[ul];

		Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

		psequence->subplans = gpdb::LAppend(psequence->subplans, child_plan);
		plan->nMotionNodes += child_plan->nMotionNodes;
	}

	psequence->subplans = gpdb::LAppend(psequence->subplans, last_child_plan);

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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDynamicTableScan *dyn_tbl_scan_dxlop = CDXLPhysicalDynamicTableScan::Cast(dyn_tbl_scan_dxlnode->GetOperator());

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(dyn_tbl_scan_dxlop->GetDXLTableDescr(), NULL /*index_descr_dxl*/, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;

	m_dxl_to_plstmt_context->AddRTE(rte);

	// create dynamic scan node
	DynamicTableScan *dyn_tbl_scan = MakeNode(DynamicTableScan);

	dyn_tbl_scan->scanrelid = index;
	dyn_tbl_scan->partIndex = dyn_tbl_scan_dxlop->GetPartIndexId();
	dyn_tbl_scan->partIndexPrintable = dyn_tbl_scan_dxlop->GetPartIndexIdPrintable();

	Plan *plan = &(dyn_tbl_scan->plan);
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
	CDXLNode *project_list_dxlnode = (*dyn_tbl_scan_dxlnode)[EdxltsIndexProjList];
	CDXLNode *filter_dxlnode = (*dyn_tbl_scan_dxlnode)[EdxltsIndexFilter];

	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		&base_table_context,	// translate context for the base table
		NULL,			// translate_ctxt_left and pdxltrctxRight,
		&plan->targetlist,
		&plan->qual,
		output_context
		);

	SetParamIds(plan);

	return (Plan *) dyn_tbl_scan;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalDynamicIndexScan *dyn_index_scan_dxlop = CDXLPhysicalDynamicIndexScan::Cast(dyn_idx_scan_dxlnode->GetOperator());
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(dyn_index_scan_dxlop->GetDXLTableDescr()->MDId());
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(dyn_index_scan_dxlop->GetDXLTableDescr(), NULL /*index_descr_dxl*/, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;
	m_dxl_to_plstmt_context->AddRTE(rte);

	DynamicIndexScan *dyn_idx_scan = MakeNode(DynamicIndexScan);

	dyn_idx_scan->scan.scanrelid = index;
	dyn_idx_scan->scan.partIndex = dyn_index_scan_dxlop->GetPartIndexId();
	dyn_idx_scan->scan.partIndexPrintable = dyn_index_scan_dxlop->GetPartIndexIdPrintable();

	CMDIdGPDB *mdid_index = CMDIdGPDB::CastMdid(dyn_index_scan_dxlop->GetDXLIndexDescr()->MDId());
	const IMDIndex *md_index = m_md_accessor->RetrieveIndex(mdid_index);
	Oid index_oid = mdid_index->Oid();

	GPOS_ASSERT(InvalidOid != index_oid);
	dyn_idx_scan->indexid = index_oid;
	dyn_idx_scan->logicalIndexInfo = gpdb::GetLogicalIndexInfo(rte->relid, index_oid);

	Plan *plan = &(dyn_idx_scan->scan.plan);
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
	CDXLNode *project_list_dxlnode = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexProjList];
	CDXLNode *filter_dxlnode = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexFilter];
	CDXLNode *index_cond_list_dxlnode = (*dyn_idx_scan_dxlnode)[CDXLPhysicalDynamicIndexScan::EdxldisIndexCondition];

	// translate proj list
	plan->targetlist = TranslateDXLProjList(project_list_dxlnode, &base_table_context, NULL /*child_contexts*/, output_context);

	// translate index filter
	plan->qual = TranslateDXLIndexFilter
					(
					filter_dxlnode,
					output_context,
					&base_table_context,
					ctxt_translation_prev_siblings
					);

	dyn_idx_scan->indexorderdir = CTranslatorUtils::GetScanDirection(dyn_index_scan_dxlop->GetIndexScanDir());

	// translate index condition list
	List *index_cond = NIL;
	List *index_orig_cond = NIL;
	List *index_strategy_list = NIL;
	List *index_subtype_list = NIL;

	TranslateIndexConditions
		(
		index_cond_list_dxlnode,
		dyn_index_scan_dxlop->GetDXLTableDescr(),
		false, // is_index_only_scan
		md_index,
		md_rel,
		output_context,
		&base_table_context,
		ctxt_translation_prev_siblings,
		&index_cond,
		&index_orig_cond,
		&index_strategy_list,
		&index_subtype_list
		);


	dyn_idx_scan->indexqual = index_cond;
	dyn_idx_scan->indexqualorig = index_orig_cond;
	dyn_idx_scan->indexstrategy = index_strategy_list;
	dyn_idx_scan->indexsubtype = index_subtype_list;
	SetParamIds(plan);

	return (Plan *) dyn_idx_scan;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translate table descriptor into a range table entry
	CDXLPhysicalDML *phy_dml_dxlop = CDXLPhysicalDML::Cast(dml_dxlnode->GetOperator());

	// create DML node
	DML *dml = MakeNode(DML);
	Plan *plan = &(dml->plan);
	AclMode acl_mode = ACL_NO_RIGHTS;
	
	switch (phy_dml_dxlop->GetDmlOpType())
	{
		case gpdxl::Edxldmldelete:
		{
			m_cmd_type = CMD_DELETE;
			acl_mode = ACL_DELETE;
			break;
		}
		case gpdxl::Edxldmlupdate:
		{
			m_cmd_type = CMD_UPDATE;
			acl_mode = ACL_UPDATE;
			break;
		}
		case gpdxl::Edxldmlinsert:
		{
			m_cmd_type = CMD_INSERT;
			acl_mode = ACL_INSERT;
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
	
	IMDId *mdid_target_table = phy_dml_dxlop->GetDXLTableDescr()->MDId();
	if (IMDRelation::EreldistrMasterOnly != m_md_accessor->RetrieveRel(mdid_target_table)->GetRelDistribution())
	{
		m_is_tgt_tbl_distributed = true;
	}
	
	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;
	dml->scanrelid = index;
	
	m_result_rel_list = gpdb::LAppendInt(m_result_rel_list, index);

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(phy_dml_dxlop->GetDXLTableDescr()->MDId());

	CDXLTableDescr *table_descr = phy_dml_dxlop->GetDXLTableDescr();
	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(table_descr, NULL /*index_descr_dxl*/, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= acl_mode;
	m_dxl_to_plstmt_context->AddRTE(rte);
	
	CDXLNode *project_list_dxlnode = (*dml_dxlnode)[0];
	CDXLNode *child_dxlnode = (*dml_dxlnode)[1];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&child_context);

	// translate proj list
	List *dml_target_list = TranslateDXLProjList
		(
		project_list_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);
	
	if (md_rel->HasDroppedColumns())
	{
		// pad DML target list with NULLs for dropped columns for all DML operator types
		List *target_list_with_dropped_cols = CreateTargetListWithNullsForDroppedCols(dml_target_list, md_rel);
		gpdb::GPDBFree(dml_target_list);
		dml_target_list = target_list_with_dropped_cols;
	}

	// Extract column numbers of the action and ctid columns from the
	// target list. ORCA also includes a third similar column for
	// partition Oid to the target list, but we don't use it for anything
	// in GPDB.
	dml->actionColIdx = AddTargetEntryForColId(&dml_target_list, &child_context, phy_dml_dxlop->ActionColId(), true /*is_resjunk*/);
	dml->ctidColIdx = AddTargetEntryForColId(&dml_target_list, &child_context, phy_dml_dxlop->GetCtIdColId(), true /*is_resjunk*/);
	if (phy_dml_dxlop->IsOidsPreserved())
	{
		dml->tupleoidColIdx = AddTargetEntryForColId(&dml_target_list, &child_context, phy_dml_dxlop->GetTupleOid(), true /*is_resjunk*/);
	}
	else
	{
		dml->tupleoidColIdx = 0;
	}

	GPOS_ASSERT(0 != dml->actionColIdx);

	plan->targetlist = dml_target_list;
	
	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	if (CMD_INSERT == m_cmd_type && 0 == plan->nMotionNodes)
	{
		List *direct_dispatch_segids = TranslateDXLDirectDispatchInfo(phy_dml_dxlop->GetDXLDirectDispatchInfo());
		plan->directDispatch.contentIds = direct_dispatch_segids;
		plan->directDispatch.isDirectDispatch = (NIL != direct_dispatch_segids);
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

	return (Plan *) dml;
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
	
	CDXLDatum2dArray *dispatch_identifier_datum_arrays = dxl_direct_dispatch_info->GetDispatchIdentifierDatumArray();
	
	if (dispatch_identifier_datum_arrays == NULL || 0 == dispatch_identifier_datum_arrays->Size())
	{
		return NIL;
	}
	
	CDXLDatumArray *dxl_datum_array = (*dispatch_identifier_datum_arrays)[0];
	GPOS_ASSERT(0 < dxl_datum_array->Size());
		
	ULONG hash_code = GetDXLDatumGPDBHash(dxl_datum_array);
	const ULONG length = dispatch_identifier_datum_arrays->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLDatumArray *dispatch_identifier_datum_array = (*dispatch_identifier_datum_arrays)[ul];
		GPOS_ASSERT(0 < dispatch_identifier_datum_array->Size());
		ULONG hash_code_new = GetDXLDatumGPDBHash(dispatch_identifier_datum_array);
		
		if (hash_code != hash_code_new)
		{
			// values don't hash to the same segment
			return NIL;
		}
	}
	
	List *segids_list = gpdb::LAppendInt(NIL, hash_code);
	return segids_list;
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
	CDXLDatumArray *dxl_datum_array
	)
{
	List *consts_list = NIL;
	
	const ULONG length = dxl_datum_array->Size();
	
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLDatum *datum_dxl = (*dxl_datum_array)[ul];
		
		Const *const_expr = (Const *) m_translator_dxl_to_scalar->TranslateDXLDatumToScalar(datum_dxl);
		consts_list = gpdb::LAppend(consts_list, const_expr);
	}

	ULONG hash = gpdb::CdbHashConstList(consts_list, m_num_of_segments);

	gpdb::ListFreeDeep(consts_list);
	
	return hash;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalSplit *phy_split_dxlop = CDXLPhysicalSplit::Cast(split_dxlnode->GetOperator());

	// create SplitUpdate node
	SplitUpdate *split = MakeNode(SplitUpdate);
	Plan *plan = &(split->plan);
	
	CDXLNode *project_list_dxlnode = (*split_dxlnode)[0];
	CDXLNode *child_dxlnode = (*split_dxlnode)[1];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&child_context);

	// translate proj list and filter
	plan->targetlist = TranslateDXLProjList
		(
		project_list_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);

	// translate delete and insert columns
	ULongPtrArray *deletion_colid_array = phy_split_dxlop->GetDeletionColIdArray();
	ULongPtrArray *insertion_colid_array = phy_split_dxlop->GetInsertionColIdArray();
		
	GPOS_ASSERT(insertion_colid_array->Size() == deletion_colid_array->Size());
	
	split->deleteColIdx = CTranslatorUtils::ConvertColidToAttnos(deletion_colid_array, &child_context);
	split->insertColIdx = CTranslatorUtils::ConvertColidToAttnos(insertion_colid_array, &child_context);
	
	const TargetEntry *te_action_col = output_context->GetTargetEntry(phy_split_dxlop->ActionColId());
	const TargetEntry *te_ctid_col = output_context->GetTargetEntry(phy_split_dxlop->GetCtIdColId());
	const TargetEntry *te_tuple_oid_col = output_context->GetTargetEntry(phy_split_dxlop->GetTupleOid());

	if (NULL  == te_action_col)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, phy_split_dxlop->ActionColId());
	}
	if (NULL  == te_ctid_col)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, phy_split_dxlop->GetCtIdColId());
	}	 
	
	split->actionColIdx = te_action_col->resno;
	split->ctidColIdx = te_ctid_col->resno;
	
	split->tupleoidColIdx = FirstLowInvalidHeapAttributeNumber;
	if (NULL != te_tuple_oid_col)
	{
		split->tupleoidColIdx = te_tuple_oid_col->resno;
	}

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;
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

	return (Plan *) split;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// create assert plan node
	AssertOp *assert_node = MakeNode(AssertOp);

	Plan *plan = &(assert_node->plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();

	CDXLPhysicalAssert *assert_dxlop = CDXLPhysicalAssert::Cast(assert_dxlnode->GetOperator());

	// translate error code into the its internal GPDB representation
	const CHAR *error_code = assert_dxlop->GetSQLState();
	GPOS_ASSERT(GPOS_SQLSTATE_LENGTH == clib::Strlen(error_code));
	
	assert_node->errcode = MAKE_SQLSTATE(error_code[0], error_code[1], error_code[2], error_code[3], error_code[4]);
	CDXLNode *filter_dxlnode = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexFilter];

	assert_node->errmessage = CTranslatorUtils::GetAssertErrorMsgs(filter_dxlnode);

	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(assert_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	// translate child plan
	CDXLNode *child_dxlnode = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexChild];
	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	GPOS_ASSERT(NULL != child_plan && "child plan cannot be NULL");

	assert_node->plan.lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;

	CDXLNode *project_list_dxlnode = (*assert_dxlnode)[CDXLPhysicalAssert::EdxlassertIndexProjList];

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(const_cast<CDXLTranslateContext*>(&child_context));

	// translate proj list
	plan->targetlist = TranslateDXLProjList
				(
				project_list_dxlnode,
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
	
	GPOS_ASSERT(gpdb::ListLength(plan->qual) == gpdb::ListLength(assert_node->errmessage));
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();

	return (Plan *) assert_node;
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalRowTrigger *phy_row_trigger_dxlop = CDXLPhysicalRowTrigger::Cast(row_trigger_dxlnode->GetOperator());

	// create RowTrigger node
	RowTrigger *row_trigger = MakeNode(RowTrigger);
	Plan *plan = &(row_trigger->plan);

	CDXLNode *project_list_dxlnode = (*row_trigger_dxlnode)[0];
	CDXLNode *child_dxlnode = (*row_trigger_dxlnode)[1];

	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *child_plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&child_context);

	// translate proj list and filter
	plan->targetlist = TranslateDXLProjList
		(
		project_list_dxlnode,
		NULL,			// translate context for the base table
		child_contexts,
		output_context
		);

	Oid relid_oid = CMDIdGPDB::CastMdid(phy_row_trigger_dxlop->GetRelMdId())->Oid();
	GPOS_ASSERT(InvalidOid != relid_oid);
	row_trigger->relid = relid_oid;
	row_trigger->eventFlags = phy_row_trigger_dxlop->GetType();

	// translate old and new columns
	ULongPtrArray *colids_old_array = phy_row_trigger_dxlop->GetColIdsOld();
	ULongPtrArray *colids_new_array = phy_row_trigger_dxlop->GetColIdsNew();

	GPOS_ASSERT_IMP(NULL != colids_old_array && NULL != colids_new_array,
					colids_new_array->Size() == colids_old_array->Size());

	if (NULL == colids_old_array)
	{
		row_trigger->oldValuesColIdx = NIL;
	}
	else
	{
		row_trigger->oldValuesColIdx = CTranslatorUtils::ConvertColidToAttnos(colids_old_array, &child_context);
	}

	if (NULL == colids_new_array)
	{
		row_trigger->newValuesColIdx = NIL;
	}
	else
	{
		row_trigger->newValuesColIdx = CTranslatorUtils::ConvertColidToAttnos(colids_new_array, &child_context);
	}

	plan->lefttree = child_plan;
	plan->nMotionNodes = child_plan->nMotionNodes;
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

	return (Plan *) row_trigger;
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
	const ULONG num_of_non_sys_cols = CTranslatorUtils::GetNumNonSystemColumns(md_rel);

	RangeTblEntry *rte = MakeNode(RangeTblEntry);
	rte->rtekind = RTE_RELATION;

	// get the index if given
	const IMDIndex *md_index = NULL;
	if (NULL != index_descr_dxl)
	{
		md_index = m_md_accessor->RetrieveIndex(index_descr_dxl->MDId());
	}

	// get oid for table
	Oid oid = CMDIdGPDB::CastMdid(table_descr->MDId())->Oid();
	GPOS_ASSERT(InvalidOid != oid);

	rte->relid = oid;
	rte->checkAsUser = table_descr->GetExecuteAsUserId();
	rte->requiredPerms |= ACL_NO_RIGHTS;

	// save oid and range index in translation context
	base_table_context->SetOID(oid);
	base_table_context->SetRelIndex(index);

	Alias *alias = MakeNode(Alias);
	alias->colnames = NIL;

	// get table alias
	alias->aliasname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(table_descr->MdName()->GetMDName()->GetBuffer());

	// get column names
	const ULONG arity = table_descr->Arity();
	
	INT last_attno = 0;
	
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		const CDXLColDescr *dxl_col_descr = table_descr->GetColumnDescrAt(ul);
		GPOS_ASSERT(NULL != dxl_col_descr);

		INT attno = dxl_col_descr->AttrNum();

		GPOS_ASSERT(0 != attno);

		if (0 < attno)
		{
			// if attno > last_attno + 1, there were dropped attributes
			// add those to the RTE as they are required by GPDB
			for (INT dropped_col_attno = last_attno + 1; dropped_col_attno < attno; dropped_col_attno++)
			{
				Value *val_dropped_colname = gpdb::MakeStringValue(PStrDup(""));
				alias->colnames = gpdb::LAppend(alias->colnames, val_dropped_colname);
			}
			
			// non-system attribute
			CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_col_descr->MdName()->GetMDName()->GetBuffer());
			Value *val_colname = gpdb::MakeStringValue(col_name_char_array);

			alias->colnames = gpdb::LAppend(alias->colnames, val_colname);
			last_attno = attno;
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
	for (ULONG ul = last_attno + 1; ul <= num_of_non_sys_cols; ul++)
	{
		Value *val_dropped_colname = gpdb::MakeStringValue(PStrDup(""));
		alias->colnames = gpdb::LAppend(alias->colnames, val_dropped_colname);
	}
	
	rte->eref = alias;

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
//		For other nodes translate_ctxt_left and pdxltrctxRight give
//		the mappings of column ids to target entries in the corresponding child nodes
//		for resolving the origin of the target entries
//
//---------------------------------------------------------------------------
List *
CTranslatorDXLToPlStmt::TranslateDXLProjList
	(
	const CDXLNode *project_list_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	CDXLTranslationContextArray *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	if (NULL == project_list_dxlnode)
	{
		return NULL;
	}

	List *target_list = NIL;

	// translate each DXL project element into a target entry
	const ULONG arity = project_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		GPOS_ASSERT(EdxlopScalarProjectElem == proj_elem_dxlnode->GetOperator()->GetDXLOperator());
		CDXLScalarProjElem *sc_proj_elem_dxlop = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());
		GPOS_ASSERT(1 == proj_elem_dxlnode->Arity());

		// translate proj element expression
		CDXLNode *expr_dxlnode = (*proj_elem_dxlnode)[0];

		CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
																(
																m_mp,
																base_table_context,
																child_contexts,
																output_context,
																m_dxl_to_plstmt_context
																);

		Expr *expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(expr_dxlnode, &colid_var_mapping);

		GPOS_ASSERT(NULL != expr);

		TargetEntry *target_entry = MakeNode(TargetEntry);
		target_entry->expr = expr;
		target_entry->resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(sc_proj_elem_dxlop->GetMdNameAlias()->GetMDName()->GetBuffer());
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
				ULONG colid = CDXLScalarIdent::Cast(expr_dxlnode->GetOperator())->GetDXLColRef()->Id();

				const CDXLTranslateContext *translate_ctxt_left = (*child_contexts)[0];
				GPOS_ASSERT(NULL != translate_ctxt_left);
				const TargetEntry *pteOriginal = translate_ctxt_left->GetTargetEntry(colid);

				if (NULL == pteOriginal)
				{
					// variable not found on the left side
					GPOS_ASSERT(2 == child_contexts->Size());
					const CDXLTranslateContext *pdxltrctxRight = (*child_contexts)[1];

					GPOS_ASSERT(NULL != pdxltrctxRight);
					pteOriginal = pdxltrctxRight->GetTargetEntry(colid);
				}

				if (NULL  == pteOriginal)
				{
					GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, colid);
				}	
				target_entry->resorigtbl = pteOriginal->resorigtbl;
				target_entry->resorigcol = pteOriginal->resorigcol;
			}
		}

		// add column mapping to output translation context
		output_context->InsertMapping(sc_proj_elem_dxlop->Id(), target_entry);

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

	List *result_list = NIL;
	ULONG last_tgt_elem = 0;
	ULONG resno = 1;
	
	const ULONG num_of_rel_cols = md_rel->ColumnCount();
	
	for (ULONG ul = 0; ul < num_of_rel_cols; ul++)
	{
		const IMDColumn *md_col = md_rel->GetMdCol(ul);
		
		if (md_col->IsSystemColumn())
		{
			continue;
		}
		
		Expr *expr = NULL;
		if (md_col->IsDropped())
		{
			// add a NULL element
			OID oid_type = CMDIdGPDB::CastMdid(m_md_accessor->PtMDType<IMDTypeInt4>()->MDId())->Oid();

			expr = (Expr *) gpdb::MakeNULLConst(oid_type);
		}
		else
		{
			TargetEntry *target_entry = (TargetEntry *) gpdb::ListNth(target_list, last_tgt_elem);
			expr = (Expr *) gpdb::CopyObject(target_entry->expr);
			last_tgt_elem++;
		}
		
		CHAR *name_str = CTranslatorUtils::CreateMultiByteCharStringFromWCString(md_col->Mdname().GetMDName()->GetBuffer());
		TargetEntry *te_new = gpdb::MakeTargetEntry(expr, resno, name_str, false /*resjunk*/);
		result_list = gpdb::LAppend(result_list, te_new);
		resno++;
	}
	
	return result_list;
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
	const CDXLNode *project_list_dxlnode,
	CDXLTranslateContext *child_context,
	CDXLTranslateContext *output_context
	)
{
	List *target_list = NIL;
	const ULONG arity = project_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *proj_elem_dxlnode = (*project_list_dxlnode)[ul];
		CDXLScalarProjElem *sc_proj_elem_dxlop = CDXLScalarProjElem::Cast(proj_elem_dxlnode->GetOperator());

		const TargetEntry *te_child = child_context->GetTargetEntry(sc_proj_elem_dxlop->Id());
		if (NULL  == te_child)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, sc_proj_elem_dxlop->Id());
		}	

		// get type oid for project element's expression
		GPOS_ASSERT(1 == proj_elem_dxlnode->Arity());

		// find column type
		OID oid_type = gpdb::ExprType((Node*) te_child->expr);
		INT type_modifier = gpdb::ExprTypeMod((Node *) te_child->expr);

		// find the original varno and attno for this column
		Index idx_varnoold = 0;
		AttrNumber attno_old = 0;

		if (IsA(te_child->expr, Var))
		{
			Var *pv = (Var*) te_child->expr;
			idx_varnoold = pv->varnoold;
			attno_old = pv->varoattno;
		}
		else
		{
			idx_varnoold = OUTER;
			attno_old = te_child->resno;
		}

		// create a Var expression for this target list entry expression
		Var *var = gpdb::MakeVar
					(
					OUTER,
					te_child->resno,
					oid_type,
					type_modifier,
					0	// varlevelsup
					);

		// set old varno and varattno since makeVar does not set them
		var->varnoold = idx_varnoold;
		var->varoattno = attno_old;

		CHAR *resname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(sc_proj_elem_dxlop->GetMdNameAlias()->GetMDName()->GetBuffer());

		TargetEntry *target_entry = gpdb::MakeTargetEntry
							(
							(Expr *) var,
							(AttrNumber) (ul + 1),
							resname,
							false		// resjunk
							);

		target_list = gpdb::LAppend(target_list, target_entry);
		output_context->InsertMapping(sc_proj_elem_dxlop->Id(), target_entry);
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
	CDXLTranslationContextArray *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	const ULONG arity = filter_dxlnode->Arity();
	if (0 == arity)
	{
		return NIL;
	}

	GPOS_ASSERT(1 == arity);

	CDXLNode *filter_cond_dxlnode = (*filter_dxlnode)[0];
	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(filter_cond_dxlnode, m_md_accessor));

	return TranslateDXLScCondToQual(filter_cond_dxlnode, base_table_context, child_contexts, output_context);
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
	const CDXLNode *condition_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	CDXLTranslationContextArray *child_contexts,
	CDXLTranslateContext *output_context
	)
{
	List *quals_list = NIL;

	GPOS_ASSERT(CTranslatorDXLToScalar::HasBoolResult(const_cast<CDXLNode*>(condition_dxlnode), m_md_accessor));

	CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
															(
															m_mp,
															base_table_context,
															child_contexts,
															output_context,
															m_dxl_to_plstmt_context
															);

	Expr *expr = m_translator_dxl_to_scalar->TranslateDXLToScalar
					(
					condition_dxlnode,
					&colid_var_mapping
					);

	quals_list = gpdb::LAppend(quals_list, expr);

	return quals_list;
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
	const CDXLNode *project_list_dxlnode,
	const CDXLNode *filter_dxlnode,
	const CDXLTranslateContextBaseTable *base_table_context,
	CDXLTranslationContextArray *child_contexts,
	List **targetlist_out,
	List **qual_out,
	CDXLTranslateContext *output_context
	)
{
	// translate proj list
	*targetlist_out = TranslateDXLProjList
						(
						project_list_dxlnode,
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

	List *hash_expr_list = NIL;
	List *hash_expr_types_list = NIL;

	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(child_context);

	const ULONG arity = hash_expr_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *hash_expr_dxlnode = (*hash_expr_list_dxlnode)[ul];
		CDXLScalarHashExpr *hash_expr_dxlop = CDXLScalarHashExpr::Cast(hash_expr_dxlnode->GetOperator());

		// the type of the hash expression in GPDB is computed as the left operand 
		// of the equality operator of the actual hash expression type
		const IMDType *md_type = m_md_accessor->RetrieveType(hash_expr_dxlop->MdidType());
		const IMDScalarOp *md_scalar_op = m_md_accessor->RetrieveScOp(md_type->GetMdidForCmpType(IMDType::EcmptEq));
		
		const IMDId *mdid_hash_type = md_scalar_op->GetLeftMdid();
		
		hash_expr_types_list = gpdb::LAppendOid(hash_expr_types_list, CMDIdGPDB::CastMdid(mdid_hash_type)->Oid());

		GPOS_ASSERT(1 == hash_expr_dxlnode->Arity());
		CDXLNode *expr_dxlnode = (*hash_expr_dxlnode)[0];

		CMappingColIdVarPlStmt colid_var_mapping = CMappingColIdVarPlStmt
																(
																m_mp,
																NULL,
																child_contexts,
																output_context,
																m_dxl_to_plstmt_context
																);

		Expr *expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(expr_dxlnode, &colid_var_mapping);

		hash_expr_list = gpdb::LAppend(hash_expr_list, expr);

		GPOS_ASSERT((ULONG) gpdb::ListLength(hash_expr_list) == ul + 1);
	}


	*hash_expr_out_list = hash_expr_list;
	*hash_expr_types_out_list = hash_expr_types_list;

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
	bool *is_nulls_first
	)
{
	const ULONG arity = sort_col_list_dxl->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *sort_col_dxlnode = (*sort_col_list_dxl)[ul];
		CDXLScalarSortCol *sc_sort_col_dxlop = CDXLScalarSortCol::Cast(sort_col_dxlnode->GetOperator());

		ULONG sort_colid = sc_sort_col_dxlop->GetColId();
		const TargetEntry *te_sort_col = child_context->GetTargetEntry(sort_colid);
		if (NULL  == te_sort_col)
		{
			GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtAttributeNotFound, sort_colid);
		}	

		att_no_sort_colids[ul] = te_sort_col->resno;
		sort_op_oids[ul] = CMDIdGPDB::CastMdid(sc_sort_col_dxlop->GetMdIdSortOp())->Oid();
		is_nulls_first[ul] = sc_sort_col_dxlop->IsSortedNullsFirst();
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

	CDXLPhysicalDML *phy_dml_dxlop = CDXLPhysicalDML::Cast(dxlop);
	IMDId *mdid = phy_dml_dxlop->GetDXLTableDescr()->MDId();

	return IMDRelation::EreldistrMasterOnly != m_md_accessor->RetrieveRel(mdid)->GetRelDistribution();
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
	CDXLTranslateContext *dxl_translate_ctxt,
	ULONG colid,
	BOOL is_resjunk
	)
{
	GPOS_ASSERT(NULL != target_list);
	
	const TargetEntry *target_entry = dxl_translate_ctxt->GetTargetEntry(colid);
	
	if (NULL == target_entry)
	{
		// colid not found in translate context
		return 0;
	}
	
	// TODO: Oct 29, 2012; see if entry already exists in the target list
	
	OID expr_oid = gpdb::ExprType((Node*) target_entry->expr);
	INT type_modifier = gpdb::ExprTypeMod((Node *) target_entry->expr);
	Var *var = gpdb::MakeVar
						(
						OUTER,
						target_entry->resno,
						expr_oid,
						type_modifier,
						0	// varlevelsup
						);
	ULONG resno = gpdb::ListLength(*target_list) + 1;
	CHAR *resname_str = PStrDup(target_entry->resname);
	TargetEntry *te_new = gpdb::MakeTargetEntry((Expr*) var, resno, resname_str, is_resjunk);
	*target_list = gpdb::LAppend(*target_list, te_new);
	
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
			jt = JOIN_IN;
			break;
		case EdxljtLeftAntiSemijoin:
			jt = JOIN_LASJ;
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
	const CDXLPhysicalCTAS *phy_ctas_dxlop,
	List *target_list
	)
{
	GPOS_ASSERT(NULL != target_list);

	IntPtrArray *var_type_mod_array = phy_ctas_dxlop->GetVarTypeModArray();
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
	const CDXLNode *ctas_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	CDXLPhysicalCTAS *phy_ctas_dxlop = CDXLPhysicalCTAS::Cast(ctas_dxlnode->GetOperator());
	CDXLNode *project_list_dxlnode = (*ctas_dxlnode)[0];
	CDXLNode *child_dxlnode = (*ctas_dxlnode)[1];

	GPOS_ASSERT(NULL == phy_ctas_dxlop->GetDxlCtasStorageOption()->GetDXLCtasOptionArray());
	
	CDXLTranslateContext child_context(m_mp, false, output_context->GetColIdToParamIdMap());

	Plan *plan = TranslateDXLOperatorToPlan(child_dxlnode, &child_context, ctxt_translation_prev_siblings);
	
	// fix target list to match the required column names
	CDXLTranslationContextArray *child_contexts = GPOS_NEW(m_mp) CDXLTranslationContextArray(m_mp);
	child_contexts->Append(&child_context);
	
	List *target_list = TranslateDXLProjList
						(
						project_list_dxlnode,
						NULL,		// base_table_context
						child_contexts,
						output_context
						);
	SetVarTypMod(phy_ctas_dxlop, target_list);
	
	SetParamIds(plan);

	// cleanup
	child_contexts->Release();


	// translate operator costs
	TranslatePlanCosts
		(
		CDXLPhysicalProperties::PdxlpropConvert(ctas_dxlnode->GetProperties())->GetDXLOperatorCost(),
		&(plan->startup_cost),
		&(plan->total_cost),
		&(plan->plan_rows),
		&(plan->plan_width)
		);

	IntoClause *into_clause = TranslateDXLPhyCtasToIntoClause(phy_ctas_dxlop);
	GpPolicy *distr_policy = TranslateDXLPhyCtasToDistrPolicy(phy_ctas_dxlop);
	m_dxl_to_plstmt_context->AddCtasInfo(into_clause, distr_policy);
	
	GPOS_ASSERT(IMDRelation::EreldistrMasterOnly != phy_ctas_dxlop->Ereldistrpolicy());
	
	m_is_tgt_tbl_distributed = true;
	
	// Add a result node on top with the correct projection list
	Result *result = MakeNode(Result);
	Plan *result_plan = &(result->plan);
	result_plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	result_plan->nMotionNodes = plan->nMotionNodes;
	result_plan->lefttree = plan;

	result_plan->targetlist = target_list;
	SetParamIds(result_plan);

	plan = (Plan *) result;

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
	const CDXLPhysicalCTAS *phy_ctas_dxlop
	)
{
	IntoClause *into_clause = MakeNode(IntoClause);
	into_clause->rel = MakeNode(RangeVar);
	into_clause->rel->istemp = phy_ctas_dxlop->IsTemporary();
	into_clause->rel->relname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(phy_ctas_dxlop->MdName()->GetMDName()->GetBuffer());
	into_clause->rel->schemaname = NULL;
	if (NULL != phy_ctas_dxlop->GetMdNameSchema())
	{
		into_clause->rel->schemaname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(phy_ctas_dxlop->GetMdNameSchema()->GetMDName()->GetBuffer());
	}
	
	CDXLCtasStorageOptions *dxl_ctas_storage_option = phy_ctas_dxlop->GetDxlCtasStorageOption();
	if (NULL != dxl_ctas_storage_option->GetMdNameTableSpace())
	{
		into_clause->tableSpaceName = CTranslatorUtils::CreateMultiByteCharStringFromWCString(phy_ctas_dxlop->GetDxlCtasStorageOption()->GetMdNameTableSpace()->GetMDName()->GetBuffer());
	}
	
	into_clause->onCommit = (OnCommitAction) dxl_ctas_storage_option->GetOnCommitAction();
	into_clause->options = TranslateDXLCtasStorageOptions(dxl_ctas_storage_option->GetDXLCtasOptionArray());
	
	// get column names
	CDXLColDescrArray *dxl_col_descr_array = phy_ctas_dxlop->GetDXLColumnDescrArray();
	const ULONG num_of_cols = dxl_col_descr_array->Size();
	into_clause->colNames = NIL;
	for (ULONG ul = 0; ul < num_of_cols; ++ul)
	{
		const CDXLColDescr *dxl_col_descr = (*dxl_col_descr_array)[ul];

		CHAR *col_name_char_array = CTranslatorUtils::CreateMultiByteCharStringFromWCString(dxl_col_descr->MdName()->GetMDName()->GetBuffer());
		
		ColumnDef *col_def = MakeNode(ColumnDef);
		col_def->colname = col_name_char_array;
		col_def->is_local = true;

		into_clause->colNames = gpdb::LAppend(into_clause->colNames, col_def);

	}

	return into_clause;
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
	ULongPtrArray *distr_col_pos_array = dxlop->GetDistrColPosArray();

	const ULONG num_of_distr_cols = (distr_col_pos_array == NULL) ? 0 : distr_col_pos_array->Size();

	ULONG num_of_distr_cols_alloc = 1;
	if (0 < num_of_distr_cols)
	{
		num_of_distr_cols_alloc = num_of_distr_cols;
	}
	
	GpPolicy *distr_policy = (GpPolicy *) gpdb::GPDBAlloc(sizeof(GpPolicy) +
															num_of_distr_cols * sizeof(AttrNumber));
	GPOS_ASSERT(IMDRelation::EreldistrHash == dxlop->Ereldistrpolicy() ||
				IMDRelation::EreldistrRandom == dxlop->Ereldistrpolicy());
	
	distr_policy->ptype = POLICYTYPE_PARTITIONED;
	distr_policy->nattrs = 0;
	if (IMDRelation::EreldistrHash == dxlop->Ereldistrpolicy())
	{
		
		GPOS_ASSERT(0 < num_of_distr_cols);
		distr_policy->nattrs = num_of_distr_cols;
		
		for (ULONG ul = 0; ul < num_of_distr_cols; ul++)
		{
			ULONG col_pos_idx = *((*distr_col_pos_array)[ul]);
			distr_policy->attrs[ul] = col_pos_idx + 1;
		}
	}
	return distr_policy;
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
	CDXLCtasStorageOptions::CDXLCtasOptionArray *ctas_storage_options
	)
{
	if (NULL == ctas_storage_options)
	{
		return NIL;
	}
	
	const ULONG num_of_options = ctas_storage_options->Size();
	List *options = NIL;
	for (ULONG ul = 0; ul < num_of_options; ul++)
	{
		CDXLCtasStorageOptions::CDXLCtasOption *pdxlopt = (*ctas_storage_options)[ul];
		CWStringBase *str_name = pdxlopt->m_str_name;
		CWStringBase *str_value = pdxlopt->m_str_value;
		DefElem *def_elem = MakeNode(DefElem);
		def_elem->defname = CTranslatorUtils::CreateMultiByteCharStringFromWCString(str_name->GetBuffer());

		if (!pdxlopt->m_is_null)
		{
			NodeTag arg_type = (NodeTag) pdxlopt->m_type;

			GPOS_ASSERT(T_Integer == arg_type || T_String == arg_type);
			if (T_Integer == arg_type)
			{
				def_elem->arg = (Node *) gpdb::MakeIntegerValue(CTranslatorUtils::GetLongFromStr(str_value));
			}
			else
			{
				def_elem->arg = (Node *) gpdb::MakeStringValue(CTranslatorUtils::CreateMultiByteCharStringFromWCString(str_value->GetBuffer()));
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	ULONG part_index_id = INVALID_PART_INDEX;
	ULONG part_idx_printable_id = INVALID_PART_INDEX;
	const CDXLTableDescr *table_descr = NULL;
	BOOL is_dynamic = false;

	CDXLOperator *dxl_operator = bitmapscan_dxlnode->GetOperator();
	if (EdxlopPhysicalBitmapTableScan == dxl_operator->GetDXLOperator())
	{
		table_descr = CDXLPhysicalBitmapTableScan::Cast(dxl_operator)->GetDXLTableDescr();
	}
	else
	{
		GPOS_ASSERT(EdxlopPhysicalDynamicBitmapTableScan == dxl_operator->GetDXLOperator());
		CDXLPhysicalDynamicBitmapTableScan *phy_dyn_bitmap_tblscan_dxlop =
				CDXLPhysicalDynamicBitmapTableScan::Cast(dxl_operator);
		table_descr = phy_dyn_bitmap_tblscan_dxlop->GetDXLTableDescr();

		part_index_id = phy_dyn_bitmap_tblscan_dxlop->GetPartIndexId();
		part_idx_printable_id = phy_dyn_bitmap_tblscan_dxlop->GetPartIndexIdPrintable();
		is_dynamic = true;
	}

	// translation context for column mappings in the base relation
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	const IMDRelation *md_rel = m_md_accessor->RetrieveRel(table_descr->MDId());

	RangeTblEntry *rte = TranslateDXLTblDescrToRangeTblEntry(table_descr, NULL /*index_descr_dxl*/, index, &base_table_context);
	GPOS_ASSERT(NULL != rte);
	rte->requiredPerms |= ACL_SELECT;

	m_dxl_to_plstmt_context->AddRTE(rte);

	BitmapTableScan *bitmap_tbl_scan = MakeNode(BitmapTableScan);
	bitmap_tbl_scan->scan.scanrelid = index;
	bitmap_tbl_scan->scan.partIndex = part_index_id;
	bitmap_tbl_scan->scan.partIndexPrintable = part_idx_printable_id;

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
	CDXLNode *project_list_dxlnode = (*bitmapscan_dxlnode)[0];
	CDXLNode *filter_dxlnode = (*bitmapscan_dxlnode)[1];
	CDXLNode *recheck_cond_dxlnode = (*bitmapscan_dxlnode)[2];
	CDXLNode *bitmap_access_path_dxlnode = (*bitmapscan_dxlnode)[3];

	List *quals_list = NULL;
	TranslateProjListAndFilter
		(
		project_list_dxlnode,
		filter_dxlnode,
		&base_table_context,	// translate context for the base table
		ctxt_translation_prev_siblings,
		&plan->targetlist,
		&quals_list,
		output_context
		);
	plan->qual = quals_list;

	bitmap_tbl_scan->bitmapqualorig = TranslateDXLFilterToQual
							(
							recheck_cond_dxlnode,
							&base_table_context,
							ctxt_translation_prev_siblings,
							output_context
							);

	bitmap_tbl_scan->scan.plan.lefttree = TranslateDXLBitmapAccessPath
								(
								bitmap_access_path_dxlnode,
								output_context,
								md_rel,
								table_descr,
								&base_table_context,
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	Edxlopid dxl_op_id = bitmap_access_path_dxlnode->GetOperator()->GetDXLOperator();
	if (EdxlopScalarBitmapIndexProbe == dxl_op_id)
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
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == dxl_op_id);

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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	GPOS_ASSERT(NULL != bitmap_boolop_dxlnode);
	GPOS_ASSERT(EdxlopScalarBitmapBoolOp == bitmap_boolop_dxlnode->GetOperator()->GetDXLOperator());

	CDXLScalarBitmapBoolOp *sc_bitmap_boolop_dxlop = CDXLScalarBitmapBoolOp::Cast(bitmap_boolop_dxlnode->GetOperator());
	
	CDXLNode *left_tree_dxlnode = (*bitmap_boolop_dxlnode)[0];
	CDXLNode *right_tree_dxlnode = (*bitmap_boolop_dxlnode)[1];
	
	Plan *left_plan = TranslateDXLBitmapAccessPath
						(
						left_tree_dxlnode,
						output_context,
						md_rel,
						table_descr,
						base_table_context,
						ctxt_translation_prev_siblings,
						bitmap_tbl_scan
						);
	Plan *right_plan = TranslateDXLBitmapAccessPath
						(
						right_tree_dxlnode,
						output_context,
						md_rel,
						table_descr,
						base_table_context,
						ctxt_translation_prev_siblings,
						bitmap_tbl_scan
						);
	List *child_plan_list = ListMake2(left_plan, right_plan);

	Plan *plan = NULL;
	
	if (CDXLScalarBitmapBoolOp::EdxlbitmapAnd == sc_bitmap_boolop_dxlop->GetDXLBitmapOpType())
	{
		BitmapAnd *bitmapand = MakeNode(BitmapAnd);
		bitmapand->bitmapplans = child_plan_list;
		bitmapand->plan.targetlist = NULL;
		bitmapand->plan.qual = NULL;
		plan = (Plan *) bitmapand;
	}
	else
	{
		BitmapOr *bitmapor = MakeNode(BitmapOr);
		bitmapor->bitmapplans = child_plan_list;
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
//		Translate CDXLScalarBitmapIndexProbe into BitmapIndexScan
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
	CDXLTranslationContextArray *ctxt_translation_prev_siblings,
	BitmapTableScan *bitmap_tbl_scan
	)
{
	CDXLScalarBitmapIndexProbe *sc_bitmap_idx_probe_dxlop =
			CDXLScalarBitmapIndexProbe::Cast(bitmap_index_probe_dxlnode->GetOperator());

	BitmapIndexScan *bitmap_idx_scan = MakeNode(BitmapIndexScan);
	bitmap_idx_scan->scan.scanrelid = bitmap_tbl_scan->scan.scanrelid;
	bitmap_idx_scan->scan.partIndex = bitmap_tbl_scan->scan.partIndex;

	CMDIdGPDB *mdid_index = CMDIdGPDB::CastMdid(sc_bitmap_idx_probe_dxlop->GetDXLIndexDescr()->MDId());
	const IMDIndex *index = m_md_accessor->RetrieveIndex(mdid_index);
	Oid index_oid = mdid_index->Oid();

	GPOS_ASSERT(InvalidOid != index_oid);
	bitmap_idx_scan->indexid = index_oid;
	OID rel_oid = CMDIdGPDB::CastMdid(table_descr->MDId())->Oid();
	bitmap_idx_scan->logicalIndexInfo = gpdb::GetLogicalIndexInfo(rel_oid, index_oid);
	Plan *plan = &(bitmap_idx_scan->scan.plan);
	plan->plan_node_id = m_dxl_to_plstmt_context->GetNextPlanId();
	plan->nMotionNodes = 0;

	GPOS_ASSERT(1 == bitmap_index_probe_dxlnode->Arity());
	CDXLNode *index_cond_list_dxlnode = (*bitmap_index_probe_dxlnode)[0];
	List *index_cond = NIL;
	List *index_orig_cond = NIL;
	List *index_strategy_list = NIL;
	List *index_subtype_list = NIL;

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
		&index_cond,
		&index_orig_cond,
		&index_strategy_list,
		&index_subtype_list
		);

	bitmap_idx_scan->indexqual = index_cond;
	bitmap_idx_scan->indexqualorig = index_orig_cond;
	bitmap_idx_scan->indexstrategy = index_strategy_list;
	bitmap_idx_scan->indexsubtype = index_subtype_list;
	SetParamIds(plan);

	return plan;
}

// translates a DXL Value Scan node into a GPDB Value scan node
Plan *
CTranslatorDXLToPlStmt::TranslateDXLValueScan
	(
	const CDXLNode *value_scan_dxlnode,
	CDXLTranslateContext *output_context,
	CDXLTranslationContextArray *ctxt_translation_prev_siblings
	)
{
	// translation context for column mappings
	CDXLTranslateContextBaseTable base_table_context(m_mp);

	// we will add the new range table entry as the last element of the range table
	Index index = gpdb::ListLength(m_dxl_to_plstmt_context->GetRTableEntriesList()) + 1;

	base_table_context.SetRelIndex(index);

	// create value scan node
	ValuesScan *value_scan = MakeNode(ValuesScan);
	value_scan->scan.scanrelid = index;
	Plan *plan = &(value_scan->scan.plan);

	RangeTblEntry *rte = TranslateDXLValueScanToRangeTblEntry(value_scan_dxlnode, output_context, &base_table_context);
	GPOS_ASSERT(NULL != rte);

	value_scan->values_lists = (List *)gpdb::CopyObject(rte->values_lists);

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

	CDXLNode *project_list_dxlnode = (*value_scan_dxlnode)[EdxltsIndexProjList];

	// translate proj list
	List *target_list = TranslateDXLProjList
							(
							project_list_dxlnode,
							&base_table_context,
							NULL,
							output_context
							);

	plan->targetlist = target_list;

	return (Plan *) value_scan;
}


// EOF
