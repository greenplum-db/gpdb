//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	Transform Inner/Outer Join to Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoin2IndexApply_H
#define GPOPT_CXformJoin2IndexApply_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformUtils.h"

namespace gpopt
{
using namespace gpos;

// fwd declaration
class CLogicalDynamicGet;

class CXformJoin2IndexApply : public CXformExploration
{
private:
	// helper to add IndexApply expression to given xform results container
	// for homogeneous b-tree indexes
	static void CreateHomogeneousBtreeIndexApplyAlternatives(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *pexprScalar,
		CExpression *origJoinPred, CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet,
		CTableDescriptor *ptabdescInner, CColRefSet *pcrsScalarExpr,
		CColRefSet *outer_refs, ULONG ulIndices, CXformResult *pxfres);

	// helper to add IndexApply expression to given xform results container
	// for homogeneous b-tree indexes
	static void CreateAlternativesForBtreeIndex(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *origJoinPred,
		CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet, CMDAccessor *md_accessor,
		CExpressionArray *pdrgpexprConjuncts, CColRefSet *pcrsScalarExpr,
		CColRefSet *outer_refs, const IMDRelation *pmdrel,
		const IMDIndex *pmdindex, CXformResult *pxfres);

	// helper to add IndexApply expression to given xform results container
	// for homogeneous bitmap indexes
	static void CreateHomogeneousBitmapIndexApplyAlternatives(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *pexprScalar,
		CExpression *origJoinPred, CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet,
		CTableDescriptor *ptabdescInner, CColRefSet *outer_refs,
		CColRefSet *pcrsReqd, CXformResult *pxfres);

	// based on the inner and the scalar expression, it computes scalar expression
	// columns, outer references and required columns
	static void ComputeColumnSets(CMemoryPool *mp, CExpression *pexprInner,
								  CExpression *pexprScalar,
								  CColRefSet **ppcrsScalarExpr,
								  CColRefSet **ppcrsOuterRefs,
								  CColRefSet **ppcrsReqd);

protected:
	// is the logical join that is being transformed an outer join?
	BOOL m_fOuterJoin;

	// helper to add IndexApply expression to given xform results container
	// for homogeneous indexes
	virtual void CreateHomogeneousIndexApplyAlternatives(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *pexprScalar,
		CExpression *origJoinPred, CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet,
		CTableDescriptor *PtabdescInner, CXformResult *pxfres,
		gpmd::IMDIndex::EmdindexType emdtype) const;

public:
	CXformJoin2IndexApply(const CXformJoin2IndexApply &) = delete;

	// ctor
	explicit CXformJoin2IndexApply(CExpression *pexprPattern)
		: CXformExploration(pexprPattern)
	{
		m_fOuterJoin = (COperator::EopLogicalLeftOuterJoin ==
						pexprPattern->Pop()->Eopid());
	}

	// dtor
	~CXformJoin2IndexApply() override = default;

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

};	// class CXformJoin2IndexApply

}  // namespace gpopt

#endif	// !GPOPT_CXformJoin2IndexApply_H

// EOF
