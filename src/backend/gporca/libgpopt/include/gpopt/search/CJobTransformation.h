//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJobTransformation.h
//
//	@doc:
//		Transform group expression job
//---------------------------------------------------------------------------
#ifndef GPOPT_CJobTransformation_H
#define GPOPT_CJobTransformation_H

#include "gpopt/search/CJob.h"
#include "gpopt/search/CJobStateMachine.h"


namespace gpopt
{
using namespace gpos;

// prototypes
class CGroup;
class CGroupExpression;
class CXform;

//---------------------------------------------------------------------------
//	@class:
//		CJobTransformation
//
//	@doc:
//		Runs the given transformation (XForm) rule.
//
//---------------------------------------------------------------------------
class CJobTransformation : public CJob
{
public:
	// transition events of a transformation
	enum EEvent
	{
		eevCompleted,

		eevSentinel
	};

	// states of a transformation
	enum EState
	{
		estInitialized = 0,
		estCompleted,

		estSentinel
	};

private:
	// shorthand for job state machine
	using JSM = CJobStateMachine<EState, estSentinel, EEvent, eevSentinel>;

	// target group expression
	CGroupExpression *m_pgexpr;

	// xform to apply to group expression
	CXform *m_xform;

	// job state machine
	JSM m_jsm;

	// apply transformation action
	static EEvent EevtTransform(CSchedulerContext *psc, CJob *pj);

public:
	CJobTransformation(const CJobTransformation &) = delete;

	// ctor
	CJobTransformation();

	// dtor
	~CJobTransformation() override;

	// initialize job
	void Init(CGroupExpression *pgexpr, CXform *pxform);

	// schedule a new transformation job
	static void ScheduleJob(CSchedulerContext *psc, CGroupExpression *pgexpr,
							CXform *pxform, CJob *pjParent);

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
	static CJobTransformation *
	PjConvert(CJob *pj)
	{
		GPOS_ASSERT(nullptr != pj);
		GPOS_ASSERT(EjtTransformation == pj->Ejt());

		return dynamic_cast<CJobTransformation *>(pj);
	}

};	// class CJobTransformation

}  // namespace gpopt

#endif	// !GPOPT_CJobTransformation_H


// EOF
