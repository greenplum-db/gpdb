//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorExprToDXL.cpp
//
//	@doc:
//		Implementation of the methods for translating Optimizer physical expression
//		trees into DXL.
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/common/CAutoTimer.h"
#include "gpos/common/CHashMap.h"

#include "naucrates/md/IMDCast.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDRelationCtasGPDB.h"

#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"
#include "naucrates/dxl/operators/CDXLWindowFrame.h"
#include "naucrates/dxl/operators/dxlops.h"

#include "naucrates/statistics/CStatistics.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/cost/ICostModel.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDAccessorUtils.h"
#include "gpopt/operators/CPhysicalAgg.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"
#include "gpopt/operators/CPhysicalIndexOnlyScan.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXLUtils.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CCastUtils.h"

#include "naucrates/base/IDatumInt8.h"
#include "naucrates/base/CDatumBoolGPDB.h"

#include "naucrates/traceflags/traceflags.h"

using namespace gpos;
using namespace gpmd;
using namespace gpdxl;
using namespace gpopt;
using namespace gpnaucrates;

#define GPOPT_MASTER_SEGMENT_ID (-1)

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::CTranslatorExprToDXL
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CTranslatorExprToDXL::CTranslatorExprToDXL
	(
	CMemoryPool *mp,
	CMDAccessor *md_accessor,
	IntPtrArray *pdrgpiSegments,
	BOOL fInitColumnFactory
	)
	:
	m_mp(mp),
	m_pmda(md_accessor),
	m_pdpplan(NULL),
	m_pcf(NULL),
	m_pdrgpiSegments(pdrgpiSegments),
	m_iMasterId(GPOPT_MASTER_SEGMENT_ID)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != md_accessor);
	GPOS_ASSERT_IMP(NULL != pdrgpiSegments, (0 < pdrgpiSegments->Size()));

	InitScalarTranslators();
	InitPhysicalTranslators();

	// initialize hash map
	m_phmcrdxln = GPOS_NEW(m_mp) ColRefToDXLNodeMap(m_mp);

	m_phmcrdxlnIndexLookup = GPOS_NEW(m_mp) ColRefToDXLNodeMap(m_mp);

	if (fInitColumnFactory)
	{
		// get column factory from optimizer context object
		m_pcf = COptCtxt::PoctxtFromTLS()->Pcf();
		GPOS_ASSERT(NULL != m_pcf);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::~CTranslatorExprToDXL
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CTranslatorExprToDXL::~CTranslatorExprToDXL()
{
	CRefCount::SafeRelease(m_pdrgpiSegments);
	m_phmcrdxln->Release();
	m_phmcrdxlnIndexLookup->Release();
	CRefCount::SafeRelease(m_pdpplan);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::InitScalarTranslators
//
//	@doc:
//		Initialize index of scalar translators
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::InitScalarTranslators()
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_rgpfScalarTranslators); ul++)
	{
		m_rgpfScalarTranslators[ul] = NULL;
	}

	// array mapping operator type to translator function
	SScTranslatorMapping rgScalarTranslators[] = 
	{
			{COperator::EopScalarIdent, &gpopt::CTranslatorExprToDXL::PdxlnScId},
			{COperator::EopScalarCmp, &gpopt::CTranslatorExprToDXL::PdxlnScCmp},
			{COperator::EopScalarIsDistinctFrom, &gpopt::CTranslatorExprToDXL::PdxlnScDistinctCmp},
			{COperator::EopScalarOp, &gpopt::CTranslatorExprToDXL::PdxlnScOp},
			{COperator::EopScalarBoolOp, &gpopt::CTranslatorExprToDXL::PdxlnScBoolExpr},
			{COperator::EopScalarConst, &gpopt::CTranslatorExprToDXL::PdxlnScConst},
			{COperator::EopScalarFunc, &gpopt::CTranslatorExprToDXL::PdxlnScFuncExpr},
			{COperator::EopScalarWindowFunc, &gpopt::CTranslatorExprToDXL::PdxlnScWindowFuncExpr},
			{COperator::EopScalarAggFunc,&gpopt::CTranslatorExprToDXL::PdxlnScAggref},
			{COperator::EopScalarNullIf, &gpopt::CTranslatorExprToDXL::PdxlnScNullIf},
			{COperator::EopScalarNullTest, &gpopt::CTranslatorExprToDXL::PdxlnScNullTest},
			{COperator::EopScalarBooleanTest, &gpopt::CTranslatorExprToDXL::PdxlnScBooleanTest},
			{COperator::EopScalarIf, &gpopt::CTranslatorExprToDXL::PdxlnScIfStmt},
			{COperator::EopScalarSwitch, &gpopt::CTranslatorExprToDXL::PdxlnScSwitch},
			{COperator::EopScalarSwitchCase, &gpopt::CTranslatorExprToDXL::PdxlnScSwitchCase},
			{COperator::EopScalarCaseTest, &gpopt::CTranslatorExprToDXL::PdxlnScCaseTest},
			{COperator::EopScalarCoalesce, &gpopt::CTranslatorExprToDXL::PdxlnScCoalesce},
			{COperator::EopScalarMinMax, &gpopt::CTranslatorExprToDXL::PdxlnScMinMax},
			{COperator::EopScalarCast, &gpopt::CTranslatorExprToDXL::PdxlnScCast},
			{COperator::EopScalarCoerceToDomain, &gpopt::CTranslatorExprToDXL::PdxlnScCoerceToDomain},
			{COperator::EopScalarCoerceViaIO, &gpopt::CTranslatorExprToDXL::PdxlnScCoerceViaIO},
			{COperator::EopScalarArrayCoerceExpr, &gpopt::CTranslatorExprToDXL::PdxlnScArrayCoerceExpr},
			{COperator::EopScalarArray, &gpopt::CTranslatorExprToDXL::PdxlnArray},
			{COperator::EopScalarArrayCmp, &gpopt::CTranslatorExprToDXL::PdxlnArrayCmp},
			{COperator::EopScalarArrayRef, &gpopt::CTranslatorExprToDXL::PdxlnArrayRef},
			{COperator::EopScalarArrayRefIndexList, &gpopt::CTranslatorExprToDXL::PdxlnArrayRefIndexList},
			{COperator::EopScalarAssertConstraintList, &gpopt::CTranslatorExprToDXL::PdxlnAssertPredicate},
			{COperator::EopScalarAssertConstraint, &gpopt::CTranslatorExprToDXL::PdxlnAssertConstraint},
			{COperator::EopScalarDMLAction, &gpopt::CTranslatorExprToDXL::PdxlnDMLAction},
			{COperator::EopScalarBitmapIndexProbe, &gpopt::CTranslatorExprToDXL::PdxlnBitmapIndexProbe},
			{COperator::EopScalarBitmapBoolOp, &gpopt::CTranslatorExprToDXL::PdxlnBitmapBoolOp},
	};

	const ULONG translators_mapping_len = GPOS_ARRAY_SIZE(rgScalarTranslators);
	for (ULONG ul = 0; ul < translators_mapping_len; ul++)
	{
		SScTranslatorMapping elem = rgScalarTranslators[ul];
		m_rgpfScalarTranslators[elem.op_id] = elem.pf;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::InitPhysicalTranslators
//
//	@doc:
//		Initialize index of physical translators
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::InitPhysicalTranslators()
{
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_rgpfPhysicalTranslators); ul++)
	{
		m_rgpfPhysicalTranslators[ul] = NULL;
	}

	// array mapping operator type to translator function
	SPhTranslatorMapping rgPhysicalTranslators[] =
	{
			{COperator::EopPhysicalFilter, &gpopt::CTranslatorExprToDXL::PdxlnResult},
			{COperator::EopPhysicalIndexScan, &gpopt::CTranslatorExprToDXL::PdxlnIndexScan},
			{COperator::EopPhysicalIndexOnlyScan, &gpopt::CTranslatorExprToDXL::PdxlnIndexOnlyScan},
			{COperator::EopPhysicalBitmapTableScan, &gpopt::CTranslatorExprToDXL::PdxlnBitmapTableScan},
			{COperator::EopPhysicalComputeScalar, &gpopt::CTranslatorExprToDXL::PdxlnComputeScalar},
			{COperator::EopPhysicalScalarAgg, &gpopt::CTranslatorExprToDXL::PdxlnAggregate},
			{COperator::EopPhysicalHashAgg, &gpopt::CTranslatorExprToDXL::PdxlnAggregate},
			{COperator::EopPhysicalStreamAgg, &gpopt::CTranslatorExprToDXL::PdxlnAggregate},
			{COperator::EopPhysicalHashAggDeduplicate, &gpopt::CTranslatorExprToDXL::PdxlnAggregateDedup},
			{COperator::EopPhysicalStreamAggDeduplicate, &gpopt::CTranslatorExprToDXL::PdxlnAggregateDedup},
			{COperator::EopPhysicalSort, &gpopt::CTranslatorExprToDXL::PdxlnSort},
			{COperator::EopPhysicalLimit, &gpopt::CTranslatorExprToDXL::PdxlnLimit},
			{COperator::EopPhysicalSequenceProject, &gpopt::CTranslatorExprToDXL::PdxlnWindow},
			{COperator::EopPhysicalInnerNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalInnerIndexNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalLeftOuterIndexNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalCorrelatedInnerNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalLeftOuterNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalCorrelatedLeftOuterNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalCorrelatedLeftSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalCorrelatedInLeftSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalLeftSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalLeftAntiSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalLeftAntiSemiNLJoinNotIn, &gpopt::CTranslatorExprToDXL::PdxlnNLJoin},
			{COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin, &gpopt::CTranslatorExprToDXL::PdxlnCorrelatedNLJoin},
			{COperator::EopPhysicalInnerHashJoin, &gpopt::CTranslatorExprToDXL::PdxlnHashJoin},
			{COperator::EopPhysicalLeftOuterHashJoin, &gpopt::CTranslatorExprToDXL::PdxlnHashJoin},
			{COperator::EopPhysicalLeftSemiHashJoin, &gpopt::CTranslatorExprToDXL::PdxlnHashJoin},
			{COperator::EopPhysicalLeftAntiSemiHashJoin, &gpopt::CTranslatorExprToDXL::PdxlnHashJoin},
			{COperator::EopPhysicalLeftAntiSemiHashJoinNotIn, &gpopt::CTranslatorExprToDXL::PdxlnHashJoin},
			{COperator::EopPhysicalMotionGather,&gpopt::CTranslatorExprToDXL::PdxlnMotion},
			{COperator::EopPhysicalMotionBroadcast,&gpopt::CTranslatorExprToDXL::PdxlnMotion},
			{COperator::EopPhysicalMotionHashDistribute,&gpopt::CTranslatorExprToDXL::PdxlnMotion},
			{COperator::EopPhysicalMotionRoutedDistribute,&gpopt::CTranslatorExprToDXL::PdxlnMotion},
			{COperator::EopPhysicalMotionRandom, &gpopt::CTranslatorExprToDXL::PdxlnMotion},
			{COperator::EopPhysicalSpool, &gpopt::CTranslatorExprToDXL::PdxlnMaterialize},
			{COperator::EopPhysicalSequence, &gpopt::CTranslatorExprToDXL::PdxlnSequence},
			{COperator::EopPhysicalDynamicTableScan, &gpopt::CTranslatorExprToDXL::PdxlnDynamicTableScan},
			{COperator::EopPhysicalDynamicBitmapTableScan, &gpopt::CTranslatorExprToDXL::PdxlnDynamicBitmapTableScan},
			{COperator::EopPhysicalDynamicIndexScan, &gpopt::CTranslatorExprToDXL::PdxlnDynamicIndexScan},
			{COperator::EopPhysicalPartitionSelector, &gpopt::CTranslatorExprToDXL::PdxlnPartitionSelector},
			{COperator::EopPhysicalPartitionSelectorDML, &gpopt::CTranslatorExprToDXL::PdxlnPartitionSelectorDML},
			{COperator::EopPhysicalConstTableGet, &gpopt::CTranslatorExprToDXL::PdxlnResultFromConstTableGet},
			{COperator::EopPhysicalTVF, &gpopt::CTranslatorExprToDXL::PdxlnTVF},
			{COperator::EopPhysicalSerialUnionAll, &gpopt::CTranslatorExprToDXL::PdxlnAppend},
			{COperator::EopPhysicalParallelUnionAll, &gpopt::CTranslatorExprToDXL::PdxlnAppend},
			{COperator::EopPhysicalDML, &gpopt::CTranslatorExprToDXL::PdxlnDML},
			{COperator::EopPhysicalSplit, &gpopt::CTranslatorExprToDXL::PdxlnSplit},
			{COperator::EopPhysicalRowTrigger, &gpopt::CTranslatorExprToDXL::PdxlnRowTrigger},
			{COperator::EopPhysicalAssert, &gpopt::CTranslatorExprToDXL::PdxlnAssert},
			{COperator::EopPhysicalCTEProducer, &gpopt::CTranslatorExprToDXL::PdxlnCTEProducer},
			{COperator::EopPhysicalCTEConsumer, &gpopt::CTranslatorExprToDXL::PdxlnCTEConsumer},
			{COperator::EopPhysicalFullMergeJoin, &gpopt::CTranslatorExprToDXL::PdxlnMergeJoin},
	};

	const ULONG translators_mapping_len = GPOS_ARRAY_SIZE(rgPhysicalTranslators);
	for (ULONG ul = 0; ul < translators_mapping_len; ul++)
	{
		SPhTranslatorMapping elem = rgPhysicalTranslators[ul];
		m_rgpfPhysicalTranslators[elem.op_id] = elem.pf;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnTranslate
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnTranslate
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CMDNameArray *pdrgpmdname
	)
{
	CAutoTimer at("\n[OPT]: Expr To DXL Translation Time", GPOS_FTRACE(EopttracePrintOptimizationStatistics));

	GPOS_ASSERT(NULL == m_pdpplan);
	
	m_pdpplan = CDrvdPropPlan::Pdpplan(pexpr->PdpDerive());
	m_pdpplan->AddRef();

	CDistributionSpecArray *pdrgpdsBaseTables = GPOS_NEW(m_mp) CDistributionSpecArray(m_mp);
	ULONG ulNonGatherMotions = 0;
	BOOL fDML = false;
	CDXLNode *dxlnode = CreateDXLNode(pexpr, colref_array, pdrgpdsBaseTables, &ulNonGatherMotions, &fDML, true /*fRemap*/, true /*fRoot*/);

	if (fDML)
	{
		pdrgpdsBaseTables->Release();
		return dxlnode;
	}

	CDXLNode *pdxlnPrL = (*dxlnode)[0];
	GPOS_ASSERT(EdxlopScalarProjectList == pdxlnPrL->GetOperator()->GetDXLOperator());

	const ULONG length = pdrgpmdname->Size();
	GPOS_ASSERT(length == colref_array->Size());
	GPOS_ASSERT(length == pdxlnPrL->Arity());
	for (ULONG ul = 0; ul < length; ul++)
	{
		// desired output column name
		CMDName *mdname = GPOS_NEW(m_mp) CMDName(m_mp, (*pdrgpmdname)[ul]->GetMDName());

		// get the old project element for the ColId
		CDXLNode *pdxlnPrElOld = (*pdxlnPrL)[ul];
		CDXLScalarProjElem *pdxlopPrElOld = CDXLScalarProjElem::Cast(pdxlnPrElOld->GetOperator());
		GPOS_ASSERT(1 == pdxlnPrElOld->Arity());
		CDXLNode *child_dxlnode = (*pdxlnPrElOld)[0];
		const ULONG colid = pdxlopPrElOld->Id();

		// create a new project element node with the col id and new column name
		// and add the scalar child
		CDXLNode *pdxlnPrElNew = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarProjElem(m_mp, colid, mdname));
		child_dxlnode->AddRef();
		pdxlnPrElNew->AddChild(child_dxlnode);

		// replace the project element
		pdxlnPrL->ReplaceChild(ul, pdxlnPrElNew);
	}

	

	if (0 == ulNonGatherMotions)
	{
		CTranslatorExprToDXLUtils::SetDirectDispatchInfo(m_mp, m_pmda, dxlnode, pexpr, pdrgpdsBaseTables);
	}

	pdrgpdsBaseTables->Release();
	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::CreateDXLNode
//
//	@doc:
//		Translates an optimizer physical expression tree into DXL.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::CreateDXLNode
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	BOOL fRemap,
	BOOL fRoot
	)
{
	GPOS_ASSERT(NULL != pexpr);
	ULONG ulOpId =  (ULONG) pexpr->Pop()->Eopid();
	if (COperator::EopPhysicalTableScan == ulOpId || COperator::EopPhysicalExternalScan == ulOpId)
	{
		CDXLNode *dxlnode = PdxlnTblScan(pexpr, NULL /*pcrsOutput*/, colref_array, pdrgpdsBaseTables, NULL /* pexprScalarCond */, NULL /* cost info */);
		CTranslatorExprToDXLUtils::SetStats(m_mp, m_pmda, dxlnode, pexpr->Pstats(), fRoot);
		
		return dxlnode;
	}
	PfPdxlnPhysical pf = m_rgpfPhysicalTranslators[ulOpId];
	if (NULL == pf)
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp, pexpr->Pop()->SzId());
	}

	// add a result node on top to project out columns not needed any further,
	// for instance, if the grouping /order by /partition/ distribution columns
	// are no longer needed
	CDXLNode *pdxlnNew = NULL;

	CDXLNode *dxlnode = (this->* pf)(pexpr, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);

	if (!fRemap || EdxlopPhysicalDML == dxlnode->GetOperator()->GetDXLOperator())
	{
		pdxlnNew = dxlnode;
	}
	else
	{
		CColRefArray *pdrgpcrRequired = NULL;
		
		if (EdxlopPhysicalCTAS == dxlnode->GetOperator()->GetDXLOperator())
		{
			colref_array->AddRef();
			pdrgpcrRequired = colref_array;
		}
		else
		{
			pdrgpcrRequired = pexpr->Prpp()->PcrsRequired()->Pdrgpcr(m_mp);
		}
		pdxlnNew = PdxlnRemapOutputColumns(pexpr, dxlnode, pdrgpcrRequired, colref_array);
		pdrgpcrRequired->Release();
	}

	if (NULL == pdxlnNew->GetProperties()->GetDxlStatsDrvdRelation())
	{
		CTranslatorExprToDXLUtils::SetStats(m_mp, m_pmda, pdxlnNew, pexpr->Pstats(), fRoot);
	}
	
	return pdxlnNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScalar
//
//	@doc:
//		Translates an optimizer scalar expression tree into DXL. Any column
//		refs that are members of the input colrefset are replaced by the input
//		subplan node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScalar
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	ULONG ulOpId =  (ULONG) pexpr->Pop()->Eopid();
	PfPdxlnScalar pf = m_rgpfScalarTranslators[ulOpId];

	if (NULL == pf)
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp, pexpr->Pop()->SzId());
	}
	
	return (this->* pf)(pexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnTblScan
//
//	@doc:
//		Create a DXL table scan node from an optimizer table scan node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnTblScan
	(
	CExpression *pexprTblScan,
	CColRefSet *pcrsOutput,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	CExpression *pexprScalar,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprTblScan);

	CPhysicalTableScan *popTblScan = CPhysicalTableScan::PopConvert(pexprTblScan->Pop());
	CColRefArray *pdrgpcrOutput = popTblScan->PdrgpcrOutput();
	
	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(popTblScan->Ptabdesc(), pdrgpcrOutput, pexprTblScan->Prpp());

	// construct plan costs, if there are not passed as a parameter
	if (NULL == dxl_properties)
	{
		dxl_properties = GetProperties(pexprTblScan);
	}

	// construct scan operator
	CDXLPhysicalTableScan *pdxlopTS = NULL;
	COperator::EOperatorId op_id = pexprTblScan->Pop()->Eopid();
	if (COperator::EopPhysicalTableScan == op_id)
	{
		pdxlopTS = GPOS_NEW(m_mp) CDXLPhysicalTableScan(m_mp, table_descr);
	}
	else
	{
		GPOS_ASSERT(COperator::EopPhysicalExternalScan == op_id);
		pdxlopTS = GPOS_NEW(m_mp) CDXLPhysicalExternalScan(m_mp, table_descr);
	}
	
	CDXLNode *pdxlnTblScan = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopTS);
	pdxlnTblScan->SetProperties(dxl_properties);
	
	// construct projection list
	GPOS_ASSERT(NULL != pexprTblScan->Prpp());

	// if the output columns are passed from above then use them
	if (NULL == pcrsOutput)
	{
	  pcrsOutput = pexprTblScan->Prpp()->PcrsRequired();
	}
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	CDXLNode *pdxlnCond = NULL;
	if (NULL != pexprScalar)
	{
	  pdxlnCond = PdxlnScalar(pexprScalar);
	}

	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);
	
	// add children in the right order
	pdxlnTblScan->AddChild(pdxlnPrL); 		// project list
	pdxlnTblScan->AddChild(filter_dxlnode);	// filter
	
#ifdef GPOS_DEBUG
	pdxlnTblScan->GetOperator()->AssertValid(pdxlnTblScan, false /* validate_children */);
#endif
	
	CDistributionSpec *pds = pexprTblScan->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	return pdxlnTblScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnIndexScan
//
//	@doc:
//		Create a DXL index scan node from an optimizer index scan node based
//		on passed properties
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnIndexScan
	(
	CExpression *pexprIndexScan,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	GPOS_ASSERT(NULL != pexprIndexScan);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprIndexScan);

	COptCtxt::PoctxtFromTLS()->AddDirectDispatchableFilterCandidate(pexprIndexScan);

	CDistributionSpec *pds = pexprIndexScan->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	return PdxlnIndexScan(pexprIndexScan, colref_array, dxl_properties, pexprIndexScan->Prpp());
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnIndexScan
//
//	@doc:
//		Create a DXL index scan node from an optimizer index scan node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnIndexScan
	(
	CExpression *pexprIndexScan,
	CColRefArray *colref_array,
	CDXLPhysicalProperties *dxl_properties,
	CReqdPropPlan *prpp
	)
{
	GPOS_ASSERT(NULL != pexprIndexScan);
	GPOS_ASSERT(NULL != dxl_properties);
	GPOS_ASSERT(NULL != prpp);

	CPhysicalIndexScan *popIs = CPhysicalIndexScan::PopConvert(pexprIndexScan->Pop());

	CColRefArray *pdrgpcrOutput = popIs->PdrgpcrOutput();

	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(popIs->Ptabdesc(), pdrgpcrOutput, pexprIndexScan->Prpp());

	// create index descriptor
	CIndexDescriptor *pindexdesc = popIs->Pindexdesc();
	CMDName *pmdnameIndex = GPOS_NEW(m_mp) CMDName(m_mp, pindexdesc->Name().Pstr());
	IMDId *pmdidIndex = pindexdesc->MDId();
	pmdidIndex->AddRef();
	CDXLIndexDescr *dxl_index_descr = GPOS_NEW(m_mp) CDXLIndexDescr(m_mp, pmdidIndex, pmdnameIndex);

	// TODO: vrgahavan; we assume that the index are always forward access.
	// create the physical index scan operator
	CDXLPhysicalIndexScan *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalIndexScan(m_mp, table_descr, dxl_index_descr, EdxlisdForward);
	CDXLNode *pdxlnIndexScan = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// set properties
	pdxlnIndexScan->SetProperties(dxl_properties);

	// translate project list
	CColRefSet *pcrsOutput = prpp->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	// translate index predicates
	CExpression *pexprCond = (*pexprIndexScan)[0];
	CDXLNode *pdxlnIndexCondList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIndexCondList(m_mp));

	CExpressionArray *pdrgpexprConds = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprCond);
	const ULONG length = pdrgpexprConds->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexprIndexCond = (*pdrgpexprConds)[ul];
		CDXLNode *pdxlnIndexCond = PdxlnScalar(pexprIndexCond);
		pdxlnIndexCondList->AddChild(pdxlnIndexCond);
	}
	pdrgpexprConds->Release();

	CDXLNode *pdxlnResidualCond = NULL;
	if (2 == pexprIndexScan->Arity())
	{
		// translate residual predicates into the filter node
		CExpression *pexprResidualCond = (*pexprIndexScan)[1];
		if (COperator::EopScalarConst != pexprResidualCond->Pop()->Eopid())
		{
			pdxlnResidualCond = PdxlnScalar(pexprResidualCond);
		}
	}

	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnResidualCond);

	pdxlnIndexScan->AddChild(pdxlnPrL);
	pdxlnIndexScan->AddChild(filter_dxlnode);
	pdxlnIndexScan->AddChild(pdxlnIndexCondList);

#ifdef GPOS_DEBUG
	pdxlnIndexScan->GetOperator()->AssertValid(pdxlnIndexScan, false /* validate_children */);
#endif


	return pdxlnIndexScan;
}

CDXLNode *
CTranslatorExprToDXL::PdxlnIndexOnlyScan
	(
	CExpression *pexprIndexOnlyScan,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	GPOS_ASSERT(NULL != pexprIndexOnlyScan);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprIndexOnlyScan);

	COptCtxt::PoctxtFromTLS()->AddDirectDispatchableFilterCandidate(pexprIndexOnlyScan);

	CDistributionSpec *pds = pexprIndexOnlyScan->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	return PdxlnIndexOnlyScan(pexprIndexOnlyScan, colref_array, dxl_properties, pexprIndexOnlyScan->Prpp());
}

CDXLNode *
CTranslatorExprToDXL::PdxlnIndexOnlyScan
	(
	CExpression *pexprIndexOnlyScan,
	CColRefArray *colref_array,
	CDXLPhysicalProperties *dxl_properties,
	CReqdPropPlan *prpp
	)
{
	GPOS_ASSERT(NULL != pexprIndexOnlyScan);
	GPOS_ASSERT(NULL != dxl_properties);
	GPOS_ASSERT(NULL != prpp);

	CPhysicalIndexOnlyScan *popIs = CPhysicalIndexOnlyScan::PopConvert(pexprIndexOnlyScan->Pop());

	CColRefArray *pdrgpcrOutput = popIs->PdrgpcrOutput();

	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(popIs->Ptabdesc(), pdrgpcrOutput, pexprIndexOnlyScan->Prpp());

	// create index descriptor
	CIndexDescriptor *pindexdesc = popIs->Pindexdesc();
	CMDName *pmdnameIndex = GPOS_NEW(m_mp) CMDName(m_mp, pindexdesc->Name().Pstr());
	IMDId *pmdidIndex = pindexdesc->MDId();
	pmdidIndex->AddRef();
	CDXLIndexDescr *dxl_index_descr = GPOS_NEW(m_mp) CDXLIndexDescr(m_mp, pmdidIndex, pmdnameIndex);

	// TODO: vrgahavan; we assume that the index are always forward access.
	// create the physical index scan operator
	CDXLPhysicalIndexOnlyScan *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalIndexOnlyScan(m_mp, table_descr, dxl_index_descr, EdxlisdForward);
	CDXLNode *pdxlnIndexOnlyScan = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// set properties
	pdxlnIndexOnlyScan->SetProperties(dxl_properties);

	// translate project list
	CColRefSet *pcrsOutput = prpp->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	// translate index predicates
	CExpression *pexprCond = (*pexprIndexOnlyScan)[0];
	CDXLNode *pdxlnIndexCondList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIndexCondList(m_mp));

	CExpressionArray *pdrgpexprConds = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprCond);
	const ULONG length = pdrgpexprConds->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexprIndexCond = (*pdrgpexprConds)[ul];
		CDXLNode *pdxlnIndexCond = PdxlnScalar(pexprIndexCond);
		pdxlnIndexCondList->AddChild(pdxlnIndexCond);
	}
	pdrgpexprConds->Release();

	CDXLNode *pdxlnResidualCond = NULL;
	if (2 == pexprIndexOnlyScan->Arity())
	{
		// translate residual predicates into the filter node
		CExpression *pexprResidualCond = (*pexprIndexOnlyScan)[1];
		if (COperator::EopScalarConst != pexprResidualCond->Pop()->Eopid())
		{
			pdxlnResidualCond = PdxlnScalar(pexprResidualCond);
		}
	}

	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnResidualCond);

	pdxlnIndexOnlyScan->AddChild(pdxlnPrL);
	pdxlnIndexOnlyScan->AddChild(filter_dxlnode);
	pdxlnIndexOnlyScan->AddChild(pdxlnIndexCondList);

#ifdef GPOS_DEBUG
	pdxlnIndexOnlyScan->GetOperator()->AssertValid(pdxlnIndexOnlyScan, false /* validate_children */);
#endif


	return pdxlnIndexOnlyScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnBitmapIndexProbe
//
//	@doc:
//		Create a DXL scalar bitmap index probe from an optimizer
//		scalar bitmap index probe operator.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnBitmapIndexProbe
	(
	CExpression *pexprBitmapIndexProbe
	)
{
	GPOS_ASSERT(NULL != pexprBitmapIndexProbe);
	CScalarBitmapIndexProbe *pop = CScalarBitmapIndexProbe::PopConvert(pexprBitmapIndexProbe->Pop());

	// create index descriptor
	CIndexDescriptor *pindexdesc = pop->Pindexdesc();
	CMDName *pmdnameIndex = GPOS_NEW(m_mp) CMDName(m_mp, pindexdesc->Name().Pstr());
	IMDId *pmdidIndex = pindexdesc->MDId();
	pmdidIndex->AddRef();

	CDXLIndexDescr *dxl_index_descr = GPOS_NEW(m_mp) CDXLIndexDescr(m_mp, pmdidIndex, pmdnameIndex);
	CDXLScalarBitmapIndexProbe *dxl_op = GPOS_NEW(m_mp) CDXLScalarBitmapIndexProbe(m_mp, dxl_index_descr);
	CDXLNode *pdxlnBitmapIndexProbe = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// translate index predicates
	CExpression *pexprCond = (*pexprBitmapIndexProbe)[0];
	CDXLNode *pdxlnIndexCondList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIndexCondList(m_mp));
	CExpressionArray *pdrgpexprConds = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprCond);
	const ULONG length = pdrgpexprConds->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexprIndexCond = (*pdrgpexprConds)[ul];
		CDXLNode *pdxlnIndexCond = PdxlnScalar(pexprIndexCond);
		pdxlnIndexCondList->AddChild(pdxlnIndexCond);
	}
	pdrgpexprConds->Release();
	pdxlnBitmapIndexProbe->AddChild(pdxlnIndexCondList);

#ifdef GPOS_DEBUG
	pdxlnBitmapIndexProbe->GetOperator()->AssertValid(pdxlnBitmapIndexProbe, false /*validate_children*/);
#endif

	return pdxlnBitmapIndexProbe;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnBitmapBoolOp
//
//	@doc:
//		Create a DXL scalar bitmap boolean operator from an optimizer
//		scalar bitmap boolean operator operator.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnBitmapBoolOp
	(
	CExpression *pexprBitmapBoolOp
	)
{
	GPOS_ASSERT(NULL != pexprBitmapBoolOp);
	GPOS_ASSERT(2 == pexprBitmapBoolOp->Arity());
	
	CScalarBitmapBoolOp *popBitmapBoolOp = CScalarBitmapBoolOp::PopConvert(pexprBitmapBoolOp->Pop());
	CExpression *pexprLeft = (*pexprBitmapBoolOp)[0];
	CExpression *pexprRight = (*pexprBitmapBoolOp)[1];
	
	CDXLNode *dxlnode_left = PdxlnScalar(pexprLeft);
	CDXLNode *dxlnode_right = PdxlnScalar(pexprRight);
	
	IMDId *mdid_type = popBitmapBoolOp->MdidType();
	mdid_type->AddRef();
	
	CDXLScalarBitmapBoolOp::EdxlBitmapBoolOp edxlbitmapop = CDXLScalarBitmapBoolOp::EdxlbitmapAnd;
	
	if (CScalarBitmapBoolOp::EbitmapboolOr == popBitmapBoolOp->Ebitmapboolop())
	{
		edxlbitmapop = CDXLScalarBitmapBoolOp::EdxlbitmapOr;
	}
	
	return GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarBitmapBoolOp(m_mp, mdid_type, edxlbitmapop),
						dxlnode_left,
						dxlnode_right
						);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnBitmapTableScan
//
//	@doc:
//		Create a DXL physical bitmap table scan from an optimizer
//		physical bitmap table scan operator.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnBitmapTableScan
	(
	CExpression *pexprBitmapTableScan,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	return PdxlnBitmapTableScan
			(
			pexprBitmapTableScan,
			NULL,  // pcrsOutput
			colref_array,
			pdrgpdsBaseTables, 
			NULL, // pexprScalar
			NULL // dxl_properties
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::AddBitmapFilterColumns
//
//	@doc:
//		Add used columns in the bitmap recheck and the remaining scalar filter
//		condition to the required output column
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::AddBitmapFilterColumns
	(
	CMemoryPool *mp,
	CPhysicalScan *pop,
	CExpression *pexprRecheckCond,
	CExpression *pexprScalar,
	CColRefSet *pcrsReqdOutput // append the required column reference
	)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(COperator::EopPhysicalDynamicBitmapTableScan == pop->Eopid() ||
				COperator::EopPhysicalBitmapTableScan == pop->Eopid());
	GPOS_ASSERT(NULL != pcrsReqdOutput);

	// compute what additional columns are required in the output of the (Dynamic) Bitmap Table Scan
	CColRefSet *pcrsAdditional =  GPOS_NEW(mp) CColRefSet(mp);

	if (NULL != pexprRecheckCond)
	{
		// add the columns used in the recheck condition
		pcrsAdditional->Include(pexprRecheckCond->DeriveUsedColumns());
	}

	if (NULL != pexprScalar)
	{
		// add the columns used in the filter condition
		pcrsAdditional->Include(pexprScalar->DeriveUsedColumns());
	}

	CColRefSet *pcrsBitmap =  GPOS_NEW(mp) CColRefSet(mp);
	pcrsBitmap->Include(pop->PdrgpcrOutput());

	// only keep the columns that are in the table associated with the bitmap
	pcrsAdditional->Intersection(pcrsBitmap);

	if (0 < pcrsAdditional->Size())
	{
		pcrsReqdOutput->Include(pcrsAdditional);
	}

	// clean up
	pcrsAdditional->Release();
	pcrsBitmap->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnBitmapTableScan
//
//	@doc:
//		Create a DXL physical bitmap table scan from an optimizer
//		physical bitmap table scan operator.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnBitmapTableScan
	(
	CExpression *pexprBitmapTableScan,
	CColRefSet *pcrsOutput,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	CExpression *pexprScalar,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprBitmapTableScan);
	CPhysicalBitmapTableScan *pop = CPhysicalBitmapTableScan::PopConvert(pexprBitmapTableScan->Pop());

	COptCtxt::PoctxtFromTLS()->AddDirectDispatchableFilterCandidate(pexprBitmapTableScan);

	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(pop->Ptabdesc(), pop->PdrgpcrOutput(), pexprBitmapTableScan->Prpp());

	CDXLPhysicalBitmapTableScan *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalBitmapTableScan(m_mp, table_descr);
	CDXLNode *pdxlnBitmapTableScan = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// set properties
	// construct plan costs, if there are not passed as a parameter
	if (NULL == dxl_properties)
	{
		dxl_properties = GetProperties(pexprBitmapTableScan);
	}
	pdxlnBitmapTableScan->SetProperties(dxl_properties);

	// build projection list
	if (NULL == pcrsOutput)
	{
		pcrsOutput = pexprBitmapTableScan->Prpp()->PcrsRequired();
	}

	// translate scalar predicate into DXL filter only if it is not redundant
	CExpression *pexprRecheckCond = (*pexprBitmapTableScan)[0];
	CDXLNode *pdxlnCond = NULL;
	if (NULL != pexprScalar &&
		!CUtils::FScalarConstTrue(pexprScalar) &&
		!pexprScalar->Matches(pexprRecheckCond))
	{
		pdxlnCond = PdxlnScalar(pexprScalar);
	}

	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);

	CDXLNode *pdxlnRecheckCond = PdxlnScalar(pexprRecheckCond);
	CDXLNode *pdxlnRecheckCondFilter =
			GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarRecheckCondFilter(m_mp), pdxlnRecheckCond
						);

	AddBitmapFilterColumns(m_mp, pop, pexprRecheckCond, pexprScalar, pcrsOutput);

	CDXLNode *proj_list_dxlnode = PdxlnProjList(pcrsOutput, colref_array);

	// translate bitmap access path
	CDXLNode *pdxlnBitmapIndexPath = PdxlnScalar((*pexprBitmapTableScan)[1]);

	pdxlnBitmapTableScan->AddChild(proj_list_dxlnode);
	pdxlnBitmapTableScan->AddChild(filter_dxlnode);
	pdxlnBitmapTableScan->AddChild(pdxlnRecheckCondFilter);
	pdxlnBitmapTableScan->AddChild(pdxlnBitmapIndexPath);
#ifdef GPOS_DEBUG
	pdxlnBitmapTableScan->GetOperator()->AssertValid(pdxlnBitmapTableScan, false /*validate_children*/);
#endif
	
	CDistributionSpec *pds = pexprBitmapTableScan->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);

	return pdxlnBitmapTableScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicTableScan
//
//	@doc:
//		Create a DXL dynamic table scan node from an optimizer 
//		dynamic table scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicTableScan
	(
	CExpression *pexprDTS,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	CExpression *pexprScalarCond = NULL;
	CDXLPhysicalProperties *dxl_properties = NULL;
	return PdxlnDynamicTableScan(pexprDTS, colref_array, pdrgpdsBaseTables, pexprScalarCond, dxl_properties);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicTableScan
//
//	@doc:
//		Create a DXL dynamic table scan node from an optimizer 
//		dynamic table scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicTableScan
	(
	CExpression *pexprDTS,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprDTS);
	GPOS_ASSERT_IFF(NULL != pexprScalarCond, NULL != dxl_properties);
	
	CPhysicalDynamicTableScan *popDTS = CPhysicalDynamicTableScan::PopConvert(pexprDTS->Pop());	
	CColRefArray *pdrgpcrOutput = popDTS->PdrgpcrOutput();

	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(popDTS->Ptabdesc(), pdrgpcrOutput, pexprDTS->Prpp());

	// construct plan costs
	CDXLPhysicalProperties *pdxlpropDTS = GetProperties(pexprDTS);
	
	if (NULL != dxl_properties)
	{
		CWStringDynamic *rows_out_str = GPOS_NEW(m_mp) CWStringDynamic(m_mp, dxl_properties->GetDXLOperatorCost()->GetRowsOutStr()->GetBuffer());
		CWStringDynamic *pstrCost = GPOS_NEW(m_mp) CWStringDynamic(m_mp, dxl_properties->GetDXLOperatorCost()->GetTotalCostStr()->GetBuffer());

		pdxlpropDTS->GetDXLOperatorCost()->SetRows(rows_out_str);
		pdxlpropDTS->GetDXLOperatorCost()->SetCost(pstrCost);
		dxl_properties->Release();
	}

	// construct dynamic table scan operator
	CDXLPhysicalDynamicTableScan *pdxlopDTS = 
			GPOS_NEW(m_mp) CDXLPhysicalDynamicTableScan
						(
						m_mp,
						table_descr, 
						popDTS->UlSecondaryScanId(),
						popDTS->ScanId()
						);

	CDXLNode *pdxlnDTS = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopDTS);
	pdxlnDTS->SetProperties(pdxlpropDTS);
	
	CDXLNode *pdxlnCond = NULL;
	
	if (NULL != pexprScalarCond)
	{
		pdxlnCond = PdxlnScalar(pexprScalarCond);
	}
	
	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);
	
	// construct projection list
	GPOS_ASSERT(NULL != pexprDTS->Prpp());
	
	CColRefSet *pcrsOutput = pexprDTS->Prpp()->PcrsRequired();
	pdxlnDTS->AddChild(PdxlnProjList(pcrsOutput, colref_array));
	pdxlnDTS->AddChild(filter_dxlnode);
	
#ifdef GPOS_DEBUG
	pdxlnDTS->GetOperator()->AssertValid(pdxlnDTS, false /* validate_children */);
#endif
	
	CDistributionSpec *pds = pexprDTS->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	
	return pdxlnDTS;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicBitmapTableScan
//
//	@doc:
//		Create a DXL dynamic bitmap table scan node from an optimizer
//		dynamic bitmap table scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicBitmapTableScan
	(
	CExpression *pexprScan,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	CExpression *pexprScalar = NULL;
	CDXLPhysicalProperties *dxl_properties = NULL;
	return PdxlnDynamicBitmapTableScan
			(
			pexprScan,
			colref_array,
			pdrgpdsBaseTables,
			pexprScalar,
			dxl_properties
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicBitmapTableScan
//
//	@doc:
//		Create a DXL dynamic bitmap table scan node from an optimizer
//		dynamic bitmap table scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicBitmapTableScan
	(
	CExpression *pexprScan,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	CExpression *pexprScalar,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprScan);

	CPhysicalDynamicBitmapTableScan *pop = CPhysicalDynamicBitmapTableScan::PopConvert(pexprScan->Pop());
	CColRefArray *pdrgpcrOutput = pop->PdrgpcrOutput();

	CDXLTableDescr *table_descr = MakeDXLTableDescr(pop->Ptabdesc(), pdrgpcrOutput, pexprScan->Prpp());
	CDXLPhysicalDynamicBitmapTableScan *pdxlopScan =
			GPOS_NEW(m_mp) CDXLPhysicalDynamicBitmapTableScan
						(
						m_mp,
						table_descr,
						pop->UlSecondaryScanId(),
						pop->ScanId()
						);

	CDXLNode *pdxlnScan = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopScan);

	// construct plan costs
	if (NULL == dxl_properties)
	{
		dxl_properties = GetProperties(pexprScan);
	}
	pdxlnScan->SetProperties(dxl_properties);

	// translate predicates into DXL filter
	CDXLNode *pdxlnCond = NULL;
	if (NULL != pexprScalar)
	{
		pdxlnCond = PdxlnScalar(pexprScalar);
	}
	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);

	CExpression *pexprRecheckCond = (*pexprScan)[0];
	CDXLNode *pdxlnRecheckCond = PdxlnScalar(pexprRecheckCond);
	CDXLNode *pdxlnRecheckCondFilter =
			GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarRecheckCondFilter(m_mp));
	pdxlnRecheckCondFilter->AddChild(pdxlnRecheckCond);

	// translate bitmap access path
	CDXLNode *pdxlnBitmapAccessPath = PdxlnScalar((*pexprScan)[1]);

	// build projection list
	CColRefSet *pcrsOutput = pexprScan->Prpp()->PcrsRequired();
	AddBitmapFilterColumns(m_mp, pop, pexprRecheckCond, pexprScalar, pcrsOutput);
	CDXLNode *proj_list_dxlnode = PdxlnProjList(pcrsOutput, colref_array);

	pdxlnScan->AddChild(proj_list_dxlnode);
	pdxlnScan->AddChild(filter_dxlnode);
	pdxlnScan->AddChild(pdxlnRecheckCondFilter);
	pdxlnScan->AddChild(pdxlnBitmapAccessPath);

#ifdef GPOS_DEBUG
	pdxlnScan->GetOperator()->AssertValid(pdxlnScan, false /* validate_children */);
#endif

	CDistributionSpec *pds = pexprScan->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	
	return pdxlnScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicIndexScan
//
//	@doc:
//		Create a DXL dynamic index scan node from an optimizer 
//		dynamic index scan node based on passed properties
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicIndexScan
	(
	CExpression *pexprDIS,
	CColRefArray *colref_array,
	CDXLPhysicalProperties *dxl_properties,
	CReqdPropPlan *prpp
	)
{
	GPOS_ASSERT(NULL != pexprDIS);
	GPOS_ASSERT(NULL != dxl_properties);
	GPOS_ASSERT(NULL != prpp);

	CPhysicalDynamicIndexScan *popDIS = CPhysicalDynamicIndexScan::PopConvert(pexprDIS->Pop());	
	CColRefArray *pdrgpcrOutput = popDIS->PdrgpcrOutput();
	
	// translate table descriptor
	CDXLTableDescr *table_descr = MakeDXLTableDescr(popDIS->Ptabdesc(), pdrgpcrOutput, pexprDIS->Prpp());

	// create index descriptor
	CIndexDescriptor *pindexdesc = popDIS->Pindexdesc();
	CMDName *pmdnameIndex = GPOS_NEW(m_mp) CMDName(m_mp, pindexdesc->Name().Pstr());
	IMDId *pmdidIndex = pindexdesc->MDId();
	pmdidIndex->AddRef();
	CDXLIndexDescr *dxl_index_descr = GPOS_NEW(m_mp) CDXLIndexDescr(m_mp, pmdidIndex, pmdnameIndex);

	// TODO: vrgahavan; we assume that the index are always forward access.
	// create the physical index scan operator
	CDXLNode *pdxlnDIS = GPOS_NEW(m_mp) CDXLNode
									(
									m_mp,
									GPOS_NEW(m_mp) CDXLPhysicalDynamicIndexScan
													(
													m_mp,
													table_descr,
													popDIS->UlSecondaryScanId(),
													popDIS->ScanId(),
													dxl_index_descr,
													EdxlisdForward
													)
									);
	
	// set plan costs
	pdxlnDIS->SetProperties(dxl_properties);
	
	// construct projection list
	CColRefSet *pcrsOutput = prpp->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);
	
	// translate index predicates
	CExpression *pexprCond = (*pexprDIS)[0];
	CDXLNode *pdxlnIndexCondList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIndexCondList(m_mp));

	CExpressionArray *pdrgpexprConds = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprCond);
	const ULONG length = pdrgpexprConds->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexprIndexCond = (*pdrgpexprConds)[ul];
		CDXLNode *pdxlnIndexCond = PdxlnScalar(pexprIndexCond);
		pdxlnIndexCondList->AddChild(pdxlnIndexCond);
	}
	pdrgpexprConds->Release();

	CDXLNode *pdxlnResidualCond = NULL;
	if (2 == pexprDIS->Arity())
	{
		// translate residual predicates into the filter node
		CExpression *pexprResidualCond = (*pexprDIS)[1];
		if (COperator::EopScalarConst != pexprResidualCond->Pop()->Eopid())
		{
			pdxlnResidualCond = PdxlnScalar(pexprResidualCond);
		}
	}

	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnResidualCond);

	pdxlnDIS->AddChild(pdxlnPrL);
	pdxlnDIS->AddChild(filter_dxlnode);
	pdxlnDIS->AddChild(pdxlnIndexCondList);
	
#ifdef GPOS_DEBUG
	pdxlnDIS->GetOperator()->AssertValid(pdxlnDIS, false /* validate_children */);
#endif
		
	return pdxlnDIS;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDynamicIndexScan
//
//	@doc:
//		Create a DXL dynamic index scan node from an optimizer
//		dynamic index scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDynamicIndexScan
	(
	CExpression *pexprDIS,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	GPOS_ASSERT(NULL != pexprDIS);

	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprDIS);

	CDistributionSpec *pds = pexprDIS->GetDrvdPropPlan()->Pds();
	pds->AddRef();
	pdrgpdsBaseTables->Append(pds);
	return PdxlnDynamicIndexScan(pexprDIS, colref_array, dxl_properties, pexprDIS->Prpp());
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResult
//
//	@doc:
//		Create a DXL result node over a relational expression with a DXL
//		scalar condition.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResult
	(
	CExpression *pexprRelational,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CDXLNode *pdxlnScalar
	)
{
	// extract physical properties from filter
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprRelational);

	return PdxlnResult(pexprRelational, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, pdxlnScalar, dxl_properties);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResult
//
//	@doc:
//		Create a DXL result node over a relational expression with a DXL
//		scalar condition using the passed DXL properties
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResult
	(
	CExpression *pexprRelational,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CDXLNode *pdxlnScalar,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprRelational);

	// translate relational child expression
	CDXLNode *pdxlnRelationalChild = CreateDXLNode(pexprRelational, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot */);
	GPOS_ASSERT(NULL != pexprRelational->Prpp());
	CColRefSet *pcrsOutput = pexprRelational->Prpp()->PcrsRequired();

	return PdxlnAddScalarFilterOnRelationalChild
			(
			pdxlnRelationalChild,
			pdxlnScalar,
			dxl_properties,
			pcrsOutput,
			colref_array
			);
}

CDXLNode *
CTranslatorExprToDXL::PdxlnAddScalarFilterOnRelationalChild
	(
	CDXLNode *pdxlnRelationalChild,
	CDXLNode *pdxlnScalarChild,
	CDXLPhysicalProperties *dxl_properties,
	CColRefSet *pcrsOutput,
	CColRefArray *pdrgpcrOrder
	)
{
	GPOS_ASSERT(NULL != dxl_properties);
	// for a true condition, just translate the child
	if (CTranslatorExprToDXLUtils::FScalarConstTrue(m_pmda, pdxlnScalarChild))
	{
		pdxlnScalarChild->Release();
		dxl_properties->Release();
		return pdxlnRelationalChild;
	}
	// create a result node over outer child
	else
	{
		// wrap condition in a DXL filter node
		CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnScalarChild);

		CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrOrder);

		// create an empty one-time filter
		CDXLNode *one_time_filter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp));

		return CTranslatorExprToDXLUtils::PdxlnResult
				(
				m_mp,
				dxl_properties,
				pdxlnPrL,
				filter_dxlnode,
				one_time_filter,
				pdxlnRelationalChild
				);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResult
//
//	@doc:
//		Create a DXL result node from an optimizer filter node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResult
	(
	CExpression *pexprFilter,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprFilter);

	CDXLNode *pdxlnode = PdxlnFromFilter(pexprFilter, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, dxl_properties);
	dxl_properties->Release();

	return pdxlnode;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnIndexScanWithInlinedCondition
//
//	@doc:
//		Create a (dynamic) index scan node after inlining the given
//		scalar condition, if needed
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnIndexScanWithInlinedCondition
	(
	CExpression *pexprIndexScan,
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables
	)
{
	GPOS_ASSERT(NULL != pexprIndexScan);
	GPOS_ASSERT(NULL != pexprScalarCond);
	GPOS_ASSERT(pexprScalarCond->Pop()->FScalar());

	COperator::EOperatorId op_id = pexprIndexScan->Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalIndexScan == op_id ||
			COperator::EopPhysicalDynamicIndexScan == op_id);

	// TODO: Index only scans work on GiST and SP-GiST only for specific operators
	// check if index is of type GiST
	BOOL isGist = false;
	if (COperator::EopPhysicalIndexScan == op_id)
	{
		CPhysicalIndexScan *indexScan = CPhysicalIndexScan::PopConvert(pexprIndexScan->Pop());
		isGist = (indexScan->Pindexdesc()->IndexType() == IMDIndex::EmdindGist);
	}
	else if (COperator::EopPhysicalIndexOnlyScan != op_id)
	{
		CPhysicalDynamicIndexScan *indexScan = CPhysicalDynamicIndexScan::PopConvert(pexprIndexScan->Pop());
		isGist = (indexScan->Pindexdesc()->IndexType() == IMDIndex::EmdindGist);
	}

	// inline scalar condition in index scan, if it is not the same as index lookup condition
	// Exception: most GiST indexes require a recheck condition since they are lossy: re-add the lookup
	// condition as a scalar condition. For now, all GiST indexes are treated as lossy
	CExpression *pexprIndexLookupCond = (*pexprIndexScan)[0];
	CDXLNode *pdxlnIndexScan = NULL;
	if ((!CUtils::FScalarConstTrue(pexprScalarCond) && !pexprScalarCond->Matches(pexprIndexLookupCond))
	|| isGist)
	{
		// combine scalar condition with existing index conditions, if any
		pexprScalarCond->AddRef();
		CExpression *pexprNewScalarCond = pexprScalarCond;
		if (2 == pexprIndexScan->Arity())
		{
			pexprNewScalarCond->Release();
			pexprNewScalarCond = CPredicateUtils::PexprConjunction(m_mp, (*pexprIndexScan)[1], pexprScalarCond);
		}
		pexprIndexLookupCond->AddRef();
		pexprIndexScan->Pop()->AddRef();
		CExpression *pexprNewIndexScan = GPOS_NEW(m_mp) CExpression(m_mp, pexprIndexScan->Pop(), pexprIndexLookupCond, pexprNewScalarCond);
		if (COperator::EopPhysicalIndexScan == op_id)
		{
			pdxlnIndexScan = PdxlnIndexScan(pexprNewIndexScan, colref_array, dxl_properties, pexprIndexScan->Prpp());
		}
		else if (COperator::EopPhysicalIndexOnlyScan == op_id)
		{
			pdxlnIndexScan = PdxlnIndexOnlyScan(pexprNewIndexScan, colref_array, dxl_properties, pexprIndexScan->Prpp());
		}
		else
		{
			pdxlnIndexScan = PdxlnDynamicIndexScan(pexprNewIndexScan, colref_array, dxl_properties, pexprIndexScan->Prpp());
		}
		pexprNewIndexScan->Release();

		CDistributionSpec *pds = pexprIndexScan->GetDrvdPropPlan()->Pds();
		pds->AddRef();
		pdrgpdsBaseTables->Append(pds);
		
		return pdxlnIndexScan;
	}

	// index scan does not need the properties of the filter, as it does not
	// need to further inline the scalar condition
	dxl_properties->Release();
	ULONG ulNonGatherMotions = 0;
	BOOL fDML = false;
	if (COperator::EopPhysicalIndexScan == op_id)
	{
		return PdxlnIndexScan(pexprIndexScan, colref_array, pdrgpdsBaseTables, &ulNonGatherMotions, &fDML);
	}
	if (COperator::EopPhysicalIndexOnlyScan == op_id)
	{
		return PdxlnIndexOnlyScan(pexprIndexScan, colref_array, pdrgpdsBaseTables, &ulNonGatherMotions, &fDML);
	}

	return PdxlnDynamicIndexScan(pexprIndexScan, colref_array, pdrgpdsBaseTables, &ulNonGatherMotions, &fDML);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResult
//
//	@doc:
//		Create a DXL result node from an optimizer filter node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnFromFilter
	(
	CExpression *pexprFilter,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexprFilter);
	GPOS_ASSERT(NULL != dxl_properties);

	// extract components
	CExpression *pexprRelational = (*pexprFilter)[0];
	CExpression *pexprScalar = (*pexprFilter)[1];

	if (CTranslatorExprToDXLUtils::FDirectDispatchableFilter(pexprFilter))

	{
		COptCtxt::PoctxtFromTLS()->AddDirectDispatchableFilterCandidate(pexprFilter);
	}

	// if the filter predicate is a constant TRUE, skip to translating relational child
	if (CUtils::FScalarConstTrue(pexprScalar))
	{
		return CreateDXLNode(pexprRelational, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /* fRoot */);
	}

	COperator::EOperatorId eopidRelational = pexprRelational->Pop()->Eopid();
	CColRefSet *pcrsOutput = pexprFilter->Prpp()->PcrsRequired();

	switch (eopidRelational)
	{
		case COperator::EopPhysicalTableScan:
		case COperator::EopPhysicalExternalScan:
		{
			// if there is a structure of the form
			// 		filter->tablescan, or filter->CTG then
			// push the scalar filter expression to the tablescan/CTG respectively
			dxl_properties->AddRef();

			// translate the table scan with the filter condition
			return PdxlnTblScan
					(
					pexprRelational,
					pcrsOutput,
					NULL /* colref_array */,
					pdrgpdsBaseTables, 
					pexprScalar,
					dxl_properties /* cost info */
					);
		}
		case COperator::EopPhysicalBitmapTableScan:
		{
			dxl_properties->AddRef();

			return PdxlnBitmapTableScan
					(
					pexprRelational,
					pcrsOutput,
					NULL /*colref_array*/,
					pdrgpdsBaseTables,
					pexprScalar,
					dxl_properties
					);
		}
		case COperator::EopPhysicalDynamicTableScan:
		{
			dxl_properties->AddRef();

			// inline condition in the Dynamic Table Scan
			return PdxlnDynamicTableScan(pexprRelational, colref_array, pdrgpdsBaseTables, pexprScalar, dxl_properties);
		}
		case COperator::EopPhysicalIndexOnlyScan:
		case COperator::EopPhysicalIndexScan:
		case COperator::EopPhysicalDynamicIndexScan:
		{
			dxl_properties->AddRef();
			return PdxlnIndexScanWithInlinedCondition
						(
						pexprRelational, 
						pexprScalar,
						dxl_properties,
						colref_array,
						pdrgpdsBaseTables
						);
		}
		case COperator::EopPhysicalDynamicBitmapTableScan:
		{
			dxl_properties->AddRef();

			return PdxlnDynamicBitmapTableScan(pexprRelational, colref_array, pdrgpdsBaseTables, pexprScalar, dxl_properties);
		}
		default:
		{
			return PdxlnResultFromFilter(pexprFilter, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
		}
	}
	
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelectorWithInlinedCondition
//
//	@doc:
//		Translate a partition selector into DXL while inlining the given
//		condition in the child
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelectorWithInlinedCondition
	(
	CExpression *pexprFilter,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprFilter);
	GPOS_ASSERT(COperator::EopPhysicalFilter == pexprFilter->Pop()->Eopid());
	GPOS_ASSERT(COperator::EopPhysicalPartitionSelector == (*pexprFilter)[0]->Pop()->Eopid());

	CExpression *pexprRelational = (*pexprFilter)[0];
	CExpression *pexprScalar = (*pexprFilter)[1];
	CExpression *pexprChild = (*pexprRelational)[0];
	COperator::EOperatorId op_id = pexprChild->Pop()->Eopid();
	BOOL fTableScanChild = (COperator::EopPhysicalDynamicTableScan == op_id);
	BOOL fIndexChild = (COperator::EopPhysicalDynamicIndexScan == op_id || COperator::EopPhysicalDynamicBitmapTableScan == op_id);
	GPOS_ASSERT(fTableScanChild || fIndexChild);

	// if we are a dynamic GiST index scan, we need to do a recheck condition since GiST indexes are lossy
	BOOL isGist = false;
	if (COperator::EopPhysicalDynamicIndexScan == op_id)
	{
		CPhysicalDynamicIndexScan *indexScan = CPhysicalDynamicIndexScan::PopConvert(pexprChild->Pop());
		isGist = indexScan->Pindexdesc()->IndexType() == IMDIndex::EmdindGist;
	}

	// inline condition in child operator if the following conditions are met:
	BOOL fInlineCondition =
		NULL != pexprScalar &&	// condition is not NULL
		!CUtils::FScalarConstTrue(pexprScalar) &&	// condition is not const True
		(
		fTableScanChild || 	// child operator is TableScan
		(fIndexChild && (!pexprScalar->Matches((*pexprChild)[0]) || isGist))	// OR, child operator is IndexScan and condition does not match index condition
		); // if it is of type GiST, inline the condition anyway as the recheck

	CExpression *pexprCond = NULL;
	CDXLPhysicalProperties *dxl_properties = NULL;
	if (fInlineCondition)
	{
		pexprCond = pexprScalar;
		dxl_properties = GetProperties(pexprFilter);
	}

	return PdxlnPartitionSelector(pexprRelational, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, pexprCond, dxl_properties);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResultFromFilter
//
//	@doc:
//		Create a DXL result node from an optimizer filter node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResultFromFilter
	(
	CExpression *pexprFilter,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprFilter);

	// extract components
	CExpression *pexprRelational = (*pexprFilter)[0];
	CExpression *pexprScalar = (*pexprFilter)[1];
	CColRefSet *pcrsOutput = pexprFilter->Prpp()->PcrsRequired();

	if (COperator::EopPhysicalPartitionSelector == pexprRelational->Pop()->Eopid())
	{
		COperator::EOperatorId op_id = (*pexprRelational)[0]->Pop()->Eopid();
		if (COperator::EopPhysicalDynamicIndexScan == op_id ||
			COperator::EopPhysicalDynamicBitmapTableScan == op_id ||
			COperator::EopPhysicalDynamicTableScan == op_id)
		{
			return PdxlnPartitionSelectorWithInlinedCondition(pexprFilter, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
		}
	}

	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprFilter);

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprRelational, NULL /* colref_array */, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// translate scalar expression in to filter and one time filter dxl nodes
	CColRefSet *relational_child_colrefset = pexprRelational->DeriveOutputColumns();
	// get all the scalar conditions in an array
	CExpressionArray *scalar_exprs = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprScalar);
	// array to hold scalar conditions which will qualify for filter condition
	CExpressionArray *filter_quals_exprs = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	// array to hold scalar conditions which qualify for one time filter condition
	CExpressionArray *one_time_filter_quals_exprs = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	for (ULONG ul=0; ul < scalar_exprs->Size(); ul++)
	{
		CExpression *scalar_child_expr = (*scalar_exprs)[ul];
		CColRefSet *scalar_child_colrefset = scalar_child_expr->DeriveUsedColumns();

		// What qualifies for a one time filter qual?
		// 1. if there is no column in the scalar child of filter expression coming from its relational
		// child
		// and
		// 2. if there is no volatile function in the scalar child
		//
		// Why quals are separated into one time filter vs filter quals?
		// one time filter quals are evaluated once for each scan, and if the filter evaluates to false,
		// the data from the nodes below is not requested, however in case of filter quals, they are
		// evaluated for each tuple coming from the nodes below. So, if the filter does not depends on the tuple
		// values coming from the nodes below, it could be a one time filter and we can save processing time on
		// each tuple and evaluating it against the filter.
		if (scalar_child_colrefset->FIntersects(relational_child_colrefset) || CPredicateUtils::FContainsVolatileFunction(scalar_child_expr))
		{
			scalar_child_expr->AddRef();
			filter_quals_exprs->Append(scalar_child_expr);
		}
		else
		{
			scalar_child_expr->AddRef();
			one_time_filter_quals_exprs->Append(scalar_child_expr);
		}
	}
	scalar_exprs->Release();

	// create an emtpy filter
	CDXLNode *filter_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarFilter(m_mp));
	// create an empty one-time filter
	CDXLNode *one_time_filter_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp));

	if (filter_quals_exprs->Size() > 0)
	{
		// create scalar cmp expression for filter expression
		CExpression *scalar_cmp_expr = CPredicateUtils::PexprConjunction(m_mp, filter_quals_exprs);
		// create dxl node for the filter
		CDXLNode *scalar_cmp_dxlnode = PdxlnScalar(scalar_cmp_expr);
		filter_dxlnode->AddChild(scalar_cmp_dxlnode);
		scalar_cmp_expr->Release();
	}
	else
	{
		filter_quals_exprs->Release();
	}

	if (one_time_filter_quals_exprs->Size() > 0)
	{
		// create scalar cmp expression for one time filter expression
		CExpression *scalar_cmp_expr = CPredicateUtils::PexprConjunction(m_mp, one_time_filter_quals_exprs);
		// create dxl node for one time filter
		CDXLNode *scalar_cmp_dxlnode = PdxlnScalar(scalar_cmp_expr);
		one_time_filter_dxlnode->AddChild(scalar_cmp_dxlnode);
		scalar_cmp_expr->Release();
	}
	else
	{
		one_time_filter_quals_exprs->Release();
	}

	GPOS_ASSERT(NULL != pexprFilter->Prpp());

	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	return CTranslatorExprToDXLUtils::PdxlnResult
											(
											m_mp,
											dxl_properties,
											pdxlnPrL,
											filter_dxlnode,
											one_time_filter_dxlnode,
											child_dxlnode
											);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAssert
//
//	@doc:
//		Translate a physical assert expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAssert
	(
	CExpression *pexprAssert,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprAssert);

	// extract components
	CExpression *pexprRelational = (*pexprAssert)[0];
	CExpression *pexprScalar = (*pexprAssert)[1];
	CPhysicalAssert *popAssert = CPhysicalAssert::PopConvert(pexprAssert->Pop());
	
	// extract physical properties from assert
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprAssert);

	CColRefSet *pcrsOutput = pexprAssert->Prpp()->PcrsRequired();

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprRelational, NULL /* colref_array */, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// translate scalar expression
	CDXLNode *pdxlnAssertPredicate = PdxlnScalar(pexprScalar);

	GPOS_ASSERT(NULL != pexprAssert->Prpp());

	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	const CHAR *sql_state = popAssert->Pexc()->GetSQLState();
	CDXLPhysicalAssert *pdxlopAssert = GPOS_NEW(m_mp) CDXLPhysicalAssert(m_mp, sql_state);
	CDXLNode *pdxlnAssert = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopAssert, pdxlnPrL, pdxlnAssertPredicate, child_dxlnode);
	
	pdxlnAssert->SetProperties(dxl_properties);
	
	return pdxlnAssert;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnCTEProducer
//
//	@doc:
//		Translate a physical cte producer expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnCTEProducer
	(
	CExpression *pexprCTEProducer,
	CColRefArray * , //colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprCTEProducer);

	// extract components
	CExpression *pexprRelational = (*pexprCTEProducer)[0];
	CPhysicalCTEProducer *popCTEProducer = CPhysicalCTEProducer::PopConvert(pexprCTEProducer->Pop());

	// extract physical properties from cte producer
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprCTEProducer);

	// extract the CTE id and the array of colids
	const ULONG ulCTEId = popCTEProducer->UlCTEId();
	ULongPtrArray *colids = CUtils::Pdrgpul(m_mp, popCTEProducer->Pdrgpcr());

	GPOS_ASSERT(NULL != pexprCTEProducer->Prpp());
	CColRefArray *pdrgpcrRequired = popCTEProducer->Pdrgpcr();
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(pdrgpcrRequired);

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprRelational, pdrgpcrRequired, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /*fRoot */);

	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrRequired);
	pcrsOutput->Release();

	CDXLNode *pdxlnCTEProducer = GPOS_NEW(m_mp) CDXLNode
										(
										m_mp,
										GPOS_NEW(m_mp) CDXLPhysicalCTEProducer(m_mp, ulCTEId, colids),
										pdxlnPrL,
										child_dxlnode
										);

	pdxlnCTEProducer->SetProperties(dxl_properties);

	return pdxlnCTEProducer;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnCTEConsumer
//
//	@doc:
//		Translate a physical cte consumer expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnCTEConsumer
	(
	CExpression *pexprCTEConsumer,
	CColRefArray *, //colref_array,
	CDistributionSpecArray *, // pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	GPOS_ASSERT(NULL != pexprCTEConsumer);

	// extract components
	CPhysicalCTEConsumer *popCTEConsumer = CPhysicalCTEConsumer::PopConvert(pexprCTEConsumer->Pop());

	// extract physical properties from cte consumer
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprCTEConsumer);

	// extract the CTE id and the array of colids
	const ULONG ulCTEId = popCTEConsumer->UlCTEId();
	CColRefArray *colref_array = popCTEConsumer->Pdrgpcr();
	ULongPtrArray *colids = CUtils::Pdrgpul(m_mp, colref_array);

	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(colref_array);

	// translate relational child expression
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, colref_array);

	CDXLNode *pdxlnCTEConsumer = GPOS_NEW(m_mp) CDXLNode
										(
										m_mp,
										GPOS_NEW(m_mp) CDXLPhysicalCTEConsumer(m_mp, ulCTEId, colids),
										pdxlnPrL
										);

	pcrsOutput->Release();

	pdxlnCTEConsumer->SetProperties(dxl_properties);

	return pdxlnCTEConsumer;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAppend
//
//	@doc:
//		Create a DXL Append node from an optimizer an union all node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAppend
	(
	CExpression *pexprUnionAll,
	CColRefArray *, //colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprUnionAll);

	CPhysicalUnionAll *popUnionAll = CPhysicalUnionAll::PopConvert(pexprUnionAll->Pop());
	CColRefArray *pdrgpcrOutputAll = popUnionAll->PdrgpcrOutput();
	CColRefSet *reqdCols = pexprUnionAll->Prpp()->PcrsRequired();

	CDXLPhysicalAppend *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalAppend(m_mp, false, false);
	CDXLNode *pdxlnAppend = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);

	// compute a list of indexes of output columns that are actually required
	CColRefArray *reqd_col_array = GPOS_NEW(m_mp) CColRefArray(m_mp);
	ULONG num_total_cols = pdrgpcrOutputAll->Size();
	for (ULONG c=0; c<num_total_cols; c++)
	{
		if (reqdCols->FMember((*pdrgpcrOutputAll)[c]))
		{
			reqd_col_array->Append((*pdrgpcrOutputAll)[c]);
		}
	}
	ULongPtrArray *reqd_col_positions = pdrgpcrOutputAll->IndexesOfSubsequence(reqd_col_array);
	CColRefArray *requiredOutput = pdrgpcrOutputAll->CreateReducedArray(reqd_col_positions);
	reqd_col_array->Release();

	GPOS_ASSERT(NULL != reqd_col_positions);

	// set properties
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprUnionAll);
	pdxlnAppend->SetProperties(dxl_properties);

	// translate project list
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(reqdCols);

	// the append node does not re-order its input or output columns. The
	// re-ordering of its output columns has to be done above it (if needed)
	// via a separate result node
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, requiredOutput);
	pcrsOutput->Release();
	requiredOutput->Release();
	pcrsOutput = NULL;
	requiredOutput = NULL;

	pdxlnAppend->AddChild(pdxlnPrL);

	// scalar condition
	CDXLNode *pdxlnCond = NULL;
	CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);
	pdxlnAppend->AddChild(filter_dxlnode);

	// translate children
	CColRef2dArray *pdrgpdrgpcrInput = popUnionAll->PdrgpdrgpcrInput();
	GPOS_ASSERT(NULL != pdrgpdrgpcrInput);
	const ULONG length = pexprUnionAll->Arity();
	GPOS_ASSERT(length == pdrgpdrgpcrInput->Size());
	for (ULONG ul = 0; ul < length; ul++)
	{
		// translate child
		CColRefArray *pdrgpcrInput = (*pdrgpdrgpcrInput)[ul];
		CColRefArray *requiredInput = pdrgpcrInput->CreateReducedArray(reqd_col_positions);

		CExpression *pexprChild = (*pexprUnionAll)[ul];
		CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, requiredInput, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

		// add a result node on top if necessary so the order of the input project list
		// matches the order in which the append node requires it
		CDXLNode *pdxlnChildProjected = PdxlnRemapOutputColumns
											(
											pexprChild,
											child_dxlnode,
											requiredInput /* required input columns */,
											requiredInput /* order of the input columns */
											);

		pdxlnAppend->AddChild(pdxlnChildProjected);
		requiredInput->Release();
	}
	reqd_col_positions->Release();

	return pdxlnAppend;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdrgpcrMerge
//
//	@doc: 
//		Combines the ordered columns and required columns into a single list
//      with members in the ordered list inserted before the remaining columns in 
//		required list. For instance, if the order list is (c, d) and 
//		the required list is (a, b, c, d) then the combined list is (c, d, a, b)
//---------------------------------------------------------------------------
CColRefArray *
CTranslatorExprToDXL::PdrgpcrMerge
	(
	CMemoryPool *mp,
	CColRefArray *pdrgpcrOrder,
	CColRefArray *pdrgpcrRequired
	)
{
	CColRefSet *pcrsOutput = GPOS_NEW(mp) CColRefSet(mp);

	CColRefArray *pdrgpcrMerge = GPOS_NEW(mp) CColRefArray(mp);

	if (NULL != pdrgpcrOrder)
	{
		const ULONG ulLenOrder = pdrgpcrOrder->Size();
		for (ULONG ul = 0; ul < ulLenOrder; ul++)
		{
			CColRef *colref = (*pdrgpcrOrder)[ul];
			pdrgpcrMerge->Append(colref);
		}
		pcrsOutput->Include(pdrgpcrMerge);
	}

	const ULONG ulLenReqd = pdrgpcrRequired->Size();
	for (ULONG ul = 0; ul < ulLenReqd; ul++)
	{
		CColRef *colref = (*pdrgpcrRequired)[ul];
		if (!pcrsOutput->FMember(colref))
		{
			pcrsOutput->Include(colref);
			pdrgpcrMerge->Append(colref);
		}
	}
	
	pcrsOutput->Release();

	return pdrgpcrMerge;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnRemapOutputColumns
//
//	@doc:
//		Checks if the project list of the given node matches the required
//		columns and their order. If not then a result node is created on
//		top of it
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnRemapOutputColumns
	(
	CExpression *pexpr,
	CDXLNode *dxlnode,
	CColRefArray *pdrgpcrRequired,
	CColRefArray *pdrgpcrOrder
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != pdrgpcrRequired);

	// get project list
	CDXLNode *pdxlnPrL = (*dxlnode)[0];

	CColRefArray *pdrgpcrOrderedReqdCols = PdrgpcrMerge(m_mp, pdrgpcrOrder, pdrgpcrRequired);
	
	// if the combined list is the same as proj list then no
	// further action needed. Otherwise we need result node on top
	if (CTranslatorExprToDXLUtils::FProjectListMatch(pdxlnPrL, pdrgpcrOrderedReqdCols))
	{
		pdrgpcrOrderedReqdCols->Release();
		return dxlnode;
	}

	pdrgpcrOrderedReqdCols->Release();
	
	// output columns of new result node
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(pdrgpcrRequired);

	CDXLNode *pdxlnPrLNew = PdxlnProjList(pcrsOutput, pdrgpcrOrder);
	pcrsOutput->Release();

	// create a result node on top of the current dxl node with a new project list
	return PdxlnResult
				(
				GetProperties(pexpr),
				pdxlnPrLNew,
				dxlnode
				);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnTVF
//
//	@doc:
//		Create a DXL TVF node from an optimizer TVF node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnTVF
	(
	CExpression *pexprTVF,
	CColRefArray *, //colref_array,
	CDistributionSpecArray *, // pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	GPOS_ASSERT(NULL != pexprTVF);

	CPhysicalTVF *popTVF = CPhysicalTVF::PopConvert(pexprTVF->Pop());

	CColRefSet *pcrsOutput = popTVF->DeriveOutputColumns();

	IMDId *mdid_func = popTVF->FuncMdId();
	mdid_func->AddRef();

	IMDId *mdid_return_type = popTVF->ReturnTypeMdId();
	mdid_return_type->AddRef();

	CWStringConst *pstrFunc = GPOS_NEW(m_mp) CWStringConst(m_mp, popTVF->Pstr()->GetBuffer());

	CDXLPhysicalTVF *dxl_op = GPOS_NEW(m_mp) CDXLPhysicalTVF(m_mp, mdid_func, mdid_return_type, pstrFunc);

	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprTVF);
	CDXLNode *pdxlnTVF = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	pdxlnTVF->SetProperties(dxl_properties);
	
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, NULL /*colref_array*/);
	pdxlnTVF->AddChild(pdxlnPrL); 		// project list

	TranslateScalarChildren(pexprTVF, pdxlnTVF);

	return pdxlnTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResultFromConstTableGet
//
//	@doc:
//		Create a DXL result node from an optimizer const table get node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResultFromConstTableGet
	(
	CExpression *pexprCTG,
	CColRefArray *colref_array,
	CExpression *pexprScalar
	)
{
	GPOS_ASSERT(NULL != pexprCTG);
	
	CPhysicalConstTableGet *popCTG = CPhysicalConstTableGet::PopConvert(pexprCTG->Pop());
	
	// construct project list from the const table get values
	CColRefArray *pdrgpcrCTGOutput = popCTG->PdrgpcrOutput();
	IDatum2dArray *pdrgpdrgdatum = popCTG->Pdrgpdrgpdatum();
	
	const ULONG ulRows = pdrgpdrgdatum->Size();
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *one_time_filter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp));

	IDatumArray *pdrgpdatum = NULL;
	if (0 == ulRows)
	{
		// no-tuples... only generate one row of NULLS and one-time "false" filter
		pdrgpdatum = CTranslatorExprToDXLUtils::PdrgpdatumNulls(m_mp, pdrgpcrCTGOutput);

		CExpression *pexprFalse = CUtils::PexprScalarConstBool(m_mp, false /*value*/, false /*is_null*/);
		CDXLNode *pdxlnFalse = PdxlnScConst(pexprFalse);
		pexprFalse->Release();

		one_time_filter->AddChild(pdxlnFalse);
	}
	else
	{
		GPOS_ASSERT(1 <= ulRows);
		pdrgpdatum = (*pdrgpdrgdatum)[0];
		pdrgpdatum->AddRef();
		CDXLNode *pdxlnCond = NULL;
		if (NULL != pexprScalar)
		{
			pdxlnCond = PdxlnScalar(pexprScalar);
			one_time_filter->AddChild(pdxlnCond);
		}
	}

	// if CTG has multiple rows then it has to be a valuescan of constants,
	// else, a Result node is created
	if (ulRows > 1)
	{
		GPOS_ASSERT(NULL != pdrgpcrCTGOutput);

		CColRefSet *pcrsOutput = pexprCTG->DeriveOutputColumns();
		pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrCTGOutput);

		CDXLNode *pdxlnValuesScan = CTranslatorExprToDXLUtils::PdxlnValuesScan
																(
																m_mp,
																GetProperties(pexprCTG),
																pdxlnPrL,
																pdrgpdrgdatum
																);
		one_time_filter->Release();
		pdrgpdatum->Release();

		return pdxlnValuesScan;
	}
	else
	{
		pdxlnPrL = PdxlnProjListFromConstTableGet(colref_array, pdrgpcrCTGOutput, pdrgpdatum);
		pdrgpdatum->Release();
		return CTranslatorExprToDXLUtils::PdxlnResult
											(
											m_mp,
											GetProperties(pexprCTG),
											pdxlnPrL,
											PdxlnFilter(NULL),
											one_time_filter,
											NULL //child_dxlnode
											);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResultFromConstTableGet
//
//	@doc:
//		Create a DXL result node from an optimizer const table get node
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResultFromConstTableGet
	(
	CExpression *pexprCTG,
	CColRefArray *colref_array,
	CDistributionSpecArray *, // pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	BOOL * // pfDML
	)
{
	return PdxlnResultFromConstTableGet(pexprCTG, colref_array, NULL /*pexprScalarCond*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnComputeScalar
//
//	@doc:
//		Create a DXL result node from an optimizer compute scalar expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnComputeScalar
	(
	CExpression *pexprComputeScalar,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprComputeScalar);

	// extract components
	CExpression *pexprRelational = (*pexprComputeScalar)[0];
	CExpression *pexprProjList = (*pexprComputeScalar)[1];

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprRelational, NULL /* colref_array */, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/ );

	// compute required columns
	GPOS_ASSERT(NULL != pexprComputeScalar->Prpp());
	CColRefSet *pcrsOutput = pexprComputeScalar->Prpp()->PcrsRequired();

	// iterate the columns in the projection list, add the columns containing
	// set-returning functions to the output columns
	const ULONG ulPrLs = pexprProjList->Arity();
	for (ULONG ul = 0; ul < ulPrLs; ul++)
	{
		CExpression *pexprPrE = (*pexprProjList)[ul];

		// for column that doesn't contain set-returning function, if it is not the
		// required column in the relational plan properties, then no need to add them
		// to the output columns
		if (pexprPrE->DeriveHasNonScalarFunction())
		{
			CScalarProjectElement *popScPrE = CScalarProjectElement::PopConvert(pexprPrE->Pop());
			pcrsOutput->Include(popScPrE->Pcr());
		}
	}

	// translate project list expression
	CDXLNode *pdxlnPrL = NULL;
	if (NULL == colref_array || CUtils::FHasDuplicates(colref_array))
	{
		pdxlnPrL = PdxlnProjList(pexprProjList, pcrsOutput);
	}
	else
	{
		pdxlnPrL = PdxlnProjList(pexprProjList, pcrsOutput, colref_array);
	}

	// construct a result node
	CDXLNode *pdxlnResult = PdxlnResult
					(
					 GetProperties(pexprComputeScalar),
					 pdxlnPrL,
					 child_dxlnode
					 );

#ifdef GPOS_DEBUG
	(void) CDXLPhysicalResult::Cast(pdxlnResult->GetOperator())->AssertValid(pdxlnResult, false /* validate_children */);
#endif
	return pdxlnResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAggregate
//
//	@doc:
//		Create a DXL aggregate node from an optimizer hash agg expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAggregate
	(
	CExpression *pexprAgg,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprAgg);
	COperator::EOperatorId op_id = pexprAgg->Pop()->Eopid();
	
	// extract components and construct an aggregate node
	CPhysicalAgg *popAgg = NULL;

	GPOS_ASSERT(COperator::EopPhysicalStreamAgg == op_id ||
				COperator::EopPhysicalHashAgg == op_id ||
				COperator::EopPhysicalScalarAgg == op_id);

	EdxlAggStrategy dxl_agg_strategy = EdxlaggstrategySentinel;

	switch (op_id)
	{
		case COperator::EopPhysicalStreamAgg:
						{
							popAgg = CPhysicalStreamAgg::PopConvert(pexprAgg->Pop());
							dxl_agg_strategy = EdxlaggstrategySorted;
							break;
						}
		case COperator::EopPhysicalHashAgg:
						{
							popAgg = CPhysicalHashAgg::PopConvert(pexprAgg->Pop());
							dxl_agg_strategy = EdxlaggstrategyHashed;
							break;
						}
		case COperator::EopPhysicalScalarAgg:
						{
							popAgg = CPhysicalScalarAgg::PopConvert(pexprAgg->Pop());
							dxl_agg_strategy = EdxlaggstrategyPlain;
							break;
						}
		default:
			{
				return NULL;	// to silence the compiler
			}
	}

	const CColRefArray *pdrgpcrGroupingCols = popAgg->PdrgpcrGroupingCols();

	return PdxlnAggregate
			(
			pexprAgg,
			colref_array,
			pdrgpdsBaseTables, 
			pulNonGatherMotions,
			pfDML,
			dxl_agg_strategy,
			pdrgpcrGroupingCols,
			NULL /*pcrsKeys*/
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAggregateDedup
//
//	@doc:
//		Create a DXL aggregate node from an optimizer dedup agg expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAggregateDedup
	(
	CExpression *pexprAgg,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprAgg);
	COperator::EOperatorId op_id = pexprAgg->Pop()->Eopid();

	GPOS_ASSERT(COperator::EopPhysicalStreamAggDeduplicate == op_id ||
				COperator::EopPhysicalHashAggDeduplicate == op_id);

	EdxlAggStrategy dxl_agg_strategy = EdxlaggstrategySentinel;
	const CColRefArray *pdrgpcrGroupingCols = NULL;
	CColRefSet *pcrsKeys = GPOS_NEW(m_mp) CColRefSet(m_mp);

	if (COperator::EopPhysicalStreamAggDeduplicate == op_id)
	{
		CPhysicalStreamAggDeduplicate *popAggDedup = CPhysicalStreamAggDeduplicate::PopConvert(pexprAgg->Pop());
		pcrsKeys->Include(popAggDedup->PdrgpcrKeys());
		pdrgpcrGroupingCols = popAggDedup->PdrgpcrGroupingCols();
		dxl_agg_strategy = EdxlaggstrategySorted;
	}
	else
	{
		CPhysicalHashAggDeduplicate *popAggDedup = CPhysicalHashAggDeduplicate::PopConvert(pexprAgg->Pop());
		pcrsKeys->Include(popAggDedup->PdrgpcrKeys());
		pdrgpcrGroupingCols = popAggDedup->PdrgpcrGroupingCols();
		dxl_agg_strategy = EdxlaggstrategyHashed;
	}

	CDXLNode *pdxlnAgg = PdxlnAggregate
							(
							pexprAgg,
							colref_array,
							pdrgpdsBaseTables, 
							pulNonGatherMotions,
							pfDML,
							dxl_agg_strategy,
							pdrgpcrGroupingCols,
							pcrsKeys
							);
	pcrsKeys->Release();

	return pdxlnAgg;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAggregate
//
//	@doc:
//		Create a DXL aggregate node from an optimizer agg expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAggregate
	(
	CExpression *pexprAgg,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	EdxlAggStrategy dxl_agg_strategy,
	const CColRefArray *pdrgpcrGroupingCols,
	CColRefSet *pcrsKeys
	)
{
	GPOS_ASSERT(NULL != pexprAgg);
	GPOS_ASSERT(NULL != pdrgpcrGroupingCols);
#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = pexprAgg->Pop()->Eopid();
	GPOS_ASSERT_IMP(NULL == pcrsKeys, COperator::EopPhysicalStreamAgg == op_id ||
									COperator::EopPhysicalHashAgg == op_id ||
									COperator::EopPhysicalScalarAgg == op_id);
#endif //GPOS_DEBUG

	// is it safe to stream the local hash aggregate
	BOOL stream_safe = CTranslatorExprToDXLUtils::FLocalHashAggStreamSafe(pexprAgg);

	CExpression *pexprChild = (*pexprAgg)[0];
	CExpression *pexprProjList = (*pexprAgg)[1];

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode
							(
							pexprChild, 
							NULL, // colref_array,
							pdrgpdsBaseTables,  
							pulNonGatherMotions, 
							pfDML, 
							false, // fRemap, 
							false // fRoot
							);

	// compute required columns
	GPOS_ASSERT(NULL != pexprAgg->Prpp());
	CColRefSet *pcrsRequired = pexprAgg->Prpp()->PcrsRequired();

	// translate project list expression
	CDXLNode *proj_list_dxlnode = PdxlnProjList(pexprProjList, pcrsRequired, colref_array);

	// create an empty filter
	CDXLNode *filter_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarFilter(m_mp));

	// construct grouping columns list and check if all the grouping column are
	// already in the project list of the aggregate operator

	const ULONG num_cols = proj_list_dxlnode->Arity();
	UlongToUlongMap *phmululPL = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CDXLNode *pdxlnProjElem = (*proj_list_dxlnode)[ul];
		ULONG colid = CDXLScalarProjElem::Cast(pdxlnProjElem->GetOperator())->Id();

		if (NULL == phmululPL->Find(&colid))
		{
#ifdef GPOS_DEBUG
			BOOL fRes =
#endif
			phmululPL->Insert(GPOS_NEW(m_mp) ULONG(colid), GPOS_NEW(m_mp) ULONG(colid));
			GPOS_ASSERT(fRes);
		}
	}

	ULongPtrArray *pdrgpulGroupingCols = GPOS_NEW(m_mp) ULongPtrArray(m_mp);

	const ULONG length = pdrgpcrGroupingCols->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *pcrGroupingCol = (*pdrgpcrGroupingCols)[ul];

		// only add columns that are either required or in the join keys.
		// if the keys colrefset is null, then skip this check
		if (NULL != pcrsKeys &&
			!pcrsKeys->FMember(pcrGroupingCol) &&
			!pcrsRequired->FMember(pcrGroupingCol))
		{
			continue;
		}

		pdrgpulGroupingCols->Append(GPOS_NEW(m_mp) ULONG(pcrGroupingCol->Id()));

		ULONG colid = pcrGroupingCol->Id();
		if (NULL == phmululPL->Find(&colid))
		{
			CDXLNode *pdxlnProjElem = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, pcrGroupingCol);
			proj_list_dxlnode->AddChild(pdxlnProjElem);
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif
				phmululPL->Insert(GPOS_NEW(m_mp) ULONG(colid), GPOS_NEW(m_mp) ULONG(colid));
			GPOS_ASSERT(fRes);
		}
	}
	
	phmululPL->Release();

	CDXLPhysicalAgg *pdxlopAgg = GPOS_NEW(m_mp) CDXLPhysicalAgg(m_mp, dxl_agg_strategy, stream_safe);
	pdxlopAgg->SetGroupingCols(pdrgpulGroupingCols);

	CDXLNode *pdxlnAgg = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopAgg);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprAgg);
	pdxlnAgg->SetProperties(dxl_properties);

	// add children
	pdxlnAgg->AddChild(proj_list_dxlnode);
	pdxlnAgg->AddChild(filter_dxlnode);
	pdxlnAgg->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlopAgg->AssertValid(pdxlnAgg, false /* validate_children */);
#endif

	return pdxlnAgg;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnSort
//
//	@doc:
//		Create a DXL sort node from an optimizer physical sort expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnSort
	(
	CExpression *pexprSort,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprSort);
	
	GPOS_ASSERT(1 == pexprSort->Arity());

	// extract components
	CPhysicalSort *popSort = CPhysicalSort::PopConvert(pexprSort->Pop());
	CExpression *pexprChild = (*pexprSort)[0];

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// translate order spec
	CDXLNode *sort_col_list_dxlnode = GetSortColListDXL(popSort->Pos());
	
	// construct project list from child project list
	GPOS_ASSERT(NULL != child_dxlnode && 1 <= child_dxlnode->Arity());
	CDXLNode *pdxlnProjListChild = (*child_dxlnode)[0];
	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);

	// create an empty filter
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL);
	
	// construct a sort node
	CDXLPhysicalSort *pdxlopSort = GPOS_NEW(m_mp) CDXLPhysicalSort(m_mp, false /*discard_duplicates*/);
	
	// construct sort node from its components
	CDXLNode *pdxlnSort = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSort);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprSort);
	pdxlnSort->SetProperties(dxl_properties);

	// construct empty limit count and offset nodes
	CDXLNode *limit_count_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarLimitCount(m_mp));
	CDXLNode *limit_offset_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarLimitOffset(m_mp));
	
	// add children
	pdxlnSort->AddChild(proj_list_dxlnode);
	pdxlnSort->AddChild(filter_dxlnode);
	pdxlnSort->AddChild(sort_col_list_dxlnode);
	pdxlnSort->AddChild(limit_count_dxlnode);
	pdxlnSort->AddChild(limit_offset_dxlnode);
	pdxlnSort->AddChild(child_dxlnode);
	
#ifdef GPOS_DEBUG
	pdxlopSort->AssertValid(pdxlnSort, false /* validate_children */);
#endif
	
	return pdxlnSort;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnLimit
//
//	@doc:
//		Create a DXL limit node from an optimizer physical limit expression.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnLimit
	(
	CExpression *pexprLimit,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprLimit);
	GPOS_ASSERT(3 == pexprLimit->Arity());
	
	// extract components
	CExpression *pexprChild = (*pexprLimit)[0];
	CExpression *pexprOffset = (*pexprLimit)[1];
	CExpression *pexprCount = (*pexprLimit)[2];

	// bypass translation of limit if it does not have row count and offset
	CPhysicalLimit *popLimit = CPhysicalLimit::PopConvert(pexprLimit->Pop());
	if (!popLimit->FHasCount() && CUtils::FHasZeroOffset(pexprLimit))
	{
		return CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /*fRoot*/);
	}
	
	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /*fRoot*/);

	// translate limit offset and count
	CDXLNode *limit_offset = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarLimitOffset(m_mp));
	limit_offset->AddChild(PdxlnScalar(pexprOffset));
	
	CDXLNode *limit_count = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarLimitCount(m_mp));
	limit_count->AddChild(PdxlnScalar(pexprCount));
	
	// construct project list from child project list
	GPOS_ASSERT(NULL != child_dxlnode && 1 <= child_dxlnode->Arity());
	CDXLNode *pdxlnProjListChild = (*child_dxlnode)[0];
	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);

	// construct a limit node
	CDXLPhysicalLimit *pdxlopLimit = GPOS_NEW(m_mp) CDXLPhysicalLimit(m_mp);

	// construct limit node from its components
	CDXLNode *pdxlnLimit = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopLimit);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprLimit);
	pdxlnLimit->SetProperties(dxl_properties);
	
	pdxlnLimit->AddChild(proj_list_dxlnode);
	pdxlnLimit->AddChild(child_dxlnode);
	pdxlnLimit->AddChild(limit_count);
	pdxlnLimit->AddChild(limit_offset);
	
#ifdef GPOS_DEBUG
	pdxlopLimit->AssertValid(pdxlnLimit, false /* validate_children */);
#endif
	
	return pdxlnLimit;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::BuildSubplansForCorrelatedLOJ
//
//	@doc:
//		Helper to build subplans from correlated LOJ
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::BuildSubplansForCorrelatedLOJ
	(
	CExpression *pexprCorrelatedLOJ,
	CDXLColRefArray *dxl_colref_array,
	CDXLNode **ppdxlnScalar, // output: scalar condition after replacing inner child reference with subplan
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprCorrelatedLOJ);
	GPOS_ASSERT(COperator::EopPhysicalCorrelatedLeftOuterNLJoin == pexprCorrelatedLOJ->Pop()->Eopid());

	CExpression *pexprInner = (*pexprCorrelatedLOJ)[1];
	CExpression *pexprScalar = (*pexprCorrelatedLOJ)[2];

	CColRefArray *pdrgpcrInner = CPhysicalNLJoin::PopConvert(pexprCorrelatedLOJ->Pop())->PdrgPcrInner();
	GPOS_ASSERT(NULL != pdrgpcrInner);

	EdxlSubPlanType dxl_subplan_type = Edxlsubplantype(pexprCorrelatedLOJ);

	if (EdxlSubPlanTypeScalar == dxl_subplan_type)
	{
		// for correlated left outer join for scalar subplan type, we generate a scalar subplan
		BuildScalarSubplans(pdrgpcrInner, pexprInner, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);

		// now translate the scalar - references to the inner child will be
		// replaced by the subplan
		*ppdxlnScalar = PdxlnScalar(pexprScalar);

		return;
	}

	GPOS_ASSERT
		(
		EdxlSubPlanTypeAny == dxl_subplan_type ||
		EdxlSubPlanTypeAll == dxl_subplan_type ||
		EdxlSubPlanTypeExists == dxl_subplan_type ||
		EdxlSubPlanTypeNotExists == dxl_subplan_type
		);

	// for correlated left outer join with non-scalar subplan type,
	// we need to generate quantified/exitential subplan
	if (EdxlSubPlanTypeAny == dxl_subplan_type || EdxlSubPlanTypeAll == dxl_subplan_type)
	{
		(void) PdxlnQuantifiedSubplan(pdrgpcrInner, pexprCorrelatedLOJ, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
	}
	else
	{
		GPOS_ASSERT(EdxlSubPlanTypeExists == dxl_subplan_type || EdxlSubPlanTypeNotExists == dxl_subplan_type);
		(void) PdxlnExistentialSubplan(pdrgpcrInner, pexprCorrelatedLOJ, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
	}

	CExpression *pexprTrue = CUtils::PexprScalarConstBool(m_mp, true /*value*/, false /*is_null*/);
	*ppdxlnScalar = PdxlnScalar(pexprTrue);
	pexprTrue->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::BuildSubplans
//
//	@doc:
//		Helper to build subplans of different types
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::BuildSubplans
	(
	CExpression *pexprCorrelatedNLJoin,
	CDXLColRefArray *dxl_colref_array,
	CDXLNode **ppdxlnScalar, // output: scalar condition after replacing inner child reference with subplan
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(CUtils::FCorrelatedNLJoin(pexprCorrelatedNLJoin->Pop()));
	GPOS_ASSERT(NULL != ppdxlnScalar);

	CExpression *pexprInner = (*pexprCorrelatedNLJoin)[1];
	CExpression *pexprScalar = (*pexprCorrelatedNLJoin)[2];

	CColRefArray *pdrgpcrInner = CPhysicalNLJoin::PopConvert(pexprCorrelatedNLJoin->Pop())->PdrgPcrInner();
	GPOS_ASSERT(NULL != pdrgpcrInner);

	COperator::EOperatorId op_id = pexprCorrelatedNLJoin->Pop()->Eopid();
	CDXLNode *pdxlnSubPlan = NULL;
	switch (op_id)
	{
		case COperator::EopPhysicalCorrelatedLeftOuterNLJoin:
			BuildSubplansForCorrelatedLOJ(pexprCorrelatedNLJoin, dxl_colref_array, ppdxlnScalar, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
			return;

		case COperator::EopPhysicalCorrelatedInnerNLJoin:
			BuildScalarSubplans(pdrgpcrInner, pexprInner, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);

			// now translate the scalar - references to the inner child will be
			// replaced by the subplan
			*ppdxlnScalar = PdxlnScalar(pexprScalar);
			return;

		case COperator::EopPhysicalCorrelatedInLeftSemiNLJoin:
		case COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin:
			pdxlnSubPlan = PdxlnQuantifiedSubplan(pdrgpcrInner, pexprCorrelatedNLJoin, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
			pdxlnSubPlan->AddRef();
			*ppdxlnScalar = pdxlnSubPlan;
			return;

		case COperator::EopPhysicalCorrelatedLeftSemiNLJoin:
		case COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin:
			pdxlnSubPlan = PdxlnExistentialSubplan(pdrgpcrInner, pexprCorrelatedNLJoin, dxl_colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
			pdxlnSubPlan->AddRef();
			*ppdxlnScalar = pdxlnSubPlan;
			return;

		default:
				GPOS_ASSERT(!"Unsupported correlated join");
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnRestrictResult
//
//	@doc:
//		Helper to build a Result expression with project list
//		restricted to required column
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnRestrictResult
	(
	CDXLNode *dxlnode,
	CColRef *colref
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != colref);

	CDXLNode *pdxlnProjListOld = (*dxlnode)[0];
	const ULONG ulPrjElems = pdxlnProjListOld->Arity();

	if (0 == ulPrjElems)
	{
		// failed to find project elements
		dxlnode->Release();
		return NULL;
	}

	CDXLNode *pdxlnResult = dxlnode;
	if (1 < ulPrjElems)
	{
		// restrict project list to required column
		CDXLScalarProjList *pdxlopPrL = GPOS_NEW(m_mp) CDXLScalarProjList(m_mp);
		CDXLNode *pdxlnProjListNew = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopPrL);

		for (ULONG ul = 0; ul < ulPrjElems; ul++)
		{
			CDXLNode *child_dxlnode = (*pdxlnProjListOld)[ul];
			CDXLScalarProjElem *pdxlPrjElem = CDXLScalarProjElem::Cast(child_dxlnode->GetOperator());
			if (pdxlPrjElem->Id() == colref->Id())
			{
				// create a new project element that simply points to required column,
				// we cannot re-use child_dxlnode here since it may have a deep expression with columns inaccessible
				// above the child (inner) DXL expression
				CDXLNode *pdxlnPrEl = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
				pdxlnProjListNew->AddChild(pdxlnPrEl);
			}
		}
		GPOS_ASSERT(1 == pdxlnProjListNew->Arity());

		pdxlnResult = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLPhysicalResult(m_mp));
		CDXLPhysicalProperties *dxl_properties = CTranslatorExprToDXLUtils::PdxlpropCopy(m_mp, dxlnode);
		pdxlnResult->SetProperties(dxl_properties);

		pdxlnResult->AddChild(pdxlnProjListNew);
		pdxlnResult->AddChild(PdxlnFilter(NULL));
		pdxlnResult->AddChild(GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp)));
		pdxlnResult->AddChild(dxlnode);
	}

	return pdxlnResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnQuantifiedSubplan
//
//	@doc:
//		Helper to build subplans for quantified (ANY/ALL) subqueries
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnQuantifiedSubplan
	(
	CColRefArray *pdrgpcrInner,
	CExpression *pexprCorrelatedNLJoin,
	CDXLColRefArray *dxl_colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	COperator *popCorrelatedJoin = pexprCorrelatedNLJoin->Pop();
	COperator::EOperatorId op_id = popCorrelatedJoin->Eopid();
	BOOL fCorrelatedLOJ = (COperator::EopPhysicalCorrelatedLeftOuterNLJoin == op_id);
	GPOS_ASSERT(COperator::EopPhysicalCorrelatedInLeftSemiNLJoin == op_id ||
			COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin == op_id ||
			fCorrelatedLOJ);

	EdxlSubPlanType dxl_subplan_type = Edxlsubplantype(pexprCorrelatedNLJoin);
	GPOS_ASSERT_IMP(fCorrelatedLOJ, EdxlSubPlanTypeAny == dxl_subplan_type || EdxlSubPlanTypeAll == dxl_subplan_type);

	CExpression *pexprInner = (*pexprCorrelatedNLJoin)[1];
	CExpression *pexprScalar = (*pexprCorrelatedNLJoin)[2];

	// translate inner child
	CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInner, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// find required column from inner child
	CColRef *pcrInner = (*pdrgpcrInner)[0];

	if (fCorrelatedLOJ)
	{
		// overwrite required inner column based on scalar expression

		CColRefSet *pcrsInner = pexprInner->DeriveOutputColumns();
		CColRefSet *pcrsUsed = GPOS_NEW(m_mp) CColRefSet (m_mp, *pexprScalar->DeriveUsedColumns());
		pcrsUsed->Intersection(pcrsInner);
		if (0 < pcrsUsed->Size())
		{
			GPOS_ASSERT(1 == pcrsUsed->Size());

			pcrInner = pcrsUsed->PcrFirst();
		}
		pcrsUsed->Release();
	}

	CDXLNode *inner_dxlnode = PdxlnRestrictResult(pdxlnInnerChild, pcrInner);
	if (NULL == inner_dxlnode)
	{
		GPOS_RAISE(gpopt::ExmaDXL, gpopt::ExmiExpr2DXLUnsupportedFeature, GPOS_WSZ_LIT("Outer references in the project list of a correlated subquery"));
	}

	// translate test expression
	CDXLNode *dxlnode_test_expr = PdxlnScalar(pexprScalar);

	const IMDTypeBool *pmdtypebool = m_pmda->PtMDType<IMDTypeBool>();
	IMDId *mdid = pmdtypebool->MDId();
	mdid->AddRef();

	// construct a subplan node, with the inner child under it
	CDXLNode *pdxlnSubPlan = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSubPlan(m_mp, mdid, dxl_colref_array, dxl_subplan_type, dxlnode_test_expr));
	pdxlnSubPlan->AddChild(inner_dxlnode);

	// add to hashmap
#ifdef GPOS_DEBUG
	BOOL fRes =
#endif // GPOS_DEBUG
		m_phmcrdxln->Insert(const_cast<CColRef *>((*pdrgpcrInner)[0]), pdxlnSubPlan);
	GPOS_ASSERT(fRes);

	return pdxlnSubPlan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjectBoolConst
//
//	@doc:
//		Helper to add a project of bool constant on top of given DXL node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjectBoolConst
	(
	CDXLNode *dxlnode,
	BOOL value
	)
{
	GPOS_ASSERT(NULL != dxlnode);

	// create a new project element with bool value
	const IMDTypeBool *pmdtypebool = m_pmda->PtMDType<IMDTypeBool>();
	IMDId *mdid = pmdtypebool->MDId();
	mdid->AddRef();

	CDXLDatumBool *dxl_datum = GPOS_NEW(m_mp) CDXLDatumBool(m_mp, mdid, false /* is_null */,  value);
	CDXLScalarConstValue *pdxlopConstValue = GPOS_NEW(m_mp) CDXLScalarConstValue(m_mp, dxl_datum);
	CColRef *colref = m_pcf->PcrCreate(pmdtypebool, default_type_modifier);
	CDXLNode *pdxlnPrEl = PdxlnProjElem(colref, GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopConstValue));

	CDXLScalarProjList *pdxlopPrL = GPOS_NEW(m_mp) CDXLScalarProjList(m_mp);
	CDXLNode *proj_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopPrL);
	proj_list_dxlnode->AddChild(pdxlnPrEl);
	CDXLNode *pdxlnResult = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLPhysicalResult(m_mp));
	CDXLPhysicalProperties *dxl_properties = CTranslatorExprToDXLUtils::PdxlpropCopy(m_mp, dxlnode);
	pdxlnResult->SetProperties(dxl_properties);

	pdxlnResult->AddChild(proj_list_dxlnode);
	pdxlnResult->AddChild(PdxlnFilter(NULL));
	pdxlnResult->AddChild(GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp)));
	pdxlnResult->AddChild(dxlnode);

	return pdxlnResult;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::EdxlsubplantypeCorrelatedLOJ
//
//	@doc:
//		Helper to find subplan type from a correlated left outer
//		join expression
//
//---------------------------------------------------------------------------
EdxlSubPlanType
CTranslatorExprToDXL::EdxlsubplantypeCorrelatedLOJ
	(
	CExpression *pexprCorrelatedLOJ
	)
{
	GPOS_ASSERT(NULL != pexprCorrelatedLOJ);
	GPOS_ASSERT(COperator::EopPhysicalCorrelatedLeftOuterNLJoin == pexprCorrelatedLOJ->Pop()->Eopid());

	COperator::EOperatorId eopidSubq =
			CPhysicalCorrelatedLeftOuterNLJoin::PopConvert(pexprCorrelatedLOJ->Pop())->EopidOriginSubq();
	switch (eopidSubq)
	{
		case COperator::EopScalarSubquery:
			return EdxlSubPlanTypeScalar;

		case COperator::EopScalarSubqueryAll:
			return EdxlSubPlanTypeAll;

		case COperator::EopScalarSubqueryAny:
			return EdxlSubPlanTypeAny;

		case COperator::EopScalarSubqueryExists:
			return EdxlSubPlanTypeExists;

		case COperator::EopScalarSubqueryNotExists:
			return  EdxlSubPlanTypeNotExists;

		default:
			GPOS_ASSERT(!"Unexpected origin subquery in correlated left outer join");
	}

	return EdxlSubPlanTypeSentinel;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::Edxlsubplantype
//
//	@doc:
//		Helper to find subplan type from a correlated join expression
//
//---------------------------------------------------------------------------
EdxlSubPlanType
CTranslatorExprToDXL::Edxlsubplantype
	(
	CExpression *pexprCorrelatedNLJoin
	)
{
	GPOS_ASSERT(NULL != pexprCorrelatedNLJoin);
	GPOS_ASSERT(CUtils::FCorrelatedNLJoin(pexprCorrelatedNLJoin->Pop()));

	COperator::EOperatorId op_id = pexprCorrelatedNLJoin->Pop()->Eopid();
	switch (op_id)
	{
		case COperator::EopPhysicalCorrelatedLeftOuterNLJoin:
			return EdxlsubplantypeCorrelatedLOJ(pexprCorrelatedNLJoin);

		case COperator::EopPhysicalCorrelatedInnerNLJoin:
			return EdxlSubPlanTypeScalar;

		case COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin:
			return EdxlSubPlanTypeAll;

		case COperator::EopPhysicalCorrelatedInLeftSemiNLJoin:
			return EdxlSubPlanTypeAny;

		case COperator::EopPhysicalCorrelatedLeftSemiNLJoin:
			return EdxlSubPlanTypeExists;

		case COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin:
			return EdxlSubPlanTypeNotExists;

		default:
			GPOS_ASSERT(!"Unexpected correlated join");
	}

	return EdxlSubPlanTypeSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnExistentialSubplan
//
//	@doc:
//		Helper to build subplans for existential subqueries
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnExistentialSubplan
	(
	CColRefArray *pdrgpcrInner,
	CExpression *pexprCorrelatedNLJoin,
	CDXLColRefArray *dxl_colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = pexprCorrelatedNLJoin->Pop()->Eopid();
	BOOL fCorrelatedLOJ = (COperator::EopPhysicalCorrelatedLeftOuterNLJoin == op_id);
#endif // GPOS_DEBUG
	GPOS_ASSERT(COperator::EopPhysicalCorrelatedLeftSemiNLJoin == op_id ||
			COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin == op_id ||
			fCorrelatedLOJ);

	EdxlSubPlanType dxl_subplan_type = Edxlsubplantype(pexprCorrelatedNLJoin);
	GPOS_ASSERT_IMP(fCorrelatedLOJ, EdxlSubPlanTypeExists == dxl_subplan_type || EdxlSubPlanTypeNotExists == dxl_subplan_type);

	// translate inner child
	CExpression *pexprInner = (*pexprCorrelatedNLJoin)[1];
	
	CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInner, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	CDXLNode *pdxlnInnerProjList = (*pdxlnInnerChild)[0];
	CDXLNode *inner_dxlnode = NULL;
	if (0 == pdxlnInnerProjList->Arity())
	{
		// no requested columns from subplan, add a dummy boolean constant to project list
		inner_dxlnode = PdxlnProjectBoolConst(pdxlnInnerChild, true /*value*/);
	}
	else
	{
		// restrict requested columns to required inner column
		inner_dxlnode = PdxlnRestrictResult(pdxlnInnerChild, (*pdrgpcrInner)[0]);
	}
	
	if (NULL == inner_dxlnode)
	{
		GPOS_RAISE(gpopt::ExmaDXL, gpopt::ExmiExpr2DXLUnsupportedFeature, GPOS_WSZ_LIT("Outer references in the project list of a correlated subquery"));
	}

	const IMDTypeBool *pmdtypebool = m_pmda->PtMDType<IMDTypeBool>();
	IMDId *mdid = pmdtypebool->MDId();
	mdid->AddRef();

	// construct a subplan node, with the inner child under it
	CDXLNode *pdxlnSubPlan =
		GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSubPlan(m_mp, mdid, dxl_colref_array, dxl_subplan_type, NULL /*dxlnode_test_expr*/));
	pdxlnSubPlan->AddChild(inner_dxlnode);

	// add to hashmap
#ifdef GPOS_DEBUG
	BOOL fRes =
#endif // GPOS_DEBUG
		m_phmcrdxln->Insert(const_cast<CColRef *>((*pdrgpcrInner)[0]), pdxlnSubPlan);
	GPOS_ASSERT(fRes);

	return pdxlnSubPlan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::BuildScalarSubplans
//
//	@doc:
//		Helper to build subplans from inner column references and store
//		generated subplans in subplan map
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::BuildScalarSubplans
	(
	CColRefArray *pdrgpcrInner,
	CExpression *pexprInner,
	CDXLColRefArray *dxl_colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	const ULONG size = pdrgpcrInner->Size();

	CDXLNodeArray *pdrgpdxlnInner = GPOS_NEW(m_mp) CDXLNodeArray(m_mp);
	for (ULONG ul = 0; ul < size; ul++)
	{
		// for each subplan, we need to re-translate inner expression
		CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInner, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
		CDXLNode *inner_dxlnode = PdxlnRestrictResult(pdxlnInnerChild, (*pdrgpcrInner)[ul]);
		if (NULL == inner_dxlnode)
		{
			GPOS_RAISE(gpopt::ExmaDXL, gpopt::ExmiExpr2DXLUnsupportedFeature, GPOS_WSZ_LIT("Outer references in the project list of a correlated subquery"));
		}
		pdrgpdxlnInner->Append(inner_dxlnode);
	}

	for (ULONG ul = 0; ul < size; ul++)
	{
		CDXLNode *inner_dxlnode = (*pdrgpdxlnInner)[ul];
		inner_dxlnode->AddRef();
		if (0 < ul)
		{
			// if there is more than one subplan, we need to add-ref passed arrays
			dxl_colref_array->AddRef();
		}
		const CColRef *pcrInner = (*pdrgpcrInner)[ul];
		BuildDxlnSubPlan(inner_dxlnode, pcrInner, dxl_colref_array);
	}

	pdrgpdxlnInner->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PcrsOuterRefsForCorrelatedNLJoin
//
//	@doc:
//		Return outer refs in correlated join inner child
//
//---------------------------------------------------------------------------
CColRefSet *
CTranslatorExprToDXL::PcrsOuterRefsForCorrelatedNLJoin
	(
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(CUtils::FCorrelatedNLJoin(pexpr->Pop()));

	CExpression *pexprInnerChild = (*pexpr)[1];

	return pexprInnerChild->DeriveOuterReferences();
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnCorrelatedNLJoin
//
//	@doc:
//		Translate correlated NLJ expression.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnCorrelatedNLJoin
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(CUtils::FCorrelatedNLJoin(pexpr->Pop()));

	// extract components
	CExpression *pexprOuterChild = (*pexpr)[0];
	CExpression *pexprInnerChild = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	// outer references in the inner child
	CDXLColRefArray *dxl_colref_array = GPOS_NEW(m_mp) CDXLColRefArray(m_mp);

	CColRefSet *outer_refs = PcrsOuterRefsForCorrelatedNLJoin(pexpr);
	CColRefSetIter crsi(*outer_refs);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CMDName *mdname = GPOS_NEW(m_mp) CMDName(m_mp, colref->Name().Pstr());
		IMDId *mdid = colref->RetrieveType()->MDId();
		mdid->AddRef();
		CDXLColRef *dxl_colref = GPOS_NEW(m_mp) CDXLColRef(m_mp, mdname, colref->Id(), mdid, colref->TypeModifier());
		dxl_colref_array->Append(dxl_colref);
	}

	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();
	CDXLNode *pdxlnCond = NULL;

    // Create a subplan with a Boolean from the inner child if we have a Const True as a join condition.
    // One scenario for this is when IN sublinks contain a projection from the outer table only such as:
    // select * from foo where foo.a in (select foo.b from bar);
    // If bar is a very small table, ORCA generates a CorrelatedInLeftSemiNLJoin with a Const true join filter
    // and condition foo.a = foo.b is added as a filter on the table scan of foo. If bar is a large table,
    // ORCA generates a plan with CorrelatedInnerNLJoin with a Const true join filter and a LIMIT over the
    // scan of bar. The same foo.a = foo.b condition is also added as a filter on the table scan of foo.
	if (CUtils::FScalarConstTrue(pexprScalar) &&
		(COperator::EopPhysicalCorrelatedInnerNLJoin == op_id || COperator::EopPhysicalCorrelatedInLeftSemiNLJoin == op_id))
	{
		// translate relational inner child expression
		CDXLNode *pdxlnInnerChild = CreateDXLNode
									(
									pexprInnerChild, 
									NULL, // colref_array,
									pdrgpdsBaseTables, 
									pulNonGatherMotions, 
									pfDML, 
									false, // fRemap
									false // fRoot
									);

		// if the filter predicate is a constant TRUE, create a subplan that returns
		// Boolean from the inner child, and use that as the scalar condition
		pdxlnCond = PdxlnBooleanScalarWithSubPlan(pdxlnInnerChild, dxl_colref_array);
	}
	else
	{
		BuildSubplans(pexpr, dxl_colref_array, &pdxlnCond, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
	}

	// extract dxl properties from correlated join
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexpr);
	CDXLNode *dxlnode = NULL;

	switch (pexprOuterChild->Pop()->Eopid())
	{
		case COperator::EopPhysicalTableScan:
		{
			dxl_properties->AddRef();
			// create and return a table scan node
			dxlnode = PdxlnTblScanFromNLJoinOuter(pexprOuterChild, pdxlnCond, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, dxl_properties);
			break;
		}

		case COperator::EopPhysicalFilter:
		{
			dxl_properties->AddRef();
			dxlnode = PdxlnResultFromNLJoinOuter(pexprOuterChild, pdxlnCond, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, dxl_properties);
			break;
		}

		default:
		{
			// create a result node over outer child
			dxl_properties->AddRef();
			dxlnode = PdxlnResult(pexprOuterChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, pdxlnCond, dxl_properties);
		}
	}

	dxl_properties->Release();
	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::BuildDxlnSubPlan
//
//	@doc:
//		Construct a scalar dxl node with a subplan as its child. Also put this
//		subplan in the hashmap with its output column, so that anyone who
//		references that column can use the subplan
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::BuildDxlnSubPlan
	(
	CDXLNode *pdxlnRelChild,
	const CColRef *colref,
	CDXLColRefArray *dxl_colref_array
	)
{
	GPOS_ASSERT(NULL != colref);
	IMDId *mdid = colref->RetrieveType()->MDId();
	mdid->AddRef();

	// construct a subplan node, with the inner child under it
	CDXLNode *pdxlnSubPlan = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSubPlan(m_mp, mdid, dxl_colref_array, EdxlSubPlanTypeScalar, NULL));
	pdxlnSubPlan->AddChild(pdxlnRelChild);

	// add to hashmap
#ifdef GPOS_DEBUG
	BOOL fRes =
#endif // GPOS_DEBUG
	m_phmcrdxln->Insert(const_cast<CColRef *>(colref), pdxlnSubPlan);
	GPOS_ASSERT(fRes);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnBooleanScalarWithSubPlan
//
//	@doc:
//		Construct a boolean scalar dxl node with a subplan as its child. The
//		sublan has a boolean output column, and has	the given relational child
//		under it
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnBooleanScalarWithSubPlan
	(
	CDXLNode *pdxlnRelChild,
	CDXLColRefArray *dxl_colref_array
	)
{
	// create a new project element (const:true), and replace the first child with it
	const IMDTypeBool *pmdtypebool = m_pmda->PtMDType<IMDTypeBool>();
	IMDId *mdid = pmdtypebool->MDId();
	mdid->AddRef();

	CDXLDatumBool *dxl_datum = GPOS_NEW(m_mp) CDXLDatumBool(m_mp, mdid, false /* is_null */, true /* value */);
	CDXLScalarConstValue *pdxlopConstValue = GPOS_NEW(m_mp) CDXLScalarConstValue(m_mp, dxl_datum);

	CColRef *colref = m_pcf->PcrCreate(pmdtypebool, default_type_modifier);

	CDXLNode *pdxlnPrEl = PdxlnProjElem(colref, GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopConstValue));

	// create a new Result node for the created project element
	CDXLNode *pdxlnProjListNew = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarProjList(m_mp));
	pdxlnProjListNew->AddChild(pdxlnPrEl);
	CDXLNode *pdxlnResult = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLPhysicalResult(m_mp));
	CDXLPhysicalProperties *dxl_properties = CTranslatorExprToDXLUtils::PdxlpropCopy(m_mp, pdxlnRelChild);
	pdxlnResult->SetProperties(dxl_properties);
	pdxlnResult->AddChild(pdxlnProjListNew);
	pdxlnResult->AddChild(PdxlnFilter(NULL));
	pdxlnResult->AddChild(GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp)));
	pdxlnResult->AddChild(pdxlnRelChild);

	// construct a subplan node, with the Result node under it
	mdid->AddRef();
	CDXLNode *pdxlnSubPlan = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSubPlan(m_mp, mdid, dxl_colref_array, EdxlSubPlanTypeScalar, NULL));
	pdxlnSubPlan->AddChild(pdxlnResult);

	return pdxlnSubPlan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScBoolExpr
//
//	@doc:
//		Create a DXL scalar boolean node given two DXL boolean nodes
//		and a boolean op
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScBoolExpr
	(
	EdxlBoolExprType boolexptype,
	CDXLNode *dxlnode_left,
	CDXLNode *dxlnode_right
	)
{
	CDXLNode *pdxlnBoolExpr = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, boolexptype));

	pdxlnBoolExpr->AddChild(dxlnode_left);
	pdxlnBoolExpr->AddChild(dxlnode_right);

	return pdxlnBoolExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnTblScanFromNLJoinOuter
//
//	@doc:
//		Create a DXL table scan node from the outer child of a NLJ
//		and a DXL scalar condition. Used for translated correlated
//		subqueries.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnTblScanFromNLJoinOuter
	(
	CExpression *pexprRelational,
	CDXLNode *pdxlnCond,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *, // pulNonGatherMotions,
	CDXLPhysicalProperties *dxl_properties
	)
{
	// create a table scan over the input expression, without a filter
	CDXLNode *pdxlnTblScan = PdxlnTblScan
								(
								pexprRelational,
								NULL, //pcrsOutput
								colref_array,
								pdrgpdsBaseTables, 
								NULL, //pexprScalar
								dxl_properties
								);

	if (!CTranslatorExprToDXLUtils::FScalarConstTrue(m_pmda, pdxlnCond))
	{
		// add the new filter to the table scan replacing its original
		// empty filter
		CDXLNode *filter_dxlnode = PdxlnFilter(pdxlnCond);
		pdxlnTblScan->ReplaceChild(EdxltsIndexFilter /*ulPos*/, filter_dxlnode);
	}
	else
	{
		// not used
		pdxlnCond->Release();
	}

	return pdxlnTblScan;
}

static ULONG
UlIndexFilter(Edxlopid edxlopid)
{
	switch (edxlopid)
	{
		case EdxlopPhysicalTableScan:
		case EdxlopPhysicalExternalScan:
			return EdxltsIndexFilter;
		case EdxlopPhysicalBitmapTableScan:
		case EdxlopPhysicalDynamicBitmapTableScan:
			return EdxlbsIndexFilter;
		case EdxlopPhysicalDynamicTableScan:
			return EdxldtsIndexFilter;
		case EdxlopPhysicalIndexScan:
		case EdxlopPhysicalDynamicIndexScan:
			return EdxlisIndexFilter;
		case EdxlopPhysicalResult:
			return EdxlresultIndexFilter;
		default:
			GPOS_RTL_ASSERT("Unexpected operator. Expected operators that contain a filter child");
			return gpos::ulong_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnResultFromNLJoinOuter
//
//	@doc:
//		Create a DXL result node from the outer child of a NLJ
//		and a DXL scalar join condition. Used for translated correlated
//		subqueries.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnResultFromNLJoinOuter
	(
	CExpression *pexprOuterChildRelational,
	CDXLNode *pdxlnJoinCond,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables, 
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CDXLPhysicalProperties *dxl_properties
	)
{
	// create a result node using the filter from the outer child of the input expression
	CDXLNode *pdxlnRelationalNew = PdxlnFromFilter(pexprOuterChildRelational, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, dxl_properties);
	dxl_properties->Release();

	Edxlopid edxlopid = pdxlnRelationalNew->GetOperator()->GetDXLOperator();
	switch (edxlopid)
	{
		case EdxlopPhysicalTableScan:
		case EdxlopPhysicalExternalScan:
		case EdxlopPhysicalBitmapTableScan:
		case EdxlopPhysicalDynamicTableScan:
		case EdxlopPhysicalIndexScan:
		case EdxlopPhysicalDynamicIndexScan:
		case EdxlopPhysicalDynamicBitmapTableScan:
		case EdxlopPhysicalResult:
		{
			// if the scalar join condition is a constant TRUE, just translate the child, no need to create an AND expression
			if (CTranslatorExprToDXLUtils::FScalarConstTrue(m_pmda, pdxlnJoinCond))
			{
				pdxlnJoinCond->Release();
				break;
			}

			// create new AND expression with the outer child's filter node and the join condition
			ULONG ulIndexFilter = UlIndexFilter(edxlopid);
			GPOS_ASSERT(ulIndexFilter != gpos::ulong_max);
			CDXLNode *pdxlnChildFilter = (*pdxlnRelationalNew)[ulIndexFilter];
			GPOS_ASSERT(EdxlopScalarFilter == pdxlnChildFilter->GetOperator()->GetDXLOperator());
			CDXLNode *newFilterPred = pdxlnJoinCond;

			if (0 < pdxlnChildFilter->Arity())
			{
				// we have both a filter condition (from the outer child) in our result node
				// and a non-trivial condition pdxlnJoinCond passed in as parameter, need to AND the two
				CDXLNode *pdxlnCondFromChildFilter = (*pdxlnChildFilter)[0];

				GPOS_ASSERT(2 > pdxlnChildFilter->Arity());
				pdxlnCondFromChildFilter->AddRef();

				newFilterPred = PdxlnScBoolExpr(Edxland, pdxlnCondFromChildFilter, pdxlnJoinCond);
			}

			// add the new filter to the result replacing its original
			// empty filter
			CDXLNode *new_filter_dxlnode = PdxlnFilter(newFilterPred);
			pdxlnRelationalNew->ReplaceChild(ulIndexFilter /*ulPos*/, new_filter_dxlnode);
		}
			break;
		// In case the OuterChild is a physical sequence, it will already have the filter in the partition selector and
		// dynamic scan, thus we should not replace the filter.
		case EdxlopPhysicalSequence:
		{
			dxl_properties->AddRef();
			GPOS_ASSERT(NULL != pexprOuterChildRelational->Prpp());
			CColRefSet *pcrsOutput = pexprOuterChildRelational->Prpp()->PcrsRequired();
			pdxlnRelationalNew = PdxlnAddScalarFilterOnRelationalChild
							(
							pdxlnRelationalNew,
							pdxlnJoinCond,
							dxl_properties,
							pcrsOutput,
							colref_array
							);
		}
			break;
		default:
			pdxlnJoinCond->Release();
			GPOS_RTL_ASSERT(false && "Unexpected node here");
	}

	return pdxlnRelationalNew;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::StoreIndexNLJOuterRefs
//
//	@doc:
//		Store outer references in index NLJ inner child into global map
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::StoreIndexNLJOuterRefs
	(
	CPhysical *pop
	)
{
	CColRefArray *colref_array = NULL;

	if (COperator::EopPhysicalInnerIndexNLJoin == pop->Eopid())
	{
		colref_array = CPhysicalInnerIndexNLJoin::PopConvert(pop)->PdrgPcrOuterRefs();
	}
	else
	{
		colref_array = CPhysicalLeftOuterIndexNLJoin::PopConvert(pop)->PdrgPcrOuterRefs();
	}
	GPOS_ASSERT(colref_array != NULL);

	const ULONG size = colref_array->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		if (NULL == m_phmcrdxlnIndexLookup->Find(colref))
		{
			CDXLNode *dxlnode = CTranslatorExprToDXLUtils::PdxlnIdent(m_mp, m_phmcrdxln, m_phmcrdxlnIndexLookup, colref);
#ifdef 	GPOS_DEBUG
			BOOL fInserted =
#endif // GPOS_DEBUG
			m_phmcrdxlnIndexLookup->Insert(colref, dxlnode);
			GPOS_ASSERT(fInserted);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnNLJoin
//
//	@doc:
//		Create a DXL nested loop join node from an optimizer nested loop
//		join expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnNLJoin
	(
	CExpression *pexprInnerNLJ,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprInnerNLJ);
	GPOS_ASSERT(3 == pexprInnerNLJ->Arity());

	// extract components
	CPhysical *pop = CPhysical::PopConvert(pexprInnerNLJ->Pop());

	CExpression *pexprOuterChild = (*pexprInnerNLJ)[0];
	CExpression *pexprInnerChild = (*pexprInnerNLJ)[1];
	CExpression *pexprScalar = (*pexprInnerNLJ)[2];


#ifdef GPOS_DEBUG
	GPOS_ASSERT_IMP(COperator::EopPhysicalInnerIndexNLJoin != pop->Eopid() &&
					COperator::EopPhysicalLeftOuterIndexNLJoin != pop->Eopid()
			, pexprInnerChild->DeriveOuterReferences()->IsDisjoint(pexprOuterChild->DeriveOutputColumns()) &&
			"detected outer references in NL inner child");
#endif // GPOS_DEBUG

	EdxlJoinType join_type = EdxljtSentinel;
	BOOL is_index_nlj = false;
	CColRefArray *outer_refs = NULL;
	switch (pop->Eopid())
	{
		case COperator::EopPhysicalInnerNLJoin:
			join_type = EdxljtInner;
			break;

		case COperator::EopPhysicalInnerIndexNLJoin:
			join_type = EdxljtInner;
			is_index_nlj = true;
			StoreIndexNLJOuterRefs(pop);
			outer_refs = CPhysicalInnerIndexNLJoin::PopConvert(pop)->PdrgPcrOuterRefs();
			break;

		case COperator::EopPhysicalLeftOuterIndexNLJoin:
			join_type = EdxljtLeft;
			is_index_nlj = true;
			StoreIndexNLJOuterRefs(pop);
			outer_refs = CPhysicalLeftOuterIndexNLJoin::PopConvert(pop)->PdrgPcrOuterRefs();
			break;

		case COperator::EopPhysicalLeftOuterNLJoin:
			join_type = EdxljtLeft;
			break;

		case COperator::EopPhysicalLeftSemiNLJoin:
			join_type = EdxljtIn;
			break;

		case COperator::EopPhysicalLeftAntiSemiNLJoin:
			join_type = EdxljtLeftAntiSemijoin;
			break;

		case COperator::EopPhysicalLeftAntiSemiNLJoinNotIn:
			join_type = EdxljtLeftAntiSemijoinNotIn;
			break;

		default:
			GPOS_ASSERT(!"Invalid join type");
	}

	// translate relational child expressions
	CDXLNode *pdxlnOuterChild = CreateDXLNode(pexprOuterChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInnerChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	CDXLNode *pdxlnCond = PdxlnScalar(pexprScalar);

	CDXLNode *dxlnode_join_filter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarJoinFilter(m_mp));
	if (NULL != pdxlnCond)
	{
		dxlnode_join_filter->AddChild(pdxlnCond);
	}

	BOOL nest_params_exists = false;
	CDXLColRefArray *col_refs = NULL;
	if (is_index_nlj && GPOS_FTRACE(EopttraceIndexedNLJOuterRefAsParams))
	{
		nest_params_exists = true;
		col_refs = GPOS_NEW(m_mp) CDXLColRefArray(m_mp);
		for (ULONG ul = 0; ul < outer_refs->Size(); ul++)
		{
			CColRef *col_ref = (*outer_refs)[ul];
			CMDName *md_name = GPOS_NEW(m_mp) CMDName(m_mp, col_ref->Name().Pstr());
			IMDId *mdid = col_ref->RetrieveType()->MDId();
			mdid->AddRef();
			CDXLColRef *colref_dxl = GPOS_NEW(m_mp) CDXLColRef(m_mp, md_name, col_ref->Id(), mdid, col_ref->TypeModifier());
			col_refs->Append(colref_dxl);
		}
	}

	// construct a join node
	CDXLPhysicalNLJoin *pdxlopNLJ = GPOS_NEW(m_mp) CDXLPhysicalNLJoin(m_mp, join_type,is_index_nlj, nest_params_exists);
	pdxlopNLJ->SetNestLoopParamsColRefs(col_refs);

	// construct projection list
	// compute required columns
	GPOS_ASSERT(NULL != pexprInnerNLJ->Prpp());
	CColRefSet *pcrsOutput = pexprInnerNLJ->Prpp()->PcrsRequired();

	CDXLNode *proj_list_dxlnode = PdxlnProjList(pcrsOutput, colref_array);

	CDXLNode *pdxlnNLJ = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopNLJ);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprInnerNLJ);
	pdxlnNLJ->SetProperties(dxl_properties);

	// construct an empty plan filter
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL);

	// add children
	pdxlnNLJ->AddChild(proj_list_dxlnode);
	pdxlnNLJ->AddChild(filter_dxlnode);
	pdxlnNLJ->AddChild(dxlnode_join_filter);
	pdxlnNLJ->AddChild(pdxlnOuterChild);
	pdxlnNLJ->AddChild(pdxlnInnerChild);

#ifdef GPOS_DEBUG
	pdxlopNLJ->AssertValid(pdxlnNLJ, false /* validate_children */);
#endif

	return pdxlnNLJ;
}

CDXLNode *
CTranslatorExprToDXL::PdxlnMergeJoin
	(
	CExpression *pexprMJ,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprMJ);
	GPOS_ASSERT(3 == pexprMJ->Arity());

	// extract components
	CPhysical *pop = CPhysical::PopConvert(pexprMJ->Pop());

	CExpression *pexprOuterChild = (*pexprMJ)[0];
	CExpression *pexprInnerChild = (*pexprMJ)[1];
	CExpression *pexprScalar = (*pexprMJ)[2];

	EdxlJoinType join_type = EdxljtSentinel;
	switch (pop->Eopid())
	{
		case COperator::EopPhysicalFullMergeJoin:
			join_type = EdxljtFull;
			break;

		default:
			GPOS_ASSERT(!"Invalid join type");
	}

	// translate relational child expressions
	CDXLNode *pdxlnOuterChild = CreateDXLNode(pexprOuterChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInnerChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	CDXLNode *dxlnode_merge_conds =
		GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarMergeCondList(m_mp));

	CExpressionArray *pdrgpexprPredicates = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprScalar);
	const ULONG length = pdrgpexprPredicates->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexprPred = (*pdrgpexprPredicates)[ul];
		// At this point, they all better be merge joinable
		GPOS_ASSERT(CPhysicalJoin::FMergeJoinCompatible(pexprPred, pexprOuterChild, pexprInnerChild));
		CExpression *pexprPredOuter = (*pexprPred)[0];
		CExpression *pexprPredInner = (*pexprPred)[1];

		// align extracted columns with outer and inner children of the join
		CColRefSet *pcrsOuterChild = pexprOuterChild->DeriveOutputColumns();
		CColRefSet *pcrsPredInner = pexprPredInner->DeriveUsedColumns();
#ifdef GPOS_DEBUG
		CColRefSet *pcrsInnerChild =
			pexprInnerChild->DeriveOutputColumns();
		CColRefSet *pcrsPredOuter = pexprPredOuter->DeriveUsedColumns();
#endif

		if (pcrsOuterChild->ContainsAll(pcrsPredInner))
		{
			GPOS_ASSERT(pcrsInnerChild->ContainsAll(pcrsPredOuter));
			std::swap(pexprPredOuter, pexprPredInner);
#ifdef GPOS_DEBUG
			std::swap(pcrsPredOuter, pcrsPredInner);
#endif

			pexprPredOuter->AddRef();
			pexprPredInner->AddRef();
			pexprPred = CUtils::PexprScalarEqCmp(m_mp, pexprPredOuter, pexprPredInner);
		}
		else
		{
			pexprPred->AddRef();
		}

		GPOS_ASSERT(pcrsOuterChild->ContainsAll(pcrsPredOuter) && pcrsInnerChild->ContainsAll(pcrsPredInner) &&
					"merge join keys are not aligned with children");

		dxlnode_merge_conds->AddChild(PdxlnScalar(pexprPred));
		pexprPred->Release();
	}
	pdrgpexprPredicates->Release();

	// construct a join node
	CDXLPhysicalMergeJoin *pdxlopMJ =
		GPOS_NEW(m_mp) CDXLPhysicalMergeJoin(m_mp, join_type, false /* is_unique_outer */);

	// construct projection list
	// compute required columns
	GPOS_ASSERT(NULL != pexprMJ->Prpp());
	CColRefSet *pcrsOutput = pexprMJ->Prpp()->PcrsRequired();

	CDXLNode *proj_list_dxlnode = PdxlnProjList(pcrsOutput, colref_array);

	CDXLNode *pdxlnMJ = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopMJ);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprMJ);
	pdxlnMJ->SetProperties(dxl_properties);

	// construct an empty plan filter and join filter
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL);
	CDXLNode *dxlnode_join_filter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarJoinFilter(m_mp));

	// add children
	pdxlnMJ->AddChild(proj_list_dxlnode);
	pdxlnMJ->AddChild(filter_dxlnode);
	pdxlnMJ->AddChild(dxlnode_join_filter);
	pdxlnMJ->AddChild(dxlnode_merge_conds);
	pdxlnMJ->AddChild(pdxlnOuterChild);
	pdxlnMJ->AddChild(pdxlnInnerChild);

#ifdef GPOS_DEBUG
	pdxlnMJ->AssertValid(false /* validate_children */);
#endif

	return pdxlnMJ;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::EdxljtHashJoin
//
//	@doc:
//		Return hash join type
//---------------------------------------------------------------------------
EdxlJoinType
CTranslatorExprToDXL::EdxljtHashJoin
	(
	CPhysicalHashJoin *popHJ
	)
{
	GPOS_ASSERT(CUtils::FHashJoin(popHJ));

	switch (popHJ->Eopid())
	{
		case COperator::EopPhysicalInnerHashJoin:
			return EdxljtInner;

		case COperator::EopPhysicalLeftOuterHashJoin:
			return EdxljtLeft;

		case COperator::EopPhysicalLeftSemiHashJoin:
			return EdxljtIn;

		case COperator::EopPhysicalLeftAntiSemiHashJoin:
			return EdxljtLeftAntiSemijoin;

		case COperator::EopPhysicalLeftAntiSemiHashJoinNotIn:
			return EdxljtLeftAntiSemijoinNotIn;

		default:
			GPOS_ASSERT(!"Invalid join type");
			return EdxljtSentinel;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnHashJoin
//
//	@doc:
//		Create a DXL hash join node from an optimizer hash join expression.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnHashJoin
	(
	CExpression *pexprHJ,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprHJ);

	GPOS_ASSERT(3 == pexprHJ->Arity());

	// extract components
	CPhysicalHashJoin *popHJ = CPhysicalHashJoin::PopConvert(pexprHJ->Pop());
	CExpression *pexprOuterChild = (*pexprHJ)[0];
	CExpression *pexprInnerChild = (*pexprHJ)[1];
	CExpression *pexprScalar = (*pexprHJ)[2];

	EdxlJoinType join_type = EdxljtHashJoin(popHJ);
	GPOS_ASSERT(popHJ->PdrgpexprOuterKeys()->Size() == popHJ->PdrgpexprInnerKeys()->Size());

	// translate relational child expression
	CDXLNode *pdxlnOuterChild = CreateDXLNode(pexprOuterChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	CDXLNode *pdxlnInnerChild = CreateDXLNode(pexprInnerChild, NULL /*colref_array*/, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// construct hash condition
	CDXLNode *pdxlnHashCondList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarHashCondList(m_mp));

#ifdef GPOS_DEBUG
	ULONG ulHashJoinPreds = 0;
#endif

	CExpressionArray *pdrgpexprPredicates = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprScalar);
	CExpressionArray *pdrgpexprRemainingPredicates = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	const ULONG size = pdrgpexprPredicates->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CExpression *pexprPred = (*pdrgpexprPredicates)[ul];
		if (CPhysicalJoin::FHashJoinCompatible(pexprPred, pexprOuterChild, pexprInnerChild))
		{
			CExpression *pexprPredOuter;
			CExpression *pexprPredInner;
			IMDId *mdid_scop;
			CPhysicalJoin::AlignJoinKeyOuterInner(pexprPred, pexprOuterChild, pexprInnerChild,
												   &pexprPredOuter, &pexprPredInner, &mdid_scop);

			pexprPredOuter->AddRef();
			pexprPredInner->AddRef();
			// create hash join predicate based on conjunct type
			if (CPredicateUtils::IsEqualityOp(pexprPred))
			{
				pexprPred = CUtils::PexprScalarCmp(m_mp, pexprPredOuter, pexprPredInner, mdid_scop);
			}
			else
			{
				GPOS_ASSERT(CPredicateUtils::FINDF(pexprPred));
				pexprPred = CUtils::PexprINDF(m_mp, pexprPredOuter, pexprPredInner, mdid_scop);
			}

			CDXLNode *pdxlnPred = PdxlnScalar(pexprPred);
			pdxlnHashCondList->AddChild(pdxlnPred);
			pexprPred->Release();
#ifdef GPOS_DEBUG
			 ulHashJoinPreds++;
#endif // GPOS_DEBUG
		}
		else
		{
			pexprPred->AddRef();
			pdrgpexprRemainingPredicates->Append(pexprPred);
		}
	}
	GPOS_ASSERT(popHJ->PdrgpexprOuterKeys()->Size() == ulHashJoinPreds);

	CDXLNode *dxlnode_join_filter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarJoinFilter(m_mp));
	if (0 < pdrgpexprRemainingPredicates->Size())
	{
		CExpression *pexprJoinCond = CPredicateUtils::PexprConjunction(m_mp, pdrgpexprRemainingPredicates);
		CDXLNode *pdxlnJoinCond = PdxlnScalar(pexprJoinCond);
		dxlnode_join_filter->AddChild(pdxlnJoinCond);
		pexprJoinCond->Release();
	}
	else
	{
		pdrgpexprRemainingPredicates->Release();
	}

	// construct a hash join node
	CDXLPhysicalHashJoin *pdxlopHJ = GPOS_NEW(m_mp) CDXLPhysicalHashJoin(m_mp, join_type);

	// construct projection list from required columns
	GPOS_ASSERT(NULL != pexprHJ->Prpp());
	CColRefSet *pcrsOutput = pexprHJ->Prpp()->PcrsRequired();
	CDXLNode *proj_list_dxlnode = PdxlnProjList(pcrsOutput, colref_array);

	CDXLNode *pdxlnHJ = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopHJ);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprHJ);
	pdxlnHJ->SetProperties(dxl_properties);

	// construct an empty plan filter
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL);

	// add children
	pdxlnHJ->AddChild(proj_list_dxlnode);
	pdxlnHJ->AddChild(filter_dxlnode);
	pdxlnHJ->AddChild(dxlnode_join_filter);
	pdxlnHJ->AddChild(pdxlnHashCondList);
	pdxlnHJ->AddChild(pdxlnOuterChild);
	pdxlnHJ->AddChild(pdxlnInnerChild);

	// cleanup 
	pdrgpexprPredicates->Release();

#ifdef GPOS_DEBUG
	pdxlopHJ->AssertValid(pdxlnHJ, false /* validate_children */);
#endif

	return pdxlnHJ;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnMotion
//
//	@doc:
//		Create a DXL motion node from an optimizer motion expression
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnMotion
	(
	CExpression *pexprMotion,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprMotion);
	GPOS_ASSERT(1 == pexprMotion->Arity());

	// extract components
	CExpression *pexprChild = (*pexprMotion)[0];

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /*fRoot*/);

	// construct a motion node
	CDXLPhysicalMotion *motion = NULL;
	BOOL fDuplicateHazardMotion = CUtils::FDuplicateHazardMotion(pexprMotion);
	switch (pexprMotion->Pop()->Eopid())
	{
		case COperator::EopPhysicalMotionGather:
			motion = GPOS_NEW(m_mp) CDXLPhysicalGatherMotion(m_mp);
			break;

		case COperator::EopPhysicalMotionBroadcast:
			motion = GPOS_NEW(m_mp) CDXLPhysicalBroadcastMotion(m_mp);
			break;

		case COperator::EopPhysicalMotionHashDistribute:
			motion = GPOS_NEW(m_mp) CDXLPhysicalRedistributeMotion(m_mp, fDuplicateHazardMotion);
			break;

		case COperator::EopPhysicalMotionRandom:
			motion = GPOS_NEW(m_mp) CDXLPhysicalRandomMotion(m_mp, fDuplicateHazardMotion);
			break;

		case COperator::EopPhysicalMotionRoutedDistribute:
			{
				CPhysicalMotionRoutedDistribute *popMotion =
						CPhysicalMotionRoutedDistribute::PopConvert(pexprMotion->Pop());
				CColRef *pcrSegmentId = dynamic_cast<const CDistributionSpecRouted* >(popMotion->Pds())->Pcr();

				motion = GPOS_NEW(m_mp) CDXLPhysicalRoutedDistributeMotion(m_mp, pcrSegmentId->Id());
				break;
			}
		default:
			GPOS_ASSERT(!"Unrecognized motion type");
	}

	if (COperator::EopPhysicalMotionGather != pexprMotion->Pop()->Eopid())
	{
		(*pulNonGatherMotions)++;
	}

	GPOS_ASSERT(NULL != motion);

	// construct project list from child project list
	GPOS_ASSERT(NULL != child_dxlnode && 1 <= child_dxlnode->Arity());
	CDXLNode *pdxlnProjListChild = (*child_dxlnode)[0];

	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);

	// set input and output segment information
	motion->SetSegmentInfo(GetInputSegIdsArray(pexprMotion), GetOutputSegIdsArray(pexprMotion));

	CDXLNode *pdxlnMotion = GPOS_NEW(m_mp) CDXLNode(m_mp, motion);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprMotion);
	pdxlnMotion->SetProperties(dxl_properties);

	// construct an empty filter node
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL /*pdxlnCond*/);

	// construct sort column list
	CDXLNode *sort_col_list_dxlnode = GetSortColListDXL(pexprMotion);

	// add children
	pdxlnMotion->AddChild(proj_list_dxlnode);
	pdxlnMotion->AddChild(filter_dxlnode);
	pdxlnMotion->AddChild(sort_col_list_dxlnode);

	if (COperator::EopPhysicalMotionHashDistribute == pexprMotion->Pop()->Eopid())
	{
		// construct a hash expr list node
		CPhysicalMotionHashDistribute *popHashDistribute = CPhysicalMotionHashDistribute::PopConvert(pexprMotion->Pop());
		CDistributionSpecHashed *pdsHashed = CDistributionSpecHashed::PdsConvert(popHashDistribute->Pds());
		CDXLNode *hash_expr_list = PdxlnHashExprList(pdsHashed->Pdrgpexpr(), pdsHashed->Opfamilies());
		pdxlnMotion->AddChild(hash_expr_list);
	}

	pdxlnMotion->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	motion->AssertValid(pdxlnMotion, false /* validate_children */);
#endif

	return pdxlnMotion;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnMaterialize
//
//	@doc:
//		Create a DXL materialize node from an optimizer spool expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnMaterialize
	(
	CExpression *pexprSpool,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprSpool);

	GPOS_ASSERT(1 == pexprSpool->Arity());

	// extract components
	CExpression *pexprChild = (*pexprSpool)[0];

	// translate relational child expression
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	CPhysicalSpool *spool = CPhysicalSpool::PopConvert(pexprSpool->Pop());

	// construct a materialize node
	CDXLPhysicalMaterialize *pdxlopMat = GPOS_NEW(m_mp) CDXLPhysicalMaterialize(m_mp, spool->FEager());

	// construct project list from child project list
	GPOS_ASSERT(NULL != child_dxlnode && 1 <= child_dxlnode->Arity());
	CDXLNode *pdxlnProjListChild = (*child_dxlnode)[0];
	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);

	CDXLNode *pdxlnMaterialize = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopMat);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprSpool);
	pdxlnMaterialize->SetProperties(dxl_properties);

	// construct an empty filter node
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL /* pdxlnCond */);

	// add children
	pdxlnMaterialize->AddChild(proj_list_dxlnode);
	pdxlnMaterialize->AddChild(filter_dxlnode);
	pdxlnMaterialize->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlopMat->AssertValid(pdxlnMaterialize, false /* validate_children */);
#endif

	return pdxlnMaterialize;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnSequence
//
//	@doc:
//		Create a DXL sequence node from an optimizer sequence expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnSequence
	(
	CExpression *pexprSequence,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprSequence);

	const ULONG arity = pexprSequence->Arity();
	GPOS_ASSERT(0 < arity);

	// construct sequence node
	CDXLPhysicalSequence *pdxlopSequence = GPOS_NEW(m_mp) CDXLPhysicalSequence(m_mp);
	CDXLNode *pdxlnSequence = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSequence);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprSequence);
	pdxlnSequence->SetProperties(dxl_properties);

	// translate children
	CDXLNodeArray *pdrgpdxlnChildren = GPOS_NEW(m_mp) CDXLNodeArray(m_mp);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexprSequence)[ul];

		CColRefArray *pdrgpcrChildOutput = NULL;
		if (ul == arity - 1)
		{
			// impose output columns on last child
			pdrgpcrChildOutput = colref_array;
		}

		CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, pdrgpcrChildOutput, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
		pdrgpdxlnChildren->Append(child_dxlnode);
	}

	// construct project list from the project list of the last child
	CDXLNode *pdxlnLastChild = (*pdrgpdxlnChildren)[arity - 1];
	CDXLNode *pdxlnProjListChild = (*pdxlnLastChild)[0];

	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);
	pdxlnSequence->AddChild(proj_list_dxlnode);

	// add children
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnChid = (*pdrgpdxlnChildren)[ul];
		pdxlnChid->AddRef();
		pdxlnSequence->AddChild(pdxlnChid);
	}

	pdrgpdxlnChildren->Release();

#ifdef GPOS_DEBUG
	pdxlopSequence->AssertValid(pdxlnSequence, false /* validate_children */);
#endif

	return pdxlnSequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelector
//
//	@doc:
//		Translate a partition selector into DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelector
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	return PdxlnPartitionSelector(pexpr, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, NULL /*pexprScalarCond*/, NULL /*dxl_properties*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelector
//
//	@doc:
//		Translate a partition selector into DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelector
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexpr->Pop());

	CExpression *pexprScalar = popSelector->PexprCombinedPred();
	if (CUtils::FScalarConstTrue(pexprScalar))
	{
		return PdxlnPartitionSelectorExpand(pexpr, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, pexprScalarCond, dxl_properties);
	}

	return PdxlnPartitionSelectorFilter(pexpr, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, pexprScalarCond, dxl_properties);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelectorDML
//
//	@doc:
//		Translate a DML partition selector into DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelectorDML
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	CExpression *pexprChild = (*pexpr)[0];
	CPhysicalPartitionSelectorDML *popSelector = CPhysicalPartitionSelectorDML::PopConvert(pexpr->Pop());

	// translate child
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	// construct project list
	IMDId *mdid = popSelector->MDId();
	GPOS_ASSERT(1 <= child_dxlnode->Arity());
	CDXLNode *pdxlnPrL = CTranslatorExprToDXLUtils::PdxlnPrLPartitionSelector
							(
							m_mp,
							m_pmda,
							m_pcf,
							m_phmcrdxln,
							true, //fUseChildProjList
							(*child_dxlnode)[0],
							popSelector->PcrOid(),
							popSelector->UlPartLevels(),
							CUtils::FGeneratePartOid(mdid)
							);

	// translate filters
	CDXLNode *pdxlnEqFilters = NULL;
	CDXLNode *pdxlnFilters = NULL;
	CDXLNode *pdxlnResidual = NULL;
	TranslatePartitionFilters(pexpr, true /*fPassThrough*/, &pdxlnEqFilters, &pdxlnFilters, &pdxlnResidual);

	// since there is no propagation for DML, we create a const null expression
	const IMDTypeInt4 *pmdtypeint4 = m_pmda->PtMDType<IMDTypeInt4>();
	CDXLDatum *pdxldatumNull = pmdtypeint4->GetDXLDatumNull(m_mp);
	CDXLNode *pdxlnPropagation = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarConstValue(m_mp, pdxldatumNull));

	// true printable filter
	CDXLNode *pdxlnPrintable = CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/);

	// construct PartitionSelector node
	IMDId *rel_mdid = popSelector->MDId();
	rel_mdid->AddRef();

	CDXLNode *pdxlnSelector = CTranslatorExprToDXLUtils::PdxlnPartitionSelector
									(
									m_mp,
									rel_mdid,
									popSelector->UlPartLevels(),
									0, // scan_id
									GetProperties(pexpr),
									pdxlnPrL,
									pdxlnEqFilters,
									pdxlnFilters,
									pdxlnResidual,
									pdxlnPropagation,
									pdxlnPrintable,
									child_dxlnode
									);

#ifdef GPOS_DEBUG
	pdxlnSelector->GetOperator()->AssertValid(pdxlnSelector, false /* validate_children */);
#endif

	return pdxlnSelector;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartFilterList
//
//	@doc:
//		Return a DXL part filter list. Can be used for the equality filters or
//		the general filters
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartFilterList
	(
	CExpression *pexpr,
	BOOL fEqFilters
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexpr->Pop());

	CDXLNode *pdxlnFilters = NULL;
	if (fEqFilters)
	{
		pdxlnFilters = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOpList(m_mp, CDXLScalarOpList::EdxloplistEqFilterList));
	}
	else
	{
		pdxlnFilters = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOpList(m_mp, CDXLScalarOpList::EdxloplistFilterList));
	}

	const ULONG ulPartLevels = popSelector->UlPartLevels();
	GPOS_ASSERT(1 <= ulPartLevels);

	for (ULONG ul = 0; ul < ulPartLevels; ul++)
	{
		CExpression *pexprFilter = NULL;
		if (fEqFilters)
		{
			pexprFilter = popSelector->PexprEqFilter(ul);
		}
		else
		{
			pexprFilter = popSelector->PexprFilter(ul);
		}

		CDXLNode *filter_dxlnode = NULL;
		if (NULL == pexprFilter)
		{
			filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/);
		}
		else
		{
			filter_dxlnode = PdxlnScalar(pexprFilter);
		}
		pdxlnFilters->AddChild(filter_dxlnode);
	}

	return pdxlnFilters;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelectorExpand
//
//	@doc:
//		Translate an expand-based partition resolver into DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelectorExpand
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	CExpression *pexprChild = (*pexpr)[0];

	GPOS_ASSERT_IMP
		(
		NULL != pexprScalarCond,
		(COperator::EopPhysicalDynamicTableScan == pexprChild->Pop()->Eopid() ||
		 COperator::EopPhysicalDynamicIndexScan == pexprChild->Pop()->Eopid() ||
		 COperator::EopPhysicalDynamicBitmapTableScan == pexprChild->Pop()->Eopid())
		&& "Inlining predicates only allowed in DynamicTableScan, DynamicIndexScan and DynamicBitmapTableScan"
		);

	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexpr->Pop());
	const ULONG ulLevels = popSelector->UlPartLevels();

	// translate child
	CDXLNode *child_dxlnode = PdxlnPartitionSelectorChild(pexprChild, pexprScalarCond, dxl_properties, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);

	// project list
	IMDId *mdid = popSelector->MDId();
	const IMDRelation *pmdrel = (IMDRelation *) m_pmda->RetrieveRel(mdid);
	CDXLNode *pdxlnPrL = CTranslatorExprToDXLUtils::PdxlnPrLPartitionSelector
							(
							m_mp,
							m_pmda,
							m_pcf,
							m_phmcrdxln,
							false, //fUseChildProjList
							NULL, //pdxlnPrLchild
							NULL, //pcrOid
							ulLevels,
							CUtils::FGeneratePartOid(mdid)
							);

	// translate filters
	CDXLNode *pdxlnEqFilters = NULL;
	CDXLNode *pdxlnFilters = NULL;
	CDXLNode *pdxlnResidual = NULL;
	TranslatePartitionFilters(pexpr, true /*fPassThrough*/, &pdxlnEqFilters, &pdxlnFilters, &pdxlnResidual);

	// construct propagation expression
	CPartIndexMap *ppimDrvd = m_pdpplan->Ppim();
	ULONG scan_id = popSelector->ScanId();
	CDXLNode *pdxlnPropagation = CTranslatorExprToDXLUtils::PdxlnPropExprPartitionSelector
									(
									m_mp,
									m_pmda,
									m_pcf,
									ppimDrvd->FPartialScans(scan_id),
									ppimDrvd->Ppartcnstrmap(scan_id),
									popSelector->Pdrgpdrgpcr(),
									scan_id,
									pmdrel->GetPartitionTypes()
									);

	// translate printable filter
	CExpression *pexprPrintable = popSelector->PexprCombinedPred();
	GPOS_ASSERT(NULL != pexprPrintable);
	CDXLNode *pdxlnPrintable = PdxlnScalar(pexprPrintable);

	// construct PartitionSelector node
	IMDId *rel_mdid = popSelector->MDId();
	rel_mdid->AddRef();

	CDXLNode *pdxlnSelector = CTranslatorExprToDXLUtils::PdxlnPartitionSelector
									(
									m_mp,
									rel_mdid,
									ulLevels,
									scan_id,
									CTranslatorExprToDXLUtils::GetProperties(m_mp),
									pdxlnPrL,
									pdxlnEqFilters,
									pdxlnFilters,
									pdxlnResidual,
									pdxlnPropagation,
									pdxlnPrintable
									);

	// construct sequence node
	CDXLPhysicalSequence *pdxlopSequence = GPOS_NEW(m_mp) CDXLPhysicalSequence(m_mp);
	CDXLNode *pdxlnSequence = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSequence);

	CDXLPhysicalProperties *pdxlpropSeq = CTranslatorExprToDXLUtils::PdxlpropCopy(m_mp, child_dxlnode);
	pdxlnSequence->SetProperties(pdxlpropSeq);

	// construct sequence's project list from the project list of the last child
	CDXLNode *pdxlnPrLChild = (*child_dxlnode)[0];
	CDXLNode *pdxlnPrLSequence = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnPrLChild);

	pdxlnSequence->AddChild(pdxlnPrLSequence);
	pdxlnSequence->AddChild(pdxlnSelector);
	pdxlnSequence->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlopSequence->AssertValid(pdxlnSequence, false /* validate_children */);
#endif

	return pdxlnSequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelectorFilter
//
//	@doc:
//		Translate a filter-based partition selector into DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelectorFilter
	(
	CExpression *pexpr,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML,
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexpr->Pop());
	CPartIndexMap *ppimDrvd = m_pdpplan->Ppim();
	ULONG scan_id = popSelector->ScanId();
	ULONG ulLevels = popSelector->UlPartLevels();
	BOOL fPartialScans = ppimDrvd->FPartialScans(scan_id);
	UlongToPartConstraintMap *ppartcnstrmap = ppimDrvd->Ppartcnstrmap(scan_id);

	BOOL fPassThrough = FEqPartFiltersAllLevels(pexpr, false /*fCheckGeneralFilters*/) && !fPartialScans;
#ifdef GPOS_DEBUG
	BOOL fPoint = FEqPartFiltersAllLevels(pexpr, true /*fCheckGeneralFilters*/) && !fPartialScans;
	GPOS_ASSERT_IMP(!fPoint && fPartialScans, NULL != ppartcnstrmap);
#endif

	CExpression *pexprChild = (*pexpr)[0];

	GPOS_ASSERT_IMP
		(
		NULL != pexprScalarCond,
		(COperator::EopPhysicalDynamicTableScan == pexprChild->Pop()->Eopid() ||
		 COperator::EopPhysicalDynamicIndexScan == pexprChild->Pop()->Eopid() ||
		 COperator::EopPhysicalDynamicBitmapTableScan == pexprChild->Pop()->Eopid())
		&& "Inlining predicates only allowed in DynamicTableScan, DynamicIndexScan and DynamicBitmapTableScan"
		);

	// translate child
	CDXLNode *child_dxlnode = PdxlnPartitionSelectorChild(pexprChild, pexprScalarCond, dxl_properties, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
	CDXLNode *pdxlnPrLChild = (*child_dxlnode)[0];

	// we add a sequence if the scan id is found below the resolver
	BOOL fNeedSequence = pexprChild->DerivePartitionInfo()->FContainsScanId(popSelector->ScanId());

	// project list
	IMDId *mdid = popSelector->MDId();
	const IMDRelation *pmdrel = (IMDRelation *) m_pmda->RetrieveRel(mdid);
	CDXLNode *pdxlnPrL = CTranslatorExprToDXLUtils::PdxlnPrLPartitionSelector
							(
							m_mp,
							m_pmda,
							m_pcf,
							m_phmcrdxln,
							!fNeedSequence,
							pdxlnPrLChild,
							NULL /*pcrOid*/,
							ulLevels,
							CUtils::FGeneratePartOid(mdid)
							);

	// translate filters
	CDXLNode *pdxlnEqFilters = NULL;
	CDXLNode *pdxlnFilters = NULL;
	CDXLNode *pdxlnResidual = NULL;
	TranslatePartitionFilters(pexpr, fPassThrough, &pdxlnEqFilters, &pdxlnFilters, &pdxlnResidual);

	// construct propagation expression
	CDXLNode *pdxlnPropagation = CTranslatorExprToDXLUtils::PdxlnPropExprPartitionSelector
									(
									m_mp,
									m_pmda,
									m_pcf,
									!fPassThrough && fPartialScans, //fConditional
									ppartcnstrmap,
									popSelector->Pdrgpdrgpcr(),
									popSelector->ScanId(),
									pmdrel->GetPartitionTypes()
									);

	// translate printable filter
	CExpression *pexprPrintable = popSelector->PexprCombinedPred();
	GPOS_ASSERT(NULL != pexprPrintable);
	CDXLNode *pdxlnPrintable = PdxlnScalar(pexprPrintable);

	// construct PartitionSelector node
	IMDId *rel_mdid = popSelector->MDId();
	rel_mdid->AddRef();

	CDXLNode *pdxlnSelectorChild = NULL;
	if (!fNeedSequence)
	{
		pdxlnSelectorChild = child_dxlnode;
	}

	CDXLNode *pdxlnSelector = CTranslatorExprToDXLUtils::PdxlnPartitionSelector
									(
									m_mp,
									rel_mdid,
									ulLevels,
									scan_id,
									CTranslatorExprToDXLUtils::GetProperties(m_mp),
									pdxlnPrL,
									pdxlnEqFilters,
									pdxlnFilters,
									pdxlnResidual,
									pdxlnPropagation,
									pdxlnPrintable,
									pdxlnSelectorChild
									);

	CDXLNode *pdxlnReturned = pdxlnSelector;
	if (fNeedSequence)
	{
		CDXLPhysicalSequence *pdxlopSequence = GPOS_NEW(m_mp) CDXLPhysicalSequence(m_mp);
		CDXLNode *pdxlnSequence = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSequence);
		CDXLPhysicalProperties *pdxlpropSeq = CTranslatorExprToDXLUtils::PdxlpropCopy(m_mp, child_dxlnode);
		pdxlnSequence->SetProperties(pdxlpropSeq);

		// construct sequence's project list from the project list of the last child
		CDXLNode *pdxlnPrLSequence = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnPrLChild);

		pdxlnSequence->AddChild(pdxlnPrLSequence);
		pdxlnSequence->AddChild(pdxlnSelector);
		pdxlnSequence->AddChild(child_dxlnode);

		pdxlnReturned = pdxlnSequence;
	}

#ifdef GPOS_DEBUG
	pdxlnReturned->GetOperator()->AssertValid(pdxlnReturned, false /* validate_children */);
#endif

	return pdxlnReturned;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::FEqPartFiltersAllLevels
//
//	@doc:
//		Check whether the given partition selector only has equality filters
//		or no filters on all partitioning levels. Return false if it has
//		non-equality filters. If fCheckGeneralFilters is true then the function
//		checks whether the content of general filter is conjunction of equality
//		filter or not. If it is false, we always view the general filter as
//		non-equality filter if the pexprFilter is not null.
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXL::FEqPartFiltersAllLevels
	(
	CExpression *pexpr,
	BOOL fCheckGeneralFilters
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexpr->Pop());
	const ULONG ulPartLevels = popSelector->UlPartLevels();
	GPOS_ASSERT(1 <= ulPartLevels);

	for (ULONG ul = 0; ul < ulPartLevels; ul++)
	{
		CExpression *pexprEqFilter = popSelector->PexprEqFilter(ul);
		CExpression *pexprFilter = popSelector->PexprFilter(ul);

		if (NULL == pexprEqFilter && NULL != pexprFilter)
		{
			if (!fCheckGeneralFilters || !CPredicateUtils::FConjunctionOfEqComparisons(m_mp, pexprFilter))
			{
				return false;
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::TranslatePartitionFilters
//
//	@doc:
//		Translate partition selector filters
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::TranslatePartitionFilters
	(
	CExpression *pexprPartSelector,
	BOOL fPassThrough,
	CDXLNode **ppdxlnEqFilters,		// output: translated equality filters
	CDXLNode **ppdxlnFilters,		// output: translated non-equality filters
	CDXLNode **ppdxlnResidual		// output: translated residual filters
	)
{
	GPOS_ASSERT(NULL != pexprPartSelector);

	if (fPassThrough)
	{
		*ppdxlnEqFilters = PdxlnPartFilterList(pexprPartSelector, true /*fEqFilters*/);
		*ppdxlnFilters = PdxlnPartFilterList(pexprPartSelector, false /*fEqFilters*/);

		CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexprPartSelector->Pop());
		CExpression *pexprResidual = popSelector->PexprResidualPred();
		if (NULL == pexprResidual)
		{
			*ppdxlnResidual = CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/);
		}
		else
		{
			*ppdxlnResidual = PdxlnScalar(pexprResidual);
		}

		return;
	}

	ConstructLevelFilters4PartitionSelector(pexprPartSelector, ppdxlnEqFilters, ppdxlnFilters);

	// TODO:  - Apr 11, 2014; translate the residual filter. Take into account
	// that this might be an arbitrary scalar expression on multiple part keys. Right
	// now we assume no residual filter in this case
	*ppdxlnResidual = CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::ConstructLevelFilters4PartitionSelector
//
//	@doc:
// 		Construct the level filter lists for partition selector
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::ConstructLevelFilters4PartitionSelector
	(
	CExpression *pexprPartSelector,
	CDXLNode **ppdxlnEqFilters,
	CDXLNode **ppdxlnFilters
	)
{
	GPOS_ASSERT(NULL != pexprPartSelector);
	CPhysicalPartitionSelector *popSelector = CPhysicalPartitionSelector::PopConvert(pexprPartSelector->Pop());
	const IMDRelation *pmdrel = (IMDRelation *) m_pmda->RetrieveRel(popSelector->MDId());

	const ULONG ulPartLevels = popSelector->UlPartLevels();
	GPOS_ASSERT(1 <= ulPartLevels);

	CColRef2dArray *pdrgpdrgpcrPartKeys = popSelector->Pdrgpdrgpcr();
	CBitSet *pbsDefaultParts = NULL;
	IMDPartConstraint *mdpart_constraint = m_pmda->RetrieveRel(popSelector->MDId())->MDPartConstraint();
	if (NULL != mdpart_constraint)
		pbsDefaultParts = CUtils::Pbs(m_mp, mdpart_constraint->GetDefaultPartsArray());

	*ppdxlnFilters = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOpList(m_mp, CDXLScalarOpList::EdxloplistFilterList));
	*ppdxlnEqFilters = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOpList(m_mp, CDXLScalarOpList::EdxloplistEqFilterList));

	for (ULONG ulLevel = 0; ulLevel < ulPartLevels; ulLevel++)
	{
		CColRef *pcrPartKey = CUtils::PcrExtractPartKey(pdrgpdrgpcrPartKeys, ulLevel);
		IMDId *pmdidTypePartKey = pcrPartKey->RetrieveType()->MDId();
		CHAR szPartType = pmdrel->PartTypeAtLevel(ulLevel);
		BOOL fRangePart = IMDRelation::ErelpartitionRange == szPartType;

		CDXLNode *filter_dxlnode = NULL;
		BOOL fDefaultPartition = pbsDefaultParts ? pbsDefaultParts->Get(ulLevel) : false;

		BOOL fLTComparison = false;
		BOOL fGTComparison = false;
		BOOL fEQComparison = false;

		// check if there is an equality filter on current level
		CExpression *pexprEqFilter = popSelector->PexprEqFilter(ulLevel);
		if (NULL != pexprEqFilter)
		{
			CDXLNode *pdxlnEq = PdxlnScalar(pexprEqFilter);
			IMDId *pmdidTypeOther = CScalar::PopConvert(pexprEqFilter->Pop())->MdidType();
			fEQComparison = true;

			if (fRangePart)
			{
				filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnRangeFilterEqCmp
								(
								m_mp,
								m_pmda,
								pdxlnEq,
								pmdidTypePartKey,
								pmdidTypeOther,
								NULL /*pmdidTypeCastExpr*/,
								NULL /*mdid_cast_func*/,
								ulLevel
								);
			}
			else // list partition
			{
				CExpression *pexprIdent = CUtils::PexprScalarIdent(m_mp, pcrPartKey);
				// Create a ScalarIdent expression from the partition key
				CDXLNode *pdxlnPartKey = CTranslatorExprToDXLUtils::PdxlnListFilterPartKey
															(
															m_mp,
															m_pmda,
															pexprIdent,
															pmdidTypePartKey,
															ulLevel
															);
				pexprIdent->Release();
				filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnListFilterScCmp
								(
								m_mp,
								m_pmda,
								pdxlnPartKey,
								pdxlnEq,
								pmdidTypePartKey,
								pmdidTypeOther,
								IMDType::EcmptEq,
								ulLevel,
								fDefaultPartition
								);
			}
		}

		// check general filters on current level
		CExpression *pexprFilter = popSelector->PexprFilter(ulLevel);
		if (NULL != pexprFilter)
		{
			CExpressionArray *pdrgpexprConjuncts = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprFilter);
			const ULONG length = pdrgpexprConjuncts->Size();

			for (ULONG ul = 0; ul < length; ul++)
			{
				CDXLNode *pdxlnScCmp = PdxlnPredOnPartKey
										(
										(*pdrgpexprConjuncts)[ul],
										pcrPartKey,
										pmdidTypePartKey,
										ulLevel,
										fRangePart,
										&fLTComparison,
										&fGTComparison,
										&fEQComparison
										);

				filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnCombineBoolean(m_mp, filter_dxlnode, pdxlnScCmp, Edxland);
			}

			pdrgpexprConjuncts->Release();
		}

		if (NULL != filter_dxlnode && fRangePart)
		{
			CDXLNode *pdxlnDefaultAndOpenEnded = CTranslatorExprToDXLUtils::PdxlnRangeFilterDefaultAndOpenEnded
										(
										m_mp,
										ulLevel,
										fLTComparison,
										fGTComparison,
										fEQComparison,
										fDefaultPartition
										);

			filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnCombineBoolean(m_mp, filter_dxlnode, pdxlnDefaultAndOpenEnded, Edxlor);
		}

		if (NULL == filter_dxlnode)
		{
			filter_dxlnode = CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/);
		}

		(*ppdxlnFilters)->AddChild(filter_dxlnode);
		(*ppdxlnEqFilters)->AddChild(CTranslatorExprToDXLUtils::PdxlnBoolConst(m_mp, m_pmda, true /*value*/));
	}

	if (NULL != pbsDefaultParts)
	{
		pbsDefaultParts->Release();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPredOnPartKey
//
//	@doc:
// 		Translate a general predicate on a part key and update the various
//		comparison type flags accordingly
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPredOnPartKey
	(
	CExpression *pexprPred,
	CColRef *pcrPartKey,
	IMDId *pmdidTypePartKey,
	ULONG ulPartLevel,
	BOOL fRangePart,
	BOOL *pfLTComparison,	// input/output
	BOOL *pfGTComparison,	// input/output
	BOOL *pfEQComparison	// input/output
	)
{
	if (CPredicateUtils::FComparison(pexprPred))
	{
		return PdxlnScCmpPartKey(pexprPred, pcrPartKey, pmdidTypePartKey, ulPartLevel, fRangePart, pfLTComparison, pfGTComparison, pfEQComparison);
	}

	if (CUtils::FScalarNullTest(pexprPred))
	{
#ifdef GPOS_DEBUG
		CExpression *pexprChild = (*pexprPred)[0];
		GPOS_ASSERT(CUtils::FScalarIdent(pexprChild, pcrPartKey) || CCastUtils::FBinaryCoercibleCastedScId(pexprChild, pcrPartKey));
#endif //GPOS_DEBUG

		return PdxlnScNullTestPartKey(pmdidTypePartKey, ulPartLevel, fRangePart, true /*is_null*/);
	}

	if (CUtils::FScalarNotNull(pexprPred))
	{
#ifdef GPOS_DEBUG
		CExpression *pexprIsNull = (*pexprPred)[0];
		CExpression *pexprChild = (*pexprIsNull)[0];
		GPOS_ASSERT(CUtils::FScalarIdent(pexprChild, pcrPartKey) || CCastUtils::FBinaryCoercibleCastedScId(pexprChild, pcrPartKey));
#endif //GPOS_DEBUG

		*pfEQComparison = true;
		return PdxlnScNullTestPartKey(pmdidTypePartKey, ulPartLevel, fRangePart, false /*is_null*/);
	}

	if (CPredicateUtils::FCompareIdentToConstArray(pexprPred))
	{
		return PdxlArrayExprOnPartKey(pexprPred, pcrPartKey, pmdidTypePartKey, ulPartLevel, fRangePart, pfLTComparison, pfGTComparison, pfEQComparison);
	}

	GPOS_ASSERT(CPredicateUtils::FOr(pexprPred) || CPredicateUtils::FAnd(pexprPred));

	return PdxlnConjDisjOnPartKey(pexprPred, pcrPartKey, pmdidTypePartKey, ulPartLevel, fRangePart, pfLTComparison, pfGTComparison, pfEQComparison);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlArrayInOnPartKey
//
//	@doc:
//		Translates an array expression on a partition key to a disjunction because
//		the DXL partition translator requires expressions containing only LT, GT,
//		or EQ comparisons
//		For example the expression:
//			X IN (1,2,3) cannot be translated
//		but when converted into a constraint and then converted into a disjunction
//			X = 1 OR x = 2 OR x = 3
//		it can be converted to DXL
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlArrayExprOnPartKey
	(
	CExpression *pexprPred,
	CColRef *pcrPartKey,
	IMDId *pmdidTypePartKey,
	ULONG ulPartLevel,
	BOOL fRangePart,
	BOOL *pfLTComparison,	// input/output
	BOOL *pfGTComparison,	// input/output
	BOOL *pfEQComparison	// input/output
	)
{
	GPOS_ASSERT(CUtils::FScalarArrayCmp(pexprPred));

	CConstraintInterval* pci = CConstraintInterval::PcnstrIntervalFromScalarArrayCmp(m_mp, pexprPred, pcrPartKey);
	GPOS_ASSERT(NULL != pci);

	// convert the interval into a disjunction
	// (do not use CScalarArrayCmp::PexprExpand, it will use non-range
	// comparators which cannot be translated to a partition filter)
	CExpression *pexprDisj = pci->PexprConstructDisjunctionScalar(m_mp);

	CDXLNode* dxlnode = PdxlnConjDisjOnPartKey(pexprDisj, pcrPartKey, pmdidTypePartKey, ulPartLevel, fRangePart, pfLTComparison, pfGTComparison, pfEQComparison);
	pexprDisj->Release();
	pci->Release();


	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnConjDisjOnPartKey
//
//	@doc:
// 		Translate a conjunctive or disjunctive predicate on a part key and update
//		the various comparison type flags accordingly
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnConjDisjOnPartKey
	(
	CExpression *pexprPred,
	CColRef *pcrPartKey,
	IMDId *pmdidTypePartKey,
	ULONG ulPartLevel,
	BOOL fRangePart,
	BOOL *pfLTComparison,	// input/output
	BOOL *pfGTComparison,	// input/output
	BOOL *pfEQComparison	// input/output
	)
{
	GPOS_ASSERT(CPredicateUtils::FOr(pexprPred) || CPredicateUtils::FAnd(pexprPred));

	CExpressionArray *pdrgpexprChildren = NULL;
	EdxlBoolExprType edxlbet = Edxland;
	if (CPredicateUtils::FAnd(pexprPred))
	{
		pdrgpexprChildren = CPredicateUtils::PdrgpexprConjuncts(m_mp, pexprPred);
	}
	else
	{
		pdrgpexprChildren = CPredicateUtils::PdrgpexprDisjuncts(m_mp, pexprPred);
		edxlbet = Edxlor;
	}

	const ULONG ulChildren = pdrgpexprChildren->Size();

	CDXLNode *pdxlnPred = NULL;
	for (ULONG ul = 0; ul < ulChildren; ul++)
	{
		CExpression *pexprChild = (*pdrgpexprChildren)[ul];
		CDXLNode *child_dxlnode = PdxlnPredOnPartKey(pexprChild, pcrPartKey, pmdidTypePartKey, ulPartLevel, fRangePart, pfLTComparison, pfGTComparison, pfEQComparison);

		if (NULL == pdxlnPred)
		{
			pdxlnPred = child_dxlnode;
		}
		else
		{
			pdxlnPred = CTranslatorExprToDXLUtils::PdxlnCombineBoolean(m_mp, pdxlnPred, child_dxlnode, edxlbet);
		}
	}

	pdrgpexprChildren->Release();

	return pdxlnPred;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCmpPartKey
//
//	@doc:
// 		Translate a scalar comparison on a part key and update the various
//		comparison type flags accordingly
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCmpPartKey
	(
	CExpression *pexprScCmp,
	CColRef *pcrPartKey,
	IMDId *pmdidTypePartKey,
	ULONG ulPartLevel,
	BOOL fRangePart,
	BOOL *pfLTComparison,	// input/output
	BOOL *pfGTComparison,	// input/output
	BOOL *pfEQComparison	// input/output
	)
{
	GPOS_ASSERT(CPredicateUtils::FComparison(pexprScCmp));

	// extract components
	CExpression *pexprPartKey = NULL;
	CExpression *pexprOther = NULL;
	IMDType::ECmpType cmp_type = IMDType::EcmptOther;

	// Make sure that pexprNewPartKey is either ScalarIdent or Cast(ident) or allowedFunc(ident)
	// and extract required components
	CPredicateUtils::ExtractComponents(pexprScCmp, pcrPartKey, &pexprPartKey, &pexprOther, &cmp_type);

	*pfLTComparison = *pfLTComparison || (IMDType::EcmptL == cmp_type) || (IMDType::EcmptLEq == cmp_type);
	*pfGTComparison = *pfGTComparison || (IMDType::EcmptG == cmp_type) || (IMDType::EcmptGEq == cmp_type);
	*pfEQComparison = *pfEQComparison || IMDType::EcmptEq == cmp_type;

	GPOS_ASSERT(NULL != pexprPartKey && NULL != pexprOther);
	GPOS_ASSERT(IMDType::EcmptOther != cmp_type);

	CDXLNode *pdxlnOther = PdxlnScalar(pexprOther);
	IMDId *pmdidTypeOther = CScalar::PopConvert(pexprOther->Pop())->MdidType();
	IMDId *pmdidTypeCastExpr = NULL;
	IMDId *mdid_cast_func = NULL;

	if (fRangePart) // range partition
	{
		CExpression *pexprNewPartKey = pexprPartKey;

		// If the pexprPartKey is not comparable with pexprOther, but can be casted to pexprOther,
		// and not yet casted, then we add a cast on top of pexprPartKey.
		if (!CMDAccessorUtils::FCmpExists(m_pmda, pmdidTypePartKey, pmdidTypeOther, cmp_type)
			&& CMDAccessorUtils::FCastExists(m_pmda, pmdidTypePartKey, pmdidTypeOther)
			&& COperator::EopScalarCast != pexprPartKey->Pop()->Eopid())
		{
			pexprNewPartKey = CUtils::PexprCast(m_mp, m_pmda, pexprPartKey, pmdidTypeOther);
			pexprPartKey->Release();
		}

		//Extract cast/func details
		CTranslatorExprToDXLUtils::ExtractCastFuncMdids(pexprNewPartKey->Pop(), &pmdidTypeCastExpr, &mdid_cast_func);

		return CTranslatorExprToDXLUtils::PdxlnRangeFilterScCmp
								(
								m_mp,
								m_pmda,
								pdxlnOther,
								pmdidTypePartKey,
								pmdidTypeOther,
								pmdidTypeCastExpr,
								mdid_cast_func,
								cmp_type,
								ulPartLevel
								);
	}
	else // list partition
	{
		cmp_type = CPredicateUtils::EcmptReverse(cmp_type);
		IMDId *pmdidTypePartKeyExpr = CScalar::PopConvert(pexprPartKey->Pop())->MdidType();

		CDXLNode *pdxlnPartKeyExpr = CTranslatorExprToDXLUtils::PdxlnListFilterPartKey
																(
																m_mp,
																m_pmda,
																pexprPartKey,
																pmdidTypePartKeyExpr,
																ulPartLevel
																);

		return CTranslatorExprToDXLUtils::PdxlnListFilterScCmp
								(
								m_mp,
								m_pmda,
								pdxlnPartKeyExpr,
								pdxlnOther,
								pmdidTypePartKeyExpr,
								pmdidTypeOther,
								cmp_type,
								ulPartLevel,
								true
								);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScNullTestPartKey
//
//	@doc:
// 		Translate a scalar null test on a part key
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScNullTestPartKey
	(
	IMDId *pmdidTypePartKey,
	ULONG ulPartLevel,
	BOOL fRangePart,
	BOOL is_null
	)
{
	if (!fRangePart) // list partition
	{
		CDXLNode *pdxlnPartListNullTest = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarPartListNullTest(m_mp, ulPartLevel, is_null));
		CDXLNode *pdxlnDefault = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarPartDefault(m_mp, ulPartLevel));
		return GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, Edxlor), pdxlnPartListNullTest, pdxlnDefault);
	}

	pmdidTypePartKey->AddRef();
	CDXLNode *pdxlnPredicateMin = GPOS_NEW(m_mp) CDXLNode
							(
							m_mp,
							GPOS_NEW(m_mp) CDXLScalarNullTest(m_mp, is_null),
							GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarPartBound(m_mp, ulPartLevel, pmdidTypePartKey, true /*is_lower_bound*/))
							);

	pmdidTypePartKey->AddRef();
	CDXLNode *pdxlnPredicateMax = GPOS_NEW(m_mp) CDXLNode
							(
							m_mp,
							GPOS_NEW(m_mp) CDXLScalarNullTest(m_mp, is_null),
							GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarPartBound(m_mp, ulPartLevel, pmdidTypePartKey, false /*is_lower_bound*/))
							);

	// construct the conjunction of the predicate for the lower and upper bounds
	CDXLNode *pdxlnNullTests = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, Edxland), pdxlnPredicateMin, pdxlnPredicateMax);

	// AND that with the following: !(default || min_open || max_open)
	CDXLNode *pdxlnDefaultOrOpenEnded = CTranslatorExprToDXLUtils::PdxlnRangeFilterDefaultAndOpenEnded
								(
								m_mp,
								ulPartLevel,
								true, //fLTComparison
								true, //fGTComparison
								false, //fEQComparison
								true //fDefaultPartition
								);

	CDXLNode *pdxlnNotDefaultOrOpenEnded = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, Edxlnot), pdxlnDefaultOrOpenEnded);

	return GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, Edxland), pdxlnNotDefaultOrOpenEnded, pdxlnNullTests);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnPartitionSelectorChild
//
//	@doc:
// 		Translate the child of a partition selector expression, pushing the given
//		scalar predicate if available
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnPartitionSelectorChild
	(
	CExpression *pexprChild,
	CExpression *pexprScalarCond,
	CDXLPhysicalProperties *dxl_properties,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT_IFF(NULL != pexprScalarCond, NULL != dxl_properties);

	if (NULL == pexprScalarCond)
	{
		return CreateDXLNode(pexprChild, colref_array, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true, false);
	}

	switch(pexprChild->Pop()->Eopid())
	{
		case COperator::EopPhysicalDynamicTableScan:
			return PdxlnDynamicTableScan(pexprChild, colref_array, pdrgpdsBaseTables, pexprScalarCond, dxl_properties);
		case COperator::EopPhysicalDynamicIndexScan:
			return PdxlnIndexScanWithInlinedCondition(pexprChild, pexprScalarCond, dxl_properties, colref_array, pdrgpdsBaseTables);
		default:
			GPOS_ASSERT(COperator::EopPhysicalDynamicBitmapTableScan == pexprChild->Pop()->Eopid());
			return PdxlnDynamicBitmapTableScan(pexprChild, colref_array, pdrgpdsBaseTables, pexprScalarCond, dxl_properties);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDML
//
//	@doc:
//		Translate a DML operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDML
	(
	CExpression *pexpr,
	CColRefArray *,// colref_array
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	ULONG action_colid = 0;
	ULONG oid_colid = 0;
	ULONG ctid_colid = 0;
	ULONG segid_colid = 0;

	// extract components
	CPhysicalDML *popDML = CPhysicalDML::PopConvert(pexpr->Pop());
	*pfDML = false;
	if (IMDId::EmdidGPDBCtas == popDML->Ptabdesc()->MDId()->MdidType())
	{
		return PdxlnCTAS(pexpr, pdrgpdsBaseTables, pulNonGatherMotions, pfDML);
	}

	EdxlDmlType dxl_dml_type = Edxldmloptype(popDML->Edmlop());

	CExpression *pexprChild = (*pexpr)[0];
	CTableDescriptor *ptabdesc = popDML->Ptabdesc();
	CColRefArray *pdrgpcrSource = popDML->PdrgpcrSource();

	CColRef *pcrAction = popDML->PcrAction();
	GPOS_ASSERT(NULL != pcrAction);
	action_colid = pcrAction->Id();

	CColRef *pcrOid = popDML->PcrTableOid();
	if (pcrOid != NULL)
	{
		oid_colid = pcrOid->Id();
	}

	CColRef *pcrCtid = popDML->PcrCtid();
	CColRef *pcrSegmentId = popDML->PcrSegmentId();
	if (NULL != pcrCtid)
	{
		GPOS_ASSERT(NULL != pcrSegmentId);
		ctid_colid = pcrCtid->Id();
		segid_colid = pcrSegmentId->Id();
	}

	CColRef *pcrTupleOid = popDML->PcrTupleOid();
	ULONG tuple_oid = 0;
	BOOL preserve_oids = false;
	if (NULL != pcrTupleOid)
	{
		preserve_oids = true;
		tuple_oid = pcrTupleOid->Id();
	}

	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, pdrgpcrSource, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	CDXLTableDescr *table_descr = MakeDXLTableDescr(ptabdesc, NULL /*pdrgpcrOutput*/, NULL /*requiredProperties*/);
	ULongPtrArray *pdrgpul = CUtils::Pdrgpul(m_mp, pdrgpcrSource);

	CDXLDirectDispatchInfo *dxl_direct_dispatch_info = GetDXLDirectDispatchInfo(pexpr);
	CDXLPhysicalDML *pdxlopDML = GPOS_NEW(m_mp) CDXLPhysicalDML
									(
									m_mp,
									dxl_dml_type,
									table_descr,
									pdrgpul,
									action_colid,
									oid_colid,
									ctid_colid,
									segid_colid,
									preserve_oids,
									tuple_oid,
									dxl_direct_dispatch_info,
									popDML->IsInputSortReq()
									);

	// project list
	CColRefSet *pcrsOutput = pexpr->Prpp()->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrSource);

	CDXLNode *pdxlnDML = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopDML);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexpr);
	pdxlnDML->SetProperties(dxl_properties);

	pdxlnDML->AddChild(pdxlnPrL);
	pdxlnDML->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlnDML->GetOperator()->AssertValid(pdxlnDML, false /* validate_children */);
#endif
	*pfDML = true;

	return pdxlnDML;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnCTAS
//
//	@doc:
//		Translate a CTAS expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnCTAS
	(
	CExpression *pexpr,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	CPhysicalDML *popDML = CPhysicalDML::PopConvert(pexpr->Pop());
	GPOS_ASSERT(CLogicalDML::EdmlInsert == popDML->Edmlop());

	CExpression *pexprChild = (*pexpr)[0];
	CTableDescriptor *ptabdesc = popDML->Ptabdesc();
	CColRefArray *pdrgpcrSource = popDML->PdrgpcrSource();
	CMDRelationCtasGPDB *pmdrel = (CMDRelationCtasGPDB *) m_pmda->RetrieveRel(ptabdesc->MDId());

	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, pdrgpcrSource, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, true /*fRoot*/);

	ULongPtrArray *pdrgpul = CUtils::Pdrgpul(m_mp, pdrgpcrSource);

	pmdrel->GetDxlCtasStorageOption()->AddRef();

	const ULONG ulColumns = ptabdesc->ColumnCount();

	IntPtrArray *vartypemod_array = pmdrel->GetVarTypeModArray();
	GPOS_ASSERT(ulColumns == vartypemod_array->Size());

	// translate col descriptors
	CDXLColDescrArray *dxl_col_descr_array = GPOS_NEW(m_mp) CDXLColDescrArray(m_mp);
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		const CColumnDescriptor *pcd = ptabdesc->Pcoldesc(ul);

		CMDName *pmdnameCol = GPOS_NEW(m_mp) CMDName(m_mp, pcd->Name().Pstr());
		CColRef *colref = m_pcf->PcrCreate(pcd->RetrieveType(), pcd->TypeModifier(), pcd->Name());

		// use the col ref id for the corresponding output output column as 
		// colid for the dxl column
		CMDIdGPDB *pmdidColType = CMDIdGPDB::CastMdid(colref->RetrieveType()->MDId());
		pmdidColType->AddRef();

		CDXLColDescr *pdxlcd = GPOS_NEW(m_mp) CDXLColDescr
											(
											m_mp,
											pmdnameCol,
											colref->Id(),
											pcd->AttrNum(),
											pmdidColType,
											colref->TypeModifier(),
											false /* fdropped */,
											pcd->Width()
											);

		dxl_col_descr_array->Append(pdxlcd);
	}

	ULongPtrArray *pdrgpulDistr = NULL;
	if (IMDRelation::EreldistrHash == pmdrel->GetRelDistribution())
	{
		pdrgpulDistr = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
		const ULONG ulDistrCols = pmdrel->DistrColumnCount();
		for (ULONG ul = 0; ul < ulDistrCols; ul++)
		{
			const IMDColumn *pmdcol = pmdrel->GetDistrColAt(ul);
			INT attno = pmdcol->AttrNum();
			GPOS_ASSERT(0 < attno);
			pdrgpulDistr->Append(GPOS_NEW(m_mp) ULONG(attno - 1));
		}
	}

	CMDName *mdname_schema = NULL;
	if (NULL != pmdrel->GetMdNameSchema())
	{
		mdname_schema = GPOS_NEW(m_mp) CMDName(m_mp, pmdrel->GetMdNameSchema()->GetMDName());
	}

	vartypemod_array->AddRef();
	CDXLPhysicalCTAS *pdxlopCTAS = GPOS_NEW(m_mp) CDXLPhysicalCTAS
									(
									m_mp,
									mdname_schema,
									GPOS_NEW(m_mp) CMDName(m_mp, pmdrel->Mdname().GetMDName()),
									dxl_col_descr_array,
									pmdrel->GetDxlCtasStorageOption(),
									pmdrel->GetRelDistribution(),
									pdrgpulDistr,
									pmdrel->IsTemporary(),
									pmdrel->HasOids(),
									pmdrel->RetrieveRelStorageType(),
									pdrgpul,
									vartypemod_array
									);

	CDXLNode *pdxlnCTAS = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopCTAS);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexpr);
	pdxlnCTAS->SetProperties(dxl_properties);

	CColRefSet *pcrsOutput = pexpr->Prpp()->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrSource);

	pdxlnCTAS->AddChild(pdxlnPrL);
	pdxlnCTAS->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlnCTAS->GetOperator()->AssertValid(pdxlnCTAS, false /* validate_children */);
#endif
	return pdxlnCTAS;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetDXLDirectDispatchInfo
//
//	@doc:
//		Return the direct dispatch info spec for the possible values of the distribution
//		key in a DML insert statement. Returns NULL if values are not constant.
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo *
CTranslatorExprToDXL::GetDXLDirectDispatchInfo
	(
	CExpression *pexprDML
	)
{
	GPOS_ASSERT(NULL != pexprDML);

	CPhysicalDML *popDML = CPhysicalDML::PopConvert(pexprDML->Pop());
	CTableDescriptor *ptabdesc = popDML->Ptabdesc();
	const CColumnDescriptorArray *pdrgpcoldescDist = ptabdesc->PdrgpcoldescDist();

	if (CLogicalDML::EdmlInsert != popDML->Edmlop() ||
		IMDRelation::EreldistrHash != ptabdesc->GetRelDistribution() ||
		1 < pdrgpcoldescDist->Size())
	{
		// directed dispatch only supported for insert statements on hash-distributed tables 
		// with a single distribution column
		return NULL;
	}


	GPOS_ASSERT(1 == pdrgpcoldescDist->Size());
	CColumnDescriptor *pcoldesc = (*pdrgpcoldescDist)[0];
	ULONG ulPos = ptabdesc->UlPos(pcoldesc, ptabdesc->Pdrgpcoldesc());
	GPOS_ASSERT(ulPos < ptabdesc->Pdrgpcoldesc()->Size() && "Column not found");

	CColRef *pcrDistrCol = (*popDML->PdrgpcrSource())[ulPos];
	CPropConstraint *ppc = (*pexprDML)[0]->DerivePropertyConstraint();

	if (NULL == ppc->Pcnstr())
	{
		return NULL;
	}

	CConstraint *pcnstrDistrCol = ppc->Pcnstr()->Pcnstr(m_mp, pcrDistrCol);
	if (!CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol))
	{
		CRefCount::SafeRelease(pcnstrDistrCol);
		return NULL;
	}

	GPOS_ASSERT(CConstraint::EctInterval == pcnstrDistrCol->Ect());

	CConstraintInterval *pci = dynamic_cast<CConstraintInterval *>(pcnstrDistrCol);
	GPOS_ASSERT(1 >= pci->Pdrgprng()->Size());

	CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(m_mp) CDXLDatumArray(m_mp);
	CDXLDatum *dxl_datum = NULL;

	if (1 == pci->Pdrgprng()->Size())
	{
		const CRange *prng = (*pci->Pdrgprng())[0];
		dxl_datum = CTranslatorExprToDXLUtils::GetDatumVal(m_mp, m_pmda, prng->PdatumLeft());
	}
	else
	{
		GPOS_ASSERT(pci->FIncludesNull());
		dxl_datum = pcrDistrCol->RetrieveType()->GetDXLDatumNull(m_mp);
	}

	pdrgpdxldatum->Append(dxl_datum);

	pcnstrDistrCol->Release();

	CDXLDatum2dArray *pdrgpdrgpdxldatum = GPOS_NEW(m_mp) CDXLDatum2dArray(m_mp);
	pdrgpdrgpdxldatum->Append(pdrgpdxldatum);
	return GPOS_NEW(m_mp) CDXLDirectDispatchInfo(pdrgpdrgpdxldatum);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnSplit
//
//	@doc:
//		Translate a split operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnSplit
	(
	CExpression *pexpr,
	CColRefArray *, // colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(2 == pexpr->Arity());

	ULONG action_colid = 0;
	ULONG ctid_colid = 0;
	ULONG segid_colid = 0;
	ULONG tuple_oid = 0;

	// extract components
	CPhysicalSplit *popSplit = CPhysicalSplit::PopConvert(pexpr->Pop());

	CExpression *pexprChild = (*pexpr)[0];
	CExpression *pexprProjList = (*pexpr)[1];

	CColRef *pcrAction = popSplit->PcrAction();
	GPOS_ASSERT(NULL != pcrAction);
	action_colid = pcrAction->Id();

	CColRef *pcrCtid = popSplit->PcrCtid();
	GPOS_ASSERT(NULL != pcrCtid);
	ctid_colid = pcrCtid->Id();

	CColRef *pcrSegmentId = popSplit->PcrSegmentId();
	GPOS_ASSERT(NULL != pcrSegmentId);
	segid_colid = pcrSegmentId->Id();

	CColRef *pcrTupleOid = popSplit->PcrTupleOid();
	BOOL preserve_oids = false;
	if (NULL != pcrTupleOid)
	{
		preserve_oids = true;
		tuple_oid = pcrTupleOid->Id();
	}

	CColRefArray *pdrgpcrDelete = popSplit->PdrgpcrDelete();
	ULongPtrArray *delete_colid_array = CUtils::Pdrgpul(m_mp, pdrgpcrDelete);

	CColRefArray *pdrgpcrInsert = popSplit->PdrgpcrInsert();
	ULongPtrArray *insert_colid_array = CUtils::Pdrgpul(m_mp, pdrgpcrInsert);

	CColRefSet *pcrsRequired = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsRequired->Include(pdrgpcrInsert);
	pcrsRequired->Include(pdrgpcrDelete);
	CColRefArray *pdrgpcrRequired = pcrsRequired->Pdrgpcr(m_mp);

	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, pdrgpcrRequired, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, true /*fRemap*/, false /*fRoot*/);
	pdrgpcrRequired->Release();
	pcrsRequired->Release();

	CDXLPhysicalSplit *pdxlopSplit = GPOS_NEW(m_mp) CDXLPhysicalSplit
													(
													m_mp,
													delete_colid_array,
													insert_colid_array,
													action_colid,
													ctid_colid,
													segid_colid,
													preserve_oids,
													tuple_oid
													);

	// project list
	CColRefSet *pcrsOutput = pexpr->Prpp()->PcrsRequired();
	CDXLNode *pdxlnPrL = PdxlnProjList(pexprProjList, pcrsOutput, pdrgpcrDelete);

	CDXLNode *pdxlnSplit = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSplit);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexpr);
	pdxlnSplit->SetProperties(dxl_properties);

	pdxlnSplit->AddChild(pdxlnPrL);
	pdxlnSplit->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlnSplit->GetOperator()->AssertValid(pdxlnSplit, false /* validate_children */);
#endif
	return pdxlnSplit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnRowTrigger
//
//	@doc:
//		Translate a row trigger operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnRowTrigger
	(
	CExpression *pexpr,
	CColRefArray *, // colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(1 == pexpr->Arity());

	// extract components
	CPhysicalRowTrigger *popRowTrigger = CPhysicalRowTrigger::PopConvert(pexpr->Pop());

	CExpression *pexprChild = (*pexpr)[0];

	IMDId *rel_mdid = popRowTrigger->GetRelMdId();
	rel_mdid->AddRef();

	INT type = popRowTrigger->GetType();

	CColRefSet *pcrsRequired = GPOS_NEW(m_mp) CColRefSet(m_mp);
	ULongPtrArray *colids_old = NULL;
	ULongPtrArray *colids_new = NULL;

	CColRefArray *pdrgpcrOld = popRowTrigger->PdrgpcrOld();
	if (NULL != pdrgpcrOld)
	{
		colids_old = CUtils::Pdrgpul(m_mp, pdrgpcrOld);
		pcrsRequired->Include(pdrgpcrOld);
	}

	CColRefArray *pdrgpcrNew = popRowTrigger->PdrgpcrNew();
	if (NULL != pdrgpcrNew)
	{
		colids_new = CUtils::Pdrgpul(m_mp, pdrgpcrNew);
		pcrsRequired->Include(pdrgpcrNew);
	}

	CColRefArray *pdrgpcrRequired = pcrsRequired->Pdrgpcr(m_mp);
	CDXLNode *child_dxlnode = CreateDXLNode(pexprChild, pdrgpcrRequired, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);
	pdrgpcrRequired->Release();
	pcrsRequired->Release();

	CDXLPhysicalRowTrigger *pdxlopRowTrigger = GPOS_NEW(m_mp) CDXLPhysicalRowTrigger
													(
													m_mp,
													rel_mdid,
													type,
													colids_old,
													colids_new
													);

	// project list
	CColRefSet *pcrsOutput = pexpr->Prpp()->PcrsRequired();
	CDXLNode *pdxlnPrL = NULL;
	if (NULL != pdrgpcrNew)
	{
		pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrNew);
	}
	else
	{
		pdxlnPrL = PdxlnProjList(pcrsOutput, pdrgpcrOld);
	}

	CDXLNode *pdxlnRowTrigger = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopRowTrigger);
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexpr);
	pdxlnRowTrigger->SetProperties(dxl_properties);

	pdxlnRowTrigger->AddChild(pdxlnPrL);
	pdxlnRowTrigger->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlnRowTrigger->GetOperator()->AssertValid(pdxlnRowTrigger, false /* validate_children */);
#endif
	return pdxlnRowTrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::Edxldmloptype
//
//	@doc:
//		Return the EdxlDmlType for a given DML op type
//
//---------------------------------------------------------------------------
EdxlDmlType
CTranslatorExprToDXL::Edxldmloptype
	(
	const CLogicalDML::EDMLOperator edmlop
	)
	const
{
	switch (edmlop)
	{
		case CLogicalDML::EdmlInsert:
			return Edxldmlinsert;

		case CLogicalDML::EdmlDelete:
			return Edxldmldelete;

		case CLogicalDML::EdmlUpdate:
			return Edxldmlupdate;

		default:
			GPOS_ASSERT(!"Unrecognized DML operation");
			return EdxldmlSentinel;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCmp
//
//	@doc:
//		Create a DXL scalar comparison node from an optimizer scalar comparison 
//		expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCmp
	(
	CExpression *pexprScCmp
	)
{
	GPOS_ASSERT(NULL != pexprScCmp);

	// extract components
	CExpression *pexprLeft = (*pexprScCmp)[0];
	CExpression *pexprRight = (*pexprScCmp)[1];

	// translate children expression
	CDXLNode *dxlnode_left = PdxlnScalar(pexprLeft);
	CDXLNode *dxlnode_right = PdxlnScalar(pexprRight);

	CScalarCmp *popScCmp = CScalarCmp::PopConvert(pexprScCmp->Pop());

	GPOS_ASSERT(NULL != popScCmp);
	GPOS_ASSERT(NULL != popScCmp->Pstr());
	GPOS_ASSERT(NULL != popScCmp->Pstr()->GetBuffer());

	// construct a scalar comparison node
	IMDId *mdid = popScCmp->MdIdOp();
	mdid->AddRef();

	CWStringConst *str_name = GPOS_NEW(m_mp) CWStringConst(m_mp, popScCmp->Pstr()->GetBuffer());

	CDXLNode *pdxlnCmp = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarComp(m_mp, mdid, str_name));

	// add children
	pdxlnCmp->AddChild(dxlnode_left);
	pdxlnCmp->AddChild(dxlnode_right);

#ifdef GPOS_DEBUG
	pdxlnCmp->GetOperator()->AssertValid(pdxlnCmp, false /* validate_children */);
#endif

	return pdxlnCmp;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScDistinctCmp
//
//	@doc:
//		Create a DXL scalar distinct comparison node from an optimizer scalar
//		is distinct from expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScDistinctCmp
	(
	CExpression *pexprScDist
	)
{
	GPOS_ASSERT(NULL != pexprScDist);

	// extract components
	CExpression *pexprLeft = (*pexprScDist)[0];
	CExpression *pexprRight = (*pexprScDist)[1];

	// translate children expression
	CDXLNode *dxlnode_left = PdxlnScalar(pexprLeft);
	CDXLNode *dxlnode_right = PdxlnScalar(pexprRight);

	CScalarIsDistinctFrom *popScIDF = CScalarIsDistinctFrom::PopConvert(pexprScDist->Pop());

	// construct a scalar distinct comparison node
	IMDId *mdid = popScIDF->MdIdOp();
	mdid->AddRef();

	CDXLNode *pdxlnDistCmp = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarDistinctComp(m_mp, mdid));

	// add children
	pdxlnDistCmp->AddChild(dxlnode_left);
	pdxlnDistCmp->AddChild(dxlnode_right);

#ifdef GPOS_DEBUG
	pdxlnDistCmp->GetOperator()->AssertValid(pdxlnDistCmp, false /* validate_children */);
#endif

	return pdxlnDistCmp;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScOp
//
//	@doc:
//		Create a DXL scalar op expr node from an optimizer scalar op expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScOp
	(
	CExpression *pexprOp
	)
{
	GPOS_ASSERT(NULL != pexprOp && ((1 == pexprOp->Arity()) || (2 == pexprOp->Arity())));
	CScalarOp *pscop = CScalarOp::PopConvert(pexprOp->Pop());

	// construct a scalar opexpr node
	CWStringConst *str_name = GPOS_NEW(m_mp) CWStringConst(m_mp, pscop->Pstr()->GetBuffer());

	IMDId *mdid_op = pscop->MdIdOp();
	mdid_op->AddRef();

	IMDId *return_type_mdid = pscop->GetReturnTypeMdId();
	if (NULL != return_type_mdid)
	{
		return_type_mdid->AddRef();
	}

	CDXLNode *pdxlnOpExpr = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOpExpr(m_mp, mdid_op, return_type_mdid, str_name));

	TranslateScalarChildren(pexprOp, pdxlnOpExpr);

#ifdef GPOS_DEBUG
	pdxlnOpExpr->GetOperator()->AssertValid(pdxlnOpExpr, false /* validate_children */);
#endif

	return pdxlnOpExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScBoolExpr
//
//	@doc:
//		Create a DXL scalar bool expression node from an optimizer scalar log op
//		expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScBoolExpr
	(
	CExpression *pexprScBoolOp
	)
{
	GPOS_ASSERT(NULL != pexprScBoolOp);
	CScalarBoolOp *popScBoolOp = CScalarBoolOp::PopConvert(pexprScBoolOp->Pop());
	EdxlBoolExprType edxlbooltype = Edxlbooltype(popScBoolOp->Eboolop());

#ifdef GPOS_DEBUG
	if(CScalarBoolOp::EboolopNot == popScBoolOp->Eboolop())
	{
		GPOS_ASSERT(1 == pexprScBoolOp->Arity());
	}
	else
	{
		GPOS_ASSERT(2 <= pexprScBoolOp->Arity());
	}
#endif // GPOS_DEBUG

	CDXLNode *pdxlnBoolExpr = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarBoolExpr(m_mp, edxlbooltype));

	TranslateScalarChildren(pexprScBoolOp, pdxlnBoolExpr);

#ifdef GPOS_DEBUG
	pdxlnBoolExpr->GetOperator()->AssertValid(pdxlnBoolExpr, false /* validate_children */);
#endif

	return pdxlnBoolExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::Edxlbooltype
//
//	@doc:
//		Return the EdxlBoolExprType for a given scalar logical op type
//
//---------------------------------------------------------------------------
EdxlBoolExprType
CTranslatorExprToDXL::Edxlbooltype
	(
	const CScalarBoolOp::EBoolOperator eboolop
	)
	const
{
	switch (eboolop)
	{
		case CScalarBoolOp::EboolopNot:
			return Edxlnot;

		case CScalarBoolOp::EboolopAnd:
			return Edxland;

		case CScalarBoolOp::EboolopOr:
			return Edxlor;

		default:
			GPOS_ASSERT(!"Unrecognized boolean expression type");
			return EdxlBoolExprTypeSentinel;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScId
//
//	@doc:
//		Create a DXL scalar identifier node from an optimizer scalar id expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScId
	(
	CExpression *pexprIdent
	)
{
	GPOS_ASSERT(NULL != pexprIdent);

	CScalarIdent *popScId = CScalarIdent::PopConvert(pexprIdent->Pop());
	CColRef *colref = const_cast<CColRef*>(popScId->Pcr());

	return CTranslatorExprToDXLUtils::PdxlnIdent(m_mp, m_phmcrdxln, m_phmcrdxlnIndexLookup, colref);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScFuncExpr
//
//	@doc:
//		Create a DXL scalar func expr node from an optimizer scalar func expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScFuncExpr
	(
	CExpression *pexprFunc
	)
{
	GPOS_ASSERT(NULL != pexprFunc);

	CScalarFunc *popScFunc = CScalarFunc::PopConvert(pexprFunc->Pop());

	IMDId *mdid_func = popScFunc->FuncMdId();
	mdid_func->AddRef();

	IMDId *mdid_return_type = popScFunc->MdidType();
	mdid_return_type->AddRef();

	const IMDFunction *pmdfunc = m_pmda->RetrieveFunc(mdid_func);

	CDXLNode *pdxlnFuncExpr = GPOS_NEW(m_mp) CDXLNode
											(
											m_mp,
											GPOS_NEW(m_mp) CDXLScalarFuncExpr(m_mp, mdid_func, mdid_return_type, popScFunc->TypeModifier(), pmdfunc->ReturnsSet())
											);

	// translate children
	TranslateScalarChildren(pexprFunc, pdxlnFuncExpr);

	return pdxlnFuncExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScWindowFuncExpr
//
//	@doc:
//		Create a DXL scalar window ref node from an optimizer scalar window
//		function expr
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScWindowFuncExpr
	(
	CExpression *pexprWindowFunc
	)
{
	GPOS_ASSERT(NULL != pexprWindowFunc);

	CScalarWindowFunc *popScWindowFunc = CScalarWindowFunc::PopConvert(pexprWindowFunc->Pop());

	IMDId *mdid_func = popScWindowFunc->FuncMdId();
	mdid_func->AddRef();

	IMDId *mdid_return_type = popScWindowFunc->MdidType();
	mdid_return_type->AddRef();

	EdxlWinStage dxl_win_stage = Ews(popScWindowFunc->Ews());
	CDXLScalarWindowRef *pdxlopWindowref = GPOS_NEW(m_mp) CDXLScalarWindowRef
															(
															m_mp,
															mdid_func,
															mdid_return_type,
															popScWindowFunc->IsDistinct(),
															popScWindowFunc->IsStarArg(),
															popScWindowFunc->IsSimpleAgg(),
															dxl_win_stage,
															0 /* ulWinspecPosition */
															);

	CDXLNode *pdxlnWindowRef = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopWindowref);

	// translate children
	TranslateScalarChildren(pexprWindowFunc, pdxlnWindowRef);

	return pdxlnWindowRef;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::Ews
//
//	@doc:
//		Get the DXL representation of the window stage
//
//---------------------------------------------------------------------------
EdxlWinStage
CTranslatorExprToDXL::Ews
	(
	CScalarWindowFunc::EWinStage ews
	)
	const
{
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] =
	{
		{EdxlwinstageImmediate, CScalarWindowFunc::EwsImmediate},
		{EdxlwinstagePreliminary, CScalarWindowFunc::EwsPreliminary},
		{EdxlwinstageRowKey, CScalarWindowFunc::EwsRowKey}
	};
#ifdef GPOS_DEBUG
	const ULONG arity = GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	GPOS_ASSERT(arity > (ULONG) ews);
#endif
	ULONG *pulElem = window_frame_boundary_to_frame_boundary_mapping[(ULONG) ews];
	EdxlWinStage edxlws = (EdxlWinStage) pulElem[0];

	return edxlws;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScAggref
//
//	@doc:
//		Create a DXL scalar aggref node from an optimizer scalar agg func expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScAggref
	(
	CExpression *pexprAggFunc
	)
{
	GPOS_ASSERT(NULL != pexprAggFunc);

	CScalarAggFunc *popScAggFunc = CScalarAggFunc::PopConvert(pexprAggFunc->Pop());
	IMDId *pmdidAggFunc = popScAggFunc->MDId();
	pmdidAggFunc->AddRef();

	IMDId *resolved_rettype = NULL;
	if (popScAggFunc->FHasAmbiguousReturnType())
	{
		// Agg has an ambiguous return type, use the resolved type instead
		resolved_rettype = popScAggFunc->MdidType();
		resolved_rettype->AddRef();
	}

	EdxlAggrefStage edxlaggrefstage = EdxlaggstageNormal;

	if (popScAggFunc->FGlobal() && popScAggFunc->FSplit())
	{
		edxlaggrefstage = EdxlaggstageFinal;
	}
	else if (EaggfuncstageIntermediate == popScAggFunc->Eaggfuncstage())
	{
		edxlaggrefstage = EdxlaggstageIntermediate;
	}
	else if (!popScAggFunc->FGlobal())
	{
		edxlaggrefstage = EdxlaggstagePartial;
	}

	CDXLScalarAggref *pdxlopAggRef = GPOS_NEW(m_mp) CDXLScalarAggref
												(
												m_mp,
												pmdidAggFunc,
												resolved_rettype,
												popScAggFunc->IsDistinct(),
												edxlaggrefstage
												);

	CDXLNode *pdxlnAggref = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopAggRef);
	TranslateScalarChildren(pexprAggFunc, pdxlnAggref);

	return pdxlnAggref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScIfStmt
//
//	@doc:
//		Create a DXL scalar if node from an optimizer scalar if expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScIfStmt
	(
	CExpression *pexprIfStmt
	)
{
	GPOS_ASSERT(NULL != pexprIfStmt);

	GPOS_ASSERT(3 == pexprIfStmt->Arity());

	CScalarIf *popScIf = CScalarIf::PopConvert(pexprIfStmt->Pop());

	IMDId *mdid_type = popScIf->MdidType();
	mdid_type->AddRef();

	CDXLNode *pdxlnIfStmt = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarIfStmt(m_mp, mdid_type));
	TranslateScalarChildren(pexprIfStmt, pdxlnIfStmt);

	return pdxlnIfStmt;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScSwitch
//
//	@doc:
//		Create a DXL scalar switch node from an optimizer scalar switch expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScSwitch
	(
	CExpression *pexprSwitch
	)
{
	GPOS_ASSERT(NULL != pexprSwitch);
	GPOS_ASSERT(1 < pexprSwitch->Arity());
	CScalarSwitch *pop = CScalarSwitch::PopConvert(pexprSwitch->Pop());

	IMDId *mdid_type = pop->MdidType();
	mdid_type->AddRef();

	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSwitch(m_mp, mdid_type));
	TranslateScalarChildren(pexprSwitch, dxlnode);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScSwitchCase
//
//	@doc:
//		Create a DXL scalar switch case node from an optimizer scalar switch
//		case expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScSwitchCase
	(
	CExpression *pexprSwitchCase
	)
{
	GPOS_ASSERT(NULL != pexprSwitchCase);
	GPOS_ASSERT(2 == pexprSwitchCase->Arity());

	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSwitchCase(m_mp));
	TranslateScalarChildren(pexprSwitchCase, dxlnode);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScNullIf
//
//	@doc:
//		Create a DXL scalar nullif node from an optimizer scalar
//		nullif expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScNullIf
	(
	CExpression *pexprScNullIf
	)
{
	GPOS_ASSERT(NULL != pexprScNullIf);

	CScalarNullIf *pop = CScalarNullIf::PopConvert(pexprScNullIf->Pop());

	IMDId *mdid = pop->MdIdOp();
	mdid->AddRef();

	IMDId *mdid_type = pop->MdidType();
	mdid_type->AddRef();

	CDXLScalarNullIf *dxl_op = GPOS_NEW(m_mp) CDXLScalarNullIf(m_mp, mdid, mdid_type);
	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
	TranslateScalarChildren(pexprScNullIf, dxlnode);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCaseTest
//
//	@doc:
//		Create a DXL scalar case test node from an optimizer scalar case test
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCaseTest
	(
	CExpression *pexprScCaseTest
	)
{
	GPOS_ASSERT(NULL != pexprScCaseTest);
	CScalarCaseTest *pop = CScalarCaseTest::PopConvert(pexprScCaseTest->Pop());

	IMDId *mdid_type = pop->MdidType();
	mdid_type->AddRef();

	CDXLScalarCaseTest *dxl_op = GPOS_NEW(m_mp) CDXLScalarCaseTest(m_mp, mdid_type);

	return GPOS_NEW(m_mp) CDXLNode(m_mp, dxl_op);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScNullTest
//
//	@doc:
//		Create a DXL scalar null test node from an optimizer scalar null test expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScNullTest
	(
	CExpression *pexprNullTest
	)
{
	GPOS_ASSERT(NULL != pexprNullTest);

	CDXLNode *pdxlnNullTest = GPOS_NEW(m_mp) CDXLNode
											(
											m_mp,
											GPOS_NEW(m_mp) CDXLScalarNullTest(m_mp, true /* is_null */));

	// translate child
	GPOS_ASSERT(1 == pexprNullTest->Arity());

	CExpression *pexprChild = (*pexprNullTest)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnNullTest->AddChild(child_dxlnode);

	return pdxlnNullTest;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScBooleanTest
//
//	@doc:
//		Create a DXL scalar null test node from an optimizer scalar null test expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScBooleanTest
	(
	CExpression *pexprScBooleanTest
	)
{
	GPOS_ASSERT(NULL != pexprScBooleanTest);
	GPOS_ASSERT(1 == pexprScBooleanTest->Arity());

	const ULONG rgulBoolTestMapping[][2] =
	{
		{CScalarBooleanTest::EbtIsTrue, EdxlbooleantestIsTrue},
		{CScalarBooleanTest::EbtIsNotTrue, EdxlbooleantestIsNotTrue},
		{CScalarBooleanTest::EbtIsFalse, EdxlbooleantestIsFalse},
		{CScalarBooleanTest::EbtIsNotFalse, EdxlbooleantestIsNotFalse},
		{CScalarBooleanTest::EbtIsUnknown, EdxlbooleantestIsUnknown},
		{CScalarBooleanTest::EbtIsNotUnknown, EdxlbooleantestIsNotUnknown},
	};

	CScalarBooleanTest *popBoolTest = CScalarBooleanTest::PopConvert(pexprScBooleanTest->Pop());
	EdxlBooleanTestType edxlbooltest = (EdxlBooleanTestType) (rgulBoolTestMapping[popBoolTest->Ebt()][1]);
	CDXLNode *pdxlnScBooleanTest = GPOS_NEW(m_mp) CDXLNode
											(
											m_mp,
											GPOS_NEW(m_mp) CDXLScalarBooleanTest(m_mp, edxlbooltest)
											);

	// translate child
	CExpression *pexprChild = (*pexprScBooleanTest)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnScBooleanTest->AddChild(child_dxlnode);

	return pdxlnScBooleanTest;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCoalesce
//
//	@doc:
//		Create a DXL scalar coalesce node from an optimizer scalar coalesce expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCoalesce
	(
	CExpression *pexprCoalesce
	)
{
	GPOS_ASSERT(NULL != pexprCoalesce);
	GPOS_ASSERT(0 < pexprCoalesce->Arity());
	CScalarCoalesce *popScCoalesce = CScalarCoalesce::PopConvert(pexprCoalesce->Pop());

	IMDId *mdid_type = popScCoalesce->MdidType();
	mdid_type->AddRef();

	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarCoalesce(m_mp, mdid_type));
	TranslateScalarChildren(pexprCoalesce, dxlnode);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScMinMax
//
//	@doc:
//		Create a DXL scalar MinMax node from an optimizer scalar MinMax expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScMinMax
	(
	CExpression *pexprMinMax
	)
{
	GPOS_ASSERT(NULL != pexprMinMax);
	GPOS_ASSERT(0 < pexprMinMax->Arity());
	CScalarMinMax *popScMinMax = CScalarMinMax::PopConvert(pexprMinMax->Pop());

	CScalarMinMax::EScalarMinMaxType esmmt = popScMinMax->Esmmt();
	GPOS_ASSERT(CScalarMinMax::EsmmtMin == esmmt || CScalarMinMax::EsmmtMax == esmmt);

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = CDXLScalarMinMax::EmmtMin;
	if (CScalarMinMax::EsmmtMax == esmmt)
	{
		min_max_type = CDXLScalarMinMax::EmmtMax;
	}

	IMDId *mdid_type = popScMinMax->MdidType();
	mdid_type->AddRef();

	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarMinMax(m_mp, mdid_type, min_max_type));
	TranslateScalarChildren(pexprMinMax, dxlnode);

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::TranslateScalarChildren
//
//	@doc:
//		Translate expression children and add them as children of the DXL node
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXL::TranslateScalarChildren
	(
	CExpression *pexpr,
	CDXLNode *dxlnode
	)
{
	const ULONG arity = pexpr->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
		dxlnode->AddChild(child_dxlnode);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCast
//
//	@doc:
//		Create a DXL scalar relabel type node from an
//		optimizer scalar relabel type expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCast
	(
	CExpression *pexprCast
	)
{
	GPOS_ASSERT(NULL != pexprCast);
	CScalarCast *popScCast = CScalarCast::PopConvert(pexprCast->Pop());

	IMDId *mdid = popScCast->MdidType();
	mdid->AddRef();

	IMDId *mdid_func = popScCast->FuncMdId();
	mdid_func->AddRef();

	CDXLNode *pdxlnCast = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarCast(m_mp, mdid, mdid_func));

	// translate child
	GPOS_ASSERT(1 == pexprCast->Arity());
	CExpression *pexprChild = (*pexprCast)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnCast->AddChild(child_dxlnode);

	return pdxlnCast;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCoerceToDomain
//
//	@doc:
//		Create a DXL scalar coerce node from an optimizer scalar coerce expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCoerceToDomain
	(
	CExpression *pexprCoerce
	)
{
	GPOS_ASSERT(NULL != pexprCoerce);
	CScalarCoerceToDomain *popScCoerce = CScalarCoerceToDomain::PopConvert(pexprCoerce->Pop());

	IMDId *mdid = popScCoerce->MdidType();
	mdid->AddRef();


	CDXLNode *pdxlnCoerce =
		GPOS_NEW(m_mp) CDXLNode
			(
			m_mp,
			GPOS_NEW(m_mp) CDXLScalarCoerceToDomain
					(
					m_mp,
					mdid,
					popScCoerce->TypeModifier(),
					(EdxlCoercionForm) popScCoerce->Ecf(), // map Coercion Form directly based on position in enum
					popScCoerce->Location()
					)
			);

	// translate child
	GPOS_ASSERT(1 == pexprCoerce->Arity());
	CExpression *pexprChild = (*pexprCoerce)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnCoerce->AddChild(child_dxlnode);

	return pdxlnCoerce;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScCoerceViaIO
//
//	@doc:
//		Create a DXL scalar coerce node from an optimizer scalar coerce expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScCoerceViaIO
	(
	CExpression *pexprCoerce
	)
{
	GPOS_ASSERT(NULL != pexprCoerce);
	CScalarCoerceViaIO *popScCerce = CScalarCoerceViaIO::PopConvert(pexprCoerce->Pop());

	IMDId *mdid = popScCerce->MdidType();
	mdid->AddRef();


	CDXLNode *pdxlnCoerce =
		GPOS_NEW(m_mp) CDXLNode
			(
			m_mp,
			GPOS_NEW(m_mp) CDXLScalarCoerceViaIO
					(
					m_mp,
					mdid,
					popScCerce->TypeModifier(),
					(EdxlCoercionForm) popScCerce->Ecf(), // map Coercion Form directly based on position in enum
					popScCerce->Location()
					)
			);

	// translate child
	GPOS_ASSERT(1 == pexprCoerce->Arity());
	CExpression *pexprChild = (*pexprCoerce)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnCoerce->AddChild(child_dxlnode);

	return pdxlnCoerce;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScArrayCoerceExpr
//
//	@doc:
//		Create a DXL node from an optimizer scalar array coerce expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScArrayCoerceExpr
	(
	CExpression *pexprArrayCoerceExpr
	)
{
	GPOS_ASSERT(NULL != pexprArrayCoerceExpr);
	CScalarArrayCoerceExpr *popScArrayCoerceExpr = CScalarArrayCoerceExpr::PopConvert(pexprArrayCoerceExpr->Pop());

	IMDId *pmdidElemFunc = popScArrayCoerceExpr->PmdidElementFunc();
	pmdidElemFunc->AddRef();
	IMDId *mdid = popScArrayCoerceExpr->MdidType();
	mdid->AddRef();

	CDXLNode *pdxlnArrayCoerceExpr =
		GPOS_NEW(m_mp) CDXLNode
			(
			m_mp,
			GPOS_NEW(m_mp) CDXLScalarArrayCoerceExpr
					(
					m_mp,
					pmdidElemFunc,
					mdid,
					popScArrayCoerceExpr->TypeModifier(),
					popScArrayCoerceExpr->IsExplicit(),
					(EdxlCoercionForm) popScArrayCoerceExpr->Ecf(), // map Coercion Form directly based on position in enum
					popScArrayCoerceExpr->Location()
					)
			);

	// translate child
	GPOS_ASSERT(1 == pexprArrayCoerceExpr->Arity());
	CExpression *pexprChild = (*pexprArrayCoerceExpr)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	pdxlnArrayCoerceExpr->AddChild(child_dxlnode);

	return pdxlnArrayCoerceExpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetWindowFrame
//
//	@doc:
//		Translate a window frame
//
//---------------------------------------------------------------------------
CDXLWindowFrame *
CTranslatorExprToDXL::GetWindowFrame
	(
	CWindowFrame *pwf
	)
{
	GPOS_ASSERT(NULL != pwf);

	if (CWindowFrame::IsEmpty(pwf))
	{
		// an empty frame is translated as 'no frame'
		return NULL;
	}

	// mappings for frame info in expression and dxl worlds
	const ULONG rgulSpecMapping[][2] =
	{
		{CWindowFrame::EfsRows, EdxlfsRow},
		{CWindowFrame::EfsRange, EdxlfsRange}
	};

	const ULONG rgulBoundaryMapping[][2] =
	{
		{CWindowFrame::EfbUnboundedPreceding, EdxlfbUnboundedPreceding},
		{CWindowFrame::EfbBoundedPreceding, EdxlfbBoundedPreceding},
		{CWindowFrame::EfbCurrentRow, EdxlfbCurrentRow},
		{CWindowFrame::EfbUnboundedFollowing, EdxlfbUnboundedFollowing},
		{CWindowFrame::EfbBoundedFollowing, EdxlfbBoundedFollowing},
		{CWindowFrame::EfbDelayedBoundedPreceding, EdxlfbDelayedBoundedPreceding},
		{CWindowFrame::EfbDelayedBoundedFollowing, EdxlfbDelayedBoundedFollowing}
	};

	const ULONG rgulExclusionStrategyMapping[][2] =
	{
		{CWindowFrame::EfesNone, EdxlfesNone},
		{CWindowFrame::EfesNulls, EdxlfesNulls},
		{CWindowFrame::EfesCurrentRow, EdxlfesCurrentRow},
		{CWindowFrame::EfseMatchingOthers, EdxlfesGroup},
		{CWindowFrame::EfesTies, EdxlfesTies}
	};

	EdxlFrameSpec edxlfs = (EdxlFrameSpec) (rgulSpecMapping[pwf->Efs()][1]);
	EdxlFrameBoundary edxlfbLeading = (EdxlFrameBoundary) (rgulBoundaryMapping[pwf->EfbLeading()][1]);
	EdxlFrameBoundary edxlfbTrailing = (EdxlFrameBoundary) (rgulBoundaryMapping[pwf->EfbTrailing()][1]);
	EdxlFrameExclusionStrategy frame_exc_strategy = (EdxlFrameExclusionStrategy) (rgulExclusionStrategyMapping[pwf->Efes()][1]);

	// translate scalar expressions representing leading and trailing frame edges
	CDXLNode *pdxlnLeading = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarWindowFrameEdge(m_mp, true /* fLeading */, edxlfbLeading));
	if (NULL != pwf->PexprLeading())
	{
		pdxlnLeading->AddChild(PdxlnScalar(pwf->PexprLeading()));
	}

	CDXLNode *pdxlnTrailing = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarWindowFrameEdge(m_mp, false /* fLeading */, edxlfbTrailing));
	if (NULL != pwf->PexprTrailing())
	{
		pdxlnTrailing->AddChild(PdxlnScalar(pwf->PexprTrailing()));
	}

	return GPOS_NEW(m_mp) CDXLWindowFrame(m_mp, edxlfs, frame_exc_strategy, pdxlnLeading, pdxlnTrailing);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnWindow
//
//	@doc:
//		Create a DXL window node from physical sequence project expression.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnWindow
	(
	CExpression *pexprSeqPrj,
	CColRefArray *colref_array,
	CDistributionSpecArray *pdrgpdsBaseTables,
	ULONG *pulNonGatherMotions,
	BOOL *pfDML
	)
{
	GPOS_ASSERT(NULL != pexprSeqPrj);

	CPhysicalSequenceProject *popSeqPrj = CPhysicalSequenceProject::PopConvert(pexprSeqPrj->Pop());
	CDistributionSpec *pds = popSeqPrj->Pds();
	ULongPtrArray *colids = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
	CExpressionArray *pdrgpexprPartCol = NULL;
	if (CDistributionSpec::EdtHashed == pds->Edt())
	{
		CDistributionSpecHashed *pdshashed = CDistributionSpecHashed::PdsConvert(pds);
		pdrgpexprPartCol = const_cast<CExpressionArray *>(pdshashed->Pdrgpexpr());
		const ULONG size = pdrgpexprPartCol->Size();
		for (ULONG ul = 0; ul < size; ul++)
		{
			CExpression *pexpr = (*pdrgpexprPartCol)[ul];
			CScalarIdent *popScId = CScalarIdent::PopConvert(pexpr->Pop());
			colids->Append(GPOS_NEW(m_mp) ULONG(popScId->Pcr()->Id()));
		}
	}

	// translate order specification and window frames into window keys
	CDXLWindowKeyArray *pdrgpdxlwk = GPOS_NEW(m_mp) CDXLWindowKeyArray(m_mp);
	COrderSpecArray *pdrgpos = popSeqPrj->Pdrgpos();
	GPOS_ASSERT(NULL != pdrgpos);
	const ULONG ulOsSize = pdrgpos->Size();
	for (ULONG ul = 0; ul < ulOsSize; ul++)
	{
		CDXLWindowKey *pdxlwk = GPOS_NEW(m_mp) CDXLWindowKey(m_mp);
		CDXLNode *sort_col_list_dxlnode = GetSortColListDXL((*popSeqPrj->Pdrgpos())[ul]);
		pdxlwk->SetSortColList(sort_col_list_dxlnode);
		pdrgpdxlwk->Append(pdxlwk);
	}

	const ULONG ulFrames = popSeqPrj->Pdrgpwf()->Size();
	for (ULONG ul = 0; ul < ulFrames; ul++)
	{
		CDXLWindowFrame *window_frame = GetWindowFrame((*popSeqPrj->Pdrgpwf())[ul]);
		if (NULL != window_frame)
		{
			GPOS_ASSERT(ul <= ulOsSize);
			CDXLWindowKey *pdxlwk = (*pdrgpdxlwk)[ul];
			pdxlwk->SetWindowFrame(window_frame);
		}
	}

	// extract physical properties
	CDXLPhysicalProperties *dxl_properties = GetProperties(pexprSeqPrj);

	// translate relational child
	CDXLNode *child_dxlnode = CreateDXLNode((*pexprSeqPrj)[0], NULL /* colref_array */, pdrgpdsBaseTables, pulNonGatherMotions, pfDML, false /*fRemap*/, false /*fRoot*/);

	GPOS_ASSERT(NULL != pexprSeqPrj->Prpp());
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(pexprSeqPrj->Prpp()->PcrsRequired());
	if (NULL != pdrgpexprPartCol)
	{
		CColRefSet *pcrs = CUtils::PcrsExtractColumns(m_mp, pdrgpexprPartCol);
		pcrsOutput->Include(pcrs);
		pcrs->Release();
	}
	for (ULONG ul = 0; ul < ulOsSize; ul++)
	{
		COrderSpec *pos = (*popSeqPrj->Pdrgpos())[ul];
		if (!pos->IsEmpty())
		{
			const CColRef *colref = pos->Pcr(ul);
			pcrsOutput->Include(colref);
		}
	}

	// translate project list expression
	CDXLNode *pdxlnPrL = PdxlnProjList((*pexprSeqPrj)[1], pcrsOutput, colref_array);

	// create an empty one-time filter
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL /* pdxlnCond */);

	// construct a Window node	
	CDXLPhysicalWindow *pdxlopWindow = GPOS_NEW(m_mp) CDXLPhysicalWindow(m_mp, colids, pdrgpdxlwk);
	CDXLNode *pdxlnWindow = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopWindow);
	pdxlnWindow->SetProperties(dxl_properties);

	// add children
	pdxlnWindow->AddChild(pdxlnPrL);
	pdxlnWindow->AddChild(filter_dxlnode);
	pdxlnWindow->AddChild(child_dxlnode);

#ifdef GPOS_DEBUG
	pdxlopWindow->AssertValid(pdxlnWindow, false /* validate_children */);
#endif

	pcrsOutput->Release();

	return pdxlnWindow;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnArray
//
//	@doc:
//		Create a DXL array node from an optimizer array expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnArray
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CScalarArray *pop = CScalarArray::PopConvert(pexpr->Pop());

	IMDId *elem_type_mdid = pop->PmdidElem();
	elem_type_mdid->AddRef();

	IMDId *array_type_mdid = pop->PmdidArray();
	array_type_mdid->AddRef();

	CDXLNode *pdxlnArray =
			GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarArray
									(
									m_mp,
									elem_type_mdid,
									array_type_mdid,
									pop->FMultiDimensional()
									)
						);

	const ULONG arity = CUtils::UlScalarArrayArity(pexpr);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = CUtils::PScalarArrayExprChildAt(m_mp, pexpr, ul);
		CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
		pdxlnArray->AddChild(child_dxlnode);
		pexprChild->Release();
	}

	return pdxlnArray;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnArrayRef
//
//	@doc:
//		Create a DXL arrayref node from an optimizer arrayref expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnArrayRef
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CScalarArrayRef *pop = CScalarArrayRef::PopConvert(pexpr->Pop());

	IMDId *elem_type_mdid = pop->PmdidElem();
	elem_type_mdid->AddRef();

	IMDId *array_type_mdid = pop->PmdidArray();
	array_type_mdid->AddRef();

	IMDId *return_type_mdid = pop->MdidType();
	return_type_mdid->AddRef();

	CDXLNode *pdxlnArrayref =
			GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarArrayRef
									(
									m_mp,
									elem_type_mdid,
									pop->TypeModifier(),
									array_type_mdid,
									return_type_mdid
									)
						);

	TranslateScalarChildren(pexpr, pdxlnArrayref);

	return pdxlnArrayref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnArrayRefIndexList
//
//	@doc:
//		Create a DXL arrayref index list from an optimizer arrayref index list
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnArrayRefIndexList
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CScalarArrayRefIndexList *pop = CScalarArrayRefIndexList::PopConvert(pexpr->Pop());

	CDXLNode *pdxlnIndexlist =
			GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarArrayRefIndexList
									(
									m_mp,
									Eilb(pop->Eilt())
									)
						);

	TranslateScalarChildren(pexpr, pdxlnIndexlist);

	return pdxlnIndexlist;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAssertPredicate
//
//	@doc:
//		Create a DXL assert predicate from an optimizer assert predicate expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAssertPredicate
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	CDXLNode *pdxlnAssertConstraintList = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarAssertConstraintList(m_mp));
	TranslateScalarChildren(pexpr, pdxlnAssertConstraintList);
	return pdxlnAssertConstraintList;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnAssertConstraint
//
//	@doc:
//		Create a DXL assert constraint from an optimizer assert constraint expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnAssertConstraint
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CScalarAssertConstraint *popAssertConstraint = CScalarAssertConstraint::PopConvert(pexpr->Pop());
	CWStringDynamic *pstrErrorMsg = GPOS_NEW(m_mp) CWStringDynamic(m_mp, popAssertConstraint->PstrErrorMsg()->GetBuffer());

	CDXLNode *pdxlnAssertConstraint  = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarAssertConstraint(m_mp, pstrErrorMsg));
	TranslateScalarChildren(pexpr, pdxlnAssertConstraint);
	return pdxlnAssertConstraint;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::Eilb
//
//	@doc:
// 		Translate the arrayref index list bound
//
//---------------------------------------------------------------------------
CDXLScalarArrayRefIndexList::EIndexListBound
CTranslatorExprToDXL::Eilb
	(
	const CScalarArrayRefIndexList::EIndexListType eilt
	)
{
	switch (eilt)
	{
		case CScalarArrayRefIndexList::EiltLower:
			return CDXLScalarArrayRefIndexList::EilbLower;

		case CScalarArrayRefIndexList::EiltUpper:
			return CDXLScalarArrayRefIndexList::EilbUpper;

		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp, GPOS_WSZ_LIT("Invalid arrayref index bound"));
			return CDXLScalarArrayRefIndexList::EilbSentinel;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnArrayCmp
//
//	@doc:
//		Create a DXL array compare node from an optimizer array expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnArrayCmp
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);
	CScalarArrayCmp *pop = CScalarArrayCmp::PopConvert(pexpr->Pop());

	IMDId *mdid_op = pop->MdIdOp();
	mdid_op->AddRef();

	const CWStringConst *str_opname = pop->Pstr();

	CScalarArrayCmp::EArrCmpType earrcmpt = pop->Earrcmpt();
	GPOS_ASSERT(CScalarArrayCmp::EarrcmpSentinel > earrcmpt);
	EdxlArrayCompType edxlarrcmpt = Edxlarraycomptypeall;
	if (CScalarArrayCmp::EarrcmpAny == earrcmpt)
	{
		edxlarrcmpt = Edxlarraycomptypeany;
	}

	CDXLNode *pdxlnArrayCmp =
			GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarArrayComp
									(
									m_mp,
									mdid_op,
									GPOS_NEW(m_mp) CWStringConst(m_mp, str_opname->GetBuffer()),
									edxlarrcmpt
									)
						);

	TranslateScalarChildren(pexpr, pdxlnArrayCmp);

	return pdxlnArrayCmp;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnDMLAction
//
//	@doc:
//		Create a DXL DML action node from an optimizer action expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnDMLAction
	(
	CExpression *
#ifdef GPOS_DEBUG
	pexpr
#endif // GPOS_DEBUG
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(COperator::EopScalarDMLAction == pexpr->Pop()->Eopid());

	return GPOS_NEW(m_mp) CDXLNode
						(
						m_mp,
						GPOS_NEW(m_mp) CDXLScalarDMLAction(m_mp)
						);

}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnScConst
//
//	@doc:
//		Create a DXL scalar constant node from an optimizer scalar const expr.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnScConst
	(
	CExpression *pexprScConst
	)
{
	GPOS_ASSERT(NULL != pexprScConst);

	CScalarConst *popScConst = CScalarConst::PopConvert(pexprScConst->Pop());

	IDatum *datum = popScConst->GetDatum();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDType *pmdtype = md_accessor->RetrieveType(datum->MDId());

	CDXLNode *dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, pmdtype->GetDXLOpScConst(m_mp, datum));

	return dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnFilter
//
//	@doc:
//		Create a DXL filter node containing the given scalar node as a child.
//		If the scalar node is NULL, a filter node with no children is returned
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnFilter
	(
	CDXLNode *pdxlnCond
	)
{
	CDXLNode *filter_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarFilter(m_mp));
	if (NULL != pdxlnCond)
	{
		filter_dxlnode->AddChild(pdxlnCond);
	}

	return filter_dxlnode;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::MakeDXLTableDescr
//
//	@doc:
//		Create a DXL table descriptor from the corresponding optimizer structure
//
//---------------------------------------------------------------------------
CDXLTableDescr *
CTranslatorExprToDXL::MakeDXLTableDescr
	(
	const CTableDescriptor *ptabdesc,
	const CColRefArray *pdrgpcrOutput,
	const CReqdPropPlan *
#ifdef GPOS_DEBUG
	 reqd_prop_plan
#endif
	)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT_IMP(NULL != pdrgpcrOutput, ptabdesc->ColumnCount() == pdrgpcrOutput->Size());

	// get tbl name
	CMDName *pmdnameTbl = GPOS_NEW(m_mp) CMDName(m_mp, ptabdesc->Name().Pstr());

	CMDIdGPDB *mdid = CMDIdGPDB::CastMdid(ptabdesc->MDId());
	mdid->AddRef();

	CDXLTableDescr *table_descr = GPOS_NEW(m_mp) CDXLTableDescr(m_mp, mdid, pmdnameTbl, ptabdesc->GetExecuteAsUserId());

	const ULONG ulColumns = ptabdesc->ColumnCount();
	// translate col descriptors
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		const CColumnDescriptor *pcd = ptabdesc->Pcoldesc(ul);

		GPOS_ASSERT(NULL != pcd);

		// output col ref for the current col descrs
		CColRef *colref = NULL;
		if (NULL != pdrgpcrOutput)
		{
			colref = (*pdrgpcrOutput)[ul];
			if (colref->GetUsage() != CColRef::EUsed)
			{
#ifdef GPOS_DEBUG
				if (NULL != reqd_prop_plan && NULL != reqd_prop_plan->PcrsRequired())
				{
					// ensure that any col removed is not a part of the plan's required cols
					GPOS_ASSERT(!reqd_prop_plan->PcrsRequired()->FMember(colref));
				}
#endif
				continue;
			}
		}
		else
		{
			colref = m_pcf->PcrCreate(pcd->RetrieveType(), pcd->TypeModifier(), pcd->Name());
		}

		CMDName *pmdnameCol = GPOS_NEW(m_mp) CMDName(m_mp, pcd->Name().Pstr());

		// use the col ref id for the corresponding output column as
		// colid for the dxl column
		CMDIdGPDB *pmdidColType = CMDIdGPDB::CastMdid(colref->RetrieveType()->MDId());
		pmdidColType->AddRef();

		CDXLColDescr *pdxlcd = GPOS_NEW(m_mp) CDXLColDescr
											(
											m_mp,
											pmdnameCol,
											colref->Id(),
											pcd->AttrNum(),
											pmdidColType,
											colref->TypeModifier(),
											false /* fdropped */,
											pcd->Width()
											);

		table_descr->AddColumnDescr(pdxlcd);
	}

	return table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetProperties
//
//	@doc:
//		Construct a DXL physical properties container with operator costs for
//		the given expression
//
//---------------------------------------------------------------------------
CDXLPhysicalProperties *
CTranslatorExprToDXL::GetProperties
	(
	const CExpression *pexpr
	)
{

	// extract out rows from statistics object
	CWStringDynamic *rows_out_str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);
	const IStatistics *stats = pexpr->Pstats();
	CDouble rows = CStatistics::DefaultRelationRows;

	// stats may not be present in artificially generated physical expression trees.
	// fill in default statistics
	if (NULL != stats)
	{
		rows = stats->Rows();
	}

	if (CDistributionSpec::EdtReplicated == pexpr->GetDrvdPropPlan()->Pds()->Edt())
	{
		// if distribution is replicated, multiply number of rows by number of segments
		ULONG ulSegments = COptCtxt::PoctxtFromTLS()->GetCostModel()->UlHosts();
		rows = rows * ulSegments;
	}

	rows_out_str->AppendFormat(GPOS_WSZ_LIT("%f"), rows.Get());

	// extract our width from statistics object
	CDouble width = CStatistics::DefaultColumnWidth;
	CReqdPropPlan *prpp = pexpr->Prpp();
	CColRefSet *pcrs = prpp->PcrsRequired();
	ULongPtrArray *colids = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
	pcrs->ExtractColIds(m_mp, colids);
	CWStringDynamic *width_str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);

	if (NULL != stats)
	{
		width = stats->Width(colids);
	}
	colids->Release();
	width_str->AppendFormat(GPOS_WSZ_LIT("%lld"), (LINT) width.Get());

	// get the cost from expression node
	CWStringDynamic str(m_mp);
	COstreamString oss (&str);
	oss << pexpr->Cost();

	CWStringDynamic *pstrStartupcost = GPOS_NEW(m_mp) CWStringDynamic(m_mp, GPOS_WSZ_LIT("0"));
	CWStringDynamic *pstrTotalcost = GPOS_NEW(m_mp) CWStringDynamic(m_mp, str.GetBuffer());

	CDXLOperatorCost *cost = GPOS_NEW(m_mp) CDXLOperatorCost(pstrStartupcost, pstrTotalcost, rows_out_str, width_str);
	CDXLPhysicalProperties *dxl_properties = GPOS_NEW(m_mp) CDXLPhysicalProperties(cost);

	return dxl_properties;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjList
//
//	@doc:
//		Translate the set of output col refs into a dxl project list.
//		If the given array of columns is not NULL, it specifies the order of the 
//		columns in the project list, otherwise any order is good
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjList
	(
	const CColRefSet *pcrsOutput,
	CColRefArray *colref_array
	)
{
	GPOS_ASSERT(NULL != pcrsOutput);

	CDXLScalarProjList *pdxlopPrL = GPOS_NEW(m_mp) CDXLScalarProjList(m_mp);
	CDXLNode *pdxlnPrL = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopPrL);

	if (NULL != colref_array)
	{
		CColRefSet *pcrs= GPOS_NEW(m_mp) CColRefSet(m_mp);

		for (ULONG ul = 0; ul < colref_array->Size(); ul++)
		{
			CColRef *colref = (*colref_array)[ul];

			CDXLNode *pdxlnPrEl = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
			pdxlnPrL->AddChild(pdxlnPrEl);
			pcrs->Include(colref);
		}

		// add the remaining required columns
		CColRefSetIter crsi(*pcrsOutput);
		while(crsi.Advance())
		{
			CColRef *colref = crsi.Pcr();

			if (!pcrs->FMember(colref))
			{
				CDXLNode *pdxlnPrEl = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
				pdxlnPrL->AddChild(pdxlnPrEl);
				pcrs->Include(colref);
			}
		}
		pcrs->Release();
	}
	else
	{
		// no order specified
		CColRefSetIter crsi(*pcrsOutput);
		while(crsi.Advance())
		{
			CColRef *colref = crsi.Pcr();
			CDXLNode *pdxlnPrEl = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
			pdxlnPrL->AddChild(pdxlnPrEl);
		}
	}

	return pdxlnPrL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjList
//
//	@doc:
//		 Translate a project list expression into DXL project list node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjList
	(
	const CExpression *pexprProjList,
	const CColRefSet *pcrsRequired,
	CColRefArray *colref_array
	)
{
	if (NULL == colref_array)
	{
		// no order specified
		return PdxlnProjList(pexprProjList, pcrsRequired);
	}

	// translate computed column expressions into DXL and index them on their col ids
	CHashMap<ULONG, CDXLNode, gpos::HashValue<ULONG>, gpos::Equals<ULONG>, CleanupDelete<ULONG>, CleanupRelease<CDXLNode> >
		*phmComputedColumns = GPOS_NEW(m_mp) CHashMap<ULONG, CDXLNode, gpos::HashValue<ULONG>, gpos::Equals<ULONG>, CleanupDelete<ULONG>, CleanupRelease<CDXLNode> >(m_mp);

	for (ULONG ul = 0; NULL != pexprProjList && ul < pexprProjList->Arity(); ul++)
	{
		CExpression *pexprProjElem = (*pexprProjList)[ul];

		// translate proj elem
		CDXLNode *pdxlnProjElem = PdxlnProjElem(pexprProjElem);

		const CScalarProjectElement *popScPrEl =
				CScalarProjectElement::PopConvert(pexprProjElem->Pop());

		ULONG *pulKey = GPOS_NEW(m_mp) ULONG(popScPrEl->Pcr()->Id());
#ifdef GPOS_DEBUG
		BOOL fInserted =
#endif // GPOS_DEBUG
		phmComputedColumns->Insert(pulKey, pdxlnProjElem);

		GPOS_ASSERT(fInserted);
	}

	// add required columns to the project list
	CColRefArray *pdrgpcrCopy = GPOS_NEW(m_mp) CColRefArray(m_mp);
	pdrgpcrCopy->AppendArray(colref_array);
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(colref_array);
	CColRefSetIter crsi(*pcrsRequired);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		if (!pcrsOutput->FMember(colref))
		{
			pdrgpcrCopy->Append(colref);
		}
	}

	// translate project list according to the specified order
	CDXLScalarProjList *pdxlopPrL = GPOS_NEW(m_mp) CDXLScalarProjList(m_mp);
	CDXLNode *proj_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopPrL);

	const ULONG num_cols = pdrgpcrCopy->Size();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *colref = (*pdrgpcrCopy)[ul];
		ULONG ulKey = colref->Id();
		CDXLNode *pdxlnProjElem = phmComputedColumns->Find(&ulKey);

		if (NULL == pdxlnProjElem)
		{
			// not a computed column
			pdxlnProjElem = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
		}
		else
		{
			pdxlnProjElem->AddRef();
		}

		proj_list_dxlnode->AddChild(pdxlnProjElem);
	}

	// cleanup
	pdrgpcrCopy->Release();
	pcrsOutput->Release();
	phmComputedColumns->Release();

	return proj_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjList
//
//	@doc:
//		 Translate a project list expression into DXL project list node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjList
	(
	const CExpression *pexprProjList,
	const CColRefSet *pcrsRequired
	)
{
	CDXLScalarProjList *pdxlopPrL = GPOS_NEW(m_mp) CDXLScalarProjList(m_mp);
	CDXLNode *proj_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopPrL);

	// create a copy of the required output columns
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp, *pcrsRequired);

	if (NULL != pexprProjList)
	{
		// translate defined columns from project list
		for (ULONG ul = 0; ul < pexprProjList->Arity(); ul++)
		{
			CExpression *pexprProjElem = (*pexprProjList)[ul];
			
			// translate proj elem
			CDXLNode *pdxlnProjElem = PdxlnProjElem(pexprProjElem);
			proj_list_dxlnode->AddChild(pdxlnProjElem);

			// exclude proj elem col ref from the output column set as it has been
			// processed already
			const CScalarProjectElement *popScPrEl =
					CScalarProjectElement::PopConvert(pexprProjElem->Pop());
			pcrsOutput->Exclude(popScPrEl->Pcr());
		}
	}
	
	// translate columns which remained after processing the project list: those
	// are columns passed from the level below
	CColRefSetIter crsi(*pcrsOutput);
	while(crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CDXLNode *pdxlnPrEl = CTranslatorExprToDXLUtils::PdxlnProjElem(m_mp, m_phmcrdxln, colref);
		proj_list_dxlnode->AddChild(pdxlnPrEl);
	}
	
	// cleanup
	pcrsOutput->Release();
	
	return proj_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjListFromConstTableGet
//
//	@doc:
//		Construct a project list node by creating references to the columns
//		of the given project list of the child node 
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjListFromConstTableGet
	(
	CColRefArray *pdrgpcrReqOutput, 
	CColRefArray *pdrgpcrCTGOutput,
	IDatumArray *pdrgpdatumValues
	)
{
	GPOS_ASSERT(NULL != pdrgpcrCTGOutput);
	GPOS_ASSERT(NULL != pdrgpdatumValues);
	GPOS_ASSERT(pdrgpcrCTGOutput->Size() == pdrgpdatumValues->Size());
	
	CDXLNode *proj_list_dxlnode = NULL;
	CColRefSet *pcrsOutput = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrsOutput->Include(pdrgpcrCTGOutput);

	if (NULL != pdrgpcrReqOutput)
	{
		const ULONG arity = pdrgpcrReqOutput->Size();
		IDatumArray *pdrgpdatumOrdered = GPOS_NEW(m_mp) IDatumArray(m_mp);

		for (ULONG ul = 0; ul < arity; ul++)
		{
			CColRef *colref = (*pdrgpcrReqOutput)[ul];
			ULONG ulPos = UlPosInArray(colref, pdrgpcrCTGOutput);
			GPOS_ASSERT(ulPos < pdrgpcrCTGOutput->Size());
			IDatum *datum = (*pdrgpdatumValues)[ulPos];
			datum->AddRef();
			pdrgpdatumOrdered->Append(datum);
			pcrsOutput->Exclude(colref);
		}

		proj_list_dxlnode = PdxlnProjListFromConstTableGet(NULL, pdrgpcrReqOutput, pdrgpdatumOrdered);
		pdrgpdatumOrdered->Release();
	}
	else
	{
		proj_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarProjList(m_mp));
	}

	// construct project elements for columns which remained after processing the required list
	CColRefSetIter crsi(*pcrsOutput);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		ULONG ulPos = UlPosInArray(colref, pdrgpcrCTGOutput);
		GPOS_ASSERT(ulPos < pdrgpcrCTGOutput->Size());
		IDatum *datum = (*pdrgpdatumValues)[ulPos];
		CDXLScalarConstValue *pdxlopConstValue = colref->RetrieveType()->GetDXLOpScConst(m_mp, datum);
		CDXLNode *pdxlnPrEl = PdxlnProjElem(colref, GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopConstValue));
		proj_list_dxlnode->AddChild(pdxlnPrEl);
	}

	// cleanup
	pcrsOutput->Release();

	return proj_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjElem
//
//	@doc:
//		 Create a project elem from a given col ref and a value
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjElem
	(
	const CColRef *colref,
	CDXLNode *pdxlnValue
	)
{
	GPOS_ASSERT(NULL != colref);
	
	CMDName *mdname = GPOS_NEW(m_mp) CMDName(m_mp, colref->Name().Pstr());
	CDXLNode *pdxlnPrEl = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarProjElem(m_mp, colref->Id(), mdname));
	
	// attach scalar id expression to proj elem
	pdxlnPrEl->AddChild(pdxlnValue);
	
	return pdxlnPrEl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnProjElem
//
//	@doc:
//		 Create a project elem from a given col ref
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnProjElem
	(
	const CExpression *pexprProjElem
	)
{
	GPOS_ASSERT(NULL != pexprProjElem && 1 == pexprProjElem->Arity());
	
	CScalarProjectElement *popScPrEl = CScalarProjectElement::PopConvert(pexprProjElem->Pop());
	
	CColRef *colref = popScPrEl->Pcr();
	
	CMDName *mdname = GPOS_NEW(m_mp) CMDName(m_mp, colref->Name().Pstr());
	CDXLNode *pdxlnPrEl = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarProjElem(m_mp, colref->Id(), mdname));
	
	CExpression *pexprChild = (*pexprProjElem)[0];
	CDXLNode *child_dxlnode = PdxlnScalar(pexprChild);
	
	pdxlnPrEl->AddChild(child_dxlnode);
	
	return pdxlnPrEl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetSortColListDXL
//
//	@doc:
//		 Create a dxl sort column list node from a given order spec
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::GetSortColListDXL
	(
	const COrderSpec *pos
	)
{
	GPOS_ASSERT(NULL != pos);
	
	CDXLNode *sort_col_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSortColList(m_mp));
	
	for (ULONG ul = 0; ul < pos->UlSortColumns(); ul++)
	{
		// get sort column components
		IMDId *sort_op_id = pos->GetMdIdSortOp(ul);
		sort_op_id->AddRef();
		
		const CColRef *colref = pos->Pcr(ul);
		
		COrderSpec::ENullTreatment ent = pos->Ent(ul);
		GPOS_ASSERT(COrderSpec::EntFirst == ent || COrderSpec::EntLast == ent || COrderSpec::EntAuto == ent);
		
		// get sort operator name
		const IMDScalarOp *md_scalar_op = m_pmda->RetrieveScOp(sort_op_id);
		
		CWStringConst *sort_op_name = 
				GPOS_NEW(m_mp) CWStringConst(m_mp, md_scalar_op->Mdname().GetMDName()->GetBuffer());
		
		BOOL fSortNullsFirst = false;
		if (COrderSpec::EntFirst == ent)
		{
			fSortNullsFirst = true;
		}

		CDXLScalarSortCol *pdxlopSortCol =
				GPOS_NEW(m_mp) CDXLScalarSortCol
							(
							m_mp,
							colref->Id(), 
							sort_op_id,
							sort_op_name,
							fSortNullsFirst
							);
		
		CDXLNode *pdxlnSortCol = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopSortCol);
		sort_col_list_dxlnode->AddChild(pdxlnSortCol);
	}
	
	return sort_col_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::PdxlnHashExprList
//
//	@doc:
//		 Create a dxl hash expr list node from a given array of column references
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::PdxlnHashExprList
	(
	const CExpressionArray *pdrgpexpr,
	const IMdIdArray *opfamilies
	)
{
	GPOS_ASSERT(NULL != pdrgpexpr);
	GPOS_ASSERT_IMP(GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution), NULL != opfamilies);
	
	CDXLNode *hash_expr_list = 
			GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarHashExprList(m_mp));
	
	for (ULONG ul = 0; ul < pdrgpexpr->Size(); ul++)
	{
		CExpression *pexpr = (*pdrgpexpr)[ul];

		IMDId *opfamily = NULL;
		if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution))
		{
			opfamily = (*opfamilies)[ul];
			opfamily->AddRef();
		}

		// constrct a hash expr node for the col ref
		CDXLNode *pdxlnHashExpr = GPOS_NEW(m_mp) CDXLNode
												(
												m_mp,
												GPOS_NEW(m_mp) CDXLScalarHashExpr
															(
															m_mp,
															opfamily
															)
												);
												
		pdxlnHashExpr->AddChild(PdxlnScalar(pexpr));
		
		hash_expr_list->AddChild(pdxlnHashExpr);
	}
	
	return hash_expr_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetSortColListDXL
//
//	@doc:
//		 Create a dxl sort column list node for a given motion operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXL::GetSortColListDXL
	(
	CExpression *pexprMotion
	)
{
	CDXLNode *sort_col_list_dxlnode = NULL;
	if (COperator::EopPhysicalMotionGather == pexprMotion->Pop()->Eopid())
	{
		// construct a sorting column list node
		CPhysicalMotionGather *popGather = CPhysicalMotionGather::PopConvert(pexprMotion->Pop());
		sort_col_list_dxlnode = GetSortColListDXL(popGather->Pos());
	}
	else
	{
		sort_col_list_dxlnode = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarSortColList(m_mp));
	}
	
	return sort_col_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetOutputSegIdsArray
//
//	@doc:
//		Construct an array with output segment indices for the given Motion
//		expression.
//
//---------------------------------------------------------------------------
IntPtrArray *
CTranslatorExprToDXL::GetOutputSegIdsArray
	(
	CExpression *pexprMotion
	)
{
	IntPtrArray *pdrgpi = NULL;
	
	COperator *pop = pexprMotion->Pop();
	
	switch (pop->Eopid())
	{
		case COperator::EopPhysicalMotionGather:
		{
			CPhysicalMotionGather *popGather = CPhysicalMotionGather::PopConvert(pop);
		
			pdrgpi = GPOS_NEW(m_mp) IntPtrArray(m_mp);
			INT iSegmentId = m_iMasterId;
			
			if (CDistributionSpecSingleton::EstSegment == popGather->Est())
			{
				// gather to first segment
				iSegmentId = *((*m_pdrgpiSegments)[0]);
			}
			pdrgpi->Append(GPOS_NEW(m_mp) INT(iSegmentId));
			break;
		}
		case COperator::EopPhysicalMotionBroadcast:
		case COperator::EopPhysicalMotionHashDistribute:
		case COperator::EopPhysicalMotionRoutedDistribute:
		case COperator::EopPhysicalMotionRandom:
		{
			m_pdrgpiSegments->AddRef();
			pdrgpi = m_pdrgpiSegments;
			break;
		}
		default:
			GPOS_ASSERT(!"Unrecognized motion operator");
	}
	
	GPOS_ASSERT(NULL != pdrgpi);
	
	return pdrgpi;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::GetInputSegIdsArray
//
//	@doc:
//		Construct an array with input segment indices for the given Motion
//		expression.
//
//---------------------------------------------------------------------------
IntPtrArray *
CTranslatorExprToDXL::GetInputSegIdsArray
	(
	CExpression *pexprMotion
	)
{
	GPOS_ASSERT(1 == pexprMotion->Arity());

	// derive the distribution of child expression
	CExpression *pexprChild = (*pexprMotion)[0];
	CDrvdPropPlan *pdpplan = CDrvdPropPlan::Pdpplan(pexprChild->PdpDerive());
	CDistributionSpec *pds = pdpplan->Pds();

	if (CDistributionSpec::EdtSingleton == pds->Edt() || CDistributionSpec::EdtStrictSingleton == pds->Edt())
	{
		IntPtrArray *pdrgpi = GPOS_NEW(m_mp) IntPtrArray(m_mp);
		INT iSegmentId = m_iMasterId;
		CDistributionSpecSingleton *pdss = CDistributionSpecSingleton::PdssConvert(pds);
		if (!pdss->FOnMaster())
		{
			// non-master singleton is currently fixed to the first segment
			iSegmentId = *((*m_pdrgpiSegments)[0]);
		}
		pdrgpi->Append(GPOS_NEW(m_mp) INT(iSegmentId));
		return pdrgpi;
	}

	if (CUtils::FDuplicateHazardMotion(pexprMotion))
	{
		// if Motion is duplicate-hazard, we have to read from one input segment
		// to avoid generating duplicate values
		IntPtrArray *pdrgpi = GPOS_NEW(m_mp) IntPtrArray(m_mp);
		INT iSegmentId = *((*m_pdrgpiSegments)[0]);
		pdrgpi->Append(GPOS_NEW(m_mp) INT(iSegmentId));
		return pdrgpi;
	}

	m_pdrgpiSegments->AddRef();
	return m_pdrgpiSegments;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXL::UlPosInArray
//
//	@doc:
//		Find position of colref in the array
//
//---------------------------------------------------------------------------
ULONG
CTranslatorExprToDXL::UlPosInArray
	(
	const CColRef *colref,
	const CColRefArray *colref_array
	)
	const
{
	GPOS_ASSERT(NULL != colref_array);
	GPOS_ASSERT(NULL != colref);
	
	const ULONG size = colref_array->Size();
	
	for (ULONG ul = 0; ul < size; ul++)
	{
		if (colref == (*colref_array)[ul])
		{
			return ul;
		}
	}
	
	// not found
	return size;
}

// A wrapper around CTranslatorExprToDXLUtils::PdxlnResult to check if the project list imposes a motion hazard,
// eventually leading to a deadlock. If yes, add a Materialize on the Result child to break the deadlock cycle
CDXLNode *
CTranslatorExprToDXL::PdxlnResult
	(
	CDXLPhysicalProperties *dxl_properties,
	CDXLNode *pdxlnPrL,
	CDXLNode *child_dxlnode
	)
{
	CDXLNode *pdxlnMaterialize = NULL;
	CDXLNode *pdxlnScalarOneTimeFilter = GPOS_NEW(m_mp) CDXLNode(m_mp, GPOS_NEW(m_mp) CDXLScalarOneTimeFilter(m_mp));

	// If the result project list contains a subplan with a Broadcast motion,
	// along with other projections from the result's child node with a motion as well,
	// it may result in a deadlock. In such cases, add a materialize node.
	if (FNeedsMaterializeUnderResult(pdxlnPrL, child_dxlnode))
	{
		pdxlnMaterialize = PdxlnMaterialize(child_dxlnode);
	}

	return CTranslatorExprToDXLUtils::PdxlnResult
										(
										 m_mp,
										 dxl_properties,
										 pdxlnPrL,
										 PdxlnFilter(NULL),
										 pdxlnScalarOneTimeFilter,
										 pdxlnMaterialize ? pdxlnMaterialize: child_dxlnode
										 );
}

CDXLNode *
CTranslatorExprToDXL::PdxlnMaterialize
	(
	CDXLNode *dxlnode // node that needs to be materialized
	)
{
	GPOS_ASSERT(NULL != dxlnode);
	GPOS_ASSERT(NULL != dxlnode->GetProperties());

	CDXLPhysicalMaterialize *pdxlopMaterialize = GPOS_NEW(m_mp) CDXLPhysicalMaterialize(m_mp, true /* fEager */);
	CDXLNode *pdxlnMaterialize = GPOS_NEW(m_mp) CDXLNode(m_mp, pdxlopMaterialize);
	CDXLPhysicalProperties *pdxlpropChild = CDXLPhysicalProperties::PdxlpropConvert(dxlnode->GetProperties());
	pdxlpropChild->AddRef();
	pdxlnMaterialize->SetProperties(pdxlpropChild);

	// construct an empty filter node
	CDXLNode *filter_dxlnode = PdxlnFilter(NULL /* pdxlnCond */);

	CDXLNode *pdxlnProjListChild = (*dxlnode)[0];
	CDXLNode *proj_list_dxlnode = CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(m_mp, m_pcf, m_phmcrdxln, pdxlnProjListChild);

	// add children
	pdxlnMaterialize->AddChild(proj_list_dxlnode);
	pdxlnMaterialize->AddChild(filter_dxlnode);
	pdxlnMaterialize->AddChild(dxlnode);
	return pdxlnMaterialize;
}

BOOL
CTranslatorExprToDXL::FNeedsMaterializeUnderResult
	(
	CDXLNode *proj_list_dxlnode,
	CDXLNode *child_dxlnode
	)
{
	if(GPOS_FTRACE(EopttraceMotionHazardHandling))
	{
		// If motion hazard handling is enabled then optimization framework has
		// already handled this, hence no materialize is needed in this case.
		return false;
	}

	BOOL fMotionHazard = false;

	// if there is no subplan with a broadcast motion in the project list,
	// then don't bother checking for motion hazard
	BOOL fPrjListContainsSubplan = CTranslatorExprToDXLUtils::FProjListContainsSubplanWithBroadCast(proj_list_dxlnode);

	if (fPrjListContainsSubplan)
	{
		CBitSet *pbsScIdentColIds = GPOS_NEW(m_mp) CBitSet(m_mp);

		// recurse into project elements to extract out columns ids of scalar idents
		CTranslatorExprToDXLUtils::ExtractIdentColIds(proj_list_dxlnode, pbsScIdentColIds);

		// result node will impose motion hazard only if it projects a Subplan
		// and an Ident produced by a tree that contains a motion
		if (pbsScIdentColIds->Size() > 0)
		{
			// motions which can impose a hazard
			gpdxl::Edxlopid rgeopid[] = {
				EdxlopPhysicalMotionBroadcast,
				EdxlopPhysicalMotionRedistribute,
				EdxlopPhysicalMotionRandom,
			};

			fMotionHazard = CTranslatorExprToDXLUtils::FMotionHazard(
																	 m_mp,
																	 child_dxlnode,
																	 rgeopid,
																	 GPOS_ARRAY_SIZE(rgeopid),
																	 pbsScIdentColIds);
		}
		pbsScIdentColIds->Release();
	}
	return fMotionHazard;
}
// EOF
