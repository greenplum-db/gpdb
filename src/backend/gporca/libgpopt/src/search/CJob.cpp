//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CJob.cpp
//
//	@doc:
//		Implementation of optimizer job base class
//---------------------------------------------------------------------------

#include "gpopt/search/CJob.h"

#include "gpos/base.h"

#include "gpopt/search/CJobQueue.h"
#include "gpopt/search/CScheduler.h"

using namespace gpopt;
using namespace gpos;

FORCE_GENERATE_DBGSTR(CJob);

//---------------------------------------------------------------------------
//	@function:
//		CJob::Reset
//
//	@doc:
//		Reset job
//
//---------------------------------------------------------------------------
void
CJob::Reset()
{
	m_pjParent = nullptr;
	m_pjq = nullptr;
	m_ulpRefs = 0;
	m_fInit = false;
#ifdef GPOS_DEBUG
	m_ejs = EjsInit;
#endif	// GPOS_DEBUG
}


//---------------------------------------------------------------------------
//	@function:
//		CJob::FResumeParent
//
//	@doc:
//		Resume parent jobs after job completion
//
//---------------------------------------------------------------------------
BOOL
CJob::FResumeParent() const
{
	GPOS_ASSERT(0 == UlpRefs());
	GPOS_ASSERT(nullptr != m_pjParent);
	GPOS_ASSERT(0 < m_pjParent->UlpRefs());

	// decrement parent's ref counter
	ULONG_PTR ulpRefs = m_pjParent->UlpDecrRefs();

	// check if job should be resumed
	return (1 == ulpRefs);
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CJob::OsPrint
//
//	@doc:
//		Print job description
//
//---------------------------------------------------------------------------
IOstream &
CJob::OsPrint(IOstream &os) const
{
	os << "ID=" << Id();

	if (nullptr != PjParent())
	{
		os << " parent=" << PjParent()->Id() << std::endl;
	}
	else
	{
		os << " ROOT" << std::endl;
	}
	return os;
}

#endif	// GPOS_DEBUG

// EOF
