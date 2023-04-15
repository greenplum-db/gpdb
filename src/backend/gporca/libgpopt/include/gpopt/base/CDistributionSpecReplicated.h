//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecReplicated.h
//
//	@doc:
//		Description of a replicated distribution;
//		Can be used as required or derived property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecReplicated_H
#define GPOPT_CDistributionSpecReplicated_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{
using namespace gpos;

class CDistributionSpecReplicated : public CDistributionSpec
{
private:
	// replicated support
	CDistributionSpec::EDistributionType m_replicated;

	BOOL m_ignore_broadcast_threshold;

public:
	CDistributionSpecReplicated(const CDistributionSpecReplicated &) = delete;

	// ctor
	CDistributionSpecReplicated(
		CDistributionSpec::EDistributionType replicated_type)
		: m_replicated(replicated_type), m_ignore_broadcast_threshold(false)
	{
		GPOS_ASSERT(replicated_type == CDistributionSpec::EdtReplicated ||
					replicated_type ==
						CDistributionSpec::EdtTaintedReplicated ||
					replicated_type == CDistributionSpec::EdtStrictReplicated);
	}

	// ctor
	CDistributionSpecReplicated(
		CDistributionSpec::EDistributionType replicated_type,
		BOOL ignore_broadcast_threshold)
		: m_replicated(replicated_type),
		  m_ignore_broadcast_threshold(ignore_broadcast_threshold)
	{
		GPOS_ASSERT(replicated_type == CDistributionSpec::EdtReplicated ||
					replicated_type ==
						CDistributionSpec::EdtTaintedReplicated ||
					replicated_type == CDistributionSpec::EdtStrictReplicated);
	}

	// accessor
	EDistributionType
	Edt() const override
	{
		return m_replicated;
	}

	// does this distribution satisfy the given one
	BOOL FSatisfies(const CDistributionSpec *pds) const override;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// return distribution partitioning type
	EDistributionPartitioningType
	Edpt() const override
	{
		return EdptNonPartitioned;
	}

	// print
	IOstream &
	OsPrint(IOstream &os) const override
	{
		switch (Edt())
		{
			case CDistributionSpec::EdtReplicated:
				os << "REPLICATED";
				break;
			case CDistributionSpec::EdtTaintedReplicated:
				os << "TAINTED REPLICATED";
				break;
			case CDistributionSpec::EdtStrictReplicated:
				os << "STRICT REPLICATED";
				break;
			default:
				GPOS_ASSERT(
					!"Replicated type must be General, Tainted, or Strict");
		}
		return os;
	}

	// conversion function
	static CDistributionSpecReplicated *
	PdsConvert(CDistributionSpec *pds)
	{
		GPOS_ASSERT(nullptr != pds);
		GPOS_ASSERT(EdtStrictReplicated == pds->Edt() ||
					EdtReplicated == pds->Edt() ||
					EdtTaintedReplicated == pds->Edt());

		return dynamic_cast<CDistributionSpecReplicated *>(pds);
	}

	BOOL
	FIgnoreBroadcastThreshold() const
	{
		return m_ignore_broadcast_threshold;
	}

};	// class CDistributionSpecReplicated

}  // namespace gpopt

#endif	// !GPOPT_CDistributionSpecReplicated_H

// EOF
