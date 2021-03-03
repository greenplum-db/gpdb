//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CMCVTest.h
//
//	@doc:
//		Testing merging most common values (MCV) and histograms
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CMCVTest_H
#define GPNAUCRATES_CMCVTest_H

#include "gpos/base.h"

namespace gpnaucrates
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMCVTest
//
//	@doc:
//		Static unit testing merging most common values (MCV) and histograms
//
//---------------------------------------------------------------------------
class CMCVTest
{
private:
	// triplet consisting of MCV input file, histogram input file and merged output file
	struct SMergeTestElem
	{
		const CHAR *szInputMCVFile;
		const CHAR *szInputHistFile;
		const CHAR *szMergedFile;
	};

public:
	// unittests
	static GPOS_RESULT EresUnittest();

	// sort MCVs tests
	static GPOS_RESULT EresUnittest_SortInt4MCVs();

	// merge MCVs and histogram
	static GPOS_RESULT EresUnittest_MergeHistMCV();

};	// class CMCVTest
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CMCVTest_H


// EOF
