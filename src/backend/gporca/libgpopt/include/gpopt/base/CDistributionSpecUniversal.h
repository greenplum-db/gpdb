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
	// private copy ctor
	CDistributionSpecUniversal(const CDistributionSpecUniversal &);

public:
	//ctor
	CDistributionSpecUniversal();

	// accessor
	virtual EDistributionType Edt() const;

	// does current distribution satisfy the given one
	virtual BOOL FSatisfies(const CDistributionSpec *pds) const;

	// return true if distribution spec can be required
	virtual BOOL FRequirable() const;

	// does this distribution match the given one
	virtual BOOL Matches(const CDistributionSpec *pds) const;

	// append enforcers to dynamic array for the given plan properties
	virtual void AppendEnforcers(CMemoryPool *,		   //mp,
								 CExpressionHandle &,  // exprhdl
								 CReqdPropPlan *,	   //prpp,
								 CExpressionArray *,   // pdrgpexpr,
								 CExpression *		   // pexpr
	);

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

	// return distribution partitioning type
	virtual EDistributionPartitioningType Edpt() const;

	// conversion function
	static CDistributionSpecUniversal *PdsConvert(CDistributionSpec *pds);

};	// class CDistributionSpecUniversal

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecUniversal_H

// EOF
