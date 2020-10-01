//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CStatsPredLike.cpp
//
//	@doc:
//		Implementation of statistics LIKE filter
//---------------------------------------------------------------------------

#include "naucrates/statistics/CStatsPredLike.h"
#include "gpopt/operators/CExpression.h"

using namespace gpnaucrates;
using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CStatsPredLike::CStatisticsFilterLike
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CStatsPredLike::CStatsPredLike(ULONG colid, CExpression *expr_left,
							   CExpression *expr_right,
							   CDouble default_scale_factor)
	: CStatsPred(colid),
	  m_expr_left(expr_left),
	  m_expr_right(expr_right),
	  m_default_scale_factor(default_scale_factor)
{
	GPOS_ASSERT(gpos::ulong_max != colid);
	GPOS_ASSERT(NULL != expr_left);
	GPOS_ASSERT(NULL != expr_right);
	GPOS_ASSERT(0 < default_scale_factor);
}

//---------------------------------------------------------------------------
//	@function:
//		CStatsPredLike::~CStatisticsFilterLike
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CStatsPredLike::~CStatsPredLike()
{
	m_expr_left->Release();
	m_expr_right->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CStatsPredLike::GetColId
//
//	@doc:
//		Return the column identifier on which the predicates are on
//
//---------------------------------------------------------------------------
ULONG
CStatsPredLike::GetColId() const
{
	return m_colid;
}

//---------------------------------------------------------------------------
//	@function:
//		CStatsPredLike::DefaultScaleFactor
//
//	@doc:
//		Return the default like scale factor
//
//---------------------------------------------------------------------------
CDouble
CStatsPredLike::DefaultScaleFactor() const
{
	return m_default_scale_factor;
}

// EOF
