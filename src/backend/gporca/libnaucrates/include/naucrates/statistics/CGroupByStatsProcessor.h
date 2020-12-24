//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CGroupByStatsProcessor.h
//
//	@doc:
//		Compute statistics for group by operation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CGroupByStatsProcessor_H
#define GPNAUCRATES_CGroupByStatsProcessor_H

#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CGroupByStatsProcessor.h"
#include "naucrates/statistics/CStatisticsUtils.h"

namespace gpnaucrates
{
class CGroupByStatsProcessor
{
public:
	// group by
	static CStatistics *CalcGroupByStats(CMemoryPool *mp,
										 const CStatistics *input_stats,
										 ULongPtrArray *GCs,
										 ULongPtrArray *aggs, CBitSet *keys);
};
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CGroupByStatsProcessor_H

// EOF
