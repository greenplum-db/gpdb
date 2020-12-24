//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#include "gpopt/base/CDistributionSpecStrictRandom.h"

using namespace gpopt;

CDistributionSpecStrictRandom::CDistributionSpecStrictRandom() = default;

BOOL
CDistributionSpecStrictRandom::Matches(const CDistributionSpec *pds) const
{
	return pds->Edt() == Edt();
}

BOOL
CDistributionSpecStrictRandom::FSatisfies(const CDistributionSpec *pds) const
{
	return Matches(pds) || EdtAny == pds->Edt() || EdtRandom == pds->Edt() ||
		   EdtNonSingleton == pds->Edt();
}
