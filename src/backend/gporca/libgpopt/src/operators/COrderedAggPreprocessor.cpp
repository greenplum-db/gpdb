//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2022 VMware, Inc. or its affiliates.
//
//	@filename:
//		COrderedAggPreprocessor.cpp
//
//	@doc:
//		Preprocessing routines of ordered-set aggregate
//---------------------------------------------------------------------------

#include "gpopt/operators/COrderedAggPreprocessor.h"

#include "gpos/base.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CLogicalCTEAnchor.h"
#include "gpopt/operators/CLogicalCTEConsumer.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CLogicalSequenceProject.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CScalarProjectElement.h"
#include "gpopt/operators/CScalarProjectList.h"
#include "gpopt/operators/CScalarSortGroupClause.h"
#include "gpopt/operators/CScalarValuesList.h"
#include "gpopt/operators/CScalarWindowFunc.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		COrderedAggPreprocessor::SplitPrjList
//
//	@doc:
//		Iterate over project elements and split them elements between
//		Ordered Aggs list, and Other Aggs list
//
//---------------------------------------------------------------------------
void
COrderedAggPreprocessor::SplitPrjList(
	CMemoryPool *mp, CExpression *pexprInputAggPrj,
	CExpressionArray **
		ppdrgpexprOrderedAggsPrEl,	// output: list of project elements with Ordered Aggs
	CExpressionArray **
		ppdrgpexprOtherPrEl	 // output: list of project elements with Other aggregate functions
)
{
	GPOS_ASSERT(nullptr != pexprInputAggPrj);
	GPOS_ASSERT(nullptr != ppdrgpexprOrderedAggsPrEl);
	GPOS_ASSERT(nullptr != ppdrgpexprOtherPrEl);

	CExpression *pexprPrjList = (*pexprInputAggPrj)[1];

	CExpressionArray *pdrgpexprOrderedAggsPrEl =
		GPOS_NEW(mp) CExpressionArray(mp);

	CExpressionArray *pdrgpexprOtherPrEl = GPOS_NEW(mp) CExpressionArray(mp);

	// iterate over project list and split project elements between
	// Ordered Aggs list, and Other aggs list
	const ULONG arity = pexprPrjList->Arity();
	CColRefArray *colref_array = nullptr;
	CExpressionArray *pdrgpexprSortClauseArray = nullptr;

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprPrjEl = (*pexprPrjList)[ul];
		CExpression *pexprAggFunc = (*pexprPrjEl)[0];
		CScalarProjectElement *popScPrjElem =
			CScalarProjectElement::PopConvert(pexprPrjEl->Pop());
		CColRef *pcrPrjElem = popScPrjElem->Pcr();

		if (CUtils::FHasOrderedAggToSplit(pexprAggFunc))
		{
			CExpression *pexprSortCol =
				(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0];
			CExpression *pexprSortGroupClause =
				(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexOrder])[0];
			CScalarSortGroupClause *curr_sort_clause =
				CScalarSortGroupClause::PopConvert(pexprSortGroupClause->Pop());

			const CColRef *colref =
				CCastUtils::PcrExtractFromScIdOrCastScId(pexprSortCol);
			BOOL skip = false;
			if (nullptr == pdrgpexprSortClauseArray)
			{
				colref_array = GPOS_NEW(mp) CColRefArray(mp);
				colref_array->Append(const_cast<CColRef *>(colref));
				pdrgpexprSortClauseArray = GPOS_NEW(mp) CExpressionArray(mp);
				pexprSortGroupClause->AddRef();
				pdrgpexprSortClauseArray->Append(pexprSortGroupClause);
			}
			else
			{
				for (ULONG uidx = 0; uidx < pdrgpexprSortClauseArray->Size();
					 uidx++)
				{
					// For multiple ordered-set aggs on the same ORDERING column and ORDERING SPEC(ASC/DESC), we optimize
					// to add the new aggregate as a ProjectElement to the ProjectList of an existing JOIN, instead of
					// creating a new JOIN for doing the same SORT.
					// Eg. SELECT percentile_cont(0.25) WITHIN GROUP(ORDER BY a), percentile_cont(0.5) WITHIN GROUP(ORDER BY a) from tab;
					// Currently, to check if colref's SORT ORDER Spec(Asc/Desc) is same, we match only the SortOp and NullFirst
					// as that is what is used for creating the OrderSpec
					// TO-DO: (enhancement) Instead of this loop, we can probably use a map (colref->SortGroupClause)
					CScalarSortGroupClause *visited_sort_clause =
						CScalarSortGroupClause::PopConvert(
							(*pdrgpexprSortClauseArray)[uidx]->Pop());
					if (colref == (*colref_array)[uidx] &&
						curr_sort_clause->SortOp() ==
							visited_sort_clause->SortOp() &&
						curr_sort_clause->NullsFirst() ==
							visited_sort_clause->NullsFirst())
					{
						// Create the new ordered agg function and append it as a ProjElement to the existing ProjectList
						// created for and already split ordered agg on the same column with same ORDERING SPEC
						// Existing ProjList:
						// +--CScalarProjectList
						// +--CScalarProjectElement \"percentile_cont\" (12)
						//    +--CScalarAggFunc
						//       |--CScalarValuesList
						//       |  |--CScalarIdent \"a\" (1)
						//       |  |--CScalarConst (0.2500)
						//       |  +--CScalarIdent \"ColRef_0041\" (41)
						//       ...
						CExpression *pCurrExprProjList =
							(*(*pdrgpexprOrderedAggsPrEl)[uidx])[1];
						CExpression *pCurrExprAggFunc =
							(*(*pCurrExprProjList)[0])[0];
						const CColRef *total_count_colref =
							CCastUtils::PcrExtractFromScIdOrCastScId((*(
								*pCurrExprAggFunc)[EAggfuncChildIndices::
													   EaggfuncIndexArgs])[2]);
						const CColRef *peer_count_colref =
							CCastUtils::PcrExtractFromScIdOrCastScId((*(
								*pCurrExprAggFunc)[EAggfuncChildIndices::
													   EaggfuncIndexArgs])[3]);

						CExpression *pexprNewAggFunc = PexprFinalAgg(
							mp, pexprAggFunc, pcrPrjElem,
							const_cast<CColRef *>(total_count_colref),
							const_cast<CColRef *>(peer_count_colref));
						CExpression *pexprAddPrjElem =
							CUtils::PexprScalarProjectElement(mp, pcrPrjElem,
															  pexprNewAggFunc);
						// Updated ProjList:
						// +--CScalarProjectList
						//    |--CScalarProjectElement \"percentile_cont\" (12)
						//    |  +--CScalarAggFunc
						//    |     |--CScalarValuesList
						//    |     |  |--CScalarIdent \"a\" (1)
						//    |     |  |--CScalarConst (0.2500)
						//    |     |  +--CScalarIdent \"ColRef_0041\" (41)
						//    |     ...
						//    +--CScalarProjectElement \"percentile_cont\" (13)
						//       +--CScalarAggFunc
						//          |--CScalarValuesList
						//          |  |--CScalarIdent \"a\" (1)
						//          |  |--CScalarConst (0.500)
						//          |  +--CScalarIdent \"ColRef_0041\" (41)
						//          ...
						CExpressionArray *pCurrProjList =
							pCurrExprProjList->PdrgPexpr();
						pCurrProjList->Append(pexprAddPrjElem);
						skip = true;
						break;
					}
				}
				if (!skip)
				{
					colref_array->Append(const_cast<CColRef *>(colref));
					pexprSortGroupClause->AddRef();
					pdrgpexprSortClauseArray->Append(pexprSortGroupClause);
				}
			}
			if (skip)
			{
				continue;
			}

			/*
			 * This preprocessor step, splits the percentile_cont() function into the following pattern
			 * Sequence
			 * |
			 * |_____ CTEProducer (SELECT col, count(col) peer_count FROM table GROUP BY col)
			 * |      |
			 * |      |______ Scan (table)
			 * |
			 * |______ CLogicalGbAgg (gp_percentile agg(col, percentile_fraction, total_cnt, peer_count))
			 *         |____NLJ
			 *              |
			 *              |_______ CLogicalGbAgg (count total rows => SELECT SUM(peer_count) total_cnt FROM CTE)
			 *              |        |
			 *              |        |_____ CTEConsumer
			 *              |
			 *              |_______ CTEConsumer
			 * */

			// STEP 1: CREATE Aggregate for peer_count
			CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
			CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
			CExpression *pexprAgg = CUtils::PexprAgg(
				mp, md_accessor, IMDType::EaggCount, colref, false);
			CColRef *peer_count_colref = col_factory->PcrCreate(
				md_accessor->RetrieveType(
					CScalarAggFunc::PopConvert(pexprAgg->Pop())->MdidType()),
				CScalarAggFunc::PopConvert(pexprAgg->Pop())->TypeModifier());
			CExpression *pexprNewPrjElem = CUtils::PexprScalarProjectElement(
				mp, peer_count_colref, pexprAgg);
			CExpression *pexprCountAggPrjList = GPOS_NEW(mp) CExpression(
				mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprNewPrjElem);
			CColRefArray *pc = GPOS_NEW(mp) CColRefArray(mp);
			pc->Append(const_cast<CColRef *>(colref));
			(*pexprInputAggPrj)[0]->AddRef();
			CExpression *pexprPeerCountAgg = CUtils::PexprLogicalGbAggGlobal(
				mp, pc, (*pexprInputAggPrj)[0], pexprCountAggPrjList);

			// STEP 1(a): CREATE CTE for the peer_count
			CExpression *pexprRemappedPCConsumer = nullptr;
			CExpression *pexprPCConsumer = nullptr;
			CreateCTE(mp, pexprPeerCountAgg, &pexprRemappedPCConsumer,
					  &pexprPCConsumer);

			// STEP 2: Create Aggregate for calculating total_count SUM(peer_count)
			CExpression *pexprSumAgg = CUtils::PexprAgg(
				mp, md_accessor, IMDType::EaggSum, peer_count_colref, false);
			CColRef *total_count_colref = col_factory->PcrCreate(
				md_accessor->RetrieveType(
					CScalarAggFunc::PopConvert(pexprSumAgg->Pop())->MdidType()),
				CScalarAggFunc::PopConvert(pexprSumAgg->Pop())->TypeModifier());
			CExpression *pexprSumPrjElem = CUtils::PexprScalarProjectElement(
				mp, total_count_colref, pexprSumAgg);
			CExpression *pexprSumAggPrjList = GPOS_NEW(mp) CExpression(
				mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprSumPrjElem);
			CExpression *pexprTotalCountAgg = CUtils::PexprLogicalGbAggGlobal(
				mp, GPOS_NEW(mp) CColRefArray(mp), pexprPeerCountAgg,
				pexprSumAggPrjList);

			// to match requested columns upstream, we have to re-use the same computed
			// columns that define the aggregates, we avoid recreating new columns during
			// expression copy by passing must_exist as false
			CColRefArray *pdrgpcrChildOutput =
				pexprPCConsumer->DeriveOutputColumns()->Pdrgpcr(mp);
			CColRefArray *pdrgpcrConsumerOutput =
				CLogicalCTEConsumer::PopConvert(pexprRemappedPCConsumer->Pop())
					->Pdrgpcr();
			UlongToColRefMap *colref_mapping = CUtils::PhmulcrMapping(
				mp, pdrgpcrChildOutput, pdrgpcrConsumerOutput);
			CExpression *pexprTotalCountAggRemapped =
				pexprTotalCountAgg->PexprCopyWithRemappedColumns(
					mp, colref_mapping, false /*must_exist*/);
			colref_mapping->Release();
			pdrgpcrChildOutput->Release();
			pexprTotalCountAgg->Release();

			// finalize total_count agg expression by replacing its child with CTE consumer
			pexprTotalCountAggRemapped->Pop()->AddRef();
			(*pexprTotalCountAggRemapped)[1]->AddRef();
			CExpression *pexprFinalGbAgg = GPOS_NEW(mp) CExpression(
				mp, pexprTotalCountAggRemapped->Pop(), pexprRemappedPCConsumer,
				(*pexprTotalCountAggRemapped)[1]);
			pexprTotalCountAggRemapped->Release();

			CExpression *pexprJoinCondition =
				CUtils::PexprScalarConstBool(mp, true /*value*/);

			// STEP 3: CREATE a join between expanded total_count and CTEConsumer expressions
			const CColRef *pcrSum = CScalarProjectElement::PopConvert(
										(*(*pexprFinalGbAgg)[1])[0]->Pop())
										->Pcr();
			CExpression *pexprScalarIdentSum =
				CUtils::PexprScalarIdent(mp, pcrSum);

			// STEP 3(a): Explicit Cast total_count to bigint.
			// Since count() returns bigint, sum(count()) returns numeric and gp_percentile agg's
			// expect the total_count as bigint, therefore adding an explicit cast on top
			IMDId *mdid_func =
				GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_INT8_CAST);
			const IMDFunction *cast_func = md_accessor->RetrieveFunc(mdid_func);
			const CWStringConst *pstrFunc = GPOS_NEW(mp) CWStringConst(
				mp, (cast_func->Mdname().GetMDName())->GetBuffer());
			mdid_func->AddRef();
			cast_func->GetResultTypeMdid()->AddRef();
			CScalarFunc *popCastScalarFunc = GPOS_NEW(mp) CScalarFunc(
				mp, mdid_func, cast_func->GetResultTypeMdid(), -1, pstrFunc);
			CExpression *pexprCastScalarIdent = GPOS_NEW(mp)
				CExpression(mp, popCastScalarFunc, pexprScalarIdentSum);
			CExpressionArray *colref_array1 = GPOS_NEW(mp) CExpressionArray(mp);
			colref_array1->Append(pexprCastScalarIdent);
			CExpression *pexprProjected = CUtils::PexprAddProjection(
				mp, pexprFinalGbAgg, colref_array1, false);
			CColRef *new_total_count_ref =
				CUtils::PcrFromProjElem((*(*pexprProjected)[1])[0]);
			CExpression *pexprJoin =
				CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
					mp, pexprPCConsumer, pexprProjected, pexprJoinCondition);
			colref_array1->Release();
			mdid_func->Release();

			// STEP 4: CREATE ordered spec for the Join to merge on the colref passed in as part
			// of the ordered agg's WITHIN(ORDER BY) clause
			COrderSpec *pos = GPOS_NEW(mp) COrderSpec(mp);
			IMDId *mdid_curr_sortop = GPOS_NEW(mp)
				CMDIdGPDB(IMDId::EmdidGeneral, curr_sort_clause->SortOp());
			IMDType::ECmpType curr_cmp_type =
				CUtils::ParseCmpType(mdid_curr_sortop);
			// Instead of using the SortOp directly, we use the colref's operator for the comparison type
			// to avoid incorret results for cast related columns
			IMDId *mdid_pos =
				colref->RetrieveType()->GetMdidForCmpType(curr_cmp_type);
			mdid_curr_sortop->Release();
			COrderSpec::ENullTreatment ent = COrderSpec::EntLast;
			if (curr_sort_clause->NullsFirst())
			{
				ent = COrderSpec::EntFirst;
			}
			mdid_pos->AddRef();
			pos->Append(mdid_pos, colref, ent);
			CLogicalLimit *popLimit = GPOS_NEW(mp)
				CLogicalLimit(mp, pos, true /* fGlobal */, true /* fHasCount */,
							  false /*fTopLimitUnderDML*/);
			CExpression *pexprLimitOffset = CUtils::PexprScalarConstInt8(mp, 0);
			CExpression *pexprLimitCount =
				CUtils::PexprScalarConstInt8(mp, 1, true);

			CExpression *pexprJoinLimit = GPOS_NEW(mp) CExpression(
				mp, popLimit, pexprJoin, pexprLimitOffset, pexprLimitCount);

			// STEP 5: CREATE the final gp_percentile agg on top
			CExpression *pexprNewAggFunc =
				PexprFinalAgg(mp, pexprAggFunc, pcrPrjElem, new_total_count_ref,
							  peer_count_colref);
			CExpression *pexprExistingPrjElem =
				CUtils::PexprScalarProjectElement(mp, pcrPrjElem,
												  pexprNewAggFunc);
			CExpression *pexprNewAggPrjList = GPOS_NEW(mp) CExpression(
				mp, GPOS_NEW(mp) CScalarProjectList(mp), pexprExistingPrjElem);
			CColRefArray *colref_array = GPOS_NEW(mp) CColRefArray(mp);
			CExpression *pexprNewAgg = CUtils::PexprLogicalGbAggGlobal(
				mp, colref_array, pexprJoinLimit, pexprNewAggPrjList);
			pdrgpexprOrderedAggsPrEl->Append(pexprNewAgg);
		}
		else
		{
			pexprAggFunc->AddRef();
			CExpression *pexprNewPrjElem =
				CUtils::PexprScalarProjectElement(mp, pcrPrjElem, pexprAggFunc);
			pdrgpexprOtherPrEl->Append(pexprNewPrjElem);
		}
	}
	CRefCount::SafeRelease(colref_array);
	CRefCount::SafeRelease(pdrgpexprSortClauseArray);
	*ppdrgpexprOrderedAggsPrEl = pdrgpexprOrderedAggsPrEl;
	*ppdrgpexprOtherPrEl = pdrgpexprOtherPrEl;
}


//---------------------------------------------------------------------------
//	@function:
//		COrderedAggPreprocessor::SplitOrderedAggsPrj
//
//	@doc:
//		Split InputAgg expression into:
//		- A GbAgg expression containing ordered aggs split to gp_percentile agg, and
//		- A GbAgg expression containing all remaining non-ordered agg functions
//
//---------------------------------------------------------------------------
void
COrderedAggPreprocessor::SplitOrderedAggsPrj(
	CMemoryPool *mp, CExpression *pexprInputAggPrj,
	CExpression **
		ppexprOrderedGbAgg,	 // output: GbAgg expression containing ordered aggs split to normal agg
	CExpression **
		ppexprRemainingAgg	// output: GbAgg expression containing all remaining agg functions
)
{
	GPOS_ASSERT(nullptr != pexprInputAggPrj);
	GPOS_ASSERT(nullptr != ppexprOrderedGbAgg);
	GPOS_ASSERT(nullptr != ppexprRemainingAgg);

	// split project elements between ordered Aggs list, and Other aggs list
	CExpressionArray *ppdrgpexprOrderedAggsPrEl = nullptr;
	CExpressionArray *pdrgpexprOtherPrEl = nullptr;
	SplitPrjList(mp, pexprInputAggPrj, &ppdrgpexprOrderedAggsPrEl,
				 &pdrgpexprOtherPrEl);

	CExpression *pexprInputAggPrjChild = (*pexprInputAggPrj)[0];

	CExpression *pexpr = (*ppdrgpexprOrderedAggsPrEl)[0];
	pexpr->AddRef();
	// in case of multiple ordered aggs, we need to expand the GbAgg expression
	// into a join expression where leaves carry split ordered aggs
	for (ULONG ul = 1; ul < ppdrgpexprOrderedAggsPrEl->Size(); ul++)
	{
		CExpression *pexprWindowConsumer = (*ppdrgpexprOrderedAggsPrEl)[ul];
		CExpression *pexprJoinCondition =
			CUtils::PexprScalarConstBool(mp, true /*value*/);

		// create a join between expanded DQAs and Window expressions
		pexprWindowConsumer->AddRef();
		CExpression *pexprJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
			mp, pexpr, pexprWindowConsumer, pexprJoinCondition);
		pexpr = pexprJoin;
	}

	*ppexprOrderedGbAgg = pexpr;
	if (0 == pdrgpexprOtherPrEl->Size())
	{
		// no remaining aggregate functions after excluding ordered aggs,
		// reuse the original InputAggPrj child in this case
		pdrgpexprOtherPrEl->Release();
		ppdrgpexprOrderedAggsPrEl->Release();
		*ppexprRemainingAgg = pexprInputAggPrjChild;

		return;
	}

	// create a new GbAgg expression for remaining aggregate functions
	CColRefArray *pdrgpcrGrpCols = nullptr;
	pdrgpcrGrpCols = GPOS_NEW(mp) CColRefArray(mp);
	pexprInputAggPrjChild->AddRef();
	*ppexprRemainingAgg = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp)
			CLogicalGbAgg(mp, pdrgpcrGrpCols, COperator::EgbaggtypeGlobal),
		pexprInputAggPrjChild,
		GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CScalarProjectList(mp),
								 pdrgpexprOtherPrEl));
	ppdrgpexprOrderedAggsPrEl->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		COrderedAggPreprocessor::CreateCTE
//
//	@doc:
//		Create a CTE with two consumers using the child expression of
//		Sequence Project
//
//---------------------------------------------------------------------------
void
COrderedAggPreprocessor::CreateCTE(CMemoryPool *mp, CExpression *pexprChild,
								   CExpression **ppexprFirstConsumer,
								   CExpression **ppexprSecondConsumer)
{
	GPOS_ASSERT(nullptr != pexprChild);
	GPOS_ASSERT(nullptr != ppexprFirstConsumer);
	GPOS_ASSERT(nullptr != ppexprSecondConsumer);

	CColRefSet *pcrsChildOutput = pexprChild->DeriveOutputColumns();
	CColRefArray *pdrgpcrChildOutput = pcrsChildOutput->Pdrgpcr(mp);

	// create a CTE producer based on SeqPrj child expression
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	const ULONG ulCTEId = pcteinfo->next_id();
	CExpression *pexprCTEProd = CXformUtils::PexprAddCTEProducer(
		mp, ulCTEId, pdrgpcrChildOutput, pexprChild);
	CColRefArray *pdrgpcrProducerOutput =
		pexprCTEProd->DeriveOutputColumns()->Pdrgpcr(mp);

	//	 first consumer creates new output columns to be used later as input to GbAgg expression
	*ppexprFirstConsumer = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CLogicalCTEConsumer(
				mp, ulCTEId, CUtils::PdrgpcrCopy(mp, pdrgpcrProducerOutput)));
	pcteinfo->IncrementConsumers(ulCTEId);
	pdrgpcrProducerOutput->Release();

	// second consumer reuses the same output columns of SeqPrj child to be able to provide any requested columns upstream
	*ppexprSecondConsumer = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CLogicalCTEConsumer(mp, ulCTEId, pdrgpcrChildOutput));
	pcteinfo->IncrementConsumers(ulCTEId);
}


//---------------------------------------------------------------------------
//    @function:
//        COrderedAggPreprocessor::PexprFinalAgg
//
//    @doc:
//        Return expression for final agg function
//        Input:
//          expression for percentile_cont(direct_arg) WITHIN GROUP(ORDER BY col)
//          total_count
//        Returns:
//          expression for gp_percentile_cont(col, direct_arg, total_count, peer_count)
//
//---------------------------------------------------------------------------
CExpression *
COrderedAggPreprocessor::PexprFinalAgg(CMemoryPool *mp,
									   CExpression *pexprAggFunc,
									   CColRef *arg_col_ref,
									   CColRef *total_count_colref,
									   CColRef *peer_count_colref)
{
	GPOS_ASSERT(nullptr != pexprAggFunc);
	GPOS_ASSERT(nullptr != arg_col_ref);
	GPOS_ASSERT(nullptr != total_count_colref);
	GPOS_ASSERT(nullptr != peer_count_colref);

	CScalarAggFunc *popScAggFunc =
		CScalarAggFunc::PopConvert(pexprAggFunc->Pop());
	CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
	CMDIdGPDB *mdid = CUtils::GetPercentileAggMDId(mp, pexprAggFunc);
	ULongPtrArray *argtypes = GPOS_NEW(mp) ULongPtrArray(mp);

	CScalarValuesList *popScalarValuesList = GPOS_NEW(mp) CScalarValuesList(mp);
	CExpressionArray *pdrgpexprChildren = GPOS_NEW(mp) CExpressionArray(mp);

	CExpression *pexprAggFuncArg =
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexArgs])[0];
	pexprAggFuncArg->AddRef();
	pdrgpexprChildren->Append(pexprAggFuncArg);
	argtypes->Append(GPOS_NEW(mp) ULONG(
		CMDIdGPDB::CastMdid(
			CScalar::PopConvert(pexprAggFuncArg->Pop())->MdidType())
			->Oid()));

	CExpression *pexprAggFuncDirectArg =
		(*(*pexprAggFunc)[EAggfuncChildIndices::EaggfuncIndexDirectArgs])[0];
	pexprAggFuncDirectArg->AddRef();
	pdrgpexprChildren->Append(pexprAggFuncDirectArg);
	argtypes->Append(GPOS_NEW(mp) ULONG(
		CMDIdGPDB::CastMdid(
			CScalar::PopConvert(pexprAggFuncDirectArg->Pop())->MdidType())
			->Oid()));
	pdrgpexprChildren->Append(CUtils::PexprScalarIdent(mp, total_count_colref));
	argtypes->Append(GPOS_NEW(mp) ULONG(
		CMDIdGPDB::CastMdid(total_count_colref->RetrieveType()->MDId())
			->Oid()));
	// Passing along the calculated peer_count col_ref for handling skew
	CExpression *pexprPeerCount =
		CUtils::PexprScalarIdent(mp, peer_count_colref);
	pdrgpexprChildren->Append(pexprPeerCount);
	argtypes->Append(GPOS_NEW(mp) ULONG(
		CMDIdGPDB::CastMdid(
			CScalar::PopConvert(pexprPeerCount->Pop())->MdidType())
			->Oid()));

	pdrgpexpr->Append(
		GPOS_NEW(mp) CExpression(mp, popScalarValuesList, pdrgpexprChildren));

	pdrgpexpr->Append(GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CScalarValuesList(mp),
									  GPOS_NEW(mp) CExpressionArray(mp)));

	pdrgpexpr->Append(GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CScalarValuesList(mp),
									  GPOS_NEW(mp) CExpressionArray(mp)));

	pdrgpexpr->Append(GPOS_NEW(mp)
						  CExpression(mp, GPOS_NEW(mp) CScalarValuesList(mp),
									  GPOS_NEW(mp) CExpressionArray(mp)));

	IMDId *ret_type = nullptr;
	if (popScAggFunc->FHasAmbiguousReturnType())
	{
		popScAggFunc->MdidType()->AddRef();
		ret_type = popScAggFunc->MdidType();
	}
	CScalarAggFunc *popNewAggFunc = CUtils::PopAggFunc(
		mp, mdid, arg_col_ref->Name().Pstr(), false /*is_distinct*/,
		EaggfuncstageGlobal /*eaggfuncstage*/, false /*fSplit*/, ret_type,
		EaggfunckindNormal, argtypes, popScAggFunc->FRepSafe());

	return GPOS_NEW(mp) CExpression(mp, popNewAggFunc, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		COrderedAggPreprocessor::PexprInputAggPrj2Join
//
//	@doc:
//		Transform GbAgg project expression with ordered aggregates
//		into an inner join expression,
//			- the outer child of the join is a GbAgg expression that computes
//			gp_percentile aggs,
//			- the inner child of the join is a GbAgg expression that computes
//			remaining aggregate functions
//
//		we use a CTE to compute the input to both join children, while maintaining
//		all column references upstream by reusing the same computed columns in the
//		original input expression
//
//---------------------------------------------------------------------------
CExpression *
COrderedAggPreprocessor::PexprInputAggPrj2Join(CMemoryPool *mp,
											   CExpression *pexprInputAggPrj)
{
	GPOS_ASSERT(nullptr != pexprInputAggPrj);
	GPOS_ASSERT(COperator::EopLogicalGbAgg == pexprInputAggPrj->Pop()->Eopid());
	GPOS_ASSERT(0 < (*pexprInputAggPrj)[1]->DeriveTotalOrderedAggs());

	// split InputAgg expression into a GbAgg expression (for ordered Aggs), and
	// another GbAgg expression (for remaining aggregate functions)
	CExpression *pexprOrderedAgg = nullptr;
	CExpression *pexprOtherAgg = nullptr;
	SplitOrderedAggsPrj(mp, pexprInputAggPrj, &pexprOrderedAgg, &pexprOtherAgg);

	CExpression *pexprFinalJoin = nullptr;
	if (COperator::EopLogicalGbAgg == pexprOtherAgg->Pop()->Eopid())
	{
		CExpression *pexprJoinCondition =
			CUtils::PexprScalarConstBool(mp, true /*value*/);
		pexprFinalJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(
			mp, pexprOtherAgg, pexprOrderedAgg, pexprJoinCondition);
	}
	else
	{
		pexprFinalJoin = pexprOrderedAgg;
	}
	ULONG arity = pexprOrderedAgg->Arity();
	BOOL has_nlj_ontop =
		(COperator::EopLogicalInnerJoin == pexprOrderedAgg->Pop()->Eopid());
	CExpression *pexprTopmostCTE = pexprOrderedAgg;
	while (COperator::EopLogicalGbAgg != pexprTopmostCTE->Pop()->Eopid())
	{
		pexprTopmostCTE = (*pexprTopmostCTE)[0];
	}
	ULONG ulCTEIdStart = CLogicalCTEConsumer::PopConvert(
							 (*(*(*pexprTopmostCTE)[0])[0])[0]->Pop())
							 ->UlCTEId();
	ULONG ulCTEIdEnd = ulCTEIdStart;
	if (has_nlj_ontop)
	{
		ulCTEIdEnd = CLogicalCTEConsumer::PopConvert(
						 (*(*(*(*pexprOrderedAgg)[arity - 2])[0])[0])[0]->Pop())
						 ->UlCTEId();
	}
	CExpression *pexpResult = pexprFinalJoin;
	for (ULONG ul = ulCTEIdEnd; ul > ulCTEIdStart; ul--)
	{
		CExpression *ctenext = GPOS_NEW(mp)
			CExpression(mp, GPOS_NEW(mp) CLogicalCTEAnchor(mp, ul), pexpResult);
		pexpResult = ctenext;
	}
	pexpResult = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CLogicalCTEAnchor(mp, ulCTEIdStart), pexpResult);
	return pexpResult;
}


//---------------------------------------------------------------------------
//	@function:
//		COrderedAggPreprocessor::PexprPreprocess
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CExpression *
COrderedAggPreprocessor::PexprPreprocess(CMemoryPool *mp, CExpression *pexpr)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr != pexpr);

	COperator *pop = pexpr->Pop();
	if (COperator::EopLogicalGbAgg == pop->Eopid() &&
		0 == CLogicalGbAgg::PopConvert(pop)->Pdrgpcr()->Size() &&
		0 < (*pexpr)[1]->DeriveTotalOrderedAggs())
	{
		CExpression *pexprJoin = PexprInputAggPrj2Join(mp, pexpr);

		// recursively process the resulting expression
		CExpression *pexprResult = PexprPreprocess(mp, pexprJoin);
		pexprJoin->Release();

		return pexprResult;
	}

	// recursively process child expressions
	const ULONG arity = pexpr->Arity();
	CExpressionArray *pdrgpexprChildren = GPOS_NEW(mp) CExpressionArray(mp);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = PexprPreprocess(mp, (*pexpr)[ul]);
		pdrgpexprChildren->Append(pexprChild);
	}

	pop->AddRef();
	return GPOS_NEW(mp) CExpression(mp, pop, pdrgpexprChildren);
}

// EOF
