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

#include "gpopt/metadata/CPartConstraint.h"
#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CColRefSet.h"
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
	ULONG scan_id, CColRefArray *pdrgpcrOutput, CColRef2dArray *pdrgpdrgpcrPart)
	: CLogical(mp),
	  m_pnameAlias(pnameAlias),
	  m_ptabdesc(ptabdesc),
	  m_scan_id(scan_id),
	  m_pdrgpcrOutput(pdrgpcrOutput),
	  m_pdrgpdrgpcrPart(pdrgpdrgpcrPart),
	  m_pcrsDist(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	GPOS_ASSERT(NULL != pdrgpdrgpcrPart);

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
	  m_pcrsDist(NULL)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameAlias);

	// generate a default column set for the table descriptor
	m_pdrgpcrOutput = PdrgpcrCreateMapping(mp, m_ptabdesc->Pdrgpcoldesc(),
										   UlOpId(), m_ptabdesc->MDId());
	m_pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(mp, m_pdrgpcrOutput,
												  m_ptabdesc->PdrgpulPart());
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

	CPartInfo *ppartinfo = GPOS_NEW(mp) CPartInfo(mp);
	ppartinfo->AddPartConsumer(mp, m_scan_id, mdid, m_pdrgpdrgpcrPart);

	return ppartinfo;
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

	if (NULL != pexprFilter)
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
