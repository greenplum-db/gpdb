//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CStatsPredPoint.h
//
//	@doc:
//		Point filter on statistics
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatsPredPoint_H
#define GPNAUCRATES_CStatsPredPoint_H

#include "gpos/base.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CStatsPred.h"

// fwd declarations
namespace gpopt
{
class CColRef;
}

namespace gpnaucrates
{
using namespace gpos;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@class:
//		CStatsPredPoint
//
//	@doc:
//		Point filter on statistics
//---------------------------------------------------------------------------
class CStatsPredPoint : public CStatsPred
{
private:
	// private copy ctor
	CStatsPredPoint(const CStatsPredPoint &);

	// private assignment operator
	CStatsPredPoint &operator=(CStatsPredPoint &);

	// comparison type
	CStatsPred::EStatsCmpType m_stats_cmp_type;

	// point to be used for comparison
	CPoint *m_pred_point;

	// add padding to datums when needed
	static IDatum *PreprocessDatum(CMemoryPool *mp, const CColRef *colref,
								   IDatum *datum);

public:
	// ctor
	CStatsPredPoint(ULONG colid, CStatsPred::EStatsCmpType stats_cmp_type,
					CPoint *point);

	// ctor
	CStatsPredPoint(CMemoryPool *mp, const CColRef *colref,
					CStatsPred::EStatsCmpType stats_cmp_type, IDatum *datum);

	// dtor
	virtual ~CStatsPredPoint()
	{
		m_pred_point->Release();
	}

	// comparison types for stats computation
	virtual CStatsPred::EStatsCmpType
	GetCmpType() const
	{
		return m_stats_cmp_type;
	}

	// filter point
	virtual CPoint *
	GetPredPoint() const
	{
		return m_pred_point;
	}

	// filter type id
	virtual EStatsPredType
	GetPredStatsType() const
	{
		return CStatsPred::EsptPoint;
	}

	// conversion function
	static CStatsPredPoint *
	ConvertPredStats(CStatsPred *pred_stats)
	{
		GPOS_ASSERT(NULL != pred_stats);
		GPOS_ASSERT(CStatsPred::EsptPoint == pred_stats->GetPredStatsType());

		return dynamic_cast<CStatsPredPoint *>(pred_stats);
	}

};	// class CStatsPredPoint

}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CStatsPredPoint_H

// EOF
