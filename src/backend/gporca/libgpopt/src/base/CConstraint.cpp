//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraint.cpp
//
//	@doc:
//		Implementation of constraints
//---------------------------------------------------------------------------

#include "gpopt/base/CConstraint.h"

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"

#include "gpopt/base/CCastUtils.h"
#include "gpopt/base/CColConstraintsArrayMapper.h"
#include "gpopt/base/CColConstraintsHashMapper.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintDisjunction.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CConstraintNegation.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/IColConstraintsMapper.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarArrayCmp.h"
#include "gpopt/operators/CScalarCmp.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDType.h"

using namespace gpopt;

FORCE_GENERATE_DBGSTR(CConstraint);

// initialize constant true
BOOL CConstraint::m_fTrue(true);

// initialize constant false
BOOL CConstraint::m_fFalse(false);

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::CConstraint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CConstraint::CConstraint(CMemoryPool *mp, CColRefSet *pcrsUsed)
	: m_phmcontain(nullptr),
	  m_mp(mp),
	  m_pcrsUsed(pcrsUsed),
	  m_pexprScalar(nullptr)
{
	m_phmcontain = GPOS_NEW(m_mp) ConstraintContainmentMap(m_mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::~CConstraint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CConstraint::~CConstraint()
{
	CRefCount::SafeRelease(m_pexprScalar);
	m_pcrsUsed->Release();
	m_phmcontain->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarArrayCmp
//
//	@doc:
//		Create constraint from scalar array comparison expression
//		originally generated for "scalar op ANY/ALL (array)" construct
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarArrayCmp(CMemoryPool *mp, CExpression *pexpr,
									  CColRef *colref, BOOL infer_nulls_as)
{
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(CUtils::FScalarArrayCmp(pexpr));

	CScalarArrayCmp *popScArrayCmp = CScalarArrayCmp::PopConvert(pexpr->Pop());
	CScalarArrayCmp::EArrCmpType earrccmpt = popScArrayCmp->Earrcmpt();

	if ((CScalarArrayCmp::EarrcmpAny == earrccmpt ||
		 CScalarArrayCmp::EarrcmpAll == earrccmpt) &&
		(CPredicateUtils::FCompareIdentToConstArray(pexpr) ||
		 CPredicateUtils::FCompareCastIdentToConstArray(pexpr)))
	{
#ifdef GPOS_DEBUG
		// verify column in expr is the same as column which was passed in
		CScalarIdent *popScId = nullptr;
		if (CUtils::FScalarIdent((*pexpr)[0]))
		{
			popScId = CScalarIdent::PopConvert((*pexpr)[0]->Pop());
		}
		else
		{
			GPOS_ASSERT(CScalarIdent::FCastedScId((*pexpr)[0]));
			popScId = CScalarIdent::PopConvert((*(*pexpr)[0])[0]->Pop());
		}
		GPOS_ASSERT(colref == (CColRef *) popScId->Pcr());
#endif	// GPOS_DEBUG

		// get comparison type
		IMDType::ECmpType cmp_type =
			CUtils::ParseCmpType(popScArrayCmp->MdIdOp());

		if (IMDType::EcmptOther == cmp_type)
		{
			// unsupported comparison operator for constraint derivation

			return nullptr;
		}
		CExpression *pexprArray = CUtils::PexprScalarArrayChild(pexpr);

		const ULONG arity = CUtils::UlScalarArrayArity(pexprArray);

		// When array size exceeds the constraint derivation threshold,
		// don't expand it into a DNF and don't derive constraints
		COptimizerConfig *optimizer_config =
			COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
		ULONG array_expansion_threshold =
			optimizer_config->GetHint()->UlArrayExpansionThreshold();

		if (arity > array_expansion_threshold)
		{
			return nullptr;
		}

		if (arity == 0)
		{
			if (earrccmpt == CScalarArrayCmp::EarrcmpAny)
			{
				CRangeArray *emptyRangeArray = GPOS_NEW(mp) CRangeArray(mp);
				// comparing with an empty array for any ANY comparison produces a "false" constraint
				// which is represented by an empty CConstraintInterval
				return GPOS_NEW(mp) CConstraintInterval(
					mp, colref, emptyRangeArray, false /*includes NULL*/);
			}
			else
			{
				// for an all comparison with an empty array, don't do further processing as we won't
				// do simplification anyway
				return nullptr;
			}
		}

		CConstraintArray *pdrgpcnstr = GPOS_NEW(mp) CConstraintArray(mp);

		for (ULONG ul = 0; ul < arity; ul++)
		{
			CScalarConst *popScConst =
				CUtils::PScalarArrayConstChildAt(pexprArray, ul);
			CConstraintInterval *pci =
				CConstraintInterval::PciIntervalFromColConstCmp(
					mp, colref, cmp_type, popScConst, infer_nulls_as);
			pdrgpcnstr->Append(pci);
		}

		if (earrccmpt == CScalarArrayCmp::EarrcmpAny)
		{
			// predicate is of the form 'A IN (1,2,3)'
			// return a disjunction of ranges {[1,1], [2,2], [3,3]}
			return GPOS_NEW(mp) CConstraintDisjunction(mp, pdrgpcnstr);
		}

		// predicate is of the form 'A NOT IN (1,2,3)'
		// return a conjunctive negation on {[1,1], [2,2], [3,3]}
		return GPOS_NEW(mp) CConstraintConjunction(mp, pdrgpcnstr);
	}

	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarExpr
//
//	@doc:
//		Create constraint from scalar expression and pass back any discovered
//		equivalence classes
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarExpr(
	CMemoryPool *mp, CExpression *pexpr,
	CColRefSetArray **ppdrgpcrs,  // output equivalence classes
	BOOL infer_nulls_as)
{
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(pexpr->Pop()->FScalar());
	GPOS_ASSERT(nullptr != ppdrgpcrs);
	GPOS_ASSERT(nullptr == *ppdrgpcrs);

	CColRefSet *pcrs = pexpr->DeriveUsedColumns();
	ULONG num_cols = pcrs->Size();

	if (0 == num_cols)
	{
		// TODO:  - May 29, 2012: in case of an expr with no columns (e.g. 1 < 2),
		// possibly evaluate the expression, and return a "TRUE" or "FALSE" constraint
		return nullptr;
	}

	if (1 == num_cols)
	{
		CColRef *colref = pcrs->PcrFirst();
		if (!CUtils::FConstrainableType(colref->RetrieveType()->MDId()))
		{
			return nullptr;
		}

		CConstraint *pcnstr = nullptr;

		// first, try creating a single interval constraint from the expression
		pcnstr = CConstraintInterval::PciIntervalFromScalarExpr(
			mp, pexpr, colref, infer_nulls_as);
		if (nullptr == pcnstr)
		{
			// if the interval creation failed, try creating a disjunction or conjunction
			// of several interval constraints in the array case
			if (CUtils::FScalarArrayCmp(pexpr))
			{
				pcnstr =
					PcnstrFromScalarArrayCmp(mp, pexpr, colref, infer_nulls_as);
			}
			else
			{
				return PcnstrFromExistsAnySubquery(mp, pexpr, ppdrgpcrs);
			}
		}

		if (nullptr != pcnstr)
		{
			*ppdrgpcrs = GPOS_NEW(mp) CColRefSetArray(mp);
			AddColumnToEquivClasses(mp, colref, *ppdrgpcrs);
		}
		return pcnstr;
	}

	switch (pexpr->Pop()->Eopid())
	{
		case COperator::EopScalarBoolOp:
			return PcnstrFromScalarBoolOp(mp, pexpr, ppdrgpcrs, infer_nulls_as);

		case COperator::EopScalarCmp:
			return PcnstrFromScalarCmp(mp, pexpr, ppdrgpcrs, infer_nulls_as);

		case COperator::EopScalarNAryJoinPredList:
			// return the constraints of the inner join predicates
			return PcnstrFromScalarExpr(mp, (*pexpr)[0], ppdrgpcrs,
										infer_nulls_as);

		case COperator::EopScalarSubqueryAny:
		case COperator::EopScalarSubqueryExists:
			return PcnstrFromExistsAnySubquery(mp, pexpr, ppdrgpcrs);

		default:
			return nullptr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjunction
//
//	@doc:
//		Create conjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjunction(CMemoryPool *mp, CConstraintArray *pdrgpcnstr)
{
	return PcnstrConjDisj(mp, pdrgpcnstr, true /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrDisjunction
//
//	@doc:
//		Create disjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrDisjunction(CMemoryPool *mp, CConstraintArray *pdrgpcnstr)
{
	return PcnstrConjDisj(mp, pdrgpcnstr, false /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjDisj
//
//	@doc:
//		Create conjunction/disjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjDisj(CMemoryPool *mp, CConstraintArray *pdrgpcnstr,
							BOOL fConj)
{
	GPOS_ASSERT(nullptr != pdrgpcnstr);

	CConstraint *pcnstr = nullptr;

	const ULONG length = pdrgpcnstr->Size();

	switch (length)
	{
		case 0:
		{
			pdrgpcnstr->Release();
			break;
		}

		case 1:
		{
			pcnstr = (*pdrgpcnstr)[0];
			pcnstr->AddRef();
			pdrgpcnstr->Release();
			break;
		}

		default:
		{
			if (fConj)
			{
				pcnstr = GPOS_NEW(mp) CConstraintConjunction(mp, pdrgpcnstr);
			}
			else
			{
				pcnstr = GPOS_NEW(mp) CConstraintDisjunction(mp, pdrgpcnstr);
			}
		}
	}

	return pcnstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::AddColumnToEquivClasses
//
//	@doc:
//		Add column as a new equivalence class, if it is not already in one of the
//		existing equivalence classes
//
//---------------------------------------------------------------------------
void
CConstraint::AddColumnToEquivClasses(CMemoryPool *mp, const CColRef *colref,
									 CColRefSetArray *pdrgpcrs)
{
	const ULONG length = pdrgpcrs->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRefSet *pcrs = (*pdrgpcrs)[ul];
		if (pcrs->FMember(colref))
		{
			return;
		}
	}

	CColRefSet *pcrsNew = GPOS_NEW(mp) CColRefSet(mp);
	pcrsNew->Include(colref);

	pdrgpcrs->Append(pcrsNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarCmp
//
//	@doc:
//		Create constraint from scalar comparison
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarCmp(
	CMemoryPool *mp, CExpression *pexpr,
	CColRefSetArray **ppdrgpcrs,  // output equivalence classes
	BOOL infer_nulls_as)
{
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(CUtils::FScalarCmp(pexpr));
	GPOS_ASSERT(nullptr != ppdrgpcrs);
	GPOS_ASSERT(nullptr == *ppdrgpcrs);

	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprRight = (*pexpr)[1];

	// check if the scalar comparison is over scalar idents or binary coercible casted scalar idents
	if ((CUtils::FScalarIdent(pexprLeft) ||
		 CCastUtils::FBinaryCoercibleCastedScId(pexprLeft)) &&
		(CUtils::FScalarIdent(pexprRight) ||
		 CCastUtils::FBinaryCoercibleCastedScId(pexprRight)))
	{
		CScalarIdent *popScIdLeft, *popScIdRight;
		if (CUtils::FScalarIdent(pexprLeft))
		{
			// col1 = ...
			popScIdLeft = CScalarIdent::PopConvert(pexprLeft->Pop());
		}
		else
		{
			// cast(col1) = ...
			GPOS_ASSERT(CCastUtils::FBinaryCoercibleCastedScId(pexprLeft));
			popScIdLeft = CScalarIdent::PopConvert((*pexprLeft)[0]->Pop());
		}

		if (CUtils::FScalarIdent(pexprRight))
		{
			// ... = col2
			popScIdRight = CScalarIdent::PopConvert(pexprRight->Pop());
		}
		else
		{
			// ... = cost(col2)
			GPOS_ASSERT(CCastUtils::FBinaryCoercibleCastedScId(pexprRight));
			popScIdRight = CScalarIdent::PopConvert((*pexprRight)[0]->Pop());
		}

		const CColRef *pcrLeft = popScIdLeft->Pcr();
		const CColRef *pcrRight = popScIdRight->Pcr();

		IMDId *left_mdid = pcrLeft->RetrieveType()->MDId();
		IMDId *right_mdid = pcrRight->RetrieveType()->MDId();
		if (!CUtils::FConstrainableType(left_mdid) ||
			!CUtils::FConstrainableType(right_mdid))
		{
			return nullptr;
		}

		if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution))
		{
			CMDAccessor *mda = COptCtxt::PoctxtFromTLS()->Pmda();
			CScalarCmp *sc_cmp = CScalarCmp::PopConvert(pexpr->Pop());
			const IMDScalarOp *op = mda->RetrieveScOp(sc_cmp->MdIdOp());

			// The left type and right type are both scalar ident types
			// In case of casting, such as
			// --CScalarCast
			//   +--CScalarIdent "a" (0)
			// We look at the data type before casting, instead of after
			// This is because equivalent classes are built with columns
			// based on their input types

			// Eg. foo.a -- varchar, bar.b -- char
			// Joining foo and bar on foo.a = bar.b requires casting
			// foo.a to bpchar. But since hash of varchar (before cast)
			// and hash of char don't belong to the same opfamily, we
			// cannot generate equivalent classes with foo.a and bar.b

			const IMDType *left_type = mda->RetrieveType(left_mdid);

			const IMDType *right_type = mda->RetrieveType(right_mdid);

			// Build constraint (e.g for equivalent classes) only when the hash families
			// of the operator and operands match.
			if (!CUtils::Equals(op->HashOpfamilyMdid(),
								left_type->GetDistrOpfamilyMdid()) ||
				!CUtils::Equals(op->HashOpfamilyMdid(),
								right_type->GetDistrOpfamilyMdid()))
			{
				return nullptr;
			}
		}

		BOOL pcrLeftIncludesNull =
			infer_nulls_as && CColRef::EcrtTable == pcrLeft->Ecrt() &&
			CColRefTable::PcrConvert(const_cast<CColRef *>(pcrLeft))
				->IsNullable();
		BOOL pcrRightIncludesNull =
			infer_nulls_as && CColRef::EcrtTable == pcrRight->Ecrt() &&
			CColRefTable::PcrConvert(const_cast<CColRef *>(pcrRight))
				->IsNullable();

		*ppdrgpcrs = GPOS_NEW(mp) CColRefSetArray(mp);
		BOOL checkEquality = CPredicateUtils::IsEqualityOp(pexpr) &&
							 !pcrLeftIncludesNull && !pcrRightIncludesNull;
		if (checkEquality)
		{
			// col1 = col2 or bcast(col1) = col2 or col1 = bcast(col2) or bcast(col1) = bcast(col2)
			CColRefSet *pcrsNew = GPOS_NEW(mp) CColRefSet(mp);
			pcrsNew->Include(pcrLeft);
			pcrsNew->Include(pcrRight);

			(*ppdrgpcrs)->Append(pcrsNew);
		}

		CConstraintArray *pdrgpcnstr = GPOS_NEW(mp) CConstraintArray(mp);
		pdrgpcnstr->Append(CConstraintInterval::PciUnbounded(
			mp, pcrLeft, pcrLeftIncludesNull /*fIncludesNull*/));
		pdrgpcnstr->Append(CConstraintInterval::PciUnbounded(
			mp, pcrRight, pcrRightIncludesNull /*fIncludesNull*/));
		return CConstraint::PcnstrConjunction(mp, pdrgpcnstr);
	}

	// TODO: , May 28, 2012; add support for other cases besides (col cmp col)

	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarBoolOp
//
//	@doc:
//		Create constraint from scalar boolean expression
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarBoolOp(
	CMemoryPool *mp, CExpression *pexpr,
	CColRefSetArray **ppdrgpcrs,  // output equivalence classes
	BOOL infer_nulls_as)
{
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(CUtils::FScalarBoolOp(pexpr));
	GPOS_ASSERT(nullptr != ppdrgpcrs);
	GPOS_ASSERT(nullptr == *ppdrgpcrs);

	const ULONG arity = pexpr->Arity();

	// Large IN/NOT IN lists that can not be converted into
	// CScalarArrayCmp, are expanded into its disjunctive normal form,
	// represented by a large boolean expression tree.
	// For instance constructs of the form:
	// "(expression1, expression2) scalar op ANY/ALL ((const-x1,const-y1), ... (const-xn,const-yn))"
	// Deriving constraints from this is quite expensive; hence don't
	// bother when the arity of OR exceeds the threshold
	COptimizerConfig *optimizer_config =
		COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
	ULONG array_expansion_threshold =
		optimizer_config->GetHint()->UlArrayExpansionThreshold();

	if (CPredicateUtils::FOr(pexpr) && arity > array_expansion_threshold)
	{
		return nullptr;
	}

	*ppdrgpcrs = GPOS_NEW(mp) CColRefSetArray(mp);
	CConstraintArray *pdrgpcnstr = GPOS_NEW(mp) CConstraintArray(mp);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CColRefSetArray *pdrgpcrsChild = nullptr;
		CConstraint *pcnstrChild = PcnstrFromScalarExpr(
			mp, (*pexpr)[ul], &pdrgpcrsChild, infer_nulls_as);
		if (nullptr == pcnstrChild || pcnstrChild->IsConstraintUnbounded())
		{
			CRefCount::SafeRelease(pcnstrChild);
			CRefCount::SafeRelease(pdrgpcrsChild);
			if (CPredicateUtils::FOr(pexpr))
			{
				pdrgpcnstr->Release();
				return nullptr;
			}
			continue;
		}
		GPOS_ASSERT(nullptr != pdrgpcrsChild);

		pdrgpcnstr->Append(pcnstrChild);
		CColRefSetArray *pdrgpcrsMerged =
			PdrgpcrsMergeFromBoolOp(mp, pexpr, *ppdrgpcrs, pdrgpcrsChild);

		(*ppdrgpcrs)->Release();
		*ppdrgpcrs = pdrgpcrsMerged;
		pdrgpcrsChild->Release();
	}

	const ULONG length = pdrgpcnstr->Size();
	if (0 == length)
	{
		pdrgpcnstr->Release();
		return nullptr;
	}

	if (1 == length)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[0];
		pcnstrChild->AddRef();
		pdrgpcnstr->Release();

		if (CPredicateUtils::FNot(pexpr))
		{
			return GPOS_NEW(mp) CConstraintNegation(mp, pcnstrChild);
		}

		return pcnstrChild;
	}

	// we know we have more than one child
	if (CPredicateUtils::FAnd(pexpr))
	{
		return GPOS_NEW(mp) CConstraintConjunction(mp, pdrgpcnstr);
	}

	if (CPredicateUtils::FOr(pexpr))
	{
		return GPOS_NEW(mp) CConstraintDisjunction(mp, pdrgpcnstr);
	}

	return nullptr;
}

// create constraint from EXISTS/ANY scalar subquery
CConstraint *
CConstraint::PcnstrFromExistsAnySubquery(CMemoryPool *mp, CExpression *pexpr,
										 CColRefSetArray **ppdrgpcrs)
{
	GPOS_ASSERT(nullptr != pexpr);

	if (!CUtils::FCorrelatedExistsAnySubquery(pexpr))
	{
		return nullptr;
	}

	CExpression *pexprRel = (*pexpr)[0];
	GPOS_ASSERT(pexprRel->Pop()->FLogical());

	CPropConstraint *ppc = pexprRel->DerivePropertyConstraint();
	if (ppc == nullptr)
	{
		return nullptr;
	}

	*ppdrgpcrs = GPOS_NEW(mp) CColRefSetArray(mp);
	CConstraintArray *pdrgpcnstr = GPOS_NEW(mp) CConstraintArray(mp);
	CColRefSet *outRefs = pexprRel->DeriveOuterReferences();
	CColRefSetIter crsi(*outRefs);

	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CColRefSet *equivOutRefs = ppc->PcrsEquivClass(colref);
		if (equivOutRefs == nullptr || equivOutRefs->Size() == 0)
		{
			CRefCount::SafeRelease(equivOutRefs);
			continue;
		}
		CConstraint *cnstr4Outer = ppc->Pcnstr()->Pcnstr(mp, equivOutRefs);
		if (cnstr4Outer == nullptr || cnstr4Outer->IsConstraintUnbounded())
		{
			CRefCount::SafeRelease(equivOutRefs);
			CRefCount::SafeRelease(cnstr4Outer);
			continue;
		}

		CConstraint *cnstrCol = cnstr4Outer->PcnstrRemapForColumn(mp, colref);
		pdrgpcnstr->Append(cnstrCol);
		cnstr4Outer->Release();
		AddColumnToEquivClasses(mp, colref, *ppdrgpcrs);
	}

	return CConstraint::PcnstrConjunction(mp, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcrsMergeFromBoolOp
//
//	@doc:
//		Merge equivalence classes coming from children of a bool op
//
//---------------------------------------------------------------------------
CColRefSetArray *
CConstraint::PdrgpcrsMergeFromBoolOp(CMemoryPool *mp, CExpression *pexpr,
									 CColRefSetArray *pdrgpcrsFst,
									 CColRefSetArray *pdrgpcrsSnd)
{
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(CUtils::FScalarBoolOp(pexpr));
	GPOS_ASSERT(nullptr != pdrgpcrsFst);
	GPOS_ASSERT(nullptr != pdrgpcrsSnd);

	if (CPredicateUtils::FAnd(pexpr))
	{
		// merge with the equivalence classes we have so far
		return CUtils::PdrgpcrsMergeEquivClasses(mp, pdrgpcrsFst, pdrgpcrsSnd);
	}

	if (CPredicateUtils::FOr(pexpr))
	{
		// in case of an OR, an equivalence class must be coming from all
		// children to be part of the output
		return CUtils::PdrgpcrsIntersectEquivClasses(mp, pdrgpcrsFst,
													 pdrgpcrsSnd);
	}

	GPOS_ASSERT(CPredicateUtils::FNot(pexpr));
	return GPOS_NEW(mp) CColRefSetArray(mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrOnColumn
//
//	@doc:
//		Return a subset of the given constraints which reference the
//		given column
//
//---------------------------------------------------------------------------
CConstraintArray *
CConstraint::PdrgpcnstrOnColumn(
	CMemoryPool *mp, CConstraintArray *pdrgpcnstr, CColRef *colref,
	BOOL fExclusive	 // returned constraints must reference ONLY the given col
)
{
	CConstraintArray *pdrgpcnstrSubset = GPOS_NEW(mp) CConstraintArray(mp);

	const ULONG length = pdrgpcnstr->Size();

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*pdrgpcnstr)[ul];
		CColRefSet *pcrs = pcnstr->PcrsUsed();

		// if the fExclusive flag is true, then colref must be the only column
		if (pcrs->FMember(colref) && (!fExclusive || 1 == pcrs->Size()))
		{
			pcnstr->AddRef();
			pdrgpcnstrSubset->Append(pcnstr);
		}
	}

	return pdrgpcnstrSubset;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PexprScalarConjDisj
//
//	@doc:
//		Construct a conjunction or disjunction scalar expression from an
//		array of constraints
//
//---------------------------------------------------------------------------
CExpression *
CConstraint::PexprScalarConjDisj(CMemoryPool *mp, CConstraintArray *pdrgpcnstr,
								 BOOL fConj)
{
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexpr = (*pdrgpcnstr)[ul]->PexprScalar(mp);
		pexpr->AddRef();
		pdrgpexpr->Append(pexpr);
	}

	if (fConj)
	{
		return CPredicateUtils::PexprConjunction(mp, pdrgpexpr);
	}

	return CPredicateUtils::PexprDisjunction(mp, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrFlatten
//
//	@doc:
//		Flatten an array of constraints to be used as children for a conjunction
//		or disjunction. If any of these children is of the same type then use
//		its children directly instead of having multiple levels of the same type
//
//---------------------------------------------------------------------------
CConstraintArray *
CConstraint::PdrgpcnstrFlatten(CMemoryPool *mp, CConstraintArray *pdrgpcnstr,
							   EConstraintType ect)
{
	CConstraintArray *pdrgpcnstrNew = GPOS_NEW(mp) CConstraintArray(mp);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[ul];
		EConstraintType ectChild = pcnstrChild->Ect();

		if (EctConjunction == ectChild && EctConjunction == ect)
		{
			CConstraintConjunction *pcconj =
				(CConstraintConjunction *) pcnstrChild;
			CUtils::AddRefAppend(pdrgpcnstrNew, pcconj->Pdrgpcnstr());
		}
		else if (EctDisjunction == ectChild && EctDisjunction == ect)
		{
			CConstraintDisjunction *pcdisj =
				(CConstraintDisjunction *) pcnstrChild;
			CUtils::AddRefAppend(pdrgpcnstrNew, pcdisj->Pdrgpcnstr());
		}
		else
		{
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
		}
	}

	pdrgpcnstr->Release();
	return PdrgpcnstrDeduplicate(mp, pdrgpcnstrNew, ect);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrDeduplicate
//
//	@doc:
//		Simplify an array of constraints to be used as children for a conjunction
//		or disjunction. If there are two or more elements that reference only one
//		particular column, these constraints are combined into one
//
//---------------------------------------------------------------------------
CConstraintArray *
CConstraint::PdrgpcnstrDeduplicate(CMemoryPool *mp,
								   CConstraintArray *pdrgpcnstr,
								   EConstraintType ect)
{
	CConstraintArray *pdrgpcnstrNew = GPOS_NEW(mp) CConstraintArray(mp);

	CAutoRef<CColRefSet> pcrsDeduped(GPOS_NEW(mp) CColRefSet(mp));
	CAutoRef<IColConstraintsMapper> arccm;

	const ULONG length = pdrgpcnstr->Size();

	if (length >= 5)
	{
		arccm = GPOS_NEW(mp) CColConstraintsHashMapper(mp, pdrgpcnstr);
	}
	else
	{
		pdrgpcnstr->AddRef();
		arccm = GPOS_NEW(mp) CColConstraintsArrayMapper(mp, pdrgpcnstr);
	}

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[ul];
		CColRefSet *pcrs = pcnstrChild->PcrsUsed();

		GPOS_ASSERT(0 != pcrs->Size());
		// we only simplify constraints that reference a single column, otherwise
		// we add constraint as is
		if (1 != pcrs->Size())
		{
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
			continue;
		}

		CColRef *colref = pcrs->PcrFirst();
		if (pcrsDeduped->FMember(colref))
		{
			// current constraint has already been combined with a previous one
			continue;
		}

		CConstraintArray *pdrgpcnstrCol = arccm->PdrgPcnstrLookup(colref);

		if (1 == pdrgpcnstrCol->Size())
		{
			// if there is only one such constraint, then no simplification
			// for this column
			pdrgpcnstrCol->Release();
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
			continue;
		}

		CExpression *pexpr = nullptr;

		if (EctConjunction == ect)
		{
			pexpr = PexprScalarConjDisj(mp, pdrgpcnstrCol, true /*fConj*/);
		}
		else
		{
			GPOS_ASSERT(EctDisjunction == ect);
			pexpr = PexprScalarConjDisj(mp, pdrgpcnstrCol, false /*fConj*/);
		}
		pdrgpcnstrCol->Release();
		GPOS_ASSERT(nullptr != pexpr);

		CConstraint *pcnstrNew =
			CConstraintInterval::PciIntervalFromScalarExpr(mp, pexpr, colref);
		if (nullptr == pcnstrNew)
		{
			// We ran into a type conflict that prevents us from using this method to simplify the constraint.
			// Give up and return the un-flattened constraint.
			// Note that if we get here, that means that
			//   a) a single constraint
			//   b) in case of a conjunction expression, none of the constraints
			//   c) in case of a disjunction, at least one of the constraints
			// could not be converted.
			pcnstrChild->AddRef();
			pcnstrNew = pcnstrChild;
		}

		pexpr->Release();
		pdrgpcnstrNew->Append(pcnstrNew);
		pcrsDeduped->Include(colref);
	}

	pdrgpcnstr->Release();

	return pdrgpcnstrNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Phmcolconstr
//
//	@doc:
//		Construct mapping between columns and arrays of constraints
//
//---------------------------------------------------------------------------
ColRefToConstraintArrayMap *
CConstraint::Phmcolconstr(CMemoryPool *mp, CColRefSet *pcrs,
						  CConstraintArray *pdrgpcnstr)
{
	ColRefToConstraintArrayMap *phmcolconstr =
		GPOS_NEW(mp) ColRefToConstraintArrayMap(mp);

	CColRefSetIter crsi(*pcrs);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CConstraintArray *pdrgpcnstrCol =
			PdrgpcnstrOnColumn(mp, pdrgpcnstr, colref, false /*fExclusive*/);

		BOOL fres GPOS_ASSERTS_ONLY =
			phmcolconstr->Insert(colref, pdrgpcnstrCol);
		GPOS_ASSERT(fres);
	}

	return phmcolconstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjDisjRemapForColumn
//
//	@doc:
//		Return a copy of the conjunction/disjunction constraint for a different column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjDisjRemapForColumn(CMemoryPool *mp, CColRef *colref,
										  CConstraintArray *pdrgpcnstr,
										  BOOL fConj)
{
	GPOS_ASSERT(nullptr != colref);

	CConstraintArray *pdrgpcnstrNew = GPOS_NEW(mp) CConstraintArray(mp);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		// clone child
		CConstraint *pcnstrChild =
			(*pdrgpcnstr)[ul]->PcnstrRemapForColumn(mp, colref);
		GPOS_ASSERT(nullptr != pcnstrChild);

		pdrgpcnstrNew->Append(pcnstrChild);
	}

	if (fConj)
	{
		return PcnstrConjunction(mp, pdrgpcnstrNew);
	}
	return PcnstrDisjunction(mp, pdrgpcnstrNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Contains
//
//	@doc:
//		Does the current constraint contain the given one?
//
//---------------------------------------------------------------------------
BOOL
CConstraint::Contains(CConstraint *pcnstr)
{
	if (IsConstraintUnbounded())
	{
		return true;
	}

	if (nullptr == pcnstr || pcnstr->IsConstraintUnbounded())
	{
		return false;
	}

	if (this == pcnstr)
	{
		// a constraint always contains itself
		return true;
	}

	// check if we have computed this containment query before
	BOOL *pfContains = m_phmcontain->Find(pcnstr);
	if (nullptr != pfContains)
	{
		return *pfContains;
	}

	BOOL fContains = true;

	// for each column used by the current constraint, we have to make sure that
	// the constraint on this column contains the corresponding given constraint
	CColRefSetIter crsi(*m_pcrsUsed);
	while (fContains && crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CConstraint *pcnstrColThis = Pcnstr(m_mp, colref);
		GPOS_ASSERT(nullptr != pcnstrColThis);
		CConstraint *pcnstrColOther = pcnstr->Pcnstr(m_mp, colref);

		// convert each of them to interval (if they are not already)
		CConstraintInterval *pciThis =
			CConstraintInterval::PciIntervalFromConstraint(m_mp, pcnstrColThis,
														   colref);
		CConstraintInterval *pciOther =
			CConstraintInterval::PciIntervalFromConstraint(m_mp, pcnstrColOther,
														   colref);

		fContains = pciThis->FContainsInterval(m_mp, pciOther);
		pciThis->Release();
		pciOther->Release();
		pcnstrColThis->Release();
		CRefCount::SafeRelease(pcnstrColOther);
	}

	// insert containment query into the local map
	BOOL fSuccess GPOS_ASSERTS_ONLY =
		m_phmcontain->Insert(pcnstr, PfVal(fContains));
	GPOS_ASSERT(fSuccess);

	return fContains;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Equals
//
//	@doc:
//		Equality function
//
//---------------------------------------------------------------------------
BOOL
CConstraint::Equals(CConstraint *pcnstr)
{
	if (nullptr == pcnstr || pcnstr->IsConstraintUnbounded())
	{
		return IsConstraintUnbounded();
	}

	// check for pointer equality first
	if (this == pcnstr)
	{
		return true;
	}

	return (this->Contains(pcnstr) && pcnstr->Contains(this));
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PrintConjunctionDisjunction
//
//	@doc:
//		Common functionality for printing conjunctions and disjunctions
//
//---------------------------------------------------------------------------
IOstream &
CConstraint::PrintConjunctionDisjunction(IOstream &os,
										 CConstraintArray *pdrgpcnstr) const
{
	EConstraintType ect = Ect();
	GPOS_ASSERT(EctConjunction == ect || EctDisjunction == ect);

	os << "(";
	const ULONG arity = pdrgpcnstr->Size();
	(*pdrgpcnstr)[0]->OsPrint(os);

	for (ULONG ul = 1; ul < arity; ul++)
	{
		if (EctConjunction == ect)
		{
			os << " AND ";
		}
		else
		{
			os << " OR ";
		}
		(*pdrgpcnstr)[ul]->OsPrint(os);
	}
	os << ")";

	return os;
}

CColRefSet *
CConstraint::PcrsFromConstraints(CMemoryPool *mp, CConstraintArray *pdrgpcnstr)
{
	CColRefSet *crs = GPOS_NEW(mp) CColRefSet(mp);

	ULONG const length = pdrgpcnstr->Size();
	GPOS_ASSERT(0 < length);

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*pdrgpcnstr)[ul];
		crs->Include(pcnstr->PcrsUsed());
	}

	return crs;
}

// EOF
