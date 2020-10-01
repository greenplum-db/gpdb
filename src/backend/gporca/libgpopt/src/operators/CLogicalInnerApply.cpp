//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalInnerApply.cpp
//
//	@doc:
//		Implementation of inner apply operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerApply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::CLogicalInnerApply
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::CLogicalInnerApply(CMemoryPool *mp) : CLogicalApply(mp)
{
	GPOS_ASSERT(NULL != mp);

	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::CLogicalInnerApply
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::CLogicalInnerApply(CMemoryPool *mp,
									   CColRefArray *pdrgpcrInner,
									   EOperatorId eopidOriginSubq)
	: CLogicalApply(mp, pdrgpcrInner, eopidOriginSubq)
{
	GPOS_ASSERT(0 < pdrgpcrInner->Size());
}



//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::~CLogicalInnerApply
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::~CLogicalInnerApply()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::DeriveMaxCard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalInnerApply::DeriveMaxCard(CMemoryPool *,  // mp
								  CExpressionHandle &exprhdl) const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, MaxcardDef(exprhdl));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalInnerApply::PxfsCandidates(CMemoryPool *mp) const
{
	CXformSet *xform_set = GPOS_NEW(mp) CXformSet(mp);

	(void) xform_set->ExchangeSet(CXform::ExfInnerApply2InnerJoin);
	(void) xform_set->ExchangeSet(
		CXform::ExfInnerApply2InnerJoinNoCorrelations);
	(void) xform_set->ExchangeSet(CXform::ExfInnerApplyWithOuterKey2InnerJoin);

	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalInnerApply::PopCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist)
{
	CColRefArray *pdrgpcrInner =
		CUtils::PdrgpcrRemap(mp, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(mp) CLogicalInnerApply(mp, pdrgpcrInner, m_eopidOriginSubq);
}

// EOF
