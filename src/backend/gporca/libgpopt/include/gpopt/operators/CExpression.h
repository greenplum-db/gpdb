//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2011 EMC CORP.
//
//	@filename:
//		CExpression.h
//
//	@doc:
//		Basic tree/DAG-based representation for an expression
//---------------------------------------------------------------------------
#ifndef GPOPT_CExpression_H
#define GPOPT_CExpression_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/DbgPrintMixin.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CCostContext.h"
#include "gpopt/base/CDrvdPropRelational.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/base/CReqdProp.h"
#include "gpopt/base/CReqdPropRelational.h"
#include "gpopt/cost/CCost.h"
#include "gpopt/operators/COperator.h"
#include "naucrates/statistics/IStatistics.h"

namespace gpopt
{
// cleanup function for arrays
class CExpression;
using CExpressionArray = CDynamicPtrArray<CExpression, CleanupRelease>;

// array of arrays of expression pointers
using CExpressionArrays = CDynamicPtrArray<CExpressionArray, CleanupRelease>;

class CGroupExpression;
class CDrvdPropPlan;
class CDrvdPropCtxt;
class CDrvdPropCtxtPlan;
class CPropConstraint;

using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CExpression
//
//	@doc:
//		Simply dynamic array for pointer types
//
//---------------------------------------------------------------------------
class CExpression : public CRefCount, public gpos::DbgPrintMixin<CExpression>
{
	friend class CExpressionHandle;

private:
	// memory pool
	CMemoryPool *m_mp;

	// operator class
	COperator *m_pop;

	// array of children
	CExpressionArray *m_pdrgpexpr;

	// derived relational properties
	CDrvdPropRelational *m_pdprel;

	// derived stats
	IStatistics *m_pstats;

	// required plan properties
	CReqdPropPlan *m_prpp;

	// derived physical properties
	CDrvdPropPlan *m_pdpplan;

	// derived scalar properties
	CDrvdPropScalar *m_pdpscalar;

	// group reference to Memo
	CGroupExpression *m_pgexpr;

	// cost of physical expression node when copied out of the memo
	CCost m_cost;

	// id of origin group, used for debugging expressions extracted from memo
	ULONG m_ulOriginGrpId;

	// id of origin group expression, used for debugging expressions extracted from memo
	ULONG m_ulOriginGrpExprId;

	// get expression's derived property given its type
	CDrvdProp *Pdp(const CDrvdProp::EPropType ept) const;

#ifdef GPOS_DEBUG

	// assert valid property derivation
	void AssertValidPropDerivation(const CDrvdProp::EPropType ept);

	// print expression properties
	void PrintProperties(IOstream &os, CPrintPrefix &pfx) const;

#endif	// GPOS_DEBUG

	// check if the distributions of all children are compatible
	BOOL FValidChildrenDistribution(CDrvdPropCtxtPlan *pdpctxtplan);

	// copy group properties and stats to expression
	void CopyGroupPropsAndStats(IStatistics *input_stats);

public:
	CExpression(const CExpression &) = delete;

	// ctor's with different arity

	// ctor for leaf nodes
	CExpression(CMemoryPool *mp, COperator *pop,
				CGroupExpression *pgexpr = nullptr);

	// ctor for unary expressions
	CExpression(CMemoryPool *mp, COperator *pop, CExpression *pexpr);

	// ctor for binary expressions
	CExpression(CMemoryPool *mp, COperator *pop, CExpression *pexprChildFirst,
				CExpression *pexprChildSecond);

	// ctor for ternary expressions
	CExpression(CMemoryPool *mp, COperator *pop, CExpression *pexprChildFirst,
				CExpression *pexprChildSecond, CExpression *pexprChildThird);

	// ctor n-ary expressions
	CExpression(CMemoryPool *mp, COperator *pop, CExpressionArray *pdrgpexpr);

	// ctor for n-ary expression with origin group expression
	CExpression(CMemoryPool *mp, COperator *pop, CGroupExpression *pgexpr,
				CExpressionArray *pdrgpexpr, CReqdPropPlan *prpp,
				IStatistics *input_stats, CCost cost = GPOPT_INVALID_COST);

	// dtor
	~CExpression() override;

	// shorthand to access children
	CExpression *
	operator[](ULONG ulPos) const
	{
		GPOS_ASSERT(nullptr != m_pdrgpexpr);
		return (*m_pdrgpexpr)[ulPos];
	};

	// arity function
	ULONG
	Arity() const
	{
		return m_pdrgpexpr == nullptr ? 0 : m_pdrgpexpr->Size();
	}

	// accessor for operator
	COperator *
	Pop() const
	{
		GPOS_ASSERT(nullptr != m_pop);
		return m_pop;
	}

	// accessor of children array
	CExpressionArray *
	PdrgPexpr() const
	{
		return m_pdrgpexpr;
	}

	// accessor for origin group expression
	CGroupExpression *
	Pgexpr() const
	{
		return m_pgexpr;
	}

	// accessor for computed required plan props
	CReqdPropPlan *
	Prpp() const
	{
		return m_prpp;
	}

	CDrvdPropRelational *GetDrvdPropRelational() const;

	CDrvdPropPlan *GetDrvdPropPlan() const;

	CDrvdPropScalar *GetDrvdPropScalar() const;

	// get derived statistics object
	const IStatistics *
	Pstats() const
	{
		return m_pstats;
	}

	// cost accessor
	CCost
	Cost() const
	{
		return m_cost;
	}

	// get the suitable derived property type based on operator
	CDrvdProp::EPropType Ept() const;

	// Derive all properties immediately. The suitable derived property is
	// determined internally. To derive properties on an on-demand bases, use
	// DeriveXXX() methods.
	CDrvdProp *PdpDerive(CDrvdPropCtxt *pdpctxt = nullptr);

	// derive statistics
	IStatistics *PstatsDerive(CReqdPropRelational *prprel,
							  IStatisticsArray *stats_ctxt);

	// reset a derived property
	void ResetDerivedProperty(CDrvdProp::EPropType ept);

	// reset all derived properties
	void ResetDerivedProperties();

	// reset expression stats
	void ResetStats();

	// check for outer references
	BOOL HasOuterRefs();

	// print driver
	IOstream &OsPrint(IOstream &os) const;

	// print driver, customized for expressions
	IOstream &OsPrintExpression(IOstream &os, const CPrintPrefix * = nullptr,
								BOOL fLast = true) const;

	// match with group expression
	BOOL FMatchPattern(CGroupExpression *pgexpr) const;

	// return a copy of the expression with remapped columns
	CExpression *PexprCopyWithRemappedColumns(CMemoryPool *mp,
											  UlongToColRefMap *colref_mapping,
											  BOOL must_exist) const;

	// compare entire expression rooted here
	BOOL Matches(CExpression *pexpr) const;

#ifdef GPOS_DEBUG
	// match against given pattern
	BOOL FMatchPattern(CExpression *pexpr) const;

	// match children against given pattern
	BOOL FMatchPatternChildren(CExpression *pexpr) const;

	// compare entire expression rooted here
	BOOL FMatchDebug(CExpression *pexpr) const;

	// debug print; for interactive debugging sessions only
	// prints expression properties as well
	void DbgPrintWithProperties() const;
#endif	// GPOS_DEBUG

	// check if the expression satisfies given required properties
	BOOL FValidPlan(const CReqdPropPlan *prpp, CDrvdPropCtxtPlan *pdpctxtplan);

	// static hash function
	static ULONG HashValue(const CExpression *pexpr);

	// static hash function
	static ULONG UlHashDedup(const CExpression *pexpr);

	// rehydrate expression from a given cost context and child expressions
	static CExpression *PexprRehydrate(CMemoryPool *mp, CCostContext *pcc,
									   CExpressionArray *pdrgpexpr,
									   CDrvdPropCtxtPlan *pdpctxtplan);

	// Relational property accessors - derived as needed
	CColRefSet *DeriveOuterReferences();
	CColRefSet *DeriveOutputColumns();
	CColRefSet *DeriveNotNullColumns();
	CColRefSet *DeriveCorrelatedApplyColumns();
	CKeyCollection *DeriveKeyCollection();
	CPropConstraint *DerivePropertyConstraint();
	CMaxCard DeriveMaxCard();
	ULONG DeriveJoinDepth();
	CFunctionProp *DeriveFunctionProperties();
	CFunctionalDependencyArray *DeriveFunctionalDependencies();
	CPartInfo *DerivePartitionInfo();
	CTableDescriptor *DeriveTableDescriptor();

	// Scalar property accessors - derived as needed
	CColRefSet *DeriveDefinedColumns();
	CColRefSet *DeriveUsedColumns();
	CColRefSet *DeriveSetReturningFunctionColumns();
	BOOL DeriveHasSubquery();
	CPartInfo *DeriveScalarPartitionInfo();
	CFunctionProp *DeriveScalarFunctionProperties();
	BOOL DeriveHasNonScalarFunction();
	ULONG DeriveTotalDistinctAggs();
	BOOL DeriveHasMultipleDistinctAggs();
	BOOL DeriveHasScalarArrayCmp();
	BOOL DeriveHasScalarFuncProject();
	BOOL DeriveContainsOnlyReplicationSafeAggFuncs();
	ULONG DeriveTotalOrderedAggs();

};	// class CExpression


// shorthand for printing
inline IOstream &
operator<<(IOstream &os, CExpression &expr)
{
	return expr.OsPrint(os);
}

// hash map from ULONG to expression
using UlongToExprMap =
	CHashMap<ULONG, CExpression, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
			 CleanupDelete<ULONG>, CleanupRelease<CExpression>>;

// map iterator
using UlongToExprMapIter =
	CHashMapIter<ULONG, CExpression, gpos::HashValue<ULONG>,
				 gpos::Equals<ULONG>, CleanupDelete<ULONG>,
				 CleanupRelease<CExpression>>;

}  // namespace gpopt


#endif	// !GPOPT_CExpression_H

// EOF
