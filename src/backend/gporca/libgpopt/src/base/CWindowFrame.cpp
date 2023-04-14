//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CWindowFrame.cpp
//
//	@doc:
//		Implementation of window frame
//---------------------------------------------------------------------------

#include "gpopt/base/CWindowFrame.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CScalarIdent.h"


using namespace gpopt;

FORCE_GENERATE_DBGSTR(CWindowFrame);

// string encoding of frame specification
const CHAR rgszFrameSpec[][10] = {"Rows", "Range", "Groups"};
GPOS_CPL_ASSERT(CWindowFrame::EfsSentinel == GPOS_ARRAY_SIZE(rgszFrameSpec));

// string encoding of frame boundary
const CHAR rgszFrameBoundary[][40] = {"Unbounded Preceding",
									  "Bounded Preceding",
									  "Current",
									  "Unbounded Following",
									  "Bounded Following",
									  "Delayed Bounded Preceding",
									  "Delayed Bounded Following"};
GPOS_CPL_ASSERT(CWindowFrame::EfbSentinel ==
				GPOS_ARRAY_SIZE(rgszFrameBoundary));

// string encoding of frame exclusion strategy
const CHAR rgszFrameExclusionStrategy[][20] = {"None", "Nulls", "Current",
											   "MatchingOthers", "Ties"};
GPOS_CPL_ASSERT(CWindowFrame::EfesSentinel ==
				GPOS_ARRAY_SIZE(rgszFrameExclusionStrategy));

// empty window frame
const CWindowFrame CWindowFrame::m_wfEmpty;

//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::CWindowFrame
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CWindowFrame::CWindowFrame(
	CMemoryPool *mp, EFrameSpec efs, EFrameBoundary efbLeading,
	EFrameBoundary efbTrailing, CExpression *pexprLeading,
	CExpression *pexprTrailing, EFrameExclusionStrategy efes,
	OID start_in_range_func, OID end_in_range_func, OID in_range_coll,
	bool in_range_asc, bool in_range_nulls_first)
	: m_efs(efs),
	  m_efbLeading(efbLeading),
	  m_efbTrailing(efbTrailing),
	  m_pexprLeading(pexprLeading),
	  m_pexprTrailing(pexprTrailing),
	  m_efes(efes),
	  m_start_in_range_func(start_in_range_func),
	  m_end_in_range_func(end_in_range_func),
	  m_in_range_coll(in_range_coll),
	  m_in_range_asc(in_range_asc),
	  m_in_range_nulls_first(in_range_nulls_first)
{
	GPOS_ASSERT_IMP(EfbBoundedPreceding == m_efbLeading ||
						EfbBoundedFollowing == m_efbLeading,
					nullptr != pexprLeading);
	GPOS_ASSERT_IMP(EfbBoundedPreceding == m_efbTrailing ||
						EfbBoundedFollowing == m_efbTrailing,
					nullptr != pexprTrailing);

	// include used columns by frame edges
	m_pcrsUsed = GPOS_NEW(mp) CColRefSet(mp);
	if (nullptr != pexprLeading)
	{
		m_pcrsUsed->Include(pexprLeading->DeriveUsedColumns());
	}

	if (nullptr != pexprTrailing)
	{
		m_pcrsUsed->Include(pexprTrailing->DeriveUsedColumns());
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::CWindowFrame
//
//	@doc:
//		Private dummy ctor used for creating empty frame
//
//---------------------------------------------------------------------------
CWindowFrame::CWindowFrame()



	= default;


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::~CWindowFrame
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CWindowFrame::~CWindowFrame()
{
	CRefCount::SafeRelease(m_pexprLeading);
	CRefCount::SafeRelease(m_pexprTrailing);
	CRefCount::SafeRelease(m_pcrsUsed);
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::Matches
//
//	@doc:
//		Check for equality between window frames
//
//---------------------------------------------------------------------------
BOOL
CWindowFrame::Matches(const CWindowFrame *pwf) const
{
	return m_efs == pwf->Efs() && m_efbLeading == pwf->EfbLeading() &&
		   m_efbTrailing == pwf->EfbTrailing() && m_efes == pwf->Efes() &&
		   CUtils::Equals(m_pexprLeading, pwf->PexprLeading()) &&
		   CUtils::Equals(m_pexprTrailing, pwf->PexprTrailing());
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::HashValue
//
//	@doc:
//		Hash of components
//
//---------------------------------------------------------------------------
ULONG
CWindowFrame::HashValue() const
{
	ULONG ulHash = 0;
	ulHash = gpos::CombineHashes(ulHash, m_efs);
	ulHash = gpos::CombineHashes(ulHash, m_efbLeading);
	ulHash = gpos::CombineHashes(ulHash, m_efbTrailing);
	ulHash = gpos::CombineHashes(ulHash, m_efes);
	if (nullptr != m_pexprLeading)
	{
		ulHash =
			gpos::CombineHashes(ulHash, CExpression::HashValue(m_pexprLeading));
	}

	if (nullptr != m_pexprTrailing)
	{
		ulHash = gpos::CombineHashes(ulHash,
									 CExpression::HashValue(m_pexprTrailing));
	}

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::PwfCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the window frame with remapped columns
//
//---------------------------------------------------------------------------
CWindowFrame *
CWindowFrame::PwfCopyWithRemappedColumns(CMemoryPool *mp,
										 UlongToColRefMap *colref_mapping,
										 BOOL must_exist)
{
	if (this == &m_wfEmpty)
	{
		this->AddRef();
		return this;
	}

	CExpression *pexprLeading = nullptr;
	if (nullptr != m_pexprLeading)
	{
		pexprLeading = m_pexprLeading->PexprCopyWithRemappedColumns(
			mp, colref_mapping, must_exist);
	}

	CExpression *pexprTrailing = nullptr;
	if (nullptr != m_pexprTrailing)
	{
		pexprTrailing = m_pexprTrailing->PexprCopyWithRemappedColumns(
			mp, colref_mapping, must_exist);
	}

	return GPOS_NEW(mp) CWindowFrame(
		mp, m_efs, m_efbLeading, m_efbTrailing, pexprLeading, pexprTrailing,
		m_efes, m_start_in_range_func, m_end_in_range_func, m_in_range_coll,
		m_in_range_asc, m_in_range_nulls_first);
}

//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::OsPrint
//
//	@doc:
//		Print window frame
//
//---------------------------------------------------------------------------
IOstream &
CWindowFrame::OsPrint(IOstream &os) const
{
	if (this == &m_wfEmpty)
	{
		os << "EMPTY FRAME";
		return os;
	}

	os << "[" << rgszFrameSpec[m_efs] << ", ";

	os << "Trail: " << rgszFrameBoundary[m_efbTrailing];
	if (nullptr != m_pexprTrailing)
	{
		os << " " << *m_pexprTrailing;
	}

	os << ", Lead: " << rgszFrameBoundary[m_efbLeading];
	if (nullptr != m_pexprLeading)
	{
		os << " " << *m_pexprLeading;
	}

	os << ", " << rgszFrameExclusionStrategy[m_efes];

	os << "]";

	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::Equals
//
//	@doc:
//		 Matching function over frame arrays
//
//---------------------------------------------------------------------------
BOOL
CWindowFrame::Equals(const CWindowFrameArray *pdrgpwfFirst,
					 const CWindowFrameArray *pdrgpwfSecond)
{
	if (nullptr == pdrgpwfFirst || nullptr == pdrgpwfSecond)
	{
		return (nullptr == pdrgpwfFirst && nullptr == pdrgpwfSecond);
	}

	if (pdrgpwfFirst->Size() != pdrgpwfSecond->Size())
	{
		return false;
	}

	const ULONG size = pdrgpwfFirst->Size();
	BOOL fMatch = true;
	for (ULONG ul = 0; fMatch && ul < size; ul++)
	{
		fMatch = (*pdrgpwfFirst)[ul]->Matches((*pdrgpwfSecond)[ul]);
	}

	return fMatch;
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::HashValue
//
//	@doc:
//		 Combine hash values of a maximum number of entries
//
//---------------------------------------------------------------------------
ULONG
CWindowFrame::HashValue(const CWindowFrameArray *pdrgpwf, ULONG ulMaxSize)
{
	GPOS_ASSERT(nullptr != pdrgpwf);
	const ULONG size = std::min(ulMaxSize, pdrgpwf->Size());

	ULONG ulHash = 0;
	for (ULONG ul = 0; ul < size; ul++)
	{
		ulHash = gpos::CombineHashes(ulHash, (*pdrgpwf)[ul]->HashValue());
	}

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CWindowFrame::OsPrint
//
//	@doc:
//		 Print array of window frame objects
//
//---------------------------------------------------------------------------
IOstream &
CWindowFrame::OsPrint(IOstream &os, const CWindowFrameArray *pdrgpwf)
{
	os << "[";
	const ULONG size = pdrgpwf->Size();
	if (0 < size)
	{
		for (ULONG ul = 0; ul < size - 1; ul++)
		{
			(void) (*pdrgpwf)[ul]->OsPrint(os);
			os << ", ";
		}

		(void) (*pdrgpwf)[size - 1]->OsPrint(os);
	}

	return os << "]";
}


// EOF
