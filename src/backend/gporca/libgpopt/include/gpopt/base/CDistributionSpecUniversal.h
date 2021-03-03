//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDistributionSpecUniversal.h
//
//	@doc:
//		Description of a general distribution which reports availability everywhere;
//		Can be used only as a derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecUniversal_H
#define GPOPT_CDistributionSpecUniversal_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecHashed.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecUniversal
//
//	@doc:
//		Class for representing general distribution specification which
//		reports availability everywhere.
//
//---------------------------------------------------------------------------
class CDistributionSpecUniversal : public CDistributionSpec
{
private:
public:
	CDistributionSpecUniversal(const CDistributionSpecUniversal &) = delete;

	//ctor
	CDistributionSpecUniversal();

	// accessor
	EDistributionType Edt() const override;

	// does current distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;

	// return true if distribution spec can be required
	BOOL FRequirable() const override;

	// does this distribution match the given one
	BOOL Matches(const CDistributionSpec *pds) const override;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *,		   //mp,
						 CExpressionHandle &,  // exprhdl
						 CReqdPropPlan *,	   //prpp,
						 CExpressionArray *,   // pdrgpexpr,
						 CExpression *		   // pexpr
						 ) override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// return distribution partitioning type
	EDistributionPartitioningType Edpt() const override;

	// conversion function
	static CDistributionSpecUniversal *PdsConvert(CDistributionSpec *pds);

};	// class CDistributionSpecUniversal

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecUniversal_H

// EOF
