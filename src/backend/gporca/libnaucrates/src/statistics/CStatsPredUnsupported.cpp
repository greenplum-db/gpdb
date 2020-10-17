//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CStatsPredUnsupported.cpp
//
//	@doc:
//		Implementation of unsupported statistics predicate
//---------------------------------------------------------------------------

#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatsPredUnsupported.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@function:
//		CStatsPredUnsupported::CStatsPredUnsupported
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CStatsPredUnsupported::CStatsPredUnsupported(
	ULONG colid, CStatsPred::EStatsCmpType stats_cmp_type)
	: CStatsPred(colid),
	  m_stats_cmp_type(stats_cmp_type),
	  m_default_scale_factor(0.0)
{
	m_default_scale_factor = InitScaleFactor();
}


//---------------------------------------------------------------------------
//	@function:
//		CStatsPredUnsupported::CStatsPredUnsupported
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CStatsPredUnsupported::CStatsPredUnsupported(
	ULONG colid, CStatsPred::EStatsCmpType stats_cmp_type,
	CDouble default_scale_factor)
	: CStatsPred(colid),
	  m_stats_cmp_type(stats_cmp_type),
	  m_default_scale_factor(default_scale_factor)
{
	GPOS_ASSERT(CStatistics::Epsilon < default_scale_factor);
}


//---------------------------------------------------------------------------
//		CStatsPredUnsupported::InitScaleFactor
//
//	@doc:
//		Initialize the scale factor of the unknown predicate
//---------------------------------------------------------------------------
CDouble
CStatsPredUnsupported::InitScaleFactor()
{
	return (1 / CHistogram::DefaultSelectivity).Get();
}

// EOF
