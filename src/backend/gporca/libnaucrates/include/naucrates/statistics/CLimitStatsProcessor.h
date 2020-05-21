//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CLimitStatsProcessor.h
//
//	@doc:
//		Compute statistics for limit operation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CLimitStatsProcessor_H
#define GPNAUCRATES_CLimitStatsProcessor_H

#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatisticsUtils.h"

namespace gpnaucrates
{
	using namespace gpos;

	class CLimitStatsProcessor
	{
		public:

		// limit
		static
		CStatistics *CalcLimitStats(CMemoryPool *mp, const CStatistics *input_stats, CDouble input_limit_rows);
	};
}

#endif // !GPNAUCRATES_CLimitStatsProcessor_H

// EOF

