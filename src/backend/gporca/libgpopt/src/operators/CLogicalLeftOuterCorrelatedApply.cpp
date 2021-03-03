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
	: CLogicalLeftOuterApply(mp, pdrgpcrInner, eopidOriginSubq)
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
