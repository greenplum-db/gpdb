//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CJobGroup.cpp
//
//	@doc:
//		Implementation of group job superclass
//---------------------------------------------------------------------------

#include "gpopt/search/CJobGroup.h"

#include "gpopt/search/CGroupProxy.h"
#include "gpopt/search/CJobFactory.h"
#include "gpopt/search/CJobGroupExpressionExploration.h"
#include "gpopt/search/CJobGroupExpressionImplementation.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CJobGroup::Init
//
//	@doc:
//		Initialize job
//
//---------------------------------------------------------------------------
void
CJobGroup::Init(CGroup *pgroup)
{
	GPOS_ASSERT(!FInit());
	GPOS_ASSERT(nullptr != pgroup);

	m_pgroup = pgroup;
	m_pgexprLastScheduled = nullptr;
}


//---------------------------------------------------------------------------
//	@function:
//		CJobGroup::PgexprFirstUnschedNonLogical
//
//	@doc:
//		Get first non-logical group expression with an unscheduled job
//
//---------------------------------------------------------------------------
CGroupExpression *
CJobGroup::PgexprFirstUnschedNonLogical()
{
	CGroupExpression *pgexpr = nullptr;
	{
		CGroupProxy gp(m_pgroup);
		if (nullptr == m_pgexprLastScheduled)
		{
			// get first group expression
			pgexpr = gp.PgexprSkipLogical(nullptr /*pgexpr*/);
		}
		else
		{
			// get group expression next to last scheduled one
			pgexpr = gp.PgexprSkipLogical(m_pgexprLastScheduled);
		}
	}

	return pgexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CJobGroup::PgexprFirstUnschedLogical
//
//	@doc:
//		Get first logical group expression with an unscheduled job
//
//---------------------------------------------------------------------------
CGroupExpression *
CJobGroup::PgexprFirstUnschedLogical()
{
	CGroupExpression *pgexpr = nullptr;
	{
		CGroupProxy gp(m_pgroup);
		if (nullptr == m_pgexprLastScheduled)
		{
			// get first group expression
			pgexpr = gp.PgexprFirst();
		}
		else
		{
			// get group expression next to last scheduled one
			pgexpr = gp.PgexprNext(m_pgexprLastScheduled);
		}
	}

	return pgexpr;
}

// EOF
