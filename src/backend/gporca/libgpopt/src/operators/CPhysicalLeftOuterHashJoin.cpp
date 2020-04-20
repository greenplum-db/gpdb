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

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/operators/CPhysicalLeftOuterHashJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftOuterHashJoin::CPhysicalLeftOuterHashJoin
	(
	CMemoryPool *mp,
	CExpressionArray *pdrgpexprOuterKeys,
	CExpressionArray *pdrgpexprInnerKeys,
	IMdIdArray *hash_opfamilies
	)
	:
	CPhysicalHashJoin(mp, pdrgpexprOuterKeys, pdrgpexprInnerKeys, hash_opfamilies)
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
CPhysicalLeftOuterHashJoin::~CPhysicalLeftOuterHashJoin()
{
}

// EOF

