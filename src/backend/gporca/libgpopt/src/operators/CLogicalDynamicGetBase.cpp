//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CLogicalDynamicGetBase.cpp
//
//	@doc:
//		Implementation of dynamic table access base class
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CPartIndexMap.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDynamicGetBase.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"

#include "naucrates/statistics/CStatsPredUtils.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::CLogicalDynamicGetBase
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDynamicGetBase::CLogicalDynamicGetBase(CMemoryPool *mp)
	: CLogical(mp),
	  m_pnameAlias(NULL),
	  m_ptabdesc(NULL),
	  m_scan_id(0),
	  m_pdrgpcrOutput(NULL),
	  m_pdrgpdrgpcrPart(NULL),
	  m_ulSecondaryScanId(0),
	  m_is_partial(false),
	  m_part_constraint(NULL),
	  m_ppartcnstrRel(NULL),
	  m_pcrsDist(NULL)
{
	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::CLogicalDynamicGetBase
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicGetBase::CLogicalDynamicGetBase(
	CMemoryPool *mp, const CName *pnameAlias, CTableDescriptor *ptabdesc,
	ULONG scan_id, CColRefArray *pdrgpcrOutput, CColRef2dArray *pdrgpdrgpcrPart,
	ULONG ulSecondaryScanId, BOOL is_partial, CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel)
	: CLogical(mp),
	  m_pnameAlias(pnameAlias),
	  m_ptabdesc(ptabdesc),
	  m_scan_id(scan_id),
	  m_pdrgpcrOutput(pdrgpcrOutput),
	  m_pdrgpdrgpcrPart(pdrgpdrgpcrPart),
	  m_ulSecondaryScanId(ulSecondaryScanId),
	  m_is_partial(is_partial),
	  m_part_constraint(ppartcnstr),
	  m_ppartcnstrRel(ppartcnstrRel),
	  m_pcrsDist(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	GPOS_ASSERT(NULL != pdrgpdrgpcrPart);
	GPOS_ASSERT(NULL != ppartcnstr);
	GPOS_ASSERT(NULL != ppartcnstrRel);

	GPOS_ASSERT_IMP(scan_id != ulSecondaryScanId, NULL != ppartcnstr);
	GPOS_ASSERT_IMP(is_partial,
					NULL != m_part_constraint->PcnstrCombined() &&
						"Partial scan with unsupported constraint type");

	m_pcrsDist = CLogical::PcrsDist(mp, m_ptabdesc, m_pdrgpcrOutput);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::CLogicalDynamicGetBase
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicGetBase::CLogicalDynamicGetBase(CMemoryPool *mp,
											   const CName *pnameAlias,
											   CTableDescriptor *ptabdesc,
											   ULONG scan_id)
	: CLogical(mp),
	  m_pnameAlias(pnameAlias),
	  m_ptabdesc(ptabdesc),
	  m_scan_id(scan_id),
	  m_pdrgpcrOutput(NULL),
	  m_ulSecondaryScanId(scan_id),
	  m_is_partial(false),
	  m_part_constraint(NULL),
	  m_ppartcnstrRel(NULL),
	  m_pcrsDist(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);

	// generate a default column set for the table descriptor
	m_pdrgpcrOutput = PdrgpcrCreateMapping(mp, m_ptabdesc->Pdrgpcoldesc(),
										   UlOpId(), m_ptabdesc->MDId());
	m_pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(mp, m_pdrgpcrOutput,
												  m_ptabdesc->PdrgpulPart());

	// generate a constraint "true"
	UlongToConstraintMap *phmulcnstr = CUtils::PhmulcnstrBoolConstOnPartKeys(
		mp, m_pdrgpdrgpcrPart, true /*value*/);
	CBitSet *pbsDefaultParts = CUtils::PbsAllSet(mp, m_pdrgpdrgpcrPart->Size());
	m_pdrgpdrgpcrPart->AddRef();
	m_part_constraint =
		GPOS_NEW(mp) CPartConstraint(mp, phmulcnstr, pbsDefaultParts,
									 true /*is_unbounded*/, m_pdrgpdrgpcrPart);
	m_part_constraint->AddRef();
	m_ppartcnstrRel = m_part_constraint;

	m_pcrsDist = CLogical::PcrsDist(mp, m_ptabdesc, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::~CLogicalDynamicGetBase
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CLogicalDynamicGetBase::~CLogicalDynamicGetBase()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrOutput);
	CRefCount::SafeRelease(m_pdrgpdrgpcrPart);
	CRefCount::SafeRelease(m_part_constraint);
	CRefCount::SafeRelease(m_ppartcnstrRel);
	CRefCount::SafeRelease(m_pcrsDist);

	GPOS_DELETE(m_pnameAlias);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalDynamicGetBase::DeriveOutputColumns(CMemoryPool *mp,
											CExpressionHandle &	 // exprhdl
)
{
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	pcrs->Include(m_pdrgpcrOutput);

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalDynamicGetBase::DeriveKeyCollection(CMemoryPool *mp,
											CExpressionHandle &	 // exprhdl
) const
{
	const CBitSetArray *pdrgpbs = m_ptabdesc->PdrgpbsKeys();

	return CLogical::PkcKeysBaseTable(mp, pdrgpbs, m_pdrgpcrOutput);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::DerivePropertyConstraint
//
//	@doc:
//		Derive constraint property
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogicalDynamicGetBase::DerivePropertyConstraint(CMemoryPool *mp,
												 CExpressionHandle &  // exprhdl
) const
{
	return PpcDeriveConstraintFromTable(mp, m_ptabdesc, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::PpartinfoDerive
//
//	@doc:
//		Derive partition consumer info
//
//---------------------------------------------------------------------------
CPartInfo *
CLogicalDynamicGetBase::DerivePartitionInfo(CMemoryPool *mp,
											CExpressionHandle &	 // exprhdl
) const
{
	IMDId *mdid = m_ptabdesc->MDId();
	mdid->AddRef();
	m_pdrgpdrgpcrPart->AddRef();
	m_ppartcnstrRel->AddRef();

	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	ppartinfo->AddPartConsumer(mp, m_scan_id, mdid, m_pdrgpdrgpcrPart,
							   m_ppartcnstrRel);

	return ppartinfo;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::SetPartConstraint
//
//	@doc:
//		Set part constraint
//
//---------------------------------------------------------------------------
void
CLogicalDynamicGetBase::SetPartConstraint(CPartConstraint *ppartcnstr)
{
	GPOS_ASSERT(NULL != ppartcnstr);
	GPOS_ASSERT(NULL != m_part_constraint);

	m_part_constraint->Release();
	m_part_constraint = ppartcnstr;

	m_ppartcnstrRel->Release();
	ppartcnstr->AddRef();
	m_ppartcnstrRel = ppartcnstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::SetSecondaryScanId
//
//	@doc:
//		Set secondary scan id
//
//---------------------------------------------------------------------------
void
CLogicalDynamicGetBase::SetSecondaryScanId(ULONG scan_id)
{
	m_ulSecondaryScanId = scan_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::SetPartial
//
//	@doc:
//		Set partial to true
//
//---------------------------------------------------------------------------
void
CLogicalDynamicGetBase::SetPartial()
{
	GPOS_ASSERT(!IsPartial());
	GPOS_ASSERT(NULL != m_part_constraint->PcnstrCombined() &&
				"Partial scan with unsupported constraint type");
	m_is_partial = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGetBase::PstatsDeriveFilter
//
//	@doc:
//		Derive stats from base table using filters on partition and/or index columns
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDynamicGetBase::PstatsDeriveFilter(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CExpression *pexprFilter) const
{
	CExpression *pexprFilterNew = NULL;
	CConstraint *pcnstr = m_part_constraint->PcnstrCombined();
	if (m_is_partial && NULL != pcnstr && !pcnstr->IsConstraintUnbounded())
	{
		if (NULL == pexprFilter)
		{
			pexprFilterNew = pcnstr->PexprScalar(mp);
			pexprFilterNew->AddRef();
		}
		else
		{
			pexprFilterNew = CPredicateUtils::PexprConjunction(
				mp, pexprFilter, pcnstr->PexprScalar(mp));
		}
	}
	else if (NULL != pexprFilter)
	{
		pexprFilterNew = pexprFilter;
		pexprFilterNew->AddRef();
	}

	CColRefSet *pcrsStat = GPOS_NEW(mp) CColRefSet(mp);

	if (NULL != pexprFilterNew)
	{
		pcrsStat->Include(pexprFilterNew->DeriveUsedColumns());
	}

	// requesting statistics on distribution columns to estimate data skew
	if (NULL != m_pcrsDist)
	{
		pcrsStat->Include(m_pcrsDist);
	}


	CStatistics *pstatsFullTable = dynamic_cast<CStatistics *>(
		PstatsBaseTable(mp, exprhdl, m_ptabdesc, pcrsStat));

	pcrsStat->Release();

	if (NULL == pexprFilterNew || pexprFilterNew->DeriveHasSubquery())
	{
		return pstatsFullTable;
	}

	CStatsPred *pred_stats = CStatsPredUtils::ExtractPredStats(
		mp, pexprFilterNew, NULL /*outer_refs*/
	);
	pexprFilterNew->Release();

	IStatistics *result_stats = CFilterStatsProcessor::MakeStatsFilter(
		mp, pstatsFullTable, pred_stats, true /* do_cap_NDVs */);
	pred_stats->Release();
	pstatsFullTable->Release();

	return result_stats;
}

// EOF
