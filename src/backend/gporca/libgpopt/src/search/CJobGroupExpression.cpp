//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CJobGroupExpression.cpp
//
//	@doc:
//		Implementation of group expression job superclass
//---------------------------------------------------------------------------

#include "gpopt/search/CJobGroupExpression.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/search/CGroupExpression.h"
#include "gpopt/search/CJobFactory.h"
#include "gpopt/search/CJobGroupExpressionExploration.h"
#include "gpopt/search/CJobGroupExpressionImplementation.h"
#include "gpopt/search/CScheduler.h"
#include "gpopt/search/CSchedulerContext.h"
#include "gpopt/xforms/CXformFactory.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CJobGroupExpression::Init
//
//	@doc:
//		Initialize job
//
//---------------------------------------------------------------------------
void
CJobGroupExpression::Init(CGroupExpression *pgexpr)
{
	GPOS_ASSERT(!FInit());
	GPOS_ASSERT(NULL != pgexpr);
	GPOS_ASSERT(NULL != pgexpr->Pgroup());
	GPOS_ASSERT(NULL != pgexpr->Pop());

	m_fChildrenScheduled = false;
	m_fXformsScheduled = false;
	m_pgexpr = pgexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CJobGroupExpression::ScheduleTransformations
//
//	@doc:
//		Schedule transformation jobs for the given set of xforms
//
//---------------------------------------------------------------------------
void
CJobGroupExpression::ScheduleTransformations(CSchedulerContext *psc,
											 CXformSet *xform_set)
{
	// iterate on xforms
	CXformSetIter xsi(*(xform_set));
	while (xsi.Advance())
	{
		CXform *pxform = CXformFactory::Pxff()->Pxf(xsi.TBit());
		CJobTransformation::ScheduleJob(psc, m_pgexpr, pxform, this);
	}
}


// EOF
