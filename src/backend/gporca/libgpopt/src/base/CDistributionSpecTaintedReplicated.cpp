//	Greenplum Database
//	Copyright (C) 2020 VMware Inc.

#include "gpopt/base/CDistributionSpecTaintedReplicated.h"

#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecSingleton.h"

namespace gpopt
{

BOOL
CDistributionSpecTaintedReplicated::FSatisfies(const CDistributionSpec *pds) const
{
	switch (pds->Edt())
	{
		default:
			return false;
		case CDistributionSpec::EdtAny:
			return true;
		case CDistributionSpec::EdtGeneralReplicated:
			return true;
		case CDistributionSpec::EdtNonSingleton:
			return CDistributionSpecNonSingleton::PdsConvert(pds)->FAllowReplicated();
		case CDistributionSpec::EdtSingleton:
			return CDistributionSpecSingleton::PdssConvert(pds)->Est() == CDistributionSpecSingleton::EstSegment;
	}
}

void
CDistributionSpecTaintedReplicated::AppendEnforcers(gpos::CMemoryPool *,
													CExpressionHandle &,
													CReqdPropPlan *,
													CExpressionArray *,
													CExpression *)
{
	GPOS_ASSERT(!"CDistributionSpecTaintedReplicated is derive-only, cannot be required");
}

} // namespace gpopt
