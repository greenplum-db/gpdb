//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPoint.cpp
//
//	@doc:
//		Implementation of histogram point
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "naucrates/statistics/CPoint.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpnaucrates;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPoint::CPoint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPoint::CPoint
	(
	IDatum *datum
	)
	: 
	m_datum(datum)
{
	GPOS_ASSERT(NULL != m_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::Equals
//
//	@doc:
//		Equality check
//
//---------------------------------------------------------------------------
BOOL
CPoint::Equals
	(
	const CPoint *point
	)
	const
{
	GPOS_ASSERT(NULL != point);
	return m_datum->StatsAreEqual(point->m_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::IsNotEqual
//
//	@doc:
//		Inequality check
//
//---------------------------------------------------------------------------
BOOL
CPoint::IsNotEqual
	(
	const CPoint *point
	)
	const
{
	return !(this->Equals(point));
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::IsLessThan
//
//	@doc:
//		Less than check
//
//---------------------------------------------------------------------------
BOOL
CPoint::IsLessThan
	(
	const CPoint *point
	)
	const
{
	GPOS_ASSERT(NULL != point);
	return m_datum->StatsAreComparable(point->m_datum) && m_datum->StatsAreLessThan(point->m_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::IsLessThanOrEqual
//
//	@doc:
//		Less than or equals check
//
//---------------------------------------------------------------------------
BOOL
CPoint::IsLessThanOrEqual
	(
	const CPoint *point
	)
	const
{
	return (this->IsLessThan(point) || this->Equals(point));
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::IsGreaterThan
//
//	@doc:
//		Greater than check
//
//---------------------------------------------------------------------------
BOOL
CPoint::IsGreaterThan
	(
	const CPoint *point
	)
	const
{
	return m_datum->StatsAreComparable(point->m_datum) && m_datum->StatsAreGreaterThan(point->m_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::IsGreaterThanOrEqual
//
//	@doc:
//		Greater than or equals check
//
//---------------------------------------------------------------------------
BOOL
CPoint::IsGreaterThanOrEqual
	(
	const CPoint *point
	)
	const
{
	return (this->IsGreaterThan(point) || this->Equals(point));
}

// Distance between two points, assuming closed lower bound and open upper bound
CDouble
CPoint::Distance
	(
	const CPoint *point
	)
	const
{
	return Width(point, true /*include_lower*/, false /*include_upper*/);
}

// Distance between two points, taking bounds into account
// this" is usually the higher value and "point" is the lower value
// [0,5) would return 5, [0,5] would return 6 and (0,5) would return 4
CDouble
CPoint::Width
	(
	const CPoint *point,
	BOOL include_lower,
	BOOL include_upper
	)
	const
{
	// default to a non zero constant for overlap computation
	CDouble width = CDouble(1.0);
	CDouble adjust = CDouble(0.0);
	GPOS_ASSERT(NULL != point);
	if (m_datum->StatsAreComparable(point->m_datum))
	{
		// default case [this, point) or (this, point]
		width = CDouble(m_datum->GetStatsDistanceFrom(point->m_datum));
		if (m_datum->IsDatumMappableToLINT())
		{
			adjust = CDouble(1.0);
		}
		else
		{
			// for the case of doubles, the distance could be any point along
			// between the int values, so make a small adjust by a factor of
			// 10 * Epsilon (as anything smaller than Epsilon is treated as 0)
			GPOS_ASSERT(m_datum->IsDatumMappableToDouble());
			adjust = CStatistics::Epsilon * 10;
		}
	}

	GPOS_ASSERT(width >= CDouble(0.0));
	// case [this, point]
	if (include_upper && include_lower)
	{
		width = width + adjust;
	}
	// case (this, point)
	else if (!include_upper && !include_lower)
	{
		width = std::max(CDouble(0.0), width - adjust);
	}
	// if [this, point) or (this, point] no adjustment needed
	return width;
}
//---------------------------------------------------------------------------
//	@function:
//		CPoint::OsPrint
//
//	@doc:
//		Print function
//
//---------------------------------------------------------------------------
IOstream&
CPoint::OsPrint
	(
	IOstream &os
	)
	const
{
	m_datum->OsPrint(os);
	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::MinPoint
//
//	@doc:
//		minimum of two points using <=
//
//---------------------------------------------------------------------------
CPoint *
CPoint::MinPoint
	(
	CPoint *point1,
	CPoint *point2
	)
{
	if (point1->IsLessThanOrEqual(point2))
	{
		return point1;
	}
	return point2;
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::MaxPoint
//
//	@doc:
//		maximum of two points using >=
//
//---------------------------------------------------------------------------
CPoint *
CPoint::MaxPoint
	(
	CPoint *point1,
	CPoint *point2
	)
{
	if (point1->IsGreaterThanOrEqual(point2))
	{
		return point1;
	}
	return point2;
}

//---------------------------------------------------------------------------
//	@function:
//		CPoint::GetDatumVal
//
//	@doc:
//		Translate the point into its DXL representation
//---------------------------------------------------------------------------
CDXLDatum *
CPoint::GetDatumVal
	(
	CMemoryPool *mp,
	CMDAccessor *md_accessor
	)
	const
{
	IMDId *mdid = m_datum->MDId();
	return md_accessor->RetrieveType(mdid)->GetDatumVal(mp, m_datum);
}

// EOF
