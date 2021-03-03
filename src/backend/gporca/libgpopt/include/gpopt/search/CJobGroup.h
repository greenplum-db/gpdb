//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJobGroup.h
//
//	@doc:
//		Superclass of group jobs
//---------------------------------------------------------------------------
#ifndef GPOPT_CJobGroup_H
#define GPOPT_CJobGroup_H

#include "gpos/base.h"

#include "gpopt/search/CJob.h"

namespace gpopt
{
using namespace gpos;

// prototypes
class CGroup;
class CGroupExpression;

//---------------------------------------------------------------------------
//	@class:
//		CJobGroup
//
//	@doc:
//		Abstract superclass of all group optimization jobs
//
//---------------------------------------------------------------------------
class CJobGroup : public CJob
{
private:
protected:
	// target group
	CGroup *m_pgroup{nullptr};

	// last scheduled group expression
	CGroupExpression *m_pgexprLastScheduled;

	// ctor
	CJobGroup() = default;

	// dtor
	~CJobGroup() override = default;

	// initialize job
	void Init(CGroup *pgroup);

	// get first unscheduled logical expression
	virtual CGroupExpression *PgexprFirstUnschedLogical();

	// get first unscheduled non-logical expression
	virtual CGroupExpression *PgexprFirstUnschedNonLogical();

	// get first unscheduled expression
	virtual CGroupExpression *PgexprFirstUnsched() = 0;

	// schedule jobs for of all new group expressions
	virtual BOOL FScheduleGroupExpressions(CSchedulerContext *psc) = 0;

	// job's function
	BOOL FExecute(CSchedulerContext *psc) override = 0;

#ifdef GPOS_DEBUG

	// print function
	IOstream &OsPrint(IOstream &os) const override = 0;

#endif	// GPOS_DEBUG
public:
	CJobGroup(const CJobGroup &) = delete;

};	// class CJobGroup

}  // namespace gpopt

#endif	// !GPOPT_CJobGroup_H


// EOF
