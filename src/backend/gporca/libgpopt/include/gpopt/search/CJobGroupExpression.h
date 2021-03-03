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

protected:
	// target group expression
	CGroupExpression *m_pgexpr{nullptr};

	// ctor
	CJobGroupExpression() = default;

	// dtor
	~CJobGroupExpression() override = default;

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
	BOOL FExecute(CSchedulerContext *psc) override = 0;

#ifdef GPOS_DEBUG

	// print function
	IOstream &OsPrint(IOstream &os) const override = 0;

#endif	// GPOS_DEBUG
public:
	CJobGroupExpression(const CJobGroupExpression &) = delete;

};	// class CJobGroupExpression

}  // namespace gpopt

#endif	// !GPOPT_CJobGroupExpression_H


// EOF
