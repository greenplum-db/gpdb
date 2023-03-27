//	Greenplum Database
//	Copyright (C) 2020 VMware Inc.

#include "gpopt/base/CDistributionSpecReplicated.h"

#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/operators/CPhysicalMotionBroadcast.h"

using namespace gpopt;


//	Check if this distribution spec satisfies the given one
//	based on following satisfiability rules:
//
//	pds = requested distribution
//	this = derived distribution
//
//	Table - Distribution satisfiability matrix:
//	  T - Satisfied
//	  F - Not satisfied; enforcer required
//
//	  Note that when checking for satisfiability for TaintedReplicated,
//	  we don't call the Match() function, since TaintedReplicated is a
//	  derived only property. Hence if FSatisfies is being called on
//	  TaintedReplicated, pds can never be TaintedReplicated.
//	+-------------------------+------------------+-------------------+
//	|                         | StrictReplicated | TaintedReplicated |
//	+-------------------------+------------------+-------------------+
//	| Matches                 | T                | (default) F       |
//	| NonSingleton            | FAllowReplicated | FAllowReplicated  |
//	| Replicated              | T                | T                 |
//	| StrictReplicated        | T                | F                 |
//	| singleton & master      | F                | F                 |
//	| singleton & segment     | T                | T                 |
//	| ANY                     | T                | T                 |
//	| others not Singleton    | T                |(default) F        |
//	+-------------------------+------------------+-------------------+
BOOL
CDistributionSpecReplicated::FSatisfies(const CDistributionSpec *pdss) const
{
	GPOS_ASSERT(Edt() != CDistributionSpec::EdtReplicated);

	if (Edt() == CDistributionSpec::EdtTaintedReplicated)
	{
		// TaintedReplicated::FSatisfies logic is similar to Replicated::FSatisifes
		// except that Replicated can match and satisfy another Replicated Spec.
		// However, Tainted will never satisfy another TaintedReplicated or
		// Replicated.
		switch (pdss->Edt())
		{
			default:
				return false;
			case CDistributionSpec::EdtAny:
				// tainted replicated distribution satisfies an any required distribution spec
				return true;
			case CDistributionSpec::EdtReplicated:
				// tainted replicated distribution satisfies a general replicated distribution spec
				return true;
			case CDistributionSpec::EdtNonSingleton:
				// a tainted replicated distribution satisfies the non-singleton
				// distribution, only if allowed by non-singleton distribution object
				return CDistributionSpecNonSingleton::PdsConvert(pdss)
					->FAllowReplicated();
			case CDistributionSpec::EdtSingleton:
				// a tainted replicated distribution satisfies singleton
				// distributions that are not master-only
				return CDistributionSpecSingleton::PdssConvert(pdss)->Est() ==
					   CDistributionSpecSingleton::EstSegment;
		}
	}
	else if (Edt() == CDistributionSpec::EdtStrictReplicated)
	{
		if (Matches(pdss))
		{
			// exact match implies satisfaction
			return true;
		}

		if (EdtNonSingleton == pdss->Edt())
		{
			// a replicated distribution satisfies the non-singleton
			// distribution, only if allowed by non-singleton distribution object
			return CDistributionSpecNonSingleton::PdsConvert(
					   const_cast<CDistributionSpec *>(pdss))
				->FAllowReplicated();
		}

		// replicated distribution satisfies a general replicated distribution spec
		if (EdtReplicated == pdss->Edt())
		{
			return true;
		}

		if (CDistributionSpec::EdtRandom == pdss->Edt() &&
			(CDistributionSpecRandom::PdsConvert(pdss))->IsDuplicateSensitive())
		{
			return false;
		}

		// a replicated distribution satisfies any non-singleton one,
		// as well as singleton distributions that are not master-only
		return !(EdtSingleton == pdss->Edt() &&
				 (dynamic_cast<const CDistributionSpecSingleton *>(pdss))
					 ->FOnMaster());
	}

	return false;
}

void
CDistributionSpecReplicated::AppendEnforcers(CMemoryPool *mp,
											 CExpressionHandle &,  // exprhdl
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
	GPOS_ASSERT(Edt() != CDistributionSpec::EdtTaintedReplicated);

	if (GPOS_FTRACE(EopttraceDisableMotionBroadcast))
	{
		// broadcast Motion is disabled
		return;
	}

	pexpr->AddRef();
	CExpression *pexprMotion = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp) CPhysicalMotionBroadcast(mp, m_ignore_broadcast_threshold),
		pexpr);
	pdrgpexpr->Append(pexprMotion);
}
// EOF
