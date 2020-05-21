//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDynamicGet.cpp
//
//	@doc:
//		Implementation of dynamic table access
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CPartIndexMap.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"

#include "naucrates/statistics/CStatistics.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::CLogicalDynamicGet
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDynamicGet::CLogicalDynamicGet
	(
	CMemoryPool *mp
	)
	:
	CLogicalDynamicGetBase(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::CLogicalDynamicGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicGet::CLogicalDynamicGet
	(
	CMemoryPool *mp,
	const CName *pnameAlias,
	CTableDescriptor *ptabdesc,
	ULONG ulPartIndex,
	CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrPart,
	ULONG ulSecondaryPartIndexId,
	BOOL is_partial,
	CPartConstraint *ppartcnstr, 
	CPartConstraint *ppartcnstrRel
	)
	:
	CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, ulPartIndex, pdrgpcrOutput, pdrgpdrgpcrPart, ulSecondaryPartIndexId, is_partial, ppartcnstr, ppartcnstrRel)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::CLogicalDynamicGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicGet::CLogicalDynamicGet
	(
	CMemoryPool *mp,
	const CName *pnameAlias,
	CTableDescriptor *ptabdesc,
	ULONG ulPartIndex
	)
	:
	CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, ulPartIndex)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::~CLogicalDynamicGet
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CLogicalDynamicGet::~CLogicalDynamicGet()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalDynamicGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicGet::Matches
	(
	COperator *pop
	)
	const
{
	return CUtils::FMatchDynamicScan(this, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDynamicGet::PopCopyWithRemappedColumns
	(
	CMemoryPool *mp,
	UlongToColRefMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRefArray *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemapAndCreate(mp, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput, colref_mapping, must_exist);
	}
	CColRef2dArray *pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(mp, pdrgpcrOutput, m_ptabdesc->PdrgpulPart());
	CName *pnameAlias = GPOS_NEW(mp) CName(mp, *m_pnameAlias);
	m_ptabdesc->AddRef();

	CPartConstraint *ppartcnstr = m_part_constraint->PpartcnstrCopyWithRemappedColumns(mp, colref_mapping, must_exist);
	CPartConstraint *ppartcnstrRel = m_ppartcnstrRel->PpartcnstrCopyWithRemappedColumns(mp, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalDynamicGet(mp, pnameAlias, m_ptabdesc, m_scan_id, pdrgpcrOutput, pdrgpdrgpcrPart, m_ulSecondaryScanId, m_is_partial, ppartcnstr, ppartcnstrRel);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::FInputOrderSensitive
//
//	@doc:
//		Not called for leaf operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicGet::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDynamicGet::PxfsCandidates
	(
	CMemoryPool *mp
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfDynamicGet2DynamicTableScan);
	return xform_set;
}



//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalDynamicGet::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}
	else
	{
		os << SzId() << " ";
		// alias of table as referenced in the query
		m_pnameAlias->OsPrint(os);

		// actual name of table in catalog and columns
		os << " (";
		m_ptabdesc->Name().OsPrint(os);
		os <<"), ";
		m_part_constraint->OsPrint(os);
		os << "), Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
		os << "] Scan Id: " << m_scan_id << "." << m_ulSecondaryScanId;
		
		if (!m_part_constraint->IsConstraintUnbounded())
		{
			os << ", ";
			m_part_constraint->OsPrint(os);
		}
	}
		
	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::PstatsDerive
//
//	@doc:
//		Load up statistics from metadata
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDynamicGet::PstatsDerive
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl,
	IStatisticsArray * // not used
	)
	const
{
	CReqdPropRelational *prprel = CReqdPropRelational::GetReqdRelationalProps(exprhdl.Prp());
	IStatistics *stats = PstatsDeriveFilter(mp, exprhdl, prprel->PexprPartPred());

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrOutput);
	CUpperBoundNDVs *upper_bound_NDVs = GPOS_NEW(mp) CUpperBoundNDVs(pcrs, stats->Rows());
	CStatistics::CastStats(stats)->AddCardUpperBound(upper_bound_NDVs);

	return stats;
}

// EOF

