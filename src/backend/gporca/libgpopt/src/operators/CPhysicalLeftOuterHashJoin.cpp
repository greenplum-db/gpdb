//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalLeftOuterHashJoin.cpp
//
//	@doc:
//		Implementation of left outer hash join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalLeftOuterHashJoin.h"

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin(
	CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
	BOOL is_null_aware, CXform::EXformId origin_xform)
	: CPhysicalHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys,
						hash_opfamilies, is_null_aware, origin_xform)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin() = default;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalJoin::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalLeftOuterHashJoin::PdsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) const
{
	return PdsDeriveForOuterJoin(mp, exprhdl);
}
// EOF
