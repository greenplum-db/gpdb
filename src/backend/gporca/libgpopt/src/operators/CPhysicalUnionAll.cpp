#include "gpopt/operators/CPhysicalUnionAll.h"

#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecStrictRandom.h"
#include "gpopt/base/CDrvdPropCtxtPlan.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CHashedDistributions.h"
#include "gpopt/operators/CScalarIdent.h"

using namespace gpopt;

static BOOL Equals(ULongPtrArray *pdrgpulFst, ULongPtrArray *pdrgpulSnd);

#ifdef GPOS_DEBUG

// helper to assert distribution delivered by UnionAll children
static void AssertValidChildDistributions(
	CMemoryPool *mp, CExpressionHandle &exprhdl,
	const CDistributionSpec::EDistributionType
		*pedt,		 // array of distribution types to check
	ULONG ulDistrs,	 // number of distribution types to check
	const CHAR *szAssertMsg);

// helper to check if UnionAll children have valid distributions
static void CheckChildDistributions(CMemoryPool *mp, CExpressionHandle &exprhdl,
									BOOL fSingletonChild, BOOL fReplicatedChild,
									BOOL fUniversalOuterChild);

#endif	// GPOS_DEBUG

// helper to do value equality check of arrays of ULONG pointers
BOOL
Equals(ULongPtrArray *pdrgpulFst, ULongPtrArray *pdrgpulSnd)
{
	GPOS_ASSERT(nullptr != pdrgpulFst);
	GPOS_ASSERT(nullptr != pdrgpulSnd);

	const ULONG ulSizeFst = pdrgpulFst->Size();
	const ULONG ulSizeSnd = pdrgpulSnd->Size();
	if (ulSizeFst != ulSizeSnd)
	{
		// arrays have different lengths
		return false;
	}

	BOOL fEqual = true;
	for (ULONG ul = 0; fEqual && ul < ulSizeFst; ul++)
	{
		ULONG ulFst = *((*pdrgpulFst)[ul]);
		ULONG ulSnd = *((*pdrgpulSnd)[ul]);
		fEqual = (ulFst == ulSnd);
	}

	return fEqual;
}

// sensitivity to order of inputs
BOOL
CPhysicalUnionAll::FInputOrderSensitive() const
{
	return false;
}

CPhysicalUnionAll::CPhysicalUnionAll(CMemoryPool *mp,
									 CColRefArray *pdrgpcrOutput,
									 CColRef2dArray *pdrgpdrgpcrInput)
	: CPhysical(mp),
	  m_pdrgpcrOutput(pdrgpcrOutput),
	  m_pdrgpdrgpcrInput(pdrgpdrgpcrInput),
	  m_pdrgpcrsInput(nullptr),
	  m_pdrgpds(nullptr)
{
	GPOS_ASSERT(nullptr != pdrgpcrOutput);
	GPOS_ASSERT(nullptr != pdrgpdrgpcrInput);

	// build set representation of input columns
	m_pdrgpcrsInput = GPOS_NEW(mp) CColRefSetArray(mp);
	const ULONG arity = m_pdrgpdrgpcrInput->Size();
	for (ULONG ulChild = 0; ulChild < arity; ulChild++)
	{
		CColRefArray *colref_array = (*m_pdrgpdrgpcrInput)[ulChild];
		m_pdrgpcrsInput->Append(GPOS_NEW(mp) CColRefSet(mp, colref_array));
	}
	PopulateDistrSpecs(mp, pdrgpcrOutput, pdrgpdrgpcrInput);
}

void
CPhysicalUnionAll::PopulateDistrSpecs(CMemoryPool *mp,
									  CColRefArray *pdrgpcrOutput,
									  CColRef2dArray *pdrgpdrgpcrInput)
{
	CDistributionSpecArray *pdrgpds = GPOS_NEW(mp) CDistributionSpecArray(mp);
	const ULONG num_cols = pdrgpcrOutput->Size();
	const ULONG arity = pdrgpdrgpcrInput->Size();
	for (ULONG ulChild = 0; ulChild < arity; ulChild++)
	{
		CColRefArray *colref_array = (*pdrgpdrgpcrInput)[ulChild];
		CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
		for (ULONG ulCol = 0; ulCol < num_cols; ulCol++)
		{
			CColRef *colref = (*colref_array)[ulCol];
			CExpression *pexpr = CUtils::PexprScalarIdent(mp, colref);
			pdrgpexpr->Append(pexpr);
		}

		// create a hashed distribution on input columns of the current child
		BOOL fNullsColocated = true;
		CDistributionSpec *pdshashed =
			CDistributionSpecHashed::MakeHashedDistrSpec(
				mp, pdrgpexpr, fNullsColocated, nullptr, nullptr);
		if (nullptr == pdshashed)
		{
			pdrgpexpr->Release();
			pdrgpds->Release();
			return;
		}
		else
		{
			pdrgpds->Append(pdshashed);
		}
	}

	m_pdrgpds = pdrgpds;
}

CPhysicalUnionAll::~CPhysicalUnionAll()
{
	m_pdrgpcrOutput->Release();
	m_pdrgpdrgpcrInput->Release();
	m_pdrgpcrsInput->Release();
	CRefCount::SafeRelease(m_pdrgpds);
}

// accessor of output column array
CColRefArray *
CPhysicalUnionAll::PdrgpcrOutput() const
{
	return m_pdrgpcrOutput;
}

// accessor of input column array
CColRef2dArray *
CPhysicalUnionAll::PdrgpdrgpcrInput() const
{
	return m_pdrgpdrgpcrInput;
}

CPhysicalUnionAll *
CPhysicalUnionAll::PopConvert(COperator *pop)
{
	GPOS_ASSERT(nullptr != pop);

	CPhysicalUnionAll *popPhysicalUnionAll =
		dynamic_cast<CPhysicalUnionAll *>(pop);
	GPOS_ASSERT(nullptr != popPhysicalUnionAll);

	return popPhysicalUnionAll;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalUnionAll::Matches(COperator *pop) const
{
	if (Eopid() == pop->Eopid())
	{
		CPhysicalUnionAll *popUnionAll = CPhysicalUnionAll::PopConvert(pop);

		return PdrgpcrOutput()->Equals(popUnionAll->PdrgpcrOutput());
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalUnionAll::PcrsRequired(CMemoryPool *mp,
								CExpressionHandle &,  //exprhdl,
								CColRefSet *pcrsRequired, ULONG child_index,
								CDrvdPropArray *,  // pdrgpdpCtxt
								ULONG			   // ulOptReq
)
{
	return MapOutputColRefsToInput(mp, pcrsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalUnionAll::PosRequired(CMemoryPool *mp,
							   CExpressionHandle &,	 //exprhdl,
							   COrderSpec *,		 //posRequired,
							   ULONG
#ifdef GPOS_DEBUG
								   child_index
#endif	// GPOS_DEBUG
							   ,
							   CDrvdPropArray *,  // pdrgpdpCtxt
							   ULONG			  // ulOptReq
) const
{
	GPOS_ASSERT(PdrgpdrgpcrInput()->Size() > child_index);

	// no order required from child expression
	return GPOS_NEW(mp) COrderSpec(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalUnionAll::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							   CRewindabilitySpec *prsRequired,
							   ULONG child_index,
							   CDrvdPropArray *,  // pdrgpdpCtxt
							   ULONG			  // ulOptReq
) const
{
	GPOS_ASSERT(PdrgpdrgpcrInput()->Size() > child_index);

	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalUnionAll::PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								CCTEReq *pcter, ULONG child_index,
								CDrvdPropArray *pdrgpdpCtxt,
								ULONG  //ulOptReq
) const
{
	return PcterNAry(mp, exprhdl, pcter, child_index, pdrgpdpCtxt);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalUnionAll::FProvidesReqdCols(CExpressionHandle &
#ifdef GPOS_DEBUG
										 exprhdl
#endif	// GPOS_DEBUG
									 ,
									 CColRefSet *pcrsRequired,
									 ULONG	// ulOptReq
) const
{
	GPOS_ASSERT(nullptr != pcrsRequired);
	GPOS_ASSERT(PdrgpdrgpcrInput()->Size() == exprhdl.Arity());

	CColRefSet *pcrs = GPOS_NEW(m_mp) CColRefSet(m_mp);

	// include output columns
	pcrs->Include(PdrgpcrOutput());
	BOOL fProvidesCols = pcrs->ContainsAll(pcrsRequired);
	pcrs->Release();

	return fProvidesCols;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalUnionAll::PosDerive(CMemoryPool *mp,
							 CExpressionHandle &  //exprhdl
) const
{
	// return empty sort order
	return GPOS_NEW(mp) COrderSpec(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalUnionAll::PrsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
{
	// TODO: shardikar; This should check all the children, not only the outer child.
	return PrsDerivePassThruOuter(mp, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalUnionAll::EpetOrder(CExpressionHandle &,  // exprhdl
							 const CEnfdOrder *
#ifdef GPOS_DEBUG
								 peo
#endif	// GPOS_DEBUG
) const
{
	GPOS_ASSERT(nullptr != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	return CEnfdProp::EpetRequired;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalUnionAll::EpetRewindability(CExpressionHandle &exprhdl,
									 const CEnfdRewindability *per) const
{
	GPOS_ASSERT(nullptr != per);

	// get rewindability delivered by the node
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		// required rewindability is already provided
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}

BOOL
CPhysicalUnionAll::FPassThruStats() const
{
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalUnionAll::PdsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const
{
	CDistributionSpecHashed *pdshashed = PdshashedDerive(mp, exprhdl);
	if (nullptr != pdshashed)
	{
		return pdshashed;
	}

	CDistributionSpec *pds = PdsDeriveFromChildren(mp, exprhdl);
	if (nullptr != pds)
	{
		// succeeded in deriving output distribution from child distributions
		pds->AddRef();
		return pds;
	}

	// derive strict random spec, if parallel union all enforces strict random
	CDistributionSpecRandom *random_dist_spec =
		PdsStrictRandomParallelUnionAllChildren(mp, exprhdl);
	if (nullptr != random_dist_spec)
	{
		return random_dist_spec;
	}

	// output has unknown distribution on all segments
	return GPOS_NEW(mp) CDistributionSpecRandom();
}

// Consider the below query:
// insert into t1_x select a from t2_x union all select a from t2_x;
// where t1_x and t2_x relations are randomly distributed
// the physical plan is as below:
// +--CPhysicalDML (Insert, "t1_x"), Source Columns: ["a" (0)], Action: ("ColRef_0016" (16))
//    +--CPhysicalComputeScalar
//    |--CPhysicalParallelUnionAll
//    |  |--CPhysicalMotionRandom ==> Derives CDistributionSpecStrictRandom
//    |  |  +--CPhysicalTableScan
//    |  +--CPhysicalMotionRandom ==> Derives CDistributionSpecStrictRandom
//    |     +--CPhysicalTableScan "t1_x" ("t1_x")
//    +--CScalarProjectList
//       +--CScalarProjectElement "ColRef_0016" (16)
//          +--CScalarConst (1)
//
// in the above plan, the child of CPhysicalParallelUnionAll
// enforces CDistributionSpecStrictRandom with CPhysicalMotionRandom
// operator. Since, the data coming to CPhysicalParallelUnionAll
// is already randomly distributed due to existence of motion,
// there is no need to redistribute the data again before
// inserting into t1_x. So, derive CDistributionSpecStrictRandom
CDistributionSpecRandom *
CPhysicalUnionAll::PdsStrictRandomParallelUnionAllChildren(
	CMemoryPool *mp, CExpressionHandle &expr_handle)
{
	if (COperator::EopPhysicalParallelUnionAll == expr_handle.Pop()->Eopid())
	{
		BOOL has_strict_random_spec = true;
		BOOL has_motion_random = true;
		for (ULONG idx = 0; has_motion_random && has_strict_random_spec &&
							idx < expr_handle.Arity();
			 idx++)
		{
			CDistributionSpec *child_dist_spec =
				expr_handle.Pdpplan(idx /*child_index*/)->Pds();
			CDistributionSpec::EDistributionType dist_spec_type =
				child_dist_spec->Edt();
			has_motion_random = COperator::EopPhysicalMotionRandom ==
								expr_handle.Pop(idx /*child_index*/)->Eopid();
			has_strict_random_spec =
				CDistributionSpec::EdtStrictRandom == dist_spec_type;
		}
		if (has_motion_random && has_strict_random_spec)
		{
			return GPOS_NEW(mp) CDistributionSpecStrictRandom();
		}
	}
	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdshashedDerive
//
//	@doc:
//		Derive hashed distribution from child hashed distributions
//
//---------------------------------------------------------------------------
CDistributionSpecHashed *
CPhysicalUnionAll::PdshashedDerive(CMemoryPool *mp,
								   CExpressionHandle &exprhdl) const
{
	if (m_pdrgpds == nullptr)
	{
		return nullptr;
	}

	BOOL fSuccess = true;
	const ULONG arity = exprhdl.Arity();

	// (1) check that all children deliver a hashed distribution that satisfies their input columns
	for (ULONG ulChild = 0; fSuccess && ulChild < arity; ulChild++)
	{
		CDistributionSpec *pdsChild = exprhdl.Pdpplan(ulChild)->Pds();
		CDistributionSpec::EDistributionType edtChild = pdsChild->Edt();
		fSuccess = (CDistributionSpec::EdtHashed == edtChild ||
					CDistributionSpec::EdtHashedNoOp == edtChild ||
					CDistributionSpec::EdtStrictHashed == edtChild) &&
				   pdsChild->FSatisfies((*m_pdrgpds)[ulChild]);
	}
	if (!fSuccess)
	{
		// a child does not deliver hashed distribution
		return nullptr;
	}

	// (2) check that child hashed distributions map to the same output columns

	// map outer child hashed distribution to corresponding UnionAll column positions.
	// make sure to look at the equivalent distribution specs
	ULongPtrArray *pdrgpulOuter = nullptr;
	CDistributionSpec *pdsChild = exprhdl.Pdpplan(0)->Pds();
	CDistributionSpecHashed *pdsHashedFirstChild =
		CDistributionSpecHashed::PdsConvert(pdsChild);
	CDistributionSpecHashed *pdsHashed = pdsHashedFirstChild;
	while (pdsHashed && nullptr == pdrgpulOuter)
	{
		pdrgpulOuter = PdrgpulMap(
			mp, CDistributionSpecHashed::PdsConvert(pdsHashed)->Pdrgpexpr(),
			0 /*child_index*/);
		pdsHashed = pdsHashed->PdshashedEquiv();
	}
	if (nullptr == pdrgpulOuter)
	{
		return nullptr;
	}

	ULongPtrArray *pdrgpulChild = nullptr;
	for (ULONG ulChild = 1; fSuccess && ulChild < arity; ulChild++)
	{
		CDistributionSpecHashed *pdsChildSpec =
			CDistributionSpecHashed::PdsConvert(
				exprhdl.Pdpplan(ulChild)->Pds());
		GPOS_ASSERT(nullptr != pdsChildSpec);
		CDistributionSpecHashed *pdsChildHashed = pdsChildSpec;
		BOOL equi_hash_spec_matches = false;
		while (pdsChildHashed && !equi_hash_spec_matches)
		{
			pdrgpulChild =
				PdrgpulMap(mp,
						   CDistributionSpecHashed::PdsConvert(pdsChildHashed)
							   ->Pdrgpexpr(),
						   ulChild);
			// match mapped column positions of current child with outer child
			equi_hash_spec_matches =
				(nullptr != pdrgpulChild) && Equals(pdrgpulOuter, pdrgpulChild);
			CRefCount::SafeRelease(pdrgpulChild);
			pdsChildHashed = pdsChildHashed->PdshashedEquiv();
		}
		fSuccess = equi_hash_spec_matches;
	}

	CDistributionSpecHashed *pdsOutput = nullptr;
	if (fSuccess)
	{
		pdsOutput = PdsMatching(mp, pdrgpulOuter);
	}

	pdrgpulOuter->Release();

	return pdsOutput;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdsMatching
//
//	@doc:
//		Compute output hashed distribution based on the outer child's
//		hashed distribution
//---------------------------------------------------------------------------
CDistributionSpecHashed *
CPhysicalUnionAll::PdsMatching(CMemoryPool *mp,
							   const ULongPtrArray *pdrgpulOuter) const
{
	GPOS_ASSERT(nullptr != pdrgpulOuter);

	const ULONG num_cols = pdrgpulOuter->Size();

	GPOS_ASSERT(num_cols <= PdrgpcrOutput()->Size());

	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	for (ULONG ulCol = 0; ulCol < num_cols; ulCol++)
	{
		ULONG idx = *(*pdrgpulOuter)[ulCol];
		CExpression *pexpr =
			CUtils::PexprScalarIdent(mp, (*PdrgpcrOutput())[idx]);
		pdrgpexpr->Append(pexpr);
	}

	GPOS_ASSERT(0 < pdrgpexpr->Size());

	return GPOS_NEW(mp)
		CDistributionSpecHashed(pdrgpexpr, true /*fNullsColocated*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdshashedPassThru
//
//	@doc:
//		Compute required hashed distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpecHashed *
CPhysicalUnionAll::PdshashedPassThru(CMemoryPool *mp,
									 CDistributionSpecHashed *pdshashedRequired,
									 ULONG child_index) const
{
	CExpressionArray *pdrgpexprRequired = pdshashedRequired->Pdrgpexpr();
	CColRefArray *pdrgpcrChild = (*PdrgpdrgpcrInput())[child_index];
	const ULONG ulExprs = pdrgpexprRequired->Size();
	const ULONG ulOutputCols = PdrgpcrOutput()->Size();

	CExpressionArray *pdrgpexprChildRequired =
		GPOS_NEW(mp) CExpressionArray(mp);
	for (ULONG ulExpr = 0; ulExpr < ulExprs; ulExpr++)
	{
		CExpression *pexpr = (*pdrgpexprRequired)[ulExpr];
		if (COperator::EopScalarIdent != pexpr->Pop()->Eopid())
		{
			// skip expressions that are not in form of scalar identifiers
			continue;
		}
		const CColRef *pcrHashed =
			CScalarIdent::PopConvert(pexpr->Pop())->Pcr();
		const IMDType *pmdtype = pcrHashed->RetrieveType();
		if (!pmdtype->IsHashable())
		{
			// skip non-hashable columns
			continue;
		}

		for (ULONG ulCol = 0; ulCol < ulOutputCols; ulCol++)
		{
			const CColRef *pcrOutput = (*PdrgpcrOutput())[ulCol];
			if (pcrOutput == pcrHashed)
			{
				const CColRef *pcrInput = (*pdrgpcrChild)[ulCol];
				pdrgpexprChildRequired->Append(
					CUtils::PexprScalarIdent(mp, pcrInput));
			}
		}
	}

	if (0 < pdrgpexprChildRequired->Size())
	{
		return GPOS_NEW(mp) CDistributionSpecHashed(
			pdrgpexprChildRequired, true /* fNullsCollocated */);
	}

	// failed to create a matching hashed distribution
	pdrgpexprChildRequired->Release();

	if (nullptr != pdshashedRequired->PdshashedEquiv())
	{
		// try again with equivalent distribution
		return PdshashedPassThru(mp, pdshashedRequired->PdshashedEquiv(),
								 child_index);
	}

	// failed to create hashed distribution
	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdsDeriveFromChildren
//
//	@doc:
//		Derive output distribution based on child distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalUnionAll::PdsDeriveFromChildren(CMemoryPool *
#ifdef GPOS_DEBUG
											 mp
#endif	// GPOS_DEBUG
										 ,
										 CExpressionHandle &exprhdl)
{
	const ULONG arity = exprhdl.Arity();

	CDistributionSpec *pdsOuter = exprhdl.Pdpplan(0 /*child_index*/)->Pds();
	CDistributionSpec *pds = pdsOuter;
	BOOL fUniversalOuterChild =
		(CDistributionSpec::EdtUniversal == pdsOuter->Edt());
	BOOL fSingletonChild = false;
	BOOL fReplicatedChild = false;
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDistributionSpec *pdsChild =
			exprhdl.Pdpplan(ul /*child_index*/)->Pds();
		CDistributionSpec::EDistributionType edtChild = pdsChild->Edt();

		if (CDistributionSpec::EdtSingleton == edtChild ||
			CDistributionSpec::EdtStrictSingleton == edtChild)
		{
			fSingletonChild = true;
			pds = pdsChild;
			break;
		}

		if (CDistributionSpec::EdtStrictReplicated == edtChild ||
			CDistributionSpec::EdtTaintedReplicated == edtChild)
		{
			fReplicatedChild = true;
			pds = pdsChild;
			break;
		}
	}

#ifdef GPOS_DEBUG
	CheckChildDistributions(mp, exprhdl, fSingletonChild, fReplicatedChild,
							fUniversalOuterChild);
#endif	// GPOS_DEBUG

	if (!(fSingletonChild || fReplicatedChild || fUniversalOuterChild))
	{
		// failed to derive distribution from children
		pds = nullptr;
	}

	// even if a single child is tainted, the result should be tainted
	if (fReplicatedChild)
	{
		for (ULONG ul = 0; ul < arity; ul++)
		{
			CDistributionSpec *pdsChild =
				exprhdl.Pdpplan(ul /*child_index*/)->Pds();
			CDistributionSpec::EDistributionType edtChild = pdsChild->Edt();

			if (CDistributionSpec::EdtTaintedReplicated == edtChild)
			{
				pds = pdsChild;
				break;
			}
		}
	}

	return pds;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalUnionAll::PdrgpulMap
//
//	@doc:
//		Map given array of scalar identifier expressions to positions of
//		UnionAll input columns in the given child;
//		the function returns NULL if no mapping could be constructed
//
//---------------------------------------------------------------------------
ULongPtrArray *
CPhysicalUnionAll::PdrgpulMap(CMemoryPool *mp, CExpressionArray *pdrgpexpr,
							  ULONG child_index) const
{
	GPOS_ASSERT(nullptr != pdrgpexpr);

	CColRefArray *colref_array = (*PdrgpdrgpcrInput())[child_index];
	const ULONG ulExprs = pdrgpexpr->Size();
	const ULONG num_cols = colref_array->Size();
	ULongPtrArray *pdrgpul = GPOS_NEW(mp) ULongPtrArray(mp);
	for (ULONG ulExpr = 0; ulExpr < ulExprs; ulExpr++)
	{
		CExpression *pexpr = (*pdrgpexpr)[ulExpr];
		if (COperator::EopScalarIdent != pexpr->Pop()->Eopid())
		{
			continue;
		}
		const CColRef *colref = CScalarIdent::PopConvert(pexpr->Pop())->Pcr();
		for (ULONG ulCol = 0; ulCol < num_cols; ulCol++)
		{
			if ((*colref_array)[ulCol] == colref)
			{
				pdrgpul->Append(GPOS_NEW(mp) ULONG(ulCol));
			}
		}
	}

	if (0 == pdrgpul->Size())
	{
		// mapping failed
		pdrgpul->Release();
		pdrgpul = nullptr;
	}

	return pdrgpul;
}

CColRefSet *
CPhysicalUnionAll::MapOutputColRefsToInput(CMemoryPool *mp,
										   CColRefSet *out_col_refs,
										   ULONG child_index)
{
	CColRefSet *result = GPOS_NEW(mp) CColRefSet(mp);
	CColRefArray *all_outcols = m_pdrgpcrOutput;
	ULONG total_num_cols = all_outcols->Size();
	CColRefArray *in_colref_array = (*PdrgpdrgpcrInput())[child_index];
	CColRefSetIter iter(*out_col_refs);
	while (iter.Advance())
	{
		BOOL found = false;
		// find the index in the complete list of output columns
		for (ULONG i = 0; i < total_num_cols && !found; i++)
		{
			if (iter.Bit() == (*all_outcols)[i]->Id())
			{
				// the input colref will have the same index, but in the list of input cols
				result->Include((*in_colref_array)[i]);
				found = true;
			}
		}
		GPOS_ASSERT(found);
	}
	return result;
}


#ifdef GPOS_DEBUG

void
AssertValidChildDistributions(
	CMemoryPool *mp, CExpressionHandle &exprhdl,
	const CDistributionSpec::EDistributionType
		*pedt,		 // array of distribution types to check
	ULONG ulDistrs,	 // number of distribution types to check
	const CHAR *szAssertMsg)
{
	const ULONG arity = exprhdl.Arity();
	for (ULONG ulChild = 0; ulChild < arity; ulChild++)
	{
		CDistributionSpec *pdsChild = exprhdl.Pdpplan(ulChild)->Pds();
		CDistributionSpec::EDistributionType edtChild = pdsChild->Edt();
		BOOL fMatch = false;
		for (ULONG ulDistr = 0; !fMatch && ulDistr < ulDistrs; ulDistr++)
		{
			fMatch = (pedt[ulDistr] == edtChild);
		}

		if (!fMatch)
		{
			CAutoTrace at(mp);
			at.Os() << szAssertMsg;
		}
		GPOS_ASSERT(fMatch);
	}
}

void
CheckChildDistributions(CMemoryPool *mp, CExpressionHandle &exprhdl,
						BOOL fSingletonChild, BOOL fReplicatedChild,
						BOOL fUniversalOuterChild)
{
	CDistributionSpec::EDistributionType rgedt[5];
	rgedt[0] = CDistributionSpec::EdtSingleton;
	rgedt[1] = CDistributionSpec::EdtStrictSingleton;
	rgedt[2] = CDistributionSpec::EdtUniversal;
	rgedt[3] = CDistributionSpec::EdtStrictReplicated;
	rgedt[4] = CDistributionSpec::EdtTaintedReplicated;

	if (fReplicatedChild)
	{
		// assert all children have distribution Universal or Replicated
		AssertValidChildDistributions(
			mp, exprhdl, rgedt + 2 /*start from Universal in rgedt*/,
			3 /*ulDistrs*/,
			"expecting Replicated or Universal distribution in UnionAll children" /*szAssertMsg*/);
	}
	else if (fSingletonChild || fUniversalOuterChild)
	{
		// assert all children have distribution Singleton, StrictSingleton or Universal
		AssertValidChildDistributions(
			mp, exprhdl, rgedt, 3 /*ulDistrs*/,
			"expecting Singleton or Universal distribution in UnionAll children" /*szAssertMsg*/);
	}
}

#endif	// GPOS_DEBUG
