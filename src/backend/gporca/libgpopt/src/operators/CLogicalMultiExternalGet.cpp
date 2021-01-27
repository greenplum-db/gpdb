//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CLogicalMultiExternalGet.cpp
//
//	@doc:
//		Implementation of external get
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalMultiExternalGet.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalMultiExternalGet::CLogicalMultiExternalGet
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalMultiExternalGet::CLogicalMultiExternalGet(CMemoryPool *mp)
	: CLogicalDynamicGetBase(mp), m_part_mdids(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalMultiExternalGet::CLogicalMultiExternalGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalMultiExternalGet::CLogicalMultiExternalGet(
	CMemoryPool *mp, IMdIdArray *part_mdids, const CName *pnameAlias,
	CTableDescriptor *ptabdesc, ULONG scan_id, CColRefArray *pdrgpcrOutput)
	: CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, scan_id, pdrgpcrOutput),
	  m_part_mdids(part_mdids)
{
	GPOS_ASSERT(NULL != m_part_mdids && m_part_mdids->Size() > 0);
}

CLogicalMultiExternalGet::~CLogicalMultiExternalGet()
{
	CRefCount::SafeRelease(m_part_mdids);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalMultiExternalGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalMultiExternalGet::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}
	CLogicalMultiExternalGet *popGet =
		CLogicalMultiExternalGet::PopConvert(pop);

	return Ptabdesc() == popGet->Ptabdesc() &&
		   PdrgpcrOutput()->Equals(popGet->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalMultiExternalGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalMultiExternalGet::PopCopyWithRemappedColumns(
	CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist)
{
	CColRefArray *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput =
			CUtils::PdrgpcrRemapAndCreate(mp, PdrgpcrOutput(), colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, PdrgpcrOutput(),
											 colref_mapping, must_exist);
	}
	CName *pnameAlias = GPOS_NEW(mp) CName(mp, Name());

	CTableDescriptor *ptabdesc = Ptabdesc();
	ptabdesc->AddRef();

	m_part_mdids->AddRef();

	return GPOS_NEW(mp) CLogicalMultiExternalGet(
		mp, m_part_mdids, pnameAlias, ptabdesc, ScanId(), pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalMultiExternalGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalMultiExternalGet::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(
		CXform::ExfMultiExternalGet2MultiExternalScan);

	return xform_set;
}

IStatistics *
CLogicalMultiExternalGet::PstatsDerive(CMemoryPool *mp,
									   CExpressionHandle &exprhdl,
									   IStatisticsArray *  // not used
) const
{
	// requesting stats on distribution columns to estimate data skew
	IStatistics *pstatsTable =
		PstatsBaseTable(mp, exprhdl, m_ptabdesc, m_pcrsDist);

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrOutput);
	CUpperBoundNDVs *upper_bound_NDVs =
		GPOS_NEW(mp) CUpperBoundNDVs(pcrs, pstatsTable->Rows());
	CStatistics::CastStats(pstatsTable)->AddCardUpperBound(upper_bound_NDVs);

	return pstatsTable;
}

// EOF
