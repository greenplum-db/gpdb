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

class CPartitionPropagationSpec;

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

	// private copy ctor
	CEnfdDistribution(const CEnfdDistribution &);

	// names of distribution matching types
	static const CHAR *m_szDistributionMatching[EdmSentinel];

public:
	// ctor
	CEnfdDistribution(CDistributionSpec *pds, EDistributionMatching edm);

	// dtor
	virtual ~CEnfdDistribution();

	// distribution spec accessor
	virtual CPropSpec *
	Pps() const
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
		GPOS_ASSERT(NULL != ped);

		return m_edm == ped->Edm() && m_pds->Equals(ped->PdsRequired());
	}

	// hash function
	virtual ULONG HashValue() const;

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
							CPartitionPropagationSpec *pppsReqd,
							BOOL fDistribReqd) const;

	// print function
	virtual IOstream &OsPrint(IOstream &os) const;

};	// class CEnfdDistribution

}  // namespace gpopt


#endif	// !GPOPT_CEnfdDistribution_H

// EOF
