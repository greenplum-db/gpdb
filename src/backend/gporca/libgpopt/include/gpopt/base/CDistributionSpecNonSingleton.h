//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CDistributionSpecNonSingleton.h
//
//	@doc:
//		Description of a general distribution which imposes no singleton
//		distribution requirements;
//		Can be used only as a required property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecNonSingleton_H
#define GPOPT_CDistributionSpecNonSingleton_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecNonSingleton
//
//	@doc:
//		Class for representing general distribution specification which
//		imposes no requirements.
//
//---------------------------------------------------------------------------
class CDistributionSpecNonSingleton : public CDistributionSpec
{
private:
	// should Replicated distribution satisfy current distribution
	BOOL m_fAllowReplicated{true};

	// should allow this non-singleton spec to be enforced?
	BOOL m_fAllowEnforced{true};

public:
	CDistributionSpecNonSingleton(const CDistributionSpecNonSingleton &) =
		delete;

	//ctor
	CDistributionSpecNonSingleton();

	//ctor
	explicit CDistributionSpecNonSingleton(BOOL fAllowReplicated,
										   BOOL fAllowEnforced);

	// should Replicated distribution satisfy current distribution
	BOOL
	FAllowReplicated() const
	{
		return m_fAllowReplicated;
	}

	// should allow this non-singleton spec to be enforced?
	BOOL
	FAllowEnforced() const
	{
		return m_fAllowEnforced;
	}

	// accessor
	EDistributionType
	Edt() const override
	{
		return CDistributionSpec::EdtNonSingleton;
	}

	// does current distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// return distribution partitioning type
	EDistributionPartitioningType
	Edpt() const override
	{
		// a non-singleton distribution could be replicated to all segments, or partitioned across segments
		return EdptUnknown;
	}

	// return true if distribution spec can be derived
	BOOL
	FDerivable() const override
	{
		return false;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CDistributionSpecNonSingleton *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtNonSingleton == pds->Edt());

		return dynamic_cast<CDistributionSpecNonSingleton *>(pds);
	}

	// conversion function
	static const CDistributionSpecNonSingleton *
	PdsConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtNonSingleton == pds->Edt());

		return dynamic_cast<const CDistributionSpecNonSingleton *>(pds);
	}

};	// class CDistributionSpecNonSingleton

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecNonSingleton_H

// EOF
