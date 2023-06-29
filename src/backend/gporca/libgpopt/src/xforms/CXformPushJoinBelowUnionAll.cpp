//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformPushJoinBelowUnionAll.cpp
//
//	@doc:
//		Implementation of pushing join below union all transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformPushJoinBelowUnionAll.h"

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLeftOuterJoin.h"
#include "gpopt/operators/CLogicalUnionAll.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternMultiLeaf.h"
#include "gpopt/operators/CPatternNode.h"

using namespace gpopt;

CXform::EXformPromise
CXformPushJoinBelowUnionAll::Exfp(CExpressionHandle &exprhdl) const
{
	if (exprhdl.DeriveHasSubquery(2) || exprhdl.HasOuterRefs())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformPushJoinBelowUnionAll::Transform
//
//	@doc:
//		Actual transformation
//
//  Input:
//                     _________Join_________
//                    |                      |
//         _______Union All_____           other
//        |      |       |      |
//     child1  child2  child3  child4
//
//
//
//  Ouptut:
//         _________________Union All_________________
//        |              |             |              |
//     _Join_          _Join_        _Join_         _Join_
//    |      |        |      |      |      |       |      |
//  child1 other   child2 other   child3 other   child4 other
//
//---------------------------------------------------------------------------
void
CXformPushJoinBelowUnionAll::Transform(CXformContext *pxfctxt,
									   CXformResult *pxfres,
									   CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components
	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprRight = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	// If both children are union alls, return
	// We only perform transform when only one child is union all
	if (COperator::EopLogicalUnionAll == pexprLeft->Pop()->Eopid() &&
		COperator::EopLogicalUnionAll == pexprRight->Pop()->Eopid())
	{
		return;
	}

	CExpression *pexprUnionAll, *pexprOther;

	// This is used to preserve the join order, which we leave
	// for other xforms to optimize
	BOOL isLeftChildUnion;

	if (COperator::EopLogicalUnionAll == pexprLeft->Pop()->Eopid())
	{
		pexprUnionAll = pexprLeft;
		pexprOther = pexprRight;
		isLeftChildUnion = true;
	}
	else
	{
		pexprUnionAll = pexprRight;
		pexprOther = pexprLeft;
		isLeftChildUnion = false;
	}

	CLogicalUnionAll *popUnionAll =
		CLogicalUnionAll::PopConvert(pexprUnionAll->Pop());
	CColRef2dArray *union_input_columns = popUnionAll->PdrgpdrgpcrInput();

	// used for alternative union all expression
	CColRef2dArray *input_columns = GPOS_NEW(mp) CColRef2dArray(mp);
	CExpressionArray *join_array = GPOS_NEW(mp) CExpressionArray(mp);

	CColRefArray *other_colref_array =
		pexprOther->DeriveOutputColumns()->Pdrgpcr(mp);
	CColRefArray *colref_array_from = GPOS_NEW(mp) CColRefArray(mp);

	// Iterate through all union all children
	const ULONG arity = pexprUnionAll->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexprUnionAll)[ul];
		CColRefArray *child_colref_array = (*union_input_columns)[ul];

		CExpression *pexprLeftChild, *pexprRightChild, *pexprRemappedScalar,
			*pexprRemappedOther, *join_expr;

		if (0 == ul)
		{
			// The 1st child is special
			// The join table and condition can be readily used
			// and doesn't require remapping
			pexprRemappedScalar = pexprScalar;
			pexprRemappedOther = pexprOther;
			pexprRemappedScalar->AddRef();
			pexprRemappedOther->AddRef();

			// We append the output columns from the 1st union all child,
			// and from the other table, and use them as the source
			// of column remapping
			colref_array_from->AppendArray(child_colref_array);
			colref_array_from->AppendArray(other_colref_array);
			input_columns->Append(colref_array_from);
		}
		else
		{
			CColRefArray *colref_array_to = GPOS_NEW(mp) CColRefArray(mp);
			CColRefArray *other_colref_array_copy =
				CUtils::PdrgpcrCopy(mp, other_colref_array);
			// We append the output columns from the 2nd (and onward)
			// union all child, and a copy of the other table's output
			// columns, and use them as the destination of column
			// remapping
			colref_array_to->AppendArray(child_colref_array);
			colref_array_to->AppendArray(other_colref_array_copy);
			input_columns->Append(colref_array_to);

			UlongToColRefMap *colref_mapping =
				CUtils::PhmulcrMapping(mp, colref_array_from, colref_array_to);

			// Create a copy of the join condition with remapped columns,
			// and a copy of the other expression with remapped columns
			pexprRemappedScalar = pexprScalar->PexprCopyWithRemappedColumns(
				mp, colref_mapping, true /*must_exist*/);
			pexprRemappedOther = pexprOther->PexprCopyWithRemappedColumns(
				mp, colref_mapping, true);
			other_colref_array_copy->Release();
			colref_mapping->Release();
		}

		// Preserve the join order
		if (isLeftChildUnion)
		{
			pexprLeftChild = pexprChild;
			pexprRightChild = pexprRemappedOther;
			pexprLeftChild->AddRef();
		}
		else
		{
			pexprLeftChild = pexprRemappedOther;
			pexprRightChild = pexprChild;
			pexprRightChild->AddRef();
		}


		BOOL isOuterJoin =
			pexpr->Pop()->Eopid() == COperator::EopLogicalLeftOuterJoin;
		if (isOuterJoin)
		{
			join_expr = CUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>(
				mp, pexprLeftChild, pexprRightChild, pexprRemappedScalar);
		}
		else
		{
			join_expr = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
				mp, pexprLeftChild, pexprRightChild, pexprRemappedScalar);
		}
		join_array->Append(join_expr);
	}
	other_colref_array->Release();

	CColRefArray *output_columns = pexpr->DeriveOutputColumns()->Pdrgpcr(mp);
	CExpression *pexprAlt = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CLogicalUnionAll(mp, output_columns, input_columns),
		join_array);

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
