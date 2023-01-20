//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDML.cpp
//
//	@doc:
//		Implementation of logical DML operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalDML.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::CLogicalDML
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDML::CLogicalDML(CMemoryPool *mp)
	: CLogical(mp),
	  m_ptabdesc(nullptr),
	  m_pdrgpcrSource(nullptr),
	  m_pbsModified(nullptr),
	  m_pcrAction(nullptr),
	  m_pcrCtid(nullptr),
	  m_pcrSegmentId(nullptr),
	  m_fSplit(true)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::CLogicalDML
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDML::CLogicalDML(CMemoryPool *mp, EDMLOperator edmlop,
						 CTableDescriptor *ptabdesc,
						 CColRefArray *pdrgpcrSource, CBitSet *pbsModified,
						 CColRef *pcrAction, CColRef *pcrCtid,
						 CColRef *pcrSegmentId, BOOL fSplit)
	: CLogical(mp),
	  m_edmlop(edmlop),
	  m_ptabdesc(ptabdesc),
	  m_pdrgpcrSource(pdrgpcrSource),
	  m_pbsModified(pbsModified),
	  m_pcrAction(pcrAction),
	  m_pcrCtid(pcrCtid),
	  m_pcrSegmentId(pcrSegmentId),
	  m_fSplit(fSplit)
{
	GPOS_ASSERT(EdmlSentinel != edmlop);
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pdrgpcrSource);
	GPOS_ASSERT(nullptr != pbsModified);
	GPOS_ASSERT(nullptr != pcrAction);
	GPOS_ASSERT_IMP(EdmlDelete == edmlop || EdmlUpdate == edmlop,
					nullptr != pcrCtid && nullptr != pcrSegmentId);

	m_pcrsLocalUsed->Include(m_pdrgpcrSource);
	m_pcrsLocalUsed->Include(m_pcrAction);
	if (nullptr != m_pcrCtid)
	{
		m_pcrsLocalUsed->Include(m_pcrCtid);
	}

	if (nullptr != m_pcrSegmentId)
	{
		m_pcrsLocalUsed->Include(m_pcrSegmentId);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::~CLogicalDML
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalDML::~CLogicalDML()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrSource);
	CRefCount::SafeRelease(m_pbsModified);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalDML::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalDML *popDML = CLogicalDML::PopConvert(pop);

	return m_pcrAction == popDML->PcrAction() &&
		   m_pcrCtid == popDML->PcrCtid() &&
		   m_pcrSegmentId == popDML->PcrSegmentId() &&
		   m_ptabdesc->MDId()->Equals(popDML->Ptabdesc()->MDId()) &&
		   m_pdrgpcrSource->Equals(popDML->PdrgpcrSource()) &&
		   m_fSplit == popDML->FSplit();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalDML::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrAction));
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrSource));

	if (EdmlDelete == m_edmlop || EdmlUpdate == m_edmlop)
	{
		ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
		ulHash =
			gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));
	}

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDML::PopCopyWithRemappedColumns(CMemoryPool *mp,
										UlongToColRefMap *colref_mapping,
										BOOL must_exist)
{
	CColRefArray *colref_array =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrSource, colref_mapping, must_exist);
	CColRef *pcrAction =
		CUtils::PcrRemap(m_pcrAction, colref_mapping, must_exist);

	// no need to remap modified columns bitset as it represent column indexes
	// and not actual columns
	m_pbsModified->AddRef();

	CColRef *pcrCtid = nullptr;
	if (nullptr != m_pcrCtid)
	{
		pcrCtid = CUtils::PcrRemap(m_pcrCtid, colref_mapping, must_exist);
	}

	CColRef *pcrSegmentId = nullptr;
	if (nullptr != m_pcrSegmentId)
	{
		pcrSegmentId =
			CUtils::PcrRemap(m_pcrSegmentId, colref_mapping, must_exist);
	}

	m_ptabdesc->AddRef();

	return GPOS_NEW(mp)
		CLogicalDML(mp, m_edmlop, m_ptabdesc, colref_array, m_pbsModified,
					pcrAction, pcrCtid, pcrSegmentId, m_fSplit);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalDML::DeriveOutputColumns(CMemoryPool *mp,
								 CExpressionHandle &  //exprhdl
)
{
	CColRefSet *pcrsOutput = GPOS_NEW(mp) CColRefSet(mp);
	pcrsOutput->Include(m_pdrgpcrSource);
	if (nullptr != m_pcrCtid)
	{
		GPOS_ASSERT(nullptr != m_pcrSegmentId);
		pcrsOutput->Include(m_pcrCtid);
		pcrsOutput->Include(m_pcrSegmentId);
	}

	pcrsOutput->Include(m_pcrAction);

	return pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::DerivePropertyConstraint
//
//	@doc:
//		Derive constraint property
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogicalDML::DerivePropertyConstraint(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) const
{
	CColRefSet *pcrsOutput = GPOS_NEW(mp) CColRefSet(mp);
	pcrsOutput->Include(m_pdrgpcrSource);
	CPropConstraint *ppc = PpcDeriveConstraintRestrict(mp, exprhdl, pcrsOutput);
	pcrsOutput->Release();

	return ppc;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalDML::DeriveKeyCollection(CMemoryPool *,	 // mp
								 CExpressionHandle &exprhdl) const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalDML::DeriveMaxCard(CMemoryPool *,  // mp
						   CExpressionHandle &exprhdl) const
{
	// pass on max card of first child
	return exprhdl.DeriveMaxCard(0);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDML::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfImplementDML);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDML::PstatsDerive(CMemoryPool *,  // mp,
						  CExpressionHandle &exprhdl,
						  IStatisticsArray *  // not used
) const
{
	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalDML::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " (";
	m_ptabdesc->Name().OsPrint(os);
	CLogicalDML::PrintOperatorType(os, m_edmlop, m_fSplit);
	os << "Affected Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrSource);
	os << "], Action: (";
	m_pcrAction->OsPrint(os);
	os << ")";

	if (EdmlDelete == m_edmlop || EdmlUpdate == m_edmlop)
	{
		os << ", ";
		m_pcrCtid->OsPrint(os);
		os << ", ";
		m_pcrSegmentId->OsPrint(os);
	}

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDML::PrintOperatorType
//
//	@doc:
//		Helper function to print DML operator type based on the given operator
//	    enum, used in OSPrint to print objects.
//
//---------------------------------------------------------------------------
void
CLogicalDML::PrintOperatorType(IOstream &os, EDMLOperator edmlOperator,
							   BOOL fSplit)
{
	switch (edmlOperator)
	{
		case EdmlInsert:
			os << "), Insert, ";
			break;

		case EdmlDelete:
			os << "), Delete, ";
			break;

		case EdmlUpdate:
			if (fSplit)
			{
				os << "), Split Update, ";
			}
			else
			{
				os << "), In-place Update, ";
			}
			break;

		default:
			GPOS_ASSERT(!"Unrecognized DML Operator");
			break;
	}
}

// EOF
