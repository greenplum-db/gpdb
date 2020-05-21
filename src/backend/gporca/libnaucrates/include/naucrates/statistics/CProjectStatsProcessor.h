//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CProjectStatsProcessor.h
//
//	@doc:
//		Compute statistics for project operation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CProjectStatsProcessor_H
#define GPNAUCRATES_CProjectStatsProcessor_H

#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatistics.h"

namespace gpnaucrates
{

	class CProjectStatsProcessor
	{
		public:

		// project
		static
    CStatistics *CalcProjStats(CMemoryPool *mp, const CStatistics *input_stats, ULongPtrArray *projection_colids, UlongToIDatumMap *datum_map);
	};
}

#endif // !GPNAUCRATES_CProjectStatsProcessor_H

// EOF

