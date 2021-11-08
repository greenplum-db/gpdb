//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2011 EMC Corp.
//
//	@filename:
//		CGroupExpression.h
//
//	@doc:
//		Equivalent of CExpression inside Memo structure
//---------------------------------------------------------------------------
#ifndef GPOPT_CGroupExpression_H
#define GPOPT_CGroupExpression_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CCostContext.h"
#include "gpopt/engine/CPartialPlan.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/xforms/CXform.h"

#define GPOPT_INVALID_GEXPR_ID gpos::ulong_max

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CGroupExpression
//
//	@doc:
//		Expression representation inside Memo structure
//
//---------------------------------------------------------------------------
class CGroupExpression : public CRefCount,
						 public DbgPrintMixin<CGroupExpression>
{
public:
#ifdef GPOS_DEBUG
	// debug print; for interactive debugging sessions only
	void DbgPrintWithProperties() const;
#endif	// GPOS_DEBUG

	// states of a group expression
	enum EState
	{
		estUnexplored,	// initial state

		estExploring,  // ongoing exploration
		estExplored,   // done exploring

		estImplementing,  // ongoing implementation
		estImplemented,	  // done implementing

		estSentinel
	};

	// circular dependency state
	enum ECircularDependency
	{
		ecdDefault,				// default state
		ecdCircularDependency,	// contains circular dependency

		ecdSentinel
	};

	// type definition of cost context hash table
	using ShtCC = CSyncHashtable<CCostContext, OPTCTXT_PTR>;

private:
	// definition of context hash table accessor
	using ShtAcc = CSyncHashtableAccessByKey<CCostContext, OPTCTXT_PTR>;

	// definition of context hash table iter
	using ShtIter = CSyncHashtableIter<CCostContext, OPTCTXT_PTR>;

	// definition of context hash table iter accessor
	using ShtAccIter = CSyncHashtableAccessByIter<CCostContext, OPTCTXT_PTR>;

	// map of partial plans to their costs
	using PartialPlanToCostMap =
		CHashMap<CPartialPlan, CCost, CPartialPlan::HashValue,
				 CPartialPlan::Equals, CleanupRelease<CPartialPlan>,
				 CleanupDelete<CCost>>;


	// expression id
	ULONG m_id{GPOPT_INVALID_GEXPR_ID};

	// duplicate group expression
	CGroupExpression *m_pgexprDuplicate;

	// operator class
	COperator *m_pop{nullptr};

	// array of child groups
	CGroupArray *m_pdrgpgroup{nullptr};

	// sorted array of children groups for faster comparison
	// of order-insensitive operators
	CGroupArray *m_pdrgpgroupSorted{nullptr};

	// back pointer to group
	CGroup *m_pgroup{nullptr};

	// id of xform that generated group expression
	CXform::EXformId m_exfidOrigin{CXform::ExfInvalid};

	// group expression that generated current group expression via xform
	CGroupExpression *m_pgexprOrigin{nullptr};

	// flag to indicate if group expression was created as a node at some
	// intermediate level when origin expression was inserted to memo
	BOOL m_fIntermediate{false};

	// state of group expression
	EState m_estate{estUnexplored};

	// optimization level
	EOptimizationLevel m_eol{EolLow};

	// map of partial plans to their cost lower bound
	PartialPlanToCostMap *m_ppartialplancostmap{nullptr};

	// circular dependency state
	ECircularDependency m_ecirculardependency;

	// hashtable of cost contexts
	ShtCC m_sht;

	// set group back pointer
	void SetGroup(CGroup *pgroup);

	// set group expression id
	void SetId(ULONG id);

	// print transformation
	static void PrintXform(CMemoryPool *mp, CXform *pxform, CExpression *pexpr,
						   CXformResult *pxfres, ULONG ulNumResults);

	// preprocessing before applying transformation
	void PreprocessTransform(CMemoryPool *pmpLocal, CMemoryPool *pmpGlobal,
							 CXform *pxform);

	// postprocessing after applying transformation
	void PostprocessTransform(CMemoryPool *pmpLocal, CMemoryPool *pmpGlobal,
							  CXform *pxform) const;

	// costing scheme
	static CCost CostCompute(CMemoryPool *mp, CCostContext *pcc);

	// set optimization level of group expression
	void SetOptimizationLevel();

	// check validity of group expression
	BOOL FValidContext(CMemoryPool *mp, COptimizationContext *poc,
					   COptimizationContextArray *pdrgpocChild);

	// remove cost context in hash table
	CCostContext *PccRemove(COptimizationContext *poc, ULONG ulOptReq);

	// insert given context in hash table only if a better context does not exist, return the context that is kept it in hash table
	CCostContext *PccInsertBest(CCostContext *pcc);

	// print group expression cost contexts
	IOstream &OsPrintCostContexts(IOstream &os, const CHAR *szPrefix) const;

	//private dummy ctor; used for creating invalid gexpr
	CGroupExpression() = default;


public:
	CGroupExpression(const CGroupExpression &) = delete;

	// ctor
	CGroupExpression(CMemoryPool *mp, COperator *pop, CGroupArray *pdrgpgroup,
					 CXform::EXformId exfid, CGroupExpression *pgexprOrigin,
					 BOOL fIntermediate);

	// dtor
	~CGroupExpression() override;

	// duplicate group expression accessor
	CGroupExpression *
	PgexprDuplicate() const
	{
		return m_pgexprDuplicate;
	}

	// set duplicate group expression
	void
	SetDuplicate(CGroupExpression *pgexpr)
	{
		GPOS_ASSERT(nullptr != pgexpr);

		m_pgexprDuplicate = pgexpr;
	}

	// cleanup cost contexts
	void CleanupContexts();

	// check if cost context already exists in group expression hash table
	BOOL FCostContextExists(COptimizationContext *poc,
							COptimizationContextArray *pdrgpoc);

	// compute and store expression's cost under a given context
	CCostContext *PccComputeCost(CMemoryPool *mp, COptimizationContext *poc,
								 ULONG ulOptReq,
								 COptimizationContextArray *pdrgpoc,
								 BOOL fPruned, CCost costLowerBound);

	// compute a cost lower bound for plans, rooted by current group expression, and satisfying the given required properties
	CCost CostLowerBound(CMemoryPool *mp, CReqdPropPlan *prppInput,
						 CCostContext *pccChild, ULONG child_index);

	// initialize group expression
	void Init(CGroup *pgroup, ULONG id);

	// reset group expression
	void
	Reset(CGroup *pgroup, ULONG id)
	{
		m_pgroup = pgroup;
		m_id = id;
	}

	// optimization level accessor
	EOptimizationLevel
	Eol() const
	{
		return m_eol;
	}

	// shorthand to access children
	CGroup *
	operator[](ULONG ulPos) const
	{
		GPOS_ASSERT(nullptr != m_pdrgpgroup);

		CGroup *pgroup = (*m_pdrgpgroup)[ulPos];

		// during optimization, the operator returns the duplicate group;
		// in exploration and implementation the group may contain
		// group expressions that have not been processed yet;
		if (0 == pgroup->UlGExprs())
		{
			GPOS_ASSERT(pgroup->FDuplicateGroup());
			return pgroup->PgroupDuplicate();
		}
		return pgroup;
	};

	// arity function
	ULONG
	Arity() const
	{
		return m_pdrgpgroup->Size();
	}

	// accessor for operator
	COperator *
	Pop() const
	{
		return m_pop;
	}

	// accessor for id
	ULONG
	Id() const
	{
		return m_id;
	}

	// accessor for containing group
	CGroup *
	Pgroup() const
	{
		return m_pgroup;
	}

	// origin xform
	CXform::EXformId
	ExfidOrigin() const
	{
		return m_exfidOrigin;
	}

	// origin group expression
	CGroupExpression *
	PgexprOrigin() const
	{
		return m_pgexprOrigin;
	}

	// cost contexts hash table accessor
	ShtCC &
	Sht()
	{
		return m_sht;
	}

	// comparison operator for hashtables
	BOOL
	operator==(const CGroupExpression &gexpr) const
	{
		return gexpr.Matches(this);
	}

	// equality function for hash table
	static BOOL
	Equals(const CGroupExpression &gexprLeft,
		   const CGroupExpression &gexprRight)
	{
		return gexprLeft == gexprRight;
	}

	// match group expression against given operator and its children
	BOOL Matches(const CGroupExpression *) const;

	// match non-scalar children of group expression against given children of passed expression
	BOOL FMatchNonScalarChildren(const CGroupExpression *pgexpr) const;

	// hash function
	ULONG
	HashValue() const
	{
		return HashValue(m_pop, m_pdrgpgroup);
	}

	// static hash function for operator and group references
	static ULONG HashValue(COperator *pop, CGroupArray *drgpgroup);

	// static hash function for group expression
	static ULONG HashValue(const CGroupExpression &);

	// transform group expression
	void Transform(CMemoryPool *mp, CMemoryPool *pmpLocal, CXform *pxform,
				   CXformResult *pxfres, ULONG *pulElapsedTime,
				   ULONG *pulNumberOfBindings);

	// set group expression state
	void SetState(EState estNewState);

	// reset group expression state
	void ResetState();

	// check if group expression has been explored
	BOOL
	FExplored() const
	{
		return (estExplored <= m_estate);
	}

	// check if group expression has been implemented
	BOOL
	FImplemented() const
	{
		return (estImplemented == m_estate);
	}

	// check if transition to the given state is completed
	BOOL FTransitioned(EState estate) const;

	CGroupArray *
	Pdrgpgroup() const
	{
		return m_pdrgpgroup;
	}

	// lookup cost context in hash table
	CCostContext *PccLookup(COptimizationContext *poc, ULONG ulOptReq);

	// lookup all cost contexts matching given optimization context
	CCostContextArray *PdrgpccLookupAll(CMemoryPool *mp,
										COptimizationContext *poc);

	// insert a cost context in hash table
	CCostContext *PccInsert(CCostContext *pcc);

	// derive statistics recursively on a given group expression
	IStatistics *PstatsRecursiveDerive(CMemoryPool *pmpLocal,
									   CMemoryPool *pmpGlobal,
									   CReqdPropRelational *prprel,
									   IStatisticsArray *stats_ctxt,
									   BOOL fComputeRootStats = true);

	// print driver
	IOstream &OsPrint(IOstream &os) const;
	IOstream &OsPrintWithPrefix(IOstream &os, const CHAR *prefix) const;

	// link for list in Group
	SLink m_linkGroup;

	// link for group expression hash table
	SLink m_linkMemo;

	// invalid group expression
	static const CGroupExpression m_gexprInvalid;

	virtual BOOL ContainsCircularDependencies();
};	// class CGroupExpression

}  // namespace gpopt


#endif	// !GPOPT_CGroupExpression_H

// EOF
