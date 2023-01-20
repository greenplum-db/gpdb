//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CExtendedStatsProcessor.h
//
//	@doc:
//		Compute extended statistics
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CExtendedStatsProcessor_H
#define GPNAUCRATES_CExtendedStatsProcessor_H


#include "naucrates/statistics/CStatistics.h"

namespace gpnaucrates
{
class CExtendedStatsProcessor
{
public:
	static void ApplyExtendedStatistics(CDoubleArray *scale_factors,
										CStatsPredConj *conjunctive_pred_stats,
										const IMDExtStatsInfo *md_statsinfo,
										UlongToIntMap *colid_to_attno_mapping,
										CMemoryPool *mp,
										UlongToHistogramMap *result_histograms);
};
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CExtendedStatsProcessor_H

// EOF
