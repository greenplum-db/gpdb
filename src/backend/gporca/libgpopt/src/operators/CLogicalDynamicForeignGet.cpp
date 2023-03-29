//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CLogicalDynamicForeignGet.cpp
//
//	@doc:
//		Implementation of foreign get
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalDynamicForeignGet.h"

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
//		CLogicalDynamicForeignGet::CLogicalDynamicForeignGet
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDynamicForeignGet::CLogicalDynamicForeignGet(CMemoryPool *mp)
	: CLogicalDynamicGetBase(mp)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicForeignGet::CLogicalDynamicForeignGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicForeignGet::CLogicalDynamicForeignGet(
	CMemoryPool *mp, const CName *pnameAlias, CTableDescriptor *ptabdesc,
	ULONG ulPartIndex, CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrPart, IMdIdArray *partition_mdids,
	OID foreign_server_oid, BOOL is_master_only)


	: CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, ulPartIndex,
							 pdrgpcrOutput, pdrgpdrgpcrPart, partition_mdids),
	  m_foreign_server_oid(foreign_server_oid),
	  m_is_master_only(is_master_only)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicForeignGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicForeignGet::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}
	CLogicalDynamicForeignGet *popGet =
		CLogicalDynamicForeignGet::PopConvert(pop);

	return Ptabdesc() == popGet->Ptabdesc() &&
		   PdrgpcrOutput()->Equals(popGet->PdrgpcrOutput()) &&
		   GetForeignServerOid() == popGet->GetForeignServerOid();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicForeignGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDynamicForeignGet::PopCopyWithRemappedColumns(
	CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist)
{
	CColRefArray *pdrgpcrOutput = nullptr;
	if (must_exist)
	{
		pdrgpcrOutput =
			CUtils::PdrgpcrRemapAndCreate(mp, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(mp, m_pdrgpcrOutput,
											 colref_mapping, must_exist);
	}
	CColRef2dArray *pdrgpdrgpcrPart =
		PdrgpdrgpcrCreatePartCols(mp, pdrgpcrOutput, m_ptabdesc->PdrgpulPart());
	CName *pnameAlias = GPOS_NEW(mp) CName(mp, *m_pnameAlias);
	m_ptabdesc->AddRef();
	m_partition_mdids->AddRef();

	return GPOS_NEW(mp) CLogicalDynamicForeignGet(
		mp, pnameAlias, m_ptabdesc, m_scan_id, pdrgpcrOutput, pdrgpdrgpcrPart,
		m_partition_mdids, m_foreign_server_oid, m_is_master_only);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicForeignGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDynamicForeignGet::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(
		CXform::ExfDynamicForeignGet2DynamicForeignScan);

	return xform_set;
}

IStatistics *
CLogicalDynamicForeignGet::PstatsDerive(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										IStatisticsArray *	// not used
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
