//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CDistributionSpecNonSingleton.cpp
//
//	@doc:
//		Specification of non-singleton distribution
//---------------------------------------------------------------------------

#include "gpopt/base/CDistributionSpecNonSingleton.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDistributionSpecStrictRandom.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecNonSingleton::CDistributionSpecNonSingleton
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDistributionSpecNonSingleton::CDistributionSpecNonSingleton()
	: m_fAllowReplicated(true), m_fAllowEnforced(true)
{
}


//---------------------------------------------------------------------------
//     @function:
//             CDistributionSpecNonSingleton::CDistributionSpecNonSingleton
//
//     @doc:
//             Ctor
//
//---------------------------------------------------------------------------
CDistributionSpecNonSingleton::CDistributionSpecNonSingleton(
	BOOL fAllowReplicated, BOOL fAllowEnforced)
	: m_fAllowReplicated(fAllowReplicated), m_fAllowEnforced(fAllowEnforced)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecNonSingleton::FSatisfies
//
//	@doc:
//		Check if this distribution spec satisfies the given one
//
//---------------------------------------------------------------------------
BOOL
CDistributionSpecNonSingleton::FSatisfies(const CDistributionSpec *	 // pds
) const
{
	GPOS_ASSERT(!"Non-Singleton distribution cannot be derived");

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecNonSingleton::AppendEnforcers
//
//	@doc:
//		Add required enforcers to dynamic array;
//		non-singleton distribution is enforced by spraying data randomly
//		on segments
//
//---------------------------------------------------------------------------
void
CDistributionSpecNonSingleton::AppendEnforcers(CMemoryPool *mp,
											   CExpressionHandle &exprhdl,
											   CReqdPropPlan *
#ifdef GPOS_DEBUG
												   prpp
#endif	// GPOS_DEBUG
											   ,
											   CExpressionArray *pdrgpexpr,
											   CExpression *pexpr)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != prpp);
	GPOS_ASSERT(NULL != pdrgpexpr);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(!GPOS_FTRACE(EopttraceDisableMotions));
	GPOS_ASSERT(
		this == prpp->Ped()->PdsRequired() &&
		"required plan properties don't match enforced distribution spec");


	if (GPOS_FTRACE(EopttraceDisableMotionRandom))
	{
		// random Motion is disabled
		return;
	}

	CDistributionSpec *expr_dist_spec =
		CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds();
	CDistributionSpecRandom *random_dist_spec = NULL;

	// random motions on top of universal specs are converted to hash filters,
	// and shouldn't be strict random distributions or we may not properly distribute tuples.
	// See comment in CDistributionSpecRandom::AppendEnforcers for details
	if (CUtils::FDuplicateHazardDistributionSpec(expr_dist_spec))
	{
		// the motion node is enforced on top of a child
		// deriving universal spec or replicated distribution, this motion node
		// will be translated to a result node with hash filter to remove
		// duplicates
		random_dist_spec = GPOS_NEW(mp) CDistributionSpecRandom();
	}
	else
	{
		// the motion added in this enforcer will translate to
		// a redistribute motion
		random_dist_spec = GPOS_NEW(mp) CDistributionSpecStrictRandom();
	}


	pexpr->AddRef();
	CExpression *pexprMotion = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CPhysicalMotionRandom(mp, random_dist_spec), pexpr);
	pdrgpexpr->Append(pexprMotion);
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecNonSingleton::OsPrint
//
//	@doc:
//		Print function
//
//---------------------------------------------------------------------------
IOstream &
CDistributionSpecNonSingleton::OsPrint(IOstream &os) const
{
	os << "NON-SINGLETON ";
	if (!m_fAllowReplicated)
	{
		os << " (NON-REPLICATED)";
	}
	return os;
}

// EOF
