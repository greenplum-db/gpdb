//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJobGroupExpression.h
//
//	@doc:
//		Superclass of group expression jobs
//---------------------------------------------------------------------------
#ifndef GPOPT_CJobGroupExpression_H
#define GPOPT_CJobGroupExpression_H

#include "gpos/base.h"

#include "gpopt/search/CJob.h"
#include "gpopt/xforms/CXform.h"


namespace gpopt
{
using namespace gpos;

// prototypes
class CGroup;
class CGroupExpression;

//---------------------------------------------------------------------------
//	@class:
//		CJobGroupExpression
//
//	@doc:
//		Abstract superclass of all group expression optimization jobs
//
//---------------------------------------------------------------------------
class CJobGroupExpression : public CJob
{
private:
	// true if job has scheduled child group jobs
	BOOL m_fChildrenScheduled;

	// true if job has scheduled transformation jobs
	BOOL m_fXformsScheduled;

	// private copy ctor
	CJobGroupExpression(const CJobGroupExpression &);

protected:
	// target group expression
	CGroupExpression *m_pgexpr;

	// ctor
	CJobGroupExpression() : m_pgexpr(NULL)
	{
	}

	// dtor
	virtual ~CJobGroupExpression()
	{
	}

	// has job scheduled child groups ?
	BOOL
	FChildrenScheduled() const
	{
		return m_fChildrenScheduled;
	}

	// set children scheduled
	void
	SetChildrenScheduled()
	{
		m_fChildrenScheduled = true;
	}

	// has job scheduled xform groups ?
	BOOL
	FXformsScheduled() const
	{
		return m_fXformsScheduled;
	}

	// set xforms scheduled
	void
	SetXformsScheduled()
	{
		m_fXformsScheduled = true;
	}

	// initialize job
	void Init(CGroupExpression *pgexpr);

	// schedule transformation jobs for applicable xforms
	virtual void ScheduleApplicableTransformations(CSchedulerContext *psc) = 0;

	// schedule jobs for all child groups
	virtual void ScheduleChildGroupsJobs(CSchedulerContext *psc) = 0;

	// schedule transformation jobs for the given set of xforms
	void ScheduleTransformations(CSchedulerContext *psc, CXformSet *xform_set);

	// job's function
	virtual BOOL FExecute(CSchedulerContext *psc) = 0;

#ifdef GPOS_DEBUG

	// print function
	virtual IOstream &OsPrint(IOstream &os) = 0;

#endif	// GPOS_DEBUG

};	// class CJobGroupExpression

}  // namespace gpopt

#endif	// !GPOPT_CJobGroupExpression_H


// EOF
