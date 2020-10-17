//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDistributionSpecExternal.cpp
//
//	@doc:
//		Specification of external distribution, where data stored externally
//---------------------------------------------------------------------------

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CDistributionSpecExternal.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecExternal::CDistributionSpecExternal
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDistributionSpecExternal::CDistributionSpecExternal()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecExternal::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CDistributionSpecExternal::Matches(const CDistributionSpec *pds) const
{
	return (Edt() == pds->Edt());
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecExternal::FSatisfies
//
//	@doc:
//		Check if this distribution spec satisfies the given one
//
//---------------------------------------------------------------------------
BOOL
CDistributionSpecExternal::FSatisfies(const CDistributionSpec *pds) const
{
	if (Matches(pds))
	{
		return true;
	}

	return EdtAny == pds->Edt();
}

void
CDistributionSpecExternal::AppendEnforcers(CMemoryPool *, CExpressionHandle &,
										   CReqdPropPlan *, CExpressionArray *,
										   CExpression *)
{
	GPOS_ASSERT(!"EXTERNAL distribution cannot be enforced, it's derive only.");
}

CDistributionSpec::EDistributionPartitioningType
CDistributionSpecExternal::Edpt() const
{
	return EdptPartitioned;
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecExternal::OsPrint
//
//	@doc:
//		Print function
//
//---------------------------------------------------------------------------
IOstream &
CDistributionSpecExternal::OsPrint(IOstream &os) const
{
	return os << this->SzId();
}

// EOF
