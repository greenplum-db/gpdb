//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#ifndef GPOPT_CDistributionSpecStrictRandom_H
#define GPOPT_CDistributionSpecStrictRandom_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecRandom.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecStrictRandom
//
//	@doc:
//		Class for representing forced random distribution.
//
//---------------------------------------------------------------------------
class CDistributionSpecStrictRandom : public CDistributionSpecRandom
{
public:
	//ctor
	CDistributionSpecStrictRandom();

	// accessor
	EDistributionType
	Edt() const override
	{
		return CDistributionSpec::EdtStrictRandom;
	}

	const CHAR *
	SzId() const override
	{
		return "STRICT RANDOM";
	}

	// does this distribution match the given one
	BOOL Matches(const CDistributionSpec *pds) const override;

	// does this distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;
};	// class CDistributionSpecStrictRandom
}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecStrictRandom_H

// EOF
