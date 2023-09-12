//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDistributionSpecRandom.cpp
//
//	@doc:
//		Specification of random distribution
//---------------------------------------------------------------------------

#include "gpopt/base/CDistributionSpecRandom.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecRouted.h"
#include "gpopt/base/CDistributionSpecStrictRandom.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecRandom::CDistributionSpecRandom
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDistributionSpecRandom::CDistributionSpecRandom()
{
	if (COptCtxt::PoctxtFromTLS()->FDMLQuery())
	{
		// set duplicate sensitive flag to enforce Hash-Distribution of
		// Const Tables in DML queries
		MarkDuplicateSensitive();
	}
}

CDistributionSpecRandom::CDistributionSpecRandom(CColRef *gp_segment_id_)
	: m_gp_segment_id(gp_segment_id_)
{
	if (COptCtxt::PoctxtFromTLS()->FDMLQuery())
	{
		// set duplicate sensitive flag to enforce Hash-Distribution of
		// Const Tables in DML queries
		MarkDuplicateSensitive();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecRandom::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CDistributionSpecRandom::Matches(const CDistributionSpec *pds) const
{
	if (pds->Edt() == CDistributionSpec::EdtRouted)
	{
		// This follows a 2x2 decision matrix:
		// If both gp_segment_ids are null, then it does not match
		// If one gp_segment_id is null and the other is not, it doesn't match
		// If both gp_segment_ids are not null, they match iff they are equal
		const CDistributionSpecRouted *pdsRouted =
			static_cast<const CDistributionSpecRouted *>(pds);
		GPOS_ASSERT(pdsRouted != nullptr);

		const BOOL localNull = m_gp_segment_id == nullptr;
		const BOOL pdsNull = pdsRouted->Pcr() == nullptr;

		if (localNull || pdsNull)
		{
			return false;
		}
		else
		{
			return m_gp_segment_id->Id() == pdsRouted->Pcr()->Id();
		}
	}
	else if (pds->Edt() == CDistributionSpec::EdtRandom)
	{
		const CDistributionSpecRandom *pdsRandom =
			static_cast<const CDistributionSpecRandom *>(pds);

		return pdsRandom->IsDuplicateSensitive() == m_is_duplicate_sensitive;
	}
	else
	{
		return false;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecRandom::FSatisfies
//
//	@doc:
//		Check if this distribution spec satisfies the given one
//
//---------------------------------------------------------------------------
BOOL
CDistributionSpecRandom::FSatisfies(const CDistributionSpec *pds) const
{
	if (Matches(pds))
	{
		return true;
	}

	if (EdtRandom == pds->Edt() &&
		(IsDuplicateSensitive() ||
		 !CDistributionSpecRandom::PdsConvert(pds)->IsDuplicateSensitive()))
	{
		return true;
	}

	return EdtAny == pds->Edt() || EdtNonSingleton == pds->Edt() ||
		   EdtNonReplicated == pds->Edt();
}

//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecRandom::AppendEnforcers
//
//	@doc:
//		Add required enforcers to dynamic array
//
//---------------------------------------------------------------------------
void
CDistributionSpecRandom::AppendEnforcers(CMemoryPool *mp,
										 CExpressionHandle &exprhdl,
										 CReqdPropPlan *
#ifdef GPOS_DEBUG
											 prpp
#endif	// GPOS_DEBUG
										 ,
										 CExpressionArray *pdrgpexpr,
										 CExpression *pexpr)
{
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr != prpp);
	GPOS_ASSERT(nullptr != pdrgpexpr);
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(!GPOS_FTRACE(EopttraceDisableMotions));
	GPOS_ASSERT(
		this == prpp->Ped()->PdsRequired() &&
		"required plan properties don't match enforced distribution spec");


	if (GPOS_FTRACE(EopttraceDisableMotionRandom))
	{
		// random Motion is disabled
		return;
	}

	// random motion added on top of a child delivering universal
	// spec is converted to a result node with hash filters in dxl to planned
	// statement translator. So, mark the spec of such a motion as random spec
	// as it will not be ultimately enforced by a motion.
	//
	// consider the query: INSERT INTO t1_random VALUES (1), (2);
	// where t1_random is randomly distributed.
	// the below plan shows the physical plan with random motion enforced in
	// physical stage, and the GPDB plan which translated the motion node on
	// top of universal spec child to a result node
	// Physical plan:
	// +--CPhysicalDML (Insert, "t1_random"), Source Columns: ["a" (0)], Action: ("ColRef_0001" (1))
	//    +--CPhysicalMotionRandom (#1)
	//       +--CPhysicalComputeScalar
	//          |--CPhysicalMotionRandom (#2)  ==> Motion delivers duplicate hazard
	//          |  +--CPhysicalConstTableGet Columns: ["a" (0)] Values: [(1); (2)] ==> Derives universal spec
	//          +--CScalarProjectList   origin: [Grp:9, GrpExpr:0]
	//             +--CScalarProjectElement "ColRef_0001" (1)
	//                +--CScalarConst (1)
	//
	// Insert  (cost=0.00..0.03 rows=1 width=4)
	//   ->  Redistribute Motion 1:1  (slice1; segments: 1)  (cost=0.00..0.00 rows=1 width=8) ==> Random Distribution
	//      ->  Result  (cost=0.00..0.00 rows=1 width=8)
	//         ->  Result  (cost=0.00..0.00 rows=1 width=1)  (#2)  ==> Motion converted to Result Node
	//            ->  Values Scan on "Values"  (cost=0.00..0.00 rows=2 width=4) ==> Derives universal spec

	CDistributionSpec *expr_dist_spec =
		CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds();
	CDistributionSpecRandom *random_dist_spec = nullptr;

	if (CUtils::FDuplicateHazardDistributionSpec(expr_dist_spec))
	{
		// the motion node is enforced on top of a child
		// deriving universal spec or replicated distribution, this motion node
		// will be translated to a result node with hash filter to remove
		// duplicates, therefore we also need to mark it as duplicate sensitive
		random_dist_spec = GPOS_NEW(mp) CDistributionSpecRandom();
		random_dist_spec->MarkDuplicateSensitive();
	}
	else
	{
		// the motion added in this enforcer will translate to
		// a redistribute motion
		random_dist_spec = GPOS_NEW(mp) CDistributionSpecStrictRandom();
	}

	// add a distribution enforcer
	pexpr->AddRef();
	CExpression *pexprMotion = GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CPhysicalMotionRandom(mp, random_dist_spec), pexpr);
	pdrgpexpr->Append(pexprMotion);
}


//---------------------------------------------------------------------------
//	@function:
//		CDistributionSpecRandom::OsPrint
//
//	@doc:
//		Print function
//
//---------------------------------------------------------------------------
IOstream &
CDistributionSpecRandom::OsPrint(IOstream &os) const
{
	return os << this->SzId();
}

// EOF
