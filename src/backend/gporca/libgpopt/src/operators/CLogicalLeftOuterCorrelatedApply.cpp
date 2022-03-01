//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterCorrelatedApply.cpp
//
//	@doc:
//		Implementation of left outer correlated apply operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalLeftOuterCorrelatedApply.h"

#include "gpos/base.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterCorrelatedApply::CLogicalLeftOuterCorrelatedApply
//
//	@doc:
//		Ctor - for patterns
//
//---------------------------------------------------------------------------
CLogicalLeftOuterCorrelatedApply::CLogicalLeftOuterCorrelatedApply(
	CMemoryPool *mp)
	: CLogicalLeftOuterApply(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterCorrelatedApply::CLogicalLeftOuterCorrelatedApply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalLeftOuterCorrelatedApply::CLogicalLeftOuterCorrelatedApply(
	CMemoryPool *mp, CColRefArray *pdrgpcrInner, EOperatorId eopidOriginSubq)
	: CLogicalLeftOuterApply(mp, pdrgpcrInner, eopidOriginSubq),
	  // In the case of subquery all, we cannot push down the predicate.
	  // Example query:
	  //
	  //   SELECT (SELECT 1) = ALL (SELECT generate_series(1, 2));
	  //
	  // Physical plan:
	  // ```
	  // +--CPhysicalComputeScalar
	  //    |--CPhysicalCorrelatedLeftOuterNLJoin
	  //    |  |--CPhysicalConstTableGet Columns: ["" (0)]
	  //    |  |--CPhysicalComputeScalar
	  //    |  |  |--CPhysicalCorrelatedLeftOuterNLJoin
	  //    |  |  |  |--CPhysicalComputeScalar
	  //    |  |  |  |  |--CPhysicalConstTableGet Columns: ["" (1)] Values: [(1)]
	  //    |  |  |  |  +--CScalarProjectList
	  //    |  |  |  |     +--CScalarProjectElement "generate_series" (2)
	  //    |  |  |  |        +--CScalarFunc (generate_series)
	  //    |  |  |  |           |--CScalarConst (1)
	  //    |  |  |  |           +--CScalarConst (2)
	  //    |  |  |  |--CPhysicalComputeScalar
	  //    |  |  |  |  |--CPhysicalConstTableGet Columns: ["" (3)] Values: [(1)]
	  //    |  |  |  |  +--CScalarProjectList
	  //    |  |  |  |     +--CScalarProjectElement "?column?" (4)
	  //    |  |  |  |        +--CScalarConst (1)
	  //    |  |  |  +--CScalarConst (1)
	  //    |  |  +--CScalarProjectList
	  //    |  |     +--CScalarProjectElement "ColRef_0006" (6)
	  //    |  |        +--CScalarConst (1)
	  //    |  +--CScalarCmp (=)
	  //    |     |--CScalarIdent "?column?" (4)
	  //    |     +--CScalarIdent "generate_series" (2)
	  //    +--CScalarProjectList
	  //       +--CScalarProjectElement "?column?" (5)
	  //          +--CScalarIdent "ColRef_0006" (6)
	  // ```
	  //
	  // If we push down CScalarCmp as a filter then we would incorrectly
	  // discard the tuple 2 output from generate_series. Instead we want to
	  // preserve it for a NULL match in the LOJ so that we correctly evaulate
	  // subplan ALL_SUBLINK.
	  m_allow_predicate_pushdown(COperator::EopScalarSubqueryAll !=
								 eopidOriginSubq)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterCorrelatedApply::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftOuterCorrelatedApply::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);
	(void) xform_set->ExchangeSet(CXform::ExfImplementLeftOuterCorrelatedApply);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterCorrelatedApply::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalLeftOuterCorrelatedApply::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		return m_pdrgpcrInner->Equals(
			CLogicalLeftOuterCorrelatedApply::PopConvert(pop)->PdrgPcrInner());
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftOuterCorrelatedApply::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLeftOuterCorrelatedApply::PopCopyWithRemappedColumns(
	CMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist)
{
	CColRefArray *pdrgpcrInner =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(mp)
		CLogicalLeftOuterCorrelatedApply(mp, pdrgpcrInner, m_eopidOriginSubq);
}

// EOF
