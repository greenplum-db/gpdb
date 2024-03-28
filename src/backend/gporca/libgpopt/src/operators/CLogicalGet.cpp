//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalGet.cpp
//
//	@doc:
//		Implementation of basic table access
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalGet.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet(CMemoryPool *mp)
	: CLogical(mp),
	  m_pnameAlias(nullptr),
	  m_ptabdesc(GPOS_NEW(mp) CTableDescriptorHashSet(mp)),
	  m_pdrgpcrOutput(nullptr),
	  m_pdrgpdrgpcrPart(nullptr),
	  m_pcrsDist(nullptr)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet(CMemoryPool *mp, const CName *pnameAlias,
						 CTableDescriptor *ptabdesc, BOOL hasSecurityQuals)
	: CLogicalGet(mp, pnameAlias, ptabdesc,
				  PdrgpcrCreateMapping(mp, ptabdesc->Pdrgpcoldesc(),
									   // XXX: UlOpId() isn't valid yet..
									   COperator::m_aulOpIdCounter + 1,
									   ptabdesc->MDId()),
				  hasSecurityQuals)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::CLogicalGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalGet::CLogicalGet(CMemoryPool *mp, const CName *pnameAlias,
						 CTableDescriptor *ptabdesc,
						 CColRefArray *pdrgpcrOutput, BOOL hasSecurityQuals)
	: CLogical(mp),
	  m_pnameAlias(pnameAlias),
	  m_ptabdesc(GPOS_NEW(mp) CTableDescriptorHashSet(mp)),
	  m_pdrgpcrOutput(pdrgpcrOutput),
	  m_pdrgpdrgpcrPart(nullptr),
	  m_has_security_quals(hasSecurityQuals)
{
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pnameAlias);

	m_ptabdesc->Insert(ptabdesc);

	if (Ptabdesc()->IsPartitioned())
	{
		m_pdrgpdrgpcrPart = PdrgpdrgpcrCreatePartCols(
			mp, m_pdrgpcrOutput, Ptabdesc()->PdrgpulPart());
	}

	m_pcrsDist = CLogical::PcrsDist(mp, Ptabdesc(), m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::~CLogicalGet
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CLogicalGet::~CLogicalGet()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrOutput);
	CRefCount::SafeRelease(m_pdrgpdrgpcrPart);
	CRefCount::SafeRelease(m_pcrsDist);

	GPOS_DELETE(m_pnameAlias);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   Ptabdesc()->MDId()->HashValue());
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	ulHash = gpos::CombineHashes(ulHash,
								 gpos::HashValue<BOOL>(&m_has_security_quals));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalGet::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}
	CLogicalGet *popGet = CLogicalGet::PopConvert(pop);

	return Ptabdesc()->MDId()->Equals(popGet->Ptabdesc()->MDId()) &&
		   m_pdrgpcrOutput->Equals(popGet->PdrgpcrOutput()) &&
		   m_has_security_quals == popGet->HasSecurityQuals();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalGet::PopCopyWithRemappedColumns(CMemoryPool *mp,
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
	CName *pnameAlias = GPOS_NEW(mp) CName(mp, *m_pnameAlias);
	Ptabdesc()->AddRef();

	return GPOS_NEW(mp) CLogicalGet(mp, pnameAlias, Ptabdesc(), pdrgpcrOutput,
									m_has_security_quals);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalGet::DeriveOutputColumns(CMemoryPool *mp,
								 CExpressionHandle &  // exprhdl
)
{
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	for (ULONG i = 0; i < m_pdrgpcrOutput->Size(); i++)
	{
		// We want to limit the output columns to only those which are referenced in the query
		// We will know the entire list of columns which are referenced in the query only after
		// translating the entire DXL to an expression. Hence we should not limit the output columns
		// before we have processed the entire DXL.
		if ((*m_pdrgpcrOutput)[i]->GetUsage() == CColRef::EUsed ||
			(*m_pdrgpcrOutput)[i]->GetUsage() == CColRef::EUnknown)
		{
			pcrs->Include((*m_pdrgpcrOutput)[i]);
		}
	}

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::DeriveNotNullColumns
//
//	@doc:
//		Derive not null output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalGet::DeriveNotNullColumns(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	// get all output columns
	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp);
	pcrs->Include(exprhdl.DeriveOutputColumns());

	// filters out nullable columns
	CColRefSetIter crsi(*exprhdl.DeriveOutputColumns());
	while (crsi.Advance())
	{
		CColRefTable *pcrtable = CColRefTable::PcrConvert(crsi.Pcr());
		if (pcrtable->IsNullable())
		{
			pcrs->Exclude(pcrtable);
		}
	}

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::FInputOrderSensitive
//
//	@doc:
//		Not called for leaf operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalGet::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalGet::DeriveKeyCollection(CMemoryPool *mp,
								 CExpressionHandle &  // exprhdl
) const
{
	const CBitSetArray *pdrgpbs = Ptabdesc()->PdrgpbsKeys();

	return CLogical::PkcKeysBaseTable(mp, pdrgpbs, m_pdrgpcrOutput);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalGet::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfGet2TableScan);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::PstatsDerive
//
//	@doc:
//		Load up statistics from metadata
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalGet::PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  IStatisticsArray *  // not used
) const
{
	// requesting stats on distribution columns to estimate data skew
	IStatistics *pstatsTable =
		PstatsBaseTable(mp, exprhdl, Ptabdesc(), m_pcrsDist);

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, m_pdrgpcrOutput);
	CUpperBoundNDVs *upper_bound_NDVs =
		GPOS_NEW(mp) CUpperBoundNDVs(pcrs, pstatsTable->Rows());
	CStatistics::CastStats(pstatsTable)->AddCardUpperBound(upper_bound_NDVs);

	return pstatsTable;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalGet::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalGet::OsPrint(IOstream &os) const
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
		Ptabdesc()->Name().OsPrint(os);
		os << "), Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
		os << "] Key sets: {";

		const ULONG ulColumns = m_pdrgpcrOutput->Size();
		const CBitSetArray *pdrgpbsKeys = Ptabdesc()->PdrgpbsKeys();
		for (ULONG ul = 0; ul < pdrgpbsKeys->Size(); ul++)
		{
			CBitSet *pbs = (*pdrgpbsKeys)[ul];
			if (0 < ul)
			{
				os << ", ";
			}
			os << "[";
			ULONG ulPrintedKeys = 0;
			for (ULONG ulKey = 0; ulKey < ulColumns; ulKey++)
			{
				if (pbs->Get(ulKey))
				{
					if (0 < ulPrintedKeys)
					{
						os << ",";
					}
					os << ulKey;
					ulPrintedKeys++;
				}
			}
			os << "]";
		}
		os << "}";
	}

	return os;
}



// EOF
