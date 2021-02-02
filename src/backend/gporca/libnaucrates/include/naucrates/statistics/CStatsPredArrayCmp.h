//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CStatsPredArrayCmp.h
//
//	@doc:
//		ArrayCmp filter on statistics
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatsPredArrayCmp_H
#define GPNAUCRATES_CStatsPredArrayCmp_H

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

class CStatsPredArrayCmp : public CStatsPred
{
private:
	// comparison type
	CStatsPred::EStatsCmpType m_stats_cmp_type;

	CPointArray *m_points;

public:
	CStatsPredArrayCmp &operator=(CStatsPredArrayCmp &) = delete;

	CStatsPredArrayCmp(const CStatsPredArrayCmp &) = delete;

	// ctor
	CStatsPredArrayCmp(ULONG colid, CStatsPred::EStatsCmpType stats_cmp_type,
					   CPointArray *points);

	// dtor
	~CStatsPredArrayCmp() override
	{
		m_points->Release();
	}

	// comparison types for stats computation
	virtual CStatsPred::EStatsCmpType
	GetCmpType() const
	{
		return m_stats_cmp_type;
	}

	// filter type id
	EStatsPredType
	GetPredStatsType() const override
	{
		return CStatsPred::EsptArrayCmp;
	}

	virtual CPointArray *
	GetPoints() const
	{
		return m_points;
	}

	// conversion function
	static CStatsPredArrayCmp *
	ConvertPredStats(CStatsPred *pred_stats)
	{
		GPOS_ASSERT(nullptr != pred_stats);
		GPOS_ASSERT(CStatsPred::EsptArrayCmp == pred_stats->GetPredStatsType());

		return dynamic_cast<CStatsPredArrayCmp *>(pred_stats);
	}

};	// class CStatsPredArrayCmp

}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CStatsPredArrayCmp_H

// EOF
