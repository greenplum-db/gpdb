//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/base/CDistributionSpecStrictHashed.h"

namespace gpopt
{
CDistributionSpecStrictHashed::CDistributionSpecStrictHashed(
	CExpressionArray *pdrgpexpr, BOOL fNullsColocated)
	: CDistributionSpecHashed(pdrgpexpr, fNullsColocated)
{
}

CDistributionSpec::EDistributionType
CDistributionSpecStrictHashed::Edt() const
{
	return CDistributionSpec::EdtStrictHashed;
}

}  // namespace gpopt
