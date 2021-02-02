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

#include "gpopt/operators/CLogicalDynamicGet.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
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
CLogicalDynamicGet::CLogicalDynamicGet(CMemoryPool *mp)
	: CLogicalDynamicGetBase(mp)
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
CLogicalDynamicGet::CLogicalDynamicGet(CMemoryPool *mp, const CName *pnameAlias,
									   CTableDescriptor *ptabdesc,
									   ULONG ulPartIndex,
									   CColRefArray *pdrgpcrOutput,
									   CColRef2dArray *pdrgpdrgpcrPart,
									   IMdIdArray *partition_mdids)
	: CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, ulPartIndex,
							 pdrgpcrOutput, pdrgpdrgpcrPart),
	  m_partition_mdids(partition_mdids)
{
	m_root_col_mapping_per_part =
		ConstructRootColMappingPerPart(mp, m_pdrgpcrOutput, m_partition_mdids);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicGet::CLogicalDynamicGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicGet::CLogicalDynamicGet(CMemoryPool *mp, const CName *pnameAlias,
									   CTableDescriptor *ptabdesc,
									   ULONG ulPartIndex,
									   IMdIdArray *partition_mdids)
	: CLogicalDynamicGetBase(mp, pnameAlias, ptabdesc, ulPartIndex),
	  m_partition_mdids(partition_mdids)
{
	m_root_col_mapping_per_part =
		ConstructRootColMappingPerPart(mp, m_pdrgpcrOutput, m_partition_mdids);
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
	CRefCount::SafeRelease(m_partition_mdids);
	CRefCount::SafeRelease(m_root_col_mapping_per_part);
}


// Construct a mapping from each column in root table to an index in each child
// partition's table descr by matching column names
ColRefToUlongMapArray *
CLogicalDynamicGet::ConstructRootColMappingPerPart(CMemoryPool *mp,
												   CColRefArray *root_cols,
												   IMdIdArray *partition_mdids)
{
	CMDAccessor *mda = COptCtxt::PoctxtFromTLS()->Pmda();

	ColRefToUlongMapArray *part_maps = GPOS_NEW(mp) ColRefToUlongMapArray(mp);
	for (ULONG ul = 0; ul < partition_mdids->Size(); ++ul)
	{
		IMDId *part_mdid = (*partition_mdids)[ul];
		const IMDRelation *partrel = mda->RetrieveRel(part_mdid);

		GPOS_ASSERT(nullptr != partrel);

		ColRefToUlongMap *mapping = GPOS_NEW(mp) ColRefToUlongMap(mp);

		for (ULONG i = 0; i < root_cols->Size(); ++i)
		{
			CColRef *root_colref = (*root_cols)[i];

			BOOL found_match = false;
			for (ULONG j = 0, idx = 0; j < partrel->ColumnCount(); ++j, ++idx)
			{
				const IMDColumn *coldesc = partrel->GetMdCol(j);
				const CWStringConst *colname = coldesc->Mdname().GetMDName();

				// Skip dropped columns of the child partition since its table descr is
				// generated without dropped columns
				if (coldesc->IsDropped())
				{
					--idx;
					continue;
				}

				if (colname->Equals(root_colref->Name().Pstr()))
				{
					// Found the corresponding column in the child partition
					// Save the index in the mapping
					mapping->Insert(root_colref, GPOS_NEW(mp) ULONG(idx));
					found_match = true;
					break;
				}
			}

			if (!found_match)
			{
				GPOS_RAISE(
					CException::ExmaInvalid, CException::ExmiInvalid,
					GPOS_WSZ_LIT(
						"Cannot generate root to child partition column mapping"));
			}
		}
		part_maps->Append(mapping);
	}
	return part_maps;
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
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

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
CLogicalDynamicGet::Matches(COperator *pop) const
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
CLogicalDynamicGet::PopCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist)
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

	return GPOS_NEW(mp)
		CLogicalDynamicGet(mp, pnameAlias, m_ptabdesc, m_scan_id, pdrgpcrOutput,
						   pdrgpdrgpcrPart, m_partition_mdids);
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

CMaxCard
CLogicalDynamicGet::DeriveMaxCard(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	if (nullptr == GetPartitionMdids() || GetPartitionMdids()->Size() == 0)
	{
		return CMaxCard(0);
	}

	return CLogical::DeriveMaxCard(mp, exprhdl);
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
CLogicalDynamicGet::PxfsCandidates(CMemoryPool *mp) const
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
CLogicalDynamicGet::OsPrint(IOstream &os) const
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
		os << "), ";
		os << "Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
		os << "] Scan Id: " << m_scan_id;
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
CLogicalDynamicGet::PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
								 IStatisticsArray *	 // not used
) const
{
	CReqdPropRelational *prprel =
		CReqdPropRelational::GetReqdRelationalProps(exprhdl.Prp());
	IStatistics *stats =
		PstatsDeriveFilter(mp, exprhdl, prprel->PexprPartPred());

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrOutput);
	CUpperBoundNDVs *upper_bound_NDVs =
		GPOS_NEW(mp) CUpperBoundNDVs(pcrs, stats->Rows());
	CStatistics::CastStats(stats)->AddCardUpperBound(upper_bound_NDVs);

	return stats;
}

// EOF
