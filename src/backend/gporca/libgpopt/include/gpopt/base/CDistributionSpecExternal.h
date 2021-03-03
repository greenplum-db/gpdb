//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecExternal.h
//
//	@doc:
//		DistributionSpec of an external table;
//		It's similar to Random distribution, however, external table require a separate
//		distribution spec to add motion properly.
//
//		Can only be used as derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecExternal_H
#define GPOPT_CDistributionSpecExternal_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecExternal
//
//	@doc:
//		Class for representing external table distribution.
//
//---------------------------------------------------------------------------
class CDistributionSpecExternal : public CDistributionSpec
{
protected:
	// private copy ctor
	CDistributionSpecExternal(const CDistributionSpecExternal &);

public:
	//ctor
	CDistributionSpecExternal();

	// accessor
	EDistributionType
	Edt() const override
	{
		return CDistributionSpec::EdtExternal;
	}

	virtual const CHAR *
	SzId() const
	{
		return "EXTERNAL";
	}

	// does this distribution match the given one
	BOOL Matches(const CDistributionSpec *pds) const override;

	// does current distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;


	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *,		   //mp,
						 CExpressionHandle &,  // exprhdl
						 CReqdPropPlan *,	   //prpp,
						 CExpressionArray *,   // pdrgpexpr,
						 CExpression *		   // pexpr
						 ) override;

	// return distribution partitioning type
	EDistributionPartitioningType Edpt() const override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CDistributionSpecExternal *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtExternal == pds->Edt());

		return dynamic_cast<CDistributionSpecExternal *>(pds);
	}

	// conversion function: const argument
	static const CDistributionSpecExternal *
	PdsConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtExternal == pds->Edt());

		return dynamic_cast<const CDistributionSpecExternal *>(pds);
	}

};	// class CDistributionSpecExternal

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecExternal_H

// EOF
