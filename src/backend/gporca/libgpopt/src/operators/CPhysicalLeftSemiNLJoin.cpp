//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftSemiNLJoin.cpp
//
//	@doc:
//		Implementation of left semi nested-loops join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalLeftSemiNLJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftSemiNLJoin::CPhysicalLeftSemiNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftSemiNLJoin::CPhysicalLeftSemiNLJoin
	(
	CMemoryPool *mp
	)
	:
	CPhysicalNLJoin(mp)
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftSemiNLJoin::~CPhysicalLeftSemiNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLeftSemiNLJoin::~CPhysicalLeftSemiNLJoin()
{}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftSemiNLJoin::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalLeftSemiNLJoin::FProvidesReqdCols
	(
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	// left semi join only propagates columns from left child
	return FOuterProvidesReqdCols(exprhdl, pcrsRequired);
}


// EOF

