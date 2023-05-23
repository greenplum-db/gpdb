//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecSingleton.h
//
//	@doc:
//		Description of a singleton distribution;
//		Can be used as required or derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecSingleton_H
#define GPOPT_CDistributionSpecSingleton_H

#include "gpos/base.h"
#include "gpos/utils.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CDistributionSpecSingleton
//
//	@doc:
//		Class for representing singleton distribution specification.
//
//---------------------------------------------------------------------------
class CDistributionSpecSingleton : public CDistributionSpec
{
public:
	// type of segment
	enum ESegmentType
	{
		EstCoordinator,
		EstSegment,
		EstSentinel
	};

protected:
	// what type of segment is data distributed on
	ESegmentType m_est;

	// segment type name
	static const CHAR *m_szSegmentType[EstSentinel];

private:
public:
	CDistributionSpecSingleton(const CDistributionSpecSingleton &) = delete;

	// ctor
	explicit CDistributionSpecSingleton(ESegmentType esegtype);

	CDistributionSpecSingleton();

	// distribution type accessor
	EDistributionType
	Edt() const override
	{
		return CDistributionSpec::EdtSingleton;
	}

	// segment type accessor
	ESegmentType
	Est() const
	{
		return m_est;
	}

	// is this a coordinator-only distribution
	BOOL
	FOnCoordinator() const
	{
		return EstCoordinator == m_est;
	}

	// return distribution partitioning type
	EDistributionPartitioningType
	Edpt() const override
	{
		return EdptNonPartitioned;
	}

	// does this distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;

	// hash function for singleton distribution spec
	ULONG
	HashValue() const override
	{
		ULONG ulEdt = (ULONG) Edt();
		BOOL fOnCoordinator = FOnCoordinator();

		return gpos::CombineHashes(gpos::HashValue<ULONG>(&ulEdt),
								   gpos::HashValue<BOOL>(&fOnCoordinator));
	}

	// match function for singleton distribution specs
	BOOL
	Matches(const CDistributionSpec *pds) const override
	{
		return Edt() == pds->Edt() &&
			   FOnCoordinator() ==
				   dynamic_cast<const CDistributionSpecSingleton *>(pds)
					   ->FOnCoordinator();
	}

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CDistributionSpecSingleton *
	PdssConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtSingleton == pds->Edt() ||
					EdtStrictSingleton == pds->Edt());

		return dynamic_cast<CDistributionSpecSingleton *>(pds);
	}

	// conversion function
	static const CDistributionSpecSingleton *
	PdssConvert(const CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtSingleton == pds->Edt() ||
					EdtStrictSingleton == pds->Edt());

		return dynamic_cast<const CDistributionSpecSingleton *>(pds);
	}

};	// class CDistributionSpecSingleton

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecSingleton_H

// EOF
