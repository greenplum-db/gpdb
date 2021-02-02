//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CEnfdDistribution.h
//
//	@doc:
//		Enforceable distribution property
//---------------------------------------------------------------------------
#ifndef GPOPT_CEnfdDistribution_H
#define GPOPT_CEnfdDistribution_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CEnfdProp.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CEnfdDistribution
//
//	@doc:
//		Enforceable distribution property;
//
//---------------------------------------------------------------------------
class CEnfdDistribution : public CEnfdProp
{
public:
	// type of distribution matching function
	enum EDistributionMatching
	{
		EdmExact = 0,
		EdmSatisfy,
		EdmSubset,

		EdmSentinel
	};

private:
	// required distribution
	CDistributionSpec *m_pds;

	// distribution matching type
	EDistributionMatching m_edm;

	// names of distribution matching types
	static const CHAR *m_szDistributionMatching[EdmSentinel];

public:
	CEnfdDistribution(const CEnfdDistribution &) = delete;

	// ctor
	CEnfdDistribution(CDistributionSpec *pds, EDistributionMatching edm);

	// dtor
	~CEnfdDistribution() override;

	// distribution spec accessor
	CPropSpec *
	Pps() const override
	{
		return m_pds;
	}

	// matching type accessor
	EDistributionMatching
	Edm() const
	{
		return m_edm;
	}

	// matching function
	BOOL
	Matches(CEnfdDistribution *ped)
	{
		GPOS_ASSERT(nullptr != ped);

		return m_edm == ped->Edm() && m_pds->Equals(ped->PdsRequired());
	}

	// hash function
	ULONG HashValue() const override;

	// check if the given distribution specification is compatible with the
	// distribution specification of this object for the specified matching type
	BOOL FCompatible(CDistributionSpec *pds) const;

	// required distribution accessor
	CDistributionSpec *
	PdsRequired() const
	{
		return m_pds;
	}

	// get distribution enforcing type for the given operator
	EPropEnforcingType Epet(CExpressionHandle &exprhdl, CPhysical *popPhysical,
							BOOL fDistribReqd) const;

	// print function
	IOstream &OsPrint(IOstream &os) const override;

};	// class CEnfdDistribution

}  // namespace gpopt


#endif	// !GPOPT_CEnfdDistribution_H

// EOF
