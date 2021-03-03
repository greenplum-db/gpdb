//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalNAryJoin.h
//
//	@doc:
//		N-ary inner join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalNAryJoin_H
#define GPOS_CLogicalNAryJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalNAryJoin
//
//	@doc:
//		N-ary inner join operator
//
//---------------------------------------------------------------------------
class CLogicalNAryJoin : public CLogicalJoin
{
private:
	// Indexes that help to find ON predicates for LOJs.
	// If all joins in this NAry join are inner joins, this pointer is NULL and
	// the scalar child of this NAry join contains all join predicates directly.
	// Otherwise, the scalar child is a CScalarNaryJoinPredList and this ULongPtr
	// array has as many entries as there are logical children of the NAry
	// join. For each logical child i, if that child i is part of an inner join
	// or the outer table of an LOJ, then the corresponding entry
	// (*m_lojChildPredIndexes)[i] in this array is 0
	// (GPOPT_ZERO_INNER_JOIN_PRED_INDEX). If the logical child is the right table
	// of an LOJ, the corresponding index indicates the child index of the
	// CScalarNAryJoinPredList expression that contains the ON predicate for the
	// LOJ.
	ULongPtrArray *m_lojChildPredIndexes;

public:
	CLogicalNAryJoin(const CLogicalNAryJoin &) = delete;

	// ctor
	explicit CLogicalNAryJoin(CMemoryPool *mp);

	CLogicalNAryJoin(CMemoryPool *mp, ULongPtrArray *lojChildIndexes);

	// dtor
	~CLogicalNAryJoin() override
	{
		CRefCount::SafeRelease(m_lojChildPredIndexes);
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalNAryJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalNAryJoin";
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive not nullable columns
	CColRefSet *DeriveNotNullColumns(CMemoryPool *mp,
									 CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *DerivePropertyConstraint(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// promise level for stat derivation
	EStatPromise
	Esp(CExpressionHandle &	 // exprhdl
	) const override
	{
		// we should use the expanded join order for stat derivation
		return EspLow;
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalNAryJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);

		return dynamic_cast<CLogicalNAryJoin *>(pop);
	}

	// conversion function, only if the NAryJoin has LOJ children
	static CLogicalNAryJoin *PopConvertNAryLOJ(COperator *pop);

	BOOL
	HasOuterJoinChildren() const
	{
		return (nullptr != m_lojChildPredIndexes);
	}

	BOOL
	IsInnerJoinChild(ULONG child_num) const
	{
		return (nullptr == m_lojChildPredIndexes ||
				*((*m_lojChildPredIndexes)[child_num]) == 0);
	}

	ULongPtrArray *
	GetLojChildPredIndexes() const
	{
		return m_lojChildPredIndexes;
	}

	CExpression *
	GetInnerJoinPreds(CExpression *nary_join_expr) const
	{
		GPOS_ASSERT(nary_join_expr->Pop() == this);
		if (HasOuterJoinChildren())
		{
			// return the first child of CScalarNAryJoinPredList
			return (*((*nary_join_expr)[nary_join_expr->Arity() - 1]))[0];
		}
		return (*nary_join_expr)[nary_join_expr->Arity() - 1];
	}

	CExpression *
	GetOnPredicateForLOJChild(CExpression *nary_join_expr,
							  ULONG child_num) const
	{
		GPOS_ASSERT(nary_join_expr->Pop() == this);
		GPOS_ASSERT(0 < *(*m_lojChildPredIndexes)[child_num]);

		// m_lojChildPredIndexes stores the index in the child of the scalar argument
		// (a CScalarNAryJoinPredList) that has our ON predicate
		//     |------ the scalar child of the NAry join ------|  |- grandchild corresponding to LOJ-|
		return (*((*nary_join_expr)[nary_join_expr->Arity() - 1]))[*(
			*m_lojChildPredIndexes)[child_num]];
	}

	// get the true inner join predicates, excluding predicates that use ColRefs
	// coming from non-inner joins
	CExpression *GetTrueInnerJoinPreds(CMemoryPool *mp,
									   CExpressionHandle &exprhdl) const;

	// given an existing scalar child of an NAry join, make a new copy, replacing
	// only the inner join predicates and leaving the LOJ ON predicates the same
	CExpression *ReplaceInnerJoinPredicates(
		CMemoryPool *mp, CExpression *old_nary_join_scalar_expr,
		CExpression *new_inner_join_preds);

	IOstream &OsPrint(IOstream &os) const override;

};	// class CLogicalNAryJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalNAryJoin_H

// EOF
