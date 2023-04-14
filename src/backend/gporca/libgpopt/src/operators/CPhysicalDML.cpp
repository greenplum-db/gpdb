//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDML.cpp
//
//	@doc:
//		Implementation of physical DML operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalDML.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecRouted.h"
#include "gpopt/base/CDistributionSpecStrictRandom.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::CPhysicalDML
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDML::CPhysicalDML(CMemoryPool *mp, CLogicalDML::EDMLOperator edmlop,
						   CTableDescriptor *ptabdesc,
						   CColRefArray *pdrgpcrSource, CBitSet *pbsModified,
						   CColRef *pcrAction, CColRef *pcrCtid,
						   CColRef *pcrSegmentId, BOOL fSplit)
	: CPhysical(mp),
	  m_edmlop(edmlop),
	  m_ptabdesc(ptabdesc),
	  m_pdrgpcrSource(pdrgpcrSource),
	  m_pbsModified(pbsModified),
	  m_pcrAction(pcrAction),
	  m_pcrCtid(pcrCtid),
	  m_pcrSegmentId(pcrSegmentId),
	  m_pds(nullptr),
	  m_pos(nullptr),
	  m_pcrsRequiredLocal(nullptr),
	  m_fSplit(fSplit)
{
	GPOS_ASSERT(CLogicalDML::EdmlSentinel != edmlop);
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pdrgpcrSource);
	GPOS_ASSERT(nullptr != pbsModified);
	GPOS_ASSERT(nullptr != pcrAction);
	GPOS_ASSERT_IMP(
		CLogicalDML::EdmlDelete == edmlop || CLogicalDML::EdmlUpdate == edmlop,
		nullptr != pcrCtid && nullptr != pcrSegmentId);

	m_pds =
		CPhysical::PdsCompute(m_mp, m_ptabdesc, pdrgpcrSource, pcrSegmentId);

	if (CDistributionSpec::EdtHashed == m_pds->Edt() &&
		ptabdesc->ConvertHashToRandom())
	{
		// The "convert hash to random" flag indicates that we have a table that was hash-partitioned
		// originally but then we either entered phase 1 of a gpexpand or we altered some of the partitions
		// to be randomly distributed (works on GPDB 5X only).
		// If this is the case, we want to handle DMLs in the following way:
		//
		// Insert: Use a hash redistribution for the insert, that means that we insert the data into
		//         the random partitions using a hash function, which can still be considered "random"
		// Delete: Use a "strict random" distribution, which will use a routed repartition operator,
		//         based on the gp_segment_id of the row, which will work for both hash and random partitions
		// Update without updating the distribution key: Same method as for delete
		// Update of the distribution key: This will be handled with a Split node below the DML node,
		//         with the split deleting the existing rows and this DML node inserting the new rows,
		//         so this is handled here like an insert, using hash distribution for all partitions.

		if (CLogicalDML::EdmlDelete == edmlop || !fSplit)
		{
			m_pds->Release();
			m_pds = GPOS_NEW(mp) CDistributionSpecRandom();
		}
	}
	m_pos = PosComputeRequired(mp, ptabdesc);
	ComputeRequiredLocalColumns(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::~CPhysicalDML
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalDML::~CPhysicalDML()
{
	m_ptabdesc->Release();
	m_pdrgpcrSource->Release();
	m_pbsModified->Release();
	m_pds->Release();
	m_pos->Release();
	m_pcrsRequiredLocal->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PosRequired
//
//	@doc:
//		Compute required sort columns of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalDML::PosRequired(CMemoryPool *,		// mp
						  CExpressionHandle &,	// exprhdl
						  COrderSpec *,			// posRequired
						  ULONG
#ifdef GPOS_DEBUG
							  child_index
#endif	// GPOS_DEBUG
						  ,
						  CDrvdPropArray *,	 // pdrgpdpCtxt
						  ULONG				 // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);
	m_pos->AddRef();
	return m_pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalDML::PosDerive(CMemoryPool *mp,
						CExpressionHandle &	 // exprhdl
) const
{
	// return empty sort order
	return GPOS_NEW(mp) COrderSpec(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalDML::EpetOrder(CExpressionHandle &exprhdl, const CEnfdOrder *peo) const
{
	GPOS_ASSERT(nullptr != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	// get the order delivered by the DML node
	COrderSpec *pos = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pos();
	if (peo->FCompatible(pos))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	// required order will be enforced on limit's output
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalDML::PcrsRequired(CMemoryPool *mp,
						   CExpressionHandle &,	 // exprhdl,
						   CColRefSet *pcrsRequired,
						   ULONG
#ifdef GPOS_DEBUG
							   child_index
#endif	// GPOS_DEBUG
						   ,
						   CDrvdPropArray *,  // pdrgpdpCtxt
						   ULONG			  // ulOptReq
)
{
	GPOS_ASSERT(
		0 == child_index &&
		"Required properties can only be computed on the relational child");

	CColRefSet *pcrs = GPOS_NEW(mp) CColRefSet(mp, *m_pcrsRequiredLocal);
	pcrs->Union(pcrsRequired);

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalDML::PdsRequired(CMemoryPool *mp,
						  CExpressionHandle &,	// exprhdl,
						  CDistributionSpec *,	// pdsInput,
						  ULONG
#ifdef GPOS_DEBUG
							  child_index
#endif	// GPOS_DEBUG
						  ,
						  CDrvdPropArray *,	 // pdrgpdpCtxt
						  ULONG				 // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	if (CDistributionSpec::EdtRandom == m_pds->Edt())
	{
		// if insert is performed on a randomly distributed table,
		// request strict random spec to ensure that CPhysicalMotionRandom
		// exists prior to CPhysicalDML (Insert),
		// CPhysicalMotionRandom ensures that there is no skew in the
		// data inserted
		if (CLogicalDML::EdmlInsert == m_edmlop)
		{
			return GPOS_NEW(mp) CDistributionSpecStrictRandom();
		}
		return GPOS_NEW(mp) CDistributionSpecRouted(m_pcrSegmentId);
	}

	m_pds->AddRef();
	return m_pds;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalDML::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CRewindabilitySpec *prsRequired, ULONG child_index,
						  CDrvdPropArray *,	 // pdrgpdpCtxt
						  ULONG				 // ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalDML::PcteRequired(CMemoryPool *,		 //mp,
						   CExpressionHandle &,	 //exprhdl,
						   CCTEReq *pcter,
						   ULONG
#ifdef GPOS_DEBUG
							   child_index
#endif
						   ,
						   CDrvdPropArray *,  //pdrgpdpCtxt,
						   ULONG			  //ulOptReq
) const
{
	GPOS_ASSERT(0 == child_index);
	return PcterPushThru(pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDML::FProvidesReqdCols(CExpressionHandle &exprhdl,
								CColRefSet *pcrsRequired,
								ULONG  // ulOptReq
) const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalDML::PdsDerive(CMemoryPool *,	//mp,
						CExpressionHandle &exprhdl) const
{
	return PdsDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalDML::PrsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
{
	return PrsDerivePassThruOuter(mp, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CPhysicalDML::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrAction));
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrSource));

	if (CLogicalDML::EdmlDelete == m_edmlop ||
		CLogicalDML::EdmlUpdate == m_edmlop)
	{
		ulHash = gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrCtid));
		ulHash =
			gpos::CombineHashes(ulHash, gpos::HashPtr<CColRef>(m_pcrSegmentId));
	}

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::Matches
//
//	@doc:
//		Match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDML::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		CPhysicalDML *popDML = CPhysicalDML::PopConvert(pop);

		return m_pcrAction == popDML->PcrAction() &&
			   m_pcrCtid == popDML->PcrCtid() &&
			   m_pcrSegmentId == popDML->PcrSegmentId() &&
			   m_ptabdesc->MDId()->Equals(popDML->Ptabdesc()->MDId()) &&
			   m_pdrgpcrSource->Equals(popDML->PdrgpcrSource()) &&
			   m_fSplit == popDML->FSplit();
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalDML::EpetRewindability(CExpressionHandle &,		// exprhdl,
								const CEnfdRewindability *	// per
) const
{
	return CEnfdProp::EpetProhibited;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::PosComputeRequired
//
//	@doc:
//		Compute required sort order based on the key information in the table
//		descriptor:
//		1. If a table has no keys, no sort order is necessary.
//
//		2. If a table has keys, but they are not modified in the update, no sort
//		order is necessary. This relies on the fact that Split always produces
//		Delete tuples before Insert tuples, so we cannot have two versions of the
//		same tuple on the same time. Consider for example tuple (A: 1, B: 2), where
//		A is key and an update "set B=B+1". Since there cannot be any other tuple
//		with A=1, and the tuple (1,2) is deleted before tuple (1,3) gets inserted,
//		we don't need to enforce specific order of deletes and inserts.
//
//		3. If the update changes a key column, enforce order on the Action column
//		to deliver Delete tuples before Insert tuples. This is done to avoid a
//		conflict between a newly inserted tuple and an old tuple that is about to be
//		deleted. Consider table with tuples (A: 1),(A: 2), where A is key, and
//		update "set A=A+1". Split will generate tuples (1,"D"), (2,"I"), (2,"D"), (3,"I").
//		If (2,"I") happens before (2,"D") we will have a violation of the key constraint.
//		Therefore we need to enforce sort order on Action to get all old tuples
//		tuples deleted before the new ones are inserted.
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalDML::PosComputeRequired(CMemoryPool *mp, CTableDescriptor *ptabdesc)
{
	COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);

	const CBitSetArray *pdrgpbsKeys = ptabdesc->PdrgpbsKeys();
	if (1 < pdrgpbsKeys->Size() && CLogicalDML::EdmlUpdate == m_edmlop)
	{
		// if this is an update on the target table's keys, enforce order on
		// the action column, see explanation in function's comment
		const ULONG ulKeySets = pdrgpbsKeys->Size();
		BOOL fNeedsSort = false;
		for (ULONG ul = 0; ul < ulKeySets; ul++)
		{
			CBitSet *pbs = (*pdrgpbsKeys)[ul];
			if (!pbs->IsDisjoint(m_pbsModified))
			{
				fNeedsSort = true;
				break;
			}
		}

		if (fNeedsSort)
		{
			IMDId *mdid =
				m_pcrAction->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
			mdid->AddRef();
			pos->Append(mdid, m_pcrAction, COrderSpec::EntAuto);
		}
	}

	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::ComputeRequiredLocalColumns
//
//	@doc:
//		Compute a set of columns required by local members
//
//---------------------------------------------------------------------------
void
CPhysicalDML::ComputeRequiredLocalColumns(CMemoryPool *mp)
{
	GPOS_ASSERT(nullptr == m_pcrsRequiredLocal);

	m_pcrsRequiredLocal = GPOS_NEW(mp) CColRefSet(mp);

	// include source columns
	m_pcrsRequiredLocal->Include(m_pdrgpcrSource);
	// Action column is not required for InPlaceUpdate operator.
	if (m_fSplit)
	{
		m_pcrsRequiredLocal->Include(m_pcrAction);
	}

	if (CLogicalDML::EdmlDelete == m_edmlop ||
		CLogicalDML::EdmlUpdate == m_edmlop)
	{
		m_pcrsRequiredLocal->Include(m_pcrCtid);
		m_pcrsRequiredLocal->Include(m_pcrSegmentId);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDML::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalDML::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " (";
	m_ptabdesc->Name().OsPrint(os);
	CLogicalDML::PrintOperatorType(os, m_edmlop, m_fSplit);
	os << "Source Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrSource);
	os << "], Action: (";
	m_pcrAction->OsPrint(os);
	os << ")";

	if (CLogicalDML::EdmlDelete == m_edmlop ||
		CLogicalDML::EdmlUpdate == m_edmlop)
	{
		os << ", ";
		m_pcrCtid->OsPrint(os);
		os << ", ";
		m_pcrSegmentId->OsPrint(os);
	}


	return os;
}

// EOF
