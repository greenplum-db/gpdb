//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDistributionSpecNonReplicated.h
//
//	@doc:
//		Description of a distribution that allows no duplicates;
//		Can be used only as a required property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecNonReplicated_H
#define GPOPT_CDistributionSpecNonReplicated_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecSingleton.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecNonReplicated
//
//	@doc:
//		Class for representing distribution that allows no duplicates
//---------------------------------------------------------------------------
class CDistributionSpecNonReplicated : public CDistributionSpecSingleton
{
private:
public:
	//ctor
	CDistributionSpecNonReplicated() = default;

	// accessor
	EDistributionType
	Edt() const override
	{
		return CDistributionSpec::EdtNonReplicated;
	}

	// does current distribution satisfy the given one
	BOOL
	FSatisfies(const CDistributionSpec *pds GPOS_UNUSED) const override
	{
		GPOS_ASSERT(!"Non-Replicated distribution cannot be derived");

		return false;
	}

	// return true if distribution spec can be derived
	BOOL
	FDerivable() const override
	{
		return false;
	}

	// print
	IOstream &
	OsPrint(IOstream &os) const override
	{
		return os << "NON-REPLICATED";
	}

	// return distribution partitioning type
	EDistributionPartitioningType
	Edpt() const override
	{
		return EdptUnknown;
	}

};	// class CDistributionSpecNonReplicated

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecNonReplicated_H

// EOF
