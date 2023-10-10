//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalScan.cpp
//
//	@doc:
//		Implementation of base scan operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalScan.h"

#include "gpos/base.h"

#include "gpopt/base/CCastUtils.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CPredicateUtils.h"
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::CPhysicalScan
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CPhysicalScan::CPhysicalScan(CMemoryPool *mp, const CName *pnameAlias,
							 CTableDescriptor *ptabdesc,
							 CColRefArray *pdrgpcrOutput)
	: CPhysical(mp),
	  m_pnameAlias(pnameAlias),
	  m_ptabdesc(ptabdesc),
	  m_pdrgpcrOutput(pdrgpcrOutput),
	  m_pds(nullptr),
	  m_pstatsBaseTable(nullptr)
{
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pnameAlias);
	GPOS_ASSERT(nullptr != pdrgpcrOutput);

	if (ptabdesc->ConvertHashToRandom())
	{
		// Treating a hash distributed table as random during planning
		m_pds = GPOS_NEW(m_mp) CDistributionSpecRandom();
	}
	else
	{
		m_pds = CPhysical::PdsCompute(m_mp, ptabdesc, pdrgpcrOutput,
									  nullptr /* gp_segment_id */);
	}
	ComputeTableStats(m_mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::~CPhysicalScan
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CPhysicalScan::~CPhysicalScan()
{
	m_ptabdesc->Release();
	m_pdrgpcrOutput->Release();
	m_pds->Release();
	m_pstatsBaseTable->Release();
	GPOS_DELETE(m_pnameAlias);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::FInputOrderSensitive
//
//	@doc:
//		Not called for leaf operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalScan::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalScan::FProvidesReqdCols(CExpressionHandle &,  // exprhdl
								 CColRefSet *pcrsRequired,
								 ULONG	// ulOptReq
) const
{
	GPOS_ASSERT(nullptr != pcrsRequired);

	CColRefSet *pcrs = GPOS_NEW(m_mp) CColRefSet(m_mp);
	pcrs->Include(m_pdrgpcrOutput);

	BOOL result = pcrs->ContainsAll(pcrsRequired);
	pcrs->Release();

	return result;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalScan::EpetOrder(CExpressionHandle &,  // exprhdl
						 const CEnfdOrder *
#ifdef GPOS_DEBUG
							 peo
#endif	// GPOS_DEBUG
) const
{
	GPOS_ASSERT(nullptr != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalScan::PdsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
{
	BOOL fIndexOrBitmapScan =
		COperator::EopPhysicalIndexScan == Eopid() ||
		COperator::EopPhysicalBitmapTableScan == Eopid() ||
		COperator::EopPhysicalDynamicIndexScan == Eopid() ||
		COperator::EopPhysicalDynamicBitmapTableScan == Eopid();
	if (fIndexOrBitmapScan && CDistributionSpec::EdtHashed == m_pds->Edt() &&
		exprhdl.HasOuterRefs())
	{
		// If index conditions have outer references and the index relation is hashed,
		// check to see if we can derive an equivalent hashed distribution for the
		// outer references. For multi-distribution key tables with an index, it is
		// possible for a spec to have a column from both the inner and the outer
		// table. This is termed "incomplete" and added as an equivalent spec.
		//
		// For example, if we have foo (a, b) distributed by (a,b)
		//                         bar (c, d) distributed by (c, d)
		// with an index on idx_bar_d, if we have the query
		// 		select * from foo join bar on a = c and b = d,
		// it is possible to get a spec of [a, d].
		//
		// An incomplete spec is relevant only when we have an index join on a
		// multi-key distributed table. This is handled either by completing the
		// equivalent spec using Filter predicates above (see
		// CPhysicalFilter::PdsDerive()), or by discarding an incomplete spec at
		// the index join (see CPhysicalJoin::PdsDerive()).
		//
		// This way the equiv spec stays incomplete only as long as it needs to be.

		CExpression *pexprIndexPred = exprhdl.PexprScalarExactChild(
			0 /*child_index*/, true /*error_on_null_return*/);

		CDistributionSpecHashed *pdshashed =
			CDistributionSpecHashed::PdsConvert(m_pds);
		CDistributionSpecHashed *pdshashedEquiv =
			CDistributionSpecHashed::TryToCompleteEquivSpec(
				mp, pdshashed, pexprIndexPred, exprhdl.DeriveOuterReferences());

		if (nullptr != pdshashedEquiv)
		{
			CExpressionArray *pdrgpexprHashed = pdshashed->Pdrgpexpr();
			pdrgpexprHashed->AddRef();
			if (nullptr != pdshashed->Opfamilies())
			{
				pdshashed->Opfamilies()->AddRef();
			}
			CDistributionSpecHashed *pdshashedResult =
				GPOS_NEW(mp) CDistributionSpecHashed(
					pdrgpexprHashed, pdshashed->FNullsColocated(),
					pdshashedEquiv, pdshashed->Opfamilies());

			return pdshashedResult;
		}
	}

	m_pds->AddRef();

	return m_pds;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::EpetDistribution
//
//	@doc:
//		Return the enforcing type for distribution property based on this
//		operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalScan::EpetDistribution(CExpressionHandle & /*exprhdl*/,
								const CEnfdDistribution *ped) const
{
	GPOS_ASSERT(nullptr != ped);

	if (ped->FCompatible(m_pds))
	{
		// required distribution will be established by the operator
		return CEnfdProp::EpetUnnecessary;
	}

	// required distribution will be enforced on output
	return CEnfdProp::EpetRequired;
}



//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::ComputeTableStats
//
//	@doc:
//		Compute stats of underlying table
//
//---------------------------------------------------------------------------
void
CPhysicalScan::ComputeTableStats(CMemoryPool *mp)
{
	GPOS_ASSERT(nullptr == m_pstatsBaseTable);

	CColRefSet *pcrsHist = GPOS_NEW(mp) CColRefSet(mp);
	CColRefSet *pcrsWidth = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrOutput);
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	m_pstatsBaseTable =
		md_accessor->Pstats(mp, m_ptabdesc->MDId(), pcrsHist, pcrsWidth);
	GPOS_ASSERT(nullptr != m_pstatsBaseTable);

	pcrsHist->Release();
	pcrsWidth->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalScan::PopConvert
//
//	@doc:
//		Conversion function
//
//---------------------------------------------------------------------------
CPhysicalScan *
CPhysicalScan::PopConvert(COperator *pop)
{
	GPOS_ASSERT(nullptr != pop);
	GPOS_ASSERT(CUtils::FPhysicalScan(pop));

	return dynamic_cast<CPhysicalScan *>(pop);
}


// EOF
