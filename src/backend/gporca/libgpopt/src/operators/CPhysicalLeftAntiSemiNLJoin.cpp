//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiNLJoin.cpp
//
//	@doc:
//		Implementation of left anti semi nested-loops join operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalLeftAntiSemiNLJoin.h"

#include "gpos/base.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftAntiSemiNLJoin::CPhysicalLeftAntiSemiNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalLeftAntiSemiNLJoin::CPhysicalLeftAntiSemiNLJoin(CMemoryPool *mp)
	: CPhysicalNLJoin(mp)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftAntiSemiNLJoin::~CPhysicalLeftAntiSemiNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalLeftAntiSemiNLJoin::~CPhysicalLeftAntiSemiNLJoin() = default;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalLeftAntiSemiNLJoin::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalLeftAntiSemiNLJoin::FProvidesReqdCols(CExpressionHandle &exprhdl,
											   CColRefSet *pcrsRequired,
											   ULONG  // ulOptReq
) const
{
	// left anti semi join only propagates columns from left child
	return FOuterProvidesReqdCols(exprhdl, pcrsRequired);
}

// EOF
