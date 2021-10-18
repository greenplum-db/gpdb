//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJobGroupExpressionExploration.h
//
//	@doc:
//		Explore group expression job
//---------------------------------------------------------------------------
#ifndef GPOPT_CJobGroupExpressionExploration_H
#define GPOPT_CJobGroupExpressionExploration_H

#include "gpopt/search/CJobGroupExpression.h"
#include "gpopt/search/CJobStateMachine.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CJobGroupExpressionExploration
//
//	@doc:
//		Explore group expression optimization job
//
//		Responsible for creating the logical rewrites of a given group
//		expression. Note that a group exploration job entails running a group
//		expression exploration job for each group expression in the underlying
//		group.
//
//---------------------------------------------------------------------------
class CJobGroupExpressionExploration : public CJobGroupExpression
{
public:
	// transition events of group expression exploration
	enum EEvent
	{
		eevExploringChildren,  // child groups exploration is in progress
		eevChildrenExplored,   // done with children exploration
		eevExploringSelf,	   // self exploration is in progress
		eevSelfExplored,	   // done with exploring group expression
		eevFinalized,		   // done with exploration

		eevSentinel
	};

	// states of group expression exploration
	enum EState
	{
		estInitialized = 0,	  // initial state
		estChildrenExplored,  // child groups explored
		estSelfExplored,	  // group expression explored
		estCompleted,		  // done exploration

		estSentinel
	};

private:
	// shorthand for job state machine
	using JSM = CJobStateMachine<EState, estSentinel, EEvent, eevSentinel>;

	// job state machine
	JSM m_jsm;

	// explore child groups action
	static EEvent EevtExploreChildren(CSchedulerContext *psc, CJob *pj);

	// explore group expression action
	static EEvent EevtExploreSelf(CSchedulerContext *psc, CJob *pj);

	// finalize action
	static EEvent EevtFinalize(CSchedulerContext *psc, CJob *pj);

protected:
	// schedule transformation jobs for applicable xforms
	void ScheduleApplicableTransformations(CSchedulerContext *psc) override;

	// schedule exploration jobs for all child groups
	void ScheduleChildGroupsJobs(CSchedulerContext *psc) override;

public:
	CJobGroupExpressionExploration(const CJobGroupExpressionExploration &) =
		delete;

	// ctor
	CJobGroupExpressionExploration();

	// dtor
	~CJobGroupExpressionExploration() override;

	// initialize job
	void Init(CGroupExpression *pgexpr);

	// schedule a new group expression exploration job
	static void ScheduleJob(CSchedulerContext *psc, CGroupExpression *pgexpr,
							CJob *pjParent);

	// job's main function
	BOOL FExecute(CSchedulerContext *psc) override;

#ifdef GPOS_DEBUG

	// print function
	IOstream &OsPrint(IOstream &os) const override;

	// dump state machine diagram in graphviz format
	virtual IOstream &
	OsDiagramToGraphviz(CMemoryPool *mp, IOstream &os,
						const WCHAR *wszTitle) const
	{
		(void) m_jsm.OsDiagramToGraphviz(mp, os, wszTitle);

		return os;
	}

	// compute unreachable states
	void
	Unreachable(CMemoryPool *mp, EState **ppestate, ULONG *pulSize) const
	{
		m_jsm.Unreachable(mp, ppestate, pulSize);
	}


#endif	// GPOS_DEBUG

	// conversion function
	static CJobGroupExpressionExploration *
	PjConvert(CJob *pj)
	{
		GPOS_ASSERT(nullptr != pj);
		GPOS_ASSERT(EjtGroupExpressionExploration == pj->Ejt());

		return dynamic_cast<CJobGroupExpressionExploration *>(pj);
	}

};	// class CJobGroupExpressionExploration

}  // namespace gpopt

#endif	// !GPOPT_CJobGroupExpressionExploration_H


// EOF
