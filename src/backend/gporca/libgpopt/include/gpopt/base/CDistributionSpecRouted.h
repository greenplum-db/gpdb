//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDistributionSpecRouted.h
//
//	@doc:
//		Description of a routed distribution;
//		Can be used as required or derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecRouted_H
#define GPOPT_CDistributionSpecRouted_H

#include "gpos/base.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecRouted
//
//	@doc:
//		Class for representing routed distribution specification.
//
//---------------------------------------------------------------------------
class CDistributionSpecRouted : public CDistributionSpec
{
private:
	// segment id column
	CColRef *m_pcrSegmentId;

	// private copy ctor
	CDistributionSpecRouted(const CDistributionSpecRouted &);

public:
	// ctor
	explicit CDistributionSpecRouted(CColRef *pcrSegmentId);

	// dtor
	virtual ~CDistributionSpecRouted();

	// distribution type accessor
	virtual EDistributionType
	Edt() const
	{
		return CDistributionSpec::EdtRouted;
	}

	// segment id column accessor
	CColRef *
	Pcr() const
	{
		return m_pcrSegmentId;
	}

	// does this distribution satisfy the given one
	virtual BOOL Matches(const CDistributionSpec *pds) const;

	// does this distribution satisfy the given one
	virtual BOOL FSatisfies(const CDistributionSpec *pds) const;

	// return a copy of the distribution spec with remapped columns
	virtual CDistributionSpec *PdsCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

	// append enforcers to dynamic array for the given plan properties
	virtual void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
								 CReqdPropPlan *prpp,
								 CExpressionArray *pdrgpexpr,
								 CExpression *pexpr);

	// hash function for routed distribution spec
	virtual ULONG HashValue() const;

	// extract columns used by the distribution spec
	virtual CColRefSet *PcrsUsed(CMemoryPool *mp) const;

	// return distribution partitioning type
	virtual EDistributionPartitioningType
	Edpt() const
	{
		return EdptPartitioned;
	}

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

	// conversion function
	static CDistributionSpecRouted *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(NULL != pds);
		GPOS_ASSERT(EdtRouted == pds->Edt());

		return dynamic_cast<CDistributionSpecRouted *>(pds);
	}

	// conversion function - const argument
	static const CDistributionSpecRouted *
	PdsConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(NULL != pds);
		GPOS_ASSERT(EdtRouted == pds->Edt());

		return dynamic_cast<const CDistributionSpecRouted *>(pds);
	}

};	// class CDistributionSpecRouted

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecRouted_H

// EOF
