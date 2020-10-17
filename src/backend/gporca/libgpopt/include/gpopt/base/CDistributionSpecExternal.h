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
	virtual EDistributionType
	Edt() const
	{
		return CDistributionSpec::EdtExternal;
	}

	virtual const CHAR *
	SzId() const
	{
		return "EXTERNAL";
	}

	// does this distribution match the given one
	virtual BOOL Matches(const CDistributionSpec *pds) const;

	// does current distribution satisfy the given one
	virtual BOOL FSatisfies(const CDistributionSpec *pds) const;


	// append enforcers to dynamic array for the given plan properties
	virtual void AppendEnforcers(CMemoryPool *,		   //mp,
								 CExpressionHandle &,  // exprhdl
								 CReqdPropPlan *,	   //prpp,
								 CExpressionArray *,   // pdrgpexpr,
								 CExpression *		   // pexpr
	);

	// return distribution partitioning type
	virtual EDistributionPartitioningType Edpt() const;

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

	// conversion function
	static CDistributionSpecExternal *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(NULL != pds);
		GPOS_ASSERT(EdtExternal == pds->Edt());

		return dynamic_cast<CDistributionSpecExternal *>(pds);
	}

	// conversion function: const argument
	static const CDistributionSpecExternal *
	PdsConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(NULL != pds);
		GPOS_ASSERT(EdtExternal == pds->Edt());

		return dynamic_cast<const CDistributionSpecExternal *>(pds);
	}

};	// class CDistributionSpecExternal

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecExternal_H

// EOF
