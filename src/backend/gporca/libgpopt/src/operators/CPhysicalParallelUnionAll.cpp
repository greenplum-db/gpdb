//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/operators/CPhysicalParallelUnionAll.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecStrictHashed.h"
#include "gpopt/base/CDistributionSpecHashedNoOp.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CStrictHashedDistributions.h"

namespace gpopt
{
CPhysicalParallelUnionAll::CPhysicalParallelUnionAll(
	CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
	CColRef2dArray *pdrgpdrgpcrInput, ULONG ulScanIdPartialIndex)
	: CPhysicalUnionAll(mp, pdrgpcrOutput, pdrgpdrgpcrInput,
						ulScanIdPartialIndex),
	  m_pdrgpds(GPOS_NEW(mp) CStrictHashedDistributions(mp, pdrgpcrOutput,
														pdrgpdrgpcrInput))
{
	// ParallelUnionAll creates two distribution requests to enforce distribution of its children:
	// (1) (StrictHashed, StrictHashed, ...): used to force redistribute motions that mirror the
	//     output columns
	// (2) (HashedNoOp, HashedNoOp, ...): used to force redistribution motions that mirror the
	//     underlying distribution of each relational child

	SetDistrRequests(2);
}

COperator::EOperatorId
CPhysicalParallelUnionAll::Eopid() const
{
	return EopPhysicalParallelUnionAll;
}

const CHAR *
CPhysicalParallelUnionAll::SzId() const
{
	return "CPhysicalParallelUnionAll";
}

CDistributionSpec *
CPhysicalParallelUnionAll::PdsRequired(CMemoryPool *mp, CExpressionHandle &,
									   CDistributionSpec *, ULONG child_index,
									   CDrvdPropArray *, ULONG ulOptReq) const
{
	if (0 == ulOptReq)
	{
		CDistributionSpec *pdsChild = (*m_pdrgpds)[child_index];
		pdsChild->AddRef();
		return pdsChild;
	}
	else
	{
		CColRefArray *pdrgpcrChildInputColumns =
			(*PdrgpdrgpcrInput())[child_index];
		CExpressionArray *pdrgpexprFakeRequestedColumns =
			GPOS_NEW(mp) CExpressionArray(mp);

		CColRef *pcrFirstColumn = (*pdrgpcrChildInputColumns)[0];
		CExpression *pexprScalarIdent =
			CUtils::PexprScalarIdent(mp, pcrFirstColumn);
		pdrgpexprFakeRequestedColumns->Append(pexprScalarIdent);

		return GPOS_NEW(mp)
			CDistributionSpecHashedNoOp(pdrgpexprFakeRequestedColumns);
	}
}

CEnfdDistribution::EDistributionMatching
CPhysicalParallelUnionAll::Edm(CReqdPropPlan *,	  // prppInput
							   ULONG,			  // child_index
							   CDrvdPropArray *,  //pdrgpdpCtxt
							   ULONG			  // ulOptReq
)
{
	return CEnfdDistribution::EdmExact;
}

CPhysicalParallelUnionAll::~CPhysicalParallelUnionAll()
{
	m_pdrgpds->Release();
}
}  // namespace gpopt
