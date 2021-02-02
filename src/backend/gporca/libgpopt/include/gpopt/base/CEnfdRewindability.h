//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CEnfdRewindability.h
//
//	@doc:
//		Enforceable rewindability property
//---------------------------------------------------------------------------
#ifndef GPOPT_CEnfdRewindability_H
#define GPOPT_CEnfdRewindability_H

#include "gpos/base.h"

#include "gpopt/base/CEnfdProp.h"
#include "gpopt/base/CRewindabilitySpec.h"


namespace gpopt
{
using namespace gpos;

// prototypes
class CPhysical;


//---------------------------------------------------------------------------
//	@class:
//		CEnfdRewindability
//
//	@doc:
//		Enforceable rewindability property;
//
//---------------------------------------------------------------------------
class CEnfdRewindability : public CEnfdProp
{
public:
	// type of rewindability matching function
	enum ERewindabilityMatching
	{
		ErmSatisfy = 0,

		ErmSentinel
	};

private:
	// required rewindability
	CRewindabilitySpec *m_prs;

	// rewindability matching type
	ERewindabilityMatching m_erm;

	// names of rewindability matching types
	static const CHAR *m_szRewindabilityMatching[ErmSentinel];

public:
	CEnfdRewindability(const CEnfdRewindability &) = delete;

	// ctor
	CEnfdRewindability(CRewindabilitySpec *prs, ERewindabilityMatching erm);

	// dtor
	~CEnfdRewindability() override;

	// hash function
	ULONG HashValue() const override;

	// check if the given rewindability specification is compatible with the
	// rewindability specification of this object for the specified matching type
	BOOL FCompatible(CRewindabilitySpec *pos) const;

	// required rewindability accessor
	CRewindabilitySpec *
	PrsRequired() const
	{
		return m_prs;
	}

	// get rewindability enforcing type for the given operator
	EPropEnforcingType Epet(CExpressionHandle &exprhdl, CPhysical *popPhysical,
							BOOL fRewindabilityReqd) const;

	// property spec accessor
	CPropSpec *
	Pps() const override
	{
		return m_prs;
	}

	// matching type accessor
	ERewindabilityMatching
	Erm() const
	{
		return m_erm;
	}

	// matching function
	BOOL
	Matches(CEnfdRewindability *per)
	{
		GPOS_ASSERT(nullptr != per);

		return m_erm == per->Erm() && m_prs->Matches(per->PrsRequired());
	}


	// print function
	IOstream &OsPrint(IOstream &os) const override;

};	// class CEnfdRewindability

}  // namespace gpopt


#endif	// !GPOPT_CEnfdRewindability_H

// EOF
