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

public:
	CDistributionSpecRouted(const CDistributionSpecRouted &) = delete;

	// ctor
	explicit CDistributionSpecRouted(CColRef *pcrSegmentId);

	// dtor
	~CDistributionSpecRouted() override;

	// distribution type accessor
	EDistributionType
	Edt() const override
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
	BOOL Matches(const CDistributionSpec *pds) const override;

	// does this distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;

	// return a copy of the distribution spec with remapped columns
	CDistributionSpec *PdsCopyWithRemappedColumns(
		CMemoryPool *mp, UlongToColRefMap *colref_mapping,
		BOOL must_exist) override;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// hash function for routed distribution spec
	ULONG HashValue() const override;

	// extract columns used by the distribution spec
	CColRefSet *PcrsUsed(CMemoryPool *mp) const override;

	// return distribution partitioning type
	EDistributionPartitioningType
	Edpt() const override
	{
		return EdptPartitioned;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CDistributionSpecRouted *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtRouted == pds->Edt());

		return dynamic_cast<CDistributionSpecRouted *>(pds);
	}

	// conversion function - const argument
	static const CDistributionSpecRouted *
	PdsConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtRouted == pds->Edt());

		return dynamic_cast<const CDistributionSpecRouted *>(pds);
	}

};	// class CDistributionSpecRouted

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecRouted_H

// EOF
