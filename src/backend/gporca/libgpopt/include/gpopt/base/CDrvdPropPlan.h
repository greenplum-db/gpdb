//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDrvdPropPlan.h
//
//	@doc:
//		Derived physical properties
//---------------------------------------------------------------------------
#ifndef GPOPT_CDrvdPropPlan_H
#define GPOPT_CDrvdPropPlan_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CDrvdProp.h"

namespace gpopt
{
using namespace gpos;

// fwd declaration
class CDistributionSpec;
class CExpressionHandle;
class COrderSpec;
class CRewindabilitySpec;
class CReqdPropPlan;
class CCTEMap;

//---------------------------------------------------------------------------
//	@class:
//		CDrvdPropPlan
//
//	@doc:
//		Derived plan properties container.
//
//		These are properties that are expression-specific and they depend on
//		the physical implementation. This includes sort order, distribution,
//		rewindability, partition propagation spec and CTE map.
//
//---------------------------------------------------------------------------
class CDrvdPropPlan : public CDrvdProp
{
private:
	// derived sort order
	COrderSpec *m_pos{nullptr};

	// derived distribution
	CDistributionSpec *m_pds{nullptr};

	// derived rewindability
	CRewindabilitySpec *m_prs{nullptr};

	// derived cte map
	CCTEMap *m_pcm{nullptr};

	// copy CTE producer plan properties from given context to current object
	void CopyCTEProducerPlanProps(CMemoryPool *mp, CDrvdPropCtxt *pdpctxt,
								  COperator *pop);

public:
	CDrvdPropPlan(const CDrvdPropPlan &) = delete;

	// ctor
	CDrvdPropPlan();

	// dtor
	~CDrvdPropPlan() override;

	// type of properties
	EPropType
	Ept() override
	{
		return EptPlan;
	}

	// derivation function
	void Derive(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CDrvdPropCtxt *pdpctxt) override;

	// short hand for conversion
	static CDrvdPropPlan *Pdpplan(CDrvdProp *pdp);

	// sort order accessor
	COrderSpec *
	Pos() const
	{
		return m_pos;
	}

	// distribution accessor
	CDistributionSpec *
	Pds() const
	{
		return m_pds;
	}

	// rewindability accessor
	CRewindabilitySpec *
	Prs() const
	{
		return m_prs;
	}

	// cte map
	CCTEMap *
	GetCostModel() const
	{
		return m_pcm;
	}

	// hash function
	virtual ULONG HashValue() const;

	// equality function
	virtual ULONG Equals(const CDrvdPropPlan *pdpplan) const;

	// check for satisfying required plan properties
	BOOL FSatisfies(const CReqdPropPlan *prpp) const override;

	// print function
	IOstream &OsPrint(IOstream &os) const override;

};	// class CDrvdPropPlan

}  // namespace gpopt


#endif	// !GPOPT_CDrvdPropPlan_H

// EOF
