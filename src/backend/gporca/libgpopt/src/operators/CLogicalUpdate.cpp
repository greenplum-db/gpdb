//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUpdate.cpp
//
//	@doc:
//		Implementation of logical Update operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalUpdate.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::CLogicalUpdate
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalUpdate::CLogicalUpdate(CMemoryPool *mp)
	: CLogical(mp),
	  m_ptabdesc(nullptr),
	  m_pdrgpcrDelete(nullptr),
	  m_pdrgpcrInsert(nullptr),
	  m_pcrCtid(nullptr),
	  m_pcrSegmentId(nullptr),
	  m_fSplit(true)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::CLogicalUpdate
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalUpdate::CLogicalUpdate(CMemoryPool *mp, CTableDescriptor *ptabdesc,
							   CColRefArray *pdrgpcrDelete,
							   CColRefArray *pdrgpcrInsert, CColRef *pcrCtid,
							   CColRef *pcrSegmentId, BOOL fSplit)
	: CLogical(mp),
	  m_ptabdesc(ptabdesc),
	  m_pdrgpcrDelete(pdrgpcrDelete),
	  m_pdrgpcrInsert(pdrgpcrInsert),
	  m_pcrCtid(pcrCtid),
	  m_pcrSegmentId(pcrSegmentId),
	  m_fSplit(fSplit)

{
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pdrgpcrDelete);
	GPOS_ASSERT(nullptr != pdrgpcrInsert);
	GPOS_ASSERT(pdrgpcrDelete->Size() == pdrgpcrInsert->Size());
	GPOS_ASSERT(nullptr != pcrCtid);
	GPOS_ASSERT(nullptr != pcrSegmentId);

	m_pcrsLocalUsed->Include(m_pdrgpcrDelete);
	m_pcrsLocalUsed->Include(m_pdrgpcrInsert);
	m_pcrsLocalUsed->Include(m_pcrCtid);
	m_pcrsLocalUsed->Include(m_pcrSegmentId);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::~CLogicalUpdate
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalUpdate::~CLogicalUpdate()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrDelete);
	CRefCount::SafeRelease(m_pdrgpcrInsert);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalUpdate::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalUpdate *popUpdate = CLogicalUpdate::PopConvert(pop);

	return m_pcrCtid == popUpdate->PcrCtid() &&
		   m_pcrSegmentId == popUpdate->PcrSegmentId() &&
		   m_ptabdesc->MDId()->Equals(popUpdate->Ptabdesc()->MDId()) &&
		   m_pdrgpcrDelete->Equals(popUpdate->PdrgpcrDelete()) &&
		   m_pdrgpcrInsert->Equals(popUpdate->PdrgpcrInsert()) &&
		   m_fSplit == popUpdate->FSplit();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalUpdate::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrDelete));
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrInsert));
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
	ulHash =
		gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalUpdate::PopCopyWithRemappedColumns(CMemoryPool *mp,
										   UlongToColRefMap *colref_mapping,
										   BOOL must_exist)
{
	CColRefArray *pdrgpcrDelete =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrDelete, colref_mapping, must_exist);
	CColRefArray *pdrgpcrInsert =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrInsert, colref_mapping, must_exist);
	CColRef *pcrCtid = CUtils::PcrRemap(m_pcrCtid, colref_mapping, must_exist);
	CColRef *pcrSegmentId =
		CUtils::PcrRemap(m_pcrSegmentId, colref_mapping, must_exist);
	m_ptabdesc->AddRef();

	return GPOS_NEW(mp)
		CLogicalUpdate(mp, m_ptabdesc, pdrgpcrDelete, pdrgpcrInsert, pcrCtid,
					   pcrSegmentId, m_fSplit);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::DeriveOutputColumns
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalUpdate::DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &	 //exprhdl
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
//		CLogicalUpdate::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalUpdate::DeriveKeyCollection(CMemoryPool *,	// mp
									CExpressionHandle &exprhdl) const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalUpdate::DeriveMaxCard(CMemoryPool *,  // mp
							  CExpressionHandle &exprhdl) const
{
	// pass on max card of first child
	return exprhdl.DeriveMaxCard(0);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalUpdate::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfUpdate2DML);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalUpdate::PstatsDerive(CMemoryPool *,	 // mp,
							 CExpressionHandle &exprhdl,
							 IStatisticsArray *	 // not used
) const
{
	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalUpdate::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalUpdate::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}
	os << SzId() << " (";
	m_ptabdesc->Name().OsPrint(os);
	if (m_fSplit)
	{
		os << "), Split Update";
	}
	else
	{
		os << "), In-place Update";
	}
	os << ", Delete Columns: [";
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
