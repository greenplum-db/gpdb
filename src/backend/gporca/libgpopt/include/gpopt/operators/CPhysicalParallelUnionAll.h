//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#ifndef GPOPT_CPhysicalParallelUnionAll_H
#define GPOPT_CPhysicalParallelUnionAll_H

#include "gpopt/operators/CPhysicalUnionAll.h"

namespace gpopt
{
// Operator that implements logical union all, but creates a slice for each
// child relation to maximize concurrency.
// See gpopt::CPhysicalSerialUnionAll for its serial sibling.
class CPhysicalParallelUnionAll : public CPhysicalUnionAll
{
private:
	// array of child hashed distributions -- used locally for distribution derivation
	CDistributionSpecArray *const m_pdrgpds;

public:
	CPhysicalParallelUnionAll(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
							  CColRef2dArray *pdrgpdrgpcrInput,
							  ULONG ulScanIdPartialIndex);

	EOperatorId Eopid() const override;

	const CHAR *SzId() const override;

	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	CEnfdDistribution::EDistributionMatching Edm(
		CReqdPropPlan *,   // prppInput
		ULONG,			   // child_index
		CDrvdPropArray *,  //pdrgpdpCtxt
		ULONG			   // ulOptReq
		) override;

	~CPhysicalParallelUnionAll() override;
};
}  // namespace gpopt

#endif	//GPOPT_CPhysicalParallelUnionAll_H
