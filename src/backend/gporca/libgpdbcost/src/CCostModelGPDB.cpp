//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CCostModelGPDB.cpp
//
//	@doc:
//		Implementation of GPDB cost model
//---------------------------------------------------------------------------

#include "gpdbcost/CCostModelGPDB.h"

#include <limits>

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/base/CWindowFrame.h"
#include "gpopt/engine/CHint.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalDynamicIndexOnlyScan.h"
#include "gpopt/operators/CPhysicalDynamicIndexScan.h"
#include "gpopt/operators/CPhysicalHashAgg.h"
#include "gpopt/operators/CPhysicalIndexOnlyScan.h"
#include "gpopt/operators/CPhysicalIndexScan.h"
#include "gpopt/operators/CPhysicalMotion.h"
#include "gpopt/operators/CPhysicalMotionBroadcast.h"
#include "gpopt/operators/CPhysicalPartitionSelector.h"
#include "gpopt/operators/CPhysicalSequenceProject.h"
#include "gpopt/operators/CPhysicalStreamAgg.h"
#include "gpopt/operators/CPhysicalUnionAll.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarBitmapIndexProbe.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpos;
using namespace gpdbcost;


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CCostModelGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CCostModelGPDB::CCostModelGPDB(CMemoryPool *mp, ULONG ulSegments,
							   CCostModelParamsGPDB *pcp)
	: m_mp(mp), m_num_of_segments(ulSegments)
{
	GPOS_ASSERT(0 < ulSegments);

	if (nullptr == pcp)
	{
		m_cost_model_params = GPOS_NEW(mp) CCostModelParamsGPDB(mp);
	}
	else
	{
		GPOS_ASSERT(nullptr != pcp);

		m_cost_model_params = pcp;
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::DRowsPerHost
//
//	@doc:
//		Return number of rows per host
//
//---------------------------------------------------------------------------
CDouble
CCostModelGPDB::DRowsPerHost(CDouble dRowsTotal) const
{
	return dRowsTotal / m_num_of_segments;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::~CCostModelGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CCostModelGPDB::~CCostModelGPDB()
{
	m_cost_model_params->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostTupleProcessing
//
//	@doc:
//		Cost of tuple processing
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostTupleProcessing(DOUBLE rows, DOUBLE width,
									ICostModelParams *pcp)
{
	GPOS_ASSERT(nullptr != pcp);

	const CDouble dTupDefaultProcCostUnit =
		pcp->PcpLookup(CCostModelParamsGPDB::EcpTupDefaultProcCostUnit)->Get();
	GPOS_ASSERT(0 < dTupDefaultProcCostUnit);

	return CCost(rows * width * dTupDefaultProcCostUnit);
}


//
//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostScanOutput
//
//	@doc:
//		Helper function to return cost of producing output tuples from
//		Scan operator
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostScanOutput(CMemoryPool *,  // mp
							   DOUBLE rows, DOUBLE width, DOUBLE num_rebinds,
							   ICostModelParams *pcp)
{
	GPOS_ASSERT(nullptr != pcp);

	const CDouble dOutputTupCostUnit =
		pcp->PcpLookup(CCostModelParamsGPDB::EcpOutputTupCostUnit)->Get();
	GPOS_ASSERT(0 < dOutputTupCostUnit);

	return CCost(num_rebinds * (rows * width * dOutputTupCostUnit));
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostComputeScalar
//
//	@doc:
//		Helper function to return cost of a plan containing compute scalar
//		operator
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostComputeScalar(CMemoryPool *mp, CExpressionHandle &exprhdl,
								  const SCostingInfo *pci,
								  ICostModelParams *pcp,
								  const CCostModelGPDB *pcmgpdb)
{
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(nullptr != pcp);
	GPOS_ASSERT(nullptr != pcmgpdb);

	DOUBLE rows = pci->Rows();
	DOUBLE width = pci->Width();
	DOUBLE num_rebinds = pci->NumRebinds();

	CCost costLocal =
		CCost(num_rebinds * CostTupleProcessing(rows, width, pcp).Get());
	CCost costChild = CostChildren(mp, exprhdl, pci, pcp);

	CCost costCompute(0);

	if (exprhdl.DeriveHasScalarFuncProject(1))
	{
		// If the compute scalar operator has a scalar func operator in the
		// project list then aggregate that cost of the scalar func. The number
		// of times the scalar func is run is proportional to the number of
		// rows.
		costCompute =
			CCost(pcmgpdb->GetCostModelParams()
					  ->PcpLookup(CCostModelParamsGPDB::EcpScalarFuncCost)
					  ->Get() *
				  rows);
	}

	return costLocal + costChild + costCompute;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostUnary
//
//	@doc:
//		Helper function to return cost of a plan rooted by unary operator
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostUnary(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  const SCostingInfo *pci, ICostModelParams *pcp)
{
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(nullptr != pcp);

	DOUBLE rows = pci->Rows();
	DOUBLE width = pci->Width();
	DOUBLE num_rebinds = pci->NumRebinds();

	CCost costLocal =
		CCost(num_rebinds * CostTupleProcessing(rows, width, pcp).Get());
	CCost costChild = CostChildren(mp, exprhdl, pci, pcp);

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostSpooling
//
//	@doc:
//		Helper function to compute spooling cost
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostSpooling(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 const SCostingInfo *pci, ICostModelParams *pcp)
{
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(nullptr != pcp);

	const CDouble dMaterializeCostUnit =
		pcp->PcpLookup(CCostModelParamsGPDB::EcpMaterializeCostUnit)->Get();
	GPOS_ASSERT(0 < dMaterializeCostUnit);

	DOUBLE rows = pci->Rows();
	DOUBLE width = pci->Width();
	DOUBLE num_rebinds = pci->NumRebinds();

	// materialization cost is correlated with the number of rows and width of returning tuples.
	CCost costLocal =
		CCost(num_rebinds * (width * rows * dMaterializeCostUnit));
	CCost costChild = CostChildren(mp, exprhdl, pci, pcp);

	return costLocal + costChild;
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::FUnary
//
//	@doc:
//		Check if given operator is unary
//
//---------------------------------------------------------------------------
BOOL
CCostModelGPDB::FUnary(COperator::EOperatorId op_id)
{
	return COperator::EopPhysicalAssert == op_id ||
		   COperator::EopPhysicalComputeScalar == op_id ||
		   COperator::EopPhysicalLimit == op_id ||
		   COperator::EopPhysicalPartitionSelector == op_id ||
		   COperator::EopPhysicalSplit == op_id ||
		   COperator::EopPhysicalSpool == op_id;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostChildren
//
//	@doc:
//		Add up children costs
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostChildren(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 const SCostingInfo *pci, ICostModelParams *pcp)
{
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(nullptr != pcp);

	DOUBLE *pdCost = pci->PdCost();
	const ULONG size = pci->ChildCount();
	BOOL fFilterParent =
		(COperator::EopPhysicalFilter == exprhdl.Pop()->Eopid());

	DOUBLE res = 0.0;
	for (ULONG ul = 0; ul < size; ul++)
	{
		DOUBLE dCostChild = pdCost[ul];
		COperator *popChild = exprhdl.Pop(ul);
		if (nullptr != popChild &&
			(CUtils::FPhysicalScan(popChild) ||
			 COperator::EopPhysicalPartitionSelector == popChild->Eopid()))
		{
			// by default, compute scan output cost based on full Scan
			DOUBLE dScanRows = pci->PdRows()[ul];
			COperator *scanOp = popChild;

			if (fFilterParent)
			{
				CPhysicalPartitionSelector *ps =
					dynamic_cast<CPhysicalPartitionSelector *>(popChild);

				if (ps)
				{
					CCostContext *grandchildContext = nullptr;

					scanOp = exprhdl.PopGrandchild(ul, 0, &grandchildContext);
					CPhysicalDynamicScan *scan =
						dynamic_cast<CPhysicalDynamicScan *>(scanOp);

					if (scan && scan->ScanId() == ps->ScanId() &&
						grandchildContext)
					{
						// We have a filter on top of a partition selector on top of a scan.
						// Base the scan output cost on the combination (filter + part sel + scan)
						// on the rows that are produced by the scan, since the runtime execution
						// plan with be sequence(part_sel, scan+filter). Note that the cost of
						// the partition selector is ignored here. It may be higher than that of
						// the complete tree (filter + part sel + scan).
						// See method CTranslatorExprToDXL::PdxlnPartitionSelectorWithInlinedCondition()
						// for how we inline the predicate into the dynamic table scan.
						dCostChild = grandchildContext->Cost().Get();
						dScanRows = pci->Rows();
					}
				}
				else
				{
					// if parent is filter, compute scan output cost based on rows produced by Filter operator
					dScanRows = pci->Rows();
				}
			}

			if (CUtils::FPhysicalScan(scanOp))
			{
				// Note: We assume that width and rebinds are the same for scan, partition selector and filter
				dCostChild = dCostChild +
							 CostScanOutput(mp, dScanRows, pci->GetWidth()[ul],
											pci->PdRebinds()[ul], pcp)
								 .Get();
			}
		}

		res = res + dCostChild;
	}

	return CCost(res);
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostMaxChild
//
//	@doc:
//		Returns cost of highest costed child
//
//---------------------------------------------------------------------------

CCost
CCostModelGPDB::CostMaxChild(CMemoryPool *, CExpressionHandle &,
							 const SCostingInfo *pci, ICostModelParams *)
{
	GPOS_ASSERT(nullptr != pci);

	DOUBLE *pdCost = pci->PdCost();
	const ULONG size = pci->ChildCount();

	DOUBLE res = 0.0;
	for (ULONG ul = 0; ul < size; ul++)
	{
		if (pdCost[ul] > res)
		{
			res = pdCost[ul];
		}
	}

	return CCost(res);
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostCTEProducer
//
//	@doc:
//		Cost of CTE producer
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostCTEProducer(CMemoryPool *mp, CExpressionHandle &exprhdl,
								const CCostModelGPDB *pcmgpdb,
								const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalCTEProducer == exprhdl.Pop()->Eopid());

	CCost cost = CostUnary(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	// In GPDB, the child of a ShareInputScan representing the producer can
	// only be a materialize or sort. Here, we check if a materialize node
	// needs to be added during DXL->PlStmt translation

	COperator *popChild = exprhdl.Pop(0 /*child_index*/);
	if (nullptr == popChild)
	{
		// child operator is not known, this could happen when computing cost bound
		return cost;
	}

	COperator::EOperatorId op_id = popChild->Eopid();
	if (COperator::EopPhysicalSpool != op_id &&
		COperator::EopPhysicalSort != op_id)
	{
		// no materialize needed
		return cost;
	}

	// a materialize (spool) node is added during DXL->PlStmt translation,
	// we need to add the cost of writing the tuples to disk
	const CDouble dMaterializeCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpMaterializeCostUnit)
			->Get();
	GPOS_ASSERT(0 < dMaterializeCostUnit);

	CCost costSpooling = CCost(pci->NumRebinds() * (pci->Rows() * pci->Width() *
													dMaterializeCostUnit));

	return cost + costSpooling;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostCTEConsumer
//
//	@doc:
//		Cost of CTE consumer
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostCTEConsumer(CMemoryPool *,	// mp
								CExpressionHandle &
#ifdef GPOS_DEBUG
									exprhdl
#endif	// GPOS_DEBUG
								,
								const CCostModelGPDB *pcmgpdb,
								const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalCTEConsumer == exprhdl.Pop()->Eopid());

	const CDouble dInitScan =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpInitScanFactor)
			->Get();
	const CDouble dTableScanCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTableScanCostUnit)
			->Get();
	const CDouble dOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpOutputTupCostUnit)
			->Get();
	GPOS_ASSERT(0 < dOutputTupCostUnit);
	GPOS_ASSERT(0 < dTableScanCostUnit);

	return CCost(pci->NumRebinds() *
				 (dInitScan + pci->Rows() * pci->Width() *
								  (dTableScanCostUnit + dOutputTupCostUnit)));
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostConstTableGet
//
//	@doc:
//		Cost of const table get
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostConstTableGet(CMemoryPool *,  // mp
								  CExpressionHandle &
#ifdef GPOS_DEBUG
									  exprhdl
#endif	// GPOS_DEBUG
								  ,
								  const CCostModelGPDB *pcmgpdb,
								  const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalConstTableGet == exprhdl.Pop()->Eopid());

	return CCost(pci->NumRebinds() *
				 CostTupleProcessing(pci->Rows(), pci->Width(),
									 pcmgpdb->GetCostModelParams())
					 .Get());
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostDML
//
//	@doc:
//		Cost of DML
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostDML(CMemoryPool *mp, CExpressionHandle &exprhdl,
						const CCostModelGPDB *pcmgpdb, const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalDML == exprhdl.Pop()->Eopid());

	const CDouble dTupUpdateBandwidth =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTupUpdateBandwith)
			->Get();
	GPOS_ASSERT(0 < dTupUpdateBandwidth);

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];

	CCost costLocal = CCost(pci->NumRebinds() * (num_rows_outer * dWidthOuter) /
							dTupUpdateBandwidth);
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostScalarAgg
//
//	@doc:
//		Cost of scalar agg
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostScalarAgg(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  const CCostModelGPDB *pcmgpdb,
							  const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalScalarAgg == exprhdl.Pop()->Eopid());

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];

	// get the number of aggregate columns
	const ULONG ulAggCols = exprhdl.DeriveUsedColumns(1)->Size();
	// get the number of aggregate functions
	const ULONG ulAggFunctions = exprhdl.PexprScalarRepChild(1)->Arity();

	const CDouble dHashAggInputTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHashAggInputTupWidthCostUnit)
			->Get();
	GPOS_ASSERT(0 < dHashAggInputTupWidthCostUnit);

	// scalarAgg cost is correlated with rows and width of the input tuples and the number of columns used in aggregate
	// It also depends on the complexity of the aggregate algorithm, which is hard to model yet shared by all the aggregate
	// operators, thus we ignore this part of cost for all.
	CCost costLocal = CCost(pci->NumRebinds() *
							(num_rows_outer * dWidthOuter * ulAggCols *
							 ulAggFunctions * dHashAggInputTupWidthCostUnit));

	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());
	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostStreamAgg
//
//	@doc:
//		Cost of stream agg
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostStreamAgg(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  const CCostModelGPDB *pcmgpdb,
							  const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalStreamAgg == op_id ||
				COperator::EopPhysicalStreamAggDeduplicate == op_id);
#endif	// GPOS_DEBUG

	const CDouble dHashAggOutputTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHashAggOutputTupWidthCostUnit)
			->Get();
	const CDouble dTupDefaultProcCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTupDefaultProcCostUnit)
			->Get();
	GPOS_ASSERT(0 < dHashAggOutputTupWidthCostUnit);
	GPOS_ASSERT(0 < dTupDefaultProcCostUnit);

	DOUBLE num_input_rows = pci->PdRows()[0];  // estimated input rows
	DOUBLE dWidthOuter = pci->GetWidth()[0];

	// In order to handle worst-case scenarios where grouping key tuples
	// are distributed across segments, and to maintain accurate
	// cardinality for local stream agg, it is crucial to ensure that the
	// local stream agg's cardinality does not exceed the NDV of the
	// grouping key in global agg (upper bound for the number of output
	// rows). This is achieved by multiplying the global agg's cardinality
	// of the grouping key with the number of segments. It can helps to
	// maintain the cardinality for local stream agg in both best and
	// worst-case scenarios.

	DOUBLE num_output_rows = pci->Rows();  // estimated output rows
	CPhysicalStreamAgg *popAgg = CPhysicalStreamAgg::PopConvert(exprhdl.Pop());
	if ((COperator::EgbaggtypeLocal == popAgg->Egbaggtype()) &&
		popAgg->FGeneratesDuplicates())
	{
		num_output_rows = num_output_rows * pcmgpdb->UlHosts();
	}

	// streamAgg cost is correlated with num_input_rows and width of input
	// tuples and num_output_rows and width of output tuples CCost
	CCost costLocal = CCost(
		pci->NumRebinds() *
		(num_input_rows * dWidthOuter * dTupDefaultProcCostUnit +
		 num_output_rows * pci->Width() * dHashAggOutputTupWidthCostUnit));
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());
	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostSequence
//
//	@doc:
//		Cost of sequence
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostSequence(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 const CCostModelGPDB *pcmgpdb,
							 const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalSequence == exprhdl.Pop()->Eopid());

	CCost costLocal = CCost(pci->NumRebinds() *
							CostTupleProcessing(pci->Rows(), pci->Width(),
												pcmgpdb->GetCostModelParams())
								.Get());
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostSort
//
//	@doc:
//		Cost of sort
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostSort(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 const CCostModelGPDB *pcmgpdb, const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalSort == exprhdl.Pop()->Eopid());

	// log operation below
	// Minimim value of of 2 is used for rows.
	// With 1 as min rows, costLocal is 0, which results in sort expression being used redundantly.
	const CDouble rows = CDouble(std::max(2.0, pci->Rows()));
	const CDouble num_rebinds = CDouble(pci->NumRebinds());
	const CDouble width = CDouble(pci->Width());

	const CDouble dSortTupWidthCost =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpSortTupWidthCostUnit)
			->Get();
	GPOS_ASSERT(0 < dSortTupWidthCost);

	// sort cost is correlated with the number of rows and width of input tuples. We use n*log(n) for sorting complexity.
	CCost costLocal =
		CCost(num_rebinds * (rows * rows.Log2() * width * dSortTupWidthCost));
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostTVF
//
//	@doc:
//		Cost of table valued function
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostTVF(CMemoryPool *,	// mp
						CExpressionHandle &
#ifdef GPOS_DEBUG
							exprhdl
#endif	// GPOS_DEBUG
						,
						const CCostModelGPDB *pcmgpdb, const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalTVF == exprhdl.Pop()->Eopid());

	return CCost(pci->NumRebinds() *
				 CostTupleProcessing(pci->Rows(), pci->Width(),
									 pcmgpdb->GetCostModelParams())
					 .Get());
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostUnionAll
//
//	@doc:
//		Cost of UnionAll
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostUnionAll(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 const CCostModelGPDB *pcmgpdb,
							 const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(nullptr != CPhysicalUnionAll::PopConvert(exprhdl.Pop()));

	if (COperator::EopPhysicalParallelUnionAll == exprhdl.Pop()->Eopid())
	{
		return CostMaxChild(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());
	}

	CCost costLocal = CCost(pci->NumRebinds() *
							CostTupleProcessing(pci->Rows(), pci->Width(),
												pcmgpdb->GetCostModelParams())
								.Get());
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostHashAgg
//
//	@doc:
//		Cost of hash agg
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostHashAgg(CMemoryPool *mp, CExpressionHandle &exprhdl,
							const CCostModelGPDB *pcmgpdb,
							const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalHashAgg == op_id ||
				COperator::EopPhysicalHashAggDeduplicate == op_id);
#endif	// GPOS_DEBUG

	DOUBLE num_input_rows = pci->PdRows()[0];  // estimated input rows

	// A local hash agg may stream partial aggregates to global agg when
	// it's hash table is full to avoid spilling.  This is dertermined by
	// the order of tuples received by local agg. In the worst case, the
	// local hash agg may see a tuple from each different group until its
	// hash table fills up all available memory, and hence it produces
	// tuples as many as its input size. On the other hand, in the best
	// case, the local agg may receive tuples sorted by grouping columns,
	// which allows it to complete all local aggregation in memory and
	// produce exactly tuples as the number of groups.
	//
	// Considering the tuples of local hash agg fit within memory. To
	// handle worst-case scenarios where the tuples of the grouping key are
	// distributed across segments. To maintain accurate cardinality for
	// local hash agg, its crucial to ensure that the cardinality of the
	// local hash agg does not exceed the NDV of the grouping key in global
	// aggs (upper bound for the number of output rows).  So we are
	// achieving this by multiplying the global agg's cardinality of grouping
	// key with the number of segments. It can help's to maintain the
	// cardinality for local hash agg across both best and worst-case
	// scenarios.

	DOUBLE num_output_rows = pci->Rows();  // estimated output rows
	CPhysicalHashAgg *popAgg = CPhysicalHashAgg::PopConvert(exprhdl.Pop());
	if ((COperator::EgbaggtypeLocal == popAgg->Egbaggtype()) &&
		popAgg->FGeneratesDuplicates())
	{
		num_output_rows = num_output_rows * pcmgpdb->UlHosts();
	}

	// get the number of grouping columns
	const ULONG ulGrpCols = CPhysicalHashAgg::PopConvert(exprhdl.Pop())
								->PdrgpcrGroupingCols()
								->Size();

	const CDouble dHashAggInputTupColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHashAggInputTupColumnCostUnit)
			->Get();
	const CDouble dHashAggInputTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHashAggInputTupWidthCostUnit)
			->Get();
	const CDouble dHashAggOutputTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHashAggOutputTupWidthCostUnit)
			->Get();
	GPOS_ASSERT(0 < dHashAggInputTupColumnCostUnit);
	GPOS_ASSERT(0 < dHashAggInputTupWidthCostUnit);
	GPOS_ASSERT(0 < dHashAggOutputTupWidthCostUnit);

	// hashAgg cost contains three parts: build hash table, aggregate tuples, and output tuples.
	// 1. build hash table is correlated with the number of num_input_rows
	// and width of input tuples and the number of columns used.
	// 2. cost of aggregate tuples depends on the complexity of aggregation
	// algorithm and thus is ignored.
	// 3. cost of output tuples is correlated with num_output_rows and
	// width of returning tuples.
	CCost costLocal = CCost(
		pci->NumRebinds() *
		(num_input_rows * ulGrpCols * dHashAggInputTupColumnCostUnit +
		 num_input_rows * ulGrpCols * pci->Width() *
			 dHashAggInputTupWidthCostUnit +
		 num_output_rows * pci->Width() * dHashAggOutputTupWidthCostUnit));
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostHashJoin
//
//	@doc:
//		Cost of hash join
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostHashJoin(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 const CCostModelGPDB *pcmgpdb,
							 const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalInnerHashJoin == op_id ||
				COperator::EopPhysicalLeftSemiHashJoin == op_id ||
				COperator::EopPhysicalLeftAntiSemiHashJoin == op_id ||
				COperator::EopPhysicalLeftAntiSemiHashJoinNotIn == op_id ||
				COperator::EopPhysicalLeftOuterHashJoin == op_id ||
				COperator::EopPhysicalRightOuterHashJoin == op_id);
#endif	// GPOS_DEBUG

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];
	const DOUBLE dRowsInner = pci->PdRows()[1];
	const DOUBLE dWidthInner = pci->GetWidth()[1];

	const CDouble dHJHashTableInitCostFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHJHashTableInitCostFactor)
			->Get();
	const CDouble dHJHashTableColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHJHashTableColumnCostUnit)
			->Get();
	const CDouble dHJHashTableWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHJHashTableWidthCostUnit)
			->Get();
	const CDouble dJoinFeedingTupColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupColumnCostUnit)
			->Get();
	const CDouble dJoinFeedingTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupWidthCostUnit)
			->Get();
	const CDouble dHJHashingTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHJHashingTupWidthCostUnit)
			->Get();
	const CDouble dJoinOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinOutputTupCostUnit)
			->Get();
	const CDouble dHJSpillingMemThreshold =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpHJSpillingMemThreshold)
			->Get();
	const CDouble dHJFeedingTupColumnSpillingCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(
				CCostModelParamsGPDB::EcpHJFeedingTupColumnSpillingCostUnit)
			->Get();
	const CDouble dHJFeedingTupWidthSpillingCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(
				CCostModelParamsGPDB::EcpHJFeedingTupWidthSpillingCostUnit)
			->Get();
	const CDouble dHJHashingTupWidthSpillingCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(
				CCostModelParamsGPDB::EcpHJHashingTupWidthSpillingCostUnit)
			->Get();
	const CDouble dPenalizeHJSkewUpperLimit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpPenalizeHJSkewUpperLimit)
			->Get();
	GPOS_ASSERT(0 < dHJHashTableInitCostFactor);
	GPOS_ASSERT(0 < dHJHashTableColumnCostUnit);
	GPOS_ASSERT(0 < dHJHashTableWidthCostUnit);
	GPOS_ASSERT(0 < dJoinFeedingTupColumnCostUnit);
	GPOS_ASSERT(0 < dJoinFeedingTupWidthCostUnit);
	GPOS_ASSERT(0 < dHJHashingTupWidthCostUnit);
	GPOS_ASSERT(0 < dJoinOutputTupCostUnit);
	GPOS_ASSERT(0 < dHJSpillingMemThreshold);
	GPOS_ASSERT(0 < dHJFeedingTupColumnSpillingCostUnit);
	GPOS_ASSERT(0 < dHJFeedingTupWidthSpillingCostUnit);
	GPOS_ASSERT(0 < dHJHashingTupWidthSpillingCostUnit);
	GPOS_ASSERT(0 < dPenalizeHJSkewUpperLimit);

	// get the number of columns used in join condition
	CExpression *pexprJoinCond = exprhdl.PexprScalarRepChild(2);
	CColRefSet *pcrsUsed = pexprJoinCond->DeriveUsedColumns();
	const ULONG ulColsUsed = pcrsUsed->Size();

	// TODO 2014-03-14
	// currently, we hard coded a spilling memory threshold for judging whether hash join spills or not
	// In the future, we should calculate it based on the number of memory-intensive operators and statement memory available
	CCost costLocal(0);

	// inner tuples fit in memory
	if (dRowsInner * dWidthInner <= dHJSpillingMemThreshold)
	{
		// hash join cost contains four parts:
		// 1. build hash table with inner tuples. This part is correlated with rows and width of
		// inner tuples and the number of columns used in join condition.
		// 2. feeding outer tuples. This part is correlated with rows and width of outer tuples
		// and the number of columns used.
		// 3. matching inner tuples. This part is correlated with rows and width of inner tuples.
		// 4. output tuples. This part is correlated with outer rows and width of the join result.
		costLocal = CCost(
			pci->NumRebinds() *
			(
				// cost of building hash table
				dRowsInner * (ulColsUsed * dHJHashTableColumnCostUnit +
							  dWidthInner * dHJHashTableWidthCostUnit) +
				// cost of feeding outer tuples
				ulColsUsed * num_rows_outer * dJoinFeedingTupColumnCostUnit +
				dWidthOuter * num_rows_outer * dJoinFeedingTupWidthCostUnit +
				// cost of matching inner tuples
				dWidthInner * dRowsInner * dHJHashingTupWidthCostUnit +
				// cost of output tuples
				pci->Rows() * pci->Width() * dJoinOutputTupCostUnit));
	}
	else
	{
		// inner tuples spill

		// hash join cost if spilling is the same as the non-spilling case, except that
		// parameter values are different.
		costLocal = CCost(
			pci->NumRebinds() *
			(dHJHashTableInitCostFactor +
			 dRowsInner * (ulColsUsed * dHJHashTableColumnCostUnit +
						   dWidthInner * dHJHashTableWidthCostUnit) +
			 ulColsUsed * num_rows_outer * dHJFeedingTupColumnSpillingCostUnit +
			 dWidthOuter * num_rows_outer * dHJFeedingTupWidthSpillingCostUnit +
			 dWidthInner * dRowsInner * dHJHashingTupWidthSpillingCostUnit +
			 pci->Rows() * pci->Width() * dJoinOutputTupCostUnit));
	}
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	COptimizerConfig *optimizer_config =
		COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();

	CDouble skew_ratio = 1;
	ULONG arity = exprhdl.Arity();

	if (GPOS_FTRACE(EopttraceDiscardRedistributeHashJoin))
	{
		for (ULONG ul = 0; ul < arity - 1; ul++)
		{
			COperator *popChild = exprhdl.Pop(ul);
			if (nullptr != popChild &&
				COperator::EopPhysicalMotionHashDistribute == popChild->Eopid())
			{
				return CCost(GPOS_FP_ABS_MAX);
			}
		}
	}

	// Hashjoin with skewed HashRedistribute below them are expensive
	// find out if there is a skewed redistribute child of this HashJoin.
	if (!GPOS_FTRACE(EopttracePenalizeSkewedHashJoin))
	{
		for (ULONG ul = 0; ul < arity - 1; ++ul)
		{
			COperator *popChild = exprhdl.Pop(ul);
			if (nullptr == popChild ||
				COperator::EopPhysicalMotionHashDistribute != popChild->Eopid())
			{
				continue;
			}

			CPhysicalMotion *motion = CPhysicalMotion::PopConvert(popChild);
			CColRefSet *columns = motion->Pds()->PcrsUsed(mp);

			// we decide if there is a skew by calculating the NDVs of the HashRedistribute
			CDouble ndv = 1.0;
			CColRefSetIter iter(*columns);
			while (iter.Advance())
			{
				CColRef *colref = iter.Pcr();
				ndv = ndv * pci->Pcstats(ul)->GetNDVs(colref);
			}

			// if the NDVs are less than number of segments then there is definitely
			// a skew. NDV < 1 implies no stats exist for the columns involved. So we don't
			// want to take any decision.
			// In case of a skew, penalize the local cost of HashJoin with a
			// skew ratio = (num of segments)/ndv
			if (ndv < pcmgpdb->UlHosts() && (ndv >= 1))
			{
				CDouble sk = pcmgpdb->UlHosts() / ndv;
				skew_ratio = CDouble(std::max(sk.Get(), skew_ratio.Get()));
			}

			ULONG skew_factor = optimizer_config->GetHint()->UlSkewFactor();
			if (skew_factor > 0)
			{
				// If user specified skew multiplier is larger than 0
				// Compute skew
				IStatistics *pcstats = pci->Pcstats(ul)->Pstats();
				// User specified skew factor is fed to a power function,
				// whose ouptut becomes the final skew multiplier.
				// This allows fine tuning when the skew factor is small,
				// and coarse tuning when the skew factor is big.
				// The multiplier caps at 1.0307^(100-1) = 20
				// The base 1.0307 is so chosen that if the data is slightly
				// skewed, i.e., skew calculated from the histogram is a
				// little above 1, we get a multiplier of 20 if we max out
				// the skew factor at 100
				skew_factor = pow(1.0307, (skew_factor - 1));
				CDouble sk1 =
					skew_factor * CPhysical::GetSkew(pcstats, motion->Pds());
				skew_ratio = CDouble(std::max(sk1.Get(), skew_ratio.Get()));
			}
			else
			{
				// If user specified skew multiplier is 0
				// Cap the skew
				// To avoid gather motions
				skew_ratio = CDouble(std::min(dPenalizeHJSkewUpperLimit.Get(),
											  skew_ratio.Get()));
			}

			columns->Release();
		}
	}

	return costChild + CCost(costLocal.Get() * skew_ratio);
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostMergeJoin
//
//	@doc:
//		Cost of merge join
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostMergeJoin(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  const CCostModelGPDB *pcmgpdb,
							  const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalFullMergeJoin == op_id);
#endif	// GPOS_DEBUG

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];
	const DOUBLE dRowsInner = pci->PdRows()[1];
	const DOUBLE dWidthInner = pci->GetWidth()[1];

	const CDouble dJoinFeedingTupColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupColumnCostUnit)
			->Get();
	const CDouble dJoinFeedingTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupWidthCostUnit)
			->Get();
	const CDouble dJoinOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinOutputTupCostUnit)
			->Get();
	const CDouble dFilterColCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpFilterColCostUnit)
			->Get();
	const CDouble dOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpOutputTupCostUnit)
			->Get();
	GPOS_ASSERT(0 < dJoinFeedingTupColumnCostUnit);
	GPOS_ASSERT(0 < dJoinFeedingTupWidthCostUnit);
	GPOS_ASSERT(0 < dJoinOutputTupCostUnit);
	GPOS_ASSERT(0 < dFilterColCostUnit);
	GPOS_ASSERT(0 < dOutputTupCostUnit);

	// get the number of columns used in join condition
	CExpression *pexprJoinCond = exprhdl.PexprScalarRepChild(2);
	CColRefSet *pcrsUsed = pexprJoinCond->DeriveUsedColumns();
	const ULONG ulColsUsed = pcrsUsed->Size();

	// We assume for costing, that the outer tuples are unique. This means that
	// we will never have to rescan a portion of the inner side.
	CCost costLocal = CCost(
		pci->NumRebinds() *
		(
			// feeding cost of outer
			ulColsUsed * num_rows_outer * dJoinFeedingTupColumnCostUnit +
			dWidthOuter * num_rows_outer * dJoinFeedingTupWidthCostUnit +
			// cost of matching
			(dRowsInner + num_rows_outer) * ulColsUsed * dFilterColCostUnit +
			// cost of extracting matched inner side
			pci->Rows() * dWidthInner * dOutputTupCostUnit +
			// cost of output tuples
			pci->Rows() * pci->Width() * dJoinOutputTupCostUnit));

	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costChild + costLocal;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostIndexNLJoin
//
//	@doc:
//		Cost of inner or outer index-nljoin
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostIndexNLJoin(CMemoryPool *mp, CExpressionHandle &exprhdl,
								const CCostModelGPDB *pcmgpdb,
								const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(
		COperator::EopPhysicalInnerIndexNLJoin == exprhdl.Pop()->Eopid() ||
		COperator::EopPhysicalLeftOuterIndexNLJoin == exprhdl.Pop()->Eopid());

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];

	const CDouble dJoinFeedingTupColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupColumnCostUnit)
			->Get();
	const CDouble dJoinFeedingTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupWidthCostUnit)
			->Get();
	const CDouble dJoinOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinOutputTupCostUnit)
			->Get();
	GPOS_ASSERT(0 < dJoinFeedingTupColumnCostUnit);
	GPOS_ASSERT(0 < dJoinFeedingTupWidthCostUnit);
	GPOS_ASSERT(0 < dJoinOutputTupCostUnit);

	// get the number of columns used in join condition
	CExpression *pexprJoinCond = exprhdl.PexprScalarRepChild(2);
	CColRefSet *pcrsUsed = pexprJoinCond->DeriveUsedColumns();
	const ULONG ulColsUsed = pcrsUsed->Size();

	// cost of Index apply contains three parts:
	// 1. feeding outer tuples. This part is correlated with rows and width of outer tuples
	// and the number of columns used.
	// 2. repetitive index scan of inner side for each feeding tuple. This part of cost is
	// calculated and counted in its index scan child node.
	// 3. output tuples. This part is correlated with outer rows and width of the join result.
	CCost costLocal =
		CCost(pci->NumRebinds() *
			  (
				  // cost of feeding outer tuples
				  ulColsUsed * num_rows_outer * dJoinFeedingTupColumnCostUnit +
				  dWidthOuter * num_rows_outer * dJoinFeedingTupWidthCostUnit +
				  // cost of output tuples
				  pci->Rows() * pci->Width() * dJoinOutputTupCostUnit));

	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	ULONG risk = pci->Pcstats()->StatsEstimationRisk();
	ULONG ulPenalizationFactor = 1;
	const CDouble dIndexJoinAllowedRiskThreshold =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexJoinAllowedRiskThreshold)
			->Get();
	BOOL fInnerJoin =
		COperator::EopPhysicalInnerIndexNLJoin == exprhdl.Pop()->Eopid();

	// Only apply penalize factor for inner index nestloop join, because we are more confident
	// on the cardinality estimation of outer join than inner join. So don't penalize outer join
	// cost, otherwise Orca generate bad plan.
	if (fInnerJoin && dIndexJoinAllowedRiskThreshold < risk)
	{
		ulPenalizationFactor = risk;
	}

	return CCost(ulPenalizationFactor * (costLocal + costChild));
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostNLJoin
//
//	@doc:
//		Cost of nljoin
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostNLJoin(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   const CCostModelGPDB *pcmgpdb,
						   const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(CUtils::FNLJoin(exprhdl.Pop()));

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];
	const DOUBLE dRowsInner = pci->PdRows()[1];
	const DOUBLE dWidthInner = pci->GetWidth()[1];

	const CDouble dJoinFeedingTupColumnCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupColumnCostUnit)
			->Get();
	const CDouble dJoinFeedingTupWidthCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinFeedingTupWidthCostUnit)
			->Get();
	const CDouble dJoinOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpJoinOutputTupCostUnit)
			->Get();
	const CDouble dInitScan =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpInitScanFactor)
			->Get();
	const CDouble dTableScanCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTableScanCostUnit)
			->Get();
	const CDouble dFilterColCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpFilterColCostUnit)
			->Get();
	const CDouble dOutputTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpOutputTupCostUnit)
			->Get();
	const CDouble dNLJFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpNLJFactor)
			->Get();

	GPOS_ASSERT(0 < dJoinFeedingTupColumnCostUnit);
	GPOS_ASSERT(0 < dJoinFeedingTupWidthCostUnit);
	GPOS_ASSERT(0 < dJoinOutputTupCostUnit);
	GPOS_ASSERT(0 < dInitScan);
	GPOS_ASSERT(0 < dTableScanCostUnit);
	GPOS_ASSERT(0 < dFilterColCostUnit);
	GPOS_ASSERT(0 < dOutputTupCostUnit);
	GPOS_ASSERT(0 < dNLJFactor);

	// get the number of columns used in join condition
	CExpression *pexprJoinCond = exprhdl.PexprScalarRepChild(2);
	CColRefSet *pcrsUsed = pexprJoinCond->DeriveUsedColumns();
	const ULONG ulColsUsed = pcrsUsed->Size();

	// cost of nested loop join contains three parts:
	// 1. feeding outer tuples. This part is correlated with rows and width of outer tuples
	// and the number of columns used.
	// 2. repetitive scan of inner side for each feeding tuple. This part of cost consists of
	// the following:
	// a. repetitive scan and filter of the materialized inner side
	// b. extract matched inner side tuples
	// with the cardinality of outer tuples, rows and width of the materialized inner side.
	// 3. output tuples. This part is correlated with outer rows and width of the join result.
	CCost costLocal = CCost(
		pci->NumRebinds() *
		(
			// cost of feeding outer tuples
			ulColsUsed * num_rows_outer * dJoinFeedingTupColumnCostUnit +
			dWidthOuter * num_rows_outer * dJoinFeedingTupWidthCostUnit +
			// cost of repetitive table scan of inner side
			dInitScan +
			num_rows_outer * (
								 // cost of scan of inner side
								 dRowsInner * dWidthInner * dTableScanCostUnit +
								 // cost of filter of inner side
								 dRowsInner * ulColsUsed * dFilterColCostUnit)
			// cost of extracting matched inner side
			+ pci->Rows() * dWidthInner * dOutputTupCostUnit +
			// cost of output tuples
			pci->Rows() * pci->Width() * dJoinOutputTupCostUnit));

	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	CCost costTotal = CCost(costLocal + costChild);

	// amplify NLJ cost based on NLJ factor and stats estimation risk
	// we don't want to penalize index join compared to nested loop join, so we make sure
	// that every time index join is penalized, we penalize nested loop join by at least the
	// same amount
	CDouble dPenalization = dNLJFactor;
	const CDouble dRisk(pci->Pcstats()->StatsEstimationRisk());
	if (dRisk > dPenalization)
	{
		const CDouble dIndexJoinAllowedRiskThreshold =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(
					CCostModelParamsGPDB::EcpIndexJoinAllowedRiskThreshold)
				->Get();
		if (dIndexJoinAllowedRiskThreshold < dRisk)
		{
			dPenalization = dRisk;
		}
	}

	return CCost(costTotal * dPenalization);
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostMotion
//
//	@doc:
//		Cost of motion
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostMotion(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   const CCostModelGPDB *pcmgpdb,
						   const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalMotionGather == op_id ||
				COperator::EopPhysicalMotionBroadcast == op_id ||
				COperator::EopPhysicalMotionHashDistribute == op_id ||
				COperator::EopPhysicalMotionRandom == op_id ||
				COperator::EopPhysicalMotionRoutedDistribute == op_id);

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];

	// motion cost contains three parts: sending cost, interconnect cost, and receiving cost.
	// TODO 2014-03-18
	// in current cost model, interconnect cost is tied with receiving cost. Because we
	// only have one set calibration results in the dimension of the number of segments.
	// Once we calibrate the cost model with different number of segments, I will update
	// the function.

	CDouble dSendCostUnit(0);
	CDouble dRecvCostUnit(0);
	CDouble recvCost(0);

	CCost costLocal(0);
	if (COperator::EopPhysicalMotionBroadcast == op_id)
	{
		// broadcast cost is amplified by the number of segments
		dSendCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpBroadcastSendCostUnit)
				->Get();
		dRecvCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpBroadcastRecvCostUnit)
				->Get();

		recvCost =
			num_rows_outer * dWidthOuter * pcmgpdb->UlHosts() * dRecvCostUnit;
	}
	else if (COperator::EopPhysicalMotionHashDistribute == op_id ||
			 COperator::EopPhysicalMotionRandom == op_id ||
			 COperator::EopPhysicalMotionRoutedDistribute == op_id)
	{
		dSendCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpRedistributeSendCostUnit)
				->Get();
		dRecvCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpRedistributeRecvCostUnit)
				->Get();

		// Adjust the cost of no-op hashed distribution to correctly reflect that no tuple movement is needed
		CPhysicalMotion *pMotion = CPhysicalMotion::PopConvert(exprhdl.Pop());
		CDistributionSpec *pds = pMotion->Pds();
		if (CDistributionSpec::EdtHashedNoOp == pds->Edt())
		{
			// promote the plan with redistribution on same distributed columns of base table for parallel append
			dSendCostUnit =
				pcmgpdb->GetCostModelParams()
					->PcpLookup(CCostModelParamsGPDB::EcpNoOpCostUnit)
					->Get();
			dRecvCostUnit =
				pcmgpdb->GetCostModelParams()
					->PcpLookup(CCostModelParamsGPDB::EcpNoOpCostUnit)
					->Get();
		}

		recvCost = pci->Rows() * pci->Width() * dRecvCostUnit;
	}
	else if (COperator::EopPhysicalMotionGather == op_id)
	{
		dSendCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpGatherSendCostUnit)
				->Get();
		dRecvCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpGatherRecvCostUnit)
				->Get();

		recvCost =
			num_rows_outer * dWidthOuter * pcmgpdb->UlHosts() * dRecvCostUnit;
	}

	GPOS_ASSERT(0 <= dSendCostUnit);
	GPOS_ASSERT(0 <= dRecvCostUnit);

	costLocal =
		CCost(pci->NumRebinds() *
			  (num_rows_outer * dWidthOuter * dSendCostUnit + recvCost));


	if (COperator::EopPhysicalMotionBroadcast == op_id)
	{
		CPhysicalMotionBroadcast *physical_broadcast =
			CPhysicalMotionBroadcast::PopConvert(exprhdl.Pop());
		COptimizerConfig *optimizer_config =
			COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
		ULONG broadcast_threshold =
			optimizer_config->GetHint()->UlBroadcastThreshold();

		// if the broadcast threshold is 0, don't penalize
		// also, if the replicated distribution is set to ignore the broadcast
		// threshold (e.g. it's under a LASJ not-in) don't penalize
		if (broadcast_threshold > 0 && num_rows_outer > broadcast_threshold &&
			!CDistributionSpecReplicated::PdsConvert(physical_broadcast->Pds())
				 ->FIgnoreBroadcastThreshold())
		{
			DOUBLE ulPenalizationFactor = 100000000000000.0;
			costLocal = CCost(ulPenalizationFactor);
		}
	}


	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostSequenceProject
//
//	@doc:
//		Cost of sequence project
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostSequenceProject(CMemoryPool *mp, CExpressionHandle &exprhdl,
									const CCostModelGPDB *pcmgpdb,
									const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalSequenceProject ==
				exprhdl.Pop()->Eopid());

	const DOUBLE num_rows_outer = pci->PdRows()[0];
	const DOUBLE dWidthOuter = pci->GetWidth()[0];

	ULONG ulSortCols = 0;
	COrderSpecArray *pdrgpos =
		CPhysicalSequenceProject::PopConvert(exprhdl.Pop())->Pdrgpos();
	const ULONG ulOrderSpecs = pdrgpos->Size();
	for (ULONG ul = 0; ul < ulOrderSpecs; ul++)
	{
		COrderSpec *pos = (*pdrgpos)[ul];
		ulSortCols += pos->UlSortColumns();
	}

	const CDouble dTupDefaultProcCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTupDefaultProcCostUnit)
			->Get();
	GPOS_ASSERT(0 < dTupDefaultProcCostUnit);

	// we process (sorted window of) input tuples to compute window function values
	CCost costLocal =
		CCost(pci->NumRebinds() * (ulSortCols * num_rows_outer * dWidthOuter *
								   dTupDefaultProcCostUnit));
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::ComputeUnusedIndexWeight
//
//	@doc:
//		Compute weight of unused index column for Index & Index only scans
//		due to mismatch in columns used in the index and the predicate
//
//---------------------------------------------------------------------------
CDouble
CCostModelGPDB::ComputeUnusedIndexWeight(CExpressionHandle &exprhdl,
										 CColRefArray *pdrgpcrIndexColumns,
										 IStatistics *pBaseTableStats,
										 CMemoryPool *mp, IMDId *rel_mdid)

{
	GPOS_ASSERT(nullptr != pBaseTableStats);

	CDouble dCummulativeUnusedIndexWeight = 0;
	CDouble dNdv(1.0);

	// Finding used predicate columns
	CExpression *pexprIndexCond = exprhdl.PexprScalarRepChild(0);
	CColRefSet *pcrsUsedPredicate = pexprIndexCond->DeriveUsedColumns();

	ULONG ulNoOfColumnsInPredicate = pcrsUsedPredicate->Size();
	ULONG ulNoOfColumnsInIndex = pdrgpcrIndexColumns->Size();

	if (ulNoOfColumnsInIndex > 0 && ulNoOfColumnsInPredicate > 0)
	{
		// Iterate through all the index columns to find, any unused
		// index column in the predicate
		// Eg- index : a,b,c  Predicate: c,d
		// Unused index columns - a,b
		// Used index column -	c
		// Indexed Predicate column - c (in the expHandle we have information of
		// only those predicate columns, which are applicable on the index)
		for (ULONG ulIndexColPos = 0; ulIndexColPos < ulNoOfColumnsInIndex;
			 ulIndexColPos++)
		{
			CColRef *colrefIndexColumn = (*pdrgpcrIndexColumns)[ulIndexColPos];

			CDouble dUnusedIndexColWeight = 0;
			bool fIndexPredColMatchFound = false;

			// For every index column, we check if it is present in the
			// predicate columns
			fIndexPredColMatchFound =
				pcrsUsedPredicate->FMember(colrefIndexColumn);

			// if no match is found, it implies, index column is not present
			// in the used predicate. We use this column for costing.
			if (!fIndexPredColMatchFound)
			{
				// Adjusting the weight for the unused index (ulNoOfColumnsInIndex-ul)
				// For index idx_abc, if column 'a' is unused, then since
				// it is the most significant index column, so it should have
				// more weightage.
				dUnusedIndexColWeight = (ulNoOfColumnsInIndex - ulIndexColPos);

				IStatistics *unusedIndexColStats =
					CStatistics::CastStats(pBaseTableStats)
						->ComputeColStats(mp, colrefIndexColumn, rel_mdid);

				// Finding NDV of the unused column
				dNdv = CStatistics::CastStats(unusedIndexColStats)
						   ->GetNDVs(colrefIndexColumn);

				CDouble dTableRows =
					CStatistics::CastStats(pBaseTableStats)->Rows();

				GPOS_ASSERT(0 != dTableRows);

				// we multiply by ratio - (dNdv/dTableRows), to adjust
				// the weight of column for distinct/duplicate values in it.
				// For eg- for index idx_abc, if unused column is 'b' and if
				// all of its value are distinct, then it's weightage should be high
				// compared to if all of its values are same.
				dUnusedIndexColWeight =
					dUnusedIndexColWeight * (dNdv / dTableRows);

				dCummulativeUnusedIndexWeight =
					dCummulativeUnusedIndexWeight + dUnusedIndexColWeight;

				unusedIndexColStats->Release();
			}
		}
	}
	return dCummulativeUnusedIndexWeight;
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::GetCommonIndexData
//
//	@doc:
// 		Get count of index keys, width of included col, array of index columns
// 		and table statistics for Index Scan, Index only scan, Dynamic Index scan
// 		& Dynamic Index only scan
//
//		'pdrgpcrIndexColumns' & 'stats' are passed by reference.
//		The calling function is responsible to release the allocated memory.
//---------------------------------------------------------------------------
template <typename T>
void
CCostModelGPDB::GetCommonIndexData(T *ptr, ULONG &ulIndexKeys,
								   ULONG &ulIncludedColWidth,
								   CColRefArray *&pdrgpcrIndexColumns,
								   IStatistics *&stats,
								   CMDAccessor *md_accessor, CMemoryPool *mp)
{
	CColumnDescriptorArray *indexIncludedArray = nullptr;

	ulIndexKeys = ptr->Pindexdesc()->Keys();

	// Index's INCLUDE columns adds to the width of the index and thus adds I/O
	// cost per index row. Account for that cost in dCostPerIndexRow.
	indexIncludedArray = ptr->Pindexdesc()->PdrgpcoldescIncluded();
	for (ULONG ul = 0; ul < indexIncludedArray->Size(); ul++)
	{
		ulIncludedColWidth += (*indexIncludedArray)[ul]->Width();
	}

	const IMDRelation *pmdrel =
		md_accessor->RetrieveRel(ptr->Ptabdesc()->MDId());

	const IMDIndex *pmdindex =
		md_accessor->RetrieveIndex(ptr->Pindexdesc()->MDId());

	pdrgpcrIndexColumns = CXformUtils::PdrgpcrIndexKeys(
		mp, ptr->PdrgpcrOutput(), pmdindex, pmdrel);

	stats = ptr->PstatsBaseTable();
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostIndexScan
//
//	@doc:
//		Cost of index scan
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostIndexScan(CMemoryPool *mp GPOS_UNUSED,
							  CExpressionHandle &exprhdl,
							  const CCostModelGPDB *pcmgpdb,
							  const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

	COperator *pop = exprhdl.Pop();
	COperator::EOperatorId op_id = pop->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalIndexScan == op_id ||
				COperator::EopPhysicalDynamicIndexScan == op_id);

	IMDId *rel_mdid = CPhysicalScan::PopConvert(pop)->Ptabdesc()->MDId();

	const CDouble dTableWidth =
		CPhysicalScan::PopConvert(pop)->PstatsBaseTable()->Width();

	const CDouble dIndexFilterCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexFilterCostUnit)
			->Get();
	const CDouble dIndexScanTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexScanTupCostUnit)
			->Get();
	const CDouble dIndexOnlyScanTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexOnlyScanTupCostUnit)
			->Get();
	const CDouble dIndexScanTupRandomFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexScanTupRandomFactor)
			->Get();
	GPOS_ASSERT(0 < dIndexFilterCostUnit);
	GPOS_ASSERT(0 < dIndexScanTupCostUnit);
	GPOS_ASSERT(0 < dIndexScanTupRandomFactor);

	CDouble dRowsIndex = pci->Rows();

	ULONG ulIndexKeys = 1;
	ULONG ulIncludedColWidth = 0;

	// Getting Meta Data Accessor  ----------------------------
	const COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();
	CColRefArray *pdrgpcrIndexColumns = nullptr;
	ULONG ulUnindexedPredCount = 0;
	IStatistics *stats = nullptr;

	if (COperator::EopPhysicalIndexScan == op_id)
	{
		// For Index Scan
		CPhysicalIndexScan *ptr = CPhysicalIndexScan::PopConvert(pop);
		GetCommonIndexData(ptr, ulIndexKeys, ulIncludedColWidth,
						   pdrgpcrIndexColumns, stats, md_accessor, mp);
		ulUnindexedPredCount = ptr->ResidualPredicateSize();
	}
	else
	{
		// For Dynamic Index Scan
		CPhysicalDynamicIndexScan *ptr =
			CPhysicalDynamicIndexScan::PopConvert(pop);
		GetCommonIndexData(ptr, ulIndexKeys, ulIncludedColWidth,
						   pdrgpcrIndexColumns, stats, md_accessor, mp);
		ulUnindexedPredCount = ptr->ResidualPredicateSize();
	}

	// TODO: 2014-02-01
	// Add logic to judge if the index column used in the filter is the first key of a multi-key index or not.
	// and separate the cost functions for the two cases.

	// index scan cost contains two parts: index-column lookup and output tuple cost.
	// 1. index-column lookup: correlated with index lookup rows, the number of index columns used in lookup,
	// table width and a randomIOFactor. also accounts for included columns which adds to the payload of index leaf
	// pages that leads to bigger a btree which subsequently leads to more random IO during index lookup.
	// 2. output tuple cost: this is handled by the Filter on top of IndexScan, if no Filter exists, we add output cost
	// when we sum-up children cost

	CDouble dIndexCostConversionFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexCostConversionFactor)
			->Get();

	CDouble dUnindexedPredCost =
		ulUnindexedPredCount * dIndexCostConversionFactor;
	CDouble dUnusedIndexCost =
		ComputeUnusedIndexWeight(exprhdl, pdrgpcrIndexColumns, stats, mp,
								 rel_mdid) *
		dIndexCostConversionFactor;

	pdrgpcrIndexColumns->Release();

	CDouble dCostPerIndexRow = ulIndexKeys * dIndexFilterCostUnit +
							   dTableWidth * dIndexScanTupCostUnit +
							   ulIncludedColWidth * dIndexOnlyScanTupCostUnit;
	return CCost(pci->NumRebinds() *
				 (dRowsIndex * dCostPerIndexRow + dIndexScanTupRandomFactor +
				  dUnindexedPredCost + dUnusedIndexCost));
}


CCost
CCostModelGPDB::CostIndexOnlyScan(CMemoryPool *mp GPOS_UNUSED,	  // mp
								  CExpressionHandle &exprhdl,	  //exprhdl
								  const CCostModelGPDB *pcmgpdb,  // pcmgpdb
								  const SCostingInfo *pci		  //pci
)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

	COperator *pop = exprhdl.Pop();
	GPOS_ASSERT(COperator::EopPhysicalIndexOnlyScan == pop->Eopid() ||
				COperator::EopPhysicalDynamicIndexOnlyScan == pop->Eopid());

	IMDId *rel_mdid = CPhysicalScan::PopConvert(pop)->Ptabdesc()->MDId();

	const CDouble dTableWidth =
		CPhysicalScan::PopConvert(pop)->PstatsBaseTable()->Width();

	BOOL isAO = CPhysicalScan::PopConvert(exprhdl.Pop())
					->Ptabdesc()
					->IsAORowOrColTable();

	CDouble dIndexFilterCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexFilterCostUnit)
			->Get();
	const CDouble dIndexScanTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexScanTupCostUnit)
			->Get();
	const CDouble dIndexOnlyScanTupCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexOnlyScanTupCostUnit)
			->Get();
	const CDouble dIndexScanTupRandomFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexScanTupRandomFactor)
			->Get();
	GPOS_ASSERT(0 < dIndexFilterCostUnit);
	GPOS_ASSERT(0 < dIndexScanTupCostUnit);
	GPOS_ASSERT(0 < dIndexScanTupRandomFactor);

	if (isAO)
	{
		// AO specific costs related to index-scan/index-only-scan:
		//
		//   * AO tables have a variable block size layout on disk (e.g. batch
		//     insert creates larger block than single row insert). However,
		//     this makes index scans tricky because the block identifier does
		//     not directly map to a fixed offset in the relfile. Instead, an
		//     additional abstraction layer, the block directory, is required
		//     to map the block identifier to an offset inside the relfile.
		//
		//   * AO table blocks are loaded in-memory into a single varblock.
		//     Heap blocks, however, support multiple in-memory instances and
		//     fit in the page cache. That means random I/O on AO tables is
		//     more susceptible to thrashing.
		//
		//   * AO tables in production often compress the blocks. That adds an
		//     additional penalty for loading blocks that is exacerbated by a
		//     poor page replacement algorithm. And, in the case of AO single
		//     varblock, any time we have to revisit a block, it will always
		//     have to be replaced and reloaded.
		//
		// Here an index filter cost is penalized more to provide a rudimentary
		// way to account for these factors. Script cal_bitmap_test.py was used to
		// identify a suitable cost.
		dIndexFilterCostUnit = dIndexFilterCostUnit * 100;
	}

	CDouble dRowsIndex = pci->Rows();

	ULONG ulIndexKeys;
	ULONG ulIncludedColWidth = 0;
	IStatistics *stats = nullptr;

	const COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();

	CColRefArray *pdrgpcrIndexColumns = nullptr;

	if (COperator::EopPhysicalIndexOnlyScan == pop->Eopid())
	{
		CPhysicalIndexOnlyScan *ptr = CPhysicalIndexOnlyScan::PopConvert(pop);
		GetCommonIndexData(ptr, ulIndexKeys, ulIncludedColWidth,
						   pdrgpcrIndexColumns, stats, md_accessor, mp);
	}
	else
	{
		CPhysicalDynamicIndexOnlyScan *ptr =
			CPhysicalDynamicIndexOnlyScan::PopConvert(pop);
		GetCommonIndexData(ptr, ulIndexKeys, ulIncludedColWidth,
						   pdrgpcrIndexColumns, stats, md_accessor, mp);
	}


	// 1. For 'Index only scans' cost component for 'Unused Index' columns
	// in the predicate is required.
	// 2. No additional cost is required for 'Unindexed Predicate' column in the
	// index, as in that case, Index Only scan will not exist.
	// 3. For Eg select a,b from t1 where a=1 and b = 'aa1' and c =17;
	// Assuming only index idx_ab, exists, then since column 'c'
	// is also used in the query, 'Index only scan' will not be generated as an
	// alternate.

	CDouble dIndexCostConversionFactor =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpIndexCostConversionFactor)
			->Get();

	CDouble dUnusedIndexCost =
		ComputeUnusedIndexWeight(exprhdl, pdrgpcrIndexColumns, stats, mp,
								 rel_mdid) *
		dIndexCostConversionFactor;

	pdrgpcrIndexColumns->Release();
	// The cost of index-only-scan is similar to index-scan with the additional
	// dimension of variable size I/O. More specifically, index-scan I/O is
	// bound to the fixed width of the relation times the number of output
	// rows. However, index-only-scan may be able to sometimes avoid the cost
	// of the full width of the relation (when the page is all visible) and
	// instead directly retrieve the row from a narrow index.
	//
	// The percent of rows that can avoid I/O on full table width is
	// approximately equal to the precent of tuples in all-visible blocks
	// compared to total blocks. It is approximate because there is no
	// guarantee that blocks are equally filled with live tuples.
	//
	// We never scan the underlying append-optimized table relfile for
	// performing visibility checks. It's as if all blocks are all-visible. See
	// cdb_estimate_rel_size(). So consider dPartialVisFrac as 0.

	CDouble dPartialVisFrac(1);
	if (isAO)
	{
		dPartialVisFrac = 0;
	}
	else if (stats->RelPages() != 0)
	{
		dPartialVisFrac =
			1 - (CDouble(stats->RelAllVisible()) / CDouble(stats->RelPages()));
	}

	CDouble dCostPerIndexRow =
		ulIndexKeys * dIndexFilterCostUnit +
		// partial visibile read from table
		dTableWidth * dIndexScanTupCostUnit * dPartialVisFrac +
		// always read from index (partial and full visible)
		ulIncludedColWidth * dIndexOnlyScanTupCostUnit;

	return CCost(pci->NumRebinds() *
				 (dRowsIndex * dCostPerIndexRow + dIndexScanTupRandomFactor +
				  dUnusedIndexCost));
}

CCost
CCostModelGPDB::CostBitmapTableScan(CMemoryPool *mp, CExpressionHandle &exprhdl,
									const CCostModelGPDB *pcmgpdb,
									const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(
		COperator::EopPhysicalBitmapTableScan == exprhdl.Pop()->Eopid() ||
		COperator::EopPhysicalDynamicBitmapTableScan == exprhdl.Pop()->Eopid());

	CCost result(0.0);
	CExpression *pexprIndexCond =
		exprhdl.PexprScalarRepChild(1 /*child_index*/);
	CColRefSet *pcrsUsed = pexprIndexCond->DeriveUsedColumns();
	CColRefSet *outerRefs = exprhdl.DeriveOuterReferences();
	CColRefSet *pcrsLocalUsed = GPOS_NEW(mp) CColRefSet(mp, *pcrsUsed);
	IMDIndex::EmdindexType indexType = IMDIndex::EmdindSentinel;

	if (COperator::EopScalarBitmapIndexProbe == pexprIndexCond->Pop()->Eopid())
	{
		indexType = CScalarBitmapIndexProbe::PopConvert(pexprIndexCond->Pop())
						->Pindexdesc()
						->IndexType();
	}

	BOOL isInPredOnBtreeIndex =
		(IMDIndex::EmdindBtree == indexType &&
		 COperator::EopScalarArrayCmp == (*pexprIndexCond)[0]->Pop()->Eopid());

	// subtract outer references from the used colrefs, so we can see
	// how many colrefs are used for this table
	pcrsLocalUsed->Exclude(outerRefs);

	const DOUBLE rows = pci->Rows();
	const DOUBLE width = pci->Width();
	CDouble dInitRebind =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpBitmapScanRebindCost)
			->Get();

	if (COperator::EopScalarBitmapIndexProbe !=
			pexprIndexCond->Pop()->Eopid() ||
		1 < pcrsLocalUsed->Size() ||
		(isInPredOnBtreeIndex && rows > 2.0 &&
		 GPOS_FTRACE(EopttraceLegacyCostModel)))
	{
		// Child is Bitmap AND/OR, or we use Multi column index or this is an IN predicate
		// that's used with the "calibrated" cost model.
		// Handling the IN predicate in this code path is to avoid plan regressions from
		// earlier versions of the code that treated IN predicates like ORs and therefore
		// also handled them in this code path. This is especially noticeable for btree
		// indexes that often have a high NDV, because the small/large NDV cost model
		// produces very high cost for cases with a higher NDV.
		const CDouble dIndexFilterCostUnit =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpIndexFilterCostUnit)
				->Get();

		GPOS_ASSERT(0 < dIndexFilterCostUnit);

		// check whether the user specified overriding values in gucs
		CDouble dInitScan =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpInitScanFactor)
				->Get();

		GPOS_ASSERT(dInitScan >= 0 && dInitRebind >= 0);

		// For now we are trying to cost Bitmap Scan similar to Index Scan. dIndexFilterCostUnit is
		// the dominant factor in costing Index Scan so we are using it in our model. Also we are giving
		// Bitmap Scan a start up cost similar to Sequential Scan. Note that in this code path we add the
		// relatively high dInitScan cost, while in the other code paths below (CostBitmapLargeNDV and
		// CostBitmapSmallNDV) we don't. That's something we should look into.

		// Conceptually the cost of evaluating index qual is also linear in the
		// number of index columns, but we're only accounting for the dominant cost

		result =
			CCost(	// cost for each byte returned by the index scan plus cost for incremental rebinds
				pci->NumRebinds() *
					(rows * width * dIndexFilterCostUnit + dInitRebind) +
				// init cost
				dInitScan);
	}
	else
	{
		// if the expression is const table get, the pcrsUsed is empty
		// so we use minimum value MinDistinct for dNDV in that case.
		CDouble dNDV = CHistogram::MinDistinct;
		CDouble dNDVThreshold =
			pcmgpdb->GetCostModelParams()
				->PcpLookup(CCostModelParamsGPDB::EcpBitmapNDVThreshold)
				->Get();

		if (rows < 1.0)
		{
			// if we aren't accessing a row every rebind, then don't charge a cost for those cases where we don't have a row
			dNDV = rows;
		}
		else if (1 == pcrsLocalUsed->Size())  // if you only have one local pred
		{
			CColRef *pcrIndexCond = pcrsLocalUsed->PcrFirst();
			GPOS_ASSERT(nullptr != pcrIndexCond);
			// get the num distinct for the rows returned by the predicate
			dNDV = pci->Pcstats()->GetNDVs(pcrIndexCond);
			// if there's an outerref
			if (1 < pcrsUsed->Size() && COperator::EopScalarBitmapIndexProbe ==
											pexprIndexCond->Pop()->Eopid())
			{
				CExpression *pexprScalarCmp = (*pexprIndexCond)[0];
				// The index condition contains an outer reference. Adjust
				// the NDV to 1, if we compare the colref with a single value
				if (CPredicateUtils::IsEqualityOp(pexprScalarCmp))
				{
					dNDV = 1.0;
				}
			}
		}

		if (GPOS_FTRACE(EopttraceLegacyCostModel))
		{
			// optimizer_cost_model = 'legacy'
			if (dNDVThreshold <= dNDV)
			{
				result = CostBitmapLargeNDV(pcmgpdb, pci, dNDV);
			}
			else
			{
				result = CostBitmapSmallNDV(pcmgpdb, pci, dNDV);
			}
		}
		else
		{
			// optimizer_cost_model = 'calibrated'|'experimental'
			CDouble dBitmapIO =
				pcmgpdb->GetCostModelParams()
					->PcpLookup(CCostModelParamsGPDB::EcpBitmapIOCostSmallNDV)
					->Get();
			CDouble c5_dInitScan =
				pcmgpdb->GetCostModelParams()
					->PcpLookup(CCostModelParamsGPDB::EcpInitScanFactor)
					->Get();
			CDouble c3_dBitmapPageCost =
				pcmgpdb->GetCostModelParams()
					->PcpLookup(CCostModelParamsGPDB::EcpBitmapPageCost)
					->Get();
			BOOL isAOTable = CPhysicalScan::PopConvert(exprhdl.Pop())
								 ->Ptabdesc()
								 ->IsAORowOrColTable();

			// some cost constants determined with the cal_bitmap_test.py script
			CDouble c1_cost_per_row(0.03);
			CDouble c2_cost_per_byte(0.0001);
			CDouble bitmap_union_cost_per_distinct_value(0.000027);
			CDouble init_cost_advantage_for_bitmap_scan(0.9);

			if (IMDIndex::EmdindBtree == indexType)
			{
				// btree indexes are not sensitive to the NDV, since they don't have any bitmaps
				c3_dBitmapPageCost = 0.0;
			}

			// Give the index scan a small initial advantage over the table scan, so we use indexes
			// for small tables - this should avoid having table scan and index scan costs being
			// very close together for many small queries.
			c5_dInitScan = c5_dInitScan * init_cost_advantage_for_bitmap_scan;

			// The numbers below were experimentally determined using regression analysis in the cal_bitmap_test.py script
			// The following dSizeCost is in the form C1 * rows + C2 * rows * width. This is because the width should have
			// significantly less weight than rows as the execution time does not grow as fast in regards to width
			CDouble dSizeCost = dBitmapIO * (rows * c1_cost_per_row +
											 rows * width * c2_cost_per_byte);

			CDouble bitmapUnionCost = 0;

			if (!isAOTable && indexType == IMDIndex::EmdindBitmap && dNDV > 1.0)
			{
				CDouble baseTableRows = CPhysicalScan::PopConvert(exprhdl.Pop())
											->PstatsBaseTable()
											->Rows();

				// for bitmap index scans on heap tables, we found that there is an additional cost
				// associated with unioning them that is proportional to the number of bitmaps involved
				// (dNDV-1) times the width of the bitmap (proportional to the number of rows in the table)
				bitmapUnionCost = std::max(0.0, dNDV.Get() - 1.0) *
								  baseTableRows *
								  bitmap_union_cost_per_distinct_value;
			}

			result = CCost(pci->NumRebinds() *
							   (dSizeCost + dNDV * c3_dBitmapPageCost +
								dInitRebind + bitmapUnionCost) +
						   c5_dInitScan);
		}
	}

	pcrsLocalUsed->Release();

	return result;
}


CCost
CCostModelGPDB::CostBitmapSmallNDV(const CCostModelGPDB *pcmgpdb,
								   const SCostingInfo *pci, CDouble dNDV)
{
	const DOUBLE rows = pci->Rows();
	const DOUBLE width = pci->Width();

	CDouble dSize = (rows * width) * 0.001;

	CDouble dBitmapIO =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpBitmapIOCostSmallNDV)
			->Get();
	CDouble dBitmapPageCost =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpBitmapPageCostSmallNDV)
			->Get();
	CDouble effectiveNDV = dNDV;

	if (rows < 1.0)
	{
		// if we aren't accessing a row every rebind, then don't charge a cost for those cases where we don't have a row
		effectiveNDV = rows;
	}

	return CCost(pci->NumRebinds() *
				 (dBitmapIO * dSize + dBitmapPageCost * effectiveNDV));
}


CCost
CCostModelGPDB::CostBitmapLargeNDV(const CCostModelGPDB *pcmgpdb,
								   const SCostingInfo *pci, CDouble dNDV)
{
	const DOUBLE rows = pci->Rows();
	const DOUBLE width = pci->Width();

	CDouble dSize = (rows * width * dNDV) * 0.001;
	CDouble dBitmapIO =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpBitmapIOCostLargeNDV)
			->Get();
	CDouble dBitmapPageCost =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpBitmapPageCostLargeNDV)
			->Get();

	return CCost(pci->NumRebinds() *
				 (dBitmapIO * dSize + dBitmapPageCost * dNDV));
}

//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostScan
//
//	@doc:
//		Cost of scan
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostScan(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl,
						 const CCostModelGPDB *pcmgpdb, const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);

	COperator *pop = exprhdl.Pop();
	COperator::EOperatorId op_id = pop->Eopid();
	GPOS_ASSERT(COperator::EopPhysicalTableScan == op_id ||
				COperator::EopPhysicalDynamicTableScan == op_id ||
				COperator::EopPhysicalForeignScan == op_id ||
				COperator::EopPhysicalDynamicForeignScan == op_id);

	const CDouble dInitScan =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpInitScanFactor)
			->Get();
	const CDouble dTableWidth =
		CPhysicalScan::PopConvert(pop)->PstatsBaseTable()->Width();

	// Get total rows for each host to scan
	const CDouble dTableScanCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpTableScanCostUnit)
			->Get();
	GPOS_ASSERT(0 < dTableScanCostUnit);

	switch (op_id)
	{
		case COperator::EopPhysicalTableScan:
		case COperator::EopPhysicalDynamicTableScan:
		case COperator::EopPhysicalForeignScan:
		case COperator::EopPhysicalDynamicForeignScan:
			// table scan cost considers only retrieving tuple cost,
			// since we scan the entire table here, the cost is correlated with table rows and table width,
			// since Scan's parent operator may be a filter that will be pushed into Scan node in GPDB plan,
			// we add Scan output tuple cost in the parent operator and not here
			return CCost(
				pci->NumRebinds() *
				(dInitScan + pci->Rows() * dTableWidth * dTableScanCostUnit));
		default:
			GPOS_ASSERT(!"invalid index scan");
			return CCost(0);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::CostFilter
//
//	@doc:
//		Cost of filter
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::CostFilter(CMemoryPool *mp, CExpressionHandle &exprhdl,
						   const CCostModelGPDB *pcmgpdb,
						   const SCostingInfo *pci)
{
	GPOS_ASSERT(nullptr != pcmgpdb);
	GPOS_ASSERT(nullptr != pci);
	GPOS_ASSERT(COperator::EopPhysicalFilter == exprhdl.Pop()->Eopid());

	const DOUBLE dInput = pci->PdRows()[0];
	const ULONG ulFilterCols = exprhdl.DeriveUsedColumns(1)->Size();

	const CDouble dFilterColCostUnit =
		pcmgpdb->GetCostModelParams()
			->PcpLookup(CCostModelParamsGPDB::EcpFilterColCostUnit)
			->Get();
	GPOS_ASSERT(0 < dFilterColCostUnit);

	// filter cost is correlated with the input rows and the number of filter columns.
	CCost costLocal = CCost(dInput * ulFilterCols * dFilterColCostUnit);

	costLocal = CCost(costLocal.Get() * pci->NumRebinds());
	CCost costChild =
		CostChildren(mp, exprhdl, pci, pcmgpdb->GetCostModelParams());

	return costLocal + costChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CCostModelGPDB::Cost
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CCost
CCostModelGPDB::Cost(
	CExpressionHandle &exprhdl,	 // handle gives access to expression properties
	const SCostingInfo *pci) const
{
	GPOS_ASSERT(nullptr != pci);

	COperator::EOperatorId op_id = exprhdl.Pop()->Eopid();
	if (op_id == COperator::EopPhysicalComputeScalar)
	{
		return CostComputeScalar(m_mp, exprhdl, pci, m_cost_model_params, this);
	}
	if (FUnary(op_id))
	{
		return CostUnary(m_mp, exprhdl, pci, m_cost_model_params);
	}

	switch (op_id)
	{
		default:
		{
			// FIXME: macro this?
			__builtin_unreachable();
		}
		case COperator::EopPhysicalTableScan:
		case COperator::EopPhysicalDynamicTableScan:
		case COperator::EopPhysicalForeignScan:
		case COperator::EopPhysicalDynamicForeignScan:

		{
			return CostScan(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalFilter:
		{
			return CostFilter(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalDynamicIndexOnlyScan:
		case COperator::EopPhysicalIndexOnlyScan:
		{
			return CostIndexOnlyScan(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalIndexScan:
		case COperator::EopPhysicalDynamicIndexScan:
		{
			return CostIndexScan(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalBitmapTableScan:
		case COperator::EopPhysicalDynamicBitmapTableScan:
		{
			return CostBitmapTableScan(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalSequenceProject:
		{
			return CostSequenceProject(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalCTEProducer:
		{
			return CostCTEProducer(m_mp, exprhdl, this, pci);
		}
		case COperator::EopPhysicalCTEConsumer:
		{
			return CostCTEConsumer(m_mp, exprhdl, this, pci);
		}
		case COperator::EopPhysicalConstTableGet:
		{
			return CostConstTableGet(m_mp, exprhdl, this, pci);
		}
		case COperator::EopPhysicalDML:
		{
			return CostDML(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalHashAgg:
		case COperator::EopPhysicalHashAggDeduplicate:
		{
			return CostHashAgg(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalScalarAgg:
		{
			return CostScalarAgg(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalStreamAgg:
		case COperator::EopPhysicalStreamAggDeduplicate:
		{
			return CostStreamAgg(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalSequence:
		{
			return CostSequence(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalSort:
		{
			return CostSort(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalTVF:
		{
			return CostTVF(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalSerialUnionAll:
		case COperator::EopPhysicalParallelUnionAll:
		{
			return CostUnionAll(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalInnerHashJoin:
		case COperator::EopPhysicalLeftSemiHashJoin:
		case COperator::EopPhysicalLeftAntiSemiHashJoin:
		case COperator::EopPhysicalLeftAntiSemiHashJoinNotIn:
		case COperator::EopPhysicalLeftOuterHashJoin:
		case COperator::EopPhysicalRightOuterHashJoin:
		{
			return CostHashJoin(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalInnerIndexNLJoin:
		case COperator::EopPhysicalLeftOuterIndexNLJoin:
		{
			return CostIndexNLJoin(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalMotionGather:
		case COperator::EopPhysicalMotionBroadcast:
		case COperator::EopPhysicalMotionHashDistribute:
		case COperator::EopPhysicalMotionRandom:
		case COperator::EopPhysicalMotionRoutedDistribute:
		{
			return CostMotion(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalInnerNLJoin:
		case COperator::EopPhysicalLeftSemiNLJoin:
		case COperator::EopPhysicalLeftAntiSemiNLJoin:
		case COperator::EopPhysicalLeftAntiSemiNLJoinNotIn:
		case COperator::EopPhysicalLeftOuterNLJoin:
		case COperator::EopPhysicalCorrelatedInnerNLJoin:
		case COperator::EopPhysicalCorrelatedLeftOuterNLJoin:
		case COperator::EopPhysicalCorrelatedLeftSemiNLJoin:
		case COperator::EopPhysicalCorrelatedInLeftSemiNLJoin:
		case COperator::EopPhysicalCorrelatedLeftAntiSemiNLJoin:
		case COperator::EopPhysicalCorrelatedNotInLeftAntiSemiNLJoin:
		{
			return CostNLJoin(m_mp, exprhdl, this, pci);
		}

		case COperator::EopPhysicalFullMergeJoin:
		{
			return CostMergeJoin(m_mp, exprhdl, this, pci);
		}
	}
}

// EOF
