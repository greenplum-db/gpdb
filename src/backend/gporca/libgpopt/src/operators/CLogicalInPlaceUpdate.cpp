//---------------------------------------------------------------------------
//	@filename:
//		CLogicalInPlaceUpdate.cpp
//
//	@doc:
//		Implementation of logical InPlace Update operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalInPlaceUpdate.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::CLogicalInPlaceUpdate
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalInPlaceUpdate::CLogicalInPlaceUpdate(CMemoryPool *mp)
	: CLogical(mp),
	  m_ptabdesc(nullptr),
	  m_pdrgpcrInsert(nullptr),
	  m_pdrgpcrDelete(nullptr),
	  m_pcrCtid(nullptr),
	  m_pcrSegmentId(nullptr)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::CLogicalInPlaceUpdate
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalInPlaceUpdate::CLogicalInPlaceUpdate(
	CMemoryPool *mp, CTableDescriptor *ptabdesc, CColRefArray *pdrgpcrInsert,
	CColRefArray *pdrgpcrDelete, CColRef *pcrCtid, CColRef *pcrSegmentId)
	: CLogical(mp),
	  m_ptabdesc(ptabdesc),
	  m_pdrgpcrInsert(pdrgpcrInsert),
	  m_pdrgpcrDelete(pdrgpcrDelete),
	  m_pcrCtid(pcrCtid),
	  m_pcrSegmentId(pcrSegmentId)

{
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pdrgpcrInsert);
	GPOS_ASSERT(nullptr != pdrgpcrDelete);
	GPOS_ASSERT(pdrgpcrDelete->Size() == pdrgpcrInsert->Size());
	GPOS_ASSERT(nullptr != pcrCtid);
	GPOS_ASSERT(nullptr != pcrSegmentId);

	m_pcrsLocalUsed->Include(m_pdrgpcrInsert);
	m_pcrsLocalUsed->Include(m_pdrgpcrDelete);
	m_pcrsLocalUsed->Include(m_pcrCtid);
	m_pcrsLocalUsed->Include(m_pcrSegmentId);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::~CLogicalInPlaceUpdate
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalInPlaceUpdate::~CLogicalInPlaceUpdate()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrInsert);
	CRefCount::SafeRelease(m_pdrgpcrDelete);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalInPlaceUpdate::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalInPlaceUpdate *popUpdate = CLogicalInPlaceUpdate::PopConvert(pop);

	return m_pcrCtid == popUpdate->PcrCtid() &&
		   m_pcrSegmentId == popUpdate->PcrSegmentId() &&
		   m_ptabdesc->MDId()->Equals(popUpdate->Ptabdesc()->MDId()) &&
		   m_pdrgpcrInsert->Equals(popUpdate->PdrgpcrInsert()) &&
		   m_pdrgpcrDelete->Equals(popUpdate->PdrgpcrDelete());
}

//---------------------------------------------------------------------------
//	@function
//		CLogicalInPlaceUpdate::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalInPlaceUpdate::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrInsert));
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrDelete));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
	ulHash =
		gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalInPlaceUpdate::PopCopyWithRemappedColumns(
	CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist)
{
	CColRefArray *pdrgpcrDelete =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrDelete, colref_mapping, must_exist);
	CColRefArray *pdrgpcrInsert =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrInsert, colref_mapping, must_exist);
	CColRef *pcrCtid = CUtils::PcrRemap(m_pcrCtid, colref_mapping, must_exist);
	CColRef *pcrSegmentId =
		CUtils::PcrRemap(m_pcrSegmentId, colref_mapping, must_exist);
	m_ptabdesc->AddRef();

	return GPOS_NEW(mp) CLogicalInPlaceUpdate(
		mp, m_ptabdesc, pdrgpcrInsert, pdrgpcrDelete, pcrCtid, pcrSegmentId);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalInPlaceUpdate::DeriveOutputColumns(CMemoryPool *mp,
										   CExpressionHandle &	//exprhdl
)
{
	CColRefSet *pcrsOutput = GPOS_NEW(mp) CColRefSet(mp);
	pcrsOutput->Include(m_pdrgpcrInsert);
	pcrsOutput->Include(m_pcrCtid);
	pcrsOutput->Include(m_pcrSegmentId);

	return pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalInPlaceUpdate::DeriveKeyCollection(CMemoryPool *,  // mp
										   CExpressionHandle &exprhdl) const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalInPlaceUpdate::DeriveMaxCard(CMemoryPool *,	 // mp
									 CExpressionHandle &exprhdl) const
{
	// pass on max card of first child
	return exprhdl.DeriveMaxCard(0);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalInPlaceUpdate::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfInPlaceUpdate2DML);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalInPlaceUpdate::PstatsDerive(CMemoryPool *,	// mp,
									CExpressionHandle &exprhdl,
									IStatisticsArray *	// not used
) const
{
	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInPlaceUpdate::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalInPlaceUpdate::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " (";
	m_ptabdesc->Name().OsPrint(os);
	os << "), Delete Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrDelete);
	os << "], Insert Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrInsert);
	os << "], ";
	m_pcrCtid->OsPrint(os);
	os << ", ";
	m_pcrSegmentId->OsPrint(os);

	return os;
}

// EOF