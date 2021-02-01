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
	void CreateHomogeneousBtreeIndexApplyAlternatives(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *pexprScalar,
		CExpression *origJoinPred, CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet,
		CTableDescriptor *ptabdescInner, CLogicalDynamicGet *popDynamicGet,
		CColRefSet *pcrsScalarExpr, CColRefSet *outer_refs,
		CColRefSet *pcrsReqd, ULONG ulIndices, CXformResult *pxfres) const;

	// helper to add IndexApply expression to given xform results container
	// for homogeneous b-tree indexes
	void CreateAlternativesForBtreeIndex(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *origJoinPred,
		CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet, CMDAccessor *md_accessor,
		CExpressionArray *pdrgpexprConjuncts, CColRefSet *pcrsScalarExpr,
		CColRefSet *outer_refs, CColRefSet *pcrsReqd, const IMDRelation *pmdrel,
		const IMDIndex *pmdindex, CPartConstraint *ppartcnstrIndex,
		CXformResult *pxfres) const;

	// helper to add IndexApply expression to given xform results container
	// for homogeneous bitmap indexes
	void CreateHomogeneousBitmapIndexApplyAlternatives(
		CMemoryPool *mp, COperator *joinOp, CExpression *pexprOuter,
		CExpression *pexprInner, CExpression *pexprScalar,
		CExpression *origJoinPred, CExpression *nodesToInsertAboveIndexGet,
		CExpression *endOfNodesToInsertAboveIndexGet,
		CTableDescriptor *ptabdescInner, CColRefSet *outer_refs,
		CColRefSet *pcrsReqd, CXformResult *pxfres) const;

	// based on the inner and the scalar expression, it computes scalar expression
	// columns, outer references and required columns
	void ComputeColumnSets(CMemoryPool *mp, CExpression *pexprInner,
						   CExpression *pexprScalar,
						   CColRefSet **ppcrsScalarExpr,
						   CColRefSet **ppcrsOuterRefs,
						   CColRefSet **ppcrsReqd) const;

	// create a union-all with the given children
	CExpression *PexprConstructUnionAll(CMemoryPool *mp,
										CColRefArray *pdrgpcrLeftSchema,
										CColRefArray *pdrgpcrRightSchema,
										CExpression *pexprLeftChild,
										CExpression *pexprRightChild,
										ULONG scan_id) const;

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
		CTableDescriptor *PtabdescInner, CLogicalDynamicGet *popDynamicGet,
		CXformResult *pxfres, gpmd::IMDIndex::EmdindexType emdtype) const;

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
