//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		COptimizationContext.cpp
//
//	@doc:
//		Implementation of optimization context
//---------------------------------------------------------------------------

#include "gpopt/base/COptimizationContext.h"

#include "gpos/base.h"
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/CEnfdOrder.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysicalAgg.h"
#include "gpopt/operators/CPhysicalCTEProducer.h"
#include "gpopt/operators/CPhysicalMotion.h"
#include "gpopt/operators/CPhysicalNLJoin.h"
#include "gpopt/operators/CPhysicalSort.h"
#include "gpopt/search/CGroupExpression.h"
#include "gpopt/xforms/CXformUtils.h"


using namespace gpopt;

FORCE_GENERATE_DBGSTR(COptimizationContext);

// invalid optimization context
const COptimizationContext COptimizationContext::m_ocInvalid;

// invalid optimization context pointer
const OPTCTXT_PTR COptimizationContext::m_pocInvalid = nullptr;



//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::~COptimizationContext
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
COptimizationContext::~COptimizationContext()
{
	CRefCount::SafeRelease(m_prpp);
	CRefCount::SafeRelease(m_prprel);
	CRefCount::SafeRelease(m_pdrgpstatCtxt);
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::PgexprBest
//
//	@doc:
//		Best group expression accessor
//
//---------------------------------------------------------------------------
CGroupExpression *
COptimizationContext::PgexprBest() const
{
	if (nullptr == m_pccBest)
	{
		return nullptr;
	}

	return m_pccBest->Pgexpr();
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::SetBest
//
//	@doc:
//		 Set best cost context
//
//---------------------------------------------------------------------------
void
COptimizationContext::SetBest(CCostContext *pcc)
{
	GPOS_ASSERT(nullptr != pcc);

	m_pccBest = pcc;

	COperator *pop = pcc->Pgexpr()->Pop();
	if (CUtils::FPhysicalAgg(pop) &&
		CPhysicalAgg::PopConvert(pop)->FMultiStage())
	{
		m_fHasMultiStageAggPlan = true;
	}
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::Matches
//
//	@doc:
//		Match against another context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::Matches(const COptimizationContext *poc) const
{
	GPOS_ASSERT(nullptr != poc);

	if (m_pgroup != poc->Pgroup() ||
		m_ulSearchStageIndex != poc->UlSearchStageIndex())
	{
		return false;
	}

	CReqdPropPlan *prppFst = this->Prpp();
	CReqdPropPlan *prppSnd = poc->Prpp();

	// make sure we are not comparing to invalid context
	if (nullptr == prppFst || nullptr == prppSnd)
	{
		return nullptr == prppFst && nullptr == prppSnd;
	}

	return prppFst->Equals(prppSnd);
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FEqualForStats
//
//	@doc:
//		Equality function used for computing stats during costing
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FEqualForStats(const COptimizationContext *pocLeft,
									 const COptimizationContext *pocRight)
{
	GPOS_ASSERT(nullptr != pocLeft);
	GPOS_ASSERT(nullptr != pocRight);

	return pocLeft->GetReqdRelationalProps()->PcrsStat()->Equals(
			   pocRight->GetReqdRelationalProps()->PcrsStat()) &&
		   pocLeft->Pdrgpstat()->Equals(pocRight->Pdrgpstat());
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FOptimize
//
//	@doc:
//		Return true if given group expression should be optimized under
//		given context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FOptimize(CMemoryPool *mp, CGroupExpression *pgexprParent,
								CGroupExpression *pgexprChild,
								COptimizationContext *pocChild,
								ULONG ulSearchStages)
{
	COperator *pop = pgexprChild->Pop();

	if (CUtils::FPhysicalMotion(pop))
	{
		return FOptimizeMotion(mp, pgexprParent, pgexprChild, pocChild,
							   ulSearchStages);
	}

	if (COperator::EopPhysicalSort == pop->Eopid())
	{
		return FOptimizeSort(mp, pgexprParent, pgexprChild, pocChild,
							 ulSearchStages);
	}

	if (CUtils::FPhysicalAgg(pop))
	{
		return FOptimizeAgg(pgexprParent, pgexprChild);
	}

	if (CUtils::FNLJoin(pop))
	{
		return FOptimizeNLJoin(mp, pgexprParent, pgexprChild, pocChild,
							   ulSearchStages);
	}

	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FEqualIds
//
//	@doc:
//		Compare array of optimization contexts based on context ids
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FEqualContextIds(COptimizationContextArray *pdrgpocFst,
									   COptimizationContextArray *pdrgpocSnd)
{
	if (nullptr == pdrgpocFst || nullptr == pdrgpocSnd)
	{
		return (nullptr == pdrgpocFst && nullptr == pdrgpocSnd);
	}

	const ULONG ulCtxts = pdrgpocFst->Size();
	if (ulCtxts != pdrgpocSnd->Size())
	{
		return false;
	}

	BOOL fEqual = true;
	for (ULONG ul = 0; fEqual && ul < ulCtxts; ul++)
	{
		fEqual = (*pdrgpocFst)[ul]->Id() == (*pdrgpocSnd)[ul]->Id();
	}

	return fEqual;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FOptimizeMotion
//
//	@doc:
//		Check if a Motion node should be optimized for the given context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FOptimizeMotion(CMemoryPool *,	   // mp
									  CGroupExpression *,  // pgexprParent
									  CGroupExpression *pgexprMotion,
									  COptimizationContext *poc,
									  ULONG	 // ulSearchStages
)
{
	GPOS_ASSERT(nullptr != pgexprMotion);
	GPOS_ASSERT(nullptr != poc);
	GPOS_ASSERT(CUtils::FPhysicalMotion(pgexprMotion->Pop()));

	CPhysicalMotion *pop = CPhysicalMotion::PopConvert(pgexprMotion->Pop());

	return poc->Prpp()->Ped()->FCompatible(pop->Pds());
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FOptimizeSort
//
//	@doc:
//		Check if a Sort node should be optimized for the given context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FOptimizeSort(CMemoryPool *,		 // mp
									CGroupExpression *,	 // pgexprParent
									CGroupExpression *pgexprSort,
									COptimizationContext *poc,
									ULONG  // ulSearchStages
)
{
	GPOS_ASSERT(nullptr != pgexprSort);
	GPOS_ASSERT(nullptr != poc);
	GPOS_ASSERT(COperator::EopPhysicalSort == pgexprSort->Pop()->Eopid());

	CPhysicalSort *pop = CPhysicalSort::PopConvert(pgexprSort->Pop());

	return poc->Prpp()->Peo()->FCompatible(
		const_cast<COrderSpec *>(pop->Pos()));
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::FOptimizeAgg
//
//	@doc:
//		Check if Agg node should be optimized for the given context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FOptimizeAgg(CGroupExpression *,	// pgexprParent
								   CGroupExpression *pgexprAgg)
{
	GPOS_ASSERT(nullptr != pgexprAgg);
	GPOS_ASSERT(CUtils::FPhysicalAgg(pgexprAgg->Pop()));

	if (GPOS_FTRACE(EopttraceForceExpandedMDQAs))
	{
		BOOL fHasMultipleDistinctAggs =
			CDrvdPropScalar::GetDrvdScalarProps((*pgexprAgg)[1]->Pdp())
				->HasMultipleDistinctAggs();
		if (fHasMultipleDistinctAggs)
		{
			// do not optimize plans with MDQAs, since preference is for plans with expanded MDQAs
			return false;
		}
	}

	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalNLJoin::FOptimizeNLJoin
//
//	@doc:
//		Check if NL join node should be optimized for the given context
//
//---------------------------------------------------------------------------
BOOL
COptimizationContext::FOptimizeNLJoin(CMemoryPool *mp,
									  CGroupExpression *,  // pgexprParent
									  CGroupExpression *pgexprJoin,
									  COptimizationContext *poc,
									  ULONG	 // ulSearchStages
)
{
	GPOS_ASSERT(nullptr != pgexprJoin);
	GPOS_ASSERT(nullptr != poc);
	GPOS_ASSERT(CUtils::FNLJoin(pgexprJoin->Pop()));

	COperator *pop = pgexprJoin->Pop();
	if (!CUtils::FCorrelatedNLJoin(pop))
	{
		return true;
	}

	// for correlated join, the requested columns must be covered by outer child
	// columns and columns to be generated from inner child
	CPhysicalNLJoin *popNLJoin = CPhysicalNLJoin::PopConvert(pop);
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, popNLJoin->PdrgPcrInner());
	CColRefSet *pcrsOuterChild =
		CDrvdPropRelational::GetRelationalProperties((*pgexprJoin)[0]->Pdp())
			->GetOutputColumns();
	pcrs->Include(pcrsOuterChild);
	BOOL fIncluded = pcrs->ContainsAll(poc->Prpp()->PcrsRequired());
	pcrs->Release();

	return fIncluded;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::PrppCTEProducer
//
//	@doc:
//		Compute required properties to CTE producer based on plan properties
//		of CTE consumer
//
//---------------------------------------------------------------------------
CReqdPropPlan *
COptimizationContext::PrppCTEProducer(CMemoryPool *mp,
									  COptimizationContext *poc,
									  ULONG ulSearchStages)
{
	GPOS_ASSERT(nullptr != poc);
	GPOS_ASSERT(nullptr != poc->PccBest());

	CCostContext *pccBest = poc->PccBest();
	CGroupExpression *pgexpr = pccBest->Pgexpr();
	BOOL fOptimizeCTESequence =
		(COperator::EopPhysicalSequence == pgexpr->Pop()->Eopid() &&
		 (*pgexpr)[0]->FHasCTEProducer());

	if (!fOptimizeCTESequence)
	{
		// best group expression is not a CTE sequence
		return nullptr;
	}

	COptimizationContext *pocProducer = (*pgexpr)[0]->PocLookupBest(
		mp, ulSearchStages, (*pccBest->Pdrgpoc())[0]->Prpp());
	if (nullptr == pocProducer)
	{
		return nullptr;
	}

	CCostContext *pccProducer = pocProducer->PccBest();
	if (nullptr == pccProducer)
	{
		return nullptr;
	}
	COptimizationContext *pocConsumer = (*pgexpr)[1]->PocLookupBest(
		mp, ulSearchStages, (*pccBest->Pdrgpoc())[1]->Prpp());
	if (nullptr == pocConsumer)
	{
		return nullptr;
	}

	CCostContext *pccConsumer = pocConsumer->PccBest();
	if (nullptr == pccConsumer)
	{
		return nullptr;
	}

	CColRefSet *pcrsInnerOutput =
		CDrvdPropRelational::GetRelationalProperties((*pgexpr)[1]->Pdp())
			->GetOutputColumns();
	CPhysicalCTEProducer *popProducer =
		CPhysicalCTEProducer::PopConvert(pccProducer->Pgexpr()->Pop());
	UlongToColRefMap *colref_mapping =
		COptCtxt::PoctxtFromTLS()->Pcteinfo()->PhmulcrConsumerToProducer(
			mp, popProducer->UlCTEId(), pcrsInnerOutput,
			popProducer->Pdrgpcr());
	CReqdPropPlan *prppProducer = CReqdPropPlan::PrppRemapForCTE(
		mp, pocProducer->Prpp(), pccConsumer->Pdpplan(), colref_mapping);
	colref_mapping->Release();

	if (prppProducer->Equals(pocProducer->Prpp()))
	{
		prppProducer->Release();

		return nullptr;
	}

	return prppProducer;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationContext::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
COptimizationContext::OsPrint(IOstream &os) const
{
	return OsPrintWithPrefix(os, "");
}

IOstream &
COptimizationContext::OsPrintWithPrefix(IOstream &os,
										const CHAR *szPrefix) const
{
	os << szPrefix << m_id << " (stage " << m_ulSearchStageIndex << "): ("
	   << *m_prpp << ") => Best Expr:";
	if (nullptr != PgexprBest())
	{
		os << PgexprBest()->Id();
	}
	os << std::endl;

	return os;
}

// EOF
