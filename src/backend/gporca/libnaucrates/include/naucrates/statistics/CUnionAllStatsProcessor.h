//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CUnionAllStatsProcessor.h
//
//	@doc:
//		Compute statistics for union all operation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CUnionAllStatsProcessor_H
#define GPNAUCRATES_CUnionAllStatsProcessor_H

#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatistics.h"

namespace gpnaucrates
{

	class CUnionAllStatsProcessor
	{
		public:

		static
		CStatistics *CreateStatsForUnionAll
						(
						CMemoryPool *mp,
						const CStatistics *stats_first_child,
						const CStatistics *stats_second_child,
						ULongPtrArray *output_colids,
						ULongPtrArray *first_child_colids,
						ULongPtrArray *second_child_colids
						);
	};
}

#endif // !GPNAUCRATES_CUnionAllStatsProcessor_H

// EOF

