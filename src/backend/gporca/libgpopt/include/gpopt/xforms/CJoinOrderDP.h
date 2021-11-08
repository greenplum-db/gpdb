//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CJoinOrderDP.h
//
//	@doc:
//		Dynamic programming-based join order generation
//---------------------------------------------------------------------------
#ifndef GPOPT_CJoinOrderDP_H
#define GPOPT_CJoinOrderDP_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/common/CHashMap.h"
#include "gpos/common/DbgPrintMixin.h"
#include "gpos/io/IOstream.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/xforms/CJoinOrder.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CJoinOrderDP
//
//	@doc:
//		Helper class for creating join orders using dynamic programming
//
//---------------------------------------------------------------------------
class CJoinOrderDP : public CJoinOrder, public gpos::DbgPrintMixin<CJoinOrderDP>
{
private:
	//---------------------------------------------------------------------------
	//	@struct:
	//		SComponentPair
	//
	//	@doc:
	//		Struct to capture a pair of components
	//
	//---------------------------------------------------------------------------
	struct SComponentPair : public CRefCount
	{
		// first component
		CBitSet *m_pbsFst;

		// second component
		CBitSet *m_pbsSnd;

		// ctor
		SComponentPair(CBitSet *pbsFst, CBitSet *pbsSnd);

		// dtor
		~SComponentPair() override;

		// hashing function
		static ULONG HashValue(const SComponentPair *pcomppair);

		// equality function
		static BOOL Equals(const SComponentPair *pcomppairFst,
						   const SComponentPair *pcomppairSnd);
	};

	// hashing function
	static ULONG
	UlHashBitSet(const CBitSet *pbs)
	{
		GPOS_ASSERT(nullptr != pbs);

		return pbs->HashValue();
	}

	// equality function
	static BOOL
	FEqualBitSet(const CBitSet *pbsFst, const CBitSet *pbsSnd)
	{
		GPOS_ASSERT(nullptr != pbsFst);
		GPOS_ASSERT(nullptr != pbsSnd);

		return pbsFst->Equals(pbsSnd);
	}

	// hash map from component to best join order
	using BitSetToExpressionMap =
		CHashMap<CBitSet, CExpression, UlHashBitSet, FEqualBitSet,
				 CleanupRelease<CBitSet>, CleanupRelease<CExpression>>;

	// hash map from component pair to connecting edges
	using ComponentPairToExpressionMap =
		CHashMap<SComponentPair, CExpression, SComponentPair::HashValue,
				 SComponentPair::Equals, CleanupRelease<SComponentPair>,
				 CleanupRelease<CExpression>>;

	// hash map from expression to cost of best join order
	using ExpressionToCostMap =
		CHashMap<CExpression, CDouble, CExpression::HashValue, CUtils::Equals,
				 CleanupRelease<CExpression>, CleanupDelete<CDouble>>;

	// lookup table for links
	ComponentPairToExpressionMap *m_phmcomplink;

	// dynamic programming table
	BitSetToExpressionMap *m_phmbsexpr;

	// map of expressions to its cost
	ExpressionToCostMap *m_phmexprcost;

	// array of top-k join expression
	CExpressionArray *m_pdrgpexprTopKOrders;

	// dummy expression to used for non-joinable components
	CExpression *m_pexprDummy;

	// build expression linking given components
	CExpression *PexprBuildPred(CBitSet *pbsFst, CBitSet *pbsSnd);

	// lookup best join order for given set
	CExpression *PexprLookup(CBitSet *pbs);

	// extract predicate joining the two given sets
	CExpression *PexprPred(CBitSet *pbsFst, CBitSet *pbsSnd);

	// join expressions in the given two sets
	CExpression *PexprJoin(CBitSet *pbsFst, CBitSet *pbsSnd);

	// join expressions in the given set
	CExpression *PexprJoin(CBitSet *pbs);

	// find best join order for given component using dynamic programming
	CExpression *PexprBestJoinOrderDP(CBitSet *pbs);

	// find best join order for given component
	CExpression *PexprBestJoinOrder(CBitSet *pbs);

	// generate cross product for the given components
	CExpression *PexprCross(CBitSet *pbs);

	// join a covered subset with uncovered subset
	CExpression *PexprJoinCoveredSubsetWithUncoveredSubset(
		CBitSet *pbs, CBitSet *pbsCovered, CBitSet *pbsUncovered);

	// return a subset of the given set covered by one or more edges
	CBitSet *PbsCovered(CBitSet *pbsInput);

	// add given join order to best results
	void AddJoinOrder(CExpression *pexprJoin, CDouble dCost);

	// compute cost of given join expression
	CDouble DCost(CExpression *pexpr);

	// derive stats on given expression
	void DeriveStats(CExpression *pexpr) override;

	// add expression to cost map
	void InsertExpressionCost(CExpression *pexpr, CDouble dCost,
							  BOOL fValidateInsert);

	// generate all subsets of the given array of elements
	static void GenerateSubsets(CMemoryPool *mp, CBitSet *pbsCurrent,
								ULONG *pulElems, ULONG size, ULONG ulIndex,
								CBitSetArray *pdrgpbsSubsets);

	// driver of subset generation
	static CBitSetArray *PdrgpbsSubsets(CMemoryPool *mp, CBitSet *pbs);

public:
	// ctor
	CJoinOrderDP(CMemoryPool *mp, CExpressionArray *pdrgpexprComponents,
				 CExpressionArray *pdrgpexprConjuncts);

	// dtor
	~CJoinOrderDP() override;

	// main handler
	virtual CExpression *PexprExpand();

	// best join orders
	CExpressionArray *
	PdrgpexprTopK() const
	{
		return m_pdrgpexprTopKOrders;
	}

	// print function
	IOstream &OsPrint(IOstream &) const;

	CXform::EXformId
	EOriginXForm() const override
	{
		return CXform::ExfExpandNAryJoinDP;
	}

};	// class CJoinOrderDP

}  // namespace gpopt

#endif	// !GPOPT_CJoinOrderDP_H

// EOF
