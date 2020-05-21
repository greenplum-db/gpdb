//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalStreamAgg.cpp
//
//	@doc:
//		Implementation of stream aggregation operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalStreamAgg.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::CPhysicalStreamAgg
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalStreamAgg::CPhysicalStreamAgg
	(
	CMemoryPool *mp,
	CColRefArray *colref_array,
	CColRefArray *pdrgpcrMinimal,
	COperator::EGbAggType egbaggtype,
	BOOL fGeneratesDuplicates,
	CColRefArray *pdrgpcrArgDQA,
	BOOL fMultiStage,
	BOOL isAggFromSplitDQA,
	CLogicalGbAgg::EAggStage aggStage,
  BOOL should_enforce_distribution
	)
	:
	CPhysicalAgg
	(
	mp,
	colref_array,
	pdrgpcrMinimal,
	egbaggtype,
	fGeneratesDuplicates,
	pdrgpcrArgDQA,
	fMultiStage,
	isAggFromSplitDQA,
	aggStage,
	should_enforce_distribution
	),
	m_pos(NULL)
{
	GPOS_ASSERT(NULL != m_pdrgpcrMinimal);
	m_pcrsMinimalGrpCols = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrMinimal);
	InitOrderSpec(mp, m_pdrgpcrMinimal);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::InitOrderSpec
//
//	@doc:
//		Initialize the order spec using the given array of columns
//
//---------------------------------------------------------------------------
void
CPhysicalStreamAgg::InitOrderSpec
	(
	CMemoryPool *mp,
	CColRefArray *pdrgpcrOrder
	)
{
	GPOS_ASSERT(NULL != pdrgpcrOrder);

	CRefCount::SafeRelease(m_pos);
	m_pos = GPOS_NEW(mp) COrderSpec(mp);
	const ULONG size = pdrgpcrOrder->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CColRef *colref = (*pdrgpcrOrder)[ul];

		// TODO: 12/21/2011 - ; this seems broken: a colref must not embed
		// a pointer to a cached object
		gpmd::IMDId *mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
		mdid->AddRef();

		m_pos->Append(mdid, colref, COrderSpec::EntLast);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::~CPhysicalStreamAgg
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalStreamAgg::~CPhysicalStreamAgg()
{
	m_pcrsMinimalGrpCols->Release();
	m_pos->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::PosCovering
//
//	@doc:
//		Construct order spec on grouping column so that it covers required
//		order spec, the function returns NULL if no covering order spec
//		can be created
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalStreamAgg::PosCovering
	(
	CMemoryPool *mp,
	COrderSpec *posRequired,
	CColRefArray *pdrgpcrGrp
	)
	const
{
	GPOS_ASSERT(NULL != posRequired);

	if (0 == posRequired->UlSortColumns())
	{
		// required order must be non-empty
		return NULL;
	}

	// create a set of required sort columns
	CColRefSet *pcrsReqd = posRequired->PcrsUsed(mp);

	COrderSpec *pos = NULL;

	CColRefSet *pcrsGrpCols = GPOS_NEW(mp) CColRefSet(mp, pdrgpcrGrp);
	if (pcrsGrpCols->ContainsAll(pcrsReqd))
	{
		// required order columns are included in grouping columns, we can
		// construct a covering order spec
		pos = GPOS_NEW(mp) COrderSpec(mp);

		// extract order expressions from required order
		const ULONG ulReqdSortCols = posRequired->UlSortColumns();
		for (ULONG ul = 0; ul < ulReqdSortCols; ul++)
		{
			CColRef *colref = const_cast<CColRef *>(posRequired->Pcr(ul));
			IMDId *mdid = posRequired->GetMdIdSortOp(ul);
			COrderSpec::ENullTreatment ent = posRequired->Ent(ul);
			mdid->AddRef();
			pos->Append(mdid, colref, ent);
		}

		// augment order with remaining grouping columns
		const ULONG size = pdrgpcrGrp->Size();
		for (ULONG ul = 0; ul < size; ul++)
		{
			CColRef *colref = (*pdrgpcrGrp)[ul];
			if (!pcrsReqd->FMember(colref))
			{
				IMDId *mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
				mdid->AddRef();
				pos->Append(mdid, colref, COrderSpec::EntLast);
			}
		}
	}
	pcrsGrpCols->Release();
	pcrsReqd->Release();

	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::PosRequiredStreamAgg
//
//	@doc:
//		Compute required sort columns of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalStreamAgg::PosRequiredStreamAgg
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl,
	COrderSpec *posRequired,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
	,
	CColRefArray *pdrgpcrGrp
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	COrderSpec *pos = PosCovering(mp, posRequired, pdrgpcrGrp);
	if (NULL == pos)
	{
		// failed to find a covering order spec, use local order spec
		m_pos->AddRef();
		pos = m_pos;
	}

	// extract sort columns from order spec
	CColRefSet *pcrs = pos->PcrsUsed(mp);

	// get key collection of the relational child
	CKeyCollection *pkc = exprhdl.DeriveKeyCollection(0);

	if (NULL != pkc && pkc->FKey(pcrs, false /*fExactMatch*/))
	{
		CColRefSet *pcrsReqd = posRequired->PcrsUsed(m_mp);
		BOOL fUsesDefinedCols = FUnaryUsesDefinedColumns(pcrsReqd, exprhdl);
		pcrsReqd->Release();
		
		if (!fUsesDefinedCols)
		{
			// we are grouping on child's key,
			// stream agg does not need to sort child and we can pass through input spec
			pos->Release();
			posRequired->AddRef();
			pos = posRequired;
		}
	}
	pcrs->Release();

	return pos;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalStreamAgg::PosDerive
	(
	CMemoryPool *, // mp
	CExpressionHandle &exprhdl
	)
	const
{
	return PosDerivePassThruOuter(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalStreamAgg::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalStreamAgg::EpetOrder
	(
	CExpressionHandle &exprhdl,
	const CEnfdOrder *peo
	)
	const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());
	
	// get the order delivered by the stream agg node
	COrderSpec *pos = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pos();
	if (peo->FCompatible(pos))
	{
		// required order will be established by the stream agg operator
		return CEnfdProp::EpetUnnecessary;
	}
	
	// required order will be enforced on limit's output
	return CEnfdProp::EpetRequired;
}

// EOF

